#pragma once

// The empty pages of the new GUI. Every list, grid and table that has nothing to show draws ui::drawEmptyPage (the
// faint panel, the same column): why it is empty and what to do next. The wording of the page most tabs share, a
// search that found nothing or is still looking, is built here once.

#include <string>

#include "ui-framework/kit/EmptyPage.h"
#include "ui-framework/kit/Text.h"

namespace patsgui {

// A search that is still running ("Searching...") or found nothing ("No games found"). `system`: the name of the
// system that was searched, empty when every game was.
inline ui::EmptyPageSpec searchEmptyPage(bool searchDone, const std::string &query, const std::string &system = std::string()) {
    ui::EmptyPageSpec s;
    if (!searchDone) {
        s.title = ui::Text::key("empty.searching.title");
        s.why = ui::Text::key("empty.searching.why");
        return s;
    }
    s.title = ui::Text::key("empty.search.title");
    s.why = ui::Text::data(system.empty() ? ui::format("empty.search.why", {query})
                                          : ui::format("empty.search.whySystem", {system, query}));
    s.next = ui::Text::key("empty.search.next");
    return s;
}

// A page with fixed words (catalog keys).
inline ui::EmptyPageSpec emptyPage(const char *title, const char *why, const char *next = nullptr) {
    ui::EmptyPageSpec s;
    s.title = ui::Text::key(title);
    s.why = ui::Text::key(why);
    if (next) s.next = ui::Text::key(next);
    return s;
}

}  // namespace patsgui
