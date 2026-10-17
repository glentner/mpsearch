---
status: unshaped
kind: feature
appetite: small
---

# The known answers: exponents, residues and factors, each with its source

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Every verdict test and every burn-in run compares against an expected value, and the repo holds
none. § Invariants 3 makes expected values fixed points: append-only, each with its source, never
edited to make a test pass. That rule needs a home, a format and a loader before any engine is graded
against it. Without them each cycle invents its own constants, and a wrong constant hides exactly
the hardware fault the burn-in exists to catch.

"Prime" is the weakest known answer. A Lucas–Lehmer residue of zero can come from a zeroed buffer,
and the Gerbicz check can't see a self-consistent arithmetic bug. The strongest known answers are
composite exponents with non-zero published residues, plus residues after a fixed number of
iterations.

Five kinds of answer are needed:

- **The 52 known Mersenne prime exponents**, 2 through 136,279,841. None has been found since
  October 2024. The rank of #52 is provisional, since not every smaller exponent has been verified.
- **Lucas–Lehmer and PRP residues of composite exponents**, across sizes. GIMPS publishes them, and
  several reproduce exactly from the definitions:
  - LL: M1237 → `DF5E6622F83F5BE2`, M1277 → `5613A480590E78BA`;
  - PRP type 1: M28603 → `F899E5F1E2B40323`.
- **Partial-run residues.** For example, s_100 of M3888509 is `71E61322CCFB396C`, the Mlucas
  self-test value. Short runs on huge exponents then still carry a known answer.
- **Factors.** Small ones (M11: 23, 89; M23: 47; M29: 233, 1103, 2089). The 64-bit boundary
  (M141900347: q = 18933328644689502913). 77-bit (M141900047: q = 114827397821638003622009). And P-1
  boundary cases:
  - M1022191's 97-bit factor is found if and only if B1 ≥ 15887 and B2 ≥ 2050333;
  - M1003417 needs B1 ≥ 181219 and B2 ≥ 39340463;
  - M141933599 needs B1 ≥ 869291 and B2 ≥ 200514673.
- **Burn-in fingerprints.** Residues after a fixed number of iterations at a fixed transform length
  and shift, agreed by two independent engines.

## Why it was deferred

Never started. It sat with the reference big integer because both precede every engine cycle. That
type landed on `main` on 2026-10-16 (`spec/reference-big-integer/`), so the computed rows can be
built now.

## Outcome / vision

A small set of plain-text tables under `test/`, readable by people and loaded by one C++ module.
Every row carries its source: a published result (URL and retrieval date) or the exact command and
engine that computed it.

- **Published rows** come from mersenne.org's plain-CSV reports: `report_LL`, `report_prp` and
  `report_factors` with `txt=1`, which accept scripted requests. mersenne.ca now challenges scripts.
  A row is accepted only when two or more shifts or programs agree and the error code is empty.
- **Computed rows** for small exponents come from the reference big integer, checked by an
  independent method.
- **Fingerprints** are added only when two engines that share no arithmetic agree, such as the FP64
  transform and the integer transform.

A check in `make check` fails on any row without a source, and on any key with two different answers.

## Sketch of the acceptance criteria

- **R1** — Every expected value SHALL carry a source: a URL with its retrieval date, or the command
  and engine that computed it.
- **R2** — `make check` SHALL fail IF a row lacks a source, or IF one key appears with two different
  values.
- **R3** — The table of Mersenne prime exponents SHALL list all 52 known exponents as of its
  retrieval date, and SHALL be the only place tests and burn-in profiles read them from.
- **R4** — A burn-in fingerprint row SHALL record the exponent, test, residue type, transform length,
  shift, iteration count and Res64, and SHALL name the two engines that agreed on it.
- **R5** — The tables SHALL be append-only: a correction adds a superseding row with its reason and
  never edits the old one in place.
- **R6** — Every published row SHALL satisfy the acceptance rule above: agreement across shifts or
  programs, and no error code.

## Notes

- Shaping decides the file format (one table per kind, with a source column) and how many rows go in
  each transform-length band.
- The factor report lists found factors only; a prime cofactor is implicit (M11 lists 23, not 89). A
  range 10^5 wide returned exactly 5,000 rows, which looks like a cap, so page in narrow ranges.
- P-1 rows need no GCD to check: test x ≡ 1 (mod q) for the known factor q.
- Every table becomes a high-blast-radius path in the commit that creates it (§ Invariants).
- References: GIMPS, "List of Known Mersenne Prime Numbers", https://www.mersenne.org/primes/; GIMPS
  reports, https://www.mersenne.org/report_LL/, https://www.mersenne.org/report_prp/,
  https://www.mersenne.org/report_factors/; E. Mayer, Mlucas README (self-test residues).
- The prime list exists already (`reference-big-integer`, its P5 ledger, 2026-10-16). The Mersenne
  prime exponents up to 10000, 2 to 9941, live in `tests/vectors/mersenne-exponents.txt`, with their
  source (OEIS A000043, `doc/references.md` entry `OEIS-A000043`) and the command that reproduces the
  file, which embeds the terms and exits non-zero unless a Lucas–Lehmer sweep in Python over every
  odd prime up to 10000 finds exactly the odd ones. `make check` reads it (`tests/check_reference.cpp`).
  This cycle takes the file over as R3's single table. Moving it keeps the old path on the
  high-blast-radius list (the list only widens) and adds the new one. Rows above 10000 cannot be
  recomputed by that command, whose sweep stops at 10000: they need a published source per row, and
  a command that reproduces the file without claiming to recompute them.
- The generator's sanity check (`reference-big-integer` P5 ledger). `tests/vectors/generate.py`
  computes the Lucas–Lehmer residues of M1237 and M1277 among its composite set, and its comment
  points here for their published Res64, `DF5E6622F83F5BE2` and `5613A480590E78BA` above, since
  `doc/references.md` has no entry for the GIMPS reports. When this cycle lands the published
  residues with their sources, repoint that comment at the table: this file goes when the cycle
  lands.
- The vector files' append order (found in `reference-big-integer` review cycle 3).
  `tests/vectors/hex.txt` and `tests/vectors/decimal.txt` hold one record per operand of
  `every_operand()` in `tests/vectors/generate.py`, in its order: the edges and M_p of the folding
  exponents, then every operand of the `arith`, `folding`, `products` and `modular` families, then
  M_p of the `COMPOSITE` exponents. A later cycle that appends a record with a **new** operand to
  any of those families (a `POWERS` or other modular record among them) puts its new `hex` and
  `dec` records before the `COMPOSITE` M_p records, in the middle of both files, against
  § Invariants 3's append-only rule. A new operand must land after every existing one: append new
  operands at the very end of `every_operand()`'s list, or reorder its families only in a way that
  leaves every existing record where it is. `reference-big-integer` avoided it by giving its four
  appended `powmod` records existing operands only, so both files stayed unchanged.
- Found by: factory adoption review, 2026-10-11.
