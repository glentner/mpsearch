---
slug: logger-state-per-translation-unit
title: One logger state for the whole program
kind: fix
appetite: small
status: done
branch: fix/logger-state-per-translation-unit
base: main
current_phase: done
last_updated: '2026-10-12'
phases:
- id: P1
  name: One logger state, and its checks
  status: done
  satisfies:
  - R1
  - R3
  - R4
  - R5
  - R6
  - R7
  depends_on: []
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/logger-state-per-translation-unit/verify/p1.sh
- id: P2
  name: -D and -d as the parser reads them
  status: done
  satisfies:
  - R1
  - R2
  - R4
  - R7
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/logger-state-per-translation-unit/verify/p2.sh && sh spec/logger-state-per-translation-unit/remediation/f1.sh
    && sh spec/logger-state-per-translation-unit/remediation/f2.sh
- id: P3
  name: Check audit, deferral ledger, cluster runs
  status: done
  satisfies:
  - R7
  depends_on:
  - P2
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/logger-state-per-translation-unit/verify/p3.sh && sh spec/logger-state-per-translation-unit/remediation/f1.sh
    && sh spec/logger-state-per-translation-unit/remediation/f2.sh
review:
  last_reviewed_commit: 6988a3ec3d1586753bf301a1486a99a234972ec6
  verdict: approved
  blocked_reason: ''
  cycle: 3
---
# TECH.md — One logger state for the whole program

The **context engine and finite-state machine** for this fix. The YAML frontmatter above is the
resume ground truth. Read it from the cycle worktree with
`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/logger-state-per-translation-unit/TECH.md`.
The per-phase checklists below are the work.

`code-build` does four things per phase:

- executes the next actionable phase;
- runs its `verify:` command;
- updates the state with `uv run --with pyyaml python "$KB/bin/factory/set_phase.py" …`;
- makes one atomic commit of the code and the state.

Sources:

- **Vision / requirements (locked):** [`GOAL.md`](GOAL.md). The R-IDs are the contract.
- **Authoritative design:** [`PLAN.md`](PLAN.md).
- **Backing research:** none. This is the lean path (PLAN § 4).

## Frontmatter field reference

- `status` (top): `planned | in_progress | blocked | in_review | done`.
- `appetite`: `small`. It caps the phase count and the build-iteration budget (the circuit breaker).
- `branch`, `base`: the cycle's branch and the profile's `base`.
- phase `status`: `pending | in_progress | done | blocked`.
- `satisfies`: the GOAL R-IDs this phase delivers (the traceability anchor for `code-review`).
- `depends_on`: the phase ids that must be `done` first.
- `parallel`: `false` throughout. P2 touches the coupled core (`src/cli.cpp`), P1 lands the state P2
  builds on, and P3 tests and pushes the result of both.
- `hammerable`: `false` throughout. Each phase is a correctness phase, and R7 is a gate on gates.
- `hill`: `uphill` → `crest` → `downhill`.
- `attempts`: the durable failed-verify counter, bumped by
  `set_phase.py --phase P<n> --record-attempt`.
- `verify`: the exact command that proves the phase.
- `review.cycle`: completed review passes.

## Conventions (apply to every phase)

- **`AGENTS.md` is the constitution.** Walk the § Invariants sections PLAN § 3 lists. This design
  touches no high-blast-radius path: `src/main.cpp`, `src/math.*` and `src/result.*` stay as they
  are, and the gates hold them equal to `9a41271`. If a phase seems to need one of them changed,
  stop and escalate; don't edit it.
- **One phase per `code-build` invocation.** One atomic commit with the code and the `TECH.md` state,
  subject `[fix] Build logger-state-per-translation-unit P<n>: …`, no trailers.
- **Same-commit rules.** No option changes, so the help, the usage and `README.md` stay as they are.
  The `AGENTS.md` § Map line for `tests/log_peer.*` lands in P1's commit, with the files. No R#/P#
  ids in `src/` or `tests/`.
- **The gates are fixed.** `spec/logger-state-per-translation-unit/verify/` is reviewed as of the
  plan commit, and each gate refuses to run if it differs. If a gate is wrong, stop and escalate
  rather than edit it.
- **What each gate needs.** The gates run on macOS and need `clang++`, `g++-16`, GNU make 3.81 at
  `/usr/bin/make`, `git`, `rsync`, `tar`, `python3` and `hostname`. P3 also needs the site file.
- **What each gate prints.** A gate prints `GATE FAIL: <reason>` and exits 1 on the first failure,
  or ends with a `Pn gate: green (…)` line. Each is cumulative. Run a gate after the phase's changes
  are in the working tree: it reads uncommitted files as a clone would hold them.
- **The gates read text from the tree** (PLAN § 2):
  - the five declarations in `src/log.hpp`, exactly;
  - the two statements in `src/cli.cpp`, exactly;
  - the `#include "log.hpp"` line in `src/math.cpp`, `src/result.cpp`, `src/cli.cpp` and
    `src/main.cpp`;
  - `logging::init();` in `src/main.cpp`;
  - `} catch (cli::argument_error const& error) {` in `src/main.cpp`;
  - the check labels and the `cli.sh` expectation names in PLAN § 2's two tables, spelled exactly.
- **Each check is one condition.** In `tests/cli.sh`, `expect NAME [ a ] && [ b ]` counts only
  `[ a ]`, so write two `expect` lines. Keep `planned` exact.
- **The checks' three rules** (PLAN § 2):
  1. Set the logger's state by assignment, with the fake host `check-host`. Never call `init()` or
     `set_hostname()` in `bin/check`.
  2. Log only through a type local to the file.
  3. Capture what is logged with `CaptureCerr`, and restore the state with `SavedLogger` before the
     check returns.

  `make check`'s output must hold no detailed line and never this machine's host name; every gate
  checks it.
- **Nothing under `tests/` describes a per-file copy of the state (R7).** That rules out phrases
  such as "one copy per translation unit", "this file's copy" and "per-file copy".

---

## Phase P1 — One logger state, and its checks
**Satisfies:** R1 (messages raised once parsing is over), R3, R4, R5, R6, R7 (their checks) ·
**Depends on:** —

**Goal:** the logger holds one state. Under `-D`, the start lines from `src/math.cpp`, the result
printer's lines and `main.cpp`'s messages carry the detailed style and the task stamp. Without `-D`,
nothing on stderr changes. `make check` shows all of it, including the cross-unit level, through
`tests/log_peer.*`.

- [x] `src/log.hpp`: the five declarations exactly as PLAN § 2 gives them (`inline`, no
      initialisers). Nothing else in the file changes but comment lines.
- [x] `tests/log_peer.hpp`, `tests/log_peer.cpp` (new): `Addresses` and `addresses()`, `set_level`,
      `set_style`, `debug`, `info`, `plain`, `stamped`, `class SavedLogger` and `class CaptureCerr`,
      as PLAN § 2 gives them, every one defined in `log_peer.cpp`; and the check
      `the logger's state is as the run began`. They have the SPDX header and the code style of
      `src/`. The peer logs only through a type local to `log_peer.cpp`.
- [x] `tests/check_log.cpp`: the three checks `the logger's state is one copy`,
      `the level holds across units` and `the detailed style holds across units`, as PLAN § 2's
      table gives them. The comment at lines 22–24 is replaced, word for word, by PLAN § 2's.
- [x] `tests/check_result.cpp`: `the printer's errors log in the shared style` (PLAN § 2's table),
      through the file's `print_run`, holding a `SavedLogger`.
- [x] `tests/cli.sh` (PLAN § 2, P1), every expected line written out literally as `9a41271` prints
      it:
  - [x] the four P1 expectations PLAN § 2 names, spelled exactly;
  - [x] `-D -t ll 31` and `-D 31`, stamped, under `TASK_ID=abc`, unset and empty;
  - [x] `-d -t ll 31` exactly as without `-d`;
  - [x] under `FORCE_COLOR=1`, the exact bytes of `-p -p 3`'s warning and `-x 31`'s critical line;
  - [x] `planned` updated.
- [x] `AGENTS.md` § Map: the `tests/` line names `log_peer.*`, a second unit the logger's checks log
      from, with their guards and the check that the state is restored.
- **Verify:** `sh spec/logger-state-per-translation-unit/verify/p1.sh` (about 4 minutes). It runs:
  - `make -B check` on `clang++` and `g++-16`: green, without warnings, with no detailed line and no
    host name in its output;
  - the files as PLAN § 2 leaves them;
  - nothing under `tests/` describing a per-file copy;
  - 245 runs byte for byte as `9a41271`'s;
  - 18 detailed runs, stamped with this host and `TASK_ID`;
  - `make check` red under each of PLAN § 6's defects 1–9.
- **Touches:** `src/log.hpp`, `tests/log_peer.hpp`, `tests/log_peer.cpp`, `tests/check_log.cpp`,
  `tests/check_result.cpp`, `tests/cli.sh`, `AGENTS.md`.

## Phase P2 — -D and -d as the parser reads them
**Satisfies:** R1 (messages raised while parsing), R2, R4 (from the point the parser reads `-d`), R7
(their checks) · **Depends on:** P1

**Goal:** the parser applies `-D` and `-d` the moment it reads them. A duplicate-use warning, an
argument error or PVAL's error raised after that is detailed and stamped. One raised before it
prints exactly as without `-D`.

- [x] `src/cli.cpp`: in `Interface::check_flag`, `logging::set_level(logging::DEBUG);` after
      `debug_mode = true;` and `logging::set_style(logging::DETAILED);` after
      `detailed_mode = true;`, each spelled as PLAN § 2 gives it and appearing once in the file.
      Nothing else in the file changes but comment lines. `src/cli.hpp`, `src/main.hpp` and
      `src/main.cpp` don't change.
- [x] `tests/check_cli.cpp`:
  - [x] `-D and -d apply as the parser reads them` (PLAN § 2's table and bullets), through
        `cli::parse_args`, each parse starting from `INFO`, `DEFAULT`, `check-host` and `abc`, the
        long forms included, every detailed line matched whole through `log_peer::stamped`;
  - [x] a `SavedLogger` in every check that parses a command line holding `-d` or `-D`, groups
        included, so no state leaks into a later check's output.
- [x] `tests/cli.sh` (PLAN § 2, P2), every expected line written out literally:
  - [x] the three P2 expectations PLAN § 2 names, spelled exactly;
  - [x] `-Dpp 3` and `-D abc` stamped under `TASK_ID=abc`;
  - [x] `-p -p -D 3`, `-f -D 31` and `-t -D 31` exactly as `9a41271` prints them, under
        `TASK_ID=abc` (not as `bin/mps` prints the same words less `-D`);
  - [x] `planned` updated.
- [x] **F1 (review cycle 1, R2, R7).** `tests/cli.sh` pins, through `bin/mps`, that a `-D` read after
      `--` changes nothing: `31 -- -D`, `-- -D 31` and `-- -D` under `TASK_ID=abc`, each one
      `expect` comparing stderr with `9a41271`'s text, so a stamp would show. `planned` 549 → 552.
      Nothing under `src/` changes.
- **Reopened 2026-10-12** for review cycle 1's F1: the reviewer's mutant (the parser applying a
  `-D` read after `--`) left `make check` green. The gate became `verify/p2.sh` followed by
  `remediation/f1.sh`, which inserts that mutant and requires `tests/cli.sh`'s own checks to go red.
  Both were green with the three expectations above (`make check` 55 + 552 on `clang++` and
  `g++-16`), with no red run.
- [x] **F2 (review cycle 2, R2, R7), as a class.** `tests/cli.sh` pins, through `bin/mps`, that every
      word after `--` is a positional word, whatever it looks like. One loop over 23 words: every
      option short and long (`--help`, `-h`, `--version`, `-v`, `--json`, `-j`, `--primes`, `-p`,
      `--detailed`, `-D`, `--debug`, `-d`, `--skip-ptest`, `-s`), the valued ones (`--test=ll`, `-t`,
      `-tll`, `--file=x`, `-f`), a group (`-Dj`), an unknown option of each form
      (`--no-such-option`, `-x`) and a second `--`. Each word runs as `31 -- <word>` (the
      "Unexpected positional argument" error) and `-- <word>` (the "Invalid PVAL" error) under
      `TASK_ID=abc`, with one `expect` for the exit status and one for the whole stderr, each
      `9a41271`'s, so a stamp, a help, a version or a test run would show. F1's three expectations
      stay. `planned` 552 → 644. Nothing under `src/` changes.
- **Reopened 2026-10-12** for review cycle 2's F2: two mutants survived F1's instance-level fix, M2
  (the parser applying a `--detailed` read after `--`) and M6 (long options still read after `--`,
  a gap that predates this cycle). Geoffrey chose one last fix round, to close the class. The gate
  became `verify/p2.sh`, `remediation/f1.sh` and `remediation/f2.sh`, which inserts M2 and M6 and
  requires `tests/cli.sh`'s own checks to go red under each. All three were green with the loop
  above (`make check` 55 + 644 on `clang++` and `g++-16`), with no red run.
- **Verify:** `sh spec/logger-state-per-translation-unit/verify/p2.sh` (about 6 minutes). It runs
  all of P1's gate, plus:
  - `src/cli.cpp` differing from `9a41271` only by the two statements;
  - the detailed runs raised while parsing, 66 in all;
  - `make check` red under each of PLAN § 6's defects 1–16.
- **Touches:** `src/cli.cpp`, `tests/check_cli.cpp`, `tests/cli.sh`.

## Phase P3 — Check audit, deferral ledger, cluster runs
**Satisfies:** R7 (the audit and the runs) · **Depends on:** P2

**Goal:** the final tree passes every gate at once, and every deferral has a home. The pushed head
passes `make check` on the three cluster targets, recorded as redacted evidence that carries no node
name.

**Graded by the orchestrator** for the cluster runs. The evidence lives in
`spec/logger-state-per-translation-unit/evidence/`, which the blind reviewer cannot see. The
orchestrator grades it from the job ids, `sacct` and the session's MCP transcript.

**Order matters.** The tested commit's code must equal `HEAD`'s outside `spec/`, `issues/` and
`ROADMAP.md`. So:

1. Any code the audit changes is committed first, as
   `[fix] Build logger-state-per-translation-unit P3: <what>`, with the phase still `in_progress`.
2. Then come the pre-push checks, the push and the runs.
3. Last comes the phase commit with the evidence and the state.

- [x] **Audit.** Walk R1–R7 against `tests/` and confirm each has a check of its own, with one
      condition per `expect`. Fix and commit anything found (above).
- [x] **Deferral ledger.** Walk P1–P3 of this file, and the build's own commits, for "do not fix
      here", "known limitation", "left as a follow-up" and any deferral the build introduced.
  - Each needs an `issues/<slug>.md` from the ISSUE template and a `ROADMAP.md` entry. None is
    planned: PLAN § 3's one deviation is accepted, not deferred.
  - Every `issues/*.md` matches exactly one `**Seed:**` line.
  - This cycle's seed stays `adopted:logger-state-per-translation-unit`.
- [x] **Pre-push.** `sh spec/logger-state-per-translation-unit/verify/prepush.sh` must be green. It
      runs the P2 gate on the final tree, the ledger, the no-uncommitted-code rule, and the scan of
      the files and the history with its self-test.
  - `prepush.sh scan` runs the scan alone.
  - See it catch a planted site name once, and a planted detailed line naming a made-up node once,
    then remove both plants.
- [x] **Push.** `git push origin fix/logger-state-per-translation-unit`, fast-forward only (the
      foreman authorizes it for this phase). Never force.
- [x] **Run.** Submit the pushed head on `x86-64-avx512`, `x86-64-avx2` and `aarch64-gb10` through the
      cluster MCP server. Use the site recipe the foreman hands over (accounts, QoS and paths, never
      committed); this is the previous cycle's P4 recipe and `ci/test-commit.sh`'s header.
  - Write the job from the pushed commit's `ci/test-commit.sh` to `<scratch>/mpsearch/jobs/`,
    without its final newline, and check its `git hash-object` before `sbatch`.
  - `sbatch` with `script_path="<file> <hash> <target>"`, the target file's resources, and `output`
    in that directory.
  - **Limits:** at most 30 minutes, 8 cores and 16 GB, and at most two jobs queued per cluster (one
    on the GB10 cluster). Output only under `<scratch>/mpsearch/`.
  - Resubmit a run that fails for a reason outside the tree, such as a preemption or a node fault.
    Record only runs that test the tree.
- [x] **Evidence.** Pass each piece through
      `sh spec/logger-state-per-translation-unit/verify/redact.sh IN OUT`, which also makes the host
      after any detailed stamp `<host>` and then scans OUT. The pieces:
  - for each of the three targets: `evidence/<target>.json` (the run's `result.json`),
    `evidence/<target>.out` (the scheduler output) and `evidence/<target>.check.log` (the run
    directory's `check.log`);
  - `evidence/jobs.tsv`: a header `label job_id state exit_code` and one tab-separated line per job
    from `sacct`.

  Before committing, confirm that no `check.log` holds a detailed line; P3's gate requires it. The
  submission times and options go in the phase commit's body, with no account, QoS, cluster or node
  names.
- **Built 2026-10-12.** The audit found every R1–R6 with checks of its own and one condition per
  `expect`, so no code changed; no deferral was introduced, and the ledger holds 26 seeds, each
  indexed once. `prepush.sh` was green, and its scan caught a planted site name and a planted
  detailed line naming a made-up node, once each, before both were removed. The pushed head
  `5f37892` passed on all three targets: jobs 16971749 (`x86-64-avx512`), 44190590 (`x86-64-avx2`)
  and 474611 (`aarch64-gb10`), each `COMPLETED 0:0`, `check: 55 passed`, `cli: 549 passed`, and no
  detailed line in any `check.log`.
- **Rerun 2026-10-12, after review cycle 1.** P2's F1 changed `tests/cli.sh`, so the runs were
  redone on the new pushed head `1d75723`, whose code equals this commit's outside `spec/`.
  `prepush.sh` was green, and its scan caught a planted site name once before the plant was
  removed. Jobs 16972600 (`x86-64-avx512`), 44191834 (`x86-64-avx2`) and 474632 (`aarch64-gb10`)
  each ended `COMPLETED 0:0` with all five steps passed under GCC 14, `check: 55 passed`,
  `cli: 552 passed`, and no detailed line in any `check.log`. The evidence above was replaced
  through `redact.sh`, and `jobs.tsv` rewritten from `sacct`. The gate is now `verify/p3.sh`
  followed by `remediation/f1.sh`; both were green.
- **Rerun 2026-10-12, after review cycle 2.** P2's F2 changed `tests/cli.sh`, so the runs were
  redone on the new pushed head `a9500c1`, whose code equals this commit's outside `spec/`.
  `prepush.sh` was green, and its scan caught a planted site name once before the plant was
  removed. Jobs 16975835 (`x86-64-avx512`), 44196796 (`x86-64-avx2`) and 474664 (`aarch64-gb10`)
  each ended `COMPLETED 0:0` with all five steps passed under GCC 14, `check: 55 passed`,
  `cli: 644 passed`, and no detailed line in any `check.log`. The evidence above was replaced
  through `redact.sh`, and `jobs.tsv` rewritten from `sacct`; no node name survives in it. The gate
  is now `verify/p3.sh`, `remediation/f1.sh` and `remediation/f2.sh`; all three were green.
- **Verify:** `sh spec/logger-state-per-translation-unit/verify/p3.sh`. It runs everything
  `prepush.sh` checks but the uncommitted-code rule, plus the evidence of PLAN § 6:
  - three passed runs with GCC 13 or newer and the committed script, on one pushed commit whose code
    equals `HEAD`'s outside `spec/`, `issues/` and `ROADMAP.md`;
  - check logs whose summaries equal this tree's counts, with no detailed stamp;
  - `sacct` agreeing;
  - the scan covering the evidence.
- **Touches:** `spec/logger-state-per-translation-unit/evidence/*`. Also `tests/*`, but only if the
  audit finds a gap, and `issues/` and `ROADMAP.md`, but only for a deferral the build introduced.

---

## How `code-build` drives this

1. `next_phase.py` prints the next actionable phase.
2. Pre-flight: the cycle worktree is clean, on `branch`, and `base` is reachable.
3. Execute every `[ ]` in the phase, consulting `PLAN.md` for detail.
4. Run the phase's `verify:` command. Never advance on a checkbox alone.
5. Amend this file freely if reality diverges: regenerate the frontmatter with `set_phase.py`, and
   note the amendment in the commit body. STOP and escalate only on a **`GOAL.md` contradiction** or
   a gate that is wrong.
6. Mark the phase `done`, advance `current_phase`, and `--touch`. Make one `[fix]` commit, then stop
   and report.
