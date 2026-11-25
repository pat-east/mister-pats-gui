#include "app/Strings.h"

namespace patsgui {

void registerSharedStrings(ui::Catalog &c) {
    // ---- Tab names (also the page title of a tab).
    c.set("tab.disc", "Disc");
    c.set("tab.home", "Home");
    c.set("tab.favorites", "Favorites");
    c.set("tab.systems", "Systems");
    c.set("tab.arcade", "Arcade");
    c.set("tab.games", "Games");
    c.set("tab.settings", "Settings");

    // ---- Hints: one word per concept (src/ui-framework/docs/components/Hints.md).
    c.set("hint.play", "Play");                       // A on a game
    c.set("hint.startGame", "Start game");            // the Start button of the Game options and of the Disc page
    c.set("hint.viewShot", "View screenshot");
    c.set("hint.favorite", "Favorite");               // Hold X on a game that is not a favorite
    c.set("hint.unfavorite", "Remove favorite");      // Hold X on a favorite
    c.set("hint.removeRecent", "Remove");             // Hold B on a recently played game
    c.set("hint.jump", "Jump to letter");
    c.set("hint.page", "Page");                       // Left/Right in a list: one page
    c.set("hint.view", "Change view");
    c.set("hint.editSearch", "Edit search");          // A on the search field that holds a query
    c.set("hint.clearSearch", "Clear search");        // B on the search field that holds a query

    // ---- The three views of a game list (also the names in Settings).
    c.set("view.grid", "Grid");
    c.set("view.small", "Small grid");
    c.set("view.list", "List with details");

    // ---- The header info: "Grid . 3 of 42", and while a search still runs "Grid . Searching... 12 found".
    c.set("grid.info", "{1} \xC2\xB7 {2}");
    c.set("grid.info.searching", "{1} \xC2\xB7 Searching\xE2\x80\xA6 {2} found");

    // ---- The facts line under a game's title in the list with details ("system . year . players").
    c.set("detail.players.one", "{2} player");
    c.set("detail.players.other", "{2} players");

    // ---- Search: the placeholder says what is searched; the keyboard's title is the same words.
    c.set("search.all", "Search all games");
    c.set("search.system", "Search {1}");
    c.set("search.favorites", "Search favorites");

    // ---- Search results that are empty or still coming.
    c.set("empty.searching.title", "Searching\xE2\x80\xA6");
    c.set("empty.searching.why", "Results appear here as they are found.");
    c.set("empty.search.title", "No games found");
    c.set("empty.search.why", "No games match \xE2\x80\x9C{1}\xE2\x80\x9D.");
    c.set("empty.search.whySystem", "No games in {1} match \xE2\x80\x9C{2}\xE2\x80\x9D.");
    c.set("empty.search.next", "Go up to the search field to change or clear the search.");

    // ---- Removal questions (a favorite, a recently played game).
    c.set("confirm.favorite.title", "Remove from favorites?");
    c.set("confirm.favorite.body", "{1} stays in your library. You can mark it as a favorite again.");
    c.set("confirm.recent.title", "Remove from Recently played?");
    c.set("confirm.recent.body", "{1} stays in your library. It only disappears from this list.");

    // ---- Notices (toasts): what happened, in a few words. {1} is the game's title.
    c.set("note.favorite.added", "{1} added to favorites");
    c.set("note.favorite.removed", "{1} removed from favorites");
    c.set("note.recent.removed", "{1} removed from Recently played");
    c.set("note.letter", "Games starting with {1}");

    // ---- A picture that is not there (box art, a disc's artwork, a video's still).
    c.set("picture.loading", "Loading picture\xE2\x80\xA6");
    c.set("picture.none", "No picture available");
}

}  // namespace patsgui
