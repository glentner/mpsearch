# SPDX-FileCopyrightText: 2024 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception

ifeq ($(origin CXX),default)
CXX      := clang++
endif
CXXFLAGS := -O3 -Wall -Wextra
IEEE     := -std=c++20 -fno-fast-math -ffp-contract=off
CPPFLAGS := -I src -MMD -MP

# The checks build with the standard libraries' bounds assertions (libstdc++ and libc++), so they
# see a write past the end of a container that no value check can. bin/mps builds without them.
BOUNDS   := -D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE

EXE      := bin/mps
CHECK    := bin/check
MODULES  := ansi cli io known mp math reference result
OBJECTS  := $(addprefix lib/,$(addsuffix .o,$(MODULES)))
CHECKED  := $(addprefix lib/check/,$(addsuffix .o,$(MODULES)))
CHECKS   := $(patsubst tests/%.cpp,lib/check/%.o,$(sort $(wildcard tests/*.cpp)))
DEPS     := $(OBJECTS:.o=.d) lib/main.d $(CHECKED:.o=.d) $(CHECKS:.o=.d)

$(EXE): lib/main.o $(OBJECTS) | bin
	$(CXX) $(CXXFLAGS) $(IEEE) $(^) -o $(@)

# The tests directory is a prerequisite so that adding or removing a check file relinks.
$(CHECK): $(CHECKS) $(CHECKED) tests | bin
	$(CXX) $(CXXFLAGS) $(IEEE) $(CHECKS) $(CHECKED) -o $(@)

# The checks' own objects come first: make 3.81 takes the first pattern that applies, and lib/%.o
# would match lib/check/*.o too if it had a source to build them from.
lib/check/%.o: src/%.cpp Makefile | lib/check
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BOUNDS) $(IEEE) -c $(<) -o $(@)

lib/check/%.o: tests/%.cpp Makefile | lib/check
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BOUNDS) -Werror=unused-function $(IEEE) -c $(<) -o $(@)

lib/%.o: src/%.cpp Makefile | lib
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(IEEE) -c $(<) -o $(@)

lib bin lib/check:
	mkdir -p $(@)

check: $(EXE) $(CHECK)
	@status=0; $(CHECK) || status=1; sh tests/cli.sh $(EXE) || status=1; exit $$status

clean:
	/bin/rm -f lib/*.o lib/*.d lib/check/*.o lib/check/*.d $(EXE) $(CHECK)

.PHONY: check clean
.DELETE_ON_ERROR:

-include $(DEPS)
