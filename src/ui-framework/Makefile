# Pat's UI Framework: host build of the library and the UI Wireframes demo.
#
#   make -C src/ui-framework            builds build/ui-demo
#   make -C src/ui-framework run        renders the splash and every tab to build/out/*.png
#   make -C src/ui-framework clean
#
# The framework includes nothing from ../app or ../legacy; the include root is src/ (..) so that
# headers are named "ui-framework/core/Color.h" and so on.

CXX      ?= c++
BUILD    := build
CXXFLAGS := -std=gnu++14 -O2 -g -Wall -Wextra -I.. $(shell pkg-config --cflags freetype2 libpng)
LIBS     := $(shell pkg-config --libs freetype2 libpng) -lm

SRCS := $(wildcard core/*.cpp) $(wildcard render/soft/*.cpp) $(wildcard kit/*.cpp) $(wildcard platform/host/*.cpp) $(wildcard demo/*.cpp)
OBJS := $(SRCS:%.cpp=$(BUILD)/%.o) $(BUILD)/gen/IconData.o
DEPS := $(OBJS:.o=.d)
DEMO := $(BUILD)/ui-demo

.PHONY: all run clean
all: $(DEMO)

$(DEMO): $(OBJS)
	$(CXX) $(OBJS) $(LIBS) -o $@

# The embedded icon set (Bootstrap Icons) is generated from the SVG files at build time.
$(BUILD)/gen/IconData.cpp: $(wildcard assets/bootstrap-icons/*.svg) tools/embed-icons.py
	@mkdir -p $(dir $@)
	python3 tools/embed-icons.py assets/bootstrap-icons $@

$(BUILD)/gen/IconData.o: $(BUILD)/gen/IconData.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(DEMO)
	$(DEMO) --out $(BUILD)/out

clean:
	rm -rf $(BUILD)

-include $(DEPS)
