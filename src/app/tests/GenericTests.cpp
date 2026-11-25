// The generic tests: the page invariants over every page id (`app-render --list-pages`) and a seeded random walk over every flow.

#include <cstdio>
#include <cstdlib>

#include "app/tests/Generic.h"
#include "app/tests/Test.h"

namespace tt {
namespace {

// ---- Every page id of every category, opened the way the host runner does (--category X --page Y).
TEST(generic_every_listed_page) {
    std::vector<std::string> ids;
    {
        Rig r;
        ids = r.tab.pageIdsForTest();
    }
    CHECK(ids.size() > 40);
    int opened = 0;
    for (const std::string &id : ids) {
        // 1. opens, shows, draws, has sane hints, and B leaves it
        {
            Rig r;
            r.note(id);
            // the pages that show a running task need the service in that state (otherwise they wait for ever, as nothing runs)
            if (id == "library/cores.installing") r.set("library.cores", "installing");
            if (id == "updates/busy") r.set("updates.phase", "downloading");
            const bool ok = r.openPage(id);
            CHECK_MSG(ok, "page id " + id + " is listed but does not open");
            if (!ok) continue;
            ++opened;
            checkPage(r);
            r.wait(1.0f);
            checkPage(r);
            r.wait(3.0f);
            checkPage(r);
            // the host runner keeps these three open whatever the snapshot says (UpdatePageEnv::forced); not a flow's own behaviour
            if (id == "updates/busy") { r.guiCloses(); continue; }
            const bool left = escapeWithB(r);
            CHECK_MSG(left, "B does not empty the stack from " + id + " (still " + std::to_string(r.depth()) + " pages: " + r.hintLine() + ")");
        }
        // 2. the GUI closes while it is open: everything is closed, nothing keeps the pad or a task
        {
            Rig r;
            r.note(id + " + GUI closes");
            if (!r.openPage(id)) continue;
            r.wait(0.5f);
            r.guiCloses();
            CHECK_MSG(r.stack().empty(), "the GUI closed but pages are still open: " + id);
            CHECK_MSG(!r.pads().rawInputWanted, "the GUI closed and the pad is still captured after " + id);
            CHECK_MSG(!r.pads().scanning(), "the GUI closed and btpair still runs after " + id);
        }
    }
    CHECK(opened == int(ids.size()));
    std::printf("    %d page ids opened\n", opened);
}

// ---- A seeded random walk over one flow. After every step the generic page check runs plus the flow's own watch.
void runSpec(const FlowSpec &spec, Rig &r) {
    if (spec.setup) spec.setup(r);
    spec.open(r);
}

void monkey(size_t index) {
    const FlowSpec &spec = flowSpecs()[index];
    // TT_WALKS / TT_LENGTH: a longer random search (the default is short enough for every run)
    const int walks = std::getenv("TT_WALKS") ? std::atoi(std::getenv("TT_WALKS")) : 40;
    const int length = std::getenv("TT_LENGTH") ? std::atoi(std::getenv("TT_LENGTH")) : 45, replays = 14;
    int ended = 0, escapes = 0;
    for (int w = 0; w < walks; ++w) {
        Rng rng(unsigned(w) + 1 + unsigned(index) * 1000);
        Rig r;
        runSpec(spec, r);
        r.watch = spec.watch;
        r.note(spec.name + " walk " + std::to_string(w));
        const size_t base = r.base();
        std::vector<Act> acts;
        for (int i = 0; i < length; ++i) {
            Act a;
            const int pick = rng.below(100);
            if (pick < 64) { a.kind = Act::Key; a.text = spec.keys[size_t(rng.below(int(spec.keys.size())))]; }
            else if (pick < 92 || spec.twiddles.empty()) {
                a.kind = Act::Wait;
                static const float waits[] = {0.3f, 0.3f, 1.0f, 2.5f, 6.0f, 31.0f};
                a.seconds = waits[rng.below(6)];
            } else {
                const Twiddle &t = spec.twiddles[size_t(rng.below(int(spec.twiddles.size())))];
                a.kind = Act::Set; a.text = t.key; a.value = t.values[size_t(rng.below(int(t.values.size())))];
            }
            acts.push_back(a);
            // B on a Step page always moves: to another page or out of the flow (never "back" to the page it is on)
            const bool backOnStep = a.kind == Act::Key && a.text == "b" && r.flow() && r.wpage() && r.wpage()->kind() == ui::WizardPage::Kind::Step;
            const int idBefore = r.id();
            apply(r, a);
            if (backOnStep && r.flow()) {
                if (r.id() == idBefore)
                    KNOWN_BUG(false, spec.name + ": B on a Step page lands on the same page again (Wizard history keeps the page when Back has an explicit target)");
            }
            CHECK_MSG(r.depth() <= base + 3, "the stack grew to " + std::to_string(r.depth()) + " pages");
            if (r.depth() <= base) { ++ended; break; }
            checkPage(r, i % 3 == 0);
            if (w < 16 + walks / 10 && i % 9 == 4 && escapes < walks * replays) {   // the same prefix again, then B only
                ++escapes;
                Rig q;
                runSpec(spec, q);
                for (size_t k = 0; k < acts.size(); ++k) apply(q, acts[k]);
                q.note("then B only (prefix " + describe(acts, acts.size()) + ")");
                const bool left = escapeWithB(q);
                {
                    std::string seq = describe(acts, acts.size());
                    if (seq.size() > 900) seq = "..." + seq.substr(seq.size() - 897);
                    CHECK_MSG(left, "B does not leave " + spec.name + " (page " + std::to_string(q.id()) + " " + q.hintLine() + ") after: " + seq);
                }
            }
        }
        r.mem["guiClosing"] = 1;
        r.guiCloses();
        CHECK_MSG(r.stack().empty(), "the GUI closed but pages are still open");
        if (spec.atEnd) spec.atEnd(r);
    }
    std::printf("    %s: %d walks, %d ended by themselves, %d B-only escapes\n", spec.name.c_str(), walks, ended, escapes);
}

// ---- From the very first page, B alone leaves every flow (and what it started is stopped).
void escapeFromStart(size_t index) {
    const FlowSpec &spec = flowSpecs()[index];
    Rig r;
    runSpec(spec, r);
    r.watch = spec.watch;
    CHECK_MSG(r.active(), spec.name + " did not open");
    checkPage(r);
    CHECK(escapeWithB(r, 6));
    CHECK_MSG(r.stack().empty(), "B from the first page does not leave " + spec.name);
    r.mem["guiClosing"] = 1;
    r.guiCloses();
    if (spec.atEnd) spec.atEnd(r);
}

struct Registration {
    Registration() {
        for (size_t i = 0; i < flowSpecs().size(); ++i) {
            const std::string name = flowSpecs()[i].name;
            cases().push_back({"generic_monkey/" + name, [i] { monkey(i); }});
            cases().push_back({"generic_escape_from_start/" + name, [i] { escapeFromStart(i); }});
        }
    }
};
static Registration registration;

}  // namespace
}  // namespace tt
