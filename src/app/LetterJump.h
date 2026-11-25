#pragma once

// The one letter jump of the new GUI (the trigger buttons): the rule of the Classic GUI's `Alphabet` (src/legacy/Alphabet.h),
// kept here because src/app/ never includes src/legacy/. Home, Favorites, Games and Arcade all use it, and the Library's
// `jumpLetter()` (the host's adapter calls Classic's Alphabet itself) answers with the same rule, so the same button
// lands on the same game on every tab:
//
//   - a title files under its first CHARACTER: 'A'..'Z' (case folded); every other first character, digits
//     included, is one group '#', announced as "0-9" (the list is sorted by the raw name, a rule that disagrees with
//     that sort would scatter a group);
//   - forward goes to the first entry of the next group; backward goes to the top of the current group first and
//     only from there to the group before; nowhere left to go: the cursor stays;
//   - the jump says "Games starting with X" (a notice, the same words and the same toast everywhere), and the row of
//     the letter goes to the top of the page (GridPage / ListDetail showRowAtTop()).
//
// A tab that holds its list uses step(); Games and Arcade ask the Library (a window of the list) and announce().

#include <functional>
#include <string>
#include <vector>

#include "app/Library.h"
#include "app/Notice.h"

namespace patsgui {

class LetterJump {
public:
    using NameAt = std::function<const std::string &(int)>;

    // The group a title files under: 'A'..'Z', or '#'.
    static char initial(const std::string &name);
    // Index of the first entry of the neighbouring group; `cursor` when there is nowhere left to go.
    static int jump(int cursor, int count, int direction, const NameAt &nameAt);
    // The group as the notice says it: "A", or "0-9" for '#'.
    static std::string label(char initial);

    // A jump in a list the tab holds: the new cursor (the old one when it did not move); announces the new letter.
    static int step(const std::vector<GameInfo> &list, int cursor, int direction, Notice *notice);
    // "Games starting with X" for the game at the new cursor.
    static void announce(Notice *notice, const std::string &title);
};

}  // namespace patsgui
