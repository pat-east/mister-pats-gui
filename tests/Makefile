# Host build for the portable parts of the renderer. Runs on the development machine, so the
# damage tracking, the navigation maths and the game database can be checked without the MiSTer.

CXX      := c++
CXXFLAGS := -std=gnu++14 -O1 -g -Wall -Wextra
BUILDDIR := ../build/tests

DAMAGE   := $(BUILDDIR)/canvas_damage_test
ALPHABET := $(BUILDDIR)/alphabet_test
GAMESDB  := $(BUILDDIR)/gamesdb_test
MISTERINI := $(BUILDDIR)/mister_ini_test
CONTROLLERMAP := $(BUILDDIR)/controller_map_test
ARCHIVE  := $(BUILDDIR)/archive_test
MRAFILE  := $(BUILDDIR)/mra_file_test
ARCADESCAN := $(BUILDDIR)/arcade_scan_test
LIBRETRO := $(BUILDDIR)/libretro_index_test
UPDATECHECK := $(BUILDDIR)/update_check_test
IMAGECACHE := $(BUILDDIR)/image_cache_worker_test
IMAGE_LIBS := $(shell pkg-config --libs libpng libjpeg)
IMAGE_CFLAGS := $(shell pkg-config --cflags libpng libjpeg)

.PHONY: all run clean

all: run

$(DAMAGE): canvas_damage_test.cpp ../src/Canvas.cpp ../src/Canvas.h ../src/Geometry.h ../src/Color.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) canvas_damage_test.cpp ../src/Canvas.cpp -o $@

$(ALPHABET): alphabet_test.cpp ../src/Alphabet.cpp ../src/Alphabet.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) alphabet_test.cpp ../src/Alphabet.cpp -o $@

$(GAMESDB): gamesdb_test.cpp ../src/GameDatabase.cpp ../src/SystemCatalog.cpp ../src/CoreIndex.cpp \
            ../src/Library.cpp ../src/GameIndex.cpp ../src/DebugLog.cpp \
            ../src/GameDatabase.h ../src/SystemCatalog.h ../src/CoreIndex.h ../src/Library.h ../src/Paths.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) gamesdb_test.cpp ../src/GameDatabase.cpp ../src/SystemCatalog.cpp \
	    ../src/CoreIndex.cpp ../src/Library.cpp ../src/GameIndex.cpp ../src/DebugLog.cpp -o $@

$(MISTERINI): mister_ini_test.cpp ../src/MisterIni.cpp ../src/SafeFileWrite.cpp \
              ../src/MisterIni.h ../src/SafeFileWrite.h ../src/Paths.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) mister_ini_test.cpp ../src/MisterIni.cpp ../src/SafeFileWrite.cpp -o $@

$(CONTROLLERMAP): controller_map_test.cpp ../src/ControllerMap.cpp ../src/SafeFileWrite.cpp \
                  ../src/ControllerMap.h ../src/ControllerId.h ../src/SafeFileWrite.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) controller_map_test.cpp ../src/ControllerMap.cpp ../src/SafeFileWrite.cpp -o $@

$(ARCHIVE): archive_test.cpp ../src/Archive.cpp ../src/Archive.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) archive_test.cpp ../src/Archive.cpp -o $@

$(MRAFILE): mra_file_test.cpp ../src/MraFile.cpp ../src/Archive.cpp ../src/MraFile.h ../src/Archive.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) mra_file_test.cpp ../src/MraFile.cpp ../src/Archive.cpp -o $@

$(ARCADESCAN): arcade_scan_test.cpp ../src/LibraryScan.cpp ../src/ArcadeScan.cpp ../src/MraFile.cpp \
               ../src/Archive.cpp ../src/GameDatabase.cpp \
               ../src/SystemCatalog.cpp ../src/CoreIndex.cpp ../src/DebugLog.cpp \
               ../src/LibraryScan.h ../src/GameDatabase.h ../src/SystemCatalog.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) arcade_scan_test.cpp ../src/LibraryScan.cpp ../src/ArcadeScan.cpp \
	    ../src/MraFile.cpp ../src/Archive.cpp ../src/GameDatabase.cpp \
	    ../src/SystemCatalog.cpp ../src/CoreIndex.cpp ../src/DebugLog.cpp -o $@

$(LIBRETRO): libretro_index_test.cpp ../src/LibretroIndex.cpp ../src/Downloader.cpp \
             ../src/LibretroIndex.h ../src/Downloader.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) libretro_index_test.cpp ../src/LibretroIndex.cpp ../src/Downloader.cpp -o $@

$(UPDATECHECK): update_check_test.cpp ../src/UpdateCheck.cpp ../src/Downloader.cpp ../src/DebugLog.cpp \
                ../src/UpdateCheck.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) update_check_test.cpp ../src/UpdateCheck.cpp ../src/Downloader.cpp \
	    ../src/DebugLog.cpp -o $@

$(IMAGECACHE): image_cache_worker_test.cpp ../src/ImageCache.cpp ../src/Image.cpp \
               ../src/ImageCache.h ../src/Image.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(IMAGE_CFLAGS) -I../src image_cache_worker_test.cpp \
	    ../src/ImageCache.cpp ../src/Image.cpp $(IMAGE_LIBS) -o $@

run: $(DAMAGE) $(ALPHABET) $(GAMESDB) $(MISTERINI) $(CONTROLLERMAP) $(ARCHIVE) $(MRAFILE) $(ARCADESCAN) $(LIBRETRO) $(UPDATECHECK) $(IMAGECACHE)
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
	@$(UPDATECHECK)
	@echo
	@$(IMAGECACHE)

clean:
	rm -rf $(BUILDDIR)
