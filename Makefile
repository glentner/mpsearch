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

# Everything the build writes: what make clean removes, and what a change of settings removes.
BUILT    := lib/*.o lib/*.d lib/check/*.o lib/check/*.d $(EXE) $(CHECK) lib/settings

# The compile settings are everything that picks the compiler or its flags, and lib/settings records
# those that built what is in lib/ and bin/. When this run's differ from the record, or there is no
# record, CHANGED names a phony prerequisite of every object and the binaries are phony, so all of
# them are out of date whatever their timestamps; and the record's recipe, which runs before the
# first compile, removes everything the last build made, so nothing another compiler or other flags
# compiled is linked. The settings are compared, not timed: make keeps whole-second timestamps, so a
# stamp file can't tell a switch from a build in the same second. Only that recipe removes anything:
# a run that builds nothing (make -n, make -q, a goal with no rule here) leaves every file as it
# was, and -n and -q still report the rebuild. An unchanged run changes nothing.
SETTINGS := $(CXX) | $(CPPFLAGS) | $(CXXFLAGS) | $(IEEE) | $(BOUNDS)
QUOTED   := '$(subst ','\'',$(SETTINGS))'
CHANGED  := $(shell test "`cat lib/settings 2>/dev/null`" = $(QUOTED) || echo FORCE)

# The link of bin/mps names its prerequisites, so the binaries can't take CHANGED as one.
ifneq ($(CHANGED),)
.PHONY: $(EXE) $(CHECK) lib/settings
endif

$(EXE): lib/main.o $(OBJECTS) | bin
	$(CXX) $(CXXFLAGS) $(IEEE) $(^) -o $(@)

# The tests directory is a prerequisite so that adding or removing a check file relinks.
$(CHECK): $(CHECKS) $(CHECKED) tests | bin
	$(CXX) $(CXXFLAGS) $(IEEE) $(CHECKS) $(CHECKED) -o $(@)

# The checks' own objects come first: make 3.81 takes the first pattern that applies, and lib/%.o
# would match lib/check/*.o too if it had a source to build them from.
lib/check/%.o: src/%.cpp Makefile $(CHANGED) | lib/check lib/settings
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BOUNDS) $(IEEE) -c $(<) -o $(@)

lib/check/%.o: tests/%.cpp Makefile $(CHANGED) | lib/check lib/settings
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BOUNDS) -Werror=unused-function $(IEEE) -c $(<) -o $(@)

lib/%.o: src/%.cpp Makefile $(CHANGED) | lib lib/settings
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(IEEE) -c $(<) -o $(@)

lib bin lib/check:
	mkdir -p $(@)

lib/settings: | lib
	@/bin/rm -f $(BUILT) && printf '%s\n' $(QUOTED) >$(@)

check: $(EXE) $(CHECK)
	@status=0; $(CHECK) || status=1; sh tests/cli.sh $(EXE) || status=1; exit $$status

clean:
	/bin/rm -f $(BUILT)

FORCE:

.PHONY: check clean FORCE
.DELETE_ON_ERROR:

-include $(DEPS)
