---
status: unshaped
kind: feature
appetite: big
---

# A GCD and Jacobi symbol that scale to the largest exponents

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Two features need a number-theoretic function of a p-bit residue and M_p:

- P-1 needs gcd(x − 1, M_p) at the end of each stage (`issues/pminus1-factoring.md`);
- the Lucas–Lehmer error check needs the Jacobi symbol (s − 2 | M_p) (`issues/lucas-lehmer-test.md`).

Both are quadratic with the textbook algorithms: binary, Euclid, or Lehmer. For p near 136 million
that is about 2 million 64-bit limbs, roughly 10^13 word operations per call. That is 15–60 minutes
on one core, about as long as a GPU's whole P-1 stage 1. The subquadratic half-GCD (Schönhage;
Möller 2008) brings both down to O(M(n) log n). Möller measured it about 30× faster than an
accelerated quadratic GCD already at 10^5 words. A Jacobi variant exists (Brent and Zimmermann 2010).
Built on the transform engine's plain multiplication (zero-padded, unweighted), 2×2 matrix products
and Newton division, one GCD at the top of the range is estimated at 600–1,800 squaring-equivalents:
seconds to minutes. That is under 0.1% of a B1 = 10^6 stage 1, and it can run on the CPU while a GPU
continues.

The established programs lean on GMP for this step. § Invariants 4 and 5 rule that out here.

## Why it was deferred

Small exponents can use the quadratic algorithms on the reference big integer. This entry is what
P-1 and the Jacobi check need before they can run at the top of the exponent range.

## Outcome / vision

A half-GCD and a subquadratic Jacobi symbol on `mps`'s own multiplication, with quadratic base cases
on the reference big integer. At p near 136 million, a GCD and a Jacobi symbol each finish in at most
a few minutes on the M4 Max. They are validated against the quadratic algorithms at sizes
where both run.

## Sketch of the acceptance criteria

- **R1** — For random inputs up to the bound where the quadratic algorithm finishes quickly, the
  subquadratic GCD and Jacobi SHALL equal the quadratic results.
- **R2** — For the factor cases in the known-answer tables, gcd(x − 1, M_p) on a stage-1 residue
  SHALL return the expected factor.
- **R3** — At an exponent near the largest known Mersenne prime, one GCD and one Jacobi symbol SHALL
  each complete within a bound set at shaping, measured on the M4 Max.

## Notes

- Expected research: half-GCD on top of a transform multiplier (the operand sizes vary, so the
  engine needs general multiply at arbitrary lengths, not just squaring mod M_p); the crossover from
  quadratic to subquadratic.
- Möller's paper compares subquadratic variants and reports the simplest at about a third of the
  code of the most elaborate, a fair guide to what is worth owning.
- References: A. Schönhage, "Schnelle Berechnung von Kettenbruchentwicklungen", *Acta Informatica* 1
  (1971); N. Möller, "On Schönhage's algorithm and subquadratic integer gcd computation",
  *Mathematics of Computation* 77(261), 2008, doi:10.1090/S0025-5718-07-02017-0; D. Stehlé and
  P. Zimmermann, "A binary recursive gcd algorithm", ANTS VI, LNCS 3076, 2004; R. P. Brent and
  P. Zimmermann, "An O(M(n) log n) algorithm for the Jacobi symbol", ANTS IX, LNCS 6197, 2010.
- Found by: factory adoption review, 2026-10-11.
