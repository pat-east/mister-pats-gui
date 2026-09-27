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

run: $(DAMAGE) $(ALPHABET) $(GAMESDB) $(MISTERINI) $(CONTROLLERMAP)
	@$(DAMAGE)
	@echo
	@$(ALPHABET)
	@echo
	@$(GAMESDB)
	@echo
	@$(MISTERINI)
	@echo
	@$(CONTROLLERMAP)

clean:
	rm -rf $(BUILDDIR)
