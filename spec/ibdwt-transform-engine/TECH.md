---
slug: ibdwt-transform-engine
title: Squaring modulo 2^p − 1 with a weighted transform (FP64, portable)
kind: feature
appetite: big
status: in_progress
branch: feature/ibdwt-transform-engine
base: main
current_phase: P4
last_updated: '2026-10-31'
phases:
- id: P1
  name: The transform, the length table and its measurement
  status: done
  satisfies:
  - R2
  - R3
  - R8
  - R10
  - R11
  - R13
  depends_on: []
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/ibdwt-transform-engine/verify/p1.sh
- id: P2
  name: 'Residues: the digit form, the squaring and its subtraction, the product, the monitor'
  status: done
  satisfies:
  - R1
  - R4
  - R6
  - R7
  - R9
  - R13
  - R14
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/ibdwt-transform-engine/verify/p2.sh
- id: P3
  name: Squaring chains against the known answers, and the two tiers' budgets
  status: done
  satisfies:
  - R5
  - R9
  - R12
  depends_on:
  - P2
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/ibdwt-transform-engine/verify/p3.sh
- id: P4
  name: Audit, deferral ledger, push, and the cluster runs' evidence
  status: in_progress
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
  depends_on:
  - P3
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/ibdwt-transform-engine/remediation/p4.sh
review:
  last_reviewed_commit: ''
  verdict: none
  blocked_reason: ''
  cycle: 0
---

# TECH.md — Squaring modulo 2^p − 1 with a weighted transform (FP64, portable)

The **context engine and finite-state machine** for building this feature. The YAML
frontmatter above is the resume ground-truth (read it from the cycle worktree with
`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/ibdwt-transform-engine/TECH.md`);
the per-phase checklists below are the work. `code-build` executes the next actionable phase, runs
its `verify:` command, updates state via
`uv run --with pyyaml python "$KB/bin/factory/set_phase.py" …`, and makes one atomic code+state
commit.

- **Vision / requirements (locked):** [`GOAL.md`](GOAL.md) — R-IDs are the contract.
- **Authoritative design:** [`PLAN.md`](PLAN.md); § 2 is the design and what it pins, § 6 the gates.
- **Backing research:** [`research/00-digest.md`](research/00-digest.md) and six briefs.

## Frontmatter field reference

- `status` (top): `planned | in_progress | blocked | in_review | done` (`done` is stamped by
  `code-publish` after confirmation, just before landing — the terminal state of the retained record)
- `appetite`: `small | big` — caps phase count and build-iteration budget (circuit breaker).
- `branch`, `base`: the cycle's branch (prefix per GOAL `kind`, from the profile's `branches`) and the
  profile's `base`.
- phase `status`: `pending | in_progress | done | blocked`
- `satisfies`: GOAL R-IDs this phase delivers (traceability anchor for `code-review`).
  **Artifact-deliverable R-IDs.** Some requirements are met by a *committed document* — a research
  audit, a protocol doc, an assessment — not by CLI-observable behaviour. Attach such an R-ID to the
  phase that produces and commits the artifact, and give that phase a content `verify:`
  (`test -f …`, a `grep -q` for the sections that must exist) rather than forcing a CLI drive.
  **Say so in the phase body**, because it changes who can grade it: `code-review` blinds its reviewer
  to all of `spec/`, so an R-ID whose evidence lives there is graded by the *orchestrator*, not the
  blind reviewer. Naming it in `TECH.md` is what lets the review skill route it correctly instead of
  reporting an unverifiable requirement. Here R13's documents (AGENTS.md, the note) live outside
  `spec/`, so the blind reviewer grades them; only P4's ledger and cluster evidence are the
  orchestrator's.
- `depends_on`: phase ids that must be `done` first (a phase is actionable only when met).
- `parallel`: `true` only for genuinely independent, non-coupled work (docs, tests, isolated leaf
  modules). Any phase touching the profile's `coupled core` is always `false`. False throughout:
  every phase edits a high-blast-radius engine path.
- `hammerable`: `false` marks a correctness/security phase that scope-hammering must **never** cut.
  False throughout: arithmetic is checked, not trusted.
- `hill`: `uphill` (still figuring it out) → `crest` (unknowns resolved) → `downhill` (just
  executing). A phase stuck `uphill` across builds is a raised hand → escalate to the human.
- `attempts`: durable failed-verify counter (absent = 0), bumped by `set_phase.py --phase P<n>
  --record-attempt` on every red verify gate; `next_phase.py` warns at ≥3 — the circuit breaker
  runs on this file, not on session memory.
- `verify`: the exact command that proves the phase (prefer driving the real CLI, not just tests —
  wrap CLI drives in the profile's `throwaway environment` helper, `<helper> sh -c "…"`, so they hit
  a disposable instance, never the developer's real data). A gate carrying `$( )` or quoting the
  calling shell would mangle is set with `set_phase.py --verify-file PATH`, not `--verify`.
- `review.cycle`: completed review passes, auto-incremented by every `set_phase.py --verdict …`;
  REVIEW.md's "Cycle {n}" mirrors it and the ≤3-cycle bound is graded against it. `--verdict approved`
  also clears `review.blocked_reason` unless one is passed explicitly.

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

- **The human gate.** § Invariants 2 and the engine's paths (high-blast-radius from P1's commit): a
  CONFIRMED review finding in `src/fft.*`, `src/ibdwt.*`, `src/lengths.*`, `src/lengths.txt` or
  `src/measure.cpp` goes to Geoffrey before the cycle lands.
- **What is pinned** (PLAN § 2.8): the declarations, the definitions' first lines, the internal
  pieces kept out of every header, the check labels and their tables, every check running every
  case after a failure and grading the full residue, the tally lines (printed passing or failing)
  and their text, the measurement's format and `bin/measure --probe`, the Makefile's
  `check-engine` recipe and `lengths` target, the note's required names and exempt numbers, the two
  reference keys, the AGENTS.md paths. The gates build against them, wrap them and trace them.
  Anything else is the builder's.
- **The gates are fixed.** `spec/ibdwt-transform-engine/verify/` is reviewed as of the plan commit,
  and each gate refuses to run if it differs. Never edit it. If a gate rejects something correct,
  stop and escalate with the gate's message.
- **Iterate on `quick`, finish on full.** `sh …/p<n>.sh quick` (about 3 minutes: clang only, no
  engine tier) while building; the phase's `verify:` is the full gate (about 8 minutes in P1, 36 to 47
  from P2). **Run one gate at a time, never two together, and nothing heavy beside it:**
  at its peak a gate runs two jobs (PLAN § 6, footprint).
- **What each gate prints.** `GATE FAIL: <reason>` and exit 1 at the first failure, or a
  `P<n> gate: green (…)` line. Run a gate with the phase's files in the working tree: it reads them
  as a clone would hold them, untracked files included.
- **What the gates need.** This Mac: clang++, g++-16, GNU make 3.81 at `/usr/bin/make`, `python3`,
  perl, rsync, git, pgrep, otool.
- **Deletion.** `rm` at the shell is wrapped and refuses; use `del` (no flags) for any scratch
  deletion, and never a bypass. The Makefile's `/bin/rm` is the repo's convention.
- **The clean room** (§ Invariants 4). Build from the papers PLAN cites and the repo's notes; never
  open the source of Prime95/mprime, Mlucas, gpuOwl/PRPLL, GMP, FFTW or any other big-number or FFT
  program. Record any new source read in the commit body.
- **No R-/P-ids in source** (§ Conventions); comments cite the paper (author, title, year).
- **Budgets** (R12): `make check` with nothing to rebuild at most 5 s over the base's (`0bc49cb`, whose code is
  `f956a7c`'s: 3.754 s) on this Mac (the gates measure it interleaved); `make check-engine` at most 15 minutes.

---

## Phase P1 — The transform, the length table and its measurement
**Satisfies:** R2, R3, R8, R10, R11, R13 · **Depends on:** —
**Goal:** the weighted transform's pieces exist and match their definitions at every odd factor;
the length table is set by the builder's own measurement, committed beside it with its command,
and `make check` holds the one to the other; `Engine` refuses what the table doesn't hold; the
engine tier exists; AGENTS.md and the note name everything this commit creates.

- [x] `src/fft.hpp`, `src/fft.cpp` (PLAN § 2.2.3): `fft::Complex`, `LARGEST_ODD_FACTOR`,
      `is_supported`, `root`, `Plan` (forward, unnormalized inverse), `fft_error`; Cooley–Tukey
      1965 and Percival 2003 cited.
- [x] `src/ibdwt.hpp`, `src/ibdwt.cpp` (§ 2.2.1–2.2.6): `engine_error`, `roundoff_error`,
      `THRESHOLD`, `MAGNITUDE_LIMIT`, `MAX_WIDTH`, `positions`, `weights`, `inverse_weights`,
      `RMS_MARGIN`, `Transform`, `Engine` (refusing through `require_held` and a threshold outside
      (0, 0.5); `rms_limit` from `rms_envelope`), `Residue`, `require_residue`, `roundoff` (NaN on a
      non-finite output), `Statistics`/`sample`; internal, in no header: `convolve`,
      `convolve_square`, `carry`, `rms`; the pinned first lines of § 2.8 exactly.
- [x] `src/lengths.hpp`, `src/lengths.cpp`: `LARGEST_LENGTH`, `SIZING_DISTANCE`,
      `SIZING_MAGNITUDE`, the protocol constants (`PROBE_SQUARINGS`, `PROBE_OUTPUTS`, `GRID_STEPS`,
      `START_BELOW`, `BACKOFF`), `is_supported`, `supported_lengths` (enumerated as m·2^k, not a
      scan of 2^20 integers; amended, see below), `largest_exponent`, `holds`, `require_held`, `length_for`, `rms_envelope`; the
      table as the two maps `largest_exponent_by_length` and `rms_envelope_by_length`.
- [x] `src/measure.cpp` (§ 2.4): the protocol through `sample`, from the constants, with each
      probe's squarings and outputs in its line; `--table FILE` (both maps, the probe `BACKOFF`
      below the last held); `--probe LENGTH EXPONENT`; SplitMix64 cited (Steele, Lea and Flood
      2014).
- [x] `Makefile`: `MODULES` += `fft ibdwt lengths`; `MEASURE := bin/measure` and its rule;
      `lib/measure.d` in `DEPS`; `$(MEASURE)` in `BUILT`; `check-engine` (§ 2.5, verbatim shape);
      `lengths`; both `.PHONY`. Nothing else in the Makefile changes.
- [x] **Measure.** `make lengths` on this Mac with clang (about 42 minutes serial; or up to three
      `bin/measure <lengths…>` processes, about 16 minutes, concatenated in length order with the
      header and trailer `make lengths` writes — byte-identical). Commit `src/lengths.txt` and paste
      the printed rows of both maps into `src/lengths.cpp`. Check the maxima ascend (every length
      assigned an exponent) and P_max ≥ 10,000,019; if not, stop and escalate (a GOAL question).
      Remeasure whenever a later change touches the convolution, the weights, the transform,
      `roundoff`, `rms` or the protocol: the gate's byte comparison and its own measurement will say
      so.
- [x] `tests/check.hpp`, `tests/check.cpp`: `check::Tier`, `Suite(Tier, …)`, `bin/check engine`,
      usage exit 2.
- [x] `tests/engine_support.hpp`, `tests/engine_support.cpp`: the shared helpers (pseudorandom
      residues, `BigInt` expectations, the digit-vector value by placing bits, assigned exponents,
      next length, `Tally` with its line printed passing or failing (PLAN § 2.8), the
      table-against-measurement check).
- [x] `tests/check_ibdwt.cpp`: the six P1 labels of § 2.6. `tests/check_engine.cpp`: the P1
      engine-tier label.
- [x] `AGENTS.md`: the four high-blast-radius lines, the § Map bullets, the two § Build and run
      lines (§ 2.7) — in this phase's commit (R13).
- [x] `doc/weighted-transforms.md`: "What mps uses" for the transform, the carry, the monitor and
      the lengths (§ 2.7; no measured or table value; 7/16 for the threshold; the exempt list);
      Sources and Provenance; `doc/references.md`: `CooleyTukey1965`, `SteeleLeaFlood2014`, each
      checked against its DOI's metadata, the Verified line saying what was read.
- **Verify:** `sh spec/ibdwt-transform-engine/verify/p1.sh` (`… p1.sh quick` while iterating).
- **Touches:** `src/fft.*`, `src/ibdwt.*`, `src/lengths.*`, `src/lengths.txt`, `src/measure.cpp`,
  `Makefile`, `tests/check.*`, `tests/check_ibdwt.cpp`, `tests/check_engine.cpp`,
  `tests/engine_support.*`, `AGENTS.md`, `doc/weighted-transforms.md`, `doc/references.md`.
- **Amendments at the build (2026-10-31):**
  - `is_supported` and `supported_lengths` are arithmetic (odd factor at most 15, at most
    `LARGEST_LENGTH`), not read from the table: the measurement has to name the lengths before a
    table exists. Neither scans 2^20 integers; `lengths.py` and the table check hold the table's
    rows to exactly those lengths.
  - The protocol's start: the Crandall–Mayer–Papadopoulos estimate is taken at N = 16 for lengths
    below 16, where its logarithms (log₂ log₂ N, log₂ ln ln N) are undefined or negative. It only
    picks where a length's probes start; the restart rule covers a start that is too high.
  - `bin/measure` with no length writes the whole file `make lengths` keeps (header, every length,
    trailer); with lengths it prints their probe lines alone. `make lengths` is serial; the
    three-process split was not used.
  - `Transform` and `Engine` also expose `exponent()` and `length()` (the gates' interposer calls
    them); the checks declare the internal pieces in `tests/engine_support.hpp`, a test header,
    never a header under `src/`.

## Phase P2 — Residues: the digit form, the squaring and its subtraction, the product, the monitor
**Satisfies:** R1, R4, R6, R7, R9, R13, R14 · **Depends on:** P1
**Goal:** residues convert to and from balanced digits and are squared, squared less 2^b at any
bit, and multiplied, each returning its distance and refusing on the monitor's four grounds with
everything left as it was; structured inputs refused or squared right; checked against `BigInt` on a sample in `make check` and at
every supported length in the engine tier.

- [x] `src/ibdwt.*`: `to_digits`, `from_digits`, `square`, `square_subtract`, `multiply` (§ 2.2.8,
      § 2.3); the monitor in its order — magnitude, distance, `checksums_agree` (internal, § 2.2.6
      (c)), `rms` past `rms_limit` — each a `roundoff_error` leaving residue and operands untouched;
      `engine_error` for more than p bits, a bit at p or above, digits that are not the engine's; the
      pinned first lines of § 2.8.
- [x] `tests/check_ibdwt.cpp`: the five P2 labels; the refusal check gains the P2 refusals.
      `tests/check_engine.cpp`: the six P2 labels, the squaring and product checks printing their
      residue digests. Every differential check grades the full `BigInt`; the monitor and structured
      checks compute the outputs' distance and rms themselves, never through `roundoff` or `rms`.
- [x] `doc/weighted-transforms.md`: the operations by name, the threshold 7/16, the four refusals,
      the checksums' identity, the envelope, and what the sizing covers (§ 2.7).
- [x] If anything the measurement depends on changed, remeasure (P1's item) in this commit.
      Nothing it depends on changed (the convolutions, the weights, the transform, `roundoff`,
      `rms` and the protocol are P1's, untouched), so the table was not remeasured.
- **Verify:** `sh spec/ibdwt-transform-engine/verify/p2.sh`.
- **Touches:** `src/ibdwt.*`, `tests/check_ibdwt.cpp`, `tests/check_engine.cpp`,
  `tests/engine_support.*`, `doc/weighted-transforms.md`.
- **Amendments at the build (2026-10-31):**
  - Outputs come out exact, with no distance to refuse below, wherever every weight is 1 and
    every root exact: lengths 1 and 2, and length 4 when it divides p (exponent 72 at length 4
    in the default tier). The monitor checks count all such cases rather than only N = 1, 2.
  - The checks' bodies for the residues and the operations live once in
    `tests/engine_support.cpp` (`expect_round_trips`, `expect_squarings`, `expect_products`,
    `expect_monitor`, `expect_repeats`, `expect_structured`), each tier calling them over its
    own lengths; the default tier's sample lengths are each odd m at 2^0, 2^1, 2^2 and 2^10
    words for all five P2 labels.
  - Every case of a round-trip tally passes through both `to_digits` and `from_digits`, so the
    digit-0 and the readback sabotage each make every case disagree (W = K).
  - `to_digits` balances its plain words through `carry`, the one carry the gates wrap;
    `from_digits` has its own plain-digit carry, since its digits may pass what a double holds.

## Phase P3 — Squaring chains against the known answers, and the two tiers' budgets
**Satisfies:** R5, R9, R12 · **Depends on:** P2
**Goal:** chains on the engine reproduce every partial row up to P_max, every final row up to
100,069 and the zero residues of the Mersenne prime exponents up to 100,000, each row also at the
next length and shifted; both tiers within R12's budgets; `tests/cli.sh` untouched.

- [x] `tests/engine_support.*`: the shifted chain (the stored s·2^k, the subtraction at
      (2k + 1) mod p, k doubled; rotated back on `BigInt`).
- [x] `tests/check_ibdwt.cpp`: `shifted chains reproduce the partial residues`.
      `tests/check_engine.cpp`: the three chain labels, digests printed; prp1's division by 9 on
      `BigInt`; expected values read from `tests/known/` through `src/known.*`.
- [x] Times on this Mac (the gate measures them): `make check` ≤ +5 s, `make check-engine` ≤ 900 s.
      If a chain trips R7, do not touch the threshold: raise the measurement's outputs per probe,
      remeasure, and record why in the commit body.
- **Verify:** `sh spec/ibdwt-transform-engine/verify/p3.sh`.
- **Touches:** `tests/check_ibdwt.cpp`, `tests/check_engine.cpp`, `tests/engine_support.*`; `src/`
  only if a chain finds a defect (then remeasure if the arithmetic changed).
- **Amendments at the build (2026-10-31):**
  - The chain and its runs live once in `tests/engine_support.*`: `chain` (one run, rotated back
    on `BigInt`), `Chain` (a row's start, steps, kind and grading), `partial_chains` (the partial
    rows under an exponent and an iteration limit) and `expect_chains` (every run of every chain,
    one tally per check: the table's length, the next length, and shifted by p/3 + 1 at the
    table's length; the zeros at the table's length only). The default tier calls it with
    p ≤ 10,303 and k ≤ 1,000, the engine tier with p ≤ P_max and every k; the final rows and the
    zeros are built in `tests/check_engine.cpp`, prp1's division by 9 there on `BigInt`.
  - Superseded rows are skipped (none exist); fingerprint rows, of which none exist, would be
    graded like any other row.
  - `doc/weighted-transforms.md` gains a clause naming the chains among what `make check-engine`
    runs (no number added), so the note's account of the engine tier stays whole; the file is on
    PLAN § 2.1's list.
  - No chain tripped the monitor: no remeasurement, the table as P1 measured it.

## Phase P4 — Audit, deferral ledger, push, and the cluster runs' evidence
**Satisfies:** R1–R14 (the audit, and the runs) · **Depends on:** P3
**Goal:** the final tree passes every gate at once, every deferral has a home, and the pushed head
passes `make check` on the three cluster targets, recorded as redacted evidence; `## Times` quotes
R12's two times from dispatch A's pre-push run.

**Graded by the orchestrator** for the ledger, the times and the cluster runs. The evidence lives
in `spec/ibdwt-transform-engine/evidence/`, which the blind reviewer cannot see.

**Two dispatches, and the foreman submits between them.** The builder never calls a tool of any
cluster MCP server, read-only included, and never submits a job: the foreman alone submits, under
the effort's approved job class, and reads the results.

- **Dispatch A** (audit, ledger, pre-push, push; stops before any cluster job): the audit and any
  fix it commits; the ledger's `## Deferrals`; `sh spec/ibdwt-transform-engine/verify/prepush.sh`
  (the ledger's bullets, the code committed, the scan, then the full P3 gate on the final tree; its
  last line gives R12's two times); the push. Stop with the phase still `in_progress`, reporting
  the pushed commit and prepush's times line verbatim.
- **The foreman** submits the pushed head on the three targets and, once every job has finished,
  hands dispatch B the job ids, `sacct`'s state and exit code for each, and each run's files.
- **Dispatch B** (evidence, gate, close): the evidence and its redaction; `## Times`, quoting
  dispatch A's prepush times line on the same code (no second full gate run for it);
  `sh spec/ibdwt-transform-engine/verify/p4.sh audit` until green (about 2 minutes), then the full
  `sh spec/ibdwt-transform-engine/verify/p4.sh`; the phase commit.

**Order matters.** The tested commit's code must equal `HEAD`'s outside `spec/`, `issues/` and
`ROADMAP.md`. So any code the audit changes is committed first, as a P4 build commit with the phase
still `in_progress`; then the pre-push checks and the push; the runs; last the phase commit with the
evidence, the ledger and the state.

- [x] **Audit.** Walk R1–R14 against `src/`, `tests/`, `doc/`, `AGENTS.md` and the Makefile, one by
      one, and confirm each has its check or its gate step (PLAN § 2.9). Fix and commit anything
      found (above).
- [x] **Deferral ledger:** `spec/ibdwt-transform-engine/evidence/ledger.md` with `## Deferrals` and
      `## Times`.
  - [x] Walk P1–P4 of this file and the build's own commits for "do not fix here", "known
        limitation", "left as a follow-up" and any deferral the build introduced. Each gets an
        `issues/<slug>.md` from the ISSUE template and a `ROADMAP.md` entry, or a Notes bullet in an
        existing seed, per `AGENTS.md` § Factory; list each under `## Deferrals` with its home.
  - [x] Notes bullets, each with the words "deferred from `ibdwt-transform-engine`" (the ledger
        check looks for them):
        `issues/large-gcd-and-jacobi.md` — the engine multiplies modulo M_p only (R14); the
        half-GCD's plain, zero-padded, unweighted product of operands of varying sizes is not in it,
        so ROADMAP Part VIII's "depends on the transform engine's general multiply" is half true:
        that product is still to build;
        `issues/cpu-transform-throughput.md` — the real-signal (or right-angle) transform that halves
        the work, left out for one code path (PLAN § 3);
        `issues/engine-differential-standing-check.md` — the suite is `make check-engine`, its time
        on this Mac at P4, and what it runs.
  - [x] Every `issues/*.md` matches exactly one `**Seed:**` line in `ROADMAP.md`; this cycle's seed
        stays `adopted:ibdwt-transform-engine`.
- [x] **Pre-push.** `sh spec/ibdwt-transform-engine/verify/prepush.sh` must be green: the ledger's
      bullets, the no-uncommitted-code rule and the scan first, then the P3 gate on the final tree.
      `prepush.sh scan` runs the scan alone; see it catch a planted site name once and a planted
      detailed line naming a made-up node once, then remove both plants.
- [ ] **Push.** `git push origin feature/ibdwt-transform-engine`, fast-forward only (the foreman
      authorizes it for this phase). Never force. The push ends dispatch A: report the pushed
      commit, its `ci/test-commit.sh` hash (`git hash-object`), and the prepush times line; the
      foreman submits the three runs
      (`x86-64-avx512`, `x86-64-avx2`, `aarch64-gb10`) from that commit's `ci/test-commit.sh`, as
      its header says, and resubmits a run that fails for a reason outside the tree (a preemption,
      a node fault). Only runs that test the tree are recorded.
- [x] **Evidence** (dispatch B, from the files the foreman hands over). Pass each piece through
      `sh spec/ibdwt-transform-engine/verify/redact.sh IN OUT`: for each target
      `evidence/<target>.json`, `evidence/<target>.out` and `evidence/<target>.check.log`; and
      `evidence/jobs.tsv` (a header `label job_id state exit_code` and one tab-separated line per
      job from `sacct`).
- [x] **Times** (R12's record): under `## Times` in `evidence/ledger.md`, two lines quoted from
      dispatch A's prepush times line: ``- make check: <the timing sentence>`` (the interleaved
      comparison against 0bc49cb) and ``- make check-engine: <s> s on clang++ (budget 900 s)``; then
      one line per target, ``- `<target>`: check step <s> s``, from the run's `result.json`.
- **Verify:** `sh spec/ibdwt-transform-engine/verify/p4.sh` (`… p4.sh audit` runs P4's own checks
  without the cumulative gate, for a fast first look).
- **Amendment (foreman ruling, 2026-10-31): the gate remediation.** The frozen `verify/prepush.sh`
  and `verify/p4.sh` run the cumulative gate at `PHASE=3`, so `files.py changes` refuses the
  `issues/` edits the ledger check requires, and no correct tree passes both. P4 runs
  `remediation/prepush.sh` in place of `verify/prepush.sh`, and its `verify:` is
  `remediation/p4.sh`: the frozen scripts with two edits only (they source the frozen `verify/`,
  and the file check grades what changed as of phase 4, derived from the frozen `gate.sh` at run
  time). `sh spec/ibdwt-transform-engine/remediation/make.sh check` proves that, and both scripts
  run it first. Every other check stays at phase 3. `verify/` is untouched.
- **Touches:** `spec/ibdwt-transform-engine/evidence/*`, `issues/large-gcd-and-jacobi.md`,
  `issues/cpu-transform-throughput.md`, `issues/engine-differential-standing-check.md`,
  `ROADMAP.md` (only if a new seed is filed); `src/`, `tests/` and `doc/` only if the audit finds a
  gap.

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
