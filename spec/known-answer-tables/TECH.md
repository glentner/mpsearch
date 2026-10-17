---
slug: known-answer-tables
title: 'The known answers: exponents and residues, each with its source'
kind: feature
appetite: small
status: done
branch: feature/known-answer-tables
base: main
current_phase: done
last_updated: '2026-10-17'
phases:
- id: P1
  name: The format, the loader, the exponent table and the reference bound
  status: done
  satisfies:
  - R1
  - R2
  - R3
  - R4
  - R5
  - R6
  - R10
  - R11
  - R12
  depends_on: []
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/known-answer-tables/verify/p1.sh
- id: P2
  name: The final residues
  status: done
  satisfies:
  - R2
  - R3
  - R7
  - R9
  - R11
  - R12
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/known-answer-tables/verify/p2.sh
- id: P3
  name: The partial residues and the fingerprint form
  status: done
  satisfies:
  - R2
  - R3
  - R4
  - R8
  - R9
  - R10
  - R11
  - R12
  depends_on:
  - P2
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/known-answer-tables/verify/p3.sh
- id: P4
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
  - R10
  - R11
  - R12
  depends_on:
  - P3
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/known-answer-tables/verify/p4.sh
review:
  last_reviewed_commit: f108547d5b7bb13a56a0d9a801bae41b312672be
  verdict: approved
  blocked_reason: ''
  cycle: 1
---

# TECH.md — The known answers: exponents and residues, each with its source

The **context engine and finite-state machine** for this cycle. The YAML frontmatter above is the
resume ground truth. Read it from the cycle worktree with
`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/known-answer-tables/TECH.md`.
The per-phase checklists below are the work.

`code-build` does four things per phase:

- executes the next actionable phase;
- runs its `verify:` command;
- updates the state with `uv run --with pyyaml python "$KB/bin/factory/set_phase.py" …`;
- makes one atomic commit of the work and the state.

Sources:

- **Vision / requirements (locked):** [`GOAL.md`](GOAL.md). The R-IDs are the contract.
- **Authoritative design:** [`PLAN.md`](PLAN.md); § 2 is the format, the loader, the tables' rows,
  the command and the checks, § 6 the gates.
- **Backing research:** [`research/00-digest.md`](research/00-digest.md) and its three briefs.

## Frontmatter field reference

- `status` (top): `planned | in_progress | blocked | in_review | done`.
- `appetite`: `small`. It caps the phase count and the build-iteration budget (the circuit breaker).
- `branch`, `base`: the cycle's branch and the profile's `base`.
- phase `status`: `pending | in_progress | done | blocked`.
- `satisfies`: the GOAL R-IDs this phase delivers. R2, R3, R11 and R12 recur because each table
  brings its sources, its refusals, its share of the time budget and its high-blast-radius line.
- `depends_on`: the phase ids that must be `done` first.
- `parallel`: `false` throughout. Every phase edits `tests/known/` or `src/known.*`, which join the
  high-blast-radius list, or `tests/vectors/`, which is on it.
- `hammerable`: `false` throughout. Every phase is a correctness phase: these are the answers every
  engine is graded against.
- `hill`: `uphill` → `crest` → `downhill`.
- `attempts`: the durable failed-verify counter, bumped by
  `set_phase.py --phase P<n> --record-attempt`.
- `verify`: the exact command that proves the phase.
- `review.cycle`: completed review passes.

## Conventions (apply to every phase)

- **`AGENTS.md` is the constitution.** Walk the § Invariants sections PLAN § 3 lists: 2, 3, 4, 5 and
  § Conventions. `tests/vectors/` is on the high-blast-radius list; `tests/known/`, `src/known.*` and
  each table join it in the commit that creates them. A CONFIRMED review finding there needs
  Geoffrey's decision.
- **One phase per `code-build` invocation.** One atomic commit with the code, the tables, the
  documents and the `TECH.md` state, subject `[feature] Build known-answer-tables P<n>` (the
  profile's `branch commits`), the phase name in the body, no trailers.
- **What is pinned** (PLAN § 2): the format and its refusal order; `known_error`, `known::TABLES`,
  `Origin`, `Source`, `Run`, `Row`, `Table`, `parse` and `load`, each public function defined once,
  out of line, at file scope, its name on the definition's first line; `RECOMPUTED_BELOW`; every
  table row, its source kind, key and date; the command's three functions, its `main`, its failure
  messages and exit status; each embedded Res64 written once; the check labels, verbatim, and which
  check holds which sample; the path constants; the `AGENTS.md` lines; the bibliography keys and
  URLs. The gates build against them, wrap them and trace them. Anything else is the builder's.
- **The gates are fixed.** `spec/known-answer-tables/verify/` is reviewed as of the plan commit, and
  each gate refuses to run if it differs. Never edit it. If a gate rejects something correct, stop
  and escalate with the gate's message.
- **What the gates need.** This Mac: clang++, g++-16, GNU make 3.81 at `/usr/bin/make`, `python3`
  and `python3.14`, git, rsync, perl, otool, strings. A full gate takes about 12 minutes, most of it
  the tables' commands, which run in the background beside the builds. **Run one gate at a time,
  never two together:** at its peak a gate runs the builds beside six table commands, seven vector
  families, four recompute processes and then three point-mutant builds and probes (PLAN § 6).
- **What each gate prints.** `GATE FAIL: <reason>` and exit 1 at the first failure, or a
  `P<n> gate: green (…)` line. Run a gate with the phase's files in the working tree: it reads them
  as a clone would hold them, untracked files included.
- **The tables are written by their command, never by hand** (PLAN § 2; § Invariants 3). Each
  table's second line is the exact command, run from the repository root. A changed expected value
  is a finding.
- **The point mutants.** The gate builds every point mutant of `src/known.cpp` against the probe. A
  survivor the builder argues cannot be told from the original by any table goes in
  `spec/known-answer-tables/equivalent-mutants.txt`, one line each, `<mutated line, stripped> | <the
  mutation as the gate prints it> | <the argument>`. The review grades the arguments; anything else
  is a missing probe case or a loader bug, not an entry.
- **The clean room** (§ Invariants 4). The published values come from GIMPS's list and reports and
  the Mlucas README; never open a Mersenne or big-number program's source. Re-check each
  bibliography entry against its page and record what was read.
- **`make check` reads the tables at run time** through `src/known.*`; nothing in it runs Python
  (R11).
- **Budget.** `make check`, warm, at most 0.5 s longer than at `c41950c` on this Mac (R11).

---

## Phase P1 — The format, the loader, the exponent table and the reference bound
**Satisfies:** R1, R2, R3, R4, R5, R6, R10, R11, R12 · **Depends on:** —

**Goal:** the loader reads and refuses every table form PLAN § 2 gives, `make check` tests it on its
own tables, and the Mersenne prime exponents move to `tests/known/` with their 22 records byte for
byte, grown to 52, each with its source; the reference tests stay at p ≤ 10,000 and read the
exponents only through the loader.

- [x] `src/known.hpp`, `src/known.cpp`: the whole format of PLAN § 2 (all three stems, sources,
      corrections and their strength rule, conflicts, fingerprints, the partial table's refusal of a
      final residue), the refusal order, `known::TABLES` with the exponent table only; `MODULES`
      exactly as PLAN § 2 gives it.
- [x] `tests/known/generate.py` with the `mersenne-exponents` stem (and its three pinned functions);
      `tests/known/mersenne-exponents.txt` written by its command.
- [x] `git rm tests/vectors/mersenne-exponents.txt`; the family out of `tests/vectors/generate.py`
      (the comment above `COMPOSITE` stays until P2); the stem and the op out of
      `tests/vector_reader.cpp`, with the pinned `STEMS` line.
- [x] `tests/check_known.cpp`: the nine P1 checks, labels verbatim, every sample of PLAN § 2 in the
      check it names. `tests/check_reference.cpp`: `listed_primes()` through the loader from the
      `EXPONENTS` constant, 3 ≤ p ≤ 10,000; `the prime list carries its source` removed.
- [x] `doc/references.md`: `GIMPSPrimes`, checked against the page; the opening sentence and
      `doc/README.md`'s bibliography sentence name `tests/known/`.
- [x] `AGENTS.md`: the three P1 list lines; the `src/known` § Map bullet; the `tests/` bullet.
- [x] `spec/known-answer-tables/equivalent-mutants.txt`, if the point mutants leave survivors.
- **Verify:** `sh spec/known-answer-tables/verify/p1.sh`.
- **Touches:** `src/known.*`, `Makefile`, `tests/known/`, `tests/vectors/generate.py`,
  `tests/vectors/mersenne-exponents.txt` (removed), `tests/vector_reader.cpp`,
  `tests/check_known.cpp`, `tests/check_reference.cpp`, `doc/references.md`, `doc/README.md`,
  `AGENTS.md`, `spec/known-answer-tables/equivalent-mutants.txt`.

## Phase P2 — The final residues
**Satisfies:** R2, R3, R7, R9, R11, R12 · **Depends on:** P1

**Goal:** the eleven published final residues, each with its agreement count and source, recomputed
by the command below 200,000, and checked in `make check` against the reference residues.

- [x] `tests/known/generate.py`: the `final-residues` stem; the table written by its command.
- [x] `known::TABLES` gains the final table.
- [x] `tests/check_known.cpp`: the two P2 checks.
- [x] `tests/vectors/generate.py`: the comment above `COMPOSITE` points at
      `tests/known/final-residues.txt` (R12); no code line changes.
- [x] `doc/references.md`: `GIMPSReportLL` and `GIMPSReportPRP`, each checked against its page.
- [x] `AGENTS.md`: the final table's list line; the `tests/` bullet names it.
- **Verify:** `sh spec/known-answer-tables/verify/p2.sh`.
- **Touches:** `tests/known/`, `src/known.cpp`, `tests/check_known.cpp`,
  `tests/vectors/generate.py`, `doc/references.md`, `AGENTS.md`.

## Phase P3 — The partial residues and the fingerprint form
**Satisfies:** R2, R3, R4, R8, R9, R10, R11, R12 · **Depends on:** P2

**Goal:** the published s_100 of M3888509 and the nineteen computed partial residues, all
recomputed by the command, in the table fingerprints will join; no fingerprint row lands (R10).

- [x] `tests/known/generate.py`: the `partial-residues` stem; the table written by its command.
- [x] `known::TABLES` gains the partial table.
- [x] `tests/check_known.cpp`: the pinned `PARTIAL` constant, and `every known-answer table loads`
      expects `{EXPONENTS, FINAL, PARTIAL}` (amended at build: PLAN § 2's files table gives
      `check_known.cpp` to P1–P3, and the check pins `known::TABLES`).
- [x] `doc/references.md`: `MlucasREADME`, checked against the page, its self-test section only.
- [x] `AGENTS.md`: the partial table's list line; the `tests/` bullet names it.
- **Verify:** `sh spec/known-answer-tables/verify/p3.sh`.
- **Touches:** `tests/known/`, `src/known.cpp`, `tests/check_known.cpp`, `doc/references.md`,
  `AGENTS.md`.

## Phase P4 — Audit, deferral ledger, push and cluster runs
**Satisfies:** R1–R12 (the audit, and the runs) · **Depends on:** P3

**Goal:** the final tree passes every gate at once, every deferral has a home, and the pushed head
passes `make check` on the three cluster targets, recorded as redacted evidence with each run's
check-step time.

**Graded by the orchestrator** for the ledger and the cluster runs. The evidence lives in
`spec/known-answer-tables/evidence/`, which the blind reviewer cannot see.

**Two dispatches.** P4 is built to run as two `code-build` invocations, so the wait for the
scheduler costs no builder time:

- **Dispatch A** (build, pre-push, push, submit): the audit and any fix it commits; the ledger;
  `sh spec/known-answer-tables/verify/prepush.sh`; the push; the three submissions. Stop with the
  phase still `in_progress`, reporting the job ids.
- **Dispatch B** (evidence, gate, close): the evidence and its redaction; `## Times`;
  `sh spec/known-answer-tables/verify/p4.sh audit` until green (about a minute), then the full
  `sh spec/known-answer-tables/verify/p4.sh`; the phase commit.

**Order matters.** The tested commit's code must equal `HEAD`'s outside `spec/`, `issues/` and
`ROADMAP.md`. So any code the audit changes is committed first, as
`[feature] Build known-answer-tables P4: <what>`, with the phase still `in_progress`; then the
pre-push checks, the push and the runs; last the phase commit with the evidence, the ledger and the
state.

- [x] **Audit.** Walk R1–R12 against `src/`, `tests/`, `doc/` and `AGENTS.md`, one by one, and
      confirm each has its check or its gate step. Fix and commit anything found (above).
- [x] **Deferral ledger:** `spec/known-answer-tables/evidence/ledger.md` with `## Deferrals` and
      `## Times`.
  - [x] Walk P1–P4 of this file and the build's own commits for "do not fix here", "known
        limitation", "left as a follow-up" and any deferral the build introduced. Each gets an
        `issues/<slug>.md` from the ISSUE template and a `ROADMAP.md` entry, or a Notes bullet in an
        existing seed, per `AGENTS.md` § Factory; list each under `## Deferrals` with its home.
  - [x] Notes bullets, each with the words "deferred from `known-answer-tables`" (the ledger check
        looks for them): `issues/known-factor-tables.md` (the factor table as a fourth stem of this
        format and loader); `issues/burn-in-mode.md` (the loader takes a path; how an installed
        `mps` finds the tables); `issues/doc-claims-standing-check.md` (the exponent list now lives
        in `tests/known/mersenne-exponents.txt`).
  - [x] A new seed, `issues/known-answer-standing-check.md`, from the ISSUE template, with a
        `ROADMAP.md` entry and the same words: a standing check that every table only grows against
        `origin/main` (R4), which this cycle's gate checks once.
  - [x] Every `issues/*.md` matches exactly one `**Seed:**` line in `ROADMAP.md`; this cycle's seed
        stays `adopted:known-answer-tables`.
- [x] **Pre-push.** `sh spec/known-answer-tables/verify/prepush.sh` must be green: the ledger, the
      no-uncommitted-code rule and the scan first, then the P3 gate on the final tree.
      `prepush.sh scan` runs the scan alone; see it catch a planted site name once and a planted
      detailed line naming a made-up node once, then remove both plants.
- [x] **Push.** `git push origin feature/known-answer-tables`, fast-forward only (the foreman
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
      `sh spec/known-answer-tables/verify/redact.sh IN OUT`: for each target
      `evidence/<target>.json`, `evidence/<target>.out` and `evidence/<target>.check.log`; and
      `evidence/jobs.tsv` (a header `label job_id state exit_code` and one tab-separated line per
      job from `sacct`).
- [x] **Times** (R11's record): under `## Times` in `evidence/ledger.md`, one line per target,
      ``- `<target>`: check step <s> s (cycle 7: <s> s)``, from the run's `result.json` and from
      `spec/reference-big-integer/evidence/<target>.json`, then what the difference holds (the
      check step compiles too, so R11 itself is the gates' measurement on this Mac, which the line
      quotes from the last gate run).
- **Verify:** `sh spec/known-answer-tables/verify/p4.sh` (`sh spec/known-answer-tables/verify/p4.sh
  audit` runs P4's own checks without the cumulative gate, for a fast first look).
- **Touches:** `spec/known-answer-tables/evidence/*`, `issues/known-factor-tables.md`,
  `issues/burn-in-mode.md`, `issues/doc-claims-standing-check.md`,
  `issues/known-answer-standing-check.md`, `ROADMAP.md`; `src/`, `tests/` and `doc/` only if the
  audit finds a gap.

---

## How `code-build` drives this

1. `next_phase.py` prints the next actionable phase (statuses are authoritative; the `current_phase`
   pointer is reconciled against them).
2. Pre-flight: the cycle worktree is clean, on `branch`, and `base` is reachable.
3. Execute every `[ ]` in the phase (consult `PLAN.md` / `research/` for detail).
4. Run the phase's `verify:` command — never advance on a checkbox alone.
5. Amend this file freely if reality diverges (regenerate frontmatter with `set_phase.py`; note the
   amendment in the commit body). STOP and escalate only on a **`GOAL.md` contradiction**, or a gate
   that rejects something correct.
6. Mark the phase `done`, advance `current_phase`, `--touch`; one `[feature]` commit; stop and report.
