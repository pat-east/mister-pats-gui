// ui::BusyPage: the elapsed time, the wait note that appears only after a quiet period, and the 1 Hz detail line.
// Framework behaviour, driven with a bare Wizard (no screen).

#include <memory>

#include "app/tests/Test.h"
#include "ui-framework/kit/Text.h"
#include "ui-framework/kit/WizardParts.h"

namespace tt {
namespace {

struct Bench {
    ui::Wizard wizard;
    ui::BusyPage *page = nullptr;
    Bench() {
        ui::registerFrameworkStrings();
        std::unique_ptr<ui::BusyPage> p(new ui::BusyPage(ui::Text::data("Work")));
        p->status = ui::Text::data("Phase one");
        p->waitNote = ui::Text::data("Be patient");
        page = wizard.add(0, std::move(p));
        wizard.open(0);
    }
    void run(float seconds) {   // 0.1 s frames
        for (int i = 0; i < int(seconds * 10.0f + 0.5f); ++i) wizard.update(0.1f);
    }
};

TEST(busy_elapsed_format) {
    CHECK_EQ(ui::formatElapsed(0), std::string("0:00"));
    CHECK_EQ(ui::formatElapsed(5), std::string("0:05"));
    CHECK_EQ(ui::formatElapsed(85), std::string("1:25"));
    CHECK_EQ(ui::formatElapsed(3660), std::string("61:00"));
    CHECK_EQ(ui::formatElapsed(-3), std::string("0:00"));
}

TEST(busy_elapsed_counts_whole_seconds) {
    Bench b;
    CHECK_EQ(b.page->elapsedSeconds(), 0);
    b.run(5.0f);
    CHECK_EQ(b.page->elapsedSeconds(), 5);
    CHECK_EQ(b.page->elapsedText(), std::string("Running for 0:05"));
    b.run(80.0f);
    CHECK_EQ(b.page->elapsedText(), std::string("Running for 1:25"));
}

TEST(busy_wait_note_only_after_quiet_period_and_reset_by_a_change) {
    Bench b;
    b.run(9.0f);
    CHECK(!b.page->waitNoteVisible());
    b.run(1.5f);
    CHECK(b.page->waitNoteVisible());
    // any visible change takes it away again: detail, status, bar
    b.page->detail = ui::Text::data("Now this");
    b.run(0.2f);
    CHECK(!b.page->waitNoteVisible());
    b.run(10.0f);
    CHECK(b.page->waitNoteVisible());
    b.page->fraction = 0.5f;
    b.run(0.2f);
    CHECK(!b.page->waitNoteVisible());
    b.run(10.0f);
    CHECK(b.page->waitNoteVisible());
    b.page->status = ui::Text::data("Phase two");
    b.run(0.2f);
    CHECK(!b.page->waitNoteVisible());
    // the elapsed clock alone is not a change
    b.run(10.0f);
    CHECK(b.page->waitNoteVisible());
}

TEST(busy_set_detail_is_throttled_to_one_a_second) {
    Bench b;
    b.page->setDetail(ui::Text::data("one"));
    CHECK_EQ(b.page->detail.str(), std::string("one"));    // the first value at once
    b.page->setDetail(ui::Text::data("two"));
    b.page->setDetail(ui::Text::data("three"));
    CHECK_EQ(b.page->detail.str(), std::string("one"));    // newer values wait
    b.run(0.5f);
    CHECK_EQ(b.page->detail.str(), std::string("one"));
    b.run(0.6f);
    CHECK_EQ(b.page->detail.str(), std::string("three"));  // the latest one is shown, not every one
    b.page->setDetail(ui::Text());
    CHECK(b.page->detail.empty());                         // an emptied line goes at once
}

}  // namespace
}  // namespace tt
