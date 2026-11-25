#pragma once

// The one search flow of the new GUI (Home, Favorites, Games and the Arcade list): the search field of the page
// header, the on-screen keyboard (ui::TextEntryDialog), the query, and the Library search that belongs to it.
// A tab keeps its own focus model (which row or zone has the cursor) and tells the header whether the cursor is on
// the field; everything else is decided here, once, for every tab (owner decisions, 2026-11-23):
//
//   D2  the search runs when the user is done (Done on the keyboard), never on every key;
//   D3  B on the field clears a query (the first B clears, a second B goes on, because Clear leaves the field with
//       no query and an unused Back goes to Home); Cancel on the keyboard changes nothing;
//   D4  Start opens the search from anywhere on the tab; A opens it while the cursor is on the field;
//   D8  the Library search belongs to this header (its own owner id), so tabs never share one answer;
//   D14 the placeholder, and the keyboard's title, say what is searched ("Search all games", "Search {system}",
//       "Search favorites").
//
// Use:
//   SearchHeader search_{library_};                       // member
//   search_.setPlaceholder(ui::Text::key("search.all"));   // every draw: what is searched now
//   // draw():   search_.fill(headerSpec, cursorIsOnField);  ... after the tab: search_.drawDialog(p, t, p.bounds());
//   // handle(): SearchHeader::Out out;
//   //           if (search_.handle(event, cursorIsOnField, out)) { if (out != None) tab-applies(search_.query()); return true; }
//   // hints():  if (search_.typing()) return search_.hints();
//   // locked(): search_.typing() || ...
//   // Library search (library tabs):  search_.run(scope) after Submitted/Cleared, search_.results() to read the answer.
// Call handle() only when nothing else is open (a modal, the Game options); while typing it takes every event.

#include <string>
#include <vector>

#include "app/Library.h"
#include "app/Tab.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/TextEntryDialog.h"

namespace patsgui {

class SearchHeader {
public:
    explicit SearchHeader(Library &library);

    enum class Out { None, Submitted, Cleared };

    // What the field says while it is empty, and the keyboard's title. Cheap; set it on every draw.
    void setPlaceholder(const ui::Text &placeholder) { placeholder_ = placeholder; }

    // Routes one event. Returns true when the header used it; `out` says what changed (the query is then
    // query()). `fieldFocused`: the cursor is on the search field.
    bool handle(Event event, bool fieldFocused, Out &out);

    bool typing() const { return typing_; }
    const std::string &query() const { return query_; }
    bool hasQuery() const { return !query_.empty(); }
    // The tab sets the query itself (a scope change ends the search); the keyboard is closed.
    void setQuery(const std::string &query) { query_ = query; typing_ = false; }
    // Closes the keyboard (the tab is left).
    void close() { typing_ = false; }

    // Fills the header's search field (placeholder, value, focus outline unless the keyboard is open).
    void fill(ui::PageHeaderSpec &header, bool fieldFocused) const;
    // The keyboard dialog; draw it above the tab. The shell shows hints() while the tab is locked().
    void drawDialog(ui::Painter &p, const ui::Theme &t, const ui::Rect &canvas) const;
    // The keyboard's hints (Choose key, A Enter, X Delete, Y Space, Start Search, B Close).
    std::vector<ui::Hint> hints() const { return entry_.hints(); }

    // ---- The Library search of this header (library tabs). `scope` empty = every system.
    void run(const SystemId &scope) { library_.setSearch(query_, scope, owner_); }
    SearchState results() const { return library_.searchState(owner_); }

    // A header with a query filters or searches; the tab may also match a list it holds itself with the library's
    // own rule, so every list answers the same way.
    bool matches(const GameInfo &g) const { return library_.matchesSearch(g.title, g.systemName, query_); }

private:
    void open();

    Library &library_;
    int owner_;
    ui::Text placeholder_;
    std::string query_;
    bool typing_ = false;
    ui::TextEntryDialog entry_;
};

}  // namespace patsgui
