#pragma once

#include <map>
#include <string>
#include <vector>

class Downloader;

// Finds a game's artwork on the libretro thumbnail server.
//
// The server is a plain directory listing, and the approach here is the one Console Mode
// uses because it is the robust one: fetch the index of a platform once, then match names
// against what is actually there. Building the remote filename from a local title instead
// means reproducing libretro's escaping rules exactly, and being wrong silently.
//
// Matching is by normalised name — bracketed parts dropped, then letters and digits only.
// "Super Mario Bros. 3 (USA) [!]" and "Super Mario Bros 3" both become "supermariobros3".
// Measured against a real 1734-title PlayStation library: 91% of games find a cover.
class LibretroIndex {
public:
    // The server's directory name for a MiSTer game directory, e.g. "PSX" ->
    // "Sony - PlayStation". Empty when the system has no thumbnails there.
    //
    // Every entry in the table was checked against the server's own list of platforms; nine
    // plausible-looking guesses turned out not to exist and were dropped rather than shipped.
    static std::string platformFor(const std::string &systemKey);

    // Fetches and parses the platform's box art listing, using the cached copy when one is
    // there. Roughly 9000 names and 2.5 MB over the wire for a big platform, which is why it
    // is cached on the card rather than fetched per game.
    bool open(const std::string &platform, Downloader &downloader, const std::string &cacheDir);

    // The server's filename for a title, or empty when nothing matches.
    std::string match(const std::string &title) const;

    size_t size() const { return byKey_.size(); }
    const std::string &platform() const { return platform_; }
    const std::string &lastError() const { return error_; }

    // Bracketed parts removed, then everything but letters and digits.
    static std::string normalise(const std::string &title);

    // USA before World before Europe before Japan, everything else last. Decides which entry
    // wins when several titles normalise to the same key.
    static int regionRank(const std::string &filename);

    static constexpr const char *kHost = "https://thumbnails.libretro.com";

private:
    bool parse(const std::string &htmlPath);

    std::string platform_;
    std::map<std::string, std::string> byKey_;   // normalised title -> server filename
    std::string error_;
};
