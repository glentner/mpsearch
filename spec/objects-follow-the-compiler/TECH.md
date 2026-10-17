---
slug: objects-follow-the-compiler
title: A compiler or flag change rebuilds what it links
kind: fix
appetite: small
status: in_review
branch: fix/objects-follow-the-compiler
base: main
current_phase: done
last_updated: '2026-10-17'
phases:
- id: P1
  name: The settings record, its documentation and the pycache rider
  status: done
  satisfies:
  - R1
  - R2
  - R3
  - R4
  - R5
  - R6
  - R7
  - R8
  - R9
  depends_on: []
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/objects-follow-the-compiler/verify/p1.sh
- id: P2
  name: Audit, deferral ledger, push and cluster runs
  status: done
  satisfies:
  - R1
  - R2
  - R3
  - R4
  - R5
  - R6
  - R7
  - R8
  - R9
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/objects-follow-the-compiler/verify/p2.sh
review:
  last_reviewed_commit: ''
  verdict: none
  blocked_reason: ''
  cycle: 0
---

# TECH.md — A compiler or flag change rebuilds what it links

The **context engine and finite-state machine** for this cycle. The YAML frontmatter above is the
resume ground truth. Read it from the cycle worktree with
`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/objects-follow-the-compiler/TECH.md`.
The per-phase checklists below are the work.

`code-build` does four things per phase:

- executes the next actionable phase;
- runs its `verify:` command;
- updates the state with `uv run --with pyyaml python "$KB/bin/factory/set_phase.py" …`;
- makes one atomic commit of the work and the state.

Sources:

- **Vision / requirements (locked):** [`GOAL.md`](GOAL.md). The R-IDs are the contract.
- **Authoritative design:** [`PLAN.md`](PLAN.md); § 2 is the `Makefile`, the `AGENTS.md` edits and
  `.gitignore`, § 6 the gates.
- **Backing research:** none (lean path); PLAN § 4 records the measurements.

## Frontmatter field reference

- `status` (top): `planned | in_progress | blocked | in_review | done`.
- `appetite`: `small`. It caps the phase count and the build-iteration budget (the circuit breaker).
- `branch`, `base`: the cycle's branch and the profile's `base`.
- phase `status`: `pending | in_progress | done | blocked`.
- `satisfies`: the GOAL R-IDs this phase delivers. P2 repeats R1–R9 because its audit walks them
  all and its cluster runs add GNU Make 4 to R3, R6 and R7.
- `depends_on`: the phase ids that must be `done` first.
- `parallel`: `false` throughout. P1 is the cycle's only code phase and P2 audits and pushes it.
- `hammerable`: `false` throughout. Both phases are correctness phases: what `make check` links is
  what every gate trusts.
- `hill`: `uphill` → `crest` → `downhill`.
- `attempts`: the durable failed-verify counter, bumped by
  `set_phase.py --phase P<n> --record-attempt`.
- `verify`: the exact command that proves the phase.
- `review.cycle`: completed review passes.

## Conventions (apply to every phase)

- **`AGENTS.md` is the constitution.** Walk the § Invariants sections PLAN § 3 lists: 2 (the numeric
  flags keep their values), 5 and § Conventions. No high-blast-radius path is touched; `src/` and
  `tests/` stay as `b3a6c5a` has them.
- **One phase per `code-build` invocation.** One atomic commit with the work and the `TECH.md`
  state, subject `[fix] Build objects-follow-the-compiler P<n>` (the profile's `branch commits`),
  the phase name in the body, no trailers. Commit messages and committed files name no cluster,
  account, partition, path or other site detail.
- **What is pinned** (PLAN § 2): the record's path `lib/settings`; the five recorded variables in
  their effective values; the reset at parse time, before any target is examined; the record's
  rule as an order-only prerequisite of every object; `BUILT`, used by the reset and by `clean`;
  silence; the compile and link recipes, `MODULES`, `DEPS`, the `check` recipe unchanged; the three
  `AGENTS.md` edits, verbatim; `.gitignore`'s one line. Anything else is the builder's.
- **The gates are fixed.** `spec/objects-follow-the-compiler/verify/` is reviewed as of the plan
  commit, and each gate refuses to run if it differs. Never edit it. If a gate rejects something
  correct, stop and escalate with the gate's message.
- **What the gates need.** This Mac: clang++, g++-16, GNU make 3.81 at `/usr/bin/make`, `python3`
  and `python3.14`, git, rsync, perl, tar, cmp. A full gate takes about 11 minutes. **Run one gate
  at a time, never two together:** at its peak a gate runs four makes at once (PLAN § 6).
- **What each gate prints.** `GATE FAIL: <reason>` and exit 1 at the first failure, or a
  `P<n> gate: green (…)` line. Run a gate with the phase's files in the working tree: it reads them
  as a clone would hold them, untracked files included. Never set `GATE_SELFTEST`: it is the
  planner's self-test and never prints green.
- **Deletion.** Every deletion in a session goes through `del`; the gates delete nothing but their
  own temporary directory. `make clean` inside a gate's copy is the product under test.
- **Budget.** `make check` with nothing to rebuild, at most 0.1 s longer than at `b3a6c5a` on this
  Mac (R4; baseline in PLAN § 6).

---

## Phase P1 — The settings record, its documentation and the pycache rider
**Satisfies:** R1–R9 · **Depends on:** —

**Goal:** a plain `make` or `make check` after any change of `CXX`, `CXXFLAGS`, `CPPFLAGS`, `IEEE`
or `BOUNDS` builds everything it links with that run's settings, an unchanged run stays exactly as
incremental and as fast as at `b3a6c5a`, `AGENTS.md` says so in the same commit, and importing a
generator leaves `git status` clean.

- [x] `Makefile`: `BUILT`, `SETTINGS` (the five variables, effective values), the reset at parse
      time above `-include $(DEPS)`, `lib/settings: | lib` writing the record silently, the record
      as an order-only prerequisite of the three object rules, `clean` as `/bin/rm -f $(BUILT)`;
      the comment says why the record is compared rather than timed (PLAN § 2; the prototype there
      passed the gates' self-test). No recipe, `MODULES`, `DEPS` or `check` change.
- [x] `AGENTS.md`: the three edits of PLAN § 2, verbatim (the profile's `verify` row, the
      `make clean` lines, the paragraph's four new lines), and no other change, in the same commit
      as the `Makefile` (R8; P2's gate checks the commit).
- [x] `.gitignore`: `__pycache__/` appended (R9).
- [x] Locally, before the gate: `make CXX=clang++ check`, then `make CXX=g++-16 check` with no `-B`,
      green; then `make CXX=g++-16 check` again prints no compile command.
- **Verify:** `sh spec/objects-follow-the-compiler/verify/p1.sh`.
- **Touches:** `Makefile`, `AGENTS.md`, `.gitignore`.

## Phase P2 — Audit, deferral ledger, push and cluster runs
**Satisfies:** R1–R9 (the audit, and the runs) · **Depends on:** P1

**Goal:** the final tree passes every gate at once, every deferral has a home, and the pushed head
passes `ci/test-commit.sh` on the three cluster targets under GNU Make 4, its check step rebuilding
nothing the build step built, recorded as redacted evidence.

**Graded by the orchestrator** for the ledger and the cluster runs. The evidence lives in
`spec/objects-follow-the-compiler/evidence/`, which the blind reviewer cannot see.

**Two dispatches.** P2 is built to run as two `code-build` invocations, so the wait for the
scheduler costs no builder time:

- **Dispatch A** (audit, ledger, pre-push, push, submit): the audit and any fix it commits; the
  ledger; `sh spec/objects-follow-the-compiler/verify/prepush.sh`; the push; the three submissions.
  Stop with the phase still `in_progress`, reporting the job ids.
- **Dispatch B** (evidence, gate, close): the evidence and its redaction;
  `sh spec/objects-follow-the-compiler/verify/p2.sh audit` until green (about a minute), then the
  full `sh spec/objects-follow-the-compiler/verify/p2.sh`; the phase commit.

**Order matters.** The tested commit's code must equal `HEAD`'s outside `spec/`, `issues/` and
`ROADMAP.md`. So any code the audit changes is committed first, as
`[fix] Build objects-follow-the-compiler P2: <what>`, with the phase still `in_progress`; then the
pre-push checks, the push and the runs; last the phase commit with the evidence, the ledger and the
state.

- [x] **Audit.** Walk R1–R9 against the `Makefile`, `AGENTS.md` and `.gitignore`, one by one, and
      confirm each has its gate step (PLAN § 6). Fix and commit anything found (above).
- [x] **Deferral ledger:** `spec/objects-follow-the-compiler/evidence/ledger.md` with
      `## Deferrals`.
  - [x] Walk P1–P2 of this file, PLAN § 5 and the build's own commits for "do not fix here", "known
        limitation", "left as a follow-up" and any deferral the build introduced. Each gets an
        `issues/<slug>.md` from the ISSUE template and a `ROADMAP.md` entry, or a Notes bullet in an
        existing seed, per `AGENTS.md` § Factory; list each under `## Deferrals` with its home.
  - [x] The known one: a Notes bullet in `issues/packaging-and-install.md` with the words "deferred
        from `objects-follow-the-compiler`" (the ledger check looks for them): an install target
        and any backend keep `lib/settings`, the reset before any target and `BUILT` as what
        `make clean` removes, and add their own outputs to `BUILT` (PLAN § 6).
  - [x] Every `issues/*.md` matches exactly one `**Seed:**` line in `ROADMAP.md`; this cycle's seed
        stays `adopted:objects-follow-the-compiler` (publish retires it).
- [x] **Pre-push.** `sh spec/objects-follow-the-compiler/verify/prepush.sh` must be green: the
      ledger, the no-uncommitted-code rule and the scan first, then the P1 gate on the final tree.
      `prepush.sh scan` runs the scan alone; see it catch a planted site name once and a planted
      detailed line naming a made-up node once, then remove both plants with `del`.
- [x] **Push.** `git push origin fix/objects-follow-the-compiler`, fast-forward only (the foreman
      authorizes it for this phase). Never force.
- [x] **Run.** Submit the pushed head on `x86-64-avx512`, `x86-64-avx2` and `aarch64-gb10` through
      the cluster MCP server, with the site recipe the foreman hands over (accounts, QoS and paths,
      never committed), as the previous cycles' last phases did and `ci/test-commit.sh`'s header
      says.
  - Write the job from the pushed commit's `ci/test-commit.sh` to `<scratch>/mpsearch/jobs/`,
    without its final newline, and check its `git hash-object` before `sbatch`.
  - `sbatch` with `script_path="<file> <hash> <target>"`, the target file's resources, and `output`
    in that directory.
  - **Limits:** at most 30 minutes, 8 cores and 16 GB, and at most two jobs queued per cluster (one
    on the GB10 cluster). Output only under `<scratch>/mpsearch/`.
  - Resubmit a run that fails for a reason outside the tree, such as a preemption or a node fault.
    Record only runs that test the tree.
- [x] **Evidence** (dispatch B). Pass each piece through
      `sh spec/objects-follow-the-compiler/verify/redact.sh IN OUT`: for each target
      `evidence/<target>.json`, `evidence/<target>.out`, `evidence/<target>.build.log` and
      `evidence/<target>.check.log` (the run's `result.json`, scheduler output, `build.log` and
      `check.log`); and `evidence/jobs.tsv` (a header `label job_id state exit_code` and one
      tab-separated line per job from `sacct`).
- **Verify:** `sh spec/objects-follow-the-compiler/verify/p2.sh`
  (`sh spec/objects-follow-the-compiler/verify/p2.sh audit` runs P2's own checks without the
  cumulative gate, for a fast first look).
- **Touches:** `spec/objects-follow-the-compiler/evidence/*`, `issues/packaging-and-install.md`,
  `ROADMAP.md` only if a new seed is needed; the `Makefile`, `AGENTS.md` and `.gitignore` only if the
  audit finds a gap.

---

## How `code-build` drives this

1. `next_phase.py` prints the next actionable phase (statuses are authoritative; the `current_phase`
   pointer is reconciled against them).
2. Pre-flight: the cycle worktree is clean, on `branch`, and `base` is reachable.
3. Execute every `[ ]` in the phase (consult `PLAN.md` for detail).
4. Run the phase's `verify:` command — never advance on a checkbox alone.
5. Amend this file freely if reality diverges (regenerate frontmatter with `set_phase.py`; note the
   amendment in the commit body). STOP and escalate only on a **`GOAL.md` contradiction**, or a gate
   that rejects something correct.
6. Mark the phase `done`, advance `current_phase`, `--touch`; one `[fix]` commit; stop and report.
