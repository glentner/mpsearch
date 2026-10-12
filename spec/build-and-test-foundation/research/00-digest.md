# Research digest — build-and-test-foundation

Two briefs: [01 — make, flags, namespace](01-make-flags-namespace.md) and
[02 — test runner, license](02-test-runner-license.md). Both prototyped on this Mac (Apple clang 21,
Homebrew GCC 16, GNU make 3.81). Decisions, with each cross-brief difference resolved once:

1. **Flags.**
   - `-O3 -fno-fast-math -ffp-contract=off` is the IEEE set. It rounds 2.75 → 3 and keeps the TwoSum
     error term on both compilers. `-Ofast` and `-ffast-math` break both.
   - Both compilers fuse multiply-adds by default, so `-ffp-contract=off` is load-bearing.
   - The set goes in its own variable, appended after `$(CXXFLAGS)`, so a command-line
     `CXXFLAGS=…` can't drop it (01 §1).
2. **Compiler.** `ifeq ($(origin CXX),default)` selects `clang++`, and both `CXX=g++-16 make` and
   `make CXX=g++-16` work (01 §1).
3. **Directories and dependencies.** Output directories are order-only prerequisites. `-MMD -MP`
   gives precise header dependencies, and `-include $(DEPS)` goes last, after the default target.
   *Resolved:* brief 02's coarse `$(HEADERS) Makefile` prerequisites are dropped in favour of `-MMD
   -MP`. `Makefile` is kept as a prerequisite of every object, so a flag change rebuilds everything.
4. **Modules.** `io` is added to `MODULES`; HEAD silently left it out. `main.cpp` compiles to
   `lib/main.o` (01 §1).
5. **Namespace.** `log` becomes `logging`: 27 lines and 29 occurrences, byte-identical output across
   32 invocations on each compiler. Logger state stays `static`; `inline` would change output, which
   R9 forbids (01 §3).
6. **Warnings.** Fix them at their sites (01 §4):
   - the colour flags move into `ansi.cpp`;
   - `FORMAT_EXAMPLE` becomes `static const char* const`;
   - the stub parameters get `[[maybe_unused]]`.
7. **Runner: names.** The unit is a *check*, because "test" means a primality test in this code. The
   files are `tests/check.hpp`, `tests/check.cpp` and `tests/check_<module>.cpp`, built to `bin/check`
   (02 §A).
8. **Runner: registration.** Self-registering `static check::Suite check_by_name {…}` tables append to
   a function-local static vector. Defaulted `std::source_location` gives `file:line` without macros.
   An empty registry, a duplicate name, or a check with no expectations each fails. An `atexit` guard
   turns a mid-run `exit(0)` into a failure. Check objects compile with `-Werror=unused-function`, so
   an unregistered check can't hide (02 §A).
9. **`make check`.** It runs `bin/check` and then `sh tests/cli.sh bin/mps`, always both, and exits
   non-zero if either failed (02 §A). *Resolved:* brief 01's sequential recipe stops at the first
   failure, so a broken runner would hide the CLI results. Brief 02's runs both halves and still fails
   as a whole.
10. **`tests/cli.sh`.** POSIX `sh` built-ins only. It captures output before comparing, never through
    a pipe, and uses no `set -e` and no `local`. `set -u` and a planned-count check catch skipped
    checks. The environment is pinned: `LC_ALL=C`, `NO_COLOR=1`, `FORCE_COLOR` and `TASK_ID` unset,
    stderr redirected. It reads the expected version from `src/main.hpp`, the version source
    (02 §A).
11. **Don't lock in known-wrong behaviour** (02 §A):
    - the sieve's `[1]`, and its out-of-range write for n < 3;
    - `-p` omitting 2;
    - the exit statuses of `mps abc`, `mps 31` and bare `mps`;
    - the `-t` and `-f` parse bugs;
    - the per-unit logger state.

    Checks and smoke lines cover only behaviour that is right today.
12. **License** (02 §B):
    - **Headers:** `SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception`. The
      exception is SPDX's listed id for the FSF template, so the expression is valid in SPDX 2.3 and
      3.0 and states the permission in every file. The GOAL records that this meets R7.
    - **`LICENSE-EXCEPTION`:** the template's paragraph with only the brackets filled.
    - **`LICENSE`:** unchanged.
    - **README:** a License section, version 3 only.
13. **Deferrals.**
    - The per-unit logger state becomes a new seed.
    - The sieve's out-of-range write and `[1]` go into `issues/results-and-cli-fixes.md`, which already
      owns `-p`.
14. **Proof the gate isn't hollow.** Under `make -B check`, each of these breaks must exit non-zero
    and name the failure:
    - a failing expectation;
    - a deleted table row (the build fails);
    - an empty body;
    - a crash;
    - `std::exit(0)`;
    - `tests/cli.sh` given `/usr/bin/true` or `/usr/bin/false`.

    make 3.81 compares whole seconds, so proofs use `make -B`.
