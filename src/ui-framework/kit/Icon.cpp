#include "ui-framework/kit/Icon.h"

#include "ui-framework/kit/Icons.h"

namespace ui {

BootstrapIcon::BootstrapIcon(const std::string &name, int width, int height)
    : Icon(width, height), name_(name), mask_(iconMask(name, width, height)) {}

void BootstrapIcon::draw(Painter &p, int x, int y, Color color) const {
    if (mask_) p.drawMask(*mask_, x, y, color);
}

}  // namespace ui
