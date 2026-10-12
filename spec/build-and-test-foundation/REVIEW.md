# REVIEW — A build both compilers accept, and a test target

> Adversarial QA by `code-review`, run as a fresh subagent at xhigh effort. The correctness pass graded
> the branch diff against [`GOAL.md`](GOAL.md) and the repo's `AGENTS.md` § Invariants only; it did
> not see `PLAN.md`, `TECH.md`, `research/` or the gates. Every finding cites an executed command.

- **Reviewed commit:** 184450e  ·  **Base:** main (origin/main 76e45c0)  ·  **Date:** 2026-10-11
- **Verdict:** changes-requested
- **Cycle:** 1 of ≤3

## Verification run

- **`make -B check`:** clean with Apple clang 21 and with `CXX=g++-16`, no warnings; `check: 6 passed, 0 failed`, `cli: 14 passed, 0 failed`.
- **Fresh clones:**
  - `make` creates `lib/` and `bin/` and builds `bin/mps`;
  - `CXX=g++-16 make -j8 check` (CXX from the environment, parallel) passes.
- **`<cmath>` collision:** a negative control fails against base's `log.hpp` under g++-16 ("'namespace log { }' redeclared as different kind of entity") and passes against head's under both compilers. `tests/check_log.cpp` built against base's header fails the same way, so the check is a real guard.
- **R5:**
  - With the Makefile's flags: no `__FAST_MATH__`, and `a*b+c` is not fused under either compiler.
  - With `-Ofast`: fast-math is on and the expression fuses.
  - With plain `-O3`: it still fuses, so `-ffp-contract=off` is load-bearing.
  - Both flags appear on every compile and link line. No in-source fast-math pragma exists.
- **R9:** 186 invocations (31 argument sets × 6 environments) are byte-identical between base and head built with clang, with timestamps normalised.
- **R3 negative controls**, in a scratch clone. Each break below printed ` FAIL <label>` inline and in the summary, and `make` exited non-zero while `cli.sh` still ran:
  - a failing `expect`;
  - a failing `expect_equal`;
  - a check that throws;
  - a check with no expectations;
  - `exit(0)`;
  - a duplicate label.

  An unlisted check broke the build through `-Werror=unused-function`. A fake binary gave `cli: 7 passed, 7 failed`.
- **R4:** `env -i PATH=/nonexistent` with dash, sh and ksh runs `tests/cli.sh` to 14 passed; make is `/usr/bin/make` 3.81 throughout.
- **R6:** sentinel files survive `make clean`, which removes only `lib/*.o`, `lib/*.d`, `bin/mps` and `bin/check`. It is idempotent, and the release sequence `make clean && make check` passes.
- **Dependencies:** touching `src/log.hpp` rebuilds its dependents and relinks both binaries.
- **Greps:**
  - the SPDX expression is in all 21 headers;
  - no `log::` call site remains;
  - no spec ids appear in source;
  - no `mkdir -p` remains in AGENTS.md;
  - `LICENSE` is unchanged.
- **Seeds:** the claims in `issues/logger-state-per-translation-unit.md` and the sieve additions in `issues/results-and-cli-fixes.md` reproduce at the lines they cite.

## Requirement → evidence matrix

All rows were verified by the blind reviewer; no requirement is an artifact under `spec/`.

| R-ID | Implemented by | Verified how | Status |
|------|----------------|--------------|--------|
| R1 | Makefile flags; `logging` rename; `[[maybe_unused]]`; colour flags moved to `ansi.cpp`; `tests/check_log.cpp` | `make -B check` with clang and g++-16, warning-free; collision control | ✅ (GCC 16 only; GCC 13–15 come with the cluster jobs) |
| R2 | order-only `lib bin:` | fresh clone `make`; fresh clone `-j8` with GCC | ✅ |
| R3 | `check` target; `tests/check.{hpp,cpp}`; `tests/cli.sh` | negative controls above | ✅ (edge case: F2) |
| R4 | built-ins-only `cli.sh`; make 3.81 Makefile | dash, sh and ksh with no `PATH`; make 3.81 | ✅ |
| R5 | `IEEE` variable last on every line | build log; probe asm and macros, both compilers | ✅ |
| R6 | `clean` | sentinel test | ✅ |
| R7 | SPDX expression in every header | grep | ✅ |
| R8 | `LICENSE` unchanged; `LICENSE-EXCEPTION`; README section | diff and checksum; read | ✅ |
| R9 | namespace rename only | 186-invocation comparison | ✅ |
| R10 | profile `make` / `make check`; release gate | grep | ✅ |

Unmapped changes (possible scope creep):
- **The § Invariants 4 contributions bullet.** It is a policy no R-ID asks for, but it sits beside R7 and R8. The foreman judges it in scope: it follows from the license decision recorded in the GOAL's clarifications.
- **The deferral ledger.** It is required by `AGENTS.md` § Factory → Where deferred work goes.

## Findings

### [HIGH/CONFIRMED] F1 — § Invariants 4 points at "the license file", but the permission is in `LICENSE-EXCEPTION`
- **Where:** `AGENTS.md:159`
- **Failure scenario:** a reader checking § Invariants 4 opens `LICENSE`, the plain GPLv3, and finds no grant. `grep -c LICENSE-EXCEPTION AGENTS.md` returns 0, so the new artifact is named nowhere in the operating manual. This is stale § Invariants text: operating-manual drift that degrades a gate.
- **Evidence:** the grep above. Commit 184450e edited this section (it added line 160) and left line 159.
- **Touches:** § Invariants 4 (text, not a breach); R8.

### [MEDIUM/CONFIRMED] F2 — deleting a check file doesn't relink `bin/check`
- **Where:** `Makefile`, the `$(CHECK)` rule.
- **Failure scenario:** after a `tests/check_*.cpp` file is deleted, its object stays linked: the prerequisite list shrank, and nothing remaining in it is newer. `make check` keeps running the deleted checks until `make -B` or `make clean`. This can only produce a false failure, never a false pass, and the release gate runs `make clean` first.
- **Evidence:**
  1. In a scratch clone, add `tests/check_tmp.cpp` with a failing check: `make check` is red.
  2. Remove the file: `make check` still prints ` FAIL tmp doomed` and `check: 6 passed, 1 failed`, and `make -n check` shows no relink.
  3. `make -B check` is green.
- **Touches:** R3.

## Human-gate triggers

None. Neither CONFIRMED finding touches a path on § Invariants' high-blast-radius list. F1 is stale wording in § Invariants 4, which is tagged `[human gate]`, but it doesn't breach the clean-room rule: no code entered. The foreman holds the author's delegated sign-off for this effort.

## Remediation route

- F1 reopens P3. The `AGENTS.md` § Invariants 4 line names `LICENSE-EXCEPTION`, and P3's gate gains a check that `AGENTS.md` names it.
- F2 reopens P2. `bin/check` relinks when the set of check files changes, and P2's gate gains a delete-then-`make check` control.

## Review cycle 2 — approved (2026-10-11)

- **Reviewed commit:** ab0f381 · **Mode:** scoped to the remediation of F1 and F2, plus a regression run. This is the foreman's call under the author's delegated sign-off. The reviewer was a fresh blind subagent at xhigh effort with the same inputs as cycle 1.
- **F1: remediated.** `AGENTS.md` § Invariants 4 names `LICENSE-EXCEPTION` as the file granting the permission, beside the GPLv3 text in `LICENSE`. `LICENSE` still matches `origin/main` (sha256), and no other "license file" wording remains.
- **F2: remediated.** In a fresh clone of HEAD:
  1. Adding a failing check relinks `bin/check`: red, ` FAIL stale check fails`.
  2. Deleting it, after a two-second wait, relinks without `-B`: green, `check: 6 passed, 0 failed`. The stale check never runs.
  3. A further `make check` does not relink.

  The same script on 184450e still fails after the deletion, so the scenario tells the two commits apart. The link lines pass no directory to the linker.
- **Regression:** `make -B check` passes with clang and with g++-16, with no warnings (`check: 6 passed, 0 failed`, `cli: 14 passed, 0 failed`). A fresh-clone `make` of HEAD builds.
- **New findings:** none. The candidates were checked and dropped: the changed link recipe, spurious relinks, make's built-in rules acting on the `tests` directory, and same-second timestamps.
- **Human-gate triggers:** none in either cycle.
