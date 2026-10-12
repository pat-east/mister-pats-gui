#include <csignal>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <execinfo.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <typeinfo>
#include <ucontext.h>
#include <unistd.h>

#include "App.h"
#include "CrashScreen.h"
#include "DebugLog.h"
#include "DiscDriveService.h"
#include "LibraryScan.h"

namespace {

App *g_app = nullptr;
volatile sig_atomic_t g_crashHandlerEntered = 0;
volatile sig_atomic_t g_stopSignal = 0;
volatile sig_atomic_t g_logDiscProbeCrash = 1;
constexpr off_t kMaxCrashLogBytes = 512 * 1024;

void rotateCrashLogIfNeeded() {
    constexpr const char *path = "/media/fat/mister-pat/logs/crash.log";
    constexpr const char *oldPath = "/media/fat/mister-pat/logs/crash.log.old";
    struct stat info {};
    if (lstat(path, &info) != 0) {
        if (errno != ENOENT) DebugLog::warn("crash log: could not inspect existing log");
        return;
    }
    if (!S_ISREG(info.st_mode) || S_ISLNK(info.st_mode)) {
        DebugLog::warn("crash log: existing log is not a regular file; crash records may be unavailable");
        return;
    }
    if (info.st_size <= kMaxCrashLogBytes) return;
    if (rename(path, oldPath) == 0)
        DebugLog::info("crash log: rotated oversized log to crash.log.old");
    else
        DebugLog::warn("crash log: could not rotate oversized log");
}

void writeHex(int fd, uintptr_t value);
void writeDecimal(int fd, int value);
void writeLiteral(int fd, const char *text);

int openCrashLog() {
    const int fd = open("/media/fat/mister-pat/logs/crash.log",
                        O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0644);
    if (fd < 0) return -1;
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(fd);
        return -1;
    }
    return fd;
}

void writeLiteral(int fd, const char *text) {
    size_t length = 0;
    while (text[length]) ++length;
    while (length) {
        const ssize_t written = write(fd, text, length);
        if (written <= 0) return;
        text += written;
        length -= size_t(written);
    }
}

bool safeTokenCharacter(unsigned char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
}

bool startsWithApiKeyParameter(const char *message, size_t offset) {
    static const char parameter[] = "apikey=";
    for (size_t i = 0; i < sizeof(parameter) - 1; ++i) {
        char ch = message[offset + i];
        if (!ch) return false;
        if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
        if (ch != parameter[i]) return false;
    }
    return true;
}

void writeSafeExceptionMessage(int fd, const char *message) {
    if (!message) { writeLiteral(fd, "(null)"); return; }
    size_t input = 0;
    size_t output = 0;
    while (message[input] && output < 512) {
        if (startsWithApiKeyParameter(message, input)) {
            writeLiteral(fd, "apikey=[redacted]");
            output += sizeof("apikey=[redacted]") - 1;
            input += sizeof("apikey=") - 1;
            while (message[input] && message[input] != '&' && message[input] != ' ' &&
                   message[input] != '\t' && message[input] != '\r' && message[input] != '\n' &&
                   message[input] != '"' && message[input] != '\'' && message[input] != ';')
                ++input;
            continue;
        }
        if (safeTokenCharacter(static_cast<unsigned char>(message[input]))) {
            size_t end = input;
            while (safeTokenCharacter(static_cast<unsigned char>(message[end]))) ++end;
            if (end - input >= 32) {
                writeLiteral(fd, "[redacted]");
                output += sizeof("[redacted]") - 1;
                input = end;
                continue;
            }
        }
        const char ch = message[input++];
        const char safe[2] = {ch == '\n' || ch == '\r' || ch == '\t' ? ' ' : ch, '\0'};
        writeLiteral(fd, safe);
        ++output;
    }
    if (message[input]) writeLiteral(fd, " [truncated]");
}

void appendCaughtException(const char *type, const char *message) {
    const int fd = openCrashLog();
    if (fd < 0) return;
    void *frames[32];
    const int count = backtrace(frames, int(sizeof(frames) / sizeof(frames[0])));
    writeLiteral(fd, "uncaught exception type=");
    writeLiteral(fd, type ? type : "unknown");
    writeLiteral(fd, " message=");
    writeSafeExceptionMessage(fd, message);
    writeLiteral(fd, " backtrace:");
    for (int i = 0; i < count; ++i) {
        writeLiteral(fd, " ");
        writeHex(fd, reinterpret_cast<uintptr_t>(frames[i]));
    }
    writeLiteral(fd, " pid=");
    writeDecimal(fd, int(getpid()));
    writeLiteral(fd, " auto_relaunch=paused\n");
    fsync(fd);
    close(fd);
}

void appendDiscProbeException(const char *type) {
    if (!g_logDiscProbeCrash) return;
    const int fd = openCrashLog();
    if (fd < 0) return;
    void *frames[32];
    const int count = backtrace(frames, int(sizeof(frames) / sizeof(frames[0])));
    writeLiteral(fd, "disc-probe exception type=");
    writeLiteral(fd, type ? type : "unknown");
    writeLiteral(fd, " backtrace:");
    for (int i = 0; i < count; ++i) {
        writeLiteral(fd, " ");
        writeHex(fd, reinterpret_cast<uintptr_t>(frames[i]));
    }
    writeLiteral(fd, " pid=");
    writeDecimal(fd, int(getpid()));
    writeLiteral(fd, " parent_gui_continues\n");
    fsync(fd);
    close(fd);
}

void writeHex(int fd, uintptr_t value) {
    static const char digits[] = "0123456789abcdef";
    char buffer[2 + sizeof(value) * 2];
    size_t end = sizeof(buffer);
    do {
        buffer[--end] = digits[value & 0x0fu];
        value >>= 4;
    } while (value && end > 2);
    buffer[--end] = 'x';
    buffer[--end] = '0';
    while (end < sizeof(buffer)) {
        const ssize_t written = write(fd, buffer + end, sizeof(buffer) - end);
        if (written <= 0) return;
        end += size_t(written);
    }
}

void writeDecimal(int fd, int value) {
    char buffer[16];
    size_t end = sizeof(buffer);
    unsigned int magnitude = value < 0 ? 0u - unsigned(value) : unsigned(value);
    do {
        buffer[--end] = char('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude && end > 1);
    if (value < 0) buffer[--end] = '-';
    while (end < sizeof(buffer)) {
        const ssize_t written = write(fd, buffer + end, sizeof(buffer) - end);
        if (written <= 0) return;
        end += size_t(written);
    }
}

// The patched MiSTer launcher retries a GUI that exits every five seconds. Stop that retry
// loop after a fatal error: otherwise every failed start adds more log writes and repeats the
// same failure indefinitely. The marker lives in /tmp (RAM) and a reboot clears it.
void pauseAutomaticRelaunch() {
    const int fd = open(App::kSuspendMarkerPath,
                        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0644);
    if (fd < 0) return;
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(fd);
        return;
    }
    writeLiteral(fd, "fatal GUI exit\n");
    fsync(fd);
    close(fd);
}

const char *signalName(int signalNumber) {
    switch (signalNumber) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS:  return "SIGBUS";
    case SIGILL:  return "SIGILL";
    case SIGFPE:  return "SIGFPE";
    case SIGABRT: return "SIGABRT";
    default:      return "SIGUNKNOWN";
    }
}

void onFatalSignal(int signalNumber, siginfo_t *info, void *context) {
    if (g_crashHandlerEntered) {
        pauseAutomaticRelaunch();
        _exit(128 + signalNumber);
    }
    g_crashHandlerEntered = 1;
    pauseAutomaticRelaunch();

    const int fd = openCrashLog();
    if (fd >= 0) {
        writeLiteral(fd, "fatal ");
        writeLiteral(fd, signalName(signalNumber));
        writeLiteral(fd, " number=");
        writeDecimal(fd, signalNumber);
        writeLiteral(fd, " code=");
        writeDecimal(fd, info ? info->si_code : 0);
        writeLiteral(fd, " address=");
        writeHex(fd, info ? reinterpret_cast<uintptr_t>(info->si_addr) : 0);
#if defined(__arm__)
        if (context) {
            const ucontext_t *uc = static_cast<const ucontext_t *>(context);
            writeLiteral(fd, " pc=");
            writeHex(fd, uintptr_t(uc->uc_mcontext.arm_pc));
            writeLiteral(fd, " lr=");
            writeHex(fd, uintptr_t(uc->uc_mcontext.arm_lr));
            writeLiteral(fd, " sp=");
            writeHex(fd, uintptr_t(uc->uc_mcontext.arm_sp));
        }
#endif
        writeLiteral(fd, " pid=");
        writeDecimal(fd, int(getpid()));
        writeLiteral(fd, " auto_relaunch=paused");
        writeLiteral(fd, "\n");
        fsync(fd);
        close(fd);
    }

    // Leave the saved record on disk and terminate this process. The launch wrapper sees the
    // nonzero status and opens the separate crash screen instead of starting the GUI again.
    _exit(128 + signalNumber);
}

void onDiscProbeFatalSignal(int signalNumber, siginfo_t *info, void *context) {
    if (g_crashHandlerEntered) _exit(128 + signalNumber);
    g_crashHandlerEntered = 1;
    if (!g_logDiscProbeCrash) _exit(128 + signalNumber);
    const int fd = openCrashLog();
    if (fd >= 0) {
        writeLiteral(fd, "fatal disc-probe ");
        writeLiteral(fd, signalName(signalNumber));
        writeLiteral(fd, " number=");
        writeDecimal(fd, signalNumber);
        writeLiteral(fd, " code=");
        writeDecimal(fd, info ? info->si_code : 0);
        writeLiteral(fd, " address=");
        writeHex(fd, info ? reinterpret_cast<uintptr_t>(info->si_addr) : 0);
#if defined(__arm__)
        if (context) {
            const ucontext_t *uc = static_cast<const ucontext_t *>(context);
            writeLiteral(fd, " pc=");
            writeHex(fd, uintptr_t(uc->uc_mcontext.arm_pc));
            writeLiteral(fd, " lr=");
            writeHex(fd, uintptr_t(uc->uc_mcontext.arm_lr));
        }
#endif
        writeLiteral(fd, " pid=");
        writeDecimal(fd, int(getpid()));
        writeLiteral(fd, " parent_gui_continues\n");
        fsync(fd);
        close(fd);
    }
    _exit(128 + signalNumber);
}

void onTerminate() {
    pauseAutomaticRelaunch();
    void *frames[32];
    const int count = backtrace(frames, int(sizeof(frames) / sizeof(frames[0])));
    const std::exception_ptr exception = std::current_exception();
    const int fd = openCrashLog();
    if (fd >= 0) {
        writeLiteral(fd, "std::terminate backtrace:");
        for (int i = 0; i < count; ++i) {
            writeLiteral(fd, " ");
            writeHex(fd, reinterpret_cast<uintptr_t>(frames[i]));
        }
        writeLiteral(fd, " exception=");
        if (!exception) {
            writeLiteral(fd, "none");
        } else {
            try {
                std::rethrow_exception(exception);
            } catch (const std::exception &error) {
                writeLiteral(fd, typeid(error).name());
                writeLiteral(fd, " what=");
                writeSafeExceptionMessage(fd, error.what());
            } catch (...) {
                writeLiteral(fd, "non-std-exception");
            }
        }
        writeLiteral(fd, " pid=");
        writeDecimal(fd, int(getpid()));
        writeLiteral(fd, " auto_relaunch=paused");
        writeLiteral(fd, "\n");
        fsync(fd);
        close(fd);
    }
    _exit(134);
}

void installFatalSignalLogging() {
    struct sigaction action{};
    action.sa_sigaction = onFatalSignal;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    for (int signalNumber : signals) sigaction(signalNumber, &action, nullptr);
}

void installDiscProbeSignalLogging(bool logCrash) {
    g_logDiscProbeCrash = logCrash ? 1 : 0;
    struct sigaction action{};
    action.sa_sigaction = onDiscProbeFatalSignal;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    for (int signalNumber : signals) sigaction(signalNumber, &action, nullptr);
}

void recordModuleMap() {
    FILE *input = std::fopen("/proc/self/maps", "r");
    if (!input) return;
    const int outputFd = open("/media/fat/mister-pat/logs/module-map.log",
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0644);
    if (outputFd < 0) { std::fclose(input); return; }
    struct stat outputInfo {};
    if (fstat(outputFd, &outputInfo) != 0 || !S_ISREG(outputInfo.st_mode)) {
        close(outputFd);
        std::fclose(input);
        return;
    }
    FILE *output = fdopen(outputFd, "w");
    if (!output) { close(outputFd); std::fclose(input); return; }
    std::fprintf(output, "pid=%ld\n", long(getpid()));
    char line[1024];
    while (std::fgets(line, sizeof(line), input)) {
        if (std::strstr(line, "/media/fat/mister-pat/") != nullptr)
            std::fputs(line, output);
    }
    std::fflush(output);
    fsync(outputFd);
    std::fclose(output);
    std::fclose(input);
}

void onSignal(int signalNumber) {
    g_stopSignal = signalNumber;
    if (g_app) g_app->requestStopFromSignal();
}

void onScreenshot(int) {
    if (g_app) g_app->requestScreenshot();
}

void usage() {
    std::printf(
        "mister-gui [options]\n"
        "  --tab disc|home|favorites|systems|arcade|games|settings  screen to open\n"
        "  --view list|grid|small                  game presentation\n"
        "  --system NAME                            preselect a system\n"
        "  --frames N                               render N frames, then exit\n"
        "  --dump PATH                              write the finished canvas to PATH\n"
        "  --press LIST                             feed button presses first, e.g. down,select,confirm\n"
        "  --launch-now                             start the preselected game at once\n"
        "  --dry-run                                print the MGL instead of loading it\n"
        "  --no-wizard                              never open the database wizard\n"
        "  --scan                                   build the game database and exit\n"
        "  --no-input                               do not open the input devices\n"
        "  --exclusive                              grab inputs exclusively (default)\n"
        "  --full-redraw                            repaint everything every frame\n"
        "  --no-splash                              skip the startup splash delay\n"
        "  --splash-ms N                            splash settle wait in ms (default 1000)\n");
}

bool parse(int argc, char **argv, App::Options &options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;

        if (arg == "--help" || arg == "-h") { usage(); return false; }

        if (arg == "--tab" && hasValue) {
            const std::string value = argv[++i];
            if (value == "disc") options.tab = Tab::Disc;
            else if (value == "home") options.tab = Tab::Home;
            else if (value == "favorites") options.tab = Tab::Favorites;
            else if (value == "systems") options.tab = Tab::Systems;
            else if (value == "arcade") options.tab = Tab::Arcade;
            else if (value == "games") options.tab = Tab::Games;
            else if (value == "search") options.tab = Tab::Games; // legacy alias
            else if (value == "settings") options.tab = Tab::Settings;
        } else if (arg == "--view" && hasValue) {
            options.view = gameViewFromName(argv[++i]);
            options.viewExplicit = true;
        } else if (arg == "--system" && hasValue) {
            options.system = argv[++i];
        } else if (arg == "--frames" && hasValue) {
            options.exitAfterFrames = std::atoi(argv[++i]);
        } else if (arg == "--dump" && hasValue) {
            options.dumpPath = argv[++i];
        } else if (arg == "--press" && hasValue) {
            options.script = argv[++i];
        } else if (arg == "--launch-now") {
            options.launchNow = true;
        } else if (arg == "--dry-run") {
            options.dryRun = true;
        } else if (arg == "--no-wizard") {
            options.wizard = false;
        } else if (arg == "--scan") {
            options.scanOnly = true;
        } else if (arg == "--full-redraw") {
            options.incremental = false;
        } else if (arg == "--stats") {
            options.stats = true;
        } else if (arg == "--no-grab" || arg == "--no-input") {
            options.readInput = false;
        } else if (arg == "--exclusive") {
            options.exclusive = true;
        } else if (arg == "--no-splash") {
            options.splashMs = 0;
        } else if (arg == "--splash-ms" && hasValue) {
            options.splashMs = std::atoi(argv[++i]);
        }
    }
    return true;
}

} // namespace

// Builds the database without opening the framebuffer, so it can be run over SSH while the
// television shows something else — which is also the only way to check a scan on a device
// that has no controller attached.
int runScanOnly() {
    LibraryScan scan;
    scan.start();

    std::string last;
    while (!scan.finished()) {
        scan.step();
        const std::string line = scan.statusLine();
        if (line != last) { std::printf("%s\n", line.c_str()); last = line; }
    }

    if (scan.state() != LibraryScan::State::Done) {
        std::fprintf(stderr, "scan failed: %s\n", scan.error().c_str());
        return 1;
    }
    return 0;
}

static int runGui(int argc, char **argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--crash-screen") == 0) {
        const int exitStatus = argc >= 3 ? std::atoi(argv[2]) : 1;
        return CrashScreen::show(exitStatus);
    }

    DebugLog::info("process: fatal-signal and terminate logging initialized");
    rotateCrashLogIfNeeded();
    std::set_terminate(onTerminate);
    installFatalSignalLogging();
    recordModuleMap();

    App::Options options;
    options.splashMs = 1000;   // first quarter of the splash is the drive settle wait
    if (!parse(argc, argv, options)) return 0;

    if (options.scanOnly) return runScanOnly();

    App app;
    g_app = &app;

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::signal(SIGHUP, SIG_IGN);    // survive the controlling terminal going away
    std::signal(SIGTTOU, SIG_IGN);   // writing to a console we do not own
    std::signal(SIGTTIN, SIG_IGN);
    std::signal(SIGUSR1, onScreenshot);   // photograph the running interface

    if (!app.initialize(options)) {
        std::fprintf(stderr, "could not initialise, giving up\n");
        return 1;
    }

    const int result = app.run();
    if (g_stopSignal == SIGINT) DebugLog::warn("run: stopped by SIGINT");
    else if (g_stopSignal == SIGTERM) DebugLog::warn("run: stopped by SIGTERM");
    return result;
}

extern "C" __attribute__((visibility("default")))
int mister_gui_main_v1(int argc, char **argv) noexcept {
    try {
        return runGui(argc, argv);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "mister-gui: uncaught exception type=%s\n", typeid(error).name());
        pauseAutomaticRelaunch();
        appendCaughtException(typeid(error).name(), error.what());
        DebugLog::error(std::string("uncaught GUI exception type=") + typeid(error).name());
    } catch (...) {
        std::fprintf(stderr, "mister-gui: unhandled exception\n");
        pauseAutomaticRelaunch();
        appendCaughtException("non-standard", "message unavailable");
        DebugLog::error("uncaught non-standard GUI exception");
    }
    return 1;
}

extern "C" __attribute__((visibility("default")))
int mister_gui_disc_probe_v1(int argc, char **argv) noexcept {
    if ((argc != 2 && argc != 3) || std::strcmp(argv[1], "--disc-probe-once") != 0 ||
        (argc == 3 && std::strcmp(argv[2], "--no-disc-probe-crash-log") != 0)) {
        DebugLog::error("disc probe process: rejected invalid command line");
        return 64;
    }
    installDiscProbeSignalLogging(argc == 2);
    try {
        const int result = DiscDriveService::runProbeRequest(STDIN_FILENO, STDOUT_FILENO);
        if (result != 0)
            DebugLog::error(std::string("disc probe process: request failed; category=") +
                            (result == 2 ? "invalid-or-incomplete-request" : "response-write-failed"));
        return result;
    } catch (const std::exception &error) {
        const char *type = typeid(error).name();
        DebugLog::error(std::string("disc probe process: uncaught exception; type=") + type);
        appendDiscProbeException(type);
        return 70;
    } catch (...) {
        DebugLog::error("disc probe process: uncaught non-standard exception");
        appendDiscProbeException("non-standard");
        return 70;
    }
}
