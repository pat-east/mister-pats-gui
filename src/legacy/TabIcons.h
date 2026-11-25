#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Image.h"
#include "Paths.h"
#include "TopBar.h"

// The small, explicitly selected Bootstrap Icons used by the top-level tabs.
class TabIcons {
public:
    static constexpr int kIconSize = 26;
    bool refresh();
    const Image *imageFor(Tab tab) const;
    bool allAvailable() const;
    std::vector<std::string> missingFiles() const;
    static bool knownFile(const std::string &filename);
    static const std::vector<std::string> &files();

    static constexpr const char *kDefaultDirectory = MISTER_PAT_ROOT "/tab-icons";

private:
    std::vector<std::pair<Tab, std::string>> paths_;
    std::vector<ImagePtr> images_;
};
