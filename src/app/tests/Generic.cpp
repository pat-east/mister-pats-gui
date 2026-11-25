#include "app/tests/Generic.h"

#include <algorithm>
#include <set>

#include "app/tests/Test.h"
#include "ui-framework/kit/WizardParts.h"

namespace tt {

// ------------------------------------------------------------------------------------------------ vocabulary

bool hintWordAllowed(ui::HintKey key, const std::string &word) {
    // Hints.md "Start rule": Start is listed where A is overloaded and names its own action ("Start Search", "Start Review").
    if (key == ui::HintKey::Start) return true;
    // docs/components/Hints.md: "Vocabulary (closed set)" and the application's own words that the same document names
    // ("Play", "Set time zone", "Restart MiSTer" in its notes table).
    static const std::set<std::string> words = {"Back", "Close", "Cancel", "Skip", "Later", "Open", "Choose", "Select", "Continue", "Start",
                                                "Install", "Restart", "Done", "Save", "Try again", "Move", "Scroll", "Jump", "Play",
                                                "Set time zone", "Restart MiSTer",
                                                // the on-screen keyboard's own hints (docs/components/Modal.md: TextEntryDialog)
                                                "Choose key", "Type", "Delete", "Space"};
    if (words.count(word)) return true;
    return word.compare(0, 7, "Change ") == 0 && word.size() > 7;   // "Change <noun>"
}

const std::map<std::string, std::string> &knownVocabularyDeviations() {
    // word -> where it shows (found by the generic hint check; not in Hints.md's closed set).
    static const std::map<std::string, std::string> known = {
        {"Select needed", "Core and MRA updates > list: X (settings.library.cores.hint.selectAll); not in the closed set"},
        {"Check again", "Core and MRA updates > list: Y (settings.library.cores.hint.again); not in the closed set (Try again is for failures only)"},
        {"Jump to letter", "Time zone > city list: Triggers (hint.jump); the closed set says Jump"},
    };
    return known;
}

// ------------------------------------------------------------------------------------------------ page check

void checkPage(Rig &r, bool draw) {
    PageStack &st = r.stack();
    if (st.empty()) return;
    const std::string where = " [" + r.hintLine() + "]";
    WizardFlowPage *f = r.flow();
    const std::vector<HintView> hints = r.hints();
    ui::WizardPage *wp = r.wpage();
    const bool silentBusy = wp && wp->kind() == ui::WizardPage::Kind::Busy && !wp->cancellable();
    if (!silentBusy && hints.empty()) {
        if (wp && wp->kind() == ui::WizardPage::Kind::Capture && wp->rawInput())
            KNOWN_BUG(false, "Button mapping, first step: the page lists no hint at all and B is not read (raw input); only pressing Start on the pad moves on, "
                             "and the way out (hold Start) is explained in the body only from the second step on");
        else CHECK_MSG(false, "a page without hints" + where);
    }

    if (f) {
        static std::set<std::string> reported;   // one report per distinct word per process
        for (const HintView &h : hints) {
            if (hintWordAllowed(h.key, h.word)) continue;
            const auto known = knownVocabularyDeviations().find(h.word);
            if (known != knownVocabularyDeviations().end()) {
                if (reported.insert(h.word).second) KNOWN_BUG(false, "hint word \"" + h.word + "\" is not in the closed vocabulary: " + known->second);
            } else {
                CHECK_MSG(false, "hint word \"" + h.word + "\" is not in the closed vocabulary (Hints.md)" + where);
            }
        }
    }
    if (wp) {
        const ui::WizardPage::Kind kind = wp->kind();
        if (kind == ui::WizardPage::Kind::Busy && wp->cancellable()) {
            CHECK_MSG(r.hasHint(ui::HintKey::B) && r.hintWord(ui::HintKey::B) == "Cancel" , "a cancellable Busy page does not list B Cancel" + where);
        }
        if (kind == ui::WizardPage::Kind::Busy && !wp->cancellable())
            CHECK_MSG(!r.hasHint(ui::HintKey::B), "a Busy page that cannot be cancelled lists a B hint" + where);
        if (kind == ui::WizardPage::Kind::Step || kind == ui::WizardPage::Kind::Capture) {
            ui::Text reason;
            if (!wp->canContinue(reason)) {
                CHECK_MSG(!reason.empty(), "a page that cannot continue gives no reason" + where);
                CHECK_MSG(!r.hasHint(wp->continueKey()), "a page that cannot continue still lists its continue key" + where);
            }
        }
        if (kind == ui::WizardPage::Kind::Step) {
            const bool hasBack = r.hasHint(ui::HintKey::B) || r.hasHint(ui::HintKey::AB);
            CHECK_MSG(hasBack, "a Step page lists no way out" + where);
        }
        if (kind == ui::WizardPage::Kind::Result) {
            ui::ResultPage *rp = dynamic_cast<ui::ResultPage *>(wp);
            CHECK_MSG(rp != nullptr, "a Result page that is not a ui::ResultPage");
            if (rp) {
                const ui::WizardResult &res = rp->result();
                CHECK_MSG(!res.headline.empty(), "a Result page without a headline" + where);
                CHECK_MSG(!res.changed.empty() || !res.items.empty(), "a Result page does not say what changed" + where);
                if (res.tone == ui::Tone::Error) CHECK_MSG(!res.next.empty(), "an Error result gives no next step" + where);
                // a status word: the page's own, or the tone's (Done, Warning, Error, Info); every tone has one
                const bool word = !res.status.empty() || res.tone == ui::Tone::Done || res.tone == ui::Tone::Warning ||
                                  res.tone == ui::Tone::Error || res.tone == ui::Tone::Neutral || res.tone == ui::Tone::Action;
                CHECK_MSG(word, "a Result page without a status word" + where);
                if (res.retry != ui::kNoPage) {
                    CHECK_MSG(r.hintWord(ui::HintKey::A) == (res.primary.empty() ? "Try again" : res.primary.str()), "A on a result with a retry page is not Try again" + where);
                    CHECK_MSG(r.hintWord(ui::HintKey::B) == "Close", "B on a result is not Close" + where);
                } else {
                    CHECK_MSG(r.hintWord(ui::HintKey::AB) == (res.primary.empty() ? "Done" : res.primary.str()), "A/B on a result without a retry page is not Done" + where);
                }
            }
        }
    }
    if (draw) r.draw();   // crashes and asserts surface here (the case runs in its own process)
}

// ------------------------------------------------------------------------------------------------ B leaves

bool escapeWithB(Rig &r, int maxPresses, float patience) {
    float waited = 0.0f;
    for (int i = 0; i < maxPresses && !r.stack().empty(); ++i) {
        ui::WizardPage *wp = r.wpage();
        if (wp && wp->kind() == ui::WizardPage::Kind::Busy && !wp->cancellable()) {
            // B cannot stop it: the page must not offer B, and it has to end by itself
            CHECK_MSG(!r.hasHint(ui::HintKey::B), "a page that cannot be cancelled lists B: " + r.hintLine());
            r.press(Event::Back);
            for (; waited < patience && r.wpage() == wp; waited += 0.5f) r.wait(0.5f);
            CHECK_MSG(r.wpage() != wp || r.stack().empty(), "a page that cannot be cancelled never ended (waited " + std::to_string(int(waited)) + " s)");
            if (r.wpage() == wp) return false;
            continue;
        }
        r.press(Event::Back);
        r.wait(0.3f);
        // B did not leave: a page that keeps the pad's raw input (the input test, the mapping capture) says "Hold Start" and
        // wants the Start button held on the pad itself (5 s).
        if (!r.stack().empty() && i >= 1) {
            bool hold = false;
            for (const HintView &h : r.hints())
                if (h.hold && h.key == ui::HintKey::Start) hold = true;
            if (hold) {
                // The page's Start is whichever button answered the first step (the user chose it), so hold every button: the
                // wizard does not capture while its Start is down. The sample's own scripted presses would overwrite the keys.
                r.set("controllers.script", "none");
                r.set("controllers.pad.keys", "");
                r.wait(0.4f);
                r.set("controllers.pad.keys", "south,east,north,west,tl,tr,tl2,tr2,select,start,mode,dpadup,dpaddown,dpadleft,dpadright");
                r.wait(6.0f);
                r.set("controllers.pad.keys", "");
                r.wait(0.5f);
            } else if (r.wpage() && r.wpage()->kind() == ui::WizardPage::Kind::Capture && r.wpage()->rawInput()) {
                // the first mapping step: the only thing that moves on is Start on the pad (the known bug above)
                r.set("controllers.script", "none");
                r.set("controllers.pad.keys", "start");
                r.wait(0.4f);
                r.set("controllers.pad.keys", "");
                r.wait(0.4f);
            } else if (r.wpage() && r.wpage()->kind() == ui::WizardPage::Kind::Capture && i >= 3) {
                CHECK_MSG(false, "B does not leave a Capture page and its hints name no way out: " + r.hintLine() + " (page " + std::to_string(r.id()) + ")");
                return false;
            }
        }
    }
    return r.stack().empty();
}

// ------------------------------------------------------------------------------------------------ walks

void apply(Rig &r, const Act &a) {
    if (a.kind == Act::Key) r.keys(a.text);
    else if (a.kind == Act::Wait) r.wait(a.seconds);
    else r.set(a.text, a.value);
}

std::string describe(const std::vector<Act> &acts, size_t n) {
    std::string s;
    for (size_t i = 0; i < n && i < acts.size(); ++i) {
        const Act &a = acts[i];
        s += (s.empty() ? "" : ",") + (a.kind == Act::Key ? a.text : a.kind == Act::Wait ? "wait" + std::to_string(a.seconds).substr(0, 4) : "[" + a.text + "=" + a.value + "]");
    }
    return s;
}

}  // namespace tt
