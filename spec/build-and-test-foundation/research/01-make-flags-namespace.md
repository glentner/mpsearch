# Research 01: make 3.81, IEEE flags, the logger namespace

Verified 2026-10-11 on this Mac: Apple clang 21.0.0, g++-16 (Homebrew GCC 16.2.0), `/usr/bin/make` 3.81. All builds ran on copies in the session scratchpad, never in the tree. GCC 13–15 aren't installed (`gcc@15` is an alias of 16), so R1's "GCC 13 or later" is verified only at 16.

## 1. GNU make 3.81

This skeleton passed every check below under make 3.81 with both compilers. `tests/` and `bin/mps-test` are placeholder names.

```make
CPPFLAGS := -I src -MMD -MP
CXXFLAGS := -O3 -fno-fast-math -ffp-contract=off -Wall -Wextra -std=c++20
OBJECTS      := $(patsubst src/%.cpp,lib/%.o,$(wildcard src/*.cpp))
LIB_OBJECTS  := $(filter-out lib/main.o,$(OBJECTS))
TEST_OBJECTS := $(patsubst tests/%.cpp,lib/test_%.o,$(wildcard tests/*.cpp))
DEPS         := $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)
.PHONY: all check clean
.DELETE_ON_ERROR:
all: $(EXE)
$(EXE): $(OBJECTS) | bin
	$(CXX) $(CXXFLAGS) $^ -o $@
$(TEST_EXE): $(LIB_OBJECTS) $(TEST_OBJECTS) | bin
	$(CXX) $(CXXFLAGS) $^ -o $@
$(OBJECTS): lib/%.o: src/%.cpp | lib
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(TEST_OBJECTS): lib/test_%.o: tests/%.cpp | lib
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
lib bin:
	mkdir -p $@
check: $(TEST_EXE)
	$(TEST_EXE)
clean:
	rm -f lib/*.o lib/*.d $(EXE) $(TEST_EXE)
-include $(DEPS)
```

- **Fresh clone.** `make` prints `mkdir -p lib`, compiles, then `mkdir -p bin` and links. A rerun prints ``make: Nothing to be done for `all'.`` New files in `lib/` or `bin/` trigger no rebuild. `make -j8 CXX=g++-16 all check` from a fresh copy has no race.
- **Order-only.** `$^` excludes the order-only prerequisite (probe: `^=[x] |=[d]`), so `bin` stays off the link line.
- **`-MMD`.** It writes `lib/X.d` beside `-o` under both compilers, as `lib/cli.o: src/cli.cpp src/main.hpp src/log.hpp …`, with no system headers. After `touch src/ansi.hpp`, only `ansi`, `cli`, `main` and `math` rebuild, then the link.
- **`-MP`.** After a header is deleted, the rebuild works. Without `-MP`: ``No rule to make target `tests/extra.hpp', needed by `lib/test_sieve.o'.  Stop.``
- **Runner.** It links `lib/{ansi,cli,io,math,mp}.o lib/test_sieve.o` and reuses the objects. A failing run gives `make: *** [check] Error 1`, exit 2.
- **`clean`.** It leaves `lib/keep.txt` and `bin/other` in place and exits 0 in a fresh clone. Leftover `.d` files are harmless.
- **For the plan:**
  - HEAD compiles `main.cpp` in the link step; it must become `lib/main.o`.
  - The wildcard adds `src/io.cpp`, which HEAD's `MODULES` leaves out.
  - `check` never builds `bin/mps`. Since R10 makes it the only verify, consider `check: $(EXE) $(TEST_EXE)`.
  - R6 should say whether `.d` files count among "objects".
  - Keep `CPPFLAGS` off the link line.

**3.81 pitfalls (each probed).**
- **Absent, fails loudly:** `undefine`, `.RECIPEPREFIX` (missing separator), `::=`, `private`, `-O`, `--trace`.
- **Absent, fails silently:** `.ONESHELL` (lines still run in separate shells), `$(file …)` (writes nothing), `X != cmd` (X empty), `a b &:` (recipe runs twice), `.SHELLFLAGS` (an ordinary variable).
- **Present:** `$|`, `-include`, static pattern rules, `.DEFAULT_GOAL`, `$(info)`, `else ifeq`, `$(lastword)`, `$(abspath)`, `.SECONDEXPANSION`, `.DELETE_ON_ERROR`.
- **`-include` above `all:`** makes the first `.d` rule the default: `make` printed ``make: `lib/ansi.o' is up to date.`` Keep it last, or set `.DEFAULT_GOAL := all`.
- **Whole-second mtimes.** An input written 0.5 ms after its target gave ``make: `out' is up to date.`` An edit followed by `make` within the same second can skip a rebuild; `make -B` avoids it.
- **Compiler choice.** The built-in default is `CXX = c++`, so `CXX ?= clang++` does nothing. `CXX := clang++` ignores `CXX=g++-16 make` but honours `make CXX=g++-16`. Wrapping it in `ifeq ($(origin CXX),default)` honours both.
- **`CXXFLAGS` override.** `make CXXFLAGS=-O2` drops the IEEE flags (`clang++ -I src -MMD -MP -O2 -c …`). Put them in their own variable after `$(CXXFLAGS)`. `-O3 -ffast-math -fno-fast-math -ffp-contract=off` gave IEEE results on both compilers, with no diagnostics.

## 2. Flags

Probe `fp.cpp` reads its inputs from argv. It computes:
- `r = (x+1.5·2^52) − 1.5·2^52`, with x = 2.75;
- TwoSum `err = (a−(s−bb)) + (b−bb)`, with a = 1e16 and b = 1;
- `f = u·u − c`, with u = 1+2^-30 and c = 1+2^-29. f is nonzero only if the compiler fuses an FMA.

| flags | clang 21 | g++-16 |
|---|---|---|
| `-O3 -fno-fast-math -ffp-contract=off -Wall -Wextra` (either order) | r=3, err=1, f=0, no diagnostics | same |
| `-O3`, or `-O3 -fno-fast-math` | r=3, err=1, **f=8.67e-19** | same |
| `-Ofast` | **r=2.75, err=0**, deprecation warning | **r=2.75, err=0**, no warning |
| `-O3 -ffast-math` | r=2.75, err=0 | r=2.75, err=0 |

- Both compilers contract by default. cc1 gets `-ffp-contract=on`, and `g++-16 -Q --help=optimizers` reports `fast`. `-fno-fast-math` alone doesn't stop FMA fusion, so `-ffp-contract=off` is load-bearing.
- clang's warning, verbatim: `clang++: warning: argument '-Ofast' is deprecated; use '-O3 -ffast-math' for the same behavior, or '-O3' to enable only conforming optimizations [-Wdeprecated-ofast]`.
- **Visibility (R5).** make echoes each recipe line: `clang++ -I src -MMD -MP -O3 -fno-fast-math -ffp-contract=off -Wall -Wextra -std=c++20 -c src/ansi.cpp -o lib/ansi.o`. Keep `@` off the compile and link recipes; `make -s` or `.SILENT` would hide them.

## 3. Namespace

**HEAD under g++-16.** Every unit that includes `log.hpp` warns, with no flags needed: `src/log.hpp:26:11: warning: built-in function 'log' declared as non-function [-Wbuiltin-declaration-mismatch]`.

**g++-16 fails with `<cmath>` in either order:**
- `<cmath>` first: `error: 'namespace log { }' redeclared as different kind of entity`.
- `<cmath>` after: `math.h:397:25: error: 'double log(double)' redeclared as different kind of entity` and `cmath:329:11: error: using-declaration may not name namespace 'log'`.

**clang accepts both orders.** libc++'s `<iostream>`, `<string>`, `<map>`, `<functional>`, `<regex>` and others already declare `::log`; libstdc++'s don't. clang tolerates a namespace declared after a system-header function. It still rejects one after a user-declared function, and an unqualified global `log(x)` is then `reference to 'log' is ambiguous`.

**Renamed to `logging`.** It compiles on both compilers with `<cmath>` before or after, and the builtin warning is gone. R9 holds: across 32 invocations × 2 compilers (`-p`, `-t`, `-D`, `-d`, a bad option, a bad PVAL; `FORCE_COLOR`, `NO_COLOR`, `TASK_ID`), the output is byte-identical to HEAD with timestamps masked.

**Call sites (27 lines, 29 occurrences):**
- `src/main.cpp`: 21, 28 (×2), 31 (×2), 63, 67, 71.
- `src/cli.cpp`: 103, 109, 117, 123, 129, 134, 139, 144, 164, 170, 176, 184, 190, 196, 202, 207, 212, 217.
- `src/math.cpp`: 55, 62, 69.
- Also `src/log.hpp:26`, and `AGENTS.md:186`, whose Code style names `log::info(…)`.

**Collisions.** All project headers built cleanly beside 34 system headers in both orders, on both compilers, after the rename. The set included `cmath`, `cfenv`, `complex`, `random`, `thread`, `err.h`, `syslog.h` and `pthread.h`.
- `-dM` shows no system macro named after any project identifier (DEBUG, INFO, ERROR, OK, RED, BOLD and the rest).
- `<err.h>`'s global `err()` and `warn()` are harmless, because the logger's are namespaced and called qualified.
- Remaining risk: a user's `-DDEBUG`. glibc is unverified (a non-goal).

**Header statics.**
- GCC `-Wunused-variable` fires for unused, trivially constructed, non-const statics: `ansi.hpp:16–19` (used only by `ansi.cpp`) and `io.hpp:32` `FORMAT_EXAMPLE` (a non-const pointer that nothing uses).
- GCC is silent for class-type statics, including the dead `style_by_name` and `level_by_name`, and for statics that inline functions reference.
- clang warns only for statics in the main file, never for ones from headers.

**Fixes, warning-free on both compilers and output-identical to HEAD:**
- `ansi.hpp`: `[[maybe_unused]] static bool …`, or `inline bool …`, or move the four flags into `ansi.cpp`.
- `io.hpp`: `static const char* const FORMAT_EXAMPLE`.
- `math.cpp`: `[[maybe_unused]] const long unsigned p`, or leave the parameter unnamed.
- `io.hpp` and `math.cpp` are high-blast-radius paths.

**R9 trap.**
- `log.hpp`'s `style`, `level`, `hostname` and `task_id` exist once per translation unit, so `-D` restyles only `main.cpp`'s messages. HEAD: `TASK_ID=abc mps -D -t ll 31` prints `    info [mps] Running Lucas-Lehmer test`, while `mps -D abc` prints `… <host> [abc] critical [mps] …`.
- Making them `inline` changes the output to `TS HOST     info [mps] …`. Keep them `static`.
- Separately, libc++ and libstdc++ word the `stoi` error differently (`stoi: no conversion` vs `stoi`), so tests mustn't match on it.

## 4. HEAD warnings, `-Wall -Wextra`

Compiled per file with `-O3 -fno-fast-math -ffp-contract=off -Wall -Wextra -std=c++20 -I src -c`. HEAD's `-Ofast` gives the same set, plus clang's `-Ofast` warning on each of its 5 commands.

clang 21 (3):
```
src/math.cpp:53:44: warning: unused parameter 'p' [-Wunused-parameter]
src/math.cpp:60:43: warning: unused parameter 'p' [-Wunused-parameter]
src/math.cpp:67:44: warning: unused parameter 'p' [-Wunused-parameter]
```

g++-16: 21 warnings, or 20 without `io.cpp`, which HEAD's Makefile doesn't build. `ansi.cpp` and `mp.cpp` are clean.
```
[cli, main, math] src/log.hpp:26:11: warning: built-in function 'log' declared as non-function [-Wbuiltin-declaration-mismatch]
[cli, main, io]   src/io.hpp:32:20: warning: 'FORMAT_EXAMPLE' defined but not used [-Wunused-variable]
[cli, main, math] src/ansi.hpp:19:13: warning: 'STDERR_COLOR' defined but not used [-Wunused-variable]
                  src/ansi.hpp:18:13: warning: 'STDOUT_COLOR' defined but not used [-Wunused-variable]
                  src/ansi.hpp:17:13: warning: 'FORCE_COLOR' defined but not used [-Wunused-variable]
                  src/ansi.hpp:16:13: warning: 'NO_COLOR' defined but not used [-Wunused-variable]
[math]            src/math.cpp:53:44, 60:43, 67:44: warning: unused parameter 'p' [-Wunused-parameter]
```

**Fresh clone at HEAD:**
- clang: `error: unable to open output file 'lib/ansi.o': 'No such file or directory'`.
- g++-16: the same message from `clang -cc1as`, because GCC on macOS assembles with the system `as`.
