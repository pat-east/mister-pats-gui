#include "ui-framework/kit/PageHeader.h"

#include <algorithm>

namespace ui {

int pageHeaderHeight(const Theme &t) { return t.px(PageHeaderMetrics::height); }

Rect pageHeaderSearchRect(const Theme &t, const Rect &area) {
    const int w = t.px(PageHeaderMetrics::fieldWidth), h = t.px(PageHeaderMetrics::fieldHeight);
    return {t.screenWidth() / 2 - w / 2, area.y + (t.px(PageHeaderMetrics::rowHeight) - h) / 2, w, h};
}

Rect drawPageHeader(Painter &p, const Theme &t, const Rect &area, const PageHeaderSpec &spec) {
    const int headerH = pageHeaderHeight(t), rowH = t.px(PageHeaderMetrics::rowHeight);
    const int gap = t.px(PageHeaderMetrics::sideGap);
    const std::string &title = spec.title.str();
    const std::string &info = spec.info.str();
    const bool hasTitle = !spec.title.empty(), hasInfo = !spec.info.empty();

    Rect titleArea{area.x, area.y, 0, rowH}, infoArea{area.right(), area.y, 0, rowH};
    if (spec.hasSearch) {
        // The field sits in the middle; the title and the info share what is left of each side.
        const Rect field = pageHeaderSearchRect(t, area);
        titleArea.w = std::max(0, field.x - gap - area.x);
        infoArea.w = std::max(0, area.right() - (field.right() + gap));
        infoArea.x = area.right() - infoArea.w;

        TextFieldSpec f;
        f.placeholder = spec.search.placeholder;
        f.value = spec.search.value;
        f.focused = spec.search.focused;
        drawTextField(p, t, field, f);
    } else {
        // No field: the info elides first (down to a third of the row), then the title.
        const int total = area.w;
        const int infoWant = hasInfo ? labelWidth(p, t, info, TextRole::Body) : 0;
        const int titleWant = hasTitle ? labelWidth(p, t, title, TextRole::Heading, true) : 0;
        const int infoW = hasInfo ? std::min(infoWant, std::max(total / 3, total - titleWant - gap)) : 0;
        infoArea.w = infoW;
        infoArea.x = area.right() - infoW;
        titleArea.w = total - infoW - (infoW > 0 ? gap : 0);
    }
    if (hasTitle && titleArea.w > 0)
        drawLabel(p, t, titleArea, title, TextRole::Heading, TextTier::Primary, Align::Left, true);
    if (hasInfo && infoArea.w > 0)
        drawLabel(p, t, infoArea, info, TextRole::Body, TextTier::Faint, Align::Right);

    return {area.x, area.y + headerH, area.w, std::max(0, area.h - headerH)};
}

}  // namespace ui
