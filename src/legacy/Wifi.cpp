#include "Wifi.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

void eraseSecret(std::string &value) {
    volatile char *data = value.empty() ? nullptr : &value[0];
    for (size_t i = 0; i < value.size(); ++i) data[i] = '\0';
    value.clear();
}

bool runTool(const std::vector<std::string> &args, const std::string &input,
             std::string &output) {
    if (args.empty()) return false;
    int inPipe[2], outPipe[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, inPipe) != 0) return false;
    if (pipe2(outPipe, O_CLOEXEC) != 0) { close(inPipe[0]); close(inPipe[1]); return false; }
    const pid_t child = fork();
    if (child < 0) {
        close(inPipe[0]); close(inPipe[1]); close(outPipe[0]); close(outPipe[1]);
        return false;
    }
    if (child == 0) {
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        const int nullFd = open("/dev/null", O_WRONLY);
        if (nullFd >= 0) dup2(nullFd, STDERR_FILENO);
        close(inPipe[0]); close(inPipe[1]); close(outPipe[0]); close(outPipe[1]);
        std::vector<char *> argv;
        for (const std::string &arg : args) argv.push_back(const_cast<char *>(arg.c_str()));
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(inPipe[0]); close(outPipe[1]);
    size_t written = 0;
    while (written < input.size()) {
        const ssize_t n = send(inPipe[1], input.data() + written,
                               input.size() - written, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        written += size_t(n);
    }
    close(inPipe[1]);
    output.clear();
    char buffer[1024];
    ssize_t count;
    while ((count = read(outPipe[0], buffer, sizeof(buffer))) > 0) {
        if (output.size() < 65536)
            output.append(buffer, std::min<size_t>(size_t(count), 65536 - output.size()));
    }
    close(outPipe[0]);

    int status = 0;
    pid_t waited;
    do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
    return written == input.size() && waited == child && WIFEXITED(status) &&
           WEXITSTATUS(status) == 0;
}

std::string hexEncode(const std::string &text) {
    static const char digits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(text.size() * 2);
    for (unsigned char c : text) {
        hex.push_back(digits[c >> 4]);
        hex.push_back(digits[c & 15]);
    }
    return hex;
}

std::string valueFor(const std::string &text, const std::string &key) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        if (text.compare(pos, key.size(), key) == 0 && pos + key.size() < end &&
            text[pos + key.size()] == '=')
            return text.substr(pos + key.size() + 1, end - pos - key.size() - 1);
        pos = end + 1;
    }
    return {};
}

std::string trim(const std::string &s);
bool validCountryCode(const std::string &country);

bool responseHasFailure(const std::string &response) {
    size_t line = 0;
    while (line < response.size()) {
        size_t end = response.find('\n', line);
        if (end == std::string::npos) end = response.size();
        const std::string row = trim(response.substr(line, end - line));
        if (row == "FAIL" || row.compare(0, 5, "FAIL-") == 0 ||
            row.compare(0, 15, "UNKNOWN COMMAND") == 0) return true;
        line = end + 1;
    }
    return false;
}

bool parseNetworkId(const std::string &response, int &networkId) {
    bool found = false;
    size_t line = 0;
    while (line < response.size()) {
        size_t end = response.find('\n', line);
        if (end == std::string::npos) end = response.size();
        std::string row = trim(response.substr(line, end - line));
        // Interactive wpa_cli may prefix command output with its prompt.
        if (!row.empty() && row[0] == '>') row = trim(row.substr(1));
        if (!row.empty()) {
            char *tail = nullptr;
            const long id = std::strtol(row.c_str(), &tail, 10);
            if (tail != row.c_str() && *tail == '\0' && id >= 0 && id <= 2147483647L) {
                networkId = int(id);
                found = true;
            }
        }
        if (end == response.size()) break;
        line = end + 1;
    }
    return found;
}

std::string currentRegulatoryCountry() {
    std::string output;
    if (!runTool({"iw", "reg", "get"}, {}, output)) return {};
    size_t line = 0;
    while (line < output.size()) {
        const size_t end = output.find('\n', line);
        const size_t length = (end == std::string::npos ? output.size() : end) - line;
        const std::string row = output.substr(line, length);
        const size_t marker = row.find("country ");
        if (marker != std::string::npos && marker + 10 < row.size() && row[marker + 10] == ':') {
            const std::string country = row.substr(marker + 8, 2);
            if (validCountryCode(country)) return country;
        }
        if (end == std::string::npos) break;
        line = end + 1;
    }
    return {};
}

bool atomicPrivateWrite(const std::string &path, const std::string &contents,
                        std::string &error) {
    if (mkdir("/media/fat/linux", 0755) != 0 && errno != EEXIST) {
        error = "Cannot open /media/fat/linux: " + std::string(std::strerror(errno));
        return false;
    }
    std::string temp = path + ".tmp.XXXXXX";
    std::vector<char> tempName(temp.begin(), temp.end());
    tempName.push_back('\0');
    const int fd = mkstemp(tempName.data());
    if (fd < 0) { error = "Cannot create Wi-Fi config: " + std::string(std::strerror(errno)); return false; }
    bool ok = fchmod(fd, 0600) == 0;
    size_t offset = 0;
    while (ok && offset < contents.size()) {
        const ssize_t n = write(fd, contents.data() + offset, contents.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ok = false; break; }
        offset += size_t(n);
    }
    if (ok && fsync(fd) != 0) ok = false;
    int savedErrno = errno;
    close(fd);
    if (ok && rename(tempName.data(), path.c_str()) != 0) { ok = false; savedErrno = errno; }
    if (!ok) {
        unlink(tempName.data());
        error = "Cannot save Wi-Fi config: " + std::string(std::strerror(savedErrno));
    }
    return ok;
}

std::string trim(const std::string &s) {
    const size_t first = s.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const size_t last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

bool validCountryCode(const std::string &country) {
    return country.size() == 2 && country != "00" &&
        std::isupper(static_cast<unsigned char>(country[0])) &&
        std::isupper(static_cast<unsigned char>(country[1]));
}

std::string configuredCountry() {
    std::ifstream in("/media/fat/linux/wpa_supplicant.conf");
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.compare(0, 8, "country=") == 0) return line.substr(8);
    }
    return {};
}

} // namespace

Wifi::~Wifi() {
    if (testNetworkId_ >= 0) rollback();
}

bool Wifi::refreshInterface() {
    interface_.clear();
    struct if_nameindex *list = if_nameindex();
    if (!list) return false;
    for (struct if_nameindex *it = list; it->if_index && it->if_name; ++it) {
        if (std::strncmp(it->if_name, "wl", 2) == 0) {
            interface_ = it->if_name;
            break;
        }
    }
    if_freenameindex(list);
    return !interface_.empty();
}

bool Wifi::command(const std::string &commands, std::string &output, std::string &error) const {
    if (interface_.empty()) { error = "No Wi-Fi adapter"; return false; }
    std::string input = commands;
    if (input.empty() || input.back() != '\n') input.push_back('\n');
    input += "quit\n";
    const bool completed = runTool({"wpa_cli", "-i", interface_}, input, output);
    eraseSecret(input);
    if (!completed) {
        error = "wpa_cli did not complete successfully";
        return false;
    }
    if (responseHasFailure(output)) { error = "The Wi-Fi supplicant rejected a command"; return false; }
    return true;
}

bool Wifi::startScan(std::string &error) {
    if (!refreshInterface()) { error = "No Wi-Fi adapter"; return false; }
    std::string response;
    if (!command("scan", response, error)) return false;
    scanTimer_ = 2.5f;
    return true;
}

std::vector<Wifi::Network> Wifi::scanResults(std::string &error) const {
    std::vector<Network> networks;
    std::string output;
    if (!command("scan_results", output, error)) return networks;
    size_t line = output.find('\n');
    if (line == std::string::npos) return networks;
    line++;
    while (line < output.size()) {
        size_t end = output.find('\n', line);
        if (end == std::string::npos) end = output.size();
        const std::string row = output.substr(line, end - line);
        size_t a = row.find('\t');
        size_t b = a == std::string::npos ? a : row.find('\t', a + 1);
        size_t c = b == std::string::npos ? b : row.find('\t', b + 1);
        size_t d = c == std::string::npos ? c : row.find('\t', c + 1);
        if (a != std::string::npos && b != std::string::npos && c != std::string::npos &&
            d != std::string::npos) {
            Network network;
            const std::string signal = row.substr(b + 1, c - b - 1);
            network.signalDbm = std::atoi(signal.c_str());
            network.security = row.substr(c + 1, d - c - 1);
            network.ssid = row.substr(d + 1);
            network.open = network.security.find("WPA") == std::string::npos &&
                           network.security.find("WEP") == std::string::npos &&
                           network.security.find("SAE") == std::string::npos;
            network.supported = network.open || network.security.find("PSK") != std::string::npos;
            if (!network.ssid.empty()) {
                auto existing = std::find_if(networks.begin(), networks.end(), [&](const Network &n) {
                    return n.ssid == network.ssid;
                });
                if (existing == networks.end()) networks.push_back(network);
                else if (network.signalDbm > existing->signalDbm) *existing = network;
            }
        }
        if (end == output.size()) break;
        line = end + 1;
    }
    std::stable_sort(networks.begin(), networks.end(), [](const Network &a, const Network &b) {
        return a.signalDbm > b.signalDbm;
    });
    return networks;
}

bool Wifi::derivePsk(const std::string &ssid, const std::string &password,
                     std::string &psk, std::string &error) const {
    if (password.size() < 8 || password.size() > 63) {
        error = "Wi-Fi passwords must be 8 to 63 characters";
        return false;
    }
    std::string output;
    std::string input = password + "\n";
    const bool generated = runTool({"wpa_passphrase", ssid}, input, output);
    eraseSecret(input);
    if (!generated) {
        eraseSecret(output);
        error = "wpa_passphrase could not derive a network key";
        return false;
    }
    psk.clear();
    size_t pos = 0;
    while (pos < output.size()) {
        size_t end = output.find('\n', pos);
        if (end == std::string::npos) end = output.size();
        const std::string line = trim(output.substr(pos, end - pos));
        if (line.compare(0, 4, "psk=") == 0) {
            psk = line.substr(4);
            break;
        }
        pos = end + 1;
    }
    if (psk.size() != 64 || psk.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
        eraseSecret(output);
        eraseSecret(psk);
        error = "wpa_passphrase returned an invalid key";
        return false;
    }
    eraseSecret(output);
    return true;
}

bool Wifi::capturePreviousNetworks(std::string &error) {
    previousNetworks_.clear();
    previousNetworkId_ = -1;
    std::string output;
    if (!command("list_networks\nstatus", output, error)) return false;
    const std::string activeId = valueFor(output, "id");
    if (!activeId.empty()) previousNetworkId_ = std::atoi(activeId.c_str());

    size_t line = output.find("network id");
    if (line == std::string::npos) return true;
    line = output.find('\n', line);
    if (line == std::string::npos) return true;
    ++line;
    while (line < output.size()) {
        size_t end = output.find('\n', line);
        if (end == std::string::npos) end = output.size();
        const std::string row = output.substr(line, end - line);
        const size_t tab = row.find('\t');
        if (tab != std::string::npos) {
            char *tail = nullptr;
            const long id = std::strtol(row.c_str(), &tail, 10);
            if (tail != row.c_str() && id >= 0) {
                const size_t flags = row.find('[', tab + 1);
                previousNetworks_.push_back({int(id), flags == std::string::npos ||
                    row.compare(flags, 10, "[DISABLED]") != 0});
            }
        }
        if (end == output.size()) break;
        line = end + 1;
    }
    return true;
}

bool Wifi::beginTest(const std::string &ssid, const std::string &password, bool open,
                     bool hidden, std::string &error) {
    if (ssid.empty() || ssid.size() > 32 || ssid.find('\0') != std::string::npos) {
        error = "Network names must contain 1 to 32 bytes";
        return false;
    }
    if (!refreshInterface()) { error = "No Wi-Fi adapter"; return false; }
    testedPsk_.clear();
    if (!open && !derivePsk(ssid, password, testedPsk_, error)) return false;
    if (!capturePreviousNetworks(error)) { eraseSecret(testedPsk_); return false; }

    testedSsid_ = ssid;
    testedOpen_ = open;
    std::string output;
    if (!command("add_network", output, error)) { eraseSecret(testedPsk_); return false; }
    if (!parseNetworkId(output, testNetworkId_)) {
        eraseSecret(testedPsk_);
        error = "The supplicant returned an invalid test network id";
        return false;
    }

    const std::string networkId = std::to_string(testNetworkId_);
    std::string commands = "set_network " + networkId + " ssid " + hexEncode(ssid) + "\n";
    if (open) {
        commands += "set_network " + networkId + " key_mgmt NONE\n";
    } else {
        commands += "set_network " + networkId + " psk " + testedPsk_ + "\n";
        commands += "set_network " + networkId + " key_mgmt WPA-PSK\n";
    }
    if (hidden) commands += "set_network " + networkId + " scan_ssid 1\n";
    commands += "set_network " + networkId + " priority 1000\n";
    commands += "enable_network " + networkId + "\nselect_network " + networkId;
    const bool selected = command(commands, output, error);
    eraseSecret(commands);
    eraseSecret(output);
    if (!selected) {
        rollback();
        return false;
    }
    testTimer_ = 30.0f;
    statusPollTimer_ = 0.0f;
    dhcpTimer_ = 0.0f;
    return true;
}

Wifi::TestResult Wifi::pollTest(float deltaSeconds, std::string &status) {
    if (testNetworkId_ < 0) { status = "No Wi-Fi test is running"; return TestResult::Failed; }
    testTimer_ -= deltaSeconds;
    statusPollTimer_ -= deltaSeconds;
    dhcpTimer_ -= deltaSeconds;
    if (statusPollTimer_ > 0.0f) {
        if (testTimer_ <= 0.0f) {
            status = "Could not connect to " + testedSsid_ + ". Check the password and signal.";
            rollback();
            return TestResult::Failed;
        }
        return TestResult::Waiting;
    }
    statusPollTimer_ = 0.5f;
    std::string output, error;
    if (!command("status", output, error)) {
        status = error;
        if (testTimer_ <= 0.0f) {
            rollback();
            return TestResult::Failed;
        }
        return TestResult::Waiting;
    }
    const std::string state = valueFor(output, "wpa_state");
    const std::string ssid = valueFor(output, "ssid");
    if (state == "COMPLETED" && ssid == testedSsid_) {
        if (valueFor(output, "ip_address").empty()) {
            if (dhcpTimer_ <= 0.0f) {
                std::string dhcpOutput;
                runTool({"dhcpcd", "-n", interface_}, {}, dhcpOutput);
                dhcpTimer_ = 3.0f;
            }
            status = "Connected; waiting for an IP address…";
        } else {
            status = "Connected to " + testedSsid_ + " — IP " + valueFor(output, "ip_address");
            return TestResult::Connected;
        }
    } else if (state == "ASSOCIATING" || state == "ASSOCIATED" || state == "4WAY_HANDSHAKE") {
        status = "Joining " + testedSsid_ + "…";
    } else {
        status = "Waiting for Wi-Fi connection…";
    }
    if (testTimer_ <= 0.0f) {
        status = "Could not connect to " + testedSsid_ + ". Check the password and signal.";
        rollback();
        return TestResult::Failed;
    }
    return TestResult::Waiting;
}

bool Wifi::needsCountryCode() const {
    return !validCountryCode(configuredCountry()) &&
           !validCountryCode(currentRegulatoryCountry());
}

bool Wifi::setCountryCode(const std::string &country, std::string &error) {
    std::string normalized = country;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return char(std::toupper(c));
    });
    if (!validCountryCode(normalized)) {
        error = "Enter a valid two-letter country code, for example DE";
        return false;
    }
    countryCode_ = normalized;
    return true;
}

void Wifi::rollback() {
    if (interface_.empty()) return;
    std::string output, error;
    if (testNetworkId_ >= 0) {
        command("disable_network " + std::to_string(testNetworkId_) + "\nremove_network " +
                std::to_string(testNetworkId_), output, error);
    }
    if (previousNetworkId_ >= 0) {
        command("select_network " + std::to_string(previousNetworkId_), output, error);
        std::string reenable;
        for (const SavedNetwork &network : previousNetworks_)
            if (network.enabled && network.id != previousNetworkId_)
                reenable += "enable_network " + std::to_string(network.id) + "\n";
        if (!reenable.empty()) command(reenable, output, error);
    } else {
        std::string reenable;
        for (const SavedNetwork &network : previousNetworks_)
            if (network.enabled) reenable += "enable_network " + std::to_string(network.id) + "\n";
        if (!reenable.empty()) command(reenable, output, error);
        else command("reassociate", output, error);
    }
    std::string dhcpOutput;
    runTool({"dhcpcd", "-n", interface_}, {}, dhcpOutput);
    testNetworkId_ = -1;
    previousNetworkId_ = -1;
    previousNetworks_.clear();
    testedSsid_.clear();
    eraseSecret(testedPsk_);
    countryCode_.clear();
    testTimer_ = 0.0f;
    statusPollTimer_ = 0.0f;
    dhcpTimer_ = 0.0f;
}

bool Wifi::saveConfiguration(const std::string &ssid, const std::string &psk, bool open,
                             std::string &error) const {
    const std::string path = "/media/fat/linux/wpa_supplicant.conf";
    std::string contents;
    {
        std::ifstream in(path.c_str(), std::ios::binary);
        if (in) {
            std::ostringstream buffer;
            buffer << in.rdbuf();
            contents = buffer.str();
            if (!in.good() && !in.eof()) { error = "Cannot read existing Wi-Fi settings"; return false; }
        }
    }

    bool hasCountry = false;
    bool invalidCountry = false;
    size_t line = 0;
    while (line < contents.size()) {
        size_t end = contents.find('\n', line);
        if (end == std::string::npos) end = contents.size();
        const std::string row = trim(contents.substr(line, end - line));
        if (row.compare(0, 8, "country=") == 0) {
            const std::string code = row.substr(8);
            hasCountry = true;
            invalidCountry = !validCountryCode(code);
            break;
        }
        line = end + 1;
    }
    if (invalidCountry) {
        if (!validCountryCode(countryCode_)) {
            error = "The saved Wi-Fi country code is invalid";
            return false;
        }
        std::string sanitized;
        line = 0;
        while (line < contents.size()) {
            size_t end = contents.find('\n', line);
            if (end == std::string::npos) end = contents.size();
            const std::string row = trim(contents.substr(line, end - line));
            if (row.compare(0, 8, "country=") != 0) {
                sanitized.append(contents, line, end - line);
                if (end < contents.size()) sanitized.push_back('\n');
            }
            line = end + 1;
        }
        contents.swap(sanitized);
        hasCountry = false;
    }
    if (!hasCountry) {
        const std::string country = validCountryCode(countryCode_)
            ? countryCode_ : currentRegulatoryCountry();
        if (!validCountryCode(country)) {
            error = "Cannot determine the Wi-Fi country code from the active regulatory domain";
            return false;
        }
        contents.insert(0, "country=" + country + "\n");
    }
    if (!contents.empty() && contents.back() != '\n') contents.push_back('\n');
    if (!contents.empty()) contents.push_back('\n');

    contents += "network={\n    ssid=" + hexEncode(ssid) + "\n";
    if (open) contents += "    key_mgmt=NONE\n";
    else contents += "    psk=" + psk + "\n";
    contents += "    priority=1000\n}\n";
    const bool saved = atomicPrivateWrite(path, contents, error);
    eraseSecret(contents);
    return saved;
}

bool Wifi::saveTestedNetwork(std::string &error) {
    if (testNetworkId_ < 0) { error = "No tested Wi-Fi network to save"; return false; }
    if (!saveConfiguration(testedSsid_, testedPsk_, testedOpen_, error)) return false;
    std::string enable;
    for (const SavedNetwork &network : previousNetworks_)
        if (network.enabled) enable += "enable_network " + std::to_string(network.id) + "\n";
    if (!enable.empty()) {
        std::string ignored, commandError;
        command(enable, ignored, commandError);
    }
    // The tested network remains active in the running supplicant. The new profile is read on
    // the next boot, avoiding a second association cycle immediately after a successful test.
    testNetworkId_ = -1;
    previousNetworkId_ = -1;
    previousNetworks_.clear();
    eraseSecret(testedPsk_);
    countryCode_.clear();
    testTimer_ = 0.0f;
    statusPollTimer_ = 0.0f;
    dhcpTimer_ = 0.0f;
    return true;
}
