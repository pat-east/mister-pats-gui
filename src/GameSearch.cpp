#include "GameSearch.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

#include "Canvas.h"
#include "ButtonLabels.h"
#include "Theme.h"

void GameSearch::submit(const SubmitHandler &onSubmit) {
    submittedQuery_ = keyboard_.value();
    const size_t first = submittedQuery_.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) submittedQuery_.clear();
    else submittedQuery_ = submittedQuery_.substr(
        first, submittedQuery_.find_last_not_of(" \t\r\n") - first + 1);
    keyboard_.setValue(submittedQuery_);
    hasSubmitted_ = true;
    keyboardVisible_ = false;
    focused_ = false;
    if (onSubmit) onSubmit(submittedQuery_);
}

void GameSearch::setQuery(const std::string &query) {
    submittedQuery_ = query;
    keyboard_.setValue(query);
    hasSubmitted_ = true;
    focused_ = false;
    keyboardVisible_ = false;
}

bool GameSearch::handle(Action action, bool atTop, const SubmitHandler &onSubmit) {
    if (keyboardVisible_) {
        if (action == Action::Back) {
            keyboardVisible_ = false;
            keyboard_.setValue(submittedQuery_);
            return true;
        }
        if (action == Action::Start) {
            submit(onSubmit);
            return true;
        }
        if (action == Action::ToggleFavorite || action == Action::Select) return true;
        const OnScreenKeyboard::Result result = keyboard_.handle(action);
        if (result == OnScreenKeyboard::Result::Submitted) submit(onSubmit);
        return true;
    }

    if (focused_) {
        if (action == Action::Back && hasQuery()) {
            submittedQuery_.clear();
            keyboard_.setValue("");
            hasSubmitted_ = true;
            if (onSubmit) onSubmit(submittedQuery_);
            return true;
        }
        if (action == Action::Down) {
            focused_ = false;
            return true;
        }
        if (action == Action::Confirm) {
            keyboard_.setValue(submittedQuery_);
            keyboardVisible_ = true;
            return true;
        }
        if (action == Action::Start) {
            keyboard_.setValue(submittedQuery_);
            submit(onSubmit);
            return true;
        }
        return action != Action::Back;
    }

    if (action == Action::Up && atTop) {
        focused_ = true;
        return true;
    }
    return false;
}

void GameSearch::renderField(Canvas &canvas, Theme &theme, const Rect &area,
                             const std::string &placeholder) const {
    canvas.fillRoundedRect(area, theme.radius(), theme.surfaceHi);
    if (focused_ || keyboardVisible_)
        canvas.strokeRoundedRect(area, theme.radius(), theme.px(2), theme.accent);
    const std::string value = keyboardVisible_ ? keyboard_.value() : submittedQuery_;
    const std::string shown = value.empty() ? placeholder : value;
    const std::string display = shown + (keyboardVisible_ ? "_" : "");
    const int size = theme.sizeBody();
    const int inset = theme.px(18);
    const bool showClearHint = focused_ && hasQuery();
    const int editWidth = theme.px(showClearHint ? 220 : 124);
    theme.regular().draw(canvas, area.x + inset,
                         area.y + (area.h - theme.regular().lineHeight(size)) / 2,
                         theme.regular().elide(display, size,
                             std::max(theme.px(80), area.w - inset * 2 - editWidth)),
                         size, value.empty() ? theme.textMuted : theme.textPrimary);
    theme.bold().drawCentered(canvas,
        Rect{area.right() - editWidth - theme.px(8), area.y, editWidth, area.h},
        formatButtonLabels(showClearHint ? "B  CLEAR   A  EDIT" : "A  EDIT"),
        theme.sizeSmall(),
        focused_ || keyboardVisible_ ? theme.textPrimary : theme.textMuted);
}

void GameSearch::renderKeyboard(Canvas &canvas, Theme &theme, const Rect &area) const {
    if (!keyboardVisible_) return;
    const int padding = theme.px(12);
    const Rect keyboardArea = area.inset(padding);
    const Rect keys = keyboard_.bounds(theme, keyboardArea);
    const Rect panel{keys.x - padding, keys.y - padding, keys.w + padding * 2,
                     keys.h + padding * 2};
    canvas.fillRoundedRect(panel, theme.radius(), theme.surface.withAlpha(235));
    keyboard_.render(canvas, theme, keyboardArea);
}

std::string GameSearch::hints(const std::string &normalHints) const {
    if (keyboardVisible_)
        return "D-pad Choose key   A Enter   X Delete   Y Space   Start Search   B Close";
    const std::string viewHint = normalHints.find("Y Change view") != std::string::npos
        ? "   Y Change view" : "";
    if (focused_) return (hasQuery() ? std::string("A Edit search   Start Search   B Clear   ↓ Game list")
                                     : std::string("A Enter search   Start Search   ↓ Game list")) + viewHint;
    return "↑ Open search   " + normalHints;
}

bool GameSearch::matches(const std::string &title, const std::string &system,
                         const std::string &query, bool includeSystem) {
    std::istringstream input(query);
    std::vector<std::string> terms;
    for (std::string term; input >> term;) {
        std::transform(term.begin(), term.end(), term.begin(), [](unsigned char ch) {
            return char(std::tolower(ch));
        });
        terms.push_back(std::move(term));
    }
    if (terms.empty()) return false;

    std::vector<std::string> words;
    auto collectWords = [&words](const std::string &text) {
        std::string word;
        for (unsigned char ch : text) {
            if (ch >= 0x80 || std::isalnum(ch)) word.push_back(char(std::tolower(ch)));
            else if (!word.empty()) { words.push_back(std::move(word)); word.clear(); }
        }
        if (!word.empty()) words.push_back(std::move(word));
    };
    collectWords(title);
    if (includeSystem) collectWords(system);

    for (const std::string &term : terms) {
        bool found = false;
        for (const std::string &word : words) {
            if (word.compare(0, term.size(), term) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}
