---
status: unshaped
kind: feature
appetite: small
---

# A standing check that the known-answer tables only grow

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

§ Invariants 3 makes the known answers append-only: a value is never edited, and a correction is a
new row that names the row it supersedes. The tables in `tests/known/` hold those answers
(`mersenne-exponents.txt`, `final-residues.txt`, `partial-residues.txt`), and `make check` refuses a
malformed table, a row without a source and a key with two answers, through `src/known.*`. Nothing
that keeps running checks that a table only grew: an edited Res64, a deleted row, or rows reordered
above existing ones all load, and pass `make check` whenever the edit is self-consistent.

Evidence:

- `known-answer-tables` R4 required each table's diff against `origin/main` to add records after the
  existing ones and change none. Its gates check it once, in
  `spec/known-answer-tables/verify/tables.py` (each table on `origin/main` must be a prefix of the
  new one, source lines and rows), which becomes a record when that cycle lands. No later cycle's
  gate checks it unless that cycle's plan asks.
- `make check` reads no history: the loader sees one table's text, and the cycle's R11 kept
  `make check` from running any tool beyond what it used, `git` included.
- The next cycles that append are the engine cycles (`issues/ibdwt-transform-engine.md`,
  `issues/ntt-transform-engine.md`), burn-in (`issues/burn-in-mode.md`) and the factor tables
  (`issues/known-factor-tables.md`), each adding rows to a table that exists.

## Why it was deferred

Out of `known-answer-tables`' scope: its R4 bound that cycle's own diff, which its gates check, and
a standing check needs a decision on where history is read (§ Outcome). Named by that cycle's
adversarial design review (F11) and deferred from `known-answer-tables` at its P4, 2026-10-17. New
work, not a defect: the rule holds on `main` today.

## Outcome / vision

Every change to a table that would edit, delete or reorder an existing record fails before it lands,
whatever cycle makes it, and an append, or a correction row that supersedes by value, passes.

Shaping chooses where the check runs. Options seen so far:

- a make target beside `make check` that compares each table with the base branch through `git`
  (`make check` itself stays free of it, as its timing and tool rules want);
- a step in the cluster test job or the landing recipe, which already read history;
- a check in `make check` against a committed record of each table's existing body (a count and a
  digest of its rows), which only says something if that record is itself held append-only.

## Sketch of the acceptance criteria

- **R1** — WHEN a table in `tests/known/` changes against the base branch, the check SHALL pass only
  if the base's source lines and rows are a prefix of the new table's, header and trailer lines
  excepted.
- **R2** — IF a record is edited, deleted or moved, THEN the check SHALL fail and name the table and
  the first line that differs.
- **R3** — A new table SHALL pass, and the check SHALL say that it had no prior version to compare.

## Notes

- Deferred from `known-answer-tables` (P4, its deferral ledger): the standing form of its R4, which
  its gates check once.
- Two recorded risks from that cycle's design review (its `PLAN.md` § 5), which a standing check
  meets first: if GIMPS ever lists a Mersenne prime below 136,279,841 (the 52nd's rank is
  provisional), the exponent table's ascending order and append-only growth can't both hold, and a
  GOAL amendment decides; and an exponent row has no correction form, since it carries no value, so
  a correction there needs a new version of the format (its magic line, `# mps known 1`).
- **The vector files' append-order trap** (found in `reference-big-integer` review cycle 3; carried
  here from the retired `known-answer-tables` seed, 2026-10-17). § Invariants 3 binds
  `tests/vectors/` too. `tests/vectors/hex.txt` and `decimal.txt` hold one record per operand of
  `every_operand()` in `tests/vectors/generate.py`, in its order: the edges and M_p of the folding
  exponents, then every operand of the `arith`, `folding`, `products` and `modular` families, then
  M_p of the `COMPOSITE` exponents. A cycle that adds a **new** operand to any of those families, or
  an exponent in its sorted place in `COMPOSITE`, puts its records before existing ones, in the
  middle of both files. A new operand goes at the very end of `every_operand()`'s list; a reorder of
  its families must leave every existing record where it is. Shaping decides whether this check covers `tests/vectors/`.
- Related: `spec/known-answer-tables/` (GOAL R4, PLAN § 2's correction rule, `verify/tables.py`);
  `issues/doc-claims-standing-check.md` and `issues/ci-job-standing-checks.md`, the same gap for the
  notes and the cluster job.
- Found by: `known-answer-tables` design review (F11), 2026-10-17; filed at its P4.
