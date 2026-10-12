# GOAL — A build both compilers accept, and a test target

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** build-and-test-foundation
- **kind:** feature
- **appetite:** small

## Problem

The build has only ever met Apple clang, and a fresh clone doesn't build at all:

- `.gitignore` drops `bin/*` and `lib/*`, the placeholders included, and the Makefile never creates
  the directories. `make` stops with "unable to open output file 'lib/ansi.o'".
- Under GCC and libstdc++, `namespace log` (`src/log.hpp`) collides with the C library's `log`
  function. Any source that includes `<cmath>` before the logger fails with "'namespace log { }'
  redeclared as different kind of entity". Every arithmetic module will include `<cmath>`.
- `-Ofast` is deprecated in clang 21. It implies fast-math, which on 2026-10-11 broke the FP64
  round-to-integer idiom and the TwoSum error term under both Apple clang 21 and GCC 16. The
  transform engines depend on both.

There is no test target, so no later cycle has anything to verify against. The profile's `verify` is
a rebuild and two smoke runs.

The license needs a valid footing. The SPDX headers say `GPLv3`, which isn't an SPDX identifier, and
later backends will link proprietary vendor runtimes that GPLv3's system-library exception may not
cover.

## Outcome / vision

`make` builds warning-free with Apple clang and GCC in a fresh clone. `make check` runs a small,
dependency-free test suite written in the house style, and becomes the profile's `verify`. Every
source compiles with IEEE-safe floating point. The license is GPL-3.0-only, with an additional
permission under GPLv3 section 7 to link named vendor runtimes, stated in every header, in one
exception file and in the README.

## Acceptance criteria (the contract)

- **R1** — WHEN `make` runs with Apple clang or with GCC 13 or later, the build SHALL succeed with no
  warnings under `-Wall -Wextra`. A source that includes `<cmath>` and the logger together SHALL
  compile with both.
- **R2** — WHEN `make` runs in a fresh clone, it SHALL create the output directories it needs and
  build `bin/mps`.
- **R3** — `make check` SHALL build and run the test suite, print the name of each failing test, and
  exit non-zero if any test fails and zero if all pass.
- **R4** — The test suite SHALL need nothing beyond the C++20 compiler, GNU make 3.81 and a POSIX
  shell.
- **R5** — Every source SHALL compile without fast-math semantics and with floating-point
  contraction set explicitly, and the build log SHALL show both settings.
- **R6** — `make clean` SHALL remove the objects in `lib/` and the binaries in `bin/`, and nothing
  else.
- **R7** — Every source file's license identifier SHALL be `GPL-3.0-only`.
- **R8** — The repository SHALL carry the GPLv3 text unchanged in `LICENSE`, plus a separate file
  granting the additional permission under section 7 to link the NVIDIA CUDA runtime and libraries,
  AMD ROCm libraries, Apple's Metal frameworks and Google's libtpu. `README.md` SHALL state both.
- **R9** — The logger's output (levels, styles, colours, the `TASK_ID` and hostname stamps) SHALL be
  unchanged by the namespace fix.
- **R10** — The profile in `AGENTS.md` SHALL name `make` as `setup` and `make check` as `verify`, and
  the `mkdir -p` workaround SHALL be gone.

## Non-goals (no-gos)

- The command-line bugs and the result type: `issues/results-and-cli-fixes.md`.
- Running on Linux. That comes with the cluster test jobs (`issues/cluster-test-jobs.md`). This cycle
  is verified on macOS with Apple clang and Homebrew GCC 16.
- Hosted CI such as GitHub Actions.
- Any arithmetic.

## Clarifications

- **Q:** Who signs off on this GOAL, the plan and the review? — **A:** The author delegated every
  factory gate to the foreman for this effort; the foreman signs off and records each call here
  (resolved 2026-10-11).
- **Q:** How is the `log` collision fixed? — **A:** Rename the namespace to `logging`, with call sites
  `logging::info(…)`. A namespace can't share the global name `log` with the C library's function,
  and wrapping the whole project in a namespace would change every file and `main` for no other
  gain (resolved 2026-10-11).
- **Q:** One flag set for all sources, or IEEE-safe flags only for numeric ones? — **A:** One set for
  every source: `-O3` with fast-math off and contraction set explicitly. Nothing in the CLI benefits
  from fast-math, and one set can't be applied to the wrong file (resolved 2026-10-11).
- **Q:** `GPL-3.0-only` or `-or-later`? — **A:** `GPL-3.0-only`, matching the existing "GPLv3"
  headers, with the additional permission. The author chose GPLv3 plus the permission (resolved
  2026-10-11).

- **Q:** Do the dependency files `make` writes beside the objects count among what `make clean`
  removes (R6)? — **A:** Yes: `lib/*.d` are build outputs like `lib/*.o`. Nothing that isn't a build
  output is removed (resolved 2026-10-11, at plan).

- **Q:** Does `GPL-3.0-only WITH GPL-3.0-linking-exception` satisfy R7's `GPL-3.0-only`? — **A:**
  Yes. The license identifier in that SPDX expression is `GPL-3.0-only`, and the `WITH` clause names
  SPDX's listed id for the FSF template that R8's permission uses, so every header also states the
  permission (resolved 2026-10-11, at plan; research/02 §B).

## Related materials

- Seed: `issues/build-and-test-foundation.md`
- FSF GPL FAQ, "What legal issues come up if I use GPL-incompatible libraries with GPL software?"
  (the additional-permission template): https://www.gnu.org/licenses/gpl-faq.html#GPLIncompatibleLibs
- GCC manual, "Options That Control Optimization"; Clang Users Manual, floating-point options.
