#pragma once

#include <string>

// Title and system matching rules for ScreenScraper lookups. No I/O, usable from tests.
namespace ScreenScraperNames {

// The title to send to a search: file extension and bracketed tags such as "(USA)" or "[!]" are
// removed, and a trailing ", The" moves to the front ("Legend of Zelda, The" -> "The Legend of
// Zelda"), which is how ScreenScraper spells such titles.
std::string searchTitle(const std::string &gameName);

// A comparison key: ASCII letters and digits only, lower case, accents folded, "&" read as
// "and", bracketed tags and a trailing ", The" handled as in searchTitle. Two names that
// differ only in case, spacing, punctuation or accents have the same key.
std::string titleKey(const std::string &name);

// The ScreenScraper system id for a game system, by the system's database key (the folder name
// MiSTer uses, compared without regard to case). 0 when the system is not one ScreenScraper
// can be asked about: computers whose "games" are boot disks, Arcade (MRA names are not game
// titles), and anything not in the table.
int systemId(const std::string &systemKey);

// A short key for the stored results of a system: the database key with characters that are not
// safe in a file name replaced.
std::string fileSafe(const std::string &text);

}  // namespace ScreenScraperNames
