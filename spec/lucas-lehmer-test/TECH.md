---
slug: lucas-lehmer-test
title: The Lucas–Lehmer test on the engine, with a shift and its error checks
kind: feature
appetite: big
status: in_progress
branch: feature/lucas-lehmer-test
base: main
current_phase: P4
last_updated: '2026-11-01'
phases:
- id: P1
  name: The Jacobi symbol and the stop rule
  status: done
  satisfies:
  - R5
  - R15
  - R18
  depends_on: []
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/lucas-lehmer-test/remediation/p3.sh && python3 -I spec/lucas-lehmer-test/remediation/F1-refusals.py && python3 -I spec/lucas-lehmer-test/remediation/F2-make-targets.py && python3 -I spec/lucas-lehmer-test/remediation/F3-mutants.py check && python3 -I spec/lucas-lehmer-test/remediation/F4-line-refs.py && python3 -I spec/lucas-lehmer-test/remediation/F5-mutants.py check && python3 -I spec/lucas-lehmer-test/remediation/F6-mutants.py check
- id: P2
  name: 'The test: the shift, the checks, the recovery and the result'
  status: done
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
  verify: sh spec/lucas-lehmer-test/remediation/p3.sh && python3 -I spec/lucas-lehmer-test/remediation/F1-refusals.py && python3 -I spec/lucas-lehmer-test/remediation/F2-make-targets.py && python3 -I spec/lucas-lehmer-test/remediation/F3-mutants.py check && python3 -I spec/lucas-lehmer-test/remediation/F4-line-refs.py && python3 -I spec/lucas-lehmer-test/remediation/F5-mutants.py check && python3 -I spec/lucas-lehmer-test/remediation/F6-mutants.py check
- id: P3
  name: The long tier, the landing gate and the remaining notes
  status: done
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
  - R15
  - R16
  - R17
  - R18
  depends_on:
  - P3
  parallel: false
  hammerable: false
  hill: uphill
  verify: sh spec/lucas-lehmer-test/remediation/p4.sh && python3 -I spec/lucas-lehmer-test/remediation/F1-refusals.py && python3 -I spec/lucas-lehmer-test/remediation/F2-make-targets.py && python3 -I spec/lucas-lehmer-test/remediation/F3-mutants.py check && python3 -I spec/lucas-lehmer-test/remediation/F4-line-refs.py && python3 -I spec/lucas-lehmer-test/remediation/F5-mutants.py check && python3 -I spec/lucas-lehmer-test/remediation/F6-mutants.py check
review:
  verdict: changes-requested
  cycle: 2
  last_reviewed_commit: 7bd10396e671f797d81e8ba1e14100e51a4602e9
  blocked_reason: 'review cycle 2: F5 (R13 progress fields unpinned), F6 (Jacobi vectors miss a borrow path); both LOW surviving mutants'
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

- [x] `src/mp.hpp`, `src/mp.cpp`: `int jacobi(const BigInt& a, const BigInt& n)` (PLAN § 2.2), its
      first line pinned; `mp_error` for a zero or even modulus; Shallit and Sorenson 1993 cited
      (or Brent and Zimmermann 2010, Remark 3: say which in the commit body and the note).
- [x] `tests/vectors/generate.py`: the `jacobi` family of research/01 (literal seeds; every exponent
      R1 and R2 cover in `make check` with symbols −1, 1 and 0; shifted LL residues; general odd
      moduli; Python's reciprocity loop on every record, Euler's criterion for prime M_p, a binary
      form for the rest); `tests/vectors/jacobi.txt` written by its command;
      `tests/vector_reader.*`: the op and field kind `J`, `jacobi` in `STEMS`.
- [x] `tests/check_mp.cpp`: "the Jacobi symbol matches the vectors", grading every record.
- [x] `src/math.cpp`: `run_in_order` stops after an error, its comment saying so (R15).
      `tests/check_result.cpp`: "run_in_order stops at the first verdict or error" (renamed from
      "…at the first verdict"), with stand-ins for an error first, in the middle and last.
- [x] `doc/reference-arithmetic.md`: the Jacobi symbol (definition, algorithm, invariant,
      termination, a check-marked example), cited; "What mps uses" names `jacobi`.
      `doc/references.md`: `ShallitSorenson1993`, Verified against its DOI's metadata.
- **Built** (amendment, 2026-11-01): `jacobi` cites Shallit and Sorenson 1993 (no locator: the
      paper's text was not read; the proof in the note is written from Brent and Zimmermann 2010,
      § 1); a zero modulus throws `mp_error("modulus zero")` and an even one `mp_error("even
      modulus")`; `compare` now delegates to a static `compare_limbs` that `jacobi` shares. The
      family is 461 records (research/01's 15 per exponent for the 24 exponents, and 101 on general
      odd moduli). Left for later phases, as planned: `AGENTS.md` § Map still says `run_in_order`
      "stops at the first decisive result" (P2's refresh of the stale wording, below, fixes it; P1
      may not touch `AGENTS.md`), and the three "published Mersenne prime exponents" sentences (P3).
- **Review cycle 2 remediation** (amendment, 2026-11-01; REVIEW.md F6, the foreman's routing: P1,
  P2 and P4 reopened, P3 done; F5 is P2's, below). Fixed as its class, with tests and vectors only:
  `src/mp.cpp` is unchanged.
  - [x] **F6** (R5): the vectors reach every carry and borrow path of `jacobi`'s steps on limbs.
        `generate.py` names the paths (`JACOBI_LIMB_PATHS`: the comparison's unequal counts, a
        decision below the top limb and equality; the strip's one and several whole limbs, bits
        carried down and an emptied top limb; the subtraction's equal limbs below the top with
        nothing borrowed, a borrow across an equal limb, across two or more limbs in a row, ending
        at a limb one above, into the top limb, past the shorter operand and across a zero limb
        there, the longer operand with nothing borrowed past the shorter, and a trim of two or more
        limbs; a gcd of several limbs whose low limb is 1), traces `jacobi` on limbs as the C++ runs
        it (`jacobi_limb_paths`), and writes 32 pairs built for the rare ones (`JACOBI_LIMB_PAIRS`,
        review cycle 2's among them). It asserts that each pair reaches its path with the
        reciprocity loop's symbol, and that the pairs reach every path. `jacobi.txt` is its
        command's output, 493 records (461 before), in its format with its trailer; each new
        symbol is the reciprocity loop's, checked by the binary form. The pairs are coprime but the
        gcd's: 21 of −1, 8 of +1, 3 of 0. `tests/check_mp.cpp`'s comment on the vectors names them.
  - **Checker** `remediation/F6-mutants.py` (record `F6-mutants.txt`, tied by sha256 to
        `src/mp.*`, the vectors, `generate.py` and `tests/check_mp.cpp`). `check` traces every
        record whose modulus has at most 2048 bits with its own model of `jacobi` on limbs and
        requires every path; then the record; then, live, the borrow through an equal limb
        dropped (review cycle 2's mutant) red under clang++. The sweep covers every decision and
        arithmetic statement of `compare_limbs`, `trim`, `strip_twos`, `subtract_in_place` and
        `jacobi` (the reduction of a ≥ n is `reduce`'s, with its own vectors, and is not swept):
        50 mutants, 42 killed under each compiler and 8 equivalent, each with its reason.
        On `7bd1039`'s tree, in a local clone, the same sweep under clang++ left three
        survivors, each now killed: review cycle 2's borrow, a strip counting whole limbs as 1 bit
        each, and a gcd read by its low limb alone. Seen red there: four paths no record takes
        (the borrow across an equal limb, across two or more limbs in a row and across a zero limb
        past the shorter operand, and the gcd of several limbs), the record not that tree's, and
        the live mutant green.
  - **Gate:** P1's `verify:` is P2's, and F6's checker and F5's both run in it, so it ran once on
        the tree holding both phases' work (P2's amendment, below, has its line).
- **Verify:** `sh spec/lucas-lehmer-test/remediation/p3.sh`, then the six checkers F1–F6 (the
  frontmatter's `verify:` from review cycle 2: a reopened phase earlier than the latest done one
  runs the latest done phase's cumulative gate; before it, `sh spec/lucas-lehmer-test/verify/p1.sh`).
- **Touches:** `src/mp.*`, `src/math.cpp`, `tests/vectors/generate.py`, `tests/vectors/jacobi.txt`,
  `tests/vector_reader.*`, `tests/check_mp.cpp`, `tests/check_result.cpp`,
  `doc/reference-arithmetic.md`, `doc/references.md`, `doc/README.md`.

## Phase P2 — The test: the shift, the checks, the recovery and the result
**Satisfies:** R1–R4, R6–R14, R16 (the injections, `tests/cli.sh`, the `make check` budget), R18
(AGENTS.md's lines for `src/lucas.*`, `doc/lucas-lehmer.md`) · **Depends on:** P1
**Goal:** `mps -t ll p` runs the test of PLAN § 2.3 and reports its run in both views; the checks
prove it on the known answers at two shifts and prove every check and recovery by injection.

- [x] `src/lucas.hpp`, `src/lucas.cpp` (PLAN § 2.3, § 2.4): the constants, `Step`, `Options` with its
      three hooks, `largest_exponent`, `unshifted`, `jacobi_of`, `degenerate`, `confirms_prime`,
      `test`, the pinned first lines; the refusals of R14 before any squaring; the log lines of
      PLAN § 2.5; no `reference::` or `known::`.
- [x] `src/math.cpp`: `test_lucas_lehmer` returns `lucas::test(p)`. `src/math.hpp` if needed.
- [x] `src/result.hpp`, `src/result.cpp`: `RunReport`, `prime_by_residue`, the run-carrying
      `composite_by_residue` and `error`, `run()`, the views of PLAN § 2.5, an error's record printed
      on stdout under `-j`, `RECORD_VERSION` 2 — in one commit with the record's change (R12, § Conventions).
- [x] `tests/check_lucas.cpp`: the twelve labels of PLAN § 2.6; `tests/check_result.cpp`,
      `tests/check_math.cpp`: what the new behaviour makes wrong, and the views of a run and of an
      error.
- [x] `tests/ll.sh` (PLAN § 2.6); `Makefile`: `MODULES += lucas`, `check` and `check-engine` run it.
- [x] `tests/cli.sh`: only the expectations whose outcome the Lucas–Lehmer test decides, and the
      record version (R16 as the GOAL now words it; the gate compares it with `bcb6fdf`'s, PLAN § 6).
- [x] `AGENTS.md`, in this phase's commit: the high-blast-radius line and § Map bullet for
      `src/lucas.hpp`, `src/lucas.cpp`; § Map's stale wording (among it the `src/math.*` bullet's
      "stops at the first decisive result", stale since P1's stop rule); `tests/check_lucas.cpp`, `tests/ll.sh`
      and the vectors in the `tests/` bullet (R18). Never change the four lines `tests/cli.sh` reads or
      the `verify` row.
- [x] `doc/lucas-lehmer.md`, in this phase's commit: "What mps uses" as built, each limit as
      "`NAME` (value)"; the 0-symbol text per the GOAL's Clarification; no stale sentence (PLAN § 2.7).
- **Built** (amendment, 2026-11-01): `GOOD_STATES` stays 32; `make check` measured +3.429 s
      over `bcb6fdf` (4.973 s against 8.402 s, the full gate's interleaved least of 5, load 5.9).
      `lucas::test` turns any `engine_error` other than a refusal into an error result carrying the
      run (PLAN § 2.3.5), checked in "the test replays a refused squaring once"; the attempt count
      of `Step` is kept per iteration since the last good state. `tests/cli.sh` adds no
      expectation (`planned` stays 644): P_max is read from `src/lengths.cpp` without one of its
      own, since a value not read fails every line that names it. `AGENTS.md` § Build and run's
      `make check` and `make check-engine` comments name `tests/ll.sh` in this commit, because the
      targets change here; P3 adds the `check-long` and landing-gate lines. The three "published
      Mersenne prime exponents" sentences stay for P3, `doc/lucas-lehmer.md`'s among them.
- **Review cycle 1 remediation** (amendment, 2026-11-01; REVIEW.md F1–F4, the foreman's routing:
  P2 and P4 reopened, P1 and P3 done). Each finding fixed as its class:
  - [x] **F1** (R14; `src/lucas.cpp` is high-blast, the human gate signed off by the foreman in
        REVIEW.md): a refusal names every reason that applies to its p, in the order not odd or
        below 3, past P_max, odd and composite, joined by "; " (an even p past P_max now names
        both). Exit 42 and no squaring before a refusal, as before. "the test refuses exponents it
        cannot run" now pins the whole domain under its own label: every p to 64 but the odd primes,
        P_max − 3 to P_max + 64, and large ones of each kind, each naming exactly the reasons that
        apply, every combination present. `doc/lucas-lehmer.md` says the same. `tests/cli.sh`
        holds no even p past P_max (its largest PVAL is 999999999, and its -s cases are 4, 33,
        999999999 and 31), so it is unchanged and `check_lucas` covers the case.
  - [x] **F2** (§ Conventions): the profile's `verify` row says what `make check` runs: the checks,
        the smoke test and the known-answer runs (`tests/ll.sh`), all always run. The sweep of every
        profile row and § Build and run line that describes a make target found no other stale one.
  - [x] **F3** (R4): "the test checks the Jacobi symbol at intervals and on the final residue" pins
        a symbol of 0 as a failed check: M83, whose least factor 167 the check derives, holding
        s = 169 at a checked iteration and at the last recovers to the run without it, and on every
        attempt ends in an error naming the symbol. The mutant sweep over every decision of
        `src/lucas.cpp` (`remediation/F3-mutants.py`, record `remediation/F3-mutants.txt`) found
        two more survivors under clang, each now pinned: an engine kept at the old length after a
        length move (the move check now compares every distance after the move with a run at the
        second length) and a given shift at p or past it reported unreduced (the known-answer check
        runs M1279 at shift 1300). Four mutants are equivalent, each with its reason in the record.
  - [x] **F4**: no seed cites a line of a file this cycle changed. `issues/verdict-path-guards.md`
        names `line_text` in `src/result.cpp`; `issues/ci-job-standing-checks.md` names the
        Makefile's `check` target, with a dated line saying it now runs `tests/ll.sh` too;
        `issues/lucas-lehmer-test.md` names the paragraphs, not their lines, with a dated line
        saying the three sentences are corrected. This rides in P2's commit.
  - **The frozen gate, wrapped** (foreman ruling, 2026-11-01). `verify/ll_files.py` keeps the old
        verify row among the AGENTS.md lines it requires, so no correct tree passes the frozen
        gates after F2. `remediation/make.sh` generates `remediation/p3.sh`, `prepush.sh` and
        `p4.sh` from the frozen scripts with two edits only (they source `verify/`; after
        `gate.sh`, `remediation/override.sh` redefines `files_check` from the frozen `gate.sh`'s own
        text with its AGENTS.md step reading `remediation/ll_files.py`, and grades the file list as
        of phase 4), and `remediation/ll_files.py` from the frozen one with only the verify row
        changed; `make.sh check` (run by the override on every gate) fails if any derived file
        differs from its frozen source by more than those edits. The file list as of phase 4 is a
        second edit beyond the ruling's one line, and a no-op in prepush.sh and p4.sh, which set
        it already: the frozen P3 gate is red on this tree for P4 dispatch A's ledger bullets in
        `issues/`, whatever F2 does. Seen red first on the fixed tree: `verify/p3.sh quick` fails
        "files: what changed" on the `issues/` edits, and the frozen `ll_files.py agents 3` fails
        "AGENTS.md lost the line: | verify | …"; the derived one passes, and `remediation/p3.sh
        quick` is green.
  - **Checkers** (each seen red on `92d08c7`'s tree, in a local clone): `F1-refusals.py` (builds
        `bin/mps`, runs `-s -t ll` over 318 refusals; red: 141 even p past P_max without P_max, and
        the note), `F2-make-targets.py` (red: the verify row lacks `tests/ll.sh` and says "both" of
        three steps), `F3-mutants.py check` (the record is this tree's and has no survivor, and live
        the symbol-0 mutant is red; red: no record of that tree, and the mutant green),
        `F4-line-refs.py` (red: six references, `src/result.cpp:99`, `Makefile:34` and four doc
        lines in `issues/lucas-lehmer-test.md`).
  - **Gate:** the retuned `verify:` green in full; R16 `make check` +3.464 s over `bcb6fdf`
        (4.978 s against 8.442 s, least of 5 interleaved, load 4.4; budget +5 s).
- **Review cycle 2 remediation** (amendment, 2026-11-01; REVIEW.md F5, the foreman's routing and
  rulings of 2026-11-01). Fixed as its class, with checks only: `src/lucas.cpp` is unchanged.
  - [x] **F5** (R13): "the test logs its progress" pins each value of the line to what R13 says it
        means. At an interval of 0, M127's 125 lines are each in the line's form (PLAN § 2.5, the
        time per iteration to three places), give iterations 1 to 125 of 125 in order, each the
        distance of its own squaring (the distance hook reports i mod 10 / 100), and each a time to
        go within half a second of its time per iteration times the iterations left. At an
        interval of 25 ms, with the distance hook sleeping 150, 40 and 40 ms at iterations 40, 41
        and 100, there is a line at each; every line's largest distance is the largest of the
        squarings since the line before, its time per iteration times those squarings is at least
        the sleep at its iteration and at most that plus the interval plus 100 ms, and its time to
        go agrees as above; the run's largest distance stays the run's. M86243, whose first
        squaring the hook makes last 60 ms (an engine error injected at iteration 2 ends the run),
        gives over an hour to go in hours, minutes and seconds that agree with its time per
        iteration. No hook sets the clock, so the times are bounded by the sleeps, not fixed (the
        foreman's ruling); the reset and the iterations left are pinned exactly. The class's other
        values, pinned under their own labels: 200 default runs of M5 report every shift in [0, 5)
        and no other ("the test chooses a fresh shift each run"); DISTANCES_LOGGED distinct large
        distances and one of exactly the sizing distance are counted, logged each with its own
        distance and not folded, and the one at the limit neither counted nor logged ("the test
        counts and logs large round-off distances"); CHECK_FAILURES + 1 failures at different
        checked iterations, a good state passed between each, end the run in an error naming the
        count ("the test recovers from a failed check, up to its limit"). No assertion is weakened;
        the labels and their count, 116, are as before. `make check` takes about 0.35 s more.
  - **Checker** `remediation/F5-mutants.py` (record `F5-mutants.txt`, tied by sha256 to
        `src/lucas.*`, `tests/check_lucas.cpp`, `tests/cli.sh` and `tests/ll.sh`). The sweep covers
        every statement of `src/lucas.cpp` that feeds a reported value: the progress line's
        variables and resets, its time per iteration and time to go, the run report's fields and
        counters, the logged and folded distances, and the transient, failure and retry counts. 55
        mutants: 51 killed under clang++ and 52 under g++-16, 4 equivalent, each with its reason
        (three not observable without a hook on the clock; one, the transient count moved before
        the replay's squaring, not observable through the hooks, which refuse before the squaring).
        `check` checks the record, then, live, review cycle 2's three survivors red under clang++.
        On `7bd1039`'s tree, in a local clone, the same sweep under clang++ left 21 survivors
        that are not equivalent, each now killed: review cycle 2's three, the line's largest
        distance unset, taken from the run or from the last squaring, the time per iteration
        undivided, divided by one squaring more or scaled to microseconds, the time to go from the
        iterations done, its minutes, seconds or hours unwrapped, the drawn shift reaching p or
        never 0, the run's largest distance taken from the line, a distance of exactly the sizing
        distance counted, the logged line naming the run's largest, a fold printed at exactly
        DISTANCES_LOGGED, the failure count reset at each good state and the reason naming the
        limit for the count. Seen red there: the record not that tree's, and the three live
        mutants green.
  - **F3's record** regenerated with its own sweep, since `tests/check_lucas.cpp` changed:
        63 mutants, 59 killed under each compiler, the same 4 equivalent, none surviving.
  - **One gate run for P1 and P2.** Their `verify:` is the same command, and F5's checker passes
        only with P2's checks in the tree, so it ran once on the tree that holds both phases' work;
        P1's and P2's files are then committed apart. Green in full: the wrapped P3 gate (both
        compilers, the vectors reproduced and recomputed, the probe, 16 mutants, the landing gate's
        run mode) and F1–F6; R16 `make check` +3.784 s over `bcb6fdf` (4.930 s against 8.714 s,
        least of 5 interleaved, load 9.9; budget +5 s), the long tier 45 s.
- **Verify:** `sh spec/lucas-lehmer-test/remediation/p3.sh`, then the six checkers F1–F6 (the
  frontmatter's `verify:`; before review cycle 1, `sh spec/lucas-lehmer-test/verify/p2.sh`).
- **Touches:** `src/lucas.*`, `src/math.*`, `src/result.*`, `tests/check_lucas.cpp`,
  `tests/check_result.cpp`, `tests/check_math.cpp`, `tests/ll.sh`, `tests/cli.sh`, `Makefile`,
  `AGENTS.md`, `doc/lucas-lehmer.md`; `tests/engine_support.*` only to share a helper.

## Phase P3 — The long tier, the landing gate and the remaining notes
**Satisfies:** R16 (the long tier, both compilers), R17, R18 (the rest) · **Depends on:** P2
**Goal:** `make check-long` runs the long tier within 15 minutes on both compilers;
`ci/engine-gate.sh` and the profile's `landing gate` row exist and pass on this tree; AGENTS.md and
the notes name everything this cycle created.

- [x] `Makefile`: `check-long` (`sh tests/ll.sh $(EXE) long`), `.PHONY`.
- [x] `ci/engine-gate.sh` (PLAN § 2.8): run mode and `compare` mode, the guards, exit statuses 0/1/2,
      its header the manual.
- [x] `AGENTS.md`, in this phase's commit: § Build and run and § Map lines for `make check-long` and
      `ci/engine-gate.sh`; the profile's `| landing gate | … |` row after `cluster test` (R17, R18).
- [x] `doc/lucas-lehmer.md`, `doc/prp-and-gerbicz.md`, `doc/reference-arithmetic.md`: the three
      sentences say what `make check` runs since `known-answer-tables` (R18).
- **Built** (amendment, 2026-11-01): `check-long` runs only `tests/ll.sh $(EXE) long`, as this
      checklist says (not `make check`'s steps first); 55 s alone on clang, 62 known answers, and
      45 s on clang in the full gate (green on both compilers). `ci/engine-gate.sh` is the
      prototype's design (PLAN § 2.8), with the larger functions documented in
      `ci/test-commit.sh`'s style; its `SUMMARIES` require the `ll:` line too, since P2 made
      `tests/ll.sh` a step of `make check-engine`. Its run mode in the full gate: clang 500 s,
      g++-16 556 s, about 18 minutes. `doc/lucas-lehmer.md`'s "What checks it" also names
      `make check-long`. AGENTS.md's § Build and run names the engine gate as `sh ci/engine-gate.sh`
      (the repo's scripts are not executable), and the § Map `tests/` bullet is rewrapped.
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

- [x] **Audit.** Walk R1–R18 against `src/`, `tests/`, `doc/`, `ci/`, `AGENTS.md` and the Makefile, one
      by one, and confirm each has its check or its gate step (PLAN § 2.9). Fix and commit anything
      found (above).
- [x] **Deferral ledger.** Walk P1–P4 of this file and the build's own commits for "do not fix here",
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
- [x] **Pre-push.** `sh spec/lucas-lehmer-test/verify/prepush.sh` must be green (about 60 minutes).
      `prepush.sh scan` runs the scan alone; see it catch a planted site name once and a planted
      detailed line naming a made-up node once, then remove both plants.
- [x] **Push.** `git push origin feature/lucas-lehmer-test`, fast-forward only (the foreman authorizes it
      for this phase). Never force. The push ends dispatch A: report the pushed commit, its
      `ci/test-commit.sh` hash (`git hash-object`), and prepush's last two lines. The foreman submits
      the three runs (`x86-64-avx512`, `x86-64-avx2`, `aarch64-gb10`) from that commit's
      `ci/test-commit.sh`, as its header says, and resubmits a run that fails for a reason outside the
      tree. Only runs that test the tree are recorded.
- [x] **Evidence** (dispatch B). Pass each piece through `sh spec/lucas-lehmer-test/verify/redact.sh IN
      OUT`: for each target `evidence/<target>.json`, `.out` and `.check.log`; and `evidence/jobs.tsv`
      (a header `label job_id state exit_code` and one tab-separated line per job from `sacct`).
- [x] **Ledger** (dispatch B): `spec/lucas-lehmer-test/evidence/ledger.md` with `## Deferrals` (each
      seed above, and any the walk found, with its home), `## Times` (``- make check: <prepush's timing
      sentence>`` and ``- make check-long: <s> s on clang++ (budget 900 s)``, from dispatch A's prepush
      line; then one line per target, ``- `<target>`: check step <s> s``, from its `result.json`), and
      `## Landing gate` (``- engine-gate: green on <hash>: …``, prepush's last line verbatim).
- **Dispatch A** (amendment, 2026-11-01): the audit walked R1–R18 against PLAN § 2.9 and found no
      gap, so no audit-fix commit; the walk of P1–P3 and their commits found no deferral beyond the
      three Notes bullets, so no new seed and `ROADMAP.md` unchanged. The foreman, not dispatch A,
      pushes the branch (its change to the Push item above): dispatch A stops after a green pre-push
      with the tree clean and P4 `in_progress`.
- **Dispatch B** (amendment, 2026-11-01): the foreman pushed `2530c06` and ran it on the three
      targets, each `COMPLETED 0:0` on the first submission (none resubmitted), each passing 116
      checks, 644 smoke-test cases and 48 `ll` cases with GCC 14; check steps 58 s, 121 s and 196 s.
      The evidence went through `verify/redact.sh` only. The ledger quotes dispatch A's pre-push
      lines (no second run) and takes the landing gate's time on this Mac from P3's full gate, since
      the pre-push line carries none and `issues/engine-differential-standing-check.md` points there.
- **Review cycle 1 remediation** (amendment, 2026-11-01): P4 reopened, since P2's remediation
  changes code and the cluster evidence of `2530c06` no longer covers HEAD. Its F4 fix rode in P2's
  commit. Dispatch A again: the audit of what the remediation touched, `remediation/prepush.sh` (the
  frozen `verify/prepush.sh` under the wrapper of P2's amendment) on the committed tree, then a stop
  before the push with P4 `in_progress`; the foreman pushes and submits the three runs, and dispatch B
  redoes the evidence, the ledger's `## Times` and `## Landing gate` from the new pre-push lines, and
  closes P4 under the retuned `verify:`.
- **Dispatch A, review cycle 1** (amendment, 2026-11-01): the audit walked what the remediation
  touched (R14's refusals in `src/lucas.cpp`, the note and the refusal check; R4's Jacobi check;
  the move and known-answer checks the sweep tightened; the profile's verify row; the three seeds)
  against R1–R18 and § Invariants, and found no gap: no new deferral, `ROADMAP.md` unchanged, the
  three Notes bullets as they were. The frozen `verify/prepush.sh` on `1e820ba` passed its ledger
  notes, code-committed and scan steps and went red at "files: AGENTS.md" on the old verify row, as
  P2's amendment rules; `remediation/prepush.sh` runs it under the wrapper.
- **Dispatch B, review cycle 1** (amendment, 2026-11-01): the foreman pushed `df877de` and ran it on
  the three targets from its `ci/test-commit.sh` (blob `c4b1f28`, unchanged), each `COMPLETED 0:0`
  on the first submission (none resubmitted), each passing 116 checks, 644 smoke-test cases and 48
  `ll` cases with GCC 14; check steps 61 s, 97 s and 197 s. The new evidence replaced the first
  round's, through `verify/redact.sh` only. The ledger's `## Times` and `## Landing gate` quote
  review cycle 1's dispatch A pre-push lines (no second run) and name `df877de` as the tested
  commit; `## Deferrals` is unchanged. The landing gate's time on this Mac stays P3's, on
  `2530c06`'s code, marked as not remeasured. The gate is the retuned `verify:`, with
  `remediation/p4.sh` in place of `verify/p4.sh`. Its `audit` mode green, then the full `verify:`
  green; its own R16 run gave `make check` +3.470 s over `bcb6fdf` (4.968 s against 8.438 s,
  least of 5 interleaved, load 4.5; budget +5 s), the long tier 46 s.
- **Review cycle 2 remediation** (amendment, 2026-11-01): P4 reopened, since P1's and P2's
  remediation change the vectors and the checks, and the cluster evidence of `df877de` no longer
  covers HEAD; its `verify:` gains F5's and F6's checkers. Dispatch A again: the audit of what the
  remediation touched, `remediation/prepush.sh` on the committed tree, then a stop before the push
  with P4 `in_progress`; the foreman pushes and submits the three runs, and dispatch B redoes the
  evidence and the ledger's `## Times` and `## Landing gate`, and closes P4 under the retuned
  `verify:`.
- **Dispatch A, review cycle 2** (amendment, 2026-11-01): the audit walked what this round touched
  (R5's vectors, generator and their check's comment; R3's, R6's, R11's and R13's checks in
  `tests/check_lucas.cpp`; the three checkers' scripts and records) against R1–R18 and § Invariants.
  R16: the hooks are still set only in `tests/`, `tests/cli.sh` is unchanged, both compilers green,
  `make check` within budget. § Invariants 3: the 461 earlier records are unchanged and the 32 new
  ones appended, each from its command with the reciprocity loop; `tests/vectors/` is on the
  high-blast list, so F6 is a CONFIRMED finding in a listed path, changed only as the foreman's
  route directed. § Invariants 2: the new progress checks read the clock for the log only, never
  for a result. § Invariants 4: no outside source opened. One gap, fixed in
  `[feature] Build lucas-lehmer-test P4: review cycle 2 audit fix` before the pre-push: a
  comment in `tests/vectors/generate.py` named the review that found a pair, a spec reference in
  a product file; the sentence is gone, the vectors are unchanged, and F6's record was swept again
  for the new digest (the same counts). No new deferral: the times the progress line prints come
  from the system clock, which R13 does not ask to be settable, so bounding them is the check, not
  a deferral; `ROADMAP.md` and the three Notes bullets are unchanged. `remediation/prepush.sh` on
  the committed tree (`0c45bc3`) is green; its last two lines:
  `prepush: times for ## Times: - make check: make check 4.974 s at bcb6fdf, 8.783 s now, +3.809 s (budget +5 s; least of 5 interleaved runs each; load 4.4); - make check-long: 46 s on clang++ (budget 900 s)`
  and
  `prepush: landing gate for ## Landing gate: - engine-gate: green on 0c45bc31db7156c26bb9b0686438d622cf4038e7: make check-engine under Apple clang version 21.0.0 (clang-2100.3.34.2) and g++-16 (Homebrew GCC 16.2.0) 16.2.0, 5 residue digests equal`.
  The tested commit is `0c45bc3`; this note's commit changes only `TECH.md`. Stopped before the
  push, with P4 `in_progress`.
- **Verify:** `sh spec/lucas-lehmer-test/remediation/p4.sh`, then the six checkers F1–F6 (the
  frontmatter's `verify:`; before review cycle 1, `sh spec/lucas-lehmer-test/verify/p4.sh`, whose
  `audit` mode the wrapper keeps).
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
