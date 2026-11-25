#pragma once

// The pages of the Library category: the scan and box-art flow (Classic ScanScreen), the Core and MRA updater
// (SettingsScreen::handleCoreUpdater), the ScreenScraper login keyboard. Each opener pushes the first page of a flow
// on the stack; the flow replaces its own pages as Classic's page enum changes. Pure UI over LibraryService.

#include <string>
#include <vector>

#include "app/settings/LibraryService.h"
#include "app/settings/SettingsPage.h"

namespace patsgui {
namespace library {

void openScanIntro(LibraryService &svc, PageStack &pages);            // A on "Build game list"   (App::openScan(false))
void openArtworkOffer(LibraryService &svc, PageStack &pages);         // A on "Prepare box art"   (App::openArtwork())
void openCoreUpdater(LibraryService &svc, PageStack &pages);          // A on "Core and MRA updates" (openCoreUpdater())
void openScrapedVideos(LibraryService &svc, PageStack &pages);       // A on "Scraped videos" (ScreenScraper.fr)
void openAccountEntry(LibraryService &svc, PageStack &pages, bool user);   // A on "User name" / "Password"

// The pages the host runner can open directly (`--page <id>`), and the code path that opens them.
std::vector<std::string> pageIds();
bool openPage(LibraryService &svc, PageStack &pages, const std::string &id);

// Registers every page text (settings.library.*).
void registerPageStrings(ui::Catalog &c);
void registerScanStrings(ui::Catalog &c);
void registerArtworkStrings(ui::Catalog &c);
void registerCoreStrings(ui::Catalog &c);
void registerAccountStrings(ui::Catalog &c);
void registerVideoStrings(ui::Catalog &c);

}  // namespace library
}  // namespace patsgui
