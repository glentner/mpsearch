# SPDX-FileCopyrightText: 2024 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception

ifeq ($(origin CXX),default)
CXX      := clang++
endif
CXXFLAGS := -O3 -Wall -Wextra
IEEE     := -std=c++20 -fno-fast-math -ffp-contract=off
CPPFLAGS := -I src -MMD -MP

EXE      := bin/mps
CHECK    := bin/check
MODULES  := ansi cli io mp math
OBJECTS  := $(addprefix lib/,$(addsuffix .o,$(MODULES)))
CHECKS   := $(patsubst tests/%.cpp,lib/%.o,$(sort $(wildcard tests/*.cpp)))
DEPS     := $(OBJECTS:.o=.d) lib/main.d $(CHECKS:.o=.d)

$(EXE): lib/main.o $(OBJECTS) | bin
	$(CXX) $(CXXFLAGS) $(IEEE) $(^) -o $(@)

# The tests directory is a prerequisite so that adding or removing a check file relinks.
$(CHECK): $(CHECKS) $(OBJECTS) tests | bin
	$(CXX) $(CXXFLAGS) $(IEEE) $(CHECKS) $(OBJECTS) -o $(@)

lib/%.o: src/%.cpp Makefile | lib
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(IEEE) -c $(<) -o $(@)

lib/%.o: tests/%.cpp Makefile | lib
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror=unused-function $(IEEE) -c $(<) -o $(@)

lib bin:
	mkdir -p $(@)

check: $(EXE) $(CHECK)
	@status=0; $(CHECK) || status=1; sh tests/cli.sh $(EXE) || status=1; exit $$status

clean:
	/bin/rm -f lib/*.o lib/*.d $(EXE) $(CHECK)

.PHONY: check clean
.DELETE_ON_ERROR:

-include $(DEPS)
