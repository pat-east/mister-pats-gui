#include "TabIcons.h"

#include <fstream>
#include <sys/stat.h>

namespace {

uint32_t littleEndian32(const unsigned char *bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
           (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

bool expectedBmp(const std::string &path) {
    unsigned char header[70] = {};
    std::ifstream input(path, std::ios::binary);
    if (!input.read(reinterpret_cast<char *>(header), sizeof(header))) return false;
    return header[0] == 'B' && header[1] == 'M' &&
           littleEndian32(header + 14) == 40 &&
           littleEndian32(header + 18) == TabIcons::kIconSize &&
           littleEndian32(header + 22) == TabIcons::kIconSize &&
           header[28] == 24 && header[29] == 0 && littleEndian32(header + 30) == 0;
}

} // namespace

const std::vector<std::string> &TabIcons::files() {
    static const std::vector<std::string> names = {
        "home.bmp", "favorites.bmp", "consoles.bmp", "arcade.bmp", "games.bmp", "settings.bmp"};
    return names;
}

bool TabIcons::knownFile(const std::string &filename) {
    for (const std::string &name : files())
        if (name == filename) return true;
    return false;
}

bool TabIcons::refresh() {
    static const std::pair<Tab, const char *> mapping[] = {
        {Tab::Home, "home.bmp"}, {Tab::Favorites, "favorites.bmp"},
        {Tab::Systems, "consoles.bmp"}, {Tab::Arcade, "arcade.bmp"},
        {Tab::Games, "games.bmp"}, {Tab::Settings, "settings.bmp"},
    };
    paths_.clear();
    images_.clear();
    bool complete = true;
    for (const auto &entry : mapping) {
        paths_.push_back({entry.first, entry.second});
        const std::string path = std::string(kDefaultDirectory) + "/" + entry.second;
        ImagePtr image = expectedBmp(path) ? Image::load(path) : nullptr;
        if (!image || !image->valid() || image->width() != kIconSize ||
            image->height() != kIconSize || !image->opaque()) {
            complete = false;
            image.reset();
        }
        images_.push_back(std::move(image));
    }
    return complete;
}

const Image *TabIcons::imageFor(Tab tab) const {
    for (size_t i = 0; i < paths_.size(); ++i)
        if (paths_[i].first == tab) return images_[i].get();
    return nullptr;
}

bool TabIcons::allAvailable() const {
    if (images_.size() != files().size()) return false;
    for (const ImagePtr &image : images_)
        if (!image) return false;
    return true;
}

std::vector<std::string> TabIcons::missingFiles() const {
    std::vector<std::string> missing;
    for (size_t i = 0; i < files().size(); ++i)
        if (i >= images_.size() || !images_[i]) missing.push_back(files()[i]);
    return missing;
}
