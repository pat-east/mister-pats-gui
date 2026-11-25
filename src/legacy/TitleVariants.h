#pragma once

// Alternative search names for a disc title. Regional header titles often differ from the
// names artwork databases use ("PANZER DRAGOON ZWEI" is "Panzer Dragoon II Zwei" or
// "Panzer Dragoon 2" elsewhere). Pure string logic, so it can be checked on the host.

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace TitleVariants {

inline std::vector<std::string> splitWords(const std::string &text) {
    std::vector<std::string> words;
    std::string word;
    for (char ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!word.empty()) words.push_back(word);
            word.clear();
        } else word.push_back(ch);
    }
    if (!word.empty()) words.push_back(word);
    return words;
}

inline std::string joinWords(const std::vector<std::string> &words) {
    std::string out;
    for (const std::string &word : words) {
        if (!out.empty()) out.push_back(' ');
        out += word;
    }
    return out;
}

inline std::string upperWord(std::string word) {
    for (char &ch : word) ch = char(std::toupper(static_cast<unsigned char>(ch)));
    return word;
}

inline std::string lowerWord(std::string word) {
    for (char &ch : word) ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return word;
}

// "I" and "X" are left out on purpose: "Mega Man X" is not "Mega Man 10", and a lone "I"
// is as often a letter or pronoun as a number.
inline int numeralValue(const std::string &word) {
    static const char *roman[] = {"II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};
    const std::string upper = upperWord(word);
    for (int i = 0; i < 8; ++i)
        if (upper == roman[i]) return i + 2;
    if (word.size() == 1 && word[0] >= '2' && word[0] <= '9') return word[0] - '0';
    return 0;
}

inline bool isRomanWord(const std::string &word) {
    return !word.empty() && !std::isdigit(static_cast<unsigned char>(word[0]));
}

inline std::string numeralText(int value, bool roman) {
    static const char *romans[] = {"", "", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};
    return roman ? std::string(romans[value]) : std::to_string(value);
}

// The same title with every standalone numeral (not the first word, not after "Disc")
// written the other way: roman to arabic, arabic to roman. Empty when nothing changes.
inline std::string swapNumerals(const std::string &title) {
    std::vector<std::string> words = splitWords(title);
    bool changed = false;
    for (size_t i = 1; i < words.size(); ++i) {
        const int value = numeralValue(words[i]);
        if (value == 0) continue;
        if (lowerWord(words[i - 1]) == "disc") continue;
        words[i] = numeralText(value, !isRomanWord(words[i]));
        changed = true;
    }
    return changed ? joinWords(words) : std::string();
}

// Numbers spelled as a trailing word in another language ("Panzer Dragoon Zwei").
inline int languageNumber(const std::string &word) {
    static const struct { const char *word; int value; } table[] = {
        {"zwei", 2}, {"drei", 3}, {"deux", 2}, {"trois", 3},
        {"dos", 2}, {"tres", 3}, {"due", 2}, {"tre", 3},
    };
    const std::string lower = lowerWord(word);
    for (const auto &entry : table)
        if (lower == entry.word) return entry.value;
    return 0;
}

// Variants that involve a trailing language word. When a numeral already precedes it
// ("Panzer Dragoon II Zwei") the word is dropped; otherwise it is replaced by the
// number it spells ("Panzer Dragoon Zwei" -> "Panzer Dragoon 2" / "II"). Never leaves
// a single word, so "Zwei" alone or "Panzer Zwei" cannot collapse to a different game.
inline std::vector<std::string> languageVariants(const std::string &title) {
    std::vector<std::string> out;
    std::vector<std::string> words = splitWords(title);
    if (words.size() < 3) return out;
    const int value = languageNumber(words.back());
    if (value == 0) return out;
    words.pop_back();
    if (numeralValue(words.back()) != 0) {
        out.push_back(joinWords(words));
    } else {
        std::vector<std::string> arabic = words, roman = words;
        arabic.push_back(numeralText(value, false));
        roman.push_back(numeralText(value, true));
        out.push_back(joinWords(arabic));
        out.push_back(joinWords(roman));
    }
    return out;
}

inline std::string key(const std::string &title) {
    std::string out;
    for (unsigned char ch : title) {
        if (ch >= 0x80) out.push_back(char(ch));
        else if (std::isalnum(ch)) out.push_back(char(std::tolower(ch)));
    }
    return out;
}

// Search names in priority order: the header title, the offline index title, numeral
// variants of both, then language-word variants of both. Inputs must already be cleaned
// of region/disc qualifiers. Duplicates (by alphanumeric key) are dropped.
inline std::vector<std::string> searchNames(const std::string &headerTitle,
                                            const std::string &indexTitle,
                                            size_t limit = 6) {
    std::vector<std::string> names;
    const auto add = [&](const std::string &name) {
        if (name.empty() || names.size() >= limit) return;
        const std::string k = key(name);
        if (k.empty()) return;
        for (const std::string &existing : names)
            if (key(existing) == k) return;
        names.push_back(name);
    };
    const std::string bases[] = {headerTitle, indexTitle};
    for (const std::string &base : bases) add(base);
    for (const std::string &base : bases) add(swapNumerals(base));
    for (const std::string &base : bases) {
        for (const std::string &variant : languageVariants(base)) {
            add(variant);
            add(swapNumerals(variant));
        }
    }
    return names;
}

} // namespace TitleVariants
