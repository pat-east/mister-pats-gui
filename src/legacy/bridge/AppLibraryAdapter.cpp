#include "bridge/AppLibraryAdapter.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <set>
#include <strings.h>

#include "Alphabet.h"
#include "DebugLog.h"
#include "MraFile.h"
#include "GameSearch.h"
#include "GamesScreen.h"

using namespace patsgui;

namespace {

ArtworkVariant variantFor(ArtSize size) {
    switch (size) {
        case ArtSize::Small:  return ArtworkVariant::Small;
        case ArtSize::Grid:   return ArtworkVariant::Grid;
        case ArtSize::Home:   return ArtworkVariant::Home;
        case ArtSize::Detail: return ArtworkVariant::Detail;
    }
    return ArtworkVariant::Grid;
}

std::string baseName(const std::string &path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

constexpr size_t kMaxConverted = 300;

}  // namespace

AppLibraryAdapter::AppLibraryAdapter(Context &context) : context_(context) {
    libraryRevision_ = context.libraryRevision;
    imageGeneration_ = context.images.generation();
    discTabAvailable_ = computeDiscTabAvailable();
}

// Classic's rule for the Disc tab (App::visibleTabs(), App::render()): a drive is present and the Disc package is
// installed, set up and has a compatible MGL.
bool AppLibraryAdapter::computeDiscTabAvailable() const {
    const auto drive = context_.discDrive.snapshot();
    const auto cores = context_.discCores.snapshot();
    return drive.state != DiscDriveService::MediaState::NoDrive && cores.alternateMainInstalled &&
           cores.scopedMainConfigured && cores.hasAnyCompatibleMgl();
}

bool AppLibraryAdapter::ready(const Context &context) { return !context.library.systems().empty(); }

bool AppLibraryAdapter::organize() const { return context_.preferences.organizeGameVersions(); }

const GameSystem *AppLibraryAdapter::system(const SystemId &id) const { return context_.library.findSystem(id); }

const Game *AppLibraryAdapter::findGame(const GameSystem &sys, const std::string &path) const {
    for (const Game &game : context_.library.gamesOf(sys, organize())) {
        if (game.path == path) return &game;
        for (const Game::Variant &version : game.versions)
            if (version.path == path) return &game;
    }
    return nullptr;
}

GameInfo AppLibraryAdapter::info(const GameSystem &sys, const Game &game) const {
    GameInfo g;
    g.ref = {sys.name, game.path};
    g.title = game.name;
    g.systemName = sys.name;
    g.favorite = isFavoriteGame(context_.favorites, game);
    g.versionCount = std::max(1, int(game.versions.size()));
    if (const ArcadeEntry *entry = arcadeFor(game)) fillArcade(g, entry->mraPath);
    return g;
}

const ArcadeEntry *AppLibraryAdapter::arcadeFor(const std::string &path) const {
    if (arcadeIndex_.empty()) return nullptr;
    const auto found = arcadeIndex_.find(path);
    return found == arcadeIndex_.end() ? nullptr : &arcadeEntries_[found->second];
}

const ArcadeEntry *AppLibraryAdapter::arcadeFor(const Game &game) const {
    if (arcadeIndex_.empty()) return nullptr;
    if (const ArcadeEntry *entry = arcadeFor(game.path)) return entry;
    for (const Game::Variant &version : game.versions)
        if (const ArcadeEntry *entry = arcadeFor(version.path)) return entry;
    return nullptr;
}

void AppLibraryAdapter::fillArcade(GameInfo &info, const std::string &path) const {
    const ArcadeEntry *entry = arcadeFor(path);
    if (!entry) return;
    info.year = entry->year;
    info.manufacturer = entry->manufacturer;
    info.category = entry->category;
    info.checked = true;
    info.works = entry->mightWork();
    info.coreFound = entry->coreFound;
    info.romFound = entry->romFound;
    info.romCrcOk = entry->romCrcOk;
}

const Game &AppLibraryAdapter::gameAt(const Item &item) const {
    return context_.library.gamesOf(context_.library.systems()[item.system], organize())[item.index];
}

const std::vector<AppLibraryAdapter::Item> *AppLibraryAdapter::itemsFor(const SystemId &scope, Works filter) {
    if (!scope.empty() && filter == Works::All) return nullptr;
    const std::string key = scope + '\x1f' + std::to_string(int(filter));
    const auto have = lists_.find(key);
    if (have != lists_.end()) return &have->second;
    // Built in one go: like the Classic "all games" list, the first call takes a moment on the device.
    std::vector<Item> items;
    const std::vector<GameSystem> &systems = context_.library.systems();
    for (size_t si = 0; si < systems.size(); ++si) {
        if (!scope.empty() && systems[si].name != scope) continue;
        const std::vector<Game> &games = context_.library.gamesOf(systems[si], organize());
        for (size_t gi = 0; gi < games.size(); ++gi) {
            if (filter != Works::All) {
                const ArcadeEntry *entry = arcadeFor(games[gi]);
                if (!entry || (filter == Works::Working) != entry->mightWork()) continue;
            }
            items.push_back({si, gi});
        }
    }
    if (scope.empty())
        std::stable_sort(items.begin(), items.end(), [this](const Item &a, const Item &b) {
            return strcasecmp(gameAt(a).name.c_str(), gameAt(b).name.c_str()) < 0;
        });
    return &lists_.emplace(key, std::move(items)).first->second;
}

void AppLibraryAdapter::dropCaches() {
    counts_.clear();
    lists_.clear();
    groupIndex_.clear();
    converted_.clear();
    searches_.clear();
    previewKey_.clear();
}

void AppLibraryAdapter::refresh() {
    // Favorites, history and hidden flags are read from memory; they change only through the adapter
    // or Classic screens, so one revision bump makes the tabs ask again.
    ++revision_;
}

// ---- per frame

void AppLibraryAdapter::update(float deltaSeconds) {
    if (libraryRevision_ != context_.libraryRevision) {
        libraryRevision_ = context_.libraryRevision;
        dropCaches();
        ++revision_;
    }
    if (context_.images.generation() != imageGeneration_) {
        imageGeneration_ = context_.images.generation();
        ++revision_;
    }
    discTabTimer_ -= deltaSeconds;
    if (discTabTimer_ <= 0.0f) {   // once a second: two small mutex-guarded copies, no file access
        discTabTimer_ = 1.0f;
        discTabAvailable_ = computeDiscTabAvailable();
    }
    iconCheckTimer_ -= deltaSeconds;
    if (iconCheckTimer_ <= 0.0f) {   // once a second, from memory: Icons::missingBitmaps() reads no file
        iconCheckTimer_ = 1.0f;
        const size_t missing = context_.icons.missingBitmaps().size();
        if (missing != iconsMissing_) {
            if (iconsMissing_ != size_t(-1)) {
                for (auto it = converted_.begin(); it != converted_.end();)
                    it = it->first.compare(0, 4, "sys|") == 0 ? converted_.erase(it) : std::next(it);
                ++revision_;
            }
            iconsMissing_ = missing;
        }
    }
    stepCounts();
    stepArcade();
    stepSearch();

    // The preview: a new status line or video frame is something the Games tab has to redraw.
    if (context_.preview.revision() != previewRevision_) {
        previewRevision_ = context_.preview.revision();
        ++revision_;
    }
    if (context_.preview.videoPlaying()) {
        if (const uint32_t *frame = context_.preview.acquireFrame()) {
            const int w = context_.preview.videoWidth(), h = context_.preview.videoHeight();
            if (w > 0 && h > 0) {
                // The one reused image: allocated only when the video size changes (then the picture a view
                // drew is stale, so revision() changes); a new frame of the same size only bumps the serial.
                if (!previewImage_ || previewImage_->width() != w || previewImage_->height() != h) {
                    previewImage_ = std::make_shared<ui::Image>(w, h);
                    ++revision_;
                }
                // Decoded frames have no padding between rows and the image is contiguous: one memcpy.
                std::memcpy(previewImage_->row(0), frame, size_t(w) * size_t(h) * sizeof(uint32_t));
                previewImage_->setOpaque(true);
                ++previewSerial_;
            }
            context_.preview.releaseFrame();
        }
    }
    if (screenshots_) {
        // A lookup that ends with nothing (Missing) or with pictures is something the panel must redraw.
        const GameScreenshotService::Snapshot snap = screenshots_->snapshot();
        const bool timedOut = snap.state == GameScreenshotService::State::Searching && screenshotTimedOut();
        if (int(snap.state) != screenshotPhase_ || snap.screenshots.size() != screenshotCount_ ||
            (timedOut && !screenshotTimeoutSeen_)) {
            static const char *const names[] = {"idle", "searching", "ready", "missing"};
            DebugLog::info(std::string("game screenshots: ") + screenshotKey_ + " -> " +
                           (timedOut ? "timed out (shown as missing)" : names[int(snap.state)]) + "; pictures=" +
                           std::to_string(snap.screenshots.size()));
            screenshotPhase_ = int(snap.state);
            screenshotCount_ = snap.screenshots.size();
            screenshotTimeoutSeen_ = timedOut;
            ++revision_;   // every transition is drawn: Searching -> Ready, -> Missing, -> timed out
        }
    }

    // The disc is only watched (and its artwork fetched) while a tab asked about it last frame. The Disc
    // tab asks every frame while it is the active tab (PatsGuiScreen::update), as the Classic screen
    // polls in its own update(); without that a state change would never be seen without a redraw.
    // A lookup or a re-check that is under way keeps being watched even when no tab asked this frame, so its
    // end is seen and drawn.
    const bool inFlight = discPolled_ && (artSnap_.state == DiscArtworkService::State::Searching ||
                                          discSnap_.state == DiscDriveService::MediaState::Checking);
    if (discWanted_ || inFlight) {
        pollDisc();
        discWanted_ = false;
    }
    stepDiscLaunch();
}

void AppLibraryAdapter::pollDisc() {
    const DiscDriveService::Snapshot drive = context_.discDrive.snapshot();
    context_.discArtwork.request(drive);
    const DiscCoreManager::Status cores = context_.discCores.snapshot();
    const DiscArtworkService::Snapshot art = context_.discArtwork.snapshot();
    if (!discPolled_ || drive.generation != discSnap_.generation || drive.state != discSnap_.state ||
        drive.system != discSnap_.system || drive.title != discSnap_.title ||
        drive.identifier != discSnap_.identifier || drive.region != discSnap_.region ||
        drive.error != discSnap_.error || drive.model != discSnap_.model ||
        cores.inspectionRevision != coreSnap_.inspectionRevision ||
        cores.readyFor(drive.system) != coreSnap_.readyFor(discSnap_.system) ||
        art.state != artSnap_.state || art.boxart != artSnap_.boxart ||
        art.screenshots.size() != artSnap_.screenshots.size() || art.overview != artSnap_.overview ||
        art.releaseYear != artSnap_.releaseYear || art.players != artSnap_.players ||
        art.rating != artSnap_.rating || art.coop != artSnap_.coop ||
        context_.discInstaller.busy() != installerBusy_) {
        ++revision_;
        installerBusy_ = context_.discInstaller.busy();
    }
    if (art.state == DiscArtworkService::State::Searching && artSnap_.state != DiscArtworkService::State::Searching) {
        artSearchStarted_ = std::chrono::steady_clock::now();
        artTimeoutSeen_ = false;
    }
    if (art.state != artSnap_.state || art.screenshots.size() != artSnap_.screenshots.size()) {
        static const char *const names[] = {"idle", "searching", "ready", "missing"};
        DebugLog::info(std::string("disc artwork (tab): -> ") + names[int(art.state)] + "; screenshots=" +
                       std::to_string(art.screenshots.size()) + "; boxart=" + (art.boxart.empty() ? "none" : "yes"));
    }
    if (art.state == DiscArtworkService::State::Searching && !artTimeoutSeen_ &&
        std::chrono::steady_clock::now() - artSearchStarted_ > std::chrono::seconds(120)) {
        artTimeoutSeen_ = true;
        DebugLog::warn("disc artwork (tab): lookup ran 120 s without an answer; shown as none found");
        ++revision_;
    }
    discSnap_ = drive;
    coreSnap_ = cores;
    artSnap_ = art;
    discPolled_ = true;
}

void AppLibraryAdapter::stepArcade() {
    if (arcadeScan_.running() && !arcadeCancelled_) {
        arcadeScan_.step();
        // Classic (ArcadeGamesScreen::render) counts the entries that "might work" over scan_.entries(); the scan's own
        // workingFound() only counts when a limit is given. Counted incrementally: each entry once.
        const std::vector<ArcadeEntry> &found = arcadeScan_.entries();
        for (; arcadeCounted_ < found.size(); ++arcadeCounted_)
            if (found[arcadeCounted_].mightWork()) ++arcadeWorking_;
        ++revision_;   // progress
    }
    if (!arcadeCollected_ && arcadeScan_.finished()) {
        arcadeEntries_ = arcadeScan_.entries();
        arcadeIndex_.clear();
        for (size_t i = 0; i < arcadeEntries_.size(); ++i) arcadeIndex_[arcadeEntries_[i].mraPath] = i;
        arcadeCollected_ = true;
        lists_.clear();
        ++revision_;
    }
}

ArcadeScanState AppLibraryAdapter::arcadeScan() {
    ArcadeScanState s;
    switch (arcadeScan_.state()) {
        case ArcadeScan::State::Idle:  s.phase = ArcadeScanState::Phase::Idle; break;
        case ArcadeScan::State::Done:  s.phase = ArcadeScanState::Phase::Done; break;
        default:
            s.phase = arcadeCancelled_ ? ArcadeScanState::Phase::Idle : ArcadeScanState::Phase::Running;
            break;
    }
    s.progress = arcadeScan_.progress();
    s.total = int(arcadeScan_.total());
    s.checked = int(arcadeScan_.done());
    s.entries = int(arcadeCounted_);
    s.working = int(arcadeWorking_);
    return s;
}

void AppLibraryAdapter::startArcadeScan() {
    if (arcadeScan_.running() && !arcadeCancelled_) return;
    arcadeCancelled_ = false;
    arcadeCounted_ = arcadeWorking_ = 0;
    arcadeScan_.start();   // walks the drives: only ever on the user's request
    arcadeCollected_ = false;
    ++revision_;
}

void AppLibraryAdapter::cancelArcadeScan() {
    if (!arcadeScan_.running()) return;
    arcadeCancelled_ = true;
    ++revision_;
}

void AppLibraryAdapter::stepCounts() {
    // One system per frame: the first call for a big system can take a moment on the device.
    for (const GameSystem &sys : context_.library.systems()) {
        if (counts_.count(sys.name)) continue;
        counts_[sys.name] = int(context_.library.gamesOf(sys, organize()).size());
        ++revision_;
        return;
    }
}

// ---- systems

std::vector<SystemInfo> AppLibraryAdapter::systems() {
    std::vector<SystemInfo> out;
    for (const GameSystem &sys : context_.library.systems()) {
        if (!context_.library.hasGames(sys)) continue;
        SystemInfo s;
        s.id = sys.name;
        s.name = sys.name;
        s.group = sys.group;
        const auto count = counts_.find(sys.name);
        s.gameCount = count == counts_.end() ? -1 : count->second;
        s.hidden = context_.hiddenSystems.contains(sys.name);
        s.isArcade = sys.isArcade;
        s.order = int(&sys - context_.library.systems().data());   // Systems tab: catalog order
        s.noCore = !sys.launchable();
        out.push_back(std::move(s));
    }
    std::sort(out.begin(), out.end(), [](const SystemInfo &a, const SystemInfo &b) {
        return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    return out;
}

bool AppLibraryAdapter::showArcadeInSystems() {
    return context_.preferences.showArcadeInConsoles();
}

void AppLibraryAdapter::setSystemHidden(const SystemId &id, bool hidden) {
    if (context_.hiddenSystems.contains(id) != hidden) {
        context_.hiddenSystems.toggle(id);   // saves itself
        ++revision_;
    }
}

Art AppLibraryAdapter::systemArt(const SystemId &id, ArtSize size) {
    const ArtBounds b = artBounds(size);
    const std::string path = context_.icons.pathFor(id);
    if (path.empty()) return Art();
    return artFor("sys|" + id + "|" + std::to_string(b.width), {path}, b.width, b.height, false);
}

// ---- games

int AppLibraryAdapter::gameCount(const SystemId &id, Works filter) {
    if (const std::vector<Item> *items = itemsFor(id, filter)) return int(items->size());
    const GameSystem *sys = system(id);
    if (!sys) return 0;
    const auto count = counts_.find(sys->name);
    if (count != counts_.end()) return count->second;
    return int(context_.library.gamesOf(*sys, organize()).size());
}

std::vector<GameInfo> AppLibraryAdapter::games(const SystemId &id, int offset, int count, Works filter) {
    std::vector<GameInfo> out;
    if (const std::vector<Item> *items = itemsFor(id, filter)) {
        const int end = std::min(int(items->size()), offset + count);
        for (int i = std::max(0, offset); i < end; ++i) {
            const Item &item = (*items)[size_t(i)];
            out.push_back(info(context_.library.systems()[item.system], gameAt(item)));
        }
        return out;
    }
    const GameSystem *sys = system(id);
    if (!sys) return out;
    const std::vector<Game> &all = context_.library.gamesOf(*sys, organize());
    const int end = std::min(int(all.size()), offset + count);
    for (int i = std::max(0, offset); i < end; ++i) out.push_back(info(*sys, all[size_t(i)]));
    return out;
}

int AppLibraryAdapter::jumpLetter(const SystemId &id, int fromIndex, int direction, Works filter) {
    if (const std::vector<Item> *items = itemsFor(id, filter))
        return Alphabet::jump(fromIndex, int(items->size()), direction, [&](int i) -> const std::string & {
            return gameAt((*items)[size_t(i)]).name;
        });
    const GameSystem *sys = system(id);
    if (!sys) return fromIndex;
    const std::vector<Game> &all = context_.library.gamesOf(*sys, organize());
    return Alphabet::jump(fromIndex, int(all.size()), direction,
                          [&all](int i) -> const std::string & { return all[size_t(i)].name; });
}

// Tiles and the list's detail panel: Settings > Interface > "Show box art" off shows no cover (Classic: GamesScreen,
// HomeScreen). "Load small boxarts in grid-view" on loads the prepared small image for a Grid tile (Classic:
// artworkVariantFor: Grid -> Small); the Home row (ArtSize::Home) and the detail size are not affected.
Art AppLibraryAdapter::gameArt(const GameRef &ref, ArtSize size, bool priority) {
    if (!context_.preferences.showBoxArt()) return Art();
    if (size == ArtSize::Grid && context_.preferences.loadSmallBoxartsInGridView()) size = ArtSize::Small;
    return coverArt(ref, size, priority);
}

// The cover at the size of the detail panel's picture slot (Classic: GamesScreen::renderDetail asks the image cache
// for artArea.w x artArea.h), gated by "Show box art".
Art AppLibraryAdapter::gameArtFit(const GameRef &ref, int width, int height, bool priority) {
    if (!context_.preferences.showBoxArt() || width <= 0 || height <= 0) return Art();
    const GameSystem *sys = system(ref.system);
    if (!sys) return Art();
    const std::string key = "gamefit|" + ref.system + "|" + ref.id + "|" + std::to_string(width) + "x" + std::to_string(height);
    const Game game = ::Library::makeGame(*sys, ref.id, variantFor(ArtSize::Detail));
    return artFor(key, game.boxartCandidates, width, height, priority);
}

// The Game options panel ignores "Show box art" (Classic: App::openGameActions requests the cover directly).
Art AppLibraryAdapter::gameOptionsArt(const GameRef &ref, ArtSize size, bool priority) { return coverArt(ref, size, priority); }

Art AppLibraryAdapter::coverArt(const GameRef &ref, ArtSize size, bool priority) {
    const GameSystem *sys = system(ref.system);
    if (!sys) return Art();
    const ArtBounds b = artBounds(size);
    const std::string key = "game|" + ref.system + "|" + ref.id + "|" + std::to_string(b.width);
    const auto have = converted_.find(key);
    if (have != converted_.end()) {
        Art art;
        art.image = have->second;
        return art;
    }
    const Game game = ::Library::makeGame(*sys, ref.id, variantFor(size));
    return artFor(key, game.boxartCandidates, b.width, b.height, priority);
}

ui::ImagePtr AppLibraryAdapter::convert(const Image &image) {
    ui::ImagePtr out = std::make_shared<ui::Image>(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) std::copy(image.row(y), image.row(y) + image.width(), out->row(y));
    out->setOpaque(image.opaque());
    return out;
}

Art AppLibraryAdapter::artFor(const std::string &key, const std::vector<std::string> &candidates, int w, int h,
                              bool priority) {
    Art art;
    const auto have = converted_.find(key);
    if (have != converted_.end()) {
        art.image = have->second;
        return art;
    }
    if (candidates.empty()) return art;
    const ImagePtr image = context_.images.requestAsyncCandidates(candidates, w, h, priority);
    if (image && image->valid()) {
        if (converted_.size() >= kMaxConverted) converted_.clear();   // cheap and rare; decoded images stay in the cache
        art.image = convert(*image);
        converted_[key] = art.image;
        return art;
    }
    art.loading = context_.images.isPendingCandidates(candidates, w, h);
    return art;
}

// ---- favorites and recents
//
// The three lists are ports of the Classic code, each with its own rules (they differ, and the differences are kept):
//   Home's "Recently played"  HomeScreen::refresh(): newest first, a game with versions once (the first, newest entry
//                             wins), titled and launched by its group when "Organize game versions" is on, by the file
//                             when it is off; no check that the file exists.
//   Home's "Favorites"        HomeScreen::refresh(): the same grouping; an entry with an empty or unknown system is not
//                             shown; no check that the file exists; sorted by title.
//   The Favorites tab         GamesScreen::reload() in favorites mode: with organizing on, the groups of the library that
//                             hold a favorite path (a deleted file is not in the library, so it is not listed); off, every
//                             stored path once, by its file name; sorted by title.

// The group (Game with versions) of a path, as Classic's Home looks it up: the path is the group's own or one of its
// versions. Only while organizing is on; the index is built once per system and dropped with the other caches.
const Game *AppLibraryAdapter::groupFor(const GameSystem &sys, const std::string &path) {
    if (!organize()) return nullptr;
    const std::vector<Game> &games = context_.library.gamesOf(sys, true);
    auto have = groupIndex_.find(sys.name);
    if (have == groupIndex_.end()) {
        std::unordered_map<std::string, size_t> index;
        index.reserve(games.size());
        for (size_t i = 0; i < games.size(); ++i) {
            index.emplace(games[i].path, i);   // emplace: the first group that owns a path keeps it, like Classic's scan
            for (const Game::Variant &version : games[i].versions) index.emplace(version.path, i);
        }
        have = groupIndex_.emplace(sys.name, std::move(index)).first;
    }
    const auto found = have->second.find(path);
    return found != have->second.end() && found->second < games.size() ? &games[found->second] : nullptr;
}

// One tile of Home's lists (Classic: homeGame() + the group's path): the group's name and path when there is a group, else
// the file's own name and path.
GameInfo AppLibraryAdapter::homeInfo(const GameSystem &sys, const std::string &path, const Game *group) {
    if (group) return info(sys, *group);   // title, favorite (own path or any version) and versionCount of the group
    GameInfo g;
    g.ref = {sys.name, path};
    g.title = ::Library::nameFor(sys, path);
    g.systemName = sys.name;
    g.favorite = context_.favorites.contains(path);
    fillArcade(g, path);
    return g;
}

std::vector<GameInfo> AppLibraryAdapter::favorites() {
    std::vector<GameInfo> out;
    // Classic: the stored paths by their system field (an empty or unknown one is skipped), then per system.
    std::map<std::string, std::set<std::string>> byProfile;
    for (const FavoriteEntry &entry : context_.favorites.entries()) byProfile[entry.system].insert(entry.path);
    for (const auto &bucket : byProfile) {
        const GameSystem *sys = context_.library.findSystem(bucket.first);
        if (!sys) continue;
        if (organize()) {
            std::set<const Game *> groups;
            std::vector<const Game *> found;
            for (const std::string &path : bucket.second)
                if (const Game *group = groupFor(*sys, path))
                    if (groups.insert(group).second) found.push_back(group);
            for (const Game *group : found) out.push_back(info(*sys, *group));
        } else {
            for (const std::string &path : bucket.second)
                if (!::Library::isSystemFile(*sys, path)) out.push_back(homeInfo(*sys, path, nullptr));
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const GameInfo &a, const GameInfo &b) {
        return strcasecmp(a.title.c_str(), b.title.c_str()) < 0;
    });
    return out;
}

std::vector<GameInfo> AppLibraryAdapter::homeFavorites() {
    std::vector<GameInfo> out;
    std::set<std::string> seen;
    for (const FavoriteEntry &entry : context_.favorites.entries()) {
        const GameSystem *sys = context_.library.findSystem(entry.system);
        if (!sys || ::Library::isSystemFile(*sys, entry.path)) continue;
        const Game *group = groupFor(*sys, entry.path);
        if (group && !seen.insert(sys->name + "\t" + group->name).second) continue;
        out.push_back(homeInfo(*sys, entry.path, group));
    }
    std::stable_sort(out.begin(), out.end(), [](const GameInfo &a, const GameInfo &b) {
        return strcasecmp(a.title.c_str(), b.title.c_str()) < 0;
    });
    return out;
}

std::vector<GameInfo> AppLibraryAdapter::recents() {
    std::vector<GameInfo> out;
    std::set<std::string> seen;
    for (const HistoryEntry &entry : context_.history.entries()) {
        const GameSystem *sys = entry.system.empty() ? context_.library.systemForPath(entry.path)
                                                      : context_.library.findSystem(entry.system);
        if (!sys || ::Library::isSystemFile(*sys, entry.path)) continue;
        const Game *group = groupFor(*sys, entry.path);
        if (group && !seen.insert(sys->name + "\t" + group->name).second) continue;   // one tile per game, the newest entry
        out.push_back(homeInfo(*sys, entry.path, group));
    }
    return out;
}

bool AppLibraryAdapter::toggleFavorite(const GameRef &ref) {
    const GameSystem *sys = system(ref.system);
    if (!sys) return false;
    const Game *game = findGame(*sys, ref.id);
    const std::string name = game ? game->name : ::Library::nameFor(*sys, ref.id);
    bool was = context_.favorites.contains(ref.id);
    if (game) was = isFavoriteGame(context_.favorites, *game);
    // Like the Classic lists: removing clears the title's path and every version; adding marks the path.
    if (was && game && !game->versions.empty()) {
        if (context_.favorites.contains(game->path)) context_.favorites.toggle(sys->name, game->path, name);
        for (const Game::Variant &version : game->versions)
            if (context_.favorites.contains(version.path)) context_.favorites.toggle(sys->name, version.path, name);
    } else {
        context_.favorites.toggle(sys->name, ref.id, name);
    }
    ++revision_;
    return !was;
}

// ---- Favorites tab: the view, as the Classic Favorites tab remembers it.
int AppLibraryAdapter::favoritesView() {
    const Preferences &prefs = context_.preferences;
    const GameView view = gameViewFromName(prefs.defaultView() == "individual" ? prefs.favoritesView()
                                                                             : prefs.defaultView());
    return view == GameView::Grid ? 0 : view == GameView::BoxartSmall ? 1 : 2;
}

void AppLibraryAdapter::setFavoritesView(int view) {
    if (context_.preferences.defaultView() != "individual") return;   // a global default is not overridden
    const GameView v = view == 0 ? GameView::Grid : view == 1 ? GameView::BoxartSmall : GameView::List;
    context_.preferences.setFavoritesView(nameForGameView(v));
}

int AppLibraryAdapter::homeView() {
    const Preferences &prefs = context_.preferences;
    const GameView view = gameViewFromName(prefs.defaultView() == "individual" ? prefs.homeView() : prefs.defaultView());
    return view == GameView::Grid ? 0 : view == GameView::BoxartSmall ? 1 : 2;
}

void AppLibraryAdapter::setHomeView(int view) {
    if (context_.preferences.defaultView() != "individual") return;   // a global default is not overridden
    const GameView v = view == 0 ? GameView::Grid : view == 1 ? GameView::BoxartSmall : GameView::List;
    context_.preferences.setHomeView(nameForGameView(v));
}

bool AppLibraryAdapter::viewsIndividual() { return context_.preferences.defaultView() == "individual"; }

int AppLibraryAdapter::gamesView(bool oneSystem) {
    const Preferences &prefs = context_.preferences;
    const GameView view = gameViewFromName(prefs.defaultView() == "individual"
                                               ? (oneSystem ? prefs.consolesView() : prefs.gamesView())
                                               : prefs.defaultView());
    return view == GameView::Grid ? 0 : view == GameView::BoxartSmall ? 1 : 2;
}

// The Arcade list (Classic: App::init / cycleDefaultView): the global view; Grid when the setting is Individual.
int AppLibraryAdapter::arcadeView() {
    const GameView view = gameViewFromName(context_.preferences.defaultView());   // "individual" is not a view: Grid
    return view == GameView::Grid ? 0 : view == GameView::BoxartSmall ? 1 : 2;
}

void AppLibraryAdapter::setGamesView(bool oneSystem, int view) {
    if (context_.preferences.defaultView() != "individual") return;   // a global default is not overridden
    const GameView v = view == 0 ? GameView::Grid : view == 1 ? GameView::BoxartSmall : GameView::List;
    if (oneSystem) context_.preferences.setConsolesView(nameForGameView(v));
    else context_.preferences.setGamesView(nameForGameView(v));
}

// A lookup that has run this long ends as "none found" in the panel (the worker may still finish; if it
// does, the pictures appear then). The service's own steps are bounded, this is the backstop.
bool AppLibraryAdapter::screenshotTimedOut() const {
    return std::chrono::steady_clock::now() - screenshotStarted_ > std::chrono::seconds(120);
}

// The game's own background picture stands in as the one thumbnail, but only when its file can actually be
// shown (on screen or being loaded): the candidate paths are composed, not checked.
bool AppLibraryAdapter::backgroundShot(const Game &game, const GameRef &ref) {
    if (game.backgroundCandidates.empty()) return false;
    const ArtBounds b = artBounds(ArtSize::Grid);
    const Art a = artFor("shot|" + ref.system + "|" + ref.id + "|bg|" + std::to_string(b.width),
                         game.backgroundCandidates, b.width, b.height, false);
    return a.image || a.loading;
}

ScreenshotState AppLibraryAdapter::gameScreenshots(const GameRef &ref) {
    ScreenshotState state;
    const GameSystem *sys = system(ref.system);
    if (!sys) return state;
    const std::string key = ref.system + "|" + ref.id;
    if (!screenshots_) {
        if (context_.gameScreenshots) screenshots_ = context_.gameScreenshots;
        else { ownScreenshots_.reset(new GameScreenshotService(context_.theGamesDB)); screenshots_ = ownScreenshots_.get(); }
    }
    const Game game = ::Library::makeGame(*sys, ref.id, ArtworkVariant::Detail);
    if (key != screenshotKey_) {
        screenshotKey_ = key;
        screenshotStarted_ = std::chrono::steady_clock::now();
        screenshotTimeoutSeen_ = false;
        const Game *named = findGame(*sys, ref.id);
        screenshots_->request(sys->dbKey.empty() ? sys->name : sys->dbKey, named ? named->name : game.name,
                              game.backgroundCandidates);
    }
    const GameScreenshotService::Snapshot snap = screenshots_->snapshot();
    // Like the Classic panel: the looked-up pictures when there are some; until then (and when the lookup
    // finds none) the game's own background picture, if it has one, as the only thumbnail.
    if (snap.state == GameScreenshotService::State::Ready && !snap.screenshots.empty()) {
        state.phase = ScreenshotState::Phase::Ready;
        state.count = int(snap.screenshots.size());
    } else if (backgroundShot(game, ref)) {
        state.phase = ScreenshotState::Phase::Ready;
        state.count = 1;
    } else if (snap.state == GameScreenshotService::State::Searching && !screenshotTimedOut()) {
        state.phase = ScreenshotState::Phase::Searching;
    } else {
        // Missing, Ready with nothing, a lookup that ran too long, or no lookup at all: "none found", never "loading".
        state.phase = ScreenshotState::Phase::Missing;
    }
    return state;
}

Art AppLibraryAdapter::gameScreenshot(const GameRef &ref, int index, ArtSize size, bool priority) {
    if (!screenshots_ || ref.system + "|" + ref.id != screenshotKey_) return Art();
    const GameScreenshotService::Snapshot snap = screenshots_->snapshot();
    const ArtBounds b = artBounds(size);
    if (snap.state != GameScreenshotService::State::Ready || snap.screenshots.empty()) {
        const GameSystem *sys = system(ref.system);
        if (!sys || index != 0) return Art();
        const Game game = ::Library::makeGame(*sys, ref.id, ArtworkVariant::Detail);
        return artFor("shot|" + screenshotKey_ + "|bg|" + std::to_string(b.width), game.backgroundCandidates,
                      b.width, b.height, priority);
    }
    if (index < 0 || index >= int(snap.screenshots.size())) return Art();
    const std::string &path = snap.screenshots[size_t(index)];
    return artFor("shot|" + path + "|" + std::to_string(b.width), {path}, b.width, b.height, priority);
}

void AppLibraryAdapter::focusPreview(const GameRef &ref, int slotWidth, int slotHeight) {
    const GameSystem *sys = system(ref.system);
    if (!sys) return;
    // findGame walks the system's list: do it when the game changes, not every frame.
    const std::string key = ref.system + "|" + ref.id;
    if (key != previewKey_) {
        const Game *game = findGame(*sys, ref.id);
        previewKey_ = key;
        previewGame_ = game ? *game : ::Library::makeGame(*sys, ref.id, ArtworkVariant::Detail);
    }
    context_.preview.setSlotSize(slotWidth, slotHeight);
    context_.preview.focus(sys, &previewGame_);
}

PreviewState AppLibraryAdapter::preview() {
    PreviewState state;
    state.status = context_.preview.statusText();
    state.playing = context_.preview.videoPlaying() && previewImage_;
    return state;
}

// Classic (HomeScreen::removeRecent): the tile's path and every version of its game.
void AppLibraryAdapter::removeRecent(const GameRef &ref) {
    std::vector<std::string> paths{ref.id};
    if (const GameSystem *sys = system(ref.system))
        if (const Game *group = groupFor(*sys, ref.id))
            for (const Game::Variant &version : group->versions) paths.push_back(version.path);
    if (context_.history.forget(paths)) ++revision_;
}

// ---- search

void AppLibraryAdapter::setSearch(const std::string &query, const SystemId &scope, int owner) {
    ++revision_;
    if (query.empty()) { searches_.erase(owner); return; }
    SearchSlot &slot = searches_[owner];
    slot = SearchSlot();
    slot.state.query = query;
    for (const GameSystem &sys : context_.library.systems()) {
        // Classic's search does not look at the hidden list (Manage Consoles only hides consoles in the Consoles tab).
        if (!context_.library.hasGames(sys)) continue;
        if (!scope.empty() && scope != sys.name) continue;
        slot.systems.push_back(sys.name);
    }
    slot.includeSystem = scope.empty();
    slot.state.done = slot.systems.empty();
}

SearchState AppLibraryAdapter::searchState(int owner) {
    const auto it = searches_.find(owner);
    return it == searches_.end() ? SearchState() : it->second.state;
}

// The Classic rule (GameSearch::matches), for a list the tab holds itself.
bool AppLibraryAdapter::matchesSearch(const std::string &title, const std::string &systemName, const std::string &query) {
    return GameSearch::matches(title, systemName, query, true);
}

// One caller's search, a few milliseconds per frame at most (the clock is read every 64 games and after each system), so
// the device stays responsive however many callers search or however large the library is.
//
// Classic has no cap: it shows every match, sorted by title. The tabs show at most kMaxSearchResults, so the slot walks
// ALL games (a cheap word match each) and keeps only the first 500 in title order (a sorted list that a better match
// pushes the last one out of), plus the number of matches. The pictures and flags (info()) are made only for the 500 kept,
// and only when the list changed. `more` says that matches beyond the 500 exist ("500+"); the result is final when done.
void AppLibraryAdapter::stepSearch() {
    for (auto &entry : searches_) {
        if (entry.second.state.done) continue;
        stepSearchSlot(entry.second);
        return;   // one slot per frame
    }
}

void AppLibraryAdapter::stepSearchSlot(SearchSlot &slot) {
    SearchState &search = slot.state;
    constexpr size_t kMax = size_t(SearchState::kMaxSearchResults);
    const auto start = std::chrono::steady_clock::now();
    const auto overBudget = [&start] { return std::chrono::steady_clock::now() - start > std::chrono::milliseconds(4); };
    const auto before = [](const SearchSlot::Kept &a, const SearchSlot::Kept &b) {
        return strcasecmp(a.title.c_str(), b.title.c_str()) < 0;
    };
    bool changed = false, yielded = false;
    unsigned visited = 0;
    while (!yielded && slot.system < slot.systems.size()) {
        const GameSystem *sys = system(slot.systems[slot.system]);
        if (!sys) { ++slot.system; slot.index = 0; continue; }
        const std::vector<Game> &all = context_.library.gamesOf(*sys, organize());
        for (; slot.index < all.size(); ++slot.index) {
            const Game &game = all[slot.index];
            if (GameSearch::matches(game.name, sys->name, search.query, slot.includeSystem)) {
                ++slot.matched;
                // Equal titles keep the order the games were met in (stable), so a tie never pushes an earlier game out.
                if (slot.kept.size() < kMax || strcasecmp(game.name.c_str(), slot.kept.back().title.c_str()) < 0) {
                    SearchSlot::Kept kept{game.name, slot.system, slot.index};
                    slot.kept.insert(std::upper_bound(slot.kept.begin(), slot.kept.end(), kept, before), std::move(kept));
                    if (slot.kept.size() > kMax) slot.kept.pop_back();
                    changed = true;
                }
            }
            if ((++visited & 63) == 0 && overBudget()) { ++slot.index; yielded = true; break; }
        }
        if (yielded) break;
        ++slot.system;
        slot.index = 0;
        if (overBudget()) yielded = true;
    }
    if (slot.matched > kMax && !search.more) { search.more = true; changed = true; }
    if (!yielded) { search.done = true; changed = true; }
    if (!changed) return;
    search.results.clear();
    search.results.reserve(slot.kept.size());
    for (const SearchSlot::Kept &kept : slot.kept) {
        const GameSystem *sys = system(slot.systems[kept.system]);
        const std::vector<Game> &all = context_.library.gamesOf(*sys, organize());
        if (kept.index < all.size()) search.results.push_back(info(*sys, all[kept.index]));
    }
    ++revision_;
}

// ---- one game

GameDetail AppLibraryAdapter::detail(const GameRef &ref) {
    GameDetail d;
    const GameSystem *sys = system(ref.system);
    if (!sys) return d;
    const Game *game = findGame(*sys, ref.id);
    if (!game) {
        d.info.ref = ref;
        d.info.systemName = sys->name;
        d.info.title = ::Library::nameFor(*sys, ref.id);
        d.info.favorite = context_.favorites.contains(ref.id);
        d.file = baseName(ref.id);
        return d;
    }
    d.info = info(*sys, *game);
    d.file = baseName(game->path);
    d.year = d.info.year;
    if (const ArcadeEntry *entry = arcadeFor(*game)) {
        d.coreFile = entry->coreFound ? entry->coreFile : std::string();
        d.romFile = entry->romName;
    }
    for (const Game::Variant &version : game->versions) d.versions.push_back({version.path, version.label});
    return d;
}

LaunchResult AppLibraryAdapter::launch(const GameRef &ref, const std::string &versionId) {
    LaunchResult result;
    const GameSystem *sys = system(ref.system);
    if (!sys) { result.error = "The system is not in the library."; return result; }
    Game selected = ::Library::makeGame(*sys, ref.id);
    if (const Game *game = findGame(*sys, ref.id)) {
        selected.name = game->name;
        if (!versionId.empty()) {
            selected.path = versionId;
        } else if (organize() && game->versions.size() > 1) {
            std::vector<std::string> candidates;
            for (const Game::Variant &v : game->versions) candidates.push_back(v.path);
            const std::string recent = context_.history.mostRecentVersion(sys->name, game->name, candidates);
            if (recent.empty()) { result.status = LaunchStatus::NeedsVersion; return result; }
            selected.path = recent;
        }
    } else if (!versionId.empty()) {
        selected.path = versionId;
    }
    if (selected.path != ref.id) selected = [&] {
        Game g = ::Library::makeGame(*sys, selected.path);
        g.name = selected.name;
        return g;
    }();
    context_.preview.stop();
    if (context_.launcher.launchGame(*sys, selected)) {
        context_.history.remember(sys->name, selected.path, selected.name);
        context_.standDown = true;
        result.status = LaunchStatus::Started;
        ++revision_;
    } else {
        result.error = context_.launcher.lastError();
        context_.showError("Could not start " + selected.name, result.error);
    }
    return result;
}

// ---- disc

DiscStatus AppLibraryAdapter::disc() {
    discWanted_ = true;
    if (!discPolled_) pollDisc();   // the first look must not show the empty defaults
    DiscStatus s;
    switch (discSnap_.state) {
        case DiscDriveService::MediaState::NoDrive:      s.media = DiscStatus::Media::NoDrive; break;
        case DiscDriveService::MediaState::NoDisc:       s.media = DiscStatus::Media::NoDisc; break;
        case DiscDriveService::MediaState::Checking:     s.media = DiscStatus::Media::Checking; break;
        case DiscDriveService::MediaState::Readable:     s.media = DiscStatus::Media::Readable; break;
        case DiscDriveService::MediaState::Unsupported:  s.media = DiscStatus::Media::Unsupported; break;
        case DiscDriveService::MediaState::Unreadable:   s.media = DiscStatus::Media::Unreadable; break;
    }
    switch (discSnap_.system) {
        case DiscDriveService::System::PlayStation: s.console = DiscStatus::Console::PlayStation; break;
        case DiscDriveService::System::Saturn:      s.console = DiscStatus::Console::Saturn; break;
        default:                                    s.console = DiscStatus::Console::Unknown; break;
    }
    s.device = discSnap_.device;
    s.model = discSnap_.model;
    s.title = discSnap_.title;
    s.identifier = discSnap_.identifier;
    s.region = discSnap_.region;
    s.error = discSnap_.error;
    s.generation = discSnap_.generation;
    s.coreReady = coreSnap_.readyFor(discSnap_.system);
    s.packageBusy = context_.discInstaller.busy();
    if (!s.coreReady) {
        const bool rbfMissing = discSnap_.system == DiscDriveService::System::PlayStation
            ? !coreSnap_.psxCoreInstalled
            : discSnap_.system == DiscDriveService::System::Saturn && !coreSnap_.saturnCoreInstalled;
        s.blocker = rbfMissing ? DiscStatus::Blocker::CoreMissing : DiscStatus::Blocker::PackageMissing;
    }
    s.launching = launching_;
    if (s.coreReady) {   // Classic: ready, but the MGL path is empty -> A says "The Physical Disc MGL is unavailable."
        const std::string &mgl = discSnap_.system == DiscDriveService::System::PlayStation ? coreSnap_.psxMgl : coreSnap_.saturnMgl;
        s.mglMissing = mgl.empty();
    }
    switch (artSnap_.state) {
        case DiscArtworkService::State::Idle:      s.artState = DiscStatus::ArtState::Idle; break;
        case DiscArtworkService::State::Searching:
            // Backstop: an artwork lookup that has run for two minutes is shown as "none found".
            s.artState = std::chrono::steady_clock::now() - artSearchStarted_ > std::chrono::seconds(120)
                ? DiscStatus::ArtState::Missing : DiscStatus::ArtState::Searching;
            break;
        case DiscArtworkService::State::Ready:     s.artState = DiscStatus::ArtState::Ready; break;
        case DiscArtworkService::State::Missing:   s.artState = DiscStatus::ArtState::Missing; break;
    }
    s.year = artSnap_.releaseYear;
    s.players = artSnap_.players;
    s.rating = artSnap_.rating;
    s.coop = artSnap_.coop;
    s.overview = artSnap_.overview;
    s.screenshotCount = int(artSnap_.screenshots.size());
    return s;
}

Art AppLibraryAdapter::discArt(ArtSize size) {
    discWanted_ = true;
    if (artSnap_.boxart.empty()) return Art();
    const ArtBounds b = artBounds(size);
    return artFor("disc|" + artSnap_.boxart + "|" + std::to_string(b.width), {artSnap_.boxart}, b.width, b.height, true);
}

Art AppLibraryAdapter::discScreenshot(int index, ArtSize size) {
    discWanted_ = true;
    if (index < 0 || index >= int(artSnap_.screenshots.size())) return Art();
    const std::string &path = artSnap_.screenshots[size_t(index)];
    const ArtBounds b = artBounds(size);
    return artFor("shot|" + path + "|" + std::to_string(b.width), {path}, b.width, b.height, false);
}

void AppLibraryAdapter::startDisc() {
    const DiscDriveService::Snapshot drive = context_.discDrive.snapshot();
    if (launching_ || drive.state != DiscDriveService::MediaState::Readable) return;
    const DiscCoreManager::Status cores = context_.discCores.snapshot();
    if (cores.readyFor(drive.system)) {   // Classic: no recheck without an MGL
        const std::string &mgl = drive.system == DiscDriveService::System::PlayStation ? cores.psxMgl : cores.saturnMgl;
        if (mgl.empty()) { context_.notify("The Physical Disc MGL is unavailable.", 5.0f); return; }
    }
    // Same sequence as the Classic Disc screen: a fresh look at the disc and the core first.
    launching_ = true;
    launchGeneration_ = drive.generation;
    launchRevision_ = context_.discCores.requestInspection();
    launchStarted_ = std::chrono::steady_clock::now();
    context_.discDrive.requestProbe();
    DebugLog::info("app disc launch: fresh disc/core recheck started");
    context_.notify("Rechecking disc and Physical Disc core…", 4.0f);
    ++revision_;
}

void AppLibraryAdapter::stepDiscLaunch() {
    if (!launching_) return;
    const DiscDriveService::Snapshot drive = context_.discDrive.snapshot();
    const DiscCoreManager::Status cores = context_.discCores.snapshot();
    auto stop = [&](const char *why, const char *message) {
        launching_ = false;
        DebugLog::warn(std::string("app disc launch: ") + why);
        context_.notify(message, 5.0f);
        ++revision_;
    };
    if (std::chrono::steady_clock::now() - launchStarted_ > std::chrono::seconds(15))
        return stop("recheck timed out", "Disc/core recheck timed out. Press Start game to try again.");
    if (context_.discInstaller.busy())
        return stop("package update started", "Physical Disc package update started. Press Start game when it finishes.");
    if (drive.state == DiscDriveService::MediaState::Checking || cores.inspectionRevision < launchRevision_) return;
    if (drive.generation != launchGeneration_)
        return stop("disc changed", "Disc changed during the check. Press Start game again.");
    std::string mgl;
    if (drive.system == DiscDriveService::System::PlayStation) mgl = cores.psxMgl;
    if (drive.system == DiscDriveService::System::Saturn) mgl = cores.saturnMgl;
    launching_ = false;
    ++revision_;
    if (drive.state == DiscDriveService::MediaState::Readable && cores.readyFor(drive.system) && !mgl.empty()) {
        if (context_.launcher.launchPhysicalDisc(mgl)) context_.standDown = true;
        else context_.showError("Could not start Physical Disc", context_.launcher.lastError());
    } else {
        context_.notify("Disc or Physical Disc core is no longer ready. Check the status and retry.", 6.0f);
    }
}

void AppLibraryAdapter::cancelDiscLaunch() {
    if (!launching_) return;
    launching_ = false;
    ++revision_;
}

void AppLibraryAdapter::rescanDisc() {
    context_.discDrive.requestProbe();
    ++revision_;
}

void AppLibraryAdapter::reloadDiscArt() {
    const DiscDriveService::Snapshot drive = context_.discDrive.snapshot();
    if (drive.state != DiscDriveService::MediaState::Readable || drive.system == DiscDriveService::System::Unknown) return;
    if (!artSnap_.boxart.empty()) {
        const size_t slash = artSnap_.boxart.find_last_of('/');
        const std::string dir = slash == std::string::npos ? std::string() : artSnap_.boxart.substr(0, slash + 1);
        std::vector<std::string> paths{artSnap_.boxart};
        for (int i = 1; i <= 4; ++i) paths.push_back(dir + "screenshot-" + std::to_string(i) + ".jpg");
        context_.images.invalidateCandidates(paths);
    }
    converted_.clear();
    context_.discArtwork.reload(drive);
    ++revision_;
}
