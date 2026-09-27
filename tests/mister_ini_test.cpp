// Host-side checks for the `deadzone=` reader/writer.
//
// This is the one file this project's controller-management feature must never be allowed to
// corrupt — see CONTROLLER.md, "Writing MiSTer.ini safely". Every line not owned by this
// writer has to survive a round trip untouched, the backup has to actually happen before any
// write, and the backup directory has to stay capped rather than growing forever.
//
// Build and run:  make -f tests/Makefile

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <dirent.h>

#include "../src/MisterIni.h"

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    std::printf("%-58s %s\n", what, condition ? "ok" : "FAILED");
    if (!condition) ++failures;
}

const char *kIni = "/tmp/mister_ini_test.ini";
const char *kBackupDir = "/tmp/mister_ini_test_backups";

void writeFile(const std::string &path, const std::string &content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

std::string readFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

int countEntries(const std::string &dir) {
    int count = 0;
    if (DIR *d = opendir(dir.c_str())) {
        while (dirent *entry = readdir(d)) {
            const std::string name = entry->d_name;
            if (name != "." && name != "..") ++count;
        }
        closedir(d);
    }
    return count;
}

void resetFixtures() {
    // A plain `rm -rf` equivalent for just these two paths — not the project's own file
    // writer under test, so a shell-out here does not weaken what is being verified.
    std::string cmd = "rm -rf '" + std::string(kBackupDir) + "'";
    system(cmd.c_str());
}

} // namespace

int main() {
    resetFixtures();

    // 1. No deadzone line yet: read misses, write adds one right after [MiSTer], everything
    //    else survives untouched, and exactly one backup is made first.
    writeFile(kIni,
             "[MiSTer]\n"
             "bootcore_timeout=10\n"
             "; a comment MiSTer users write for themselves\n"
             "\n"
             "reset_combo=0\n");

    int value = -1;
    check(!MisterIni::readDeadzone(0x045e02a1u, value, kIni), "no deadzone line yet: read misses");

    check(MisterIni::writeDeadzone(0x045e02a1u, 25, kIni, kBackupDir),
          "write succeeds for a fresh file");

    std::string content = readFile(kIni);
    check(content.find("deadzone=0x045e02a1, 25") != std::string::npos,
          "new deadzone line present with the right hex and value");
    check(content.find("bootcore_timeout=10") != std::string::npos,
          "unrelated key survives untouched");
    check(content.find("; a comment MiSTer users write for themselves") != std::string::npos,
          "a comment line survives untouched");
    check(content.find("reset_combo=0") != std::string::npos,
          "a key after the insertion point survives untouched");
    check(countEntries(kBackupDir) == 1, "exactly one backup made before the write");

    // 2. A second write for the SAME pad replaces the line in place rather than adding
    //    another one.
    check(MisterIni::writeDeadzone(0x045e02a1u, 40, kIni, kBackupDir),
          "second write for the same pad succeeds");
    content = readFile(kIni);
    const size_t firstOccurrence = content.find("deadzone=0x045e02a1");
    const size_t secondOccurrence =
        content.find("deadzone=0x045e02a1", firstOccurrence + 1);
    check(secondOccurrence == std::string::npos, "still only one line for this pad");
    check(content.find("deadzone=0x045e02a1, 40") != std::string::npos,
          "line updated to the new value");
    check(countEntries(kBackupDir) == 2, "a second backup was made before the second write");

    check(MisterIni::readDeadzone(0x045e02a1u, value, kIni) && value == 40,
          "read now sees the updated value");

    // 3. A DIFFERENT pad's line must never be touched by a write for this one.
    writeFile(kIni,
             "[MiSTer]\n"
             "deadzone=0x054c0ce6, 10\n"
             "bootcore_timeout=10\n");
    check(MisterIni::writeDeadzone(0x045e02a1u, 25, kIni, kBackupDir),
          "write succeeds alongside another pad's line");
    content = readFile(kIni);
    check(content.find("deadzone=0x054c0ce6, 10") != std::string::npos,
          "a different pad's deadzone line is left completely alone");
    check(content.find("deadzone=0x045e02a1, 25") != std::string::npos,
          "this pad's own line was still added");

    // 4. CRLF lines already in the file keep their line ending.
    writeFile(kIni, "[MiSTer]\r\nbootcore_timeout=10\r\n");
    check(MisterIni::writeDeadzone(0x045e02a1u, 25, kIni, kBackupDir),
          "write succeeds on a CRLF file");
    content = readFile(kIni);
    check(content.find("bootcore_timeout=10\r\n") != std::string::npos,
          "a pre-existing CRLF line keeps its \\r on a round trip");

    // 5. Backups are capped, oldest out first.
    resetFixtures();
    writeFile(kIni, "[MiSTer]\nbootcore_timeout=10\n");
    for (int i = 0; i < 35; ++i) {
        MisterIni::writeDeadzone(0x045e02a1u, i, kIni, kBackupDir);
        // The backup filename's uniqueness (and therefore its sort order) rests on a
        // millisecond timestamp; without a short pause, a fast loop can produce two backups
        // in the same millisecond and collide on one filename.
        struct timespec pause{0, 2 * 1000000L};
        nanosleep(&pause, nullptr);
    }
    check(countEntries(kBackupDir) == 30, "backup directory capped at 30 entries");

    // 6. Refuses to write, and makes no backup, if the source file cannot be read at all.
    resetFixtures();
    std::remove("/tmp/mister_ini_test_missing.ini");
    check(!MisterIni::writeDeadzone(0x045e02a1u, 25, "/tmp/mister_ini_test_missing.ini",
                                   kBackupDir),
          "write refuses when the ini file does not exist");
    check(countEntries(kBackupDir) == 0, "no backup directory created for a failed write");

    resetFixtures();
    std::remove(kIni);

    std::printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
    return failures ? 1 : 0;
}
