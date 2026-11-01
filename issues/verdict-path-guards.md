---
status: unshaped
kind: tests
appetite: small
---

# Pin the verdict path's guards in `make check`

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The verdict path's behaviour is right and was verified exhaustively by `verdicts-and-exit-statuses`
review cycle 2 (2026-10-12). What is missing is regression protection: three guards can be broken
while `make check` stays green.

- **G1.** The exponent check's exact re-check is pinned in `check_exponent_by` only. Rewriting
  `check_exponent`, or building the composite in `src/main.cpp`, stays green. Pinning it needs a
  fault on the production path, a search that lies inside `bin/mps`.
- **G2.** `utf8_length`'s table bounds in `src/result.cpp` are under-pinned. These pass `make check`
  and produce invalid records:
  - E0 or F0 taking an overlong second byte;
  - leads F5–F7;
  - unchecked third or fourth bytes.
  Check every lead and continuation boundary against an independent validator.
- **G3.** `verdict_line` writes a factor as it is, and so does the contradiction line on stderr. Put
  factors, and every string on those lines, through `line_text` in `src/result.cpp`, as reasons
  are.

## Why it was deferred

Deferred from `verdicts-and-exit-statuses` (review cycle 2, by Geoffrey's decision, 2026-10-12) to
`issues/lucas-lehmer-test.md`, the first cycle with real verdicts. Its shaping split it out again
(foreman's ruling, 2026-10-31; `spec/lucas-lehmer-test/` Clarifications, Q5): none of the three
touches the Lucas–Lehmer driver, and that pilot was already past the soft limit. Pre-existing on
`main`; no wrong output is known, only missing pins.

## Outcome / vision

Each guard has a check that fails when the guard is removed or weakened.

## Sketch of the acceptance criteria

- **R1** — IF the exponent check's re-check is bypassed on the path `bin/mps` runs, THEN a check SHALL
  fail.
- **R2** — `utf8_length` SHALL agree with an independent UTF-8 validator at every lead and
  continuation boundary, overlong forms and leads F5–F7 included.
- **R3** — Every string on the verdict line and the contradiction line, factors included, SHALL pass
  through `line_text`, and a check SHALL show a control character in a factor written as `\x` and
  two hexadecimal digits.

## Notes

- `src/result.*`, `src/math.*` and `src/main.cpp` are high-blast-radius paths (§ Invariants).
- Related: `spec/verdicts-and-exit-statuses/`, `spec/lucas-lehmer-test/`.
- Found by: factory adoption review, 2026-10-11; `verdicts-and-exit-statuses` review cycle 2,
  2026-10-12.
