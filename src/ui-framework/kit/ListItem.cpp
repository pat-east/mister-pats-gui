#include "ui-framework/kit/Components.h"

#include <algorithm>

namespace ui {

int listItemFrameHeight(const Theme &t, bool twoLine, Density d) {
    return t.px(twoLine ? 62 : d == Density::Dense ? 46 : 56);
}
int listItemPitch(const Theme &t, bool twoLine, Density d) {
    const bool dense = twoLine || d == Density::Dense;
    return listItemFrameHeight(t, twoLine, d) + t.px(dense ? Space::rowGapDense : Space::rowGapRoomy);
}

static void drawButtonGlyph(Painter &p, const Theme &t, int right, const Rect &frame, PadButton button) {
    // The confirm glyph in the user's layout (Theme::buttonLayout): a round outline with the button's name.
    const std::string letter = buttonName(t.buttonLayout(), button);
    const int d = t.px(26);
    const Rect box{right - d, frame.y + (frame.h - d) / 2, d, d};
    p.strokeRoundedRect(box, d / 2, std::max(1, t.px(2)), t.palette.textMuted);
    drawLabel(p, t, box, letter, TextRole::Small, TextTier::Primary, Align::Center, true);
}

void drawListItem(Painter &p, const Theme &t, const Rect &frame, const ListItemSpec &spec,
                  const ListItemState &state) {
    const int radius = t.px(Radius::row);
    const int inset = t.px(Space::inset);
    const int insetRight = t.px(Space::insetRight);
    const bool heading = spec.kind == ListItemKind::Heading;
    const bool twoLine = spec.kind == ListItemKind::Choice ||
                         (spec.kind == ListItemKind::Entry && !spec.detail.empty());

    if (heading) {
        const Rect label{frame.x + inset, frame.y, frame.w - inset - insetRight, frame.h - t.px(6)};
        drawLabel(p, t, label, spec.title.str(), TextRole::Body, TextTier::Muted, Align::Left, true);
        p.fillRect({frame.x + inset, frame.bottom() - t.px(6), frame.w - inset - insetRight, 1},
                   t.palette.textMuted.withAlpha(80));
        return;
    }

    // High contrast: a plain white border on every control row, so each row is outlined even when it is not
    // focused (the focus outline is drawn over it).
    if (t.borders() && spec.kind != ListItemKind::Entry)
        p.strokeRoundedRect(frame, radius, std::max(1, t.px(2)), Color::rgb(0xFFFFFF));

    // Row chrome: raised fill on the cursor row of the active pane; a remembered position in an
    // inactive pane is a faint fill only.
    if (state.cursor && state.paneActive) {
        p.fillRoundedRect(frame, radius, t.palette.surfaceHi);
        drawFocusMarker(p, t, frame);
        drawFocusFrame(p, t, frame, radius);
    } else if (state.cursor) {
        p.fillRoundedRect(frame, radius, t.palette.surface.withAlpha(190));
    }

    const TextTier titleTier = spec.disabled ? TextTier::Disabled
                                             : (state.cursor ? TextTier::Primary : TextTier::Muted);
    int textLeft = frame.x + inset;
    int right = frame.right() - insetRight;

    // Right slot first, so the title knows how much room is left.
    int slotWidth = 0;
    switch (spec.kind) {
        case ListItemKind::Nav:
        case ListItemKind::Action:
            drawButtonGlyph(p, t, right, frame, PadButton::A);
            slotWidth = t.px(26);
            if (spec.kind == ListItemKind::Nav && !spec.value.empty()) {   // Nav with a value: it shows where A leads
                const std::string &v = spec.value.str();
                const int w = labelWidth(p, t, v, TextRole::Body), at = right - slotWidth - t.px(12);
                drawLabelColor(p, t, {at - w, frame.y, w, frame.h}, v, TextRole::Body,
                               spec.disabled ? t.text(TextTier::Disabled) : t.palette.accentText, Align::Right);
                slotWidth += t.px(12) + w;
            }
            break;
        case ListItemKind::Value: {
            const std::string &v = spec.value.str();
            slotWidth = labelWidth(p, t, v, TextRole::Body);
            drawLabelColor(p, t, {right - slotWidth, frame.y, slotWidth, frame.h}, v, TextRole::Body,
                           spec.disabled ? t.text(TextTier::Disabled) : t.palette.accentText, Align::Right);
            break;
        }
        case ListItemKind::Switch: {
            const std::string word = spec.checked ? Text::key("switch.on").str() : Text::key("switch.off").str();
            const int box = checkboxSize(t, false);
            const int wordW = std::max(labelWidth(p, t, Text::key("switch.on").str(), TextRole::Body),
                                       labelWidth(p, t, Text::key("switch.off").str(), TextRole::Body));
            drawLabel(p, t, {right - wordW, frame.y, wordW, frame.h}, word, TextRole::Body,
                      spec.disabled ? TextTier::Disabled : TextTier::Primary, Align::Right);
            drawCheckbox(p, t, {right - wordW - t.px(10) - box, frame.y + (frame.h - box) / 2, box, box}, spec.checked);
            slotWidth = wordW + t.px(10) + box;
            break;
        }
        case ListItemKind::Info: {
            const std::string &v = spec.value.str();
            slotWidth = labelWidth(p, t, v, TextRole::Body);
            drawLabel(p, t, {right - slotWidth, frame.y, slotWidth, frame.h}, v, TextRole::Body, TextTier::Muted,
                      Align::Right);
            break;
        }
        case ListItemKind::Choice: {
            const std::string &s = spec.status.str();
            slotWidth = labelWidth(p, t, s, TextRole::Body, true);
            drawLabelColor(p, t, {right - slotWidth, frame.y, slotWidth, frame.h}, s, TextRole::Body,
                           t.tone(spec.statusTone), Align::Right, true);
            const int box = checkboxSize(t, true);
            drawCheckbox(p, t, {frame.x + inset, frame.y + (frame.h - box) / 2, box, box}, spec.checked);
            textLeft = frame.x + inset + box + inset;
            break;
        }
        case ListItemKind::Entry: {
            if (spec.favorite) {
                const int w = t.px(24);
                drawLabelColor(p, t, {right - w, frame.y, w, frame.h}, "*", TextRole::Body, t.palette.accentText, Align::Center, true);
                slotWidth = w;
            }
            break;
        }
        case ListItemKind::Heading:
            break;
    }

    const int gap = slotWidth > 0 ? t.px(Space::gap) : 0;
    const int textW = std::max(0, right - slotWidth - gap - textLeft);
    if (twoLine) {
        const int lineA = p.lineHeight(FontRole::Bold, t.size(TextRole::Body));
        // A row without a second line centers its title on the row, level with the mark on the left.
        const int lineB = spec.detail.empty() ? 0 : p.lineHeight(FontRole::Regular, t.size(TextRole::Small));
        const int top = frame.y + (frame.h - lineA - lineB) / 2;
        const bool entry = spec.kind == ListItemKind::Entry;   // content rows: regular title, tier by cursor
        drawLabel(p, t, {textLeft, top, textW, lineA}, spec.title.str(), TextRole::Body,
                  entry ? titleTier : TextTier::Primary, Align::Left, !entry);
        drawLabel(p, t, {textLeft, top + lineA, textW, lineB}, spec.detail.str(), TextRole::Small, TextTier::Muted);
    } else {
        drawLabel(p, t, {textLeft, frame.y, textW, frame.h}, spec.title.str(), TextRole::Body, titleTier);
    }
}

}  // namespace ui
