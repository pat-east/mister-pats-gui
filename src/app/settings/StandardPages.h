#pragma once

// The pages every category needs, built from framework components only (src/ui-framework/kit), so a category never invents its
// own chooser, confirmation, progress view, keyboard page or text viewer. Each page takes a small spec of
// ui::Text keys and std::function callbacks; a callback receives the PageStack and may open or close pages.
// All of them close themselves on their normal outcome (`closeOn...` flags say otherwise) and on Back.
//
//   Page          Presentation  Built from                       Typical use
//   ChoicePage    Modal         ui::Modal::choose                time zone (region, then city)
//   ConfirmPage   Modal         ui::Modal::confirm               reboot, install, restore
//   ResultPage    Modal         ui::Modal::info/result/error     "Wi-Fi saved", "Backup saved to ...", failures
//   ProgressPage  Content       PageHeader, ui::drawProgressPanel  download, install, check, wizard "working" steps
//   TextInputPage Modal         ui::TextEntryDialog              ScreenScraper user and password, Wi-Fi name
//   InfoPage      Content       PageHeader, KeyValueList, text    System info, Disc drive, changelog, release notes
//
// A flow of two or more steps (the Wi-Fi setup, the core updater, the scan wizard) is NOT a chain of these pages:
// it is a WizardFlowPage (WizardFlowPage.h), every step a ui::WizardPage in one dialog frame, see
// src/app/settings/PORTING.md "Flows". These pages are for a single question, notice, keyboard or viewer (and are
// what the older flows still use until they are converted).

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/ModalInput.h"
#include "app/TextEntryInput.h"
#include "app/settings/SettingsPage.h"
#include "ui-framework/kit/Data.h"
#include "ui-framework/kit/Modal.h"
#include "ui-framework/kit/PageHeader.h"
#include "ui-framework/kit/TextEntryDialog.h"

namespace patsgui {

using PageCallback = std::function<void(PageStack &)>;

// Registers the texts the standard pages use (called by the Settings tab's strings function).
void registerStandardPageStrings(ui::Catalog &c);

// The standard frame of a Content page: the page header (title left, optional info right) and the faint panel
// the three panes use. Returns the inner rectangle to draw the page's content in.
ui::Rect drawSettingsPageFrame(ui::Painter &p, const ui::Theme &t, const ui::Rect &content, const ui::Text &title,
                               const ui::Text &info = ui::Text());

// ---- ChoicePage: a list of options with the current one marked and under the cursor.
//   ChoiceSpec spec;
//   spec.title = ui::Text::key("settings.system.timezone.region");
//   spec.items = {{ui::Text::data("Europe"), {}}, {ui::Text::data("Asia"), {}}};
//   spec.current = 0;                                   // marked, and the cursor starts here
//   spec.closeOnChoose = false;                         // the region page stays under the city page
//   spec.onChosen = [](int index, PageStack &stack) { stack.push(makeCityPage(index)); };
//   pages.push(std::unique_ptr<SettingsPage>(new ChoicePage(spec)));
// Hints: Move, Select, Cancel (the words can be replaced). B cancels (onCancel) and closes the page.
struct ChoiceItem {
    ui::Text title;
    ui::Text detail;     // optional second line
};
struct ChoiceSpec {
    ui::Text title;
    std::vector<ChoiceItem> items;
    int current = -1;                   // the option in use: marked, and where the cursor starts
    int cursor = -1;                    // where the cursor starts when it differs from `current` (-1 = at `current`)
    bool closeOnChoose = true;
    std::function<void(int index, PageStack &)> onChosen;
    PageCallback onCancel;
    ui::Text chooseHint, cancelHint;    // replace "Select" / "Cancel" (empty = the framework words)
    // A status line (Classic: the muted line under a list title, e.g. the Wi-Fi scan result or an error). Drawn in a pill
    // under the panel (the choose modal has no message slot). `message` is read at draw time through
    // `messageProvider` when set, else the fixed `message`; empty = no line.
    ui::Text message;
    std::function<ui::Text()> messageProvider;
};
class ChoicePage : public SettingsPage {
public:
    explicit ChoicePage(const ChoiceSpec &spec);
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event, PageStack &stack) override;
    std::vector<ui::Hint> hints() const override;
    int cursor() const { return cursor_; }

private:
    void rebuild(int cursor);
    ChoiceSpec spec_;
    ui::ModalStack modal_;
    int cursor_ = 0;
};

// ---- ConfirmPage: a yes/no question with a body.
//   ConfirmSpec spec;
//   spec.title = ui::Text::key("settings.system.reboot.title");        // "Reboot MiSTer?"
//   spec.body = ui::Text::key("settings.system.reboot.body");          // "The system will restart now."
//   spec.confirmHint = ui::Text::key("settings.system.reboot.confirm"); // "Reboot" (A), optional
//   spec.onConfirm = [svc](PageStack &) { std::string error; svc->reboot(error); };
//   pages.push(std::unique_ptr<SettingsPage>(new ConfirmPage(spec)));
struct ConfirmSpec {
    ui::Text title, body;
    ui::Text confirmHint, cancelHint;   // replace "Confirm" / "Cancel" (empty = the framework words)
    bool closeOnConfirm = true;
    PageCallback onConfirm, onCancel;
    std::function<void()> onLeave;      // the page left the stack (any way, also with the GUI closed): undo what it started
};
class ConfirmPage : public SettingsPage {
public:
    explicit ConfirmPage(const ConfirmSpec &spec);
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event, PageStack &stack) override;
    std::vector<ui::Hint> hints() const override;
    void onClose() override { if (spec_.onLeave) spec_.onLeave(); }

private:
    ConfirmSpec spec_;
    ui::ModalStack modal_;
};

// ---- ResultPage: what happened. A on Done/Info/Warning closes; an Error offers "Try again" when onRetry is set.
//   ResultSpec spec;
//   spec.kind = ResultSpec::Error;
//   spec.title = ui::Text::key("settings.library.login.failed");           // "Could not save the login"
//   spec.body = ui::Text::key("settings.library.login.failed.body");       // "Nothing was changed. ..."
//   spec.whatToDo = ui::Text::key("settings.library.login.failed.next");
//   spec.details = ui::Text::data(osError);                                // small secondary line, optional
//   pages.push(std::unique_ptr<SettingsPage>(new ResultPage(spec)));
struct ResultSpec {
    enum Kind { Info, Done, Warning, Error } kind = Info;
    ui::Text title;
    ui::Text status;       // Done/Warning: the status word in the tone ("Saved")
    ui::Text body;         // the text; for an Error: what went wrong, and whether anything was changed
    ui::Text whatToDo;     // Error: what to try
    ui::Text details;      // Error: the technical reason, a small line
    PageCallback onRetry;  // Error: set to offer A "Try again"
    ui::Text closeHint;    // replaces "Close" on the A hint of Info/Done/Warning ("Finish"); empty = the framework word
    PageCallback onClose;
};
class ResultPage : public SettingsPage {
public:
    explicit ResultPage(const ResultSpec &spec);
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event, PageStack &stack) override;
    std::vector<ui::Hint> hints() const override;

private:
    ResultSpec spec_;
    ui::ModalStack modal_;
};

// ---- ProgressPage: calm progress with one status line, an optional detail line and B Cancel.
//   ProgressSpec spec;
//   spec.title = ui::Text::key("settings.updates.downloading");
//   spec.onCancel = [svc](PageStack &) { svc->cancelBeforeCommit(); };      // the page then closes
//   auto page = std::make_unique<ProgressPage>(spec);
//   page->onUpdate = [svc](ProgressPage &self, PageStack &stack) {          // runs every frame
//       const UpdateInfo info = svc->snapshot();                           // poll, never block
//       self.setFraction(info.bytesTotal ? float(info.bytesDone) / float(info.bytesTotal) : 0.0f);
//       self.setStatus(ui::Text::data(percentText(info)));
//       if (info.phase == UpdatePhase::RestartRequired) stack.replace(makeDonePage());
//   };
//   pages.push(std::move(page));
// `cancellable = false` swallows Back and shows no Cancel hint (an install that must not be interrupted).
struct ProgressSpec {
    ui::Text header;                 // the page header's title; empty = "Settings"
    ui::Text title, status, detail;
    float fraction = 0.0f;
    bool cancellable = true;
    ui::Text cancelHint;             // replaces "Cancel"
    PageCallback onCancel;           // B on a cancellable page; the page closes afterwards
    std::function<void()> onLeave;   // the page left the stack (any way, also with the GUI closed): undo what it started
};
class ProgressPage : public SettingsPage {
public:
    explicit ProgressPage(const ProgressSpec &spec);
    // Runs every frame while the page is on top: poll a service and call the setters.
    std::function<void(ProgressPage &, PageStack &)> onUpdate;

    void setTitle(const ui::Text &v) { set(spec_.title, v); }
    void setStatus(const ui::Text &v) { set(spec_.status, v); }
    void setDetail(const ui::Text &v) { set(spec_.detail, v); }
    void setFraction(float v);
    void setCancellable(bool v) { if (spec_.cancellable != v) { spec_.cancellable = v; dirty_ = true; } }
    float deltaSeconds() const { return deltaSeconds_; }   // the frame time of the update that runs onUpdate

    PagePresentation presentation() const override { return PagePresentation::Content; }
    bool update(float deltaSeconds, PageStack &stack) override;
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event, PageStack &stack) override;
    std::vector<ui::Hint> hints() const override;
    void onClose() override { if (spec_.onLeave) spec_.onLeave(); }

private:
    void set(ui::Text &field, const ui::Text &v);
    ProgressSpec spec_;
    bool dirty_ = false;
    float deltaSeconds_ = 0.0f;
};

// ---- TextInputPage: the on-screen keyboard page (ui::TextEntryDialog).
//   TextInputSpec spec;
//   spec.entry.title = ui::Text::key("settings.library.ss.password");
//   spec.entry.masked = true;                  // one * per character; Show/Hide key on the keyboard
//   spec.entry.maxLength = 64;                 // Classic's limit
//   spec.validate = [](const std::string &v) { return v.size() < 8 ? ui::Text::key("settings.wifi.shortPassword")
//                                                                  : ui::Text(); };   // empty = valid
//   spec.onSubmit = [svc](const std::string &value, PageStack &) { svc->save(value); };
//   pages.push(std::unique_ptr<SettingsPage>(new TextInputPage(spec)));
// A value that fails validate() stays in the keyboard and a notice modal says why (Classic shows the message in
// the dialog; the framework dialog has no message line: a listed difference).
struct TextInputSpec {
    ui::TextEntrySpec entry;
    std::function<ui::Text(const std::string &value)> validate;   // optional; returns the message, empty = valid
    ui::Text invalidTitle;                                          // the notice's title (default "Check your entry")
    std::function<void(const std::string &value, PageStack &)> onSubmit;
    PageCallback onCancel;
    bool closeOnSubmit = true;
    // A status line under the dialog (Classic: the muted line between the field and the keyboard, e.g. "Enter the
    // password for X"). The dialog has no message slot, so it is drawn in a pill under the panel. Empty = none.
    ui::Text message;
    std::function<void()> onLeave;   // the page left the stack (any way, also with the GUI closed)
};
class TextInputPage : public SettingsPage {
public:
    explicit TextInputPage(const TextInputSpec &spec);
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event, PageStack &stack) override;
    std::vector<ui::Hint> hints() const override;
    void onClose() override;     // wipes the typed text

private:
    TextInputSpec spec_;
    ui::TextEntryDialog dialog_;
    ui::ModalStack notice_;
};

// ---- InfoPage: read-only content in the tab's content area, two kinds.
//   Facts (label/value rows, optionally refreshed while open):
//       InfoSpec spec;  spec.title = ui::Text::key("settings.system.info.title");
//       pages.push(InfoPage::facts(spec, [svc]() { return toEntries(svc->facts()); }, 2.0f));
//   Viewer (a scrolling text: a changelog, release notes; Up/Down read one line, A is an optional action):
//       InfoSpec spec;  spec.title = ui::Text::key("settings.updates.install.title");
//       spec.actionHint = ui::Text::key("settings.updates.install.hint");          // A "Install update"
//       spec.onAction = [svc](PageStack &) { svc->startInstall(); };
//       auto page = InfoPage::viewer(spec);
//       page->setText(releaseNotes);          // or setLoading(...), setError(..., retry), setEmpty(...)
//       pages.push(std::move(page));
struct InfoSpec {
    ui::Text title;           // the page header's title (the page's subject: "System information", "Changelog")
    ui::Text subtitle;        // the page header's info, right of the title (optional): "Full project changelog"
    ui::Text actionHint;      // viewer: the words of A ("Install update"); empty = A closes like B
    ui::Text closeHint;       // replaces "Close" on B
    ui::Text scrollHint;      // viewer: replaces "Read more" on Up/Down (Classic's release notes say "Read release notes")
    PageCallback onAction;    // viewer: A pressed (when actionHint is set); the page closes afterwards
    PageCallback onClose;     // B or close
    bool closeOnAction = true;
    // Facts: a small muted note under the rows and one accent line (Classic's Disc drive panel: the hint about
    // Install/update Disc cores and the project URL). Empty = none.
    ui::Text footer, footerAccent;
};
class InfoPage : public SettingsPage {
public:
    using Provider = std::function<std::vector<ui::KeyValueEntry>()>;
    static std::unique_ptr<InfoPage> facts(const InfoSpec &spec, Provider provider, float refreshSeconds = 0.0f);
    static std::unique_ptr<InfoPage> viewer(const InfoSpec &spec);

    // Viewer content. Text is Markdown-lite like Classic's changelog: lines starting with # are headings; ** and `
    // are removed; blank lines are paragraph gaps.
    void setText(const std::string &markdownLite);
    void setLoading(const ui::Text &line);
    void setError(const ui::Text &message, bool retry);     // retry: A "Try again" calls onRetry
    void setError(const ui::Text &headline, const ui::Text &detail, bool retry);   // detail: the raw reason, one muted line
    void setEmpty(const ui::Text &line);
    PageCallback onRetry;

    PagePresentation presentation() const override { return PagePresentation::Content; }
    bool update(float deltaSeconds, PageStack &stack) override;
    void draw(ui::Painter &p, const ui::Theme &t, const ui::Rect &content) override;
    bool handle(Event event, PageStack &stack) override;
    std::vector<ui::Hint> hints() const override;

private:
    enum class Kind { Facts, Viewer };
    enum class View { Text, Loading, Error, Empty };
    struct Line { std::string text; bool heading = false; bool blank = false; };
    InfoPage(const InfoSpec &spec, Kind kind) : spec_(spec), kind_(kind) {}
    void layoutText(ui::Painter &p, const ui::Theme &t, int width);

    InfoSpec spec_;
    Kind kind_;
    Provider provider_;
    float refreshSeconds_ = 0.0f, sinceRefresh_ = 0.0f;
    std::vector<ui::KeyValueEntry> entries_;
    bool entriesRead_ = false;

    View view_ = View::Empty;
    ui::Text message_, detail_;
    ui::KeyValueView facts_;          // Facts: the scroll position and what the last draw showed
    bool retry_ = false;
    std::vector<Line> logical_;
    std::vector<Line> wrapped_;       // wrapped to wrappedWidth_
    int wrappedWidth_ = -1, wrappedScale_ = -1;
    int scroll_ = 0, visibleLines_ = 1;
};

}  // namespace patsgui
