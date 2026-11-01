---
status: unshaped
kind: feature
appetite: big
---

# The checkpoint file, and resume without changing the answer

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The largest known exponent needs 136 million squarings: days on one device. Even inside the portable
engine's range, a run near its largest exponent (20,381,696) takes days, and a run at a few million
takes hours. A run has to survive interruption without changing its answer, and today nothing it
computes outlives the process.

`spec/lucas-lehmer-test/` gives the Lucas–Lehmer test good states held in memory: residues, with
their iteration, shift and length, that passed every error check. It recovers from a failed check or
a reproducible round-off refusal by returning to one. A kill, a node failure or a batch job's time
limit loses them all. What exists for persistence is pre-existing and unused:

- `-f/--file PATH` is parsed into `file_path`, defaulting to `<PVAL>.mps` (`src/cli.cpp:79`,
  `:244`), and nothing reads it;
- `src/io.hpp` sketches a plain-text key–value `.mps` format (`MPS_File`, `FORMAT_EXAMPLE`), and
  nothing reads or writes it.

PRP with the Gerbicz check (`issues/prp-gerbicz-test.md`) and P-1 (`issues/pminus1-factoring.md`)
need the same file, so its format is shared, not the Lucas–Lehmer test's own.

## Why it was deferred

Split from `lucas-lehmer-test` at its shaping by the size circuit-breaker (foreman's ruling,
2026-10-31; that GOAL's Clarifications, Q1 and Q2). That cycle builds the test in one process; an
in-memory good state is its checkpoint for § Invariants 1 and 2, and this seed persists the same
state across processes. New work, not a defect.

## Outcome / vision

A run writes its last good state to a versioned, checksummed `.mps` file on a timer, atomically, and
a run started with the same file resumes from it and ends with the residue an uninterrupted run
gives. The format is the one checkpoint format of `mps`, shared with PRP and P-1. With resume in
place, the Lucas–Lehmer runs too long for a check run: the final residues at 1,000,193 and above,
and Part II's "every known Mersenne prime up to a few million".

## Sketch of the acceptance criteria

- **R1** — WHEN a run is killed and restarted with the same checkpoint file, it SHALL resume from the
  last checkpoint and finish with the same residue as an uninterrupted run.
- **R2** — The checkpoint SHALL record the format version, the test, the exponent, the iteration, the
  shift, the transform length and the engine, with a checksum over the residue. A file that fails its
  checksum, or names another test, exponent or format version, SHALL NOT be resumed.
- **R3** — The checkpoint SHALL be written to a temporary name and renamed into place, so an
  interruption never leaves a partial file under the real name.
- **R4** — Only a good state (every error check passed at its iteration) SHALL be written.
- **R5** — `-f PATH` SHALL name the file; the default stays `<PVAL>.mps`.
- **R6** — The Lucas–Lehmer test SHALL reproduce the `ll` final residues at 1,000,193 (and larger,
  as shaping sets), and end in zero for the known Mersenne primes up to a bound shaping sets, as runs
  outside `make check` (a long target or cluster jobs).

## Notes

- Seed sketch carried from `issues/lucas-lehmer-test.md` (its R3, R7 and R8, and its Problem's
  "Checkpoints" bullet).
- Prime95 keeps two save files that passed the Jacobi check; shaping decides how many this keeps.
- § Conventions: a change to a checkpoint format bumps its version in the same commit; `src/io.*` is
  on the high-blast-radius list.
- `ROADMAP.md`'s PRP entry depends on this seed.
- Related: `spec/lucas-lehmer-test/` (good states, the recovery), `issues/prp-gerbicz-test.md`,
  `issues/pminus1-factoring.md`, `issues/known-primes-campaign.md`.
- Found by: `lucas-lehmer-test` shaping, 2026-10-31.
