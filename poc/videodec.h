// Shared by the video experiments (videobench, videoplay): opens an MP4, decodes H.264 video
// and, optionally, the AAC audio, and hands out frames converted to BGRA at the video's OWN
// size. There is no scaling anywhere: the project rule is that a preview video is shown in its
// original resolution. swscale is used here only for the YUV to BGRA conversion.
#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

enum class DecodeVariant {
    Normal,     // everything decoded, loop filter on
    NoDeblock,  // loop (deblocking) filter skipped: faster, slightly softer picture
    NonRef,     // additionally drops frames nothing else refers to (only helps with B-frames)
};

class VideoDecoder {
public:
    ~VideoDecoder() { close(); }

    // Called from next() with each block of converted audio (S16, stereo, 48 kHz): data and the
    // number of sample frames. Runs on the thread that calls next(); keep it short.
    std::function<void(const uint8_t *, int)> audioSink;

    bool open(const std::string &path, DecodeVariant variant, bool withAudio) {
        close();
        if (avformat_open_input(&fmt_, path.c_str(), nullptr, nullptr) < 0) {
            error_ = "cannot open file (not a readable MP4?)";
            return false;
        }
        if (avformat_find_stream_info(fmt_, nullptr) < 0) {
            error_ = "cannot read stream information";
            return false;
        }

        const AVCodec *vcodec = nullptr;
        vindex_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);
        if (vindex_ < 0 || !vcodec) { error_ = "no decodable video stream"; return false; }
        AVStream *vs = fmt_->streams[vindex_];
        vctx_ = avcodec_alloc_context3(vcodec);
        if (!vctx_ || avcodec_parameters_to_context(vctx_, vs->codecpar) < 0) {
            error_ = "cannot set up the video decoder";
            return false;
        }
        // One decoder thread: the second core stays free for the GUI.
        vctx_->thread_count = 1;
        if (variant == DecodeVariant::NoDeblock || variant == DecodeVariant::NonRef)
            vctx_->skip_loop_filter = AVDISCARD_ALL;
        if (variant == DecodeVariant::NonRef) vctx_->skip_frame = AVDISCARD_NONREF;
        if (avcodec_open2(vctx_, vcodec, nullptr) < 0) { error_ = "cannot open the video decoder"; return false; }

        width_ = vctx_->width;
        height_ = vctx_->height;
        codecName_ = vcodec->name;
        const char *profile = avcodec_profile_name(vctx_->codec_id, vctx_->profile);
        profile_ = profile ? profile : "?";
        level_ = vctx_->level;
        fps_ = av_q2d(vs->avg_frame_rate);
        if (fps_ <= 0) fps_ = av_q2d(vs->r_frame_rate);
        if (fps_ <= 0) fps_ = 30.0;
        timeBase_ = av_q2d(vs->time_base);
        duration_ = fmt_->duration > 0 ? double(fmt_->duration) / AV_TIME_BASE : 0.0;

        if (withAudio) {
            const AVCodec *acodec = nullptr;
            aindex_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_AUDIO, -1, -1, &acodec, 0);
            if (aindex_ >= 0 && acodec) {
                actx_ = avcodec_alloc_context3(acodec);
                if (actx_ && avcodec_parameters_to_context(actx_, fmt_->streams[aindex_]->codecpar) >= 0 &&
                    avcodec_open2(actx_, acodec, nullptr) >= 0) {
                    AVChannelLayout stereo;
                    av_channel_layout_default(&stereo, 2);
                    if (swr_alloc_set_opts2(&swr_, &stereo, AV_SAMPLE_FMT_S16, 48000,
                                            &actx_->ch_layout, actx_->sample_fmt, actx_->sample_rate,
                                            0, nullptr) < 0 || swr_init(swr_) < 0) {
                        swr_free(&swr_);
                        avcodec_free_context(&actx_);
                        aindex_ = -1;
                    }
                    av_channel_layout_uninit(&stereo);
                } else {
                    avcodec_free_context(&actx_);
                    aindex_ = -1;
                }
            }
        }

        frame_ = av_frame_alloc();
        audioFrame_ = av_frame_alloc();
        packet_ = av_packet_alloc();
        if (!frame_ || !audioFrame_ || !packet_) { error_ = "out of memory"; return false; }
        return true;
    }

    void close() {
        if (sws_) { sws_freeContext(sws_); sws_ = nullptr; }
        if (swr_) swr_free(&swr_);
        if (packet_) av_packet_free(&packet_);
        if (frame_) av_frame_free(&frame_);
        if (audioFrame_) av_frame_free(&audioFrame_);
        if (vctx_) avcodec_free_context(&vctx_);
        if (actx_) avcodec_free_context(&actx_);
        if (fmt_) avformat_close_input(&fmt_);
        eof_ = false;
        vindex_ = aindex_ = -1;
        audioSamples_ = 0;
        counter_ = 0;
    }

    // Writes the next frame as BGRA into `dst` (stride width*4) and returns its time in
    // seconds from the start of the file. False at the end of the file or on a hard error.
    bool next(uint8_t *dst, double &pts) {
        av_frame_unref(frame_);
        if (!receive(pts)) return false;
        convert(dst);
        av_frame_unref(frame_);
        return true;
    }

    // The next decoded frame as the decoder made it (normally planar YUV), with no conversion.
    // The pointer stays valid until the next call. Null at the end of the file.
    const AVFrame *nextRaw(double &pts) {
        av_frame_unref(frame_);
        return receive(pts) ? frame_ : nullptr;
    }

    // Back to the start for looping.
    void rewind() {
        av_seek_frame(fmt_, -1, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(vctx_);
        if (actx_) avcodec_flush_buffers(actx_);
        eof_ = false;
        counter_ = 0;
    }

    int width() const { return width_; }
    int height() const { return height_; }
    double fps() const { return fps_; }
    double duration() const { return duration_; }
    const std::string &codecName() const { return codecName_; }
    const std::string &profile() const { return profile_; }
    int level() const { return level_; }
    bool hasAudio() const { return actx_ != nullptr; }
    int audioRate() const { return actx_ ? actx_->sample_rate : 0; }
    int audioChannels() const { return actx_ ? actx_->ch_layout.nb_channels : 0; }
    long long audioSamples() const { return audioSamples_; }
    const std::string &error() const { return error_; }

private:
    // Fills frame_ with the next decoded video frame. Audio packets met on the way go to
    // decodeAudio() when audio is enabled.
    bool receive(double &pts) {
        for (;;) {
            int r = avcodec_receive_frame(vctx_, frame_);
            if (r == 0) {
                int64_t t = frame_->best_effort_timestamp;
                pts = t != AV_NOPTS_VALUE ? double(t) * timeBase_ : double(counter_) / fps_;
                ++counter_;
                return true;
            }
            if (r != AVERROR(EAGAIN)) return false;  // EOF or error
            if (eof_) return false;

            const int rr = av_read_frame(fmt_, packet_);
            if (rr < 0) {
                avcodec_send_packet(vctx_, nullptr);  // drain
                eof_ = true;
                continue;
            }
            if (packet_->stream_index == vindex_) {
                avcodec_send_packet(vctx_, packet_);
            } else if (actx_ && packet_->stream_index == aindex_) {
                decodeAudio();
            }
            av_packet_unref(packet_);
        }
    }

    void convert(uint8_t *dst) {
        // Same width and height in and out: this is a pixel format conversion, not a resize.
        sws_ = sws_getCachedContext(sws_, frame_->width, frame_->height, AVPixelFormat(frame_->format),
                                    frame_->width, frame_->height, AV_PIX_FMT_BGRA, SWS_POINT,
                                    nullptr, nullptr, nullptr);
        uint8_t *planes[4] = {dst, nullptr, nullptr, nullptr};
        int strides[4] = {frame_->width * 4, 0, 0, 0};
        sws_scale(sws_, frame_->data, frame_->linesize, 0, frame_->height, planes, strides);
    }

    void decodeAudio() {
        if (avcodec_send_packet(actx_, packet_) < 0) return;
        while (avcodec_receive_frame(actx_, audioFrame_) == 0) {
            const int maxOut = swr_get_out_samples(swr_, audioFrame_->nb_samples);
            if (maxOut > 0) {
                if (int(audioBuf_.size()) < maxOut * 4) audioBuf_.resize(size_t(maxOut) * 4);
                uint8_t *out[1] = {audioBuf_.data()};
                const int got = swr_convert(swr_, out, maxOut,
                                            const_cast<const uint8_t **>(audioFrame_->extended_data),
                                            audioFrame_->nb_samples);
                if (got > 0) {
                    audioSamples_ += got;
                    // Signed 16-bit, stereo, 48 kHz: 4 bytes per sample frame.
                    if (audioSink) audioSink(audioBuf_.data(), got);
                }
            }
            av_frame_unref(audioFrame_);
        }
    }

    AVFormatContext *fmt_ = nullptr;
    AVCodecContext *vctx_ = nullptr;
    AVCodecContext *actx_ = nullptr;
    SwsContext *sws_ = nullptr;
    SwrContext *swr_ = nullptr;
    AVFrame *frame_ = nullptr;
    AVFrame *audioFrame_ = nullptr;
    AVPacket *packet_ = nullptr;
    int vindex_ = -1;
    int aindex_ = -1;
    int width_ = 0;
    int height_ = 0;
    int level_ = 0;
    double fps_ = 30.0;
    double timeBase_ = 0.0;
    double duration_ = 0.0;
    long long counter_ = 0;
    long long audioSamples_ = 0;
    bool eof_ = false;
    std::string codecName_;
    std::string profile_;
    std::string error_;
    std::vector<uint8_t> audioBuf_;
};
