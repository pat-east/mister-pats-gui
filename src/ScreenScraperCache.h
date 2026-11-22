#pragma once

#include <map>
#include <mutex>
#include <string>

// What a ScreenScraper lookup found for one game, stored so the same game is never looked up
// again. Only definitive answers are stored: a network failure or a rate limit is not a result.
struct ScreenScraperResult {
    enum class Status {
        Video,     // the game is known and has a video (the fields below describe it)
        NoVideo,   // the game is known and has no video
        NoGame,    // ScreenScraper has no game by this title for this system
    };
    Status status = Status::NoGame;
    long long gameId = 0;
    std::string title;       // the name ScreenScraper uses
    std::string videoType;   // "video-normalized" or "video"
    long long size = 0;      // bytes, as ScreenScraper reports them
    unsigned long crc = 0;   // CRC-32 of the file, as ScreenScraper reports it
    long long fetched = 0;   // when it was looked up, Unix seconds
};

// The stored results (one small text file per system) and the downloaded video files.
//
//   <directory>/results/<system>.tsv     one line per game, appended to as results arrive
//   <directory>/videos/<id>-<type>.mp4   a video; <id>-<type>.part while it downloads
//
// Results hold no credentials and no URLs. The file of a system is read once, on first use;
// after that every answer comes from memory, so scrolling through a list never touches the card.
class ScreenScraperCache {
public:
    explicit ScreenScraperCache(std::string directory);

    // The stored result for a game (system key and title key as in ScreenScraperNames).
    bool find(const std::string &systemKey, const std::string &titleKey, ScreenScraperResult &out);
    // Remembers a result in memory and appends it to the system's file.
    bool store(const std::string &systemKey, const std::string &titleKey,
               const ScreenScraperResult &result);
    // Forgets every stored result (the video files stay). Returns how many were forgotten.
    size_t clearResults();

    std::string videoPath(long long gameId, const std::string &type) const;
    std::string partialPath(long long gameId, const std::string &type) const;
    bool hasVideo(long long gameId, const std::string &type) const;

    // Total size of the finished videos, and room-making for a new one: deletes the oldest videos
    // until `incomingBytes` fit under `limitBytes` (0 means no limit). False if they cannot fit
    // even with nothing else stored. Only files this class names are ever removed.
    long long videoBytes() const;
    bool makeRoom(long long incomingBytes, long long limitBytes) const;
    // Free space on the card holding the cache; -1 if it cannot be told.
    long long freeBytes() const;

    const std::string &directory() const { return directory_; }

private:
    using SystemResults = std::map<std::string, ScreenScraperResult>;

    SystemResults &load(const std::string &systemKey);   // caller holds mutex_
    std::string resultsPath(const std::string &systemKey) const;

    std::string directory_;
    mutable std::mutex mutex_;
    std::map<std::string, SystemResults> systems_;
};
