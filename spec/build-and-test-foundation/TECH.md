---
slug: build-and-test-foundation
title: A build both compilers accept, and a test target
kind: feature
appetite: small
status: done
branch: feature/build-and-test-foundation
base: main
current_phase: done
last_updated: '2026-10-11'
phases:
- id: P1
  name: 'Build: directories, IEEE flags, CXX, clean, logging rename, warnings'
  status: done
  satisfies:
  - R1
  - R2
  - R5
  - R6
  - R9
  depends_on: []
  parallel: false
  hammerable: false
  hill: downhill
  attempts: 1
  verify: sh spec/build-and-test-foundation/verify/p1.sh
- id: P2
  name: 'Checks: runner, first checks, CLI smoke test, make check, profile verify'
  status: done
  satisfies:
  - R1
  - R3
  - R4
  - R9
  - R10
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/build-and-test-foundation/verify/p2.sh
- id: P3
  name: 'License: SPDX expression, LICENSE-EXCEPTION, README; deferral ledger'
  status: done
  satisfies:
  - R7
  - R8
  depends_on:
  - P2
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/build-and-test-foundation/verify/p3.sh
review:
  last_reviewed_commit: ab0f38122fe2a9236b9c51282c8861116a299f20
  verdict: approved
  blocked_reason: ''
  cycle: 2
---
# TECH.md — A build both compilers accept, and a test target

The **context engine and finite-state machine** for this cycle. The frontmatter is the resume ground
truth (`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/build-and-test-foundation/TECH.md`);
the checklists below are the work.

- **Vision / requirements (locked):** [`GOAL.md`](GOAL.md). R-IDs are the contract.
- **Authoritative design:** [`PLAN.md`](PLAN.md).
- **Backing research:** [`research/00-digest.md`](research/00-digest.md) + briefs 01, 02.
- **Gates:** `spec/build-and-test-foundation/verify/p{1,2,3}.sh`. Each works on a scratch copy of the
  working tree (tracked files plus untracked, unignored files), never the tree itself, and exits non-zero
  with a reason on any failure. They prove the phases; after merge they are a record, not a gate
  (`make check` is the standing gate).

## Conventions (apply to every phase)

- `AGENTS.md` is the constitution: § Conventions → Code style, the house idioms, tabs, no spec ids in
  source. Walk § Invariants 2, 4 and 5.
- One atomic commit per phase, code plus `TECH.md` state: `[feature] Build build-and-test-foundation
  P<n>: <phase name>`. No trailers.
- Same-commit rules: the profile's `verify` changes with the `check` target (P2).

---

## Phase P1 — Build: directories, IEEE flags, CXX, clean, logging rename, warnings
**Satisfies:** R1, R2, R5, R6, R9 · **Depends on:** —
**Goal:** a fresh copy builds warning-free with Apple clang and g++-16, with IEEE flags on every compile
line and output identical to HEAD.

- [x] Rewrite `Makefile` per PLAN §2.1:
  - the `CXX` origin check, `CXXFLAGS`/`IEEE`/`CPPFLAGS`, `MODULES := ansi cli io mp math`;
  - `lib/main.o`, the order-only `lib`/`bin`, both pattern rules with `Makefile` as a prerequisite;
  - `clean`, `.PHONY`, `.DELETE_ON_ERROR`, and `-include $(DEPS)` last.

  The `check` target and the check pattern rule may land now or in P2. The SPDX header changes in P3.
- [x] Rename `namespace log` → `namespace logging` in `src/log.hpp` and every call site in
  `src/main.cpp`, `src/cli.cpp` and `src/math.cpp` (PLAN §2.2). Nothing else in the logger changes;
  the state stays `static`.
- [x] Fix the warnings (PLAN §2.3):
  - the colour flags move from `src/ansi.hpp` into `src/ansi.cpp` as file-local `static bool`;
  - `FORMAT_EXAMPLE` becomes `static const char* const`;
  - `[[maybe_unused]]` on the three stub parameters in `src/math.cpp` (and its header if the
    declarations need it).
- [x] `AGENTS.md`: Code style names `logging::info(…)`.
- **Verify:** `sh spec/build-and-test-foundation/verify/p1.sh`.
- **Touches:** `Makefile`, `src/log.hpp`, `src/main.cpp`, `src/cli.cpp`, `src/math.cpp`, `src/math.hpp`,
  `src/ansi.hpp`, `src/ansi.cpp`, `src/io.hpp`, `AGENTS.md`.

## Phase P2 — Checks: runner, first checks, CLI smoke test, make check, profile verify
**Satisfies:** R1, R3, R4, R9, R10 · **Depends on:** P1
**Goal:** `make check` runs a self-registering check runner and a POSIX smoke test, always both. It
fails on any failure, on an empty registry, on a hollow check and on a mid-run `exit`, and it is the
profile's `verify`.

- [x] `tests/check.hpp` and `tests/check.cpp`: `check::Suite`, `expect`, `require`, `expect_equal`,
  `check::failure`, and the runner `main` with the guards (PLAN §2.4). House style throughout.
- [x] `tests/check_math.cpp`, `tests/check_log.cpp` and `tests/check_cli.cpp`, holding only the
  safe-to-lock behaviours (PLAN §2.4).
- [x] `tests/cli.sh` (PLAN §2.5).
- [x] `Makefile`: the check pattern rule (with `-Werror=unused-function`), `$(CHECK)`, and the `check`
  recipe that runs both halves.
- [x] `AGENTS.md`, same commit (R10):
  - the profile's `setup` = `make` and `verify` = `make check`;
  - the release `gate` = `[make clean, make check]`;
  - the Build and run block (no `mkdir -p`; `make check`, `make clean`);
  - the Map gains `tests/`.
- **Verify:** `sh spec/build-and-test-foundation/verify/p2.sh`.
- **Touches:** `tests/*`, `Makefile`, `AGENTS.md`.

## Phase P3 — License: SPDX expression, LICENSE-EXCEPTION, README; deferral ledger
**Satisfies:** R7, R8 · **Depends on:** P2
**Goal:** every header carries `GPL-3.0-only WITH GPL-3.0-linking-exception`, the exception is granted
in `LICENSE-EXCEPTION` and stated in the README, and every deferral this cycle made has its seed.

- [x] Every header in `src/*`, `tests/*` and the `Makefile` gets
  `SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception`. Copyright years stay; new
  files say 2026.
- [x] `LICENSE-EXCEPTION`: the FSF section 7 paragraph, filled in exactly as research/02 §B gives it.
  `LICENSE` is unchanged.
- [x] `README.md`: the License section (research/02 §B).
- [x] `AGENTS.md` § Invariants 4: contributions come under the same expression (PLAN §2.6).
- [x] **Deferral ledger.** Walk P1–P3 for "recorded, not fixed", "known limitation" and "deferred".
  Confirm each has its `issues/` seed and `ROADMAP.md` entry:
  - new `issues/logger-state-per-translation-unit.md` (kind `fix`, `unshaped`, from the ISSUE
    template), with a `ROADMAP.md` entry in Part I after the CLI-fixes entry;
  - `issues/results-and-cli-fixes.md` gains the sieve's out-of-range write for n < 3 and
    `prime_sieve(n)[1]`, with evidence.
- **Verify:** `sh spec/build-and-test-foundation/verify/p3.sh`.
- **Touches:** every source header, `Makefile`, `tests/*`, `LICENSE-EXCEPTION`, `README.md`,
  `AGENTS.md`, `issues/logger-state-per-translation-unit.md`, `issues/results-and-cli-fixes.md`,
  `ROADMAP.md`.

---

## How `code-build` drives this

1. `next_phase.py` prints the next actionable phase.
2. Pre-flight: the worktree is clean and on `branch`.
3. Execute every `[ ]` in the phase (PLAN and research for detail).
4. Run the phase's `verify:`; never advance on a checkbox alone. See it fail once (the gates' own
   negative controls do this for P2).
5. Mark the phase done with `set_phase.py`; one `[feature]` commit; report.
