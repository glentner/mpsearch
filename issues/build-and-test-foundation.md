---
status: adopted:build-and-test-foundation
kind: feature
appetite: small
---

# A build both compilers accept, and a test target

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The build works with Apple clang and nothing has tested it anywhere else. Under GCC and libstdc++,
`namespace log` (`src/log.hpp:26`) collides with the C library's `log` function: a translation unit
that includes `<cmath>` before `log.hpp` fails with "'namespace log { }' redeclared as different
kind of entity" (g++-16, checked 2026-10-11). Every arithmetic module will need `<cmath>`, so the
Linux build breaks the day the transform engine lands.

`-Ofast` (`Makefile:5`) is deprecated in clang 21, which warns "use '-O3 -ffast-math' for the same
behavior". The fast-math it implies lets the compiler reassociate floating-point sums and assume no
NaN or infinity. Weighted-transform squaring depends on exactly the rounding behaviour those
assumptions throw away (§ Invariants 2). This was measured on 2026-10-11 with both Apple clang 21 and
GCC 16. Under `-Ofast`, the standard FP64 round-to-integer idiom (x + 1.5·2^52) − 1.5·2^52 returned
2.75 where 3 was due, and the TwoSum error term came out 0. `-fno-associative-math` fixes both.

A fresh clone doesn't build. `.gitignore` ignores `bin/*` and `lib/*`, placeholder files included,
so a clone has neither directory, and the Makefile never creates them. `make` stops at once with
"unable to open output file 'lib/ansi.o': 'No such file or directory'" (checked in a fresh worktree
of `257acb7`, 2026-10-11). The profile's `setup` and `verify` work around it with `mkdir -p lib bin`.

There is no test target and no `clean` target on `main`. The profile's `verify` is a rebuild and two
smoke runs, so no later cycle has anything to verify against. `clang++ -Wall -Wextra` reports only
three unused parameters, in the test stubs.

The license needs a valid footing too. The SPDX headers say `GPLv3`, which isn't a valid SPDX
identifier. And later backends will link proprietary vendor runtimes, such as NVIDIA's CUDA runtime,
which GPLv3's system-library exception may not cover. The author decided (2026-10-11) to keep
GPL-3.0-only and grant an additional permission under GPLv3 section 7 for linking those runtimes.

## Why it was deferred

Pre-existing on `main` since 2024. It comes first because every later cycle needs a test target to
verify against, and the GCC failure blocks every Linux target.

## Outcome / vision

`make` builds warning-free with Apple clang, LLVM clang and GCC, on macOS arm64 and on Linux x86_64
and aarch64. `make check` runs a small, dependency-free test suite written in the house style, and
the profile's `verify` becomes `make check`. Numeric code compiles with IEEE-safe flags; the rest may
stay as aggressive as the author likes. The license is GPL-3.0-only with the additional permission,
stated in every header and in one exception file. Linux runs come with the cluster test jobs
(`issues/cluster-test-jobs.md`).

## Sketch of the acceptance criteria

- **R1** — WHEN `make` runs with GCC (13 or later) or clang, the build SHALL succeed whichever
  sources include `<cmath>`.
- **R2** — WHEN `make` runs in a fresh clone, it SHALL create the output directories it needs and
  build `bin/mps`. The profile's `mkdir -p` workaround is dropped in the same commit.
- **R3** — `make check` SHALL build and run the test suite, name each failing test, and exit
  non-zero if any test fails.
- **R4** — The test suite SHALL need nothing beyond the C++20 compiler and GNU make 3.81.
- **R5** — Sources that do floating-point arithmetic SHALL compile without fast-math semantics and
  with floating-point contraction set explicitly, and the build log SHALL show both.
- **R6** — `make clean` SHALL remove the objects in `lib/` and the binary in `bin/`, and nothing
  else.
- **R7** — Every source file's license identifier SHALL be `GPL-3.0-only`.
- **R8** — The repository SHALL carry the GPLv3 text unchanged in `LICENSE`, plus a separate file
  granting the additional permission under section 7 to link the named vendor GPU and accelerator
  runtimes. `README.md` SHALL state both.

## Notes

- The profile's `verify` moves to `make check` in the same commit (§ Conventions).
- Shaping decides the namespace fix: rename `log` (for example, `logging`) or wrap the project in
  `namespace mps`. The `log::info(…)` call sites are part of the house style, so the choice is the
  author's.
- The additional permission follows the FSF's template for GPL-incompatible libraries (GPL FAQ,
  "What legal issues come up if I use GPL-incompatible libraries with GPL software?"). It names the
  NVIDIA CUDA runtime and libraries, AMD ROCm libraries, Apple's Metal frameworks and Google's libtpu.
  The author confirms the wording with counsel.
- Contraction defaults differ: GCC contracts multiply-adds across statements in GNU dialects and not
  in ISO ones, and clang contracts within a statement. Fused multiply-adds break double-word
  arithmetic and make results differ between builds, so R5 pins the setting (for example,
  `-ffp-contract=off`, with explicit `std::fma` where a fused operation is intended).
- The author's uncommitted 2024 Makefile adds a `clean` target; R6 carries that intent.
- Found by: factory adoption review, 2026-10-11.
