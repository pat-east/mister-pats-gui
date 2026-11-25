// ScreenScraper user name and password (Classic: SettingsScreen::openAccountEntry / handleAccountEntry,
// src/legacy/SettingsScreen.cpp:753-830). The on-screen keyboard is the framework's text entry dialog; the password is masked
// (with the Show/Hide key) and the page wipes what was typed when it closes.

#include "app/settings/library/LibraryPages.h"
#include "app/settings/StandardPages.h"

namespace patsgui {
namespace library {

void openAccountEntry(LibraryService &svc, PageStack &pages, bool user) {
    LibraryService *s = &svc;
    TextInputSpec spec;
    spec.entry.title = ui::Text::key(user ? "settings.library.account.user.title" : "settings.library.account.password.title");
    spec.entry.placeholder = ui::Text::key(user ? "settings.library.account.user.placeholder" : "settings.library.account.password.placeholder");
    spec.entry.value = user ? s->screenScraperUser() : std::string();   // the user name starts with the stored one
    spec.entry.masked = !user;
    spec.entry.maxLength = 64;      // OnScreenKeyboard accountKeyboard_{64}
    spec.entry.trim = false;        // no trimming: an empty value is saved as it is and clears the entry
    spec.entry.live = false;
    spec.message = ui::Text::key(user ? "settings.library.account.user.message" : "settings.library.account.password.message");
    spec.onSubmit = [s, user](const std::string &value, PageStack &stack) {
        if (!s->saveScreenScraperLogin(user, value)) {
            ResultSpec failed;   // Classic: context_.showError("Could not save the login", "Nothing was changed. ...")
            failed.kind = ResultSpec::Error;
            failed.title = ui::Text::key("settings.library.account.saveFailed.title");
            failed.body = ui::Text::key("settings.library.account.saveFailed.body");
            stack.push(std::unique_ptr<SettingsPage>(new ResultPage(failed)));
            return;
        }
        stack.toast(ui::Text::key(user ? "settings.library.account.user.saved" : "settings.library.account.password.saved"));
    };
    pages.push(std::unique_ptr<SettingsPage>(new TextInputPage(spec)));
}

void registerAccountStrings(ui::Catalog &c) {
    c.set("settings.library.account.user.title", "ScreenScraper user name");
    c.set("settings.library.account.user.placeholder", "Enter user name\xE2\x80\xA6");
    c.set("settings.library.account.user.message", "The user name of your ScreenScraper account");
    c.set("settings.library.account.user.saved", "ScreenScraper user name saved");
    c.set("settings.library.account.password.title", "ScreenScraper password");
    c.set("settings.library.account.password.placeholder", "Enter password\xE2\x80\xA6");
    c.set("settings.library.account.password.message", "The password of your ScreenScraper account");
    c.set("settings.library.account.password.saved", "ScreenScraper password saved");
    c.set("settings.library.account.saveFailed.title", "Could not save the login");
    c.set("settings.library.account.saveFailed.body", "Nothing was changed. The ScreenScraper settings file could not be written.");
}

}  // namespace library
}  // namespace patsgui
