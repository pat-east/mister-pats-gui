#include "VideoPlayer.h"

#include <algorithm>
#include <cstring>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

#include "DebugLog.h"
#include "PcmOutput.h"

namespace {

using Clock = std::chrono::steady_clock;

// Owns the FFmpeg objects of one open file, so every exit path frees them.
struct Media {
    AVFormatContext *format = nullptr;
    AVCodecContext *codec = nullptr;
    SwsContext *sws = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;
    AVCodecContext *audio = nullptr;
    SwrContext *swr = nullptr;
    AVFrame *audioFrame = nullptr;
    int videoIndex = -1;
    int audioIndex = -1;
    double timeBase = 0.0;
    double fps = 30.0;
    ~Media() {
        if (swr) swr_free(&swr);
        if (audioFrame) av_frame_free(&audioFrame);
        if (audio) avcodec_free_context(&audio);
        if (sws) sws_freeContext(sws);
        if (packet) av_packet_free(&packet);
        if (frame) av_frame_free(&frame);
        if (codec) avcodec_free_context(&codec);
        if (format) avformat_close_input(&format);
    }
};

}  // namespace

VideoPlayer::~VideoPlayer() { stop(); }

VideoPlayer::State VideoPlayer::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::Playing && decodeDone_) {
        for (const Slot &slot : slots_)
            if (slot.ready || slot.showing) return State::Playing;
        return State::Finished;
    }
    return state_;
}

std::string VideoPlayer::error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_;
}

int VideoPlayer::width() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return width_;
}

int VideoPlayer::height() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return height_;
}

void VideoPlayer::fail(const std::string &message) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::Failed;
    error_ = message;
    decodeDone_ = true;
    changed_.notify_all();
}

void VideoPlayer::start(const std::string &path, int maxWidth, int maxHeight, int maxPlays,
                        bool withSound, int volumePercent) {
    stop();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = State::Opening;
        error_.clear();
        width_ = height_ = 0;
        decodeDone_ = false;
        clockRunning_ = false;
        for (Slot &slot : slots_) { slot = Slot(); }
    }
    stop_ = false;
    try {
        thread_ = std::thread([this, path, maxWidth, maxHeight, maxPlays, withSound, volumePercent] {
            decodeLoop(path, maxWidth, maxHeight, maxPlays, withSound, volumePercent);
        });
    } catch (...) {
        fail("the player could not start");
    }
}

void VideoPlayer::stop() {
    stop_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        changed_.notify_all();
    }
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::Idle;
    width_ = height_ = 0;
    decodeDone_ = false;
    clockRunning_ = false;
    for (Slot &slot : slots_) slot = Slot();
}

const uint32_t *VideoPlayer::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::Playing) return nullptr;
    for (const Slot &slot : slots_) if (slot.showing) return nullptr;   // one at a time

    if (!clockRunning_) {
        // The clock starts when the first frame is ready, so the picture never starts late.
        int first = -1;
        for (int i = 0; i < kPool; ++i)
            if (slots_[i].ready && (first < 0 || slots_[i].pts < slots_[first].pts)) first = i;
        if (first < 0) return nullptr;
        clockRunning_ = true;
        start_ = Clock::now() - std::chrono::duration_cast<Clock::duration>(
                                    std::chrono::duration<double>(slots_[first].pts));
    }
    const double elapsed = std::chrono::duration<double>(Clock::now() - start_).count();

    int pick = -1;
    for (int i = 0; i < kPool; ++i)
        if (slots_[i].ready && slots_[i].pts <= elapsed &&
            (pick < 0 || slots_[i].pts > slots_[pick].pts)) pick = i;
    if (pick < 0) return nullptr;
    // Older ready frames are dropped, never queued up.
    for (int i = 0; i < kPool; ++i)
        if (i != pick && slots_[i].ready && slots_[i].pts < slots_[pick].pts) slots_[i].ready = false;
    slots_[pick].ready = false;
    slots_[pick].showing = true;
    changed_.notify_all();
    return slots_[pick].pixels.data();
}

void VideoPlayer::release() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (Slot &slot : slots_) slot.showing = false;
    changed_.notify_all();
}

void VideoPlayer::decodeLoop(std::string path, int maxWidth, int maxHeight, int plays,
                             bool withSound, int volumePercent) {
    // Low priority for this thread (Linux: a thread has its own id), so the interface wins.
    setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 5);

    Media m;
    av_log_set_level(AV_LOG_QUIET);
    if (avformat_open_input(&m.format, path.c_str(), nullptr, nullptr) < 0 ||
        avformat_find_stream_info(m.format, nullptr) < 0) {
        fail("the file cannot be read");
        return;
    }
    const AVCodec *decoder = nullptr;
    m.videoIndex = av_find_best_stream(m.format, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (m.videoIndex < 0 || !decoder) {
        // Say which format it is, so a report can name it.
        std::string codecName;
        for (unsigned i = 0; i < m.format->nb_streams && codecName.empty(); ++i)
            if (m.format->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                codecName = avcodec_get_name(m.format->streams[i]->codecpar->codec_id);
        fail(codecName.empty() ? "the file has no video"
                               : "format not supported (" + codecName + ")");
        return;
    }
    AVStream *stream = m.format->streams[m.videoIndex];
    m.codec = avcodec_alloc_context3(decoder);
    if (!m.codec || avcodec_parameters_to_context(m.codec, stream->codecpar) < 0) {
        fail("the player could not start");
        return;
    }
    m.codec->thread_count = 1;   // the other core stays free for the interface
    if (avcodec_open2(m.codec, decoder, nullptr) < 0) {
        fail(std::string("format not supported (") + avcodec_get_name(m.codec->codec_id) + ")");
        return;
    }

    const int w = m.codec->width, h = m.codec->height;
    if (w <= 0 || h <= 0) { fail("the video has no size"); return; }
    if (w > maxWidth || h > maxHeight) {
        // Videos are never scaled; one that does not fit is not played.
        fail("too large to show (" + std::to_string(w) + " x " + std::to_string(h) + ", room for " +
             std::to_string(maxWidth) + " x " + std::to_string(maxHeight) + ")");
        return;
    }
    m.fps = av_q2d(stream->avg_frame_rate);
    if (m.fps <= 0) m.fps = av_q2d(stream->r_frame_rate);
    if (m.fps <= 0 || m.fps > 120) m.fps = 30.0;
    m.timeBase = av_q2d(stream->time_base);
    m.frame = av_frame_alloc();
    m.packet = av_packet_alloc();
    if (!m.frame || !m.packet) { fail("not enough memory"); return; }

    // Sound is optional in every way: no track, no decoder, no aplay all just mean silence.
    PcmOutput pcm;
    bool soundOn = false;
    if (withSound) {
        const AVCodec *audioDecoder = nullptr;
        m.audioIndex = av_find_best_stream(m.format, AVMEDIA_TYPE_AUDIO, -1, -1, &audioDecoder, 0);
        if (m.audioIndex >= 0 && audioDecoder) {
            m.audio = avcodec_alloc_context3(audioDecoder);
            if (m.audio && avcodec_parameters_to_context(m.audio, m.format->streams[m.audioIndex]->codecpar) >= 0 &&
                avcodec_open2(m.audio, audioDecoder, nullptr) >= 0) {
                AVChannelLayout stereo;
                av_channel_layout_default(&stereo, 2);
                if (swr_alloc_set_opts2(&m.swr, &stereo, AV_SAMPLE_FMT_S16, 48000, &m.audio->ch_layout,
                                        m.audio->sample_fmt, m.audio->sample_rate, 0, nullptr) >= 0 &&
                    swr_init(m.swr) >= 0 && (m.audioFrame = av_frame_alloc()) != nullptr)
                    soundOn = pcm.start(150);
                av_channel_layout_uninit(&stereo);
            }
        }
        if (!soundOn) {
            if (m.swr) swr_free(&m.swr);
            if (m.audioFrame) av_frame_free(&m.audioFrame);
            if (m.audio) avcodec_free_context(&m.audio);
            m.audioIndex = -1;
        }
    }
    const int volume256 = std::max(0, std::min(100, volumePercent)) * 256 / 100;
    std::vector<uint8_t> audioBuffer;
    const auto playAudioPacket = [&]() {
        if (avcodec_send_packet(m.audio, m.packet) < 0) return;
        while (avcodec_receive_frame(m.audio, m.audioFrame) == 0) {
            const int maxOut = swr_get_out_samples(m.swr, m.audioFrame->nb_samples);
            if (maxOut > 0) {
                if (int(audioBuffer.size()) < maxOut * 4) audioBuffer.resize(size_t(maxOut) * 4);
                uint8_t *out[1] = {audioBuffer.data()};
                const int got = swr_convert(m.swr, out, maxOut,
                                            const_cast<const uint8_t **>(m.audioFrame->extended_data),
                                            m.audioFrame->nb_samples);
                if (got > 0) pcm.push(audioBuffer.data(), got, volume256);
            }
            av_frame_unref(m.audioFrame);
        }
    };

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (Slot &slot : slots_) slot.pixels.assign(size_t(w) * size_t(h), 0u);
        width_ = w;
        height_ = h;
        state_ = State::Playing;
    }

    double offset = 0.0, lastPts = 0.0;
    long long counter = 0;
    int played = 0;
    bool draining = false;

    auto deliver = [&]() -> bool {
        // One decoded frame -> a free slot, converted in place at the video's own size.
        int index = -1;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            changed_.wait(lock, [&] {
                if (stop_.load()) return true;
                for (const Slot &s : slots_) if (!s.ready && !s.showing) return true;
                return false;
            });
            if (stop_.load()) return false;
            for (int i = 0; i < kPool; ++i)
                if (!slots_[i].ready && !slots_[i].showing) { index = i; break; }
        }
        // The slot is ours until it is marked ready: nobody else reads a slot that is not ready.
        m.sws = sws_getCachedContext(m.sws, m.frame->width, m.frame->height,
                                     AVPixelFormat(m.frame->format), w, h, AV_PIX_FMT_BGRA,
                                     SWS_POINT, nullptr, nullptr, nullptr);
        if (!m.sws) return false;
        uint8_t *planes[4] = {reinterpret_cast<uint8_t *>(slots_[index].pixels.data()), nullptr, nullptr, nullptr};
        int strides[4] = {w * 4, 0, 0, 0};
        sws_scale(m.sws, m.frame->data, m.frame->linesize, 0, m.frame->height, planes, strides);
        const int64_t t = m.frame->best_effort_timestamp;
        const double pts = (t != AV_NOPTS_VALUE ? double(t) * m.timeBase : double(counter) / m.fps);
        ++counter;
        lastPts = pts;
        std::lock_guard<std::mutex> lock(mutex_);
        slots_[index].pts = pts + offset;
        slots_[index].ready = true;
        changed_.notify_all();
        return true;
    };

    while (!stop_.load()) {
        int r = avcodec_receive_frame(m.codec, m.frame);
        if (r == 0) {
            const bool ok = deliver();
            av_frame_unref(m.frame);
            if (!ok) break;
            continue;
        }
        if (r == AVERROR_EOF || (r != AVERROR(EAGAIN))) {
            // End of this play: start over until the requested number of plays is done.
            if (++played >= plays) break;
            offset += lastPts + 1.0 / m.fps;
            lastPts = 0.0;
            counter = 0;
            av_seek_frame(m.format, -1, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(m.codec);
            if (m.audio) avcodec_flush_buffers(m.audio);
            draining = false;
            continue;
        }
        if (draining) break;   // EAGAIN while draining cannot happen; never spin on it
        const int rr = av_read_frame(m.format, m.packet);
        if (rr < 0) {
            avcodec_send_packet(m.codec, nullptr);   // drain what the decoder still holds
            draining = true;
            continue;
        }
        if (m.packet->stream_index == m.videoIndex) avcodec_send_packet(m.codec, m.packet);
        else if (soundOn && m.packet->stream_index == m.audioIndex) playAudioPacket();
        av_packet_unref(m.packet);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    decodeDone_ = true;
    changed_.notify_all();
    // `pcm` goes out of scope with this function: aplay is stopped at once.
}
