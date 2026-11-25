# Host build for the portable parts of the renderer. Runs on the development machine, so the
# damage tracking, the navigation maths and the game database can be checked without the MiSTer.

CXX      := c++
BUILDDIR := ../build/tests

# src/legacy/Version.h includes a header the ARM build generates from release.mk. The host build
# generates the same one next to the tests, so they never depend on a cross-build having run.
include ../release.mk
CONFIG_HEADER := $(BUILDDIR)/gen/ReleaseConfig.h
CXXFLAGS := -std=gnu++14 -O1 -g -Wall -Wextra -I$(BUILDDIR)/gen

$(CONFIG_HEADER): ../release.mk
	@mkdir -p $(dir $@)
	@printf '#pragma once\nconstexpr const char *kReleaseVersion = "$(VERSION)";\nconstexpr const char *kAppVersion = "$(VERSION)";\nconstexpr const char *kDisplayVersion = "$(VERSION)";\nconstexpr unsigned kGamesDbFormat = $(GAMESDB_FORMAT);\nconstexpr const char *kGamesDbHeader = "#mister-pat gamesdb $(GAMESDB_FORMAT)";\n' > $@

DAMAGE   := $(BUILDDIR)/canvas_damage_test
ALPHABET := $(BUILDDIR)/alphabet_test
GAMESDB  := $(BUILDDIR)/gamesdb_test
MISTERINI := $(BUILDDIR)/mister_ini_test
CONTROLLERMAP := $(BUILDDIR)/controller_map_test
ARCHIVE  := $(BUILDDIR)/archive_test
MRAFILE  := $(BUILDDIR)/mra_file_test
ARCADESCAN := $(BUILDDIR)/arcade_scan_test
LIBRETRO := $(BUILDDIR)/libretro_index_test
RELEASEINFO := $(BUILDDIR)/release_info_test
IMAGECACHE := $(BUILDDIR)/image_cache_worker_test
SSNAMES := $(BUILDDIR)/screenscraper_names_test
SSSTORE := $(BUILDDIR)/screenscraper_store_test
UISOUNDS := $(BUILDDIR)/ui_sounds_test
IMAGE_LIBS := $(shell pkg-config --libs libpng libjpeg)
IMAGE_CFLAGS := $(shell pkg-config --cflags libpng libjpeg)

.DEFAULT_GOAL := all
.PHONY: all run clean

$(DAMAGE) $(ALPHABET) $(GAMESDB) $(MISTERINI) $(CONTROLLERMAP) $(ARCHIVE) $(MRAFILE) $(ARCADESCAN) $(LIBRETRO) $(RELEASEINFO) $(IMAGECACHE): | $(CONFIG_HEADER)

all: run

$(DAMAGE): canvas_damage_test.cpp ../src/legacy/Canvas.cpp ../src/legacy/Canvas.h ../src/legacy/Geometry.h ../src/legacy/Color.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) canvas_damage_test.cpp ../src/legacy/Canvas.cpp -o $@

$(ALPHABET): alphabet_test.cpp ../src/legacy/Alphabet.cpp ../src/legacy/Alphabet.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) alphabet_test.cpp ../src/legacy/Alphabet.cpp -o $@

$(GAMESDB): gamesdb_test.cpp ../src/legacy/GameDatabase.cpp ../src/legacy/SystemCatalog.cpp ../src/legacy/CoreIndex.cpp \
            ../src/legacy/Library.cpp ../src/legacy/GameIndex.cpp ../src/legacy/DebugLog.cpp \
            ../src/legacy/GameDatabase.h ../src/legacy/SystemCatalog.h ../src/legacy/CoreIndex.h ../src/legacy/Library.h ../src/legacy/Paths.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) gamesdb_test.cpp ../src/legacy/GameDatabase.cpp ../src/legacy/SystemCatalog.cpp \
	    ../src/legacy/CoreIndex.cpp ../src/legacy/Library.cpp ../src/legacy/GameIndex.cpp ../src/legacy/DebugLog.cpp -o $@

$(MISTERINI): mister_ini_test.cpp ../src/legacy/MisterIni.cpp ../src/legacy/SafeFileWrite.cpp \
              ../src/legacy/MisterIni.h ../src/legacy/SafeFileWrite.h ../src/legacy/Paths.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) mister_ini_test.cpp ../src/legacy/MisterIni.cpp ../src/legacy/SafeFileWrite.cpp -o $@

$(CONTROLLERMAP): controller_map_test.cpp ../src/legacy/ControllerMap.cpp ../src/legacy/SafeFileWrite.cpp \
                  ../src/legacy/ControllerMap.h ../src/legacy/ControllerId.h ../src/legacy/SafeFileWrite.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) controller_map_test.cpp ../src/legacy/ControllerMap.cpp ../src/legacy/SafeFileWrite.cpp -o $@

$(ARCHIVE): archive_test.cpp ../src/legacy/Archive.cpp ../src/legacy/Archive.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) archive_test.cpp ../src/legacy/Archive.cpp -o $@

$(MRAFILE): mra_file_test.cpp ../src/legacy/MraFile.cpp ../src/legacy/Archive.cpp ../src/legacy/MraFile.h ../src/legacy/Archive.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) mra_file_test.cpp ../src/legacy/MraFile.cpp ../src/legacy/Archive.cpp -o $@

$(ARCADESCAN): arcade_scan_test.cpp ../src/legacy/LibraryScan.cpp ../src/legacy/ArcadeScan.cpp ../src/legacy/MraFile.cpp \
               ../src/legacy/Archive.cpp ../src/legacy/GameDatabase.cpp \
               ../src/legacy/SystemCatalog.cpp ../src/legacy/CoreIndex.cpp ../src/legacy/DebugLog.cpp \
               ../src/legacy/LibraryScan.h ../src/legacy/GameDatabase.h ../src/legacy/SystemCatalog.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) arcade_scan_test.cpp ../src/legacy/LibraryScan.cpp ../src/legacy/ArcadeScan.cpp \
	    ../src/legacy/MraFile.cpp ../src/legacy/Archive.cpp ../src/legacy/GameDatabase.cpp \
	    ../src/legacy/SystemCatalog.cpp ../src/legacy/CoreIndex.cpp ../src/legacy/DebugLog.cpp -o $@

$(LIBRETRO): libretro_index_test.cpp ../src/legacy/LibretroIndex.cpp ../src/legacy/Downloader.cpp \
             ../src/legacy/LibretroIndex.h ../src/legacy/Downloader.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) libretro_index_test.cpp ../src/legacy/LibretroIndex.cpp ../src/legacy/Downloader.cpp -o $@

$(RELEASEINFO): release_info_test.cpp ../src/legacy/ReleaseInfo.cpp ../src/legacy/ReleaseInfo.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) release_info_test.cpp ../src/legacy/ReleaseInfo.cpp -o $@

$(IMAGECACHE): image_cache_worker_test.cpp ../src/legacy/ImageCache.cpp ../src/legacy/Image.cpp \
               ../src/legacy/ArtworkIndex.cpp ../src/legacy/DebugLog.cpp ../src/legacy/SafeFileWrite.cpp \
               ../src/legacy/ImageCache.h ../src/legacy/Image.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(IMAGE_CFLAGS) -I../src/legacy image_cache_worker_test.cpp \
	    ../src/legacy/ImageCache.cpp ../src/legacy/Image.cpp ../src/legacy/ArtworkIndex.cpp \
	    ../src/legacy/DebugLog.cpp ../src/legacy/SafeFileWrite.cpp $(IMAGE_LIBS) -o $@

$(SSNAMES): screenscraper_names_test.cpp ../src/legacy/ScreenScraperNames.cpp ../src/legacy/ScreenScraperNames.h \
            ../src/legacy/MiniJson.cpp ../src/legacy/MiniJson.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) screenscraper_names_test.cpp ../src/legacy/ScreenScraperNames.cpp ../src/legacy/MiniJson.cpp -o $@

$(SSSTORE): screenscraper_store_test.cpp ../src/legacy/ScreenScraperCache.cpp ../src/legacy/ScreenScraperCache.h \
            ../src/legacy/ScreenScraperNames.cpp ../src/legacy/ScreenScraperSettings.cpp ../src/legacy/ScreenScraperSettings.h \
            ../src/legacy/SafeFileWrite.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) screenscraper_store_test.cpp ../src/legacy/ScreenScraperCache.cpp ../src/legacy/ScreenScraperNames.cpp \
		../src/legacy/ScreenScraperSettings.cpp ../src/legacy/SafeFileWrite.cpp -o $@

$(UISOUNDS): ui_sounds_test.cpp ../src/legacy/UiSounds.cpp ../src/legacy/UiSounds.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -pthread -I../src/legacy ui_sounds_test.cpp ../src/legacy/UiSounds.cpp -o $@

run: $(UISOUNDS) $(DAMAGE) $(ALPHABET) $(GAMESDB) $(MISTERINI) $(CONTROLLERMAP) $(ARCHIVE) $(MRAFILE) $(ARCADESCAN) $(LIBRETRO) $(RELEASEINFO) $(IMAGECACHE) $(SSNAMES) $(SSSTORE)
	@$(DAMAGE)
	@echo
	@$(ALPHABET)
	@echo
	@$(GAMESDB)
	@echo
	@$(MISTERINI)
	@echo
	@$(CONTROLLERMAP)
	@echo
	@$(ARCHIVE)
	@echo
	@$(MRAFILE)
	@echo
	@$(ARCADESCAN)
	@echo
	@$(LIBRETRO)
	@echo
	@$(RELEASEINFO)
	@echo
	@$(IMAGECACHE)
	@echo
	@$(SSNAMES)
	@echo
	@$(SSSTORE)
	@$(UISOUNDS)

clean:
	rm -rf $(BUILDDIR)
