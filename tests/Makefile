# Host build for the portable parts of the renderer. Runs on the development machine, so the
# damage tracking, the navigation maths and the game database can be checked without the MiSTer.

CXX      := c++
CXXFLAGS := -std=gnu++14 -O1 -g -Wall -Wextra
BUILDDIR := ../build/tests

DAMAGE   := $(BUILDDIR)/canvas_damage_test
ALPHABET := $(BUILDDIR)/alphabet_test
GAMESDB  := $(BUILDDIR)/gamesdb_test

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

run: $(DAMAGE) $(ALPHABET) $(GAMESDB)
	@$(DAMAGE)
	@echo
	@$(ALPHABET)
	@echo
	@$(GAMESDB)

clean:
	rm -rf $(BUILDDIR)
