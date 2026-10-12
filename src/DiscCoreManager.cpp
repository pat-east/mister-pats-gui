#include "DiscCoreManager.h"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "UpdateFiles.h"

namespace {

std::string lower(std::string value) {
    for (char &ch : value)
        if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
    return value;
}

std::string trim(const std::string &value) {
    size_t first = 0, last = value.size();
    while (first < last && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

bool regularFile(const std::string &path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode) && info.st_size > 0;
}

bool endsWith(const std::string &text, const char *suffix);

bool installedRbf(const std::string &fatRoot, const char *coreName) {
    const std::string directory = fatRoot + "/_Console";
    DIR *dir = opendir(directory.c_str());
    if (!dir) return false;
    const std::string wanted = lower(coreName);
    bool found = false;
    while (dirent *entry = readdir(dir)) {
        std::string name = lower(entry->d_name);
        if (!endsWith(name, ".rbf")) continue;
        name.resize(name.size() - 4);
        const size_t underscore = name.find_last_of('_');
        if (underscore != std::string::npos && name.size() - underscore == 9) {
            bool date = true;
            for (size_t i = underscore + 1; i < name.size(); ++i)
                if (name[i] < '0' || name[i] > '9') date = false;
            if (date) name.resize(underscore);
        }
        if (name != wanted) continue;
        found = regularFile(directory + "/" + entry->d_name);
        if (found) break;
    }
    closedir(dir);
    return found;
}

bool endsWith(const std::string &text, const char *suffix) {
    const size_t length = std::char_traits<char>::length(suffix);
    return text.size() >= length && text.compare(text.size() - length, length, suffix) == 0;
}

void findMgls(const std::string &directory, unsigned depth, std::vector<std::string> &out) {
    if (depth > 3) return;
    DIR *dir = opendir(directory.c_str());
    if (!dir) return;
    std::vector<std::string> nested;
    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        const std::string path = directory + "/" + name;
        struct stat info {};
        if (lstat(path.c_str(), &info) != 0) continue;
        if (S_ISDIR(info.st_mode)) nested.push_back(path);
        else if (S_ISREG(info.st_mode) && endsWith(lower(name), ".mgl")) out.push_back(path);
    }
    closedir(dir);
    std::sort(nested.begin(), nested.end());
    for (const std::string &path : nested) findMgls(path, depth + 1, out);
}

bool hasScopedMain(const std::string &path) {
    std::ifstream input(path.c_str());
    if (!input) return false;
    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        const size_t comment = line.find_first_of(";#");
        if (comment != std::string::npos) line.erase(comment);
        line = trim(line);
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') {
            section = lower(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        if (section != "cd-*") continue;
        const size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        if (lower(trim(line.substr(0, equals))) == "main" &&
            lower(trim(line.substr(equals + 1))) == "mister-disc") return true;
    }
    return false;
}

bool removeXmlComments(const std::string &xml, std::string &out) {
    out.clear();
    size_t cursor = 0;
    while (cursor < xml.size()) {
        const size_t begin = xml.find("<!--", cursor);
        if (begin == std::string::npos) {
            out.append(xml, cursor, std::string::npos);
            return true;
        }
        out.append(xml, cursor, begin - cursor);
        const size_t end = xml.find("-->", begin + 4);
        if (end == std::string::npos) return false;
        cursor = end + 3;
    }
    return true;
}

std::string tagValue(const std::string &xml, const std::string &tag) {
    const std::string lowerXml = lower(xml);
    const std::string open = "<" + tag;
    size_t start = lowerXml.find(open);
    while (start != std::string::npos) {
        const size_t boundary = start + open.size();
        if (boundary < lowerXml.size() &&
            (lowerXml[boundary] == '>' || std::isspace(static_cast<unsigned char>(lowerXml[boundary]))))
            break;
        start = lowerXml.find(open, boundary);
    }
    if (start == std::string::npos) return {};
    const size_t startTagEnd = lowerXml.find('>', start + open.size());
    if (startTagEnd == std::string::npos) return {};
    const std::string close = "</" + tag + ">";
    const size_t end = lowerXml.find(close, startTagEnd + 1);
    if (end == std::string::npos) return {};
    if (lowerXml.find(open, startTagEnd + 1) != std::string::npos) return {};
    if (lowerXml.find(close, end + close.size()) != std::string::npos) return {};
    return trim(xml.substr(startTagEnd + 1, end - startTagEnd - 1));
}

bool compatibleDiscMgl(const std::string &path, const char *core) {
    std::string rawXml, xml;
    if (!UpdateFiles::readLimited(path, 16384, rawXml) || !removeXmlComments(rawXml, xml))
        return false;
    const std::string lowered = lower(xml);
    if (lowered.find("<mistergamedescription") == std::string::npos ||
        lowered.find("</mistergamedescription>") == std::string::npos) return false;
    if (lower(tagValue(xml, "rbf")) != std::string("_console/") + lower(core)) return false;
    const std::string setname = lower(tagValue(xml, "setname"));
    return setname.compare(0, 3, "cd-") == 0;
}

} // namespace

DiscCoreManager::Status DiscCoreManager::inspect(const std::string &fatRoot,
                                                const std::string &iniPath) const {
    Status status;
    status.alternateMainInstalled = regularFile(fatRoot + "/MiSTer-disc");
    status.scopedMainConfigured = hasScopedMain(iniPath);
    status.psxCoreInstalled = installedRbf(fatRoot, "PSX");
    status.saturnCoreInstalled = installedRbf(fatRoot, "Saturn");

    std::vector<std::string> mgls;
    findMgls(fatRoot + "/_Disc_Cores", 0, mgls);
    findMgls(fatRoot + "/mgl/_Disc_Cores", 0, mgls);
    std::sort(mgls.begin(), mgls.end());
    for (const std::string &path : mgls) {
        const size_t slash = path.find_last_of('/');
        const std::string filename = lower(path.substr(slash == std::string::npos ? 0 : slash + 1));
        if ((filename.find("psx") != std::string::npos ||
             filename.find("playstation") != std::string::npos) && regularFile(path)) {
            status.psxMglInstalled = true;
            const bool compatible = compatibleDiscMgl(path, "PSX");
            if (status.psxMgl.empty() || compatible) status.psxMgl = path;
            status.psxMglCompatible = status.psxMglCompatible || compatible;
        }
        if (filename.find("saturn") != std::string::npos && regularFile(path)) {
            status.saturnMglInstalled = true;
            const bool compatible = compatibleDiscMgl(path, "Saturn");
            if (status.saturnMgl.empty() || compatible) status.saturnMgl = path;
            status.saturnMglCompatible = status.saturnMglCompatible || compatible;
        }
    }
    return status;
}

const char *DiscCoreManager::projectUrl() {
    return "https://github.com/theshaneobrien/mister-disc-drive-support";
}
