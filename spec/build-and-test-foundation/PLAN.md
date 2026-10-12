# PLAN — A build both compilers accept, and a test target

> **Status:** Draft for review · **Last updated:** 2026-10-11
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). Backing detail is in
> [`research/`](research/) (digest: [`research/00-digest.md`](research/00-digest.md)).

## 1. Summary

Three separable changes, each verified from a fresh copy of the tree:

1. **The build.** The Makefile creates its own directories, tracks header dependencies, compiles every
   source with one IEEE-safe flag set that a command-line `CXXFLAGS` can't drop, honours `CXX`, and
   gains `clean`. The logger's namespace becomes `logging`, and GCC's warnings are fixed at their
   sites.
2. **A check suite.** `make check` builds `bin/check`, a dependency-free runner of self-registering
   checks, and runs it. It then runs `tests/cli.sh`, a POSIX smoke test of `bin/mps`. Both always run,
   and either failing fails the target.
3. **The license.**
   - **Headers:** `GPL-3.0-only WITH GPL-3.0-linking-exception`.
   - **`LICENSE-EXCEPTION`:** the FSF's section 7 template, filled in.
   - **README:** a License section.

The profile's `verify` becomes `make check` in the same commit as the target.

## 2. Design

### 2.1 Makefile (`Makefile`)

Adapted from the prototypes (research/01 §1, research/02 §A) into the house style: an explicit `MODULES`
list, `$(<)`/`$(@)` with parentheses, the SPDX header.

```make
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
```

- **Targets.** The default target is `$(EXE)`, the first rule.
  - `$(EXE)` links `lib/main.o` with `$(OBJECTS)`.
  - `$(CHECK)` links `$(CHECKS)` with `$(OBJECTS)`, so every module except `main`.
  - Objects come from `src/%.cpp` or `tests/%.cpp` through two pattern rules. Each has `Makefile` as a
    prerequisite, so a flag change rebuilds, and `| lib` as order-only. Check objects add
    `-Werror=unused-function` (§2.4).
  - `lib bin:` runs `mkdir -p $(@)`.
- **Compile lines** read `$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(IEEE) -c $(<) -o $(@)`. With `$(IEEE)` last,
  even a user's `-ffast-math` is overridden (research/01 §1). No `@` prefix: the log shows every
  compile line (R5).
- **`check`** depends on `$(EXE)` and `$(CHECK)`. Its recipe runs both halves, records any failure,
  and exits with it:

  ```make
  	@status=0; $(CHECK) || status=1; sh tests/cli.sh $(EXE) || status=1; exit $$status
  ```
- **`clean`** runs `/bin/rm -f lib/*.o lib/*.d $(EXE) $(CHECK)`. Only build outputs go, per R6 and the
  GOAL clarification; other files in `lib/` and `bin/` survive.
- **Placement.** `.PHONY: check clean` and `.DELETE_ON_ERROR:` are declared. `-include $(DEPS)` comes
  **last**, because above the first target it would steal the default goal.

### 2.2 Logger (`src/log.hpp` and its call sites)

`namespace log` becomes `namespace logging`. The call sites follow: 27 lines and 29 occurrences in
`src/cli.cpp`, `src/main.cpp` and `src/math.cpp` (research/01 §3), plus `AGENTS.md`'s Code style
line. Nothing else changes; the state stays `static`. Research found the output byte-identical to HEAD
across 32 invocations on each compiler (R9), and `tests/cli.sh` pins an exact stderr line.

**Known limitation, recorded and not fixed.** The logger's state is `static` in a header, so each
translation unit has its own copy. `-D` restyles only `main.cpp`'s messages, and the other units log
at the zero-initialised level, `DEBUG`. The fix, `inline` state, changes output, which R9 forbids
here. It goes to `issues/logger-state-per-translation-unit.md`, with a `ROADMAP.md` entry, in P3.

### 2.3 Warnings (R1)

clang reports 3 warnings at HEAD and g++-16 reports 21 (research/01 §4). Each fix leaves output
byte-identical:

- **`src/log.hpp`:** GCC's `-Wbuiltin-declaration-mismatch` disappears with the rename.
- **`src/ansi.hpp`:** `NO_COLOR`, `FORCE_COLOR`, `STDOUT_COLOR` and `STDERR_COLOR` are used only in
  `src/ansi.cpp` (verified by grep), so they move there as file-local `static bool`s.
- **`src/io.hpp`:** `FORMAT_EXAMPLE` becomes `static const char* const`.
- **`src/math.cpp`:** the three stub parameters get `[[maybe_unused]]`.

### 2.4 Check runner (`tests/check.hpp`, `tests/check.cpp`, `tests/check_<module>.cpp`)

"Test" already means a primality test in this code (`test_lucas_lehmer`, `-t`), so the unit is a
*check* (research/02 §A).

- **`check::Suite`.** A check is a `static void` function with no arguments. Each file declares
  `static check::Suite check_by_name { {"label", function}, … };`, the house `x_by_y` idiom with the
  type on the line above. Check files include `"check.hpp"`; the phase gates rely on this exact API. Its constructor appends the entries to a
  vector held in a function-local static, so static-initialization order doesn't matter. A defaulted
  `std::source_location` records `file:line` without macros.
- **Expectations.**
  - `check::expect(cond, what)` records a result.
  - `check::require(cond, what)` also throws `check::failure`, ending the check.
  - `check::expect_equal(a, b, what)` prints both values on failure.
- **Runner (`main` in `tests/check.cpp`).** Before each body it prints exactly `check <label>` and flushes. It
  catches `check::failure`, `std::exception` and `...` per check. Each of these fails:
  - a check that records no expectation;
  - a duplicate label;
  - an empty registry.

  Each failure prints ` FAIL <label>` (one leading space) at once and again in a final summary. The
  summary's last line is exactly `check: <passed> passed, <failed> failed`. An empty registry prints
  ` FAIL (no checks registered)` and `check: 0 passed, 1 failed`. The phase gates match these lines
  exactly. The exit status is 1 on any failure, else 0. An `atexit` guard
  `_Exit(1)`s if anything calls `exit` mid-run. Output goes through plain iostreams, not the logger,
  which is itself under check.
- **Unregistered checks.** Check objects compile with `-Werror=unused-function`, so a `static` check
  function missing from its table fails the build.
- **First checks.** Only behaviour that is right today is pinned (research/02 §A, "safe to lock in"):
  - **`check_math.cpp`:** `prime_sieve(n)` agrees with trial division for every `i` in `[2, n)` at
    several `n ≥ 3`, and marks squares at the bound composite. Index 1 and `n < 3` are known-wrong
    and left out.
  - **`check_log.cpp`:** a translation unit that includes `<cmath>` and `log.hpp` and calls both
    `std::log` and `logging::` (R1), and the level filter's ordering.
  - **`check_cli.cpp`:** `cli::parse_args` accepts `-t ll 31` and rejects an unknown option with
    `cli::argument_error`.

### 2.5 CLI smoke test (`tests/cli.sh`)

POSIX `sh`, shell built-ins only (R4). `run` captures output and status into variables, never through
a pipe. `expect NAME CMD…` passes only when `CMD` succeeds. There is no `set -e` and no `local`.
`set -u` stops a misspelled variable, and a `planned` count catches a skipped `expect`. The environment
is pinned (`LC_ALL=C`, `NO_COLOR=1`, `FORCE_COLOR` and `TASK_ID` unset) and stderr is redirected.

Checks:

- `--version` and `-v` print `mps v<APP_VERSION>`, read from `src/main.hpp`, and exit 0;
- `--help` exits 0 and starts with `Usage:`;
- `-p 30` prints the odd primes 3 to 29, accepting a leading `2` so the CLI-fixes cycle needn't
  edit this;
- `-t ll 31` writes `info [mps] Running Lucas-Lehmer test` to stderr (R9);
- an unknown option, an unknown `-t` name, two PVALs and a 10-digit PVAL each exit 1.

Each failed expectation prints ` FAIL <name>` (one leading space). The script's last line is exactly
`cli: <passed> passed, <failed> failed`. It exits 1 unless no
check failed and the passed count equals `planned`. It calls nothing but shell built-ins and the binary
under test, so it passes even with an empty `PATH` (R4).

### 2.6 License (`LICENSE-EXCEPTION`, `README.md`, every header)

- **`LICENSE-EXCEPTION`:** the FSF template's paragraph, "Additional permission under GNU GPL
  version 3 section 7 …", with only the brackets filled. It names the NVIDIA CUDA Toolkit runtime and
  libraries and the CUDA libraries of the NVIDIA driver, the AMD ROCm libraries, Apple's Metal,
  Foundation and QuartzCore frameworks, and Google's libtpu runtime with its PJRT plugin. The braced
  sentence is dropped: only the vendors can ship that source (research/02 §B).
- **`LICENSE`:** unchanged.
- **Headers:** `// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception`, in `#` form
  for the Makefile and `cli.sh`. Existing files keep their 2024 copyright year, and new files take 2026
  (R7; GOAL clarification).
- **README:** a "License" section, version 3 only, naming both files and saying the CPU build links
  none of the runtimes (R8).
- **`AGENTS.md` § Invariants 4:** gains one rule. Contributions come under the same expression,
  because outside GPL code can't carry the additional permission.

### 2.7 AGENTS.md (same commit as `check`)

- The profile's `setup` becomes `make` and `verify` becomes `make check`.
- The release recipe's `gate` becomes `[make clean, make check]`.
- The Build and run block loses `mkdir -p` and gains `make check` and `make clean`.
- The Map gains `tests/`.
- Code style names `logging::info(…)`.

### Requirement → design map

| R-ID | Design element(s) |
|------|-------------------|
| R1 | §2.1 flags and `CXX`; §2.2 rename; §2.3 warning fixes; `check_log.cpp` |
| R2 | §2.1 order-only `lib`/`bin` |
| R3 | §2.1 `check` recipe; §2.4 runner; §2.5 script |
| R4 | §2.4 C++20 only; §2.5 `sh` built-ins only |
| R5 | §2.1 `IEEE` variable, last on every compile line, echoed |
| R6 | §2.1 `clean` |
| R7 | §2.6 headers |
| R8 | §2.6 `LICENSE-EXCEPTION`, README |
| R9 | §2.2 rename only; §2.5 exact stderr line |
| R10 | §2.7 |

## 3. Invariant gate (AGENTS.md constitution check)

Gate 1 (from intent) and gate 2 (against this design) touch the same sections:

- **§ Invariants 2, arithmetic is checked.** The rule that numeric code compiles without fast-math is
  implemented for every source, and pinned last on the line. No arithmetic changes.
- **§ Invariants 4, clean room.** The exception text is the FSF template, cited. No third-party code
  enters. The new contributions rule strengthens the section.
- **§ Invariants 5, dependency-free core.** The runner and the script need only the compiler, make and
  `sh`.
- **§ Invariants 1 and 3** are untouched: no verdict path or known answer changes. The stubs keep their
  behaviour; only the attribute is added.
- **High-blast-radius paths.** `src/math.*`, `src/mp.*` and `src/io.*` are touched only by the header
  line, the rename, `[[maybe_unused]]` and a `const`. Research trigger 2 fired on that basis.
- **§ Conventions.** `verify` changes with the target it names. No spec ids appear in source. The code
  style is followed.

### Deviation justifications

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| — | — | — |

## 4. Rabbit holes (resolved)

- **The default goal.** `-include` above the first target steals it, so `-include $(DEPS)` goes last
  ([01](research/01-make-flags-namespace.md) §1).
- **Command-line flags.** `make CXXFLAGS=…` drops the IEEE flags, so they get their own variable
  (01 §1).
- **Inline logger state.** `inline` would fix the per-unit copies but change output, so the state stays
  `static` and the fix is deferred (01 §3).
- **Hollow gates** (registry, crash, `exit(0)`, pipes, `set -e`, environment leaks) are all guarded,
  each guard proven by breaking it on a prototype ([02](research/02-test-runner-license.md) §A).
- **SPDX expression.** `GPL-3.0-linking-exception` is listed, so headers can carry the permission
  without `LicenseRef`/`AdditionRef` (02 §B).
- **Adversarial design review** (research trigger 3: the deliverable is a gate). It ran on this design
  before commit; its outcome is recorded in §5.

## 5. Risks & open questions

- **GCC versions.** R1's "GCC 13 or later" is verified only at GCC 16 on macOS (with libstdc++, where
  the `log` collision lives). GCC 13–15 and glibc come with the next cycle's cluster jobs.
- **Whole-second times.** make 3.81 compares file times in whole seconds, so gates rebuild with
  `make -B` from fresh copies.
- **`stoi` wording** differs between libc++ and libstdc++; no check matches on it.
- **Scope of the R5 override.** Appending `$(IEEE)` overrides a user's `-ffast-math` or `-Ofast`
  (`-fno-fast-math` resets every sub-flag on GCC). On clang, a separately passed sub-flag such as
  `-fno-signed-zeros` survives `-fno-fast-math`, so a user who passes individual unsafe sub-flags owns
  the result. The default build and every gate use neither.
- **Counsel.** The author confirms the exception wording with counsel. If counsel rewords it beyond
  the template, headers fall back to `GPL-3.0-only` plus a pointer to `LICENSE-EXCEPTION`
  (research/02 §B).
- **Adversarial review outcome (2026-10-11).** A fresh reviewer at high effort found 12 issues in the
  draft gates, three of them HIGH:
  - the break checks' label grep also matched the runner's `check <label>` line and clang's echo of a
    non-compiling break;
  - the clean test never saw `bin/check`;
  - nothing proved `CXX` was honoured, so the GCC leg could silently build with clang.

  All twelve are folded into the gates:
  - copies hold git-known files only, modelling a clone, and logs live outside them;
  - make is pinned to 3.81;
  - every compile and link line must start with the chosen compiler;
  - the IEEE count and the `-ffast-math` ordering also run on the check build;
  - `clean` is checked by a before/after file snapshot;
  - runtime breaks must first build, then match anchored ` FAIL <label>` or `check <label>` lines;
  - the empty registry must print its exact line, and an unlisted check must be named by the
    compiler;
  - the probe compiles with `-c` and the build's flags;
  - `git archive` failures surface;
  - headers are matched exactly within the first three lines;
  - R10 also checks the release gate;
  - the smoke script may not call a tool by absolute path.

## 6. Verification strategy

Every phase gate runs from a scratch copy of the working tree, so a fresh-clone build is what gets
tested, with `make -B` (whole-second mtimes).

- **P1 (build).** With clang and with g++-16:
  - a fresh-copy `make` succeeds;
  - the log contains no `warning:` and shows `-fno-fast-math -ffp-contract=off` on every compile line;
  - `make CXXFLAGS=-ffast-math` still shows the IEEE flags last;
  - a `<cmath>` + logger probe compiles in both orders;
  - `make clean` leaves exactly the tracked files plus non-build files;
  - stderr for 12 invocations matches HEAD byte for byte, with timestamps masked.
- **P2 (checks).** `make -B check` is green with both compilers. Each break in research/02's proof
  list then turns it red and names the failure. The breaks run in a scratch copy, never the worktree.
- **P3 (license).**
  - No `GPLv3` remains.
  - Every source and script header carries the expression.
  - `LICENSE` matches `origin/main`.
  - `LICENSE-EXCEPTION` and the README section exist.
  - The deferral ledger holds: each "recorded, not fixed" in this cycle has its seed and ROADMAP
    entry.
