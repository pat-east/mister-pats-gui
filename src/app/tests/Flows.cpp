// The list of flows the generic tests walk: how each one opens, which sample states change its paths, and the rules that
// belong to that flow alone (the Wi-Fi rollback, "the task is closed once", "nothing keeps running after the GUI closed").

#include "app/tests/Generic.h"
#include "app/tests/Test.h"

namespace tt {
namespace {

using Open = std::function<void(Rig &)>;

Open row(const char *category, const char *rowId) {
    std::string c = category, id = rowId;
    return [c, id](Rig &r) { r.open(c, id); };
}

// "library.build" and "library.boxart": the visit ends once, and libraryTaskClosed() is called exactly once for it.
void libraryVisit(Rig &r, bool before) {
    if (before) return;
    const bool now = r.active();
    if (r.mem["opened"] == 0) { r.mem["opened"] = 1; r.mem["closed0"] = r.lib().tasksClosed; r.mem["was"] = now; return; }
    if (r.mem["was"] && !now && !r.mem["guiClosing"]) {
        CHECK_MSG(r.lib().tasksClosed - r.mem["closed0"] == 1,
                  "libraryTaskClosed() called " + std::to_string(r.lib().tasksClosed - r.mem["closed0"]) + " times for one visit (want exactly 1)");
    }
    r.mem["was"] = now;
}

// Wi-Fi: whenever the flow ends while a test is active, the rollback is called exactly once (or the network was saved,
// or the service rolled back by itself); never a second rollback; a test never outlives the flow.
void wifiRollback(Rig &r, bool before) {
    if (before) {
        r.mem["testing"] = r.sys().testing;
        r.mem["rollbacks"] = r.calls("wifi.rollback");
        r.mem["saves"] = r.calls("wifi.save");
        r.mem["was"] = r.active();
        return;
    }
    const long rolled = r.calls("wifi.rollback") - r.mem["rollbacks"];
    CHECK_MSG(rolled <= 1, "wifi.rollback called " + std::to_string(rolled) + " times in one step");
    if (r.mem["was"] && !r.active()) {   // the flow ended in this step
        const bool saved = r.sys().calls.size() && r.calls("wifi.save") > r.mem["saves"] && !r.sys().testing;
        if (r.mem["testing"] && !saved) CHECK_MSG(rolled == 1, "the flow ended while a test was active but wifi.rollback was called " + std::to_string(rolled) + " times");
        CHECK_MSG(!r.sys().testing, "the flow ended and the Wi-Fi test is still active in the service");
    }
}

void wifiAtEnd(Rig &r) {
    CHECK_MSG(!r.sys().testing, "the GUI closed and the Wi-Fi test is still active in the service");
    CHECK_MSG(r.calls("wifi.rollback") <= r.calls("wifi.begin"), "more rollbacks than tests: " + r.callsText());
}

void nothingRunning(Rig &r) {
    using S = ScanProgress::State;
    const S s = r.lib().scan().state;
    CHECK_MSG(s != S::Discovering && s != S::Scanning && s != S::Writing, "the GUI closed and the game list scan still runs");
    const ArtworkProgress::State a = r.lib().artwork().state;
    CHECK_MSG(a != ArtworkProgress::State::Preparing && a != ArtworkProgress::State::Working, "the GUI closed and the box-art job still runs");
    const CoreMraProgress::Phase c = r.lib().cores().phase;
    CHECK_MSG(c != CoreMraProgress::Phase::Checking, "the GUI closed and the core check still runs");
    CHECK_MSG(!r.pads().rawInputWanted, "the GUI closed and the pad is still captured (raw input wanted)");
    CHECK_MSG(!r.pads().scanning(), "the GUI closed and btpair still runs");
}

// Controllers: the raw-input flag of the service follows the flow's own "wanted" while the flow is open.
void rawFollowsFlow(Rig &r, bool before) {
    if (before) return;
    WizardFlowPage *f = r.flow();
    if (f) CHECK_MSG(r.padRawFlag() == f->wantsRawInput(), std::string("raw input: the service says ") + (r.padRawFlag() ? "wanted" : "not wanted") + " but the flow says the opposite");
    else CHECK_MSG(!r.padRawFlag() || r.depth() > 0, "no flow is open and the pad is still captured");
}

// The first start: the guide is completed exactly once, only when the user ends it (never while it is open, never when the GUI closes).
void firstRunComplete(Rig &r, bool before) {
    if (before) return;
    const int n = r.onboarding().completes;
    CHECK_MSG(n <= 1, "setComplete() called " + std::to_string(n) + " times (want at most 1)");
    if (r.active()) CHECK_MSG(n == 0, "the guide is complete while it is still open");
    else if (!r.mem["guiClosing"]) CHECK_MSG(n == 1, "the guide ended by the user but setComplete() was called " + std::to_string(n) + " times");
    if (r.active()) r.mem["wasOpen"] = 1;
}

void firstRunAtEnd(Rig &r) {
    nothingRunning(r);
    CHECK_MSG(r.onboarding().completes <= 1, "setComplete() called more than once");
}

std::function<void(Rig &, bool)> both(std::function<void(Rig &, bool)> a, std::function<void(Rig &, bool)> b) {
    return [a, b](Rig &r, bool before) { a(r, before); b(r, before); };
}

std::vector<FlowSpec> build() {
    std::vector<FlowSpec> v;
    auto add = [&](FlowSpec s) { v.push_back(std::move(s)); };

    {
        FlowSpec s;
        s.name = "library.scan";
        s.open = row("library", "library.build");
        s.twiddles = {{"library.scan.ends", {"done", "failed", "nodrives"}}, {"library.art.ends", {"done", "nomatch"}},
                      {"library.roots", {"none", "one", "two", "six"}}, {"library.systems", {"none", "many", "two"}}};
        s.watch = libraryVisit;
        s.atEnd = nothingRunning;
        add(s);
    }
    {
        FlowSpec s;
        s.name = "library.artwork";
        s.open = row("library", "library.boxart");
        s.twiddles = {{"library.art.ends", {"done", "nomatch"}}, {"library.systems", {"none", "many", "two"}}};
        s.watch = libraryVisit;
        s.atEnd = nothingRunning;
        add(s);
    }
    {
        FlowSpec s;
        s.name = "library.cores";
        s.open = row("library", "library.coreupdates");
        s.keys = {"a", "a", "a", "b", "down", "up", "start", "left", "right", "x", "y", "x", "y"};
        s.twiddles = {{"library.cores.ends", {"ready", "failed"}}, {"library.install.ends", {"complete", "failed"}},
                      {"library.online", {"0", "1"}}, {"library.cores.count", {"0", "5", "24", "60"}}};
        s.atEnd = nothingRunning;
        add(s);
    }
    {
        FlowSpec s;
        s.name = "system.wifi";
        s.open = row("system", "system.wifi");
        s.twiddles = {{"system.wifiscan", {"ok", "fail", "error", "none"}}, {"system.wifitest", {"ok", "fail", "stall"}},
                      {"system.country", {"0", "1"}}, {"system.save", {"ok", "fail"}}, {"system.wifibegin", {"ok", "fail"}},
                      {"system.wifi", {"none", "ready"}}};
        s.keys = {"a", "a", "a", "a", "b", "down", "down", "up", "start", "x", "y", "left", "right"};
        s.watch = wifiRollback;
        s.atEnd = wifiAtEnd;
        add(s);
    }
    {
        FlowSpec s;
        s.name = "system.timezone";
        s.open = row("system", "system.timezone");
        s.twiddles = {{"system.zones", {"ok", "empty", "regionempty"}}, {"system.zonesave", {"ok", "fail"}}};
        s.keys = {"a", "a", "a", "b", "down", "down", "up", "lnext", "lprev", "left", "right"};
        add(s);
    }
    {
        FlowSpec s;
        s.name = "system.backup";
        s.open = row("system", "system.backup");
        s.twiddles = {{"system.backupresult", {"ok", "fail"}}};
        add(s);
    }
    {
        FlowSpec s;
        s.name = "system.icons";
        s.open = row("system", "system.icons");
        s.twiddles = {{"system.iconsresult", {"ok", "fail"}}, {"system.icons", {"3", "ready", "startfails"}}};
        add(s);
    }
    {
        FlowSpec s;
        s.name = "system.discinstall";
        s.setup = [](Rig &r) { r.set("system.discpackage", "none"); };
        s.open = row("system", "system.disccores");
        s.twiddles = {{"system.installend", {"ready", "failed", "cancelled"}}, {"system.discinstall", {"idle", "failed", "blocked"}}};
        add(s);
    }
    {
        FlowSpec s;
        s.name = "system.reboot";
        s.open = row("system", "system.reboot");
        s.twiddles = {{"system.reboot", {"ok", "fails"}}};
        add(s);
    }
    {
        FlowSpec s;
        s.name = "updates.install";
        s.setup = [](Rig &r) { r.set("updates.phase", "available"); };
        s.open = row("updates", "updates.install");
        s.twiddles = {{"updates.total", {"0", "1000000"}}, {"updates.fail", {"none", "restart", "pin"}}, {"updates.pinned", {"0", "1"}}};
        add(s);
    }
    {
        FlowSpec s;
        s.name = "updates.restart";
        s.setup = [](Rig &r) { r.set("updates.phase", "restart-hidden"); };
        s.open = row("updates", "updates.restart");
        s.twiddles = {{"updates.fail", {"none", "restart", "pin"}}, {"updates.pinned", {"0", "1"}}};
        add(s);
    }
    {
        FlowSpec s;
        s.name = "updates.pinned";
        s.setup = [](Rig &r) { r.set("updates.pinned", "1"); r.set("updates.phase", "restart-hidden"); };
        s.open = row("updates", "updates.restart");
        s.twiddles = {{"updates.fail", {"none", "restart", "pin"}}};
        add(s);
    }
    {
        FlowSpec s;
        s.name = "controllers.wizard";
        s.open = [](Rig &r) { r.openPage("controllers/wizard"); r.setBase(1); };
        s.twiddles = {{"controllers.pad.keys", {"", "start", "south", "dpadup", "east"}}, {"controllers.script", {"none", "wizard", "wizard-abort"}},
                      {"controllers.mapping.fail", {"0", "1"}}, {"controllers.unplug", {"1", "30"}}};
        s.keys = {"a", "a", "start", "b", "down", "up", "x", "y"};
        s.watch = rawFollowsFlow;
        s.atEnd = nothingRunning;
        add(s);
    }
    {
        FlowSpec s;
        s.name = "controllers.bluetooth";
        s.open = [](Rig &r) { r.openPage("controllers/bluetooth"); r.setBase(1); };
        s.twiddles = {{"controllers.bt.found", {"never", "1", "3"}}, {"controllers.bt.start", {"ok", "fail"}}, {"controllers.bt.timeout", {"2", "5", "120"}},
                      {"controllers.bt.mapped", {"0", "1"}}, {"controllers.bluetooth", {"none", "available"}}};
        s.keys = {"a", "a", "a", "b", "start", "down", "up"};
        s.watch = rawFollowsFlow;
        s.atEnd = nothingRunning;
        add(s);
    }
    {
        FlowSpec s;
        s.name = "first.run";
        s.open = [](Rig &r) { r.openFirstRun(); };
        s.twiddles = {{"library.scan.ends", {"done", "failed", "nodrives", "nogames"}}, {"library.art.ends", {"done", "nomatch"}},
                      {"accessibility.font", {"on", "missing", "failing"}}, {"system.icons", {"ready", "3", "startfails"}},
                      {"system.iconsresult", {"ok", "fail"}}};
        s.watch = firstRunComplete;
        s.atEnd = firstRunAtEnd;
        add(s);
    }
    (void)both;
    return v;
}

}  // namespace

const std::vector<FlowSpec> &flowSpecs() {
    static const std::vector<FlowSpec> specs = build();
    return specs;
}

const FlowSpec &specNamed(const std::string &name) {
    for (const FlowSpec &s : flowSpecs())
        if (s.name == name) return s;
    CHECK_MSG(false, "no flow spec " + name);
    return flowSpecs()[0];
}

}  // namespace tt
