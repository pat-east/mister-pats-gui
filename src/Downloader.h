#pragma once

#include <string>

// Fetches a URL to a local file by running curl or wget.
//
// Neither libcurl nor any TLS library is linked in: dragging one into a statically linked
// frontend to fetch a few thousand pictures is a poor trade, and both tools are already on
// the device. The first one that works is remembered for the rest of the session, so a box
// missing curl does not pay the probe cost on every image.
class Downloader {
public:
    // Returns true when `destination` holds the fetched bytes. `timeoutSeconds` covers the
    // whole transfer; the connect timeout is fixed and short, because an unreachable host
    // should fail fast rather than stall a scrape of several thousand files.
    bool fetch(const std::string &url, const std::string &destination, int timeoutSeconds = 25);

    // Whether any downloader could be found at all. Only meaningful after a fetch.
    bool available() const { return tool_ != Tool::None; }

    const std::string &lastError() const { return error_; }

    // A URL is only ever handed to a shell after this says yes.
    static bool safeUrl(const std::string &url);

    // Percent-encodes everything outside RFC 3986's unreserved set, including the slash.
    // Used on path components, which is why the slash has to go too.
    static std::string encodeComponent(const std::string &text);

private:
    enum class Tool { Unknown, Curl, CurlInsecure, Wget, None };

    std::string command(Tool tool, const std::string &url, const std::string &destination,
                        int timeoutSeconds) const;
    static std::string certificateArgument();

    Tool tool_ = Tool::Unknown;
    std::string error_;
};
