# Cross-build for the MiSTer's ARM target. Statically linked, so the binary does not depend
# on the libraries shipped in the MiSTer root filesystem.

CROSS   ?= arm-unknown-linux-gnueabihf
CXX     := $(CROSS)-g++
STRIP   := $(CROSS)-strip

SYSROOT := third_party/sysroot
TARGET  := build/mister-gui

ARCH_FLAGS := -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -mthumb
CXXFLAGS   := -std=gnu++14 -O2 -Wall -Wextra $(ARCH_FLAGS) \
              -I$(SYSROOT)/include -I$(SYSROOT)/include/freetype2
LDFLAGS    := -static -L$(SYSROOT)/lib
LDLIBS     := -lfreetype -lpng16 -ljpeg -lz -lm

SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(patsubst src/%.cpp,build/obj/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

DEVICE ?= root@192.168.64.163
REMOTE ?= /media/fat/mister-pat

.PHONY: all clean deps deploy run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@
	$(STRIP) $@
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

clean:
	rm -rf build

-include $(DEPS)
