#pragma once

#include <functional>
#include <string>

// Letter navigation over an alphabetically ordered list. A console library holds a few
// thousand titles per system, so stepping tile by tile is not a way to reach the letter W.
class Alphabet {
public:
    // The letter a title files under: 'A'..'Z' from its first character, '#' for everything
    // else. Deliberately the first character and not the first *letter*: the list is sorted
    // by the raw name, and a rule that disagrees with that sort would scatter a group across
    // the list instead of keeping it in one block.
    static char initial(const std::string &name);

    // Index of the first entry of the neighbouring letter group. `nameAt` supplies the label
    // for an index so lists of different element types can share this. Returns `cursor`
    // unchanged when there is nowhere left to go.
    static int jump(int cursor, int count, int direction,
                    const std::function<const std::string &(int)> &nameAt);
};
