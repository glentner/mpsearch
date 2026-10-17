---
status: unshaped
kind: feature
appetite: small
---

# A standing check for the notes' numbers and citations

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Every concrete number in `doc/` sits in a check span, an HTML comment holding an integer expression
that must evaluate to True and match the integers the text shows, or in a cited span beside a
located citation. Every citation must resolve to an entry in `doc/references.md`, and every entry
must be cited. Once `math-notes-and-references` merges, nothing the repository runs checks any of
this.

Evidence:

- The evaluator and its functions live in `spec/math-notes-and-references/verify/` (`notes.py`,
  `claims.py`, `refs.py`), and only that cycle's phase gates `p1.sh` to `p5.sh` run them. Once the
  cycle merges they are records of the cycle, not standing gates.
- `make check` runs `bin/check` and `tests/cli.sh` and nothing else; nothing under `tests/` or in
  the `Makefile` reads `doc/`. The cycle's GOAL kept its checks out of `make check` unless the plan
  argued otherwise (`spec/math-notes-and-references/GOAL.md`, R7), and the plan did not.
- `AGENTS.md` § Conventions now requires a cycle that implements or changes an algorithm to update
  that algorithm's note in the same commit. The plan records that the convention requires the update,
  not the checks (`spec/math-notes-and-references/PLAN.md` § 5, "The markers bind future edits"). So
  the first algorithm cycle can edit a note, break a span or a citation, and pass every check the
  repository runs.
- `doc/README.md` points readers to `spec/math-notes-and-references/verify/claims.py` and
  `notes.py claims` for the functions and the evaluator: paths inside a cycle record.

## Why it was deferred

Introduced by `math-notes-and-references`, which added `doc/` and its markers. That cycle changes no
code, and its GOAL leaves `tests/` and the `Makefile` alone unless the plan put the checks in
`make check`; the plan kept them as the cycle's gates.

## Outcome / vision

A standing check that every cycle touching `doc/` runs: each check span in each note evaluates to
True and matches its text, no number of 100 or more stands outside a span, every citation resolves,
and every entry is cited by a note or a `src/` comment. The evaluator and its functions move out of
the cycle record to a durable home that `doc/README.md` names. Shaping decides where the check runs
(`make check`, a target of its own, or the cluster job), given that it needs `python3` and the
standing gate so far needs no interpreter, and whether the network check of the references runs
outside a cycle at all.

## Sketch of the acceptance criteria

- **R1** — The evaluator and the check functions SHALL live outside `spec/`, and `doc/README.md`
  SHALL name that home.
- **R2** — WHEN the standing check runs, it SHALL fail, naming the file and line, on any check span
  that does not evaluate to True, or whose text shows an integer the expression neither writes nor
  computes.
- **R3** — WHEN the standing check runs, it SHALL fail on any integer of absolute value 100 or more
  outside a span, any citation that resolves to no entry in `doc/references.md`, and any entry that
  no note and no `src/` comment cites.
- **R4** — The check SHALL never link into `bin/mps` (§ Invariants 5), and SHALL need no network.

## Notes

- Related: `spec/math-notes-and-references/verify/notes.py` and `claims.py`, which the check can
  start from; `spec/math-notes-and-references/PLAN.md` § 2 ("Citations" and "Claims") for the rules;
  `issues/ci-job-standing-checks.md`, the same gap for the cluster job.
- `claims.py` bounds every function's arguments, so the checks of all seven notes, with the
  structure and citation rules, ran in about 15 s on the author's Mac.
- Found by: `math-notes-and-references` P5 (the deferral ledger).
- The rule covers small numbers too. `reference-big-integer`'s review (cycle 1, F1) found worked values
  below 100 stated outside markers in `doc/prp-and-gerbicz.md` and `doc/reference-arithmetic.md`,
  because that cycle's docs gate enforced markers only on numbers of 100 or more. That review's
  cycle 3 ended in Geoffrey's ruling on what "concrete number" covers, which `doc/README.md` now
  states: definitions, format constants and theorem values (the general results a proof
  establishes) take no markers; a number stated about a specific instance, or a restatement of a
  number the note marks elsewhere, does. The standing check enforces the rule as amended.
- Shaping also settles a convention `doc/README.md` leaves open: whether and how a note may state a
  measured value, such as a threshold or a time measured on one machine. No check computes it and no
  work publishes it, so neither marker can carry it; until the convention exists, a note states no
  measured value and points to the constant in the code that holds it.
- The check should also verify that every locator a note cites (a section, theorem, exercise or
  algorithm) is covered by that entry's verification record in `doc/references.md`.
  `reference-big-integer`'s review (cycle 4) found that `BrentZimmermannMCA2010`'s record lists only
  §§ 2.4.1, 2.4.2 and 2.9 as read, while the notes cite §§ 1.3–1.7, 2.4.4 and 2.6.1 and Exercises
  1.19 and 1.20. The same review found that MCA's bibliography dates Knuth's third edition 1998,
  where `Knuth1997` gives 1997 from Knuth's page.
