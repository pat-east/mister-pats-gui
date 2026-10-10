# Cross-build for the MiSTer ARM target. The small loader stays at the configured gui= path;
# the C++ GUI is a versioned shared library; an optional relative symlink pins a version.
include release.mk

CROSS   ?= arm-unknown-linux-gnueabihf
CC      := $(CROSS)-gcc
CXX     := $(CROSS)-g++
STRIP   := $(CROSS)-strip
OBJCOPY := $(CROSS)-objcopy

SYSROOT := third_party/sysroot
DEV_BUILD ?= 0
ifeq ($(DEV_BUILD),1)
FLAVOR := dev
OUT := build/dev
DISPLAY_VERSION := $(VERSION)-dev
else
FLAVOR := release
OUT := build
DISPLAY_VERSION := $(VERSION)
endif

CONFIG_DIR := build/$(FLAVOR)
CONFIG_HEADER := $(CONFIG_DIR)/ReleaseConfig.h
LOADER := $(OUT)/mister-gui
LIBRARY := $(OUT)/mister-pats-gui-$(VERSION).so
LOADER_OBJ := build/obj/$(FLAVOR)/Loader.o
APP_SOURCES := $(wildcard src/*.cpp)
APP_OBJECTS := $(patsubst src/%.cpp,build/obj/$(FLAVOR)/%.o,$(APP_SOURCES))
DEPS := $(APP_OBJECTS:.o=.d) $(LOADER_OBJ:.o=.d)
BUILD_CONFIG := $(CONFIG_DIR)/.build-config
ICON_BENCH := build/icon-bench

ARCH_FLAGS := -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb
COMMON_FLAGS := -O2 -g -fno-omit-frame-pointer -Wall -Wextra $(ARCH_FLAGS)
CXXFLAGS := -std=gnu++14 $(COMMON_FLAGS) -fPIC -fvisibility=hidden -pthread \
            -I$(CONFIG_DIR) -I$(SYSROOT)/include -I$(SYSROOT)/include/freetype2
CFLAGS := -std=c11 $(COMMON_FLAGS) -Isrc
LIBRARY_LDFLAGS := -shared -static-libstdc++ -static-libgcc -Wl,--build-id=sha1 \
                   -Wl,--no-undefined -L$(SYSROOT)/lib
LIBRARY_LIBS := -lfreetype -lpng16 -ljpeg -lz -lm -pthread
LOADER_LDFLAGS := -Wl,--build-id=sha1 -ldl

DEVICE ?= root@192.168.64.163
REMOTE ?= /media/fat/mister-pat

.PHONY: all clean deps deploy run bench-icons package-release FORCE

all: $(LOADER) $(LIBRARY)

FORCE:

$(CONFIG_HEADER): release.mk Makefile
	@mkdir -p $(dir $@)
	@printf '#pragma once\nconstexpr const char *kReleaseVersion = "$(VERSION)";\nconstexpr const char *kAppVersion = "$(DISPLAY_VERSION)";\nconstexpr unsigned kGamesDbFormat = $(GAMESDB_FORMAT);\nconstexpr const char *kGamesDbHeader = "#mister-pat gamesdb $(GAMESDB_FORMAT)";\n' > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

$(BUILD_CONFIG): FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' '$(CXXFLAGS)' '$(CFLAGS)' '$(LIBRARY_LDFLAGS)' '$(LIBRARY_LIBS)' > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

$(APP_OBJECTS): $(CONFIG_HEADER) $(BUILD_CONFIG)
$(LOADER_OBJ): $(BUILD_CONFIG)

build/obj/$(FLAVOR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(LOADER_OBJ): src/Loader.c src/Paths.h
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

deploy:
	$(MAKE) DEV_BUILD=1 all
	DEVICE=$(DEVICE) REMOTE=$(REMOTE) tools/deploy.sh

run: deploy
	ssh $(DEVICE) $(REMOTE)/mister-gui

package-release: all
	tools/package-release.sh

$(ICON_BENCH): tools/icon_bench.cpp src/Image.cpp src/Canvas.cpp src/Image.h src/Canvas.h Makefile
	@mkdir -p $(dir $@)
	$(CXX) -std=gnu++14 $(COMMON_FLAGS) -pthread -I$(SYSROOT)/include \
		-Isrc tools/icon_bench.cpp src/Image.cpp src/Canvas.cpp \
		-static -L$(SYSROOT)/lib -lpng16 -ljpeg -lz -lm -pthread -o $@

bench-icons: $(ICON_BENCH)

clean:
	rm -rf build

-include $(DEPS)
