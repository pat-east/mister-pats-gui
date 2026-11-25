# Cross-build for the MiSTer ARM target. The small loader stays at the configured gui= path;
# the C++ GUI is a versioned shared library; an optional relative symlink pins a version.
include release.mk

CROSS   ?= arm-unknown-linux-gnueabihf
CC      := $(CROSS)-gcc
CXX     := $(CROSS)-g++
STRIP   := $(CROSS)-strip
OBJCOPY := $(CROSS)-objcopy

SYSROOT := third_party/sysroot
FLAVOR := release
OUT := build
# kAppVersion (= VERSION) must stay plain X.Y.Z: updates compare it and the loader reads library file names. A
# development build may show another text (make DISPLAY_VERSION=0.9.0-dev) through kDisplayVersion only.
DISPLAY_VERSION := $(VERSION)

CONFIG_DIR := build/$(FLAVOR)
CONFIG_HEADER := $(CONFIG_DIR)/ReleaseConfig.h
LOADER := $(OUT)/mister-gui
LIBRARY := $(OUT)/mister-pats-gui-$(VERSION).so
LOADER_OBJ := build/obj/$(FLAVOR)/legacy/Loader.o
# Source layout (src/README.md): src/legacy (the Classic GUI and its device code, with src/legacy/bridge
# connecting the new GUI to it), src/ui-framework, src/app. Object files mirror the source tree below
# build/obj/$(FLAVOR)/.
APP_SOURCES := $(wildcard src/legacy/*.cpp src/legacy/bridge/*.cpp src/legacy/bridge/settings/*.cpp)
APP_OBJECTS := $(patsubst src/%.cpp,build/obj/$(FLAVOR)/%.o,$(APP_SOURCES))
# The UI framework (src/ui-framework) and the new GUI (src/app) are compiled into the library; the
# framework's host runner and PNG presenter (demo/main.cpp, platform/host) and src/app/host stay out.
UI_SOURCES := $(wildcard src/ui-framework/core/*.cpp src/ui-framework/render/soft/*.cpp src/ui-framework/kit/*.cpp src/app/*.cpp src/app/settings/*.cpp src/app/settings/*/*.cpp) $(filter-out src/ui-framework/demo/main.cpp,$(wildcard src/ui-framework/demo/*.cpp))
UI_OBJECTS := $(patsubst src/%.cpp,build/obj/$(FLAVOR)/%.o,$(UI_SOURCES))
APP_OBJECTS += $(UI_OBJECTS)
ICON_DATA_CPP := build/$(FLAVOR)/IconData.cpp
ICON_DATA_OBJ := build/obj/$(FLAVOR)/IconData.o
APP_OBJECTS += $(ICON_DATA_OBJ)
EMBED_INDEX_CPP := build/$(FLAVOR)/DiscIndexData.cpp
EMBED_INDEX_OBJ := build/obj/$(FLAVOR)/DiscIndexData.o
APP_OBJECTS += $(EMBED_INDEX_OBJ)
DEV_LOGIN_ENV := $(firstword $(wildcard _build/screenscraper.env .screenscraper.fr.env) _build/screenscraper.env)
DEV_LOGIN_CPP := build/$(FLAVOR)/ScreenScraperDevLogin.cpp
DEV_LOGIN_OBJ := build/obj/$(FLAVOR)/ScreenScraperDevLogin.o
APP_OBJECTS += $(DEV_LOGIN_OBJ)
DEPS := $(APP_OBJECTS:.o=.d) $(LOADER_OBJ:.o=.d)
BUILD_CONFIG := $(CONFIG_DIR)/.build-config
ICON_BENCH := build/icon-bench

ARCH_FLAGS := -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb
COMMON_FLAGS := -O2 -g -fno-omit-frame-pointer -Wall -Wextra $(ARCH_FLAGS)
CXXFLAGS := -std=gnu++14 $(COMMON_FLAGS) -fPIC -fvisibility=hidden -pthread \
            -I$(CONFIG_DIR) -Isrc -Isrc/legacy -I$(SYSROOT)/include -I$(SYSROOT)/include/freetype2
CFLAGS := -std=c11 $(COMMON_FLAGS) -Isrc/legacy
LIBRARY_LDFLAGS := -shared -static-libstdc++ -static-libgcc -Wl,--build-id=sha1 \
                   -Wl,--no-undefined -L$(SYSROOT)/lib
LIBRARY_LIBS := -lfreetype -lpng16 -ljpeg -lavformat -lavcodec -lswscale -lswresample -lavutil -lz -lm -pthread
LOADER_LDFLAGS := -Wl,--build-id=sha1 -ldl

DEVICE ?= root@192.168.64.163
REMOTE ?= /media/fat/mister-pat

.PHONY: all clean deps deploy run bench-icons poc-video package-release FORCE

all: $(LOADER) $(LIBRARY)

FORCE:

$(CONFIG_HEADER): release.mk Makefile
	@mkdir -p $(dir $@)
	@printf '#pragma once\nconstexpr const char *kReleaseVersion = "$(VERSION)";\nconstexpr const char *kAppVersion = "$(VERSION)";\nconstexpr const char *kDisplayVersion = "$(DISPLAY_VERSION)";\nconstexpr unsigned kGamesDbFormat = $(GAMESDB_FORMAT);\nconstexpr const char *kGamesDbHeader = "#mister-pat gamesdb $(GAMESDB_FORMAT)";\n' > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

$(BUILD_CONFIG): FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' '$(CXXFLAGS)' '$(CFLAGS)' '$(LIBRARY_LDFLAGS)' '$(LIBRARY_LIBS)' > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

$(APP_OBJECTS): $(CONFIG_HEADER) $(BUILD_CONFIG)
$(LOADER_OBJ): $(BUILD_CONFIG)

$(EMBED_INDEX_CPP): assets/disc-index.tsv tools/embed-disc-index.py
	@mkdir -p $(dir $@)
	python3 tools/embed-disc-index.py assets/disc-index.tsv $@

# The UI framework's icon set: Bootstrap Icons as SVG, embedded at build time (src/ui-framework/tools/embed-icons.py).
$(ICON_DATA_CPP): $(wildcard src/ui-framework/assets/bootstrap-icons/*.svg) src/ui-framework/tools/embed-icons.py
	@mkdir -p $(dir $@)
	python3 src/ui-framework/tools/embed-icons.py src/ui-framework/assets/bootstrap-icons $@

$(ICON_DATA_OBJ): $(ICON_DATA_CPP) $(CONFIG_HEADER) $(BUILD_CONFIG)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $(ICON_DATA_CPP) -o $@

$(EMBED_INDEX_OBJ): $(EMBED_INDEX_CPP) src/legacy/DiscIndexData.h $(CONFIG_HEADER) $(BUILD_CONFIG)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $(EMBED_INDEX_CPP) -o $@

# The developer login comes from the git-ignored $(DEV_LOGIN_ENV), scrambled; without it the
# build has no login. FORCE-free on purpose: the file is rebuilt when the env file changes.
$(DEV_LOGIN_CPP): $(wildcard $(DEV_LOGIN_ENV)) tools/embed-dev-login.py
	@mkdir -p $(dir $@)
	python3 tools/embed-dev-login.py $(DEV_LOGIN_ENV) $@

$(DEV_LOGIN_OBJ): $(DEV_LOGIN_CPP) src/legacy/ScreenScraperDevLogin.h $(CONFIG_HEADER) $(BUILD_CONFIG)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $(DEV_LOGIN_CPP) -o $@

build/obj/$(FLAVOR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(LOADER_OBJ): src/legacy/Loader.c src/legacy/Paths.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(LOADER): $(LOADER_OBJ) Makefile
	@mkdir -p $(dir $@)
	$(CC) $(LOADER_OBJ) $(ARCH_FLAGS) $(LOADER_LDFLAGS) -o $@.full
	$(OBJCOPY) --only-keep-debug $@.full $@.debug
	cp $@.full $@
	$(STRIP) --strip-debug --strip-unneeded $@
	$(OBJCOPY) --add-gnu-debuglink=$@.debug $@
	rm -f $@.full

$(LIBRARY): $(APP_OBJECTS) $(CONFIG_HEADER) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(APP_OBJECTS) $(ARCH_FLAGS) $(LIBRARY_LDFLAGS) $(LIBRARY_LIBS) -o $@.full
	$(OBJCOPY) --only-keep-debug $@.full $@.debug
	cp $@.full $@
	$(STRIP) --strip-debug --strip-unneeded $@
	$(OBJCOPY) --add-gnu-debuglink=$@.debug $@
	rm -f $@.full

deps:
	third_party/build.sh

deploy: all
	DEVICE=$(DEVICE) REMOTE=$(REMOTE) tools/deploy.sh

run: deploy
	ssh $(DEVICE) $(REMOTE)/mister-gui

package-release: all
	tools/package-release.sh

$(ICON_BENCH): tools/icon_bench.cpp src/legacy/Image.cpp src/legacy/Canvas.cpp src/legacy/Image.h src/legacy/Canvas.h Makefile
	@mkdir -p $(dir $@)
	$(CXX) -std=gnu++14 $(COMMON_FLAGS) -pthread -I$(SYSROOT)/include \
		-Isrc/legacy tools/icon_bench.cpp src/legacy/Image.cpp src/legacy/Canvas.cpp \
		-static -L$(SYSROOT)/lib -lpng16 -ljpeg -lz -lm -pthread -o $@

bench-icons: $(ICON_BENCH)

# Video experiments (see _vnext/SCREENSCRAPER-POC.md): standalone, statically linked tools that
# need the FFmpeg libraries from `third_party/build-ffmpeg.sh`. Not part of the GUI or its release.
VIDEO_POC := build/poc/videobench build/poc/videoplay
FFMPEG_LIBS := -lavformat -lavcodec -lswscale -lswresample -lavutil

build/poc/%: poc/%.cpp poc/videodec.h Makefile
	@mkdir -p $(dir $@)
	$(CXX) -std=gnu++14 $(COMMON_FLAGS) -pthread -I$(SYSROOT)/include poc/$*.cpp \
		-static -L$(SYSROOT)/lib $(FFMPEG_LIBS) -lm -pthread -o $@
	$(STRIP) --strip-debug $@

poc-video: $(VIDEO_POC)

clean:
	rm -rf build

-include $(DEPS)
