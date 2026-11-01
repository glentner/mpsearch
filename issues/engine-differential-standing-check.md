---
status: unshaped
kind: feature
appetite: small
---

# A standing check for the engine's differential tests

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

§ Invariants 2 lets an arithmetic engine produce a verdict only after it has been tested against the
reference arithmetic on the known answers. The FP64 weighted-transform engine
(`spec/ibdwt-transform-engine/`) is tested in two tiers (its GOAL, R12). The default `make check`
runs a short tier within a 5 s budget. A separate make target runs the full differential suite in
about 15 minutes: the digit round-trip, squarings and products against `BigInt` at every supported
length, and squaring chains against the known-answer tables (R1–R9 and R14). The full suite runs in
that cycle's gates. Once the cycle merges, nothing runs it unless someone remembers to. A later
change to the engine, the length table, the build flags or the compiler could break a length the
short tier doesn't reach, and every check the repository runs would still pass.

Geoffrey ruled (2026-10-22) that the heavy target stays out of the default `make check` and out of
the profile's `verify`, and that a standing check enforces it instead.

## Why it was deferred

Out of `ibdwt-transform-engine`'s scope: that cycle builds the engine and its two tiers, and where a
15-minute suite keeps running is a separate decision. Deferred from `ibdwt-transform-engine` at its
GOAL sign-off, 2026-10-22. New work, not a defect: the suite exists and passes when run, and nothing
produces a verdict from the engine yet.

## Outcome / vision

The engine's full differential suite keeps running after it lands, and a change that breaks it is
seen before any verdict depends on the result. Shaping chooses the form, and must choose it before
the Lucas–Lehmer cycle produces verdicts on the engine. Options seen so far:

- a step in the cluster test job (`ci/test-commit.sh`), run for every cycle that touches the engine,
  its table or the build flags;
- a scheduled run against `main`;
- a gate that the landing recipe requires of any change to the engine's paths.

## Sketch of the acceptance criteria

- **R1** — WHEN a change to the engine's sources, its length table or the numeric build flags is
  about to land, the full differential suite SHALL have run on that change and passed.
- **R2** — IF the suite fails, THEN the check SHALL name the failing length, exponent or row.
- **R3** — The check SHALL leave the default `make check` and the profile's `verify` unchanged.

## Notes

- Deferred from `ibdwt-transform-engine` (its GOAL, R12 and Clarifications; Geoffrey's ruling of
  2026-10-22).
- Its form is decided at the shaping of `issues/lucas-lehmer-test.md` at the latest, before any
  verdict depends on the engine.
- Later engines (`issues/ntt-transform-engine.md`, `issues/cpu-transform-throughput.md`) will have
  suites of the same kind; shaping decides whether one check covers every engine.
- Related: `issues/known-answer-standing-check.md`, `issues/ci-job-standing-checks.md` and
  `issues/doc-claims-standing-check.md`, the same gap for the tables, the cluster job and the notes.
- Found by: `ibdwt-transform-engine` shaping, 2026-10-22.
- **The suite as built** (deferred from `ibdwt-transform-engine`, its P4, 2026-10-31). The suite is
  `make check-engine`: `make check`'s two steps, then `bin/check engine`, the engine tier in
  `tests/check_engine.cpp`. Its ten checks run the digit round-trip, squarings with and without the
  subtraction, products, the round-off monitor, repeats and structured inputs at both assigned
  exponents of all 144 supported lengths; then squaring chains, at the length the table names, at
  the next length and shifted, that reproduce every partial residue up to the table's largest
  exponent and every final residue up to 100,069; and Lucas–Lehmer chains that end in zero for the
  27 Mersenne prime exponents from 3 to 100,000. On an Apple M2 Pro with clang, alone and with
  nothing to rebuild, it took 459 s at P4 (budget 900 s, that GOAL's R12; the cycle's gates time it
  beside other work). The squaring, product and chain checks print residue digests, which a clang
  and a GCC build must share (that GOAL's R9). Only the cycle's gates compared them, and nothing does
  once it merges, so the standing check should run both compilers and compare the digests.
- **No check isolates the magnitude refusal** (deferred from `ibdwt-transform-engine`, its PLAN § 5,
  2026-10-22). The engine refuses an output that is not finite or reaches 2^51, where no distance can
  be seen (`ibdwt::MAGNITUDE_LIMIT`). The envelope refuses every input the checks build before an
  output gets that large, so deleting the magnitude refusal leaves both tiers green; it stays as
  defence in depth. A block input that reaches 2^51 inside the envelope would isolate it.
  `lucas-lehmer-test`, the first cycle to act on the engine's refusals, was to decide whether to
  build that input. Its shaping said no (foreman's ruling, 2026-10-31; `spec/lucas-lehmer-test/` Clarifications, Q8): the driver
  meets all four refusals through one `roundoff_error` path, proved by injection, so the gap is this
  suite's, and the note moved here from `issues/lucas-lehmer-test.md`.
- **The form is decided** (`lucas-lehmer-test` shaping, foreman's ruling of 2026-10-31, under
  Geoffrey's ruling of 2026-10-22 that this shaping decides it; `spec/lucas-lehmer-test/`
  Clarifications, Q4). It is a landing gate the profile names: `make check-engine` under clang and
  under GCC with the residue digests compared, before any change to the engine's sources, its length
  table or the numeric build flags lands. `lucas-lehmer-test` adds the row and passes the gate (its
  R17). What stays here: whether one gate covers later engines' suites
  (`issues/ntt-transform-engine.md`, `issues/cpu-transform-throughput.md`), and any scheduled run.
- **The gate as built** (deferred from `lucas-lehmer-test`, its P4, 2026-11-01). `ci/engine-gate.sh
  [REV]` writes `git archive REV` (default HEAD) into two fresh copies and runs `make check-engine` in
  each, under clang (`MPS_CLANG`) and then under a real GCC (`MPS_GCC`, told by its predefined
  macros, never by its name: on macOS `g++` is clang), one after the other; then it compares the two
  logs, which `sh ci/engine-gate.sh compare LOG_A LOG_B` does alone. A log passes only as a green
  `make check-engine`: the four summaries its `SUMMARIES` constant names (two `check:`, one `cli:`,
  one `ll:`), each with 0 failed, no ` FAIL ` line, and one well-formed digest line for each of the
  five labels in `LABELS` (squarings, products, partial chains, final chains, zero chains). Its exit
  status is 0 when green, its last line `engine-gate: green on <full hash>: …`; 1 for a red make, a
  log that is not a green `make check-engine` or a digest that differs, naming each such label with
  both digests; 2 for bad arguments, a missing log or commit, or a compiler missing or not the one it
  must be. The profile's `landing gate` row (`AGENTS.md` § Factory) requires it green on the commit
  that lands before a change to `src/ibdwt.*`, `src/fft.*`, `src/lengths.*`, `src/lengths.txt`,
  `src/measure.cpp` or `IEEE` in the `Makefile`. Its time is in this cycle's ledger
  (`spec/lucas-lehmer-test/evidence/ledger.md`). What stays here is as above: later engines' suites,
  a scheduled run, and the input that isolates the magnitude refusal.
