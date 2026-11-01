---
slug: lucas-lehmer-test
title: The Lucas–Lehmer test on the engine, with a shift and its error checks
kind: feature
appetite: big
status: in_progress
branch: feature/lucas-lehmer-test
base: main
current_phase: P1
last_updated: '2026-11-01'
phases:
- id: P1
  name: The Jacobi symbol and the stop rule
  status: pending
  satisfies:
  - R5
  - R15
  - R18
  depends_on: []
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/lucas-lehmer-test/verify/p1.sh
- id: P2
  name: 'The test: the shift, the checks, the recovery and the result'
  status: pending
  satisfies:
  - R1
  - R2
  - R3
  - R4
  - R6
  - R7
  - R8
  - R9
  - R10
  - R11
  - R12
  - R13
  - R14
  - R16
  - R18
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/lucas-lehmer-test/verify/p2.sh
- id: P3
  name: The long tier, the landing gate and the remaining notes
  status: pending
  satisfies:
  - R16
  - R17
  - R18
  depends_on:
  - P2
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/lucas-lehmer-test/verify/p3.sh
- id: P4
  name: Audit, deferral ledger, push, the landing gate and the cluster runs' evidence
  status: pending
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
  - R13
  - R14
  - R15
  - R16
  - R17
  - R18
  depends_on:
  - P3
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/lucas-lehmer-test/verify/p4.sh
---

# TECH.md — The Lucas–Lehmer test on the engine, with a shift and its error checks

The **context engine and finite-state machine** for building this feature. The YAML frontmatter
above is the resume ground-truth (read it from the cycle worktree with
`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/lucas-lehmer-test/TECH.md`); the
per-phase checklists below are the work. `code-build` executes the next actionable phase, runs its
`verify:` command, updates state via `uv run --with pyyaml python "$KB/bin/factory/set_phase.py" …`,
and makes one atomic code+state commit.

- **Vision / requirements (locked):** [`GOAL.md`](GOAL.md) — R-IDs are the contract.
- **Authoritative design:** [`PLAN.md`](PLAN.md); § 2 is the design and what it pins, § 6 the gates.
- **Backing research:** [`research/00-digest.md`](research/00-digest.md) and four briefs.

## Frontmatter field reference

- `status` (top): `planned | in_progress | blocked | in_review | done` (`done` is stamped by
  `code-publish` after confirmation, just before landing — the terminal state of the retained record)
- `appetite`: `small | big` — caps phase count and build-iteration budget (circuit breaker).
- `branch`, `base`: the cycle's branch (prefix per GOAL `kind`, from the profile's `branches`) and the
  profile's `base`.
- phase `status`: `pending | in_progress | done | blocked`
- `satisfies`: GOAL R-IDs this phase delivers (traceability anchor for `code-review`).
  **Artifact-deliverable R-IDs.** Some requirements are met by a *committed document*. Attach such an
  R-ID to the phase that produces and commits the artifact, and give that phase a content `verify:`.
  **Say so in the phase body**, because it changes who can grade it: `code-review` blinds its reviewer
  to all of `spec/`, so an R-ID whose evidence lives there is graded by the *orchestrator*. Here R18's
  documents (AGENTS.md, the notes) and R17's command and row live outside `spec/`, so the blind
  reviewer grades them; R16's recorded baseline (PLAN § 6), P4's ledger, its landing-gate quote and
  its cluster evidence are the orchestrator's.
- `depends_on`: phase ids that must be `done` first (a phase is actionable only when met).
- `parallel`: `true` only for genuinely independent, non-coupled work. False throughout: every phase
  edits a high-blast-radius path or the coupled core's test entry points.
- `hammerable`: `false` marks a correctness/security phase that scope-hammering must **never** cut.
  False throughout: verdicts are earned.
- `hill`: `uphill` → `crest` → `downhill`. A phase stuck `uphill` across builds is a raised hand.
- `attempts`: durable failed-verify counter, bumped by `set_phase.py --phase P<n> --record-attempt` on
  every red verify gate; `next_phase.py` warns at ≥3.
- `verify`: the exact command that proves the phase. A gate carrying `$( )` or quoting the calling
  shell would mangle is set with `set_phase.py --verify-file PATH`, not `--verify`.
- `review.cycle`: completed review passes, auto-incremented by every `set_phase.py --verdict …`.

## Conventions (apply to every phase)

- Commit conventions, code style, and load-bearing invariants come from the repo's `AGENTS.md` — it
  is the constitution. Walk the sections of its § Invariants that this change touches; that is the
  curated footgun checklist.
- One phase per `code-build` invocation by default; one atomic commit containing **both** the code and
  the `TECH.md` state change. The branch commit subject is code-build's (`$KB/skill/code-build.md` →
  Step 6), not restated here; branch commits are squashed away at `code-publish` per the profile's
  `landing`. Trailers follow the profile's `branch commits`.
- The last phase carries the standing last-phase items: the deferral ledger (`code-plan` Step 6.8),
  and every item the previous cycle's last phase carried for a project-level obligation (mpsearch's
  `## Times` quote, taken as `code-build` Step 4 says). A plan that drops one says why in `PLAN.md`.
- A PR body (when the profile's `landing` opens one) ends with an attribution naming the actual
  harness, model, and variant.
- A CLI/feature change obeys the profile's `same-commit rules` (§ Conventions in `AGENTS.md`): the
  help text, docs, or generated files it names change **in the same commit**.

This cycle's own rules:

- **The human gate.** § Invariants 1, 2 and 3 and the paths `src/math.*`, `src/mp.*`, `src/result.*`,
  `tests/vectors/` and, from P2's commit, `src/lucas.*`: a CONFIRMED review finding there goes to
  Geoffrey before the cycle lands.
- **What is pinned** (PLAN § 2.4, § 2.5, § 2.6): the declarations and definitions' first lines, the
  check labels and their tables, the views' formats and the three log lines, `lucas::test` calling its
  four helpers and `ibdwt::square_subtract`, no reference or tables in `src/lucas.cpp`, the hooks set
  nowhere in `src/` but their header, the Makefile's recipes and flags, the AGENTS.md lines and the
  notes' names and limits. The gates build against them, wrap them and trace them. Anything else is
  the builder's.
- **The gates are fixed.** `spec/lucas-lehmer-test/verify/` is reviewed as of the plan commit, and
  each gate refuses to run if it differs. Never edit it. If a gate rejects something correct, stop and
  escalate with the gate's message.
- **Iterate on `quick`, finish on full.** `sh …/p<n>.sh quick` (about 3 to 5 minutes: clang only, no
  timing, no long tier, no run of the landing gate) while building; the phase's `verify:` is the full
  gate (PLAN § 6 gives each one's time). **Run one gate at a time, never two together, and nothing
  heavy beside it.**
- **What each gate prints.** `GATE FAIL: <reason>` and exit 1 at the first failure, or a
  `P<n> gate: green (…)` line. Run a gate with the phase's files in the working tree: it reads them as
  a clone would hold them, untracked files included.
- **What the gates need.** This Mac: clang++, g++-16, GNU make 3.81 at `/usr/bin/make`, `python3`,
  perl, rsync, git, pgrep; and for P4 the site file the scan reads.
- **Deletion.** `rm` at the shell is wrapped and refuses; use `del` (no flags) for any scratch
  deletion, and never a bypass. The Makefile's `/bin/rm` is the repo's convention.
- **The clean room** (§ Invariants 4). Build from the papers PLAN cites and the repo's notes; never
  open the source of Prime95/mprime, Mlucas, gpuOwl/PRPLL, CUDALucas, GMP or any other big-number
  program. Record any new source read in the commit body.
- **No R-/P-ids in source** (§ Conventions); comments cite the paper (author, title, year).
- **Budgets** (R16): `make check` with nothing to rebuild at most 5 s over `bcb6fdf`'s on this Mac
  (4.967 s, PLAN § 6; the gates measure it interleaved); `make check-long` at most 900 s.

---

## Phase P1 — The Jacobi symbol and the stop rule
**Satisfies:** R5, R15, R18 (the Jacobi symbol's write-up) · **Depends on:** —
**Goal:** `jacobi` on `BigInt`, equal to Python's on committed vectors that carry their command;
`run_in_order` stops at an error; the algorithm written up and cited in its module's note.

- [ ] `src/mp.hpp`, `src/mp.cpp`: `int jacobi(const BigInt& a, const BigInt& n)` (PLAN § 2.2), its
      first line pinned; `mp_error` for a zero or even modulus; Shallit and Sorenson 1993 cited
      (or Brent and Zimmermann 2010, Remark 3: say which in the commit body and the note).
- [ ] `tests/vectors/generate.py`: the `jacobi` family of research/01 (literal seeds; every exponent
      R1 and R2 cover in `make check` with symbols −1, 1 and 0; shifted LL residues; general odd
      moduli; Python's reciprocity loop on every record, Euler's criterion for prime M_p, a binary
      form for the rest); `tests/vectors/jacobi.txt` written by its command;
      `tests/vector_reader.*`: the op and field kind `J`, `jacobi` in `STEMS`.
- [ ] `tests/check_mp.cpp`: "the Jacobi symbol matches the vectors", grading every record.
- [ ] `src/math.cpp`: `run_in_order` stops after an error, its comment saying so (R15).
      `tests/check_result.cpp`: "run_in_order stops at the first verdict or error" (renamed from
      "…at the first verdict"), with stand-ins for an error first, in the middle and last.
- [ ] `doc/reference-arithmetic.md`: the Jacobi symbol (definition, algorithm, invariant,
      termination, a check-marked example), cited; "What mps uses" names `jacobi`.
      `doc/references.md`: `ShallitSorenson1993`, Verified against its DOI's metadata.
- **Verify:** `sh spec/lucas-lehmer-test/verify/p1.sh` (`… p1.sh quick` while iterating).
- **Touches:** `src/mp.*`, `src/math.cpp`, `tests/vectors/generate.py`, `tests/vectors/jacobi.txt`,
  `tests/vector_reader.*`, `tests/check_mp.cpp`, `tests/check_result.cpp`,
  `doc/reference-arithmetic.md`, `doc/references.md`, `doc/README.md`.

## Phase P2 — The test: the shift, the checks, the recovery and the result
**Satisfies:** R1–R4, R6–R14, R16 (the injections, `tests/cli.sh`, the `make check` budget), R18
(AGENTS.md's lines for `src/lucas.*`, `doc/lucas-lehmer.md`) · **Depends on:** P1
**Goal:** `mps -t ll p` runs the test of PLAN § 2.3 and reports its run in both views; the checks
prove it on the known answers at two shifts and prove every check and recovery by injection.

- [ ] `src/lucas.hpp`, `src/lucas.cpp` (PLAN § 2.3, § 2.4): the constants, `Step`, `Options` with its
      three hooks, `largest_exponent`, `unshifted`, `jacobi_of`, `degenerate`, `confirms_prime`,
      `test`, the pinned first lines; the refusals of R14 before any squaring; the log lines of
      PLAN § 2.5; no `reference::` or `known::`.
- [ ] `src/math.cpp`: `test_lucas_lehmer` returns `lucas::test(p)`. `src/math.hpp` if needed.
- [ ] `src/result.hpp`, `src/result.cpp`: `RunReport`, `prime_by_residue`, the run-carrying
      `composite_by_residue` and `error`, `run()`, the views of PLAN § 2.5, an error's record printed
      on stdout under `-j`, `RECORD_VERSION` 2 — in one commit with the record's change (R12, § Conventions).
- [ ] `tests/check_lucas.cpp`: the twelve labels of PLAN § 2.6; `tests/check_result.cpp`,
      `tests/check_math.cpp`: what the new behaviour makes wrong, and the views of a run and of an
      error.
- [ ] `tests/ll.sh` (PLAN § 2.6); `Makefile`: `MODULES += lucas`, `check` and `check-engine` run it.
- [ ] `tests/cli.sh`: only the expectations whose outcome the Lucas–Lehmer test decides, and the
      record version (R16 as the GOAL now words it; the gate compares it with `bcb6fdf`'s, PLAN § 6).
- [ ] `AGENTS.md`, in this phase's commit: the high-blast-radius line and § Map bullet for
      `src/lucas.hpp`, `src/lucas.cpp`; § Map's stale wording; `tests/check_lucas.cpp`, `tests/ll.sh`
      and the vectors in the `tests/` bullet (R18). Never change the four lines `tests/cli.sh` reads or
      the `verify` row.
- [ ] `doc/lucas-lehmer.md`, in this phase's commit: "What mps uses" as built, each limit as
      "`NAME` (value)"; the 0-symbol text per the GOAL's Clarification; no stale sentence (PLAN § 2.7).
- **Verify:** `sh spec/lucas-lehmer-test/verify/p2.sh`.
- **Touches:** `src/lucas.*`, `src/math.*`, `src/result.*`, `tests/check_lucas.cpp`,
  `tests/check_result.cpp`, `tests/check_math.cpp`, `tests/ll.sh`, `tests/cli.sh`, `Makefile`,
  `AGENTS.md`, `doc/lucas-lehmer.md`; `tests/engine_support.*` only to share a helper.

## Phase P3 — The long tier, the landing gate and the remaining notes
**Satisfies:** R16 (the long tier, both compilers), R17, R18 (the rest) · **Depends on:** P2
**Goal:** `make check-long` runs the long tier within 15 minutes on both compilers;
`ci/engine-gate.sh` and the profile's `landing gate` row exist and pass on this tree; AGENTS.md and
the notes name everything this cycle created.

- [ ] `Makefile`: `check-long` (`sh tests/ll.sh $(EXE) long`), `.PHONY`.
- [ ] `ci/engine-gate.sh` (PLAN § 2.8): run mode and `compare` mode, the guards, exit statuses 0/1/2,
      its header the manual.
- [ ] `AGENTS.md`, in this phase's commit: § Build and run and § Map lines for `make check-long` and
      `ci/engine-gate.sh`; the profile's `| landing gate | … |` row after `cluster test` (R17, R18).
- [ ] `doc/lucas-lehmer.md`, `doc/prp-and-gerbicz.md`, `doc/reference-arithmetic.md`: the three
      sentences say what `make check` runs since `known-answer-tables` (R18).
- **Verify:** `sh spec/lucas-lehmer-test/verify/p3.sh` (its full mode runs the landing gate on a
  commit of the working tree: about 20 of its minutes).
- **Touches:** `Makefile`, `ci/engine-gate.sh`, `AGENTS.md`, the three notes.

## Phase P4 — Audit, deferral ledger, push, the landing gate and the cluster runs' evidence
**Satisfies:** R1–R18 (the audit, and the runs) · **Depends on:** P3
**Goal:** the final tree passes every gate at once, every deferral has a home, the landing gate is
green on the pushed head, and the pushed head passes `make check` on the three cluster targets,
recorded as redacted evidence; `## Times` quotes R16's two times from dispatch A's pre-push run.

**Graded by the orchestrator** for the ledger, the times, the landing gate's quote and the cluster
runs. The evidence lives in `spec/lucas-lehmer-test/evidence/`, which the blind reviewer cannot see.

**Two dispatches, and the foreman submits between them.** The builder never calls a tool of any
cluster MCP server, read-only included, and never submits a job: the foreman alone submits, under the
effort's approved job class, and reads the results.

- **Dispatch A** (audit, ledger notes, pre-push, push; stops before any cluster job): the audit and any
  fix it commits; the three Notes bullets below; `sh spec/lucas-lehmer-test/verify/prepush.sh` (the
  bullets, the code committed, the scan, the full P3 gate on the final tree with its file list graded
  as of P4, then `ci/engine-gate.sh` on HEAD; its last two lines give R16's two times and the landing
  gate's green line); the push. Stop with the phase still `in_progress`, reporting the pushed commit
  and prepush's last two lines verbatim.
- **The foreman** submits the pushed head on the three targets and, once every job has finished, hands
  dispatch B the job ids, `sacct`'s state and exit code for each, and each run's files.
- **Dispatch B** (evidence, ledger, gate, close): the evidence and its redaction;
  `evidence/ledger.md`, quoting dispatch A's prepush lines (no second run for them);
  `sh spec/lucas-lehmer-test/verify/p4.sh audit` until green (about 2 minutes), then the full
  `sh spec/lucas-lehmer-test/verify/p4.sh`; the phase commit.

**Order matters.** The tested commit's code must equal `HEAD`'s outside `spec/`, `issues/` and
`ROADMAP.md`. So any code the audit changes is committed first, as a P4 build commit with the phase
still `in_progress`; then the pre-push checks and the push; the runs; last the phase commit with the
evidence, the ledger and the state. The push target is the one branch `PUSH_REF` names
(`origin/feature/lucas-lehmer-test`, `verify/common.sh`), pushed as a fast-forward.

- [ ] **Audit.** Walk R1–R18 against `src/`, `tests/`, `doc/`, `ci/`, `AGENTS.md` and the Makefile, one
      by one, and confirm each has its check or its gate step (PLAN § 2.9). Fix and commit anything
      found (above).
- [ ] **Deferral ledger.** Walk P1–P4 of this file and the build's own commits for "do not fix here",
      "known limitation", "left as a follow-up" and any deferral the build introduced. Each gets an
      `issues/<slug>.md` from the ISSUE template and a `ROADMAP.md` entry, or a Notes bullet in an
      existing seed, per `AGENTS.md` § Factory. The three this plan already knows, each a Notes bullet
      with the words "deferred from `lucas-lehmer-test`" (the ledger check reads them), written in
      dispatch A **without any measured time** (a time comes from a run after them, and lives in the
      ledger):
      - `issues/large-gcd-and-jacobi.md` — the Jacobi check runs the quadratic binary symbol at
        `GOOD_STATES` points per run; at the top of the range each is a long computation, so the
        schedule there waits on the subquadratic symbol;
      - `issues/checkpoint-and-resume.md` — the good states are held in memory (iteration, shift,
        residue, length), a failed check redoes up to 1/`GOOD_STATES` of the run, and the file persists
        the same state;
      - `issues/engine-differential-standing-check.md` — the gate as built: `ci/engine-gate.sh`, the
        profile's `landing gate` row, what it compares; its time on this Mac is in this cycle's ledger.
      Every `issues/*.md` matches exactly one `**Seed:**` line in `ROADMAP.md`; this cycle's seed stays
      `adopted:lucas-lehmer-test`.
- [ ] **Pre-push.** `sh spec/lucas-lehmer-test/verify/prepush.sh` must be green (about 60 minutes).
      `prepush.sh scan` runs the scan alone; see it catch a planted site name once and a planted
      detailed line naming a made-up node once, then remove both plants.
- [ ] **Push.** `git push origin feature/lucas-lehmer-test`, fast-forward only (the foreman authorizes it
      for this phase). Never force. The push ends dispatch A: report the pushed commit, its
      `ci/test-commit.sh` hash (`git hash-object`), and prepush's last two lines. The foreman submits
      the three runs (`x86-64-avx512`, `x86-64-avx2`, `aarch64-gb10`) from that commit's
      `ci/test-commit.sh`, as its header says, and resubmits a run that fails for a reason outside the
      tree. Only runs that test the tree are recorded.
- [ ] **Evidence** (dispatch B). Pass each piece through `sh spec/lucas-lehmer-test/verify/redact.sh IN
      OUT`: for each target `evidence/<target>.json`, `.out` and `.check.log`; and `evidence/jobs.tsv`
      (a header `label job_id state exit_code` and one tab-separated line per job from `sacct`).
- [ ] **Ledger** (dispatch B): `spec/lucas-lehmer-test/evidence/ledger.md` with `## Deferrals` (each
      seed above, and any the walk found, with its home), `## Times` (``- make check: <prepush's timing
      sentence>`` and ``- make check-long: <s> s on clang++ (budget 900 s)``, from dispatch A's prepush
      line; then one line per target, ``- `<target>`: check step <s> s``, from its `result.json`), and
      `## Landing gate` (``- engine-gate: green on <hash>: …``, prepush's last line verbatim).
- **Verify:** `sh spec/lucas-lehmer-test/verify/p4.sh` (`… p4.sh audit` runs P4's own checks without
  the cumulative gate, for a fast first look).
- **Touches:** `spec/lucas-lehmer-test/evidence/*`, `issues/large-gcd-and-jacobi.md`,
  `issues/checkpoint-and-resume.md`, `issues/engine-differential-standing-check.md`, `ROADMAP.md`
  (only if a new seed is filed); `src/`, `tests/`, `doc/`, `ci/` only if the audit finds a gap.

---

## How `code-build` drives this

1. `next_phase.py` prints the next actionable phase (statuses are authoritative; the
   `current_phase` pointer is reconciled against them).
2. Pre-flight: the cycle worktree is clean, on `branch`, and `base` is reachable.
3. Execute every `[ ]` in the phase (consult `PLAN.md` / `research/` for detail).
4. Run the phase's `verify:` command — never advance on a checkbox alone.
5. Amend this file freely if reality diverges (regenerate frontmatter with `set_phase.py`; note the
   amendment in the commit body). STOP and escalate only on a **`GOAL.md` contradiction**, or a gate
   that rejects something correct.
6. Mark the phase `done`, advance `current_phase`, `--touch`; one `[{category}]` commit; stop and report.
