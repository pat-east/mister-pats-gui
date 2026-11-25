#include "ui-framework/kit/Icons.h"

#include <cstring>
#include <map>

#include "ui-framework/core/Svg.h"

namespace ui {

ImagePtr iconMask(const std::string &name, int width, int height) {
    static std::map<std::string, ImagePtr> cache;
    const std::string key = name + "@" + std::to_string(width) + "x" + std::to_string(height);
    const auto hit = cache.find(key);
    if (hit != cache.end()) return hit->second;

    ImagePtr mask;
    // The table is sorted by name: binary search.
    int lo = 0, hi = kIconSourceCount;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        const int order = std::strcmp(kIconSources[mid].name, name.c_str());
        if (order == 0) {
            SvgIcon icon;
            if (icon.parse(kIconSources[mid].svg)) mask = icon.rasterize(width, height);
            break;
        }
        if (order < 0) lo = mid + 1; else hi = mid;
    }
    cache[key] = mask;   // an unknown name is cached too, so it is looked up once
    return mask;
}

}  // namespace ui
