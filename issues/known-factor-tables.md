---
status: unshaped
kind: feature
appetite: small
---

# The known factors: small, boundary and P-1 cases, each with its source

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Trial factoring, P-1 and the large-exponent GCD of Part VIII each grade against known factors, and
the repo holds none. Their seeds already cite "the factor cases in the known-answer tables"
(`issues/trial-factoring.md`, `issues/gpu-trial-factoring.md`, `issues/pminus1-factoring.md`,
`issues/large-gcd-and-jacobi.md`). § Invariants 3 makes those factors fixed points: append-only, each
with its source, never edited to make a test pass. A wrong factor or a wrong bound would pass a
broken factoring engine, or fail a correct one.

The cases needed:

- **Small factors:** M11: 23, 89; M23: 47; M29: 233, 1103, 2089.
- **The 64-bit boundary:** M141900347, q = 18933328644689502913.
- **A 77-bit factor:** M141900047, q = 114827397821638003622009.
- **P-1 boundary cases**, each found if and only if both bounds are met:
  - M1022191's 97-bit factor: B1 ≥ 15887 and B2 ≥ 2050333;
  - M1003417: B1 ≥ 181219 and B2 ≥ 39340463;
  - M141933599: B1 ≥ 869291 and B2 ≥ 200514673.

## Why it was deferred

Split from `known-answer-tables` at shaping, 2026-10-17, to keep that cycle small. The factors
serve only Part VIII, not the engines or the burn-in that come first. This is new work, not a defect:
nothing on `main` records a factor today.

## Outcome / vision

Factor tables in the table set `known-answer-tables` builds (`spec/known-answer-tables/`, its
R1–R4): the same directory, format, loader, per-row source, conflict check and append-only rule.
Every row carries its source: a published result (a `doc/references.md` entry and its retrieval
date) or the exact command that recomputes it by a method independent of every `mps` engine.

- **Published rows** come from mersenne.org's plain-CSV factor report, `report_factors` with
  `txt=1`, which accepts scripted requests. mersenne.ca now challenges scripts. A published row is
  accepted only when two or more results agree and the error code is empty, the rule
  `known-answer-tables` applies to residues; how that rule reads for a factor report is shaping's
  to settle.
- **Every factor row is re-checked exactly** in `make check`: 2^p ≡ 1 (mod q), with the reference
  `BigInt`, which is cheap at these sizes.
- **P-1 rows** record the bounds and enough of q − 1 = 2kp to derive them.

## Sketch of the acceptance criteria

- **R1** — Every factor row SHALL carry a source: a published result with its retrieval date, or the
  command and method that computed it.
- **R2** — `make check` SHALL re-check every factor row with exact integer arithmetic
  (2^p ≡ 1 mod q) and SHALL fail IF any row does not divide its M_p.
- **R3** — The factor table SHALL hold the small factors, the 64-bit boundary factor and the 77-bit
  factor above, each tied to its exponent.
- **R4** — Each P-1 boundary row SHALL record q, its minimum B1 and B2, and the factorization of k
  they follow from, under a stated convention for what stage 1 and stage 2 cover.
- **R5** — The factor tables SHALL be append-only through `known-answer-tables`' superseding rows,
  and each new path SHALL join the high-blast-radius list in the commit that creates it.

## Notes

- **The factor report** lists found factors only; a prime cofactor is implicit (M11 lists 23, not
  89), so a cofactor row is a computed row, M_p divided by the listed factors and checked. A range
  10^5 wide returned exactly 5,000 rows, which looks like a cap, so page in narrow ranges.
- **P-1 rows need no GCD to check:** test x ≡ 1 (mod q) for the known factor q.
- **A fourth stem of the same format and loader** (deferred from `known-answer-tables`, 2026-10-17).
  That cycle built `tests/known/` with three stems (`mersenne-exponents`, `final-residues`,
  `partial-residues`), one command, `tests/known/generate.py`, and one loader, `src/known.*`, whose
  format and refusal order are in its `PLAN.md` § 2. A factor table is a new stem: a function in the
  command, a row of the format table (ops, fields, attributes, key) in `src/known.cpp`, its path
  appended to `known::TABLES` (which `tests/check_known.cpp`'s `every known-answer table loads`
  pins), and its path on the high-blast-radius list in the commit that creates it. The key needs
  thought: a residue row's key carries one answer, so two factors of one M_p must differ in the key
  (p and q, as the exponent table's p is its whole row), or the conflict check refuses the second.
  The correction rule (`supersedes=` by value, never weaker than what it replaces) and the published
  and computed source forms carry over; the 200,000 recompute bound (`RECOMPUTED_BELOW`) is the final
  residues' alone.
- **Depends on** `known-answer-tables` (the table mechanism and loader, its R1–R4). Part VIII's
  trial-factoring and P-1 cycles need this one.
- References: GIMPS reports, https://www.mersenne.org/report_factors/; GIMPS, "List of Known
  Mersenne Prime Numbers", https://www.mersenne.org/primes/.
- Related: `issues/trial-factoring.md`, `issues/gpu-trial-factoring.md`,
  `issues/pminus1-factoring.md`, `issues/large-gcd-and-jacobi.md`; `spec/known-answer-tables/GOAL.md`
  (its first Clarification records the split).
- Found by: `known-answer-tables` shaping, 2026-10-17, splitting the factor rows out of
  `issues/known-answer-tables.md` (originally from the factory adoption review, 2026-10-11).
