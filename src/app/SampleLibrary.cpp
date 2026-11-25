#include "app/SampleLibrary.h"

#include "app/LetterJump.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstdlib>

namespace patsgui {

namespace {

uint32_t hashOf(const std::string &s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h;
}

uint32_t shade(uint32_t base, int percent) {
    const int r = std::min(255, int((base >> 16 & 255) * percent / 100));
    const int g = std::min(255, int((base >> 8 & 255) * percent / 100));
    const int b = std::min(255, int((base & 255) * percent / 100));
    return 0xFF000000u | uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(b);
}

std::string number(int n, int width) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%0*d", width, n);
    return buf;
}

}  // namespace

SampleLibrary::SampleLibrary() {
    static const int counts[] = {6, 42, 120, 640, 3000, 10000, 18, 75};
    for (int i = 0; i < 8; ++i) {
        Sys s;
        s.id = std::string("sample-") + char('a' + i);
        s.name = std::string("Example system ") + char('A' + i);
        s.group = i < 5 ? "Consoles" : "Computers";
        s.count = counts[i];
        s.hidden = false;
        s.arcade = (i == 7);
        systems_.push_back(s);
    }
    for (int i : {3, 0, 7, 12}) recents_.push_back({systems_[size_t(i % 8)].id, number(i + 1, 5)});
    for (int i : {1, 4, 9, 2}) favorites_.insert(systems_[size_t(i % 8)].id + "/" + number(i + 1, 5));
    disc_.media = DiscStatus::Media::Readable;
    disc_.console = DiscStatus::Console::PlayStation;
    disc_.device = "/dev/sr0";
    disc_.model = "Example drive";
    disc_.title = "Sample disc";
    disc_.identifier = "SAMP-00001";
    disc_.region = "Region X";
    disc_.coreReady = true;
    disc_.artState = DiscStatus::ArtState::Ready;
    disc_.year = "1999";
    disc_.players = "1-2";
    disc_.overview = "A generic description of the sample disc. It exists only to fill the layout.";
    disc_.screenshotCount = 3;
    disc_.generation = 1;
    setArcadeScan(ArcadeScanState::Phase::Done, 1.0f);
}

void SampleLibrary::setDisc(DiscStatus::Media media, DiscStatus::Console console) {
    disc_.media = media;
    disc_.console = console;
    if (media != DiscStatus::Media::Readable) {
        disc_.title.clear(); disc_.identifier.clear(); disc_.region.clear();
        disc_.artState = DiscStatus::ArtState::Idle;
        disc_.screenshotCount = 0;
        disc_.overview.clear(); disc_.year.clear(); disc_.players.clear();
    }
    if (media == DiscStatus::Media::NoDrive) { disc_.device.clear(); disc_.model.clear(); }
    if (media == DiscStatus::Media::Unreadable) disc_.error = "Sample read error";
    ++disc_.generation;
    ++revision_;
}

const SampleLibrary::Sys *SampleLibrary::find(const SystemId &id) const {
    for (const Sys &s : systems_) if (s.id == id) return &s;
    return nullptr;
}

GameInfo SampleLibrary::make(const Sys &s, int index) const {
    GameInfo g;
    g.ref = {s.id, number(index + 1, 5)};
    g.title = "Sample title " + number(index + 1, 5);
    g.systemName = s.name;
    g.favorite = favorites_.count(key(g.ref)) != 0;
    g.versionCount = (index % 7 == 3) ? 2 : 1;
    // Varied generic metadata for every game, so tables and detail panels have something to show.
    g.year = std::to_string(1978 + (index * 7 + int(s.id.back())) % 25);
    g.manufacturer = "Example maker " + std::string(1, char('A' + (index * 3) % 6));
    g.category = (index % 3 == 0) ? "Example category one" : (index % 3 == 1) ? "Example category two" : "Example category three";
    if (checkedGame(s)) {
        g.checked = true;
        g.coreFound = index % 5 != 2;
        g.romFound = index % 7 != 3;
        g.romCrcOk = g.romFound && index % 9 != 4;
        g.works = g.coreFound && g.romFound && g.romCrcOk;
    }
    return g;
}

void SampleLibrary::setArcadeScan(ArcadeScanState::Phase phase, float progress) {
    for (const Sys &s : systems_) if (s.arcade) scan_.total = s.count;
    scan_.phase = phase;
    scan_.progress = progress;
    scan_.checked = int(scan_.total * progress);
    scan_.entries = scan_.checked;
    scan_.working = scan_.checked * 3 / 5;
    lists_.clear();
    ++revision_;
}

const std::vector<SampleLibrary::Item> &SampleLibrary::list(const SystemId &scope, Works filter) {
    const std::string k = scope + "|" + std::to_string(int(filter)) + "|" + std::to_string(int(scan_.phase));
    auto found = lists_.find(k);
    if (found != lists_.end()) return found->second;
    std::vector<Item> items;
    for (size_t si = 0; si < systems_.size(); ++si) {
        const Sys &s = systems_[si];
        if (!scope.empty() && scope != s.id) continue;
        for (int i = 0; i < s.count; ++i) {
            if (filter != Works::All) {
                const GameInfo g = make(s, i);
                if (!g.checked || (filter == Works::Working) != g.works) continue;
            }
            items.push_back({int(si), i});
        }
    }
    // Title order across systems; within one system the order is already by title.
    if (scope.empty())
        std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
            return a.index < b.index;   // the title is the zero-padded number
        });
    return lists_.emplace(k, std::move(items)).first->second;
}

std::vector<SystemInfo> SampleLibrary::systems() {
    std::vector<SystemInfo> out;
    for (const Sys &s : systems_) {
        SystemInfo i;
        i.id = s.id; i.name = s.name; i.group = s.group;
        i.gameCount = s.count; i.hidden = s.hidden; i.isArcade = s.arcade;
        i.order = int(out.size());
        i.noCore = i.id == "sample-c";   // one system without a core, so the tile's "No core" line shows
        out.push_back(i);
    }
    return out;
}

void SampleLibrary::setSystemHidden(const SystemId &system, bool hidden) {
    for (Sys &s : systems_) if (s.id == system) s.hidden = hidden;
    ++revision_;
}

ui::ImagePtr SampleLibrary::art(const std::string &seed, int w, int h, bool icon) {
    const std::string k = seed + "|" + std::to_string(w) + "x" + std::to_string(h) + (icon ? "i" : "c");
    auto found = artCache_.find(k);
    if (found != artCache_.end()) return found->second;
    // Covers are about 3:4; icons are square. A diagonal gradient of one hue per seed with a lighter
    // frame and a block in the middle, so each picture is recognisable and clearly not real art.
    int iw = icon ? std::min(w, h) : std::min(w, h * 3 / 4);
    int ih = icon ? iw : std::min(h, w * 4 / 3);
    iw = std::max(8, iw); ih = std::max(8, ih);
    const uint32_t hs = hashOf(seed);
    const int hue = int(hs % 360);
    // Hue to RGB, fixed saturation and value.
    const int sector = hue / 60, f = (hue % 60) * 255 / 60;
    const int v = 200, p = 60, q = v - (v - p) * f / 255, t = p + (v - p) * f / 255;
    int r, g, b;
    switch (sector) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    const uint32_t base = 0xFF000000u | uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(b);
    ui::ImagePtr image = std::make_shared<ui::Image>(iw, ih);
    const int border = std::max(2, iw / 40);
    for (int y = 0; y < ih; ++y) {
        uint32_t *row = image->row(y);
        for (int x = 0; x < iw; ++x) {
            const int percent = 45 + 55 * (x + y) / (iw + ih);
            uint32_t c = shade(base, percent);
            const bool frame = x < border || y < border || x >= iw - border || y >= ih - border;
            const bool block = x > iw / 4 && x < iw * 3 / 4 && y > ih / 3 && y < ih * 2 / 3;
            if (frame) c = shade(base, 150);
            else if (block) c = icon ? 0xFFFFFFFFu : shade(base, 60);
            row[x] = c;
        }
    }
    image->setOpaque(true);
    artCache_[k] = image;
    return image;
}

Art SampleLibrary::systemArt(const SystemId &system, ArtSize size) {
    const ArtBounds b = artBounds(size);
    Art a;
    a.image = art(system, b.width, b.height, true);
    return a;
}

int SampleLibrary::gameCount(const SystemId &system, Works filter) {
    if (!system.empty() && !find(system)) return 0;
    if (!system.empty() && filter == Works::All) return find(system)->count;
    return int(list(system, filter).size());
}

std::vector<GameInfo> SampleLibrary::games(const SystemId &system, int offset, int count, Works filter) {
    std::vector<GameInfo> out;
    if (!system.empty() && !find(system)) return out;
    const std::vector<Item> &items = list(system, filter);
    for (int i = std::max(0, offset); i < std::min(int(items.size()), offset + count); ++i)
        out.push_back(make(systems_[size_t(items[size_t(i)].system)], items[size_t(i)].index));
    return out;
}

int SampleLibrary::jumpLetter(const SystemId &system, int fromIndex, int direction, Works filter) {
    // The same rule as the Classic GUI's Alphabet (LetterJump). The sample titles all start with "S", so in the
    // sample data a jump finds no other letter and the cursor stays, exactly as Classic would on such a list.
    const int n = gameCount(system, filter);
    if (n == 0) return fromIndex;
    std::string name;
    return LetterJump::jump(fromIndex, n, direction, [&](int i) -> const std::string & {
        const std::vector<GameInfo> one = games(system, i, 1, filter);
        name = one.empty() ? std::string() : one[0].title;
        return name;
    });
}

// "Show box art" off: no cover on tiles and the list's detail panel; "Load small boxarts": a Grid tile gets the Small image.
Art SampleLibrary::gameArt(const GameRef &game, ArtSize size, bool) {
    if (!showBoxArt_) return Art();
    if (size == ArtSize::Grid) { ++askedGrid; if (smallGrid_) size = ArtSize::Small; }
    if (size == ArtSize::Small) ++askedSmall;
    return coverArt(game, size);
}

// The cover sized to the detail panel's slot: the sample art is generated at exactly that size.
Art SampleLibrary::gameArtFit(const GameRef &game, int width, int height, bool) {
    if (!showBoxArt_ || width <= 0 || height <= 0) return Art();
    Art a;
    if (std::atoi(game.id.c_str()) % 11 == 0) return a;
    a.image = art(key(game), width, height, false);
    return a;
}

// The Game options panel ignores "Show box art" (Classic).
Art SampleLibrary::gameOptionsArt(const GameRef &game, ArtSize size, bool) { return coverArt(game, size); }

Art SampleLibrary::coverArt(const GameRef &game, ArtSize size) {
    const ArtBounds b = artBounds(size);
    Art a;
    // Every 11th game has no art, to exercise the placeholder.
    if (std::atoi(game.id.c_str()) % 11 == 0) return a;
    a.image = art(key(game), b.width, b.height, false);
    return a;
}

std::vector<GameInfo> SampleLibrary::favorites() {
    std::vector<GameInfo> out;
    for (const std::string &k : favorites_) {
        const size_t slash = k.find('/');
        const Sys *s = find(k.substr(0, slash));
        if (s) out.push_back(make(*s, std::atoi(k.c_str() + slash + 1) - 1));
    }
    std::sort(out.begin(), out.end(), [](const GameInfo &a, const GameInfo &b) { return a.title < b.title; });
    return out;
}

std::vector<GameInfo> SampleLibrary::recents() {
    std::vector<GameInfo> out;
    for (const GameRef &r : recents_)
        if (const Sys *s = find(r.system)) out.push_back(make(*s, std::atoi(r.id.c_str()) - 1));
    return out;
}

bool SampleLibrary::toggleFavorite(const GameRef &game) {
    const std::string k = key(game);
    const bool now = favorites_.erase(k) == 0;
    if (now) favorites_.insert(k);
    ++revision_;
    return now;
}

void SampleLibrary::removeRecent(const GameRef &game) {
    recents_.erase(std::remove_if(recents_.begin(), recents_.end(),
                                  [&](const GameRef &r) { return r.system == game.system && r.id == game.id; }),
                   recents_.end());
    ++revision_;
}

namespace {
// The words of a text (letters, digits and any non-ASCII byte), lower case.
void words(const std::string &text, std::vector<std::string> &out) {
    std::string w;
    for (unsigned char ch : text) {
        if (ch >= 0x80 || std::isalnum(ch)) w.push_back(char(std::tolower(ch)));
        else if (!w.empty()) { out.push_back(w); w.clear(); }
    }
    if (!w.empty()) out.push_back(w);
}
}  // namespace

// The Classic rule: every term of the query starts a word of the title or the system name.
bool SampleLibrary::matchesSearch(const std::string &title, const std::string &systemName, const std::string &query) {
    std::vector<std::string> terms, hay;
    words(query, terms);
    if (terms.empty()) return false;
    words(title, hay);
    words(systemName, hay);
    for (const std::string &term : terms) {
        bool found = false;
        for (const std::string &w : hay)
            if (w.compare(0, term.size(), term) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

void SampleLibrary::setSearch(const std::string &query, const SystemId &scope, int owner) {
    ++revision_;
    if (query.empty()) { searches_.erase(owner); return; }
    SearchState &state = searches_[owner];
    state = SearchState();
    state.query = query;
    // Synchronous, so done at once. Like the device: all systems also match the system's name.
    for (const Sys &s : systems_) {
        if (s.hidden || (!scope.empty() && scope != s.id)) continue;
        for (int i = 0; i < s.count && int(state.results.size()) <= SearchState::kMaxSearchResults; ++i) {
            GameInfo g = make(s, i);
            if (matchesSearch(g.title, scope.empty() ? g.systemName : std::string(), query)) state.results.push_back(g);
        }
    }
    if (int(state.results.size()) > SearchState::kMaxSearchResults) {   // one more than shown: "500+"
        state.results.pop_back();
        state.more = true;
    }
}

SearchState SampleLibrary::searchState(int owner) {
    const auto it = searches_.find(owner);
    return it == searches_.end() ? SearchState() : it->second;
}

ScreenshotState SampleLibrary::gameScreenshots(const GameRef &game) {
    ScreenshotState s;
    // Every 5th game has none, to exercise the empty state.
    if (std::atoi(game.id.c_str()) % 5 == 0) { s.phase = ScreenshotState::Phase::Missing; return s; }
    s.phase = ScreenshotState::Phase::Ready;
    s.count = 4;
    return s;
}

Art SampleLibrary::gameScreenshot(const GameRef &game, int index, ArtSize size, bool) {
    Art a;
    if (index < 0 || index >= gameScreenshots(game).count) return a;
    const ArtBounds b = artBounds(size);
    a.image = art(key(game) + "|shot" + std::to_string(index), b.width, b.width * 3 / 4, false);
    return a;
}

GameDetail SampleLibrary::detail(const GameRef &game) {
    GameDetail d;
    if (const Sys *s = find(game.system)) d.info = make(*s, std::atoi(game.id.c_str()) - 1);
    d.file = d.info.title + ".sample";
    if (d.info.versionCount > 1) d.versions = {{"v1", "Region X"}, {"v2", "Region Y"}};
    d.year = d.info.year;
    d.coreFile = d.info.coreFound ? "Example-core_20260101.rbf" : std::string();
    d.romFile = d.info.checked ? d.info.title + ".zip" : std::string();
    d.players = "1-2";
    d.overview = "A generic description of a sample title. It exists only to fill the layout.";
    return d;
}

LaunchResult SampleLibrary::launch(const GameRef &game, const std::string &versionId) {
    LaunchResult r;
    const GameDetail d = detail(game);
    r.status = (d.versions.size() > 1 && versionId.empty()) ? LaunchStatus::NeedsVersion : LaunchStatus::Started;
    if (r.status == LaunchStatus::Started) standDown_ = true;
    return r;
}

Art SampleLibrary::discArt(ArtSize size) {
    Art a;
    if (disc_.media != DiscStatus::Media::Readable) return a;
    const ArtBounds b = artBounds(size);
    a.image = art("disc", b.width, b.height, false);
    return a;
}

Art SampleLibrary::discScreenshot(int index, ArtSize size) {
    Art a;
    if (index < 0 || index >= disc_.screenshotCount) return a;
    const ArtBounds b = artBounds(size);
    a.image = art("shot" + std::to_string(index), b.width, b.width * 3 / 4, false);
    return a;
}

// ---- the sample preview

void SampleLibrary::focusPreview(const GameRef &game, int slotWidth, int slotHeight) {
    previewTouched_ = true;
    previewSlotW_ = slotWidth;
    previewSlotH_ = slotHeight;
    const std::string key = game.system + "|" + game.id;
    if (key != previewKey_) { previewKey_ = key; previewTime_ = 0.0f; previewImage_.reset(); ++revision_; }
}

void SampleLibrary::update(float dt) {
    const bool was = previewActive_;
    previewActive_ = previewTouched_;
    previewTouched_ = false;
    if (!previewActive_) {
        if (was) { previewTime_ = 0.0f; previewKey_.clear(); previewImage_.reset(); ++revision_; }
        return;
    }
    const float before = previewTime_;
    previewTime_ += dt;
    constexpr float kCountdown = 3.0f;
    if (previewTime_ < kCountdown) {
        if (int(before) != int(previewTime_)) ++revision_;   // the status line counts down
        return;
    }
    // Playing: a new frame about every 1/15 s, drawn into the one buffer.
    previewFrameTime_ += dt;
    if (!previewImage_ || previewFrameTime_ >= 1.0f / 15.0f) {
        previewFrameTime_ = 0.0f;
        const std::shared_ptr<ui::Image> before = previewImage_;
        paintPreviewFrame();
        if (previewImage_ != before) ++revision_;   // the first frame (or a new size) changes what is shown
        ++previewSerial_;
    }
}

PreviewState SampleLibrary::preview() {
    PreviewState p;
    if (!previewActive_ && !previewTouched_) return p;
    if (previewTime_ < 3.0f) p.status = "Video plays in: " + std::to_string(3 - int(previewTime_)) + " s";
    else p.playing = previewImage_ != nullptr;
    return p;
}

// A moving test pattern: diagonal bars over a gradient, with a bouncing block.
void SampleLibrary::paintPreviewFrame() {
    const int w = std::min(320, std::max(16, previewSlotW_)), h = std::min(240, std::max(16, previewSlotH_));
    if (!previewImage_ || previewImage_->width() != w || previewImage_->height() != h) {
        previewImage_ = std::make_shared<ui::Image>(w, h);
        previewImage_->setOpaque(true);
    }
    const int t = int((previewTime_ - 3.0f) * 60.0f);
    const int rangeX = std::max(1, w - 40), rangeY = std::max(1, h - 40);
    const int bx = std::abs((t * 3) % (2 * rangeX) - rangeX), by = std::abs((t * 2) % (2 * rangeY) - rangeY);
    for (int y = 0; y < h; ++y) {
        uint32_t *row = previewImage_->row(y);
        for (int x = 0; x < w; ++x) {
            const int bar = ((x + y + t * 2) / 24) % 2;
            const int r = 40 + 120 * x / w, g = 40 + 120 * y / h, b = bar ? 200 : 90;
            uint32_t c = 0xFF000000u | uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(b);
            if (x >= bx && x < bx + 40 && y >= by && y < by + 40) c = 0xFFFFFFFFu;
            row[x] = c;
        }
    }
}

}  // namespace patsgui
