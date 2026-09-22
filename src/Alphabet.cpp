#include "Alphabet.h"

#include <algorithm>

char Alphabet::initial(const std::string &name) {
    if (name.empty()) return '#';

    const char c = name[0];
    if (c >= 'a' && c <= 'z') return char(c - 32);
    if (c >= 'A' && c <= 'Z') return c;
    return '#';
}

int Alphabet::jump(int cursor, int count, int direction,
                   const std::function<const std::string &(int)> &nameAt) {
    if (count < 2 || direction == 0) return cursor;

    int index = std::min(std::max(cursor, 0), count - 1);
    const char here = initial(nameAt(index));

    if (direction > 0) {
        while (index < count - 1 && initial(nameAt(index + 1)) == here) ++index;
        if (index < count - 1) ++index;
        return index;
    }

    // Backwards lands on the top of the current letter first, which is what someone in the
    // middle of a long group wants. Only once already there does it reach the letter before.
    while (index > 0 && initial(nameAt(index - 1)) == here) --index;
    if (index < cursor || index == 0) return index;

    --index;
    const char previous = initial(nameAt(index));
    while (index > 0 && initial(nameAt(index - 1)) == previous) --index;
    return index;
}
