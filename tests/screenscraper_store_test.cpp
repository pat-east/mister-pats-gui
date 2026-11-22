// Host-side checks for the ScreenScraper result cache, the video room-making and the login file.
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <ctime>
#include <unistd.h>

#include "../src/ScreenScraperCache.h"
#include "../src/ScreenScraperSettings.h"

namespace {
int failures = 0;
void expect(bool ok, const char *what) {
    if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}
void touch(const std::string &path, size_t bytes, long age) {
    std::ofstream out(path, std::ios::binary);
    out << std::string(bytes, 'x');
    out.close();
    struct timespec times[2];
    times[0].tv_sec = time(nullptr) - age; times[0].tv_nsec = 0;
    times[1] = times[0];
    utimensat(AT_FDCWD, path.c_str(), times, 0);
}
}  // namespace

int main() {
    char tmpl[] = "/tmp/ss_store_testXXXXXX";
    const std::string dir = mkdtemp(tmpl);

    {   // results
        ScreenScraperCache cache(dir + "/cache");
        ScreenScraperResult r;
        expect(!cache.find("snes", "marioworld", r), "unknown game is not found");
        ScreenScraperResult video;
        video.status = ScreenScraperResult::Status::Video;
        video.gameId = 5396;
        video.title = "Super Mario 64";
        video.videoType = "video-normalized";
        video.size = 3021662;
        video.crc = 0xABCDEF12ul;
        video.fetched = 1700000000;
        expect(cache.store("N64", "supermario64", video), "store a video result");
        ScreenScraperResult none;
        none.status = ScreenScraperResult::Status::NoVideo;
        none.gameId = 12;
        none.title = "Some\tGame";
        expect(cache.store("N64", "somegame", none), "store a no-video result");
        ScreenScraperResult nogame;
        expect(cache.store("N64", "unknowngame", nogame), "store a no-game result");

        // A fresh cache object reads the file back.
        ScreenScraperCache again(dir + "/cache");
        expect(again.find("N64", "supermario64", r) && r.status == ScreenScraperResult::Status::Video &&
               r.gameId == 5396 && r.videoType == "video-normalized" && r.size == 3021662 &&
               r.crc == 0xABCDEF12ul && r.title == "Super Mario 64", "video result round trip");
        expect(again.find("N64", "somegame", r) && r.status == ScreenScraperResult::Status::NoVideo &&
               r.title == "Some Game", "no-video result round trip, tab removed from the title");
        expect(again.find("N64", "unknowngame", r) && r.status == ScreenScraperResult::Status::NoGame,
               "no-game result round trip");
        expect(!again.find("snes", "supermario64", r), "results are per system");
        expect(!again.store("N64", "bad\tkey", video), "a key with a tab is refused");

        // A damaged line is skipped, the rest survives.
        {
            std::ofstream out(dir + "/cache/results/GBA.tsv", std::ios::app);
            out << "#mister-pat screenscraper 1\n";
            out << "badline\n";
            out << "evil\tvideo\t0\t../../etc/passwd\t5\t0\t0\tx\n";
            out << "good\tvideo\t7\tvideo\t100\t1f\t5\tGood\n";
        }
        ScreenScraperCache damaged(dir + "/cache");
        expect(!damaged.find("GBA", "badline", r), "a line without fields is ignored");
        expect(!damaged.find("GBA", "evil", r), "a video type with a path is not trusted");
        expect(damaged.find("GBA", "good", r) && r.gameId == 7 && r.crc == 0x1f, "a good line is read");

        expect(again.clearResults() >= 3, "clearing forgets the results");
        expect(!again.find("N64", "supermario64", r), "cleared results are gone");
        ScreenScraperCache afterClear(dir + "/cache");
        expect(!afterClear.find("N64", "supermario64", r), "cleared results are gone from disk too");
    }

    {   // videos
        ScreenScraperCache cache(dir + "/cache2");
        mkdir((dir + "/cache2").c_str(), 0755);
        mkdir((dir + "/cache2/videos").c_str(), 0755);
        touch(cache.videoPath(1, "video"), 1000, 300);
        touch(cache.videoPath(2, "video-normalized"), 1000, 200);
        touch(cache.videoPath(3, "video"), 1000, 100);
        touch(dir + "/cache2/videos/notes.txt", 5000, 500);
        touch(dir + "/cache2/videos/9-video.part", 5000, 500);
        expect(cache.hasVideo(2, "video-normalized") && !cache.hasVideo(2, "video"), "hasVideo");
        expect(cache.videoBytes() == 3000, "only finished videos are counted");
        expect(cache.makeRoom(500, 0), "no limit always fits");
        expect(cache.makeRoom(500, 3500) && cache.videoBytes() == 3000, "fits without deleting");
        expect(cache.makeRoom(1000, 3500), "making room succeeds");
        expect(!cache.hasVideo(1, "video") && cache.hasVideo(2, "video-normalized") &&
               cache.hasVideo(3, "video"), "the oldest video was deleted first");
        expect(!cache.makeRoom(5000, 3500), "a video larger than the limit never fits");
        struct stat info {};
        expect(stat((dir + "/cache2/videos/notes.txt").c_str(), &info) == 0, "foreign files are never deleted");
        expect(cache.freeBytes() > 0, "free space can be told");
    }

    {   // login file
        const std::string path = dir + "/settings.ini";
        expect(ScreenScraperSettings::save("SCREENSCRAPER_SSID", "pat", path), "create the file with one key");
        expect(ScreenScraperSettings::save("SCREENSCRAPER_SSPASSWORD", "p\"w\\d 1", path), "save a hard password");
        {   // a hand-written developer part must survive
            std::ifstream in(path);
            std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            std::string edited;
            size_t pos = 0;
            while (pos < all.size()) {
                const size_t end = all.find('\n', pos);
                std::string line = all.substr(pos, end - pos);
                if (line.compare(0, 19, "SCREENSCRAPER_DEVID") == 0) line = "SCREENSCRAPER_DEVID=\"dev1\"";
                if (line.compare(0, 25, "SCREENSCRAPER_DEVPASSWORD") == 0) line = "SCREENSCRAPER_DEVPASSWORD=\"devpw\"";
                if (line.compare(0, 22, "SCREENSCRAPER_SOFTNAME") == 0) line = "SCREENSCRAPER_SOFTNAME=\"Soft\"";
                edited += line + "\n";
                if (end == std::string::npos) break;
                pos = end + 1;
            }
            std::ofstream out(path);
            out << "# a comment of mine\n" << edited;
        }
        expect(ScreenScraperSettings::save("SCREENSCRAPER_SSID", "pat2", path), "change one key");
        const ScreenScraperCredentials c = ScreenScraperSettings::load(path);
        expect(c.userId == "pat2" && c.userPassword == "p\"w\\d 1", "user login round trip, escapes included");
        expect(c.devId == "dev1" && c.devPassword == "devpw" && c.softName == "Soft", "other keys are kept");
        expect(c.hasDeveloper() && c.hasUser(), "complete");
        std::ifstream in(path);
        std::string first;
        std::getline(in, first);
        expect(first == "# a comment of mine", "comments are kept");
        expect(!ScreenScraperSettings::save("SOMETHING_ELSE", "x", path), "unknown keys are refused");
        expect(!ScreenScraperSettings::save("SCREENSCRAPER_SSID", "a\nb", path), "newlines are refused");
        expect(!ScreenScraperSettings::load(dir + "/missing.ini").hasUser(), "a missing file is empty");
        expect(ScreenScraperSettings::load(dir + "/missing.ini").softName == ScreenScraperSettings::kSoftName,
               "the software name defaults even without a file");

        // A file the GUI created has the software name in it, and an empty name still defaults.
        const std::string fresh = dir + "/fresh.ini";
        expect(ScreenScraperSettings::save("SCREENSCRAPER_SSID", "someone", fresh), "create a fresh file");
        std::ifstream freshIn(fresh);
        std::string freshText((std::istreambuf_iterator<char>(freshIn)), std::istreambuf_iterator<char>());
        expect(freshText.find("SCREENSCRAPER_SOFTNAME=\"MiSTerPatsGUI\"") != std::string::npos,
               "a new file is written with the software name");
        const std::string emptyName = dir + "/empty-name.ini";
        { std::ofstream out(emptyName); out << "SCREENSCRAPER_SOFTNAME=\"\"\n"; }
        expect(ScreenScraperSettings::load(emptyName).softName == "MiSTerPatsGUI", "an empty name defaults");
    }

    std::string cleanup = "rm -rf " + dir;
    if (std::system(cleanup.c_str()) != 0) {}
    if (failures == 0) std::printf("screenscraper_store_test: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
