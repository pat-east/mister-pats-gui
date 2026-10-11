#pragma once

#include <functional>
#include <string>

#include "Input.h"
#include "OnScreenKeyboard.h"
#include "Geometry.h"

class Canvas;
class Theme;

// A compact, controller-operated search field shared by game collection screens.
class GameSearch {
public:
    using SubmitHandler = std::function<void(const std::string &)>;

    bool handle(Action action, bool atTop, const SubmitHandler &onSubmit);
    void renderField(Canvas &canvas, Theme &theme, const Rect &area,
                     const std::string &placeholder = "Search games…") const;
    void renderKeyboard(Canvas &canvas, Theme &theme, const Rect &area) const;

    bool keyboardVisible() const { return keyboardVisible_; }
    bool wantsBack() const { return keyboardVisible_ || focused_; }
    bool usesStartAction() const { return keyboardVisible_ || focused_; }
    bool focused() const { return focused_; }
    bool hasQuery() const { return hasSubmitted_ && !submittedQuery_.empty(); }
    const std::string &query() const { return submittedQuery_; }
    void setQuery(const std::string &query);
    std::string hints(const std::string &normalHints) const;

    static bool matches(const std::string &title, const std::string &system,
                        const std::string &query, bool includeSystem);

private:
    void submit(const SubmitHandler &onSubmit);

    OnScreenKeyboard keyboard_{64};
    std::string submittedQuery_;
    bool hasSubmitted_ = false;
    bool focused_ = false;
    bool keyboardVisible_ = false;
};
