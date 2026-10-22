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
