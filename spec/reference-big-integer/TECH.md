---
slug: reference-big-integer
title: A reference big integer to check every engine against
kind: feature
appetite: big
status: done
branch: feature/reference-big-integer
base: main
current_phase: done
last_updated: '2026-10-16'
phases:
- id: P1
  name: The type, sums, differences, shifts, folding, Res64, hexadecimal and the vector
    framework
  status: done
  satisfies:
  - R1
  - R2
  - R3
  - R4
  - R5
  - R8
  - R9
  - R10
  - R11
  - R12
  depends_on: []
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/reference-big-integer/verify/p1.sh
- id: P2
  name: Schoolbook and Karatsuba products and squares
  status: done
  satisfies:
  - R1
  - R8
  - R9
  - R10
  - R11
  - R12
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/reference-big-integer/verify/p2.sh
- id: P3
  name: Division, general reduction, modular powers and decimal conversion
  status: done
  satisfies:
  - R2
  - R3
  - R5
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
  verify: sh spec/reference-big-integer/verify/p3.sh
- id: P4
  name: The reference Lucas-Lehmer and PRP tests and the prime list
  status: done
  satisfies:
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
  verify: sh spec/reference-big-integer/verify/p4.sh
- id: P5
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
  - P4
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/reference-big-integer/verify/p5.sh && python3 -I -B spec/reference-big-integer/remediation/f1-markers.py
review:
  last_reviewed_commit: 4b49238dcd19fa274334427cca4bf87b3cdf3cbd
  verdict: approved
  blocked_reason: ''
  cycle: 5
---
# TECH.md — A reference big integer to check every engine against

The **context engine and finite-state machine** for this cycle. The YAML frontmatter above is the
resume ground truth. Read it from the cycle worktree with
`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/reference-big-integer/TECH.md`.
The per-phase checklists below are the work.

`code-build` does four things per phase:

- executes the next actionable phase;
- runs its `verify:` command;
- updates the state with `uv run --with pyyaml python "$KB/bin/factory/set_phase.py" …`;
- makes one atomic commit of the work and the state.

Sources:

- **Vision / requirements (locked):** [`GOAL.md`](GOAL.md). The R-IDs are the contract.
- **Authoritative design:** [`PLAN.md`](PLAN.md); § 2 is the interface, the algorithms, the vectors and
  the checks, § 6 the gates.
- **Backing research:** [`research/00-digest.md`](research/00-digest.md) and its four briefs.

## Frontmatter field reference

- `status` (top): `planned | in_progress | blocked | in_review | done`.
- `appetite`: `big`. It caps the phase count and the build-iteration budget (the circuit breaker).
- `branch`, `base`: the cycle's branch and the profile's `base`.
- phase `status`: `pending | in_progress | done | blocked`.
- `satisfies`: the GOAL R-IDs this phase delivers (the traceability anchor for `code-review`). R9–R12
  recur in every build phase because each phase must keep `bin/mps` unchanged, stay within the time
  budget, carry checks for what it adds, and land its algorithms' part of the note.
- `depends_on`: the phase ids that must be `done` first.
- `parallel`: `false` throughout. Every phase edits `src/mp.*` (high-blast-radius), `tests/check_mp.cpp`
  or the vector families and the note, so no two can run at once.
- `hammerable`: `false` throughout. Every phase is a correctness phase: the reference is what every
  engine is graded against.
- `hill`: `uphill` → `crest` → `downhill`.
- `attempts`: the durable failed-verify counter, bumped by
  `set_phase.py --phase P<n> --record-attempt`.
- `verify`: the exact command that proves the phase.
- `review.cycle`: completed review passes.

## Conventions (apply to every phase)

- **`AGENTS.md` is the constitution.** Walk the § Invariants sections PLAN § 3 lists: 2, 3, 4, 5 and
  § Conventions. `src/mp.*`, `src/result.cpp` and `src/reference.*` are on the high-blast-radius list,
  and the vectors join it in P1: a CONFIRMED review finding there needs Geoffrey's decision.
- **One phase per `code-build` invocation.** One atomic commit with the code, the vectors, the note
  and the `TECH.md` state, subject `[feature] Build reference-big-integer P<n>: …`, no trailers.
- **The interface is pinned** (PLAN § 2): the names and signatures, each public free function defined
  once, out of line, at file scope, its name on the definition's first line; `KARATSUBA_THRESHOLD`;
  `Division`; `mp_error`; the file names and stems; the vector format and op table; the check labels,
  verbatim; the canonical strings; the `AGENTS.md` lines; the note's file, sections and headings. The
  gates build against them and wrap the functions. Anything else is the builder's: helpers, loops,
  the division's correction strategy, the generator's internals.
- **The gates are fixed.** `spec/reference-big-integer/verify/` is reviewed as of the plan commit, and
  each gate refuses to run if it differs. Never edit it. If a gate rejects something correct, stop
  and escalate with the gate's message.
- **What the gates need.** This Mac: clang++, g++-16, GNU make 3.81 at `/usr/bin/make`, `python3` and
  `python3.14`, git, rsync, perl, otool. Each takes about 4–6 minutes (P4 and P5 longer: the
  generator's Lucas–Lehmer sweep runs under both Pythons).
- **What each gate prints.** `GATE FAIL: <reason>` and exit 1 at the first failure, or a
  `P<n> gate: green (…)` line. Run a gate with the phase's files in the working tree: it reads them as
  a clone would hold them, untracked files included.
- **The vectors** (PLAN § 2, The vectors): write the generator first, then the files, each with the
  exact command its second line states, run from the repository root. Never edit a vector file by
  hand; a changed expected value is a finding (§ Invariants 3). Families of earlier phases change
  only when the generator's tables gain cases at their end, and `hex` and `decimal` are regenerated
  as families are added, to cover every operand.
- **`make check` reads the vectors at run time.** `bin/check` opens `tests/vectors/<stem>.txt` from
  the repository root; nothing in `make check` runs Python (R9).
- **The clean room** (§ Invariants 4). Never open another big-number or Mersenne program's source,
  nor Rosetta Code or Stack Overflow code. Write every algorithm from the works PLAN § 2 cites, and
  cite them in each function's doc block.
- **The note** (`doc/reference-arithmetic.md`) follows the math-notes conventions: the five sections;
  every number in a check span over `claims.py`'s functions or a cited span; citations as
  `[Key, locator](references.md#key)`; no code in prose. Check it with
  `python3 -I -B spec/math-notes-and-references/verify/notes.py claims doc/reference-arithmetic.md`
  before the gate.
- **Budget.** `make check`, warm, at most 3.0 s longer than at `c9b82ff` on this Mac (R10). Keep the
  clarity-first code; if the time runs short, look at allocation in the hot loops before anything
  clever.

---

## Phase P1 — The type, sums, differences, shifts, folding, Res64, hexadecimal and the vector framework
**Satisfies:** R1 (comparison, sums, differences, shifts), R2 (folding), R3 (hexadecimal), R4, R5
(the P1 errors), R8, R9, R10, R11, R12 (folding, Res64) · **Depends on:** —

**Goal:** `BigInt` holds 64-bit limbs and compares, adds, subtracts, shifts, folds modulo M_p, gives
Res64 and converts to and from hexadecimal. The vector framework (generator, reader, SHA-256,
SplitMix64) exists and checks it. `hex64` lives in `mp.*`. The note starts.

- [x] `src/mp.hpp`, `src/mp.cpp`: replace the decimal stub with PLAN § 2's type, `mp_error`,
      `KARATSUBA_THRESHOLD` and the P1 functions (`compare`, `operator==`, `add`, `subtract`,
      `shift_left`, `shift_right`, `mersenne`, `reduce_mersenne`, `hex64`, `res64`, `to_hex`,
      `from_hex`), each citing its source.
- [x] `src/result.cpp`: delete the file-local `hex64` and include `mp.hpp`; nothing else changes.
- [x] `tests/vectors/generate.py` with the `arith`, `folding` and `hex` families, the sizes of PLAN § 2;
      the three files written by their commands.
- [x] `tests/vector_reader.hpp`, `tests/vector_reader.cpp`: the reader with every `require` of PLAN § 2,
      SHA-256 (FIPS 180-4) and SplitMix64.
- [x] `tests/check_mp.cpp`: the P1 checks, labels verbatim, including `expect_mp_error` and the P1
      error cases; `every vector file carries its command and source` lists the stems present.
- [x] `doc/reference-arithmetic.md` with `### Folding modulo M_p` and `### Res64`; its line in
      `doc/README.md`; Knuth's volume 2 in `doc/references.md` if the note cites it.
- [x] `AGENTS.md`: the `tests/vectors/` line in the high-blast-radius list; the § Map lines for
      `src/mp.*` and `tests/`, as PLAN § 2 gives them.
- **Verify:** `sh spec/reference-big-integer/verify/p1.sh`.
- **Touches:** `src/mp.*`, `src/result.cpp`, `tests/vector_reader.*`, `tests/check_mp.cpp`,
  `tests/vectors/`, `doc/reference-arithmetic.md`, `doc/README.md`, `doc/references.md`, `AGENTS.md`.

## Phase P2 — Schoolbook and Karatsuba products and squares
**Satisfies:** R1 (products and squares), R8–R12 (Karatsuba and schoolbook in the note) ·
**Depends on:** P1

**Goal:** products and squares by schoolbook and by Karatsuba, each checked separately and through
the dispatchers, up to about 2^20 bits.

- [x] `src/mp.*`: `multiply_schoolbook`, `multiply_karatsuba(a, b, cutoff)`, `multiply`,
      `square_schoolbook` (symmetric), `square_karatsuba(a, cutoff)`, `square`, as PLAN § 2 gives them.
- [x] Measure `KARATSUBA_THRESHOLD` on the products as written and set it within 8 to 256 (PLAN § 2).
      P1 landed it at PLAN's starting value, 48, unmeasured, since no product existed yet.
      *Measured in P2: 96.* One split against the schoolbook form breaks even at about 72 limbs for a
      product and 132 for a square; the full recursion, as the mean of time over the best per size on
      11 sizes from 100 to 16384 limbs, products and squares together, is flat at 96–104 on both
      compilers in the checks' build (clang++ 1.034 at 96, 1.031 at 104, 1.198 at 48; g++-16 1.035,
      1.033, 1.206), and 96 is the value taken, as in the prototype.
- [x] The generator's `products` family; `hex` regenerated to cover its operands. The generator reads
      the threshold from `src/mp.hpp` rather than restating it, so the family's sizes follow it.
- [x] `tests/check_mp.cpp`: the six P2 checks, schoolbook and Karatsuba at the sizes and cutoffs PLAN
      § 2 gives; `products` added to the header check's stems.
- [x] The note's `### Schoolbook` and `### Karatsuba`; `KaratsubaOfman1962` in `doc/references.md`,
      checked against the work (retry MathNet.ru; otherwise cite as the bibliography's paywall rule
      allows and say so in its Verified line). MathNet.ru refused connections again on 2026-10-16, so
      the entry is checked against MCA's bibliography entry 136 and cited without a locator.
- **Verify:** `sh spec/reference-big-integer/verify/p2.sh`.
- **Touches:** `src/mp.*`, `tests/check_mp.cpp`, `tests/vectors/`, `doc/reference-arithmetic.md`,
  `doc/references.md`; and, amended in P2, `tests/vector_reader.cpp` (the reader learns the `mul`
  and `sqr` ops and the `products` stem) and `doc/README.md` (the note's line names the products).

## Phase P3 — Division, general reduction, modular powers and decimal conversion
**Satisfies:** R2 (general reduction, powers), R3 (decimal), R5 (the P3 errors), R8–R12 ·
**Depends on:** P2

**Goal:** `divide`, `reduce` and `power_mod`, and decimal conversion both ways, the output divide and
conquer, checked on every vector operand within R10's budget.

- [x] `src/mp.*`: `Division`, `divide` (normalization, quotient-digit estimate and correction, one-limb
      short division), `reduce`, `power_mod`, `to_decimal` (MCA Algorithm 1.26), `from_decimal`.
      *Amended in P3:* the correction is MCA Algorithm 1.6 as the book writes it: the one-limb
      estimate, clamped, subtracted, then added back while the remainder is below zero (at most
      twice, Theorem 1.4.1), so the add-back path runs on about a quarter of random digits. It is
      neither of the two options PLAN § 2 names (compare-before-subtract, or Knuth's refined
      estimate); the choice is the builder's, the vectors and the gate cover all three paths.
      Knuth's refinement measured 0.18 s against 0.22 s for one 2^20-bit `to_decimal`, too little
      for the extra loop. `mp.cpp`'s comments cite Knuth 1997 by section only: P2's "Algorithm M"
      letter is dropped, as research 01 advises.
- [x] The generator's `modular` family, with the clamp, two-correction and add-back cases it asserts by
      simulation, and the `decimal` family; `hex` regenerated. The two-correction case is a seeded
      pair (`r:640:700`, `r:192:701`) found by searching seeds and fixed in the table; `hex` gained
      the modular operands at its end only.
- [x] `tests/check_mp.cpp`: the three P3 checks, `divide`'s quotient identity, the P3 error cases;
      `modular` and `decimal` added to the header check's stems.
      *Amended in P3 (R10):* above 2^17 bits, that is for A and C only, the decimal check stops once
      the parse gives back A: printing the parse is printing A, already compared, and the
      leading-zero parse is checked on every other operand. The two skipped calls cost 0.6 s of
      warm `make check` here. GOAL R3 holds in full; PLAN § 2's extra round trip holds below 2^17
      bits.
- [x] The note's `### Division`, `### Powers` and `### Radix conversion`. `Knuth1997` in
      `doc/references.md` (checked against the author's page; the note cites § 4.3.1 only), which
      also covers the citation P2's code already made.
- **Verify:** `sh spec/reference-big-integer/verify/p3.sh`.
- **Touches:** `src/mp.*`, `tests/check_mp.cpp`, `tests/vectors/`, `doc/reference-arithmetic.md`,
  `doc/references.md`; and, amended in P3, `tests/vector_reader.cpp` (the `mod`, `powmod` and `dec`
  ops and the two stems) and `doc/README.md` (the note's line names division, powers and decimal).
- **Measured (P3 gate):** R10 +1.33 s with clang++ (1.21 s at `c9b82ff`, 2.54 s now) and +1.20 s with
  g++-16 (4.03 s, 5.24 s), against +3.0 s; P3's own share is about +0.7 s, almost all of it the
  two 2^20-bit decimal conversions (0.22 s out and 0.08 s in, each).

## Phase P4 — The reference Lucas-Lehmer and PRP tests and the prime list
**Satisfies:** R5 (the exponents' domain), R6, R7, R8 (the prime list's source), R9–R12 ·
**Depends on:** P3

**Goal:** `reference::lucas_lehmer` and `reference::prp3` (type-1) end in 0 and 1 on every Mersenne
prime exponent from 3 to 9941, reproduce the composite set's residues, and refuse any p outside odd
p ≥ 3. The prime list carries its source.

- [x] `src/reference.hpp`, `src/reference.cpp` (PLAN § 2), and `reference` in the Makefile's `MODULES`,
      exactly as PLAN § 2 gives the line.
- [x] The generator's `reference` and `mersenne-exponents` families; `hex` and `decimal` regenerated
      for the new M_p. M_p of the 28 composite exponents is appended at the end of `every_operand()`,
      so `hex` and `decimal` keep their 294 earlier records line for line and gain 28 (322 each). The
      Lucas-Lehmer residues are computed twice in the generator (folding, and Python's `%`), and its
      Res64 at 1237 and 1277 equals the published values `issues/known-answer-tables.md` records.
      The sweep behind the prime list takes about 150 s under `python3` (3.9) here.
- [x] `tests/check_reference.cpp`: the six P4 checks; `reference` and `mersenne-exponents` added to the
      header check's stems.
- [x] The note's "What mps uses" names the reference tests; `doc/lucas-lehmer.md` and
      `doc/prp-and-gerbicz.md` "What mps uses" as PLAN § 2 gives them (the PRP note says the
      reference returns the type-1 residue, R7).
- [x] `AGENTS.md`: the two list lines and the § Map line for `src/reference.*`.
- **Verify:** `sh spec/reference-big-integer/verify/p4.sh`.
- **Touches:** `src/reference.*`, `Makefile`, `tests/check_reference.cpp`, `tests/check_mp.cpp`,
  `tests/vectors/`, `doc/`, `AGENTS.md`; and, amended in P4, `tests/vector_reader.cpp`, not
  `tests/check_mp.cpp`: the header check iterates the reader's `STEMS`, so the two stems join that
  list, and the reader learns the `ll`, `prp` and `mersenne` ops, with a full-value field kind for the
  residues, which PLAN § 2 writes as `x:` at any length. `doc/README.md`'s line for the note names
  the reference tests too.
- **Measured (P4 gate):** R10 +2.40 s with clang++ (1.17 s at `c9b82ff`, 3.57 s now) and +2.28 s with
  g++-16 (4.00 s, 6.28 s), against +3.0 s; P4's own share is about +1.07 s. No lever was needed:
  the reference tests stay at PLAN's B and composite set, and both square with `square` and reduce
  with `reduce_mersenne`. `make check` runs 81 checks (75 before). The gate took about 12.5 minutes.

## Phase P5 — Audit, deferral ledger, push and cluster runs
**Satisfies:** R1–R12 (the audit, and the runs) · **Depends on:** P4

**Goal:** the final tree passes every gate at once, every deferral has a home, and the pushed head
passes `make check` on the three cluster targets, recorded as redacted evidence with each run's
check-step time.

**Graded by the orchestrator** for the ledger and the cluster runs. The evidence lives in
`spec/reference-big-integer/evidence/`, which the blind reviewer cannot see. The orchestrator grades it
from the job ids, `sacct` and the session's MCP transcript.

**Order matters.** The tested commit's code must equal `HEAD`'s outside `spec/`, `issues/` and
`ROADMAP.md`. So:

1. Any code the audit changes is committed first, as
   `[feature] Build reference-big-integer P5: <what>`, with the phase still `in_progress`.
2. Then come the pre-push checks, the push and the runs.
3. Last comes the phase commit with the evidence, the ledger and the state.

- [x] **Audit.** Walk R1–R12 against `src/`, `tests/`, `doc/` and `AGENTS.md`, one by one, and confirm
      each R1–R7 has its own check and each check one condition per `expect`. Fix and commit anything
      found (above).
- [x] **Deferral ledger:** `spec/reference-big-integer/evidence/ledger.md` with `## Deferrals` and
      `## Times`.
  - [x] Walk P1–P5 of this file, and the build's own commits, for "do not fix here", "known
        limitation", "left as a follow-up" and any deferral the build introduced. Each gets an
        `issues/<slug>.md` from the ISSUE template and a `ROADMAP.md` entry, or a Notes bullet in an
        existing seed, per `AGENTS.md` § Factory; list each under `## Deferrals` with its home.
  - [x] **The balanced digit form** (GOAL, Non-goals): add a Notes bullet to
        `issues/ibdwt-transform-engine.md` saying that the balanced variable-base digit form and its
        round-trip with `BigInt` are that cycle's, in the words "deferred from
        `reference-big-integer`" (the ledger check looks for them), and that it
        builds them on `BigInt::from_limbs` and `limbs()`; and name the balanced digit form in its
        `ROADMAP.md` entry ("Squaring modulo 2^p − 1 with a weighted transform").
  - [x] **The prime list** (GOAL, Notes): add a Notes bullet to `issues/known-answer-tables.md` that the
        exponents up to 10000 live in `tests/vectors/mersenne-exponents.txt` with their source and
        command, that its cycle takes the file over (moving it keeps the old path on the
        high-blast-radius list), and that rows above B cannot be recomputed by that command.
  - [x] Every `issues/*.md` matches exactly one `**Seed:**` line in `ROADMAP.md`; this cycle's seed
        stays `adopted:reference-big-integer`.
- [x] **Pre-push.** `sh spec/reference-big-integer/verify/prepush.sh` must be green: the P4 gate on the
      final tree, the ledger, the no-uncommitted-code rule, and the scan of the files and the history
      with its self-test. `prepush.sh scan` runs the scan alone; see it catch a planted site name once
      and a planted detailed line naming a made-up node once, then remove both plants.
- [x] **Push.** `git push origin feature/reference-big-integer`, fast-forward only (the foreman
      authorizes it for this phase). Never force.
- [x] **Run.** Submit the pushed head on `x86-64-avx512`, `x86-64-avx2` and `aarch64-gb10` through the
      cluster MCP server, with the site recipe the foreman hands over (accounts, QoS and paths, never
      committed), as the previous cycles' last phases did and `ci/test-commit.sh`'s header says.
  - Write the job from the pushed commit's `ci/test-commit.sh` to `<scratch>/mpsearch/jobs/`, without
    its final newline, and check its `git hash-object` before `sbatch`.
  - `sbatch` with `script_path="<file> <hash> <target>"`, the target file's resources, and `output` in
    that directory.
  - **Limits:** at most 30 minutes, 8 cores and 16 GB, and at most two jobs queued per cluster (one on
    the GB10 cluster). Output only under `<scratch>/mpsearch/`.
  - Resubmit a run that fails for a reason outside the tree, such as a preemption or a node fault.
    Record only runs that test the tree.
- [x] **Evidence.** Pass each piece through `sh spec/reference-big-integer/verify/redact.sh IN OUT`:
      for each target `evidence/<target>.json`, `evidence/<target>.out` and
      `evidence/<target>.check.log`; and `evidence/jobs.tsv` (a header `label job_id state exit_code`
      and one tab-separated line per job from `sacct`).
- [x] **Times** (R10's record): under `## Times` in `evidence/ledger.md`, one line per target,
      ``- `<target>`: check step <s> s (cycle 6: <s> s)``, from the run's `result.json` and from
      `spec/logger-state-per-translation-unit/evidence/<target>.json`, then what the difference
      holds (the check step compiles the checks too, so R10 itself is the gates' measurement on this
      Mac, which the line quotes from the last gate run).
- [x] **F1 remediation (R12; `REVIEW.md`, cycle 1).** Reopened 2026-10-16. The class: every numeral
      on a line this cycle added to a note in `doc/` stands in a check or cited span, unless it is a
      citation's locator, a link target, inline code, part of math with a free variable (a general
      statement the proof proves), or a judgment exemption listed by category (a definition or design
      parameter, the value of a statement for every p, a label, or F2's measured thresholds, deferred).
      Math with no free variable, a binding to a closed value and a closed power inside any math are
      concrete. `spec/reference-big-integer/remediation/f1-markers.py` evaluates that over
      `git diff c9b82ff -- doc/` and runs `notes.py claims` on the notes it covers; it went red on
      four sites, now marked: the two the review named (`doc/prp-and-gerbicz.md`, the residue 2 at
      p = 3; `doc/reference-arithmetic.md`, ⌈3/2⌉ + 1 = 3), the bound $10^{38}$ in Radix conversion
      (a check), and GIMPS's 64-bit residue in Res64 (a cited span). The README index and the
      bibliography carry no claims, so they are outside it.
- [x] **Again on the F1 head:** the pre-push, the push, the three runs, the evidence and `## Times`,
      as above, since the doc change is outside `spec/`, `issues/` and `ROADMAP.md`.
- [x] **F1 remediation (R12; `REVIEW.md`, cycle 2).** Reopened 2026-10-16. The class: **the notes
      state no measured value** (a threshold, a time, a ratio from a benchmark), since no check
      computes one and no work publishes it, so neither marker can carry it. The cycle-1 item above
      exempted F2's measured thresholds as deferred; R12 is a SHALL and can't defer an instance, so
      that category is gone. Swept: every line `git diff c9b82ff -- doc/` adds to the three notes
      (`doc/lucas-lehmer.md`, `doc/prp-and-gerbicz.md`, `doc/reference-arithmetic.md`) for measured
      or timed values. One site: `doc/reference-arithmetic.md`, The threshold (the product crossover
      at about 72 limbs, the square's at nearly twice that, the sweep up to $2^{14}$ limbs, and the
      cutoff at 96 limbs, twice), and Provenance's "the measured threshold", which stated no number.
      The paragraph keeps the cited range from MCA § 1.3.2 and now says the threshold is a design
      parameter set from a measurement on this code, its value `KARATSUBA_THRESHOLD` in `src/mp.hpp`,
      with no number restated: the value is a tunable the next measurement may change, and a copy in
      prose would drift unchecked. Provenance says the project measured it when it set the value.
      `f1-markers.py` drops the F2 entries and the category: an exemption of any category but D, G
      or S fails, so a measured number that returns unmarked fails as an unmarked numeral. The seed
      `issues/doc-claims-standing-check.md` keeps only the open convention on measured values.
      Follow-up after `2b791aa`: the comment above `KARATSUBA_THRESHOLD` in `src/mp.hpp` still gave
      the measured crossovers and cited the note as their source, which the note no longer holds, so
      the pointer was stale. The comment is now the home of the measurement record: it keeps the
      measured values (code comments are outside R12's marker rule), drops the citation of the note,
      and reads as how the value was chosen, measured when it was set. Comments only; nothing
      compiles differently. Swept `grep -rn 'reference-arithmetic' src tests AGENTS.md Makefile`:
      the other three hits (`src/mp.hpp` and `src/mp.cpp` file headers, `reduce_mersenne`) point to
      the mathematics and the folding derivation, which the note does hold, not to measured values.
- [x] **Again on the cycle-2 F1 head:** the pre-push, the push, the three runs, the evidence and
      `## Times`, as above, since the doc change is outside `spec/`, `issues/` and `ROADMAP.md`. That
      head is now the threshold-comment follow-up commit, the child of `2b791aa`.
- [x] **Review cycle 3 (R2, R11, R12; `REVIEW.md`, cycle 3 and Geoffrey's escalation decision).**
      Reopened 2026-10-16, past the three-cycle bound on his override; the review's F1 to F3 and G1.
  - **The ruling** goes into `doc/README.md`: definitions, format constants and theorem values (the
    general results a proof establishes) are not concrete numbers and take no markers; a number
    stated about a specific instance, or a restatement of a number the note marks elsewhere, does.
    One sentence after the rule; the index still carries no number and no marker (`check_readme`).
  - **F1, restatements (R12).** The class: a restatement, on a line this cycle added to one of the
    three notes, of a number the note marks somewhere else. Number words count, so `f1-markers.py`
    now reads two to twelve, twice and thrice as numbers (one and ordinals left out). Sites: in
    `doc/reference-arithmetic.md`, the bound on long division's estimate, marked once in a cited span
    (MCA, Theorem 1.4.1) and restated at The result (now its own cited span with that locator), at
    the add-backs "at most twice" (joined to the cited span beside it) and in Sources (now "the
    bound on the estimate and its add-backs"); and the chunk length of decimal input, marked once in
    the radix check ("A chunk of 19 decimal digits fits a limb") and restated as "chunks of 19" at
    The result and in Radix conversion and as "a multiple of 19", now pointers ("as many as a limb
    always holds", "such chunks", "a whole number of chunks"); The result's "powers of $10^{19}$"
    became "powers of the chunk base" with it. Kept by the ruling: the chunk base $10^{19}$ where the
    algorithm is defined (Horner's base, $P_0$, the quadratic alternative), the theorem bound "at
    most two folds" and its proof, the limb width, Res64's form, `x_p = 9`, and the counts that
    define a method or name its parts; each is an exemption by category in `f1-markers.py`.
  - **F2.** `doc/prp-and-gerbicz.md`, What mps uses: `x_p` is congruent to 9 times the type-1
    residue modulo `M_p`, by the proof of Theorem 2, not 9 times it as integers; a new check span
    carries the counterexample and the congruence at p = 11 (type-1 residue 1013, `x_11` = 929 =
    9 · 1013 mod `M_11`). The marked "or 2 at p = 3" span is unchanged.
  - **F3.** `Knuth1997`'s entry backs § 4.3.1 only as MCA cites it, for the at-most-twice theorem.
    Every site now says no more: in the note, Knuth is cited inside the cited span of that bound
    ("which cites [Knuth1997, § 4.3.1] for it"), the "classical method of Knuth" clause is gone, the
    refined estimate cites MCA, Exercise 1.20 (with § 1.4.1, the "more accurate partial quotient"),
    Sources lists Exercise 1.20 and gives Knuth's line as the theorem behind the bound, and
    Provenance says the division is written from MCA, which cites Knuth for that bound. In
    `tests/vectors/generate.py`, `division_steps`' comment cites MCA, 1.4.1 and Exercise 1.20, and
    the modular family's third source line names "the refined estimate of its Exercise 1.20". In
    `src/mp.cpp`, `multiply_schoolbook` drops the Knuth locator and `divide` cites Knuth only for
    Theorem 1.4.1's bound; comments only (the preprocessed token stream is byte-identical).
    The source line of `tests/vectors/modular.txt` changes with the generator: § Invariants 3 makes
    the expected values append-only and has each carry its source, and a source line naming a work
    the bibliography does not back for that claim is a provenance defect, not an expected value.
    Every record line stays as it was, the file still reproduces byte for byte under both Pythons,
    and the reader asks only for a nonempty source line.
  - **G1 (R2, R11).** Four `powmod` records appended at the end of `POWERS`, all with e = 1: a base
    above m, a base that is a multiple of m, the base equal to m, and a multiple of the one-limb
    m = 3. Their operands are all earlier operands, so `every_operand()` gains none and `hex.txt` and
    `decimal.txt` are unchanged; `modular.txt`'s records 6 to 41 are byte-identical, the four come
    after them, and the trailer counts 40. The mutant `BigInt b = base;` in `power_mod` passed
    `make check` with the old file and fails it with the new one. Geoffrey's escalation decision
    covers this append to `tests/vectors/` (§ Invariants 3, high-blast-radius).
  - `f1-markers.py`: the exemptions for the changed phrases are gone, the categories stay D, G and
    S, and the number-word entries are new; it went red on the pre-fix tree at every restatement
    above and green after. The seed `issues/doc-claims-standing-check.md` notes that the README now
    states the ruling, so the standing check enforces the rule as amended.
- [x] **Again on the review-3 head:** the pre-push, the push, the three runs, the evidence and
      `## Times`, as above, since the change is outside `spec/`, `issues/` and `ROADMAP.md`.
- [x] **Review cycle 4, N1 (R12; `REVIEW.md`, cycle 4 and Geoffrey's triage).** Reopened 2026-10-16.
      `doc/prp-and-gerbicz.md`, What mps uses, said `x_p` is congruent to 9 times the type-1 residue
      "but not equal to it as an integer", false as a universal statement: every probable prime with
      p ≥ 5 has `x_p` = 9 = 9 · 1. It now says "but not in general equal to it as an integer", and
      the p = 11 check span, unchanged, shows a case where they differ; "So a probable prime has
      `x_p` = 9" follows as before. The class: a universal claim the F2 fix introduced, on a line this
      cycle added to one of the three notes; the sweep of those lines found no other site.
      `f1-markers.py` is unchanged, since its exemption phrase "congruent to 9 times" still matches
      once. The outside-scope items get their home in the seed `issues/doc-claims-standing-check.md`:
      a Notes bullet that the standing check verify every locator a note cites against the entry's
      verification record, with MCA's record and the Knuth edition's year as the instances found.
- [x] **Again on the review-4 head:** the pre-push, the push, the three runs, the evidence and
      `## Times`, as above, since the change is outside `spec/`, `issues/` and `ROADMAP.md`.
- **Verify:** `sh spec/reference-big-integer/verify/p5.sh && python3 -I -B spec/reference-big-integer/remediation/f1-markers.py`
  (amended for F1: `verify/` is frozen, so the class check lives outside it).
- **Touches:** `spec/reference-big-integer/evidence/*`, `issues/ibdwt-transform-engine.md`,
  `issues/known-answer-tables.md`, `ROADMAP.md`; `src/`, `tests/` and `doc/` only if the audit finds a
  gap; and, amended in P5, `issues/objects-follow-the-compiler.md` (a new seed, with its `ROADMAP.md`
  entry, for the Makefile's objects that don't follow `CXX`) and, from the audit (`b8863f0`), comments
  and one diagnostic in `src/mp.hpp`, `tests/check_mp.cpp` and `tests/vectors/generate.py`; and, for
  F1, `doc/prp-and-gerbicz.md`, `doc/reference-arithmetic.md` and
  `spec/reference-big-integer/remediation/f1-markers.py`; and, for cycle 2's F1,
  `doc/reference-arithmetic.md`, `f1-markers.py` and `issues/doc-claims-standing-check.md`, then the
  comment above `KARATSUBA_THRESHOLD` in `src/mp.hpp`; and, for review cycle 3, `doc/README.md`,
  `doc/reference-arithmetic.md`, `doc/prp-and-gerbicz.md`, comments in `src/mp.cpp`,
  `tests/vectors/generate.py` and `tests/vectors/modular.txt` (its source line and four appended
  records), `f1-markers.py` and `issues/doc-claims-standing-check.md`; and, for review cycle 4,
  `doc/prp-and-gerbicz.md` and `issues/doc-claims-standing-check.md`.
- **Measured (P5):** the pushed head `b8863f0` passed on all three targets (check step 36 s, 45 s and
  108 s, against cycle 6's 27 s, 34 s and 76 s); the P5 gate gave R10 +2.42 s with clang++ and
  +2.34 s with g++-16, against +3.0 s. **F1 (P5 reopened):** the pushed head `b5668c3` passed on all
  three (check step 34 s, 46 s and 107 s); the retuned gate gave R10 +2.43 s with clang++ and +2.33 s
  with g++-16. **Cycle-2 F1:** the pushed head `3c2b4bb` passed on all three (check step 39 s, 46 s
  and 107 s); the gate gave R10 +2.43 s with clang++ and +2.36 s with g++-16. **Review cycle 3:**
  the pushed head `53e3ab2` passed on all three (check step 42 s, 72 s and 106 s); the gate gave R10
  +2.41 s with clang++ and +2.32 s with g++-16. **Review cycle 4:** the pushed head `6e8c20d` passed
  on all three (check step 32 s, 45 s and 106 s); the gate gave R10 +2.42 s with clang++ and +2.30 s
  with g++-16.

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
