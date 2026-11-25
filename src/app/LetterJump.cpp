#include "app/LetterJump.h"

#include <algorithm>

#include "ui-framework/kit/Text.h"

namespace patsgui {

char LetterJump::initial(const std::string &name) {
    if (name.empty()) return '#';
    const char c = name[0];
    if (c >= 'a' && c <= 'z') return char(c - 32);
    if (c >= 'A' && c <= 'Z') return c;
    return '#';
}

int LetterJump::jump(int cursor, int count, int direction, const NameAt &nameAt) {
    if (count < 2 || direction == 0) return cursor;

    int index = std::min(std::max(cursor, 0), count - 1);
    const char here = initial(nameAt(index));

    if (direction > 0) {
        while (index < count - 1 && initial(nameAt(index + 1)) == here) ++index;
        if (index < count - 1) ++index;
        return index;
    }

    // Backwards lands on the top of the current letter first; only once already there does it reach the letter before.
    while (index > 0 && initial(nameAt(index - 1)) == here) --index;
    if (index < cursor || index == 0) return index;

    --index;
    const char previous = initial(nameAt(index));
    while (index > 0 && initial(nameAt(index - 1)) == previous) --index;
    return index;
}

std::string LetterJump::label(char initial) {
    return initial == '#' ? std::string("0\xE2\x80\x93" "9") : std::string(1, initial);
}

int LetterJump::step(const std::vector<GameInfo> &list, int cursor, int direction, Notice *notice) {
    const int target = jump(cursor, int(list.size()), direction,
                            [&list](int i) -> const std::string & { return list[size_t(i)].title; });
    if (target != cursor) announce(notice, list[size_t(target)].title);
    return target;
}

void LetterJump::announce(Notice *notice, const std::string &title) {
    patsgui::notice(notice, ui::Text::data(ui::format("note.letter", {label(initial(title))})));
}

}  // namespace patsgui
