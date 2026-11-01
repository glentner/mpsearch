---
status: unshaped
kind: docs
appetite: small
---

# A cumulative `make check` time baseline in the profile

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Each cycle that adds checks bounds its own growth of `make check`: `ibdwt-transform-engine` allowed
5 s over its base, and `lucas-lehmer-test` another 5 s over `bcb6fdf`. These are per-cycle bounds.
Nothing states what `make check` may cost in total, so the default gate can grow without limit, 5 s
at a time, and each shaping has to ask again. The factory's shaping rule reads cumulative budgets from
the profile (`AGENTS.md` § Factory) and never rebuilds them from earlier GOALs' Clarifications, where
a per-cycle bound reads like a ceiling; this profile has no such row.

## Why it was deferred

Found at `lucas-lehmer-test`'s shaping (2026-10-31), which took a per-cycle bound by the foreman's
ruling (its Clarifications, Q6) and filed the missing row here. A profile row with a measured
baseline is its own decision, not that cycle's scope. Pre-existing: the profile has never had one.

## Outcome / vision

The profile names the measured no-op time of `make check` (with nothing to rebuild, clang, a named
machine), the ceiling it may reach, and how a cycle that would pass the ceiling asks for more.

## Sketch of the acceptance criteria

- **R1** — The profile SHALL carry a row giving `make check`'s measured no-op time, the machine and
  compiler it was measured with, and the commit.
- **R2** — The row SHALL state a ceiling, and the rule a cycle follows when its checks would pass it.
- **R3** — The row SHALL say who updates the baseline, and when (for example, at each landing that
  changes `make check`).

## Notes

- Per-cycle bounds so far: `spec/ibdwt-transform-engine/` R12, `spec/lucas-lehmer-test/` R16.
- The engine suite's own budget (15 minutes, `make check-engine`) may belong in the same row.
- Related: `issues/engine-differential-standing-check.md`.
- Found by: `lucas-lehmer-test` shaping, 2026-10-31.
