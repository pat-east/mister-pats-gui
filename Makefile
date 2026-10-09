# Cross-build for the MiSTer's ARM target. Statically linked, so the binary does not depend
# on the libraries shipped in the MiSTer root filesystem.

CROSS   ?= arm-unknown-linux-gnueabihf
CXX     := $(CROSS)-g++
STRIP   := $(CROSS)-strip
OBJCOPY := $(CROSS)-objcopy

SYSROOT := third_party/sysroot
TARGET  := build/mister-gui
DEBUG_TARGET := $(TARGET).debug
ICON_BENCH := build/icon-bench

ARCH_FLAGS := -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb
CXXFLAGS   := -std=gnu++14 -O2 -g -fno-omit-frame-pointer -Wall -Wextra $(ARCH_FLAGS) \
              -pthread -I$(SYSROOT)/include -I$(SYSROOT)/include/freetype2
# libstdc++'s static-init guard references these pthread functions weakly. With a static
# libpthread, weak references alone do not pull their archive members into the binary. A
# missing broadcast becomes a no-op and the guard throws __concurrence_broadcast_error.
LDFLAGS    := -static -Wl,--build-id=sha1 \
              -Wl,--require-defined=pthread_cond_broadcast \
              -Wl,--require-defined=pthread_cond_wait -L$(SYSROOT)/lib
LDLIBS     := -lfreetype -lpng16 -ljpeg -lz -lm -pthread

SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(patsubst src/%.cpp,build/obj/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)
BUILD_CONFIG := build/.build-config

DEVICE ?= root@192.168.64.163
REMOTE ?= /media/fat/mister-pat

.PHONY: all clean deps deploy run bench-icons FORCE

all: $(TARGET)

FORCE:

# Make does not normally notice compiler-flag changes. Track them so existing objects from a
# stripped/no-debug build are rebuilt the first time this debug build is used.
$(BUILD_CONFIG): FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' '$(CXXFLAGS)' '$(LDFLAGS)' '$(LDLIBS)' > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

$(OBJECTS): $(BUILD_CONFIG)

$(TARGET): $(OBJECTS) $(BUILD_CONFIG) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@.full
	$(OBJCOPY) --only-keep-debug $@.full $(DEBUG_TARGET)
	cp $@.full $@
	$(STRIP) --strip-debug --strip-unneeded $@
	$(OBJCOPY) --add-gnu-debuglink=$(DEBUG_TARGET) $@
	rm -f $@.full
	@ls -lh $@ | awk '{print "built " $$9 " (" $$5 ")"}'

build/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

deps:
	third_party/build.sh

deploy: $(TARGET)
	scp -q $(TARGET) $(DEVICE):$(REMOTE)/

run: deploy
	ssh $(DEVICE) $(REMOTE)/mister-gui

$(ICON_BENCH): tools/icon_bench.cpp src/Image.cpp src/Canvas.cpp src/Image.h src/Canvas.h Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc tools/icon_bench.cpp src/Image.cpp src/Canvas.cpp \
		$(LDFLAGS) -lpng16 -ljpeg -lz -lm -pthread -o $@

bench-icons: $(ICON_BENCH)

clean:
	rm -rf build

-include $(DEPS)
