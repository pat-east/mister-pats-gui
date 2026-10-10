#include "SearchScreen.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

#include "Canvas.h"

namespace {
constexpr size_t kBuildBatch = 160;
}

SearchScreen::SearchScreen(Context &context) : context_(context), results_(context) {
    results_.setView(GameView::Grid);
}

void SearchScreen::reload() {
    entries_.clear();
    matches_.clear();
    buildPaths_.clear();
    buildSystemPosition_ = buildPathPosition_ = 0;
    indexReady_ = false;
}

bool SearchScreen::matchesQuery(const std::string &gameName, const std::string &systemName,
                                const std::string &query) {
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
            // Keep UTF-8 bytes in a word; only ASCII punctuation and whitespace split words.
            if (ch >= 0x80 || std::isalnum(ch)) {
                word.push_back(char(std::tolower(ch)));
            } else if (!word.empty()) {
                words.push_back(std::move(word));
                word.clear();
            }
        }
        if (!word.empty()) words.push_back(std::move(word));
    };
    collectWords(gameName);
    collectWords(systemName);

    for (const std::string &term : terms) {
        bool found = false;
        for (const std::string &word : words) {
            if (word.compare(0, term.size(), term) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

void SearchScreen::submit() {
    submittedQuery_ = keyboard_.value();
    hasSubmittedSearch_ = true;
    keyboardVisible_ = false;
    inputFocused_ = false;
    if (!indexReady_) {
        submitPending_ = true;
        return;
    }
    submitPending_ = false;
    matches_.clear();
    if (!submittedQuery_.empty()) {
        for (const Entry &entry : entries_)
            if (matchesQuery(entry.game.name, entry.system->name, submittedQuery_))
                matches_.emplace_back(entry.system, entry.game);
    }
    results_.showSearchResults(matches_);
}

void SearchScreen::update(float deltaSeconds) {
    if (!indexReady_) {
        size_t budget = kBuildBatch;
        const auto &systems = context_.library.systems();
        while (budget && buildSystemPosition_ < systems.size()) {
            const GameSystem &system = systems[buildSystemPosition_];
            if (buildPaths_.empty() && buildPathPosition_ == 0)
                buildPaths_ = context_.library.pathsOf(system);
            while (budget && buildPathPosition_ < buildPaths_.size()) {
                const std::string &path = buildPaths_[buildPathPosition_++];
                --budget;
                if (!Library::isSystemFile(system, path))
                    entries_.push_back({&system, Library::makeStub(system, path)});
            }
            if (buildPathPosition_ >= buildPaths_.size()) {
                buildPaths_.clear();
                buildPathPosition_ = 0;
                ++buildSystemPosition_;
            }
        }
        if (buildSystemPosition_ >= systems.size()) {
            indexReady_ = true;
            if (submitPending_) submit();
        }
    }
    results_.update(deltaSeconds);
}

void SearchScreen::handle(Action action) {
    if (keyboardVisible_) {
        if (action == Action::Back) {
            keyboardVisible_ = false;
            keyboard_.setValue(submittedQuery_);
            return;
        }
        if (action == Action::Start) {
            submit();
            return;
        }
        if (action == Action::ToggleFavorite || action == Action::Select) return;
        const auto result = keyboard_.handle(action);
        if (result == OnScreenKeyboard::Result::Submitted) submit();
        return;
    }

    if (inputFocused_) {
        if (action == Action::Down) inputFocused_ = false;
        else if (action == Action::Confirm) keyboardVisible_ = true;
        else if (action == Action::Start) submit();
        return;
    }
    if (action == Action::Up && !results_.canMoveUp()) {
        inputFocused_ = true;
        return;
    }
    if (action == Action::CycleView) return; // Search results always use the grid.
    if (action == Action::Start) {
        submit();
        return;
    }
    results_.handle(action);
}

void SearchScreen::render(Canvas &canvas, const Rect &area, bool /*fullRedraw*/) {
    Theme &theme = context_.theme;
    const int headerHeight = theme.px(66);
    theme.bold().draw(canvas, area.x, area.y, "Search", theme.sizeHeading(), theme.textPrimary);
    const int inputY = area.y + headerHeight;
    const Rect queryFrame{area.x, inputY, area.w, theme.px(62)};
    const bool inputActive = keyboardVisible_ || inputFocused_;
    canvas.fillRoundedRect(queryFrame, theme.radius(), theme.surfaceHi);
    if (inputActive)
        canvas.strokeRoundedRect(queryFrame, theme.radius(), theme.px(2), theme.accent);
    const std::string shown = keyboardVisible_ ? keyboard_.value() : submittedQuery_;
    const std::string query = shown.empty() ? "Search games…" : shown;
    const std::string display = query + (keyboardVisible_ ? "_" : "");
    theme.regular().draw(canvas, queryFrame.x + theme.px(18),
                         queryFrame.y + (queryFrame.h - theme.regular().lineHeight(theme.sizeBody())) / 2,
                         theme.regular().elide(display, theme.sizeBody(), queryFrame.w - theme.px(180)),
                         theme.sizeBody(), submittedQuery_.empty() ? theme.textMuted : theme.textPrimary);
    theme.bold().drawCentered(canvas,
        Rect{queryFrame.right() - theme.px(142), queryFrame.y, theme.px(124), queryFrame.h},
        "A  EDIT", theme.sizeSmall(), inputActive ? theme.textPrimary : theme.textMuted);

    const Rect body{area.x, queryFrame.bottom() + theme.px(12), area.w,
                    area.bottom() - queryFrame.bottom() - theme.px(12)};
    if (keyboardVisible_) {
        const int padding = theme.px(12);
        const Rect keyboardArea = body.inset(padding);
        const Rect keys = keyboard_.bounds(theme, keyboardArea);
        const Rect panel{keys.x - padding, keys.y - padding,
                         keys.w + padding * 2, keys.h + padding * 2};
        canvas.fillRoundedRect(panel, theme.radius(), theme.surface.withAlpha(235));
        keyboard_.render(canvas, theme, keyboardArea);
    } else {
        // App restores the content area each frame because the query field and keyboard
        // overlay change independently of the grid's own repaint cache.
        if (hasSubmittedSearch_) results_.render(canvas, body, true);
        if (!indexReady_) {
            theme.regular().draw(canvas, body.x + theme.px(18), body.y + theme.px(10),
                                 "Preparing game list…", theme.sizeSmall(), theme.textMuted);
        }
    }
}

std::string SearchScreen::hints() const {
    if (keyboardVisible_)
        return "D-pad Type   A Select key   X Delete   Y Space   Start Search   B Cancel";
    return inputFocused_ ? "A Edit search   ↓ Browse results   Start Search"
                         : "D-pad Browse   A Start   X Favorite   ↑ Search   Start Search";
}
