# Host build of the new GUI (src/app/) on the UI framework with the SampleLibrary. No device, no src/legacy/.
#
#   make -C src/app            builds build/app-render
#   make -C src/app run        renders the splash-less shell with every tab to build/out/app-*.png
#   make -C src/app test       builds and runs the flow tests (tests/README.md): TARGS="scan wifi" runs the cases whose name contains a word
#   make -C src/app clean
#
# Extra arguments: make -C src/app run ARGS="--tab disc --disc nodisc"

CXX      ?= c++
BUILD    := build
CXXFLAGS := -std=gnu++14 -O2 -g -Wall -Wextra -I.. $(shell pkg-config --cflags freetype2 libpng)
LIBS     := $(shell pkg-config --libs freetype2 libpng) -lm

UI   := ../ui-framework
SRCS := $(wildcard $(UI)/core/*.cpp) $(wildcard $(UI)/render/soft/*.cpp) $(wildcard $(UI)/kit/*.cpp) \
        $(wildcard $(UI)/platform/host/*.cpp) $(wildcard *.cpp) $(wildcard settings/*.cpp) $(wildcard settings/*/*.cpp) $(wildcard host/*.cpp)
OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(patsubst ../%,%,$(SRCS))) $(BUILD)/gen/IconData.o
DEPS := $(OBJS:.o=.d)
BIN  := $(BUILD)/app-render
ARGS ?=

.PHONY: all run clean test golden-check golden-update
all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(OBJS) $(LIBS) -o $@

$(BUILD)/gen/IconData.cpp: $(wildcard $(UI)/assets/bootstrap-icons/*.svg) $(UI)/tools/embed-icons.py
	@mkdir -p $(dir $@)
	python3 $(UI)/tools/embed-icons.py $(UI)/assets/bootstrap-icons $@

$(BUILD)/gen/IconData.o: $(BUILD)/gen/IconData.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/ui-framework/%.o: ../ui-framework/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(BIN)
	$(BIN) --out $(BUILD)/out $(ARGS)

clean:
	rm -rf $(BUILD)

# The flow tests: the app objects without the host runner's main(), plus tests/*.cpp (behaviour, no pixels).
TEST_OBJS := $(filter-out $(BUILD)/host/main.o,$(OBJS)) $(patsubst %.cpp,$(BUILD)/%.o,$(wildcard tests/*.cpp))
TEST_BIN  := $(BUILD)/app-test
TARGS     ?=
$(TEST_BIN): $(TEST_OBJS)
	$(CXX) $(TEST_OBJS) $(LIBS) -o $@
test: $(TEST_BIN)
	$(TEST_BIN) $(TARGS)

# Golden images and layout rules (tests/golden.sh, tests/README.md): GARGS="--update" accepts changes, "--determinism" renders twice.
GARGS ?=
golden-check: $(BIN)
	tests/golden.sh $(GARGS)
golden-update: $(BIN)
	tests/golden.sh --update

-include $(DEPS) $(TEST_OBJS:.o=.d)
