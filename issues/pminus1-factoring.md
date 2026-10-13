---
status: unshaped
kind: feature
appetite: big
---

# P-1 factoring: stage 1 and stage 2

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

P-1 is the middle test of the trio, and at `257acb7` it is a stub that returns `true`
(`test_p_minus_one`). Pollard's method finds a factor q of M_p when q − 1 is smooth. Every factor has
the form q = 2kp + 1, so the exponent can start at 2p and only k needs to be smooth. That makes P-1
unusually strong on Mersenne numbers.

- **Stage 1** computes x = 3^(2pE) mod M_p, with E = ∏ r^⌊log_r B1⌋ over primes r ≤ B1, then
  gcd(x − 1, M_p). It costs about 1.44·B1 squarings. At current GIMPS bounds (B1 ≈ 1.2×10^6, near
  p = 1.42×10^8) that is about 1.2% of a PRP test.
- **Stage 2** extends the search to one larger prime in (B1, B2]. GIMPS runs B2 near 1.4×10^9, about
  1,150·B1. The options are prime pairing and baby-step giant-step (Montgomery 1987), and the FFT and
  polynomial continuations (Montgomery and Silverman 1990; Montgomery and Kruppa 2008). Prime95 30.8
  added a polynomial stage 2, "substantially faster" when memory is plentiful.

Stage 2 "loves memory": doubling the memory doubles the throughput. An effective run near
p = 1.5×10^8 wants about 32 GB. A crude estimate puts the yield at about 2% for stage 1 alone and
7–8% with stage 2, so stage 2 carries the yield. That needs checking against GIMPS's own estimator
before the cycle commits to it.

## Why it was deferred

It is not on the burn-in's critical path, which needs PRP and Lucas–Lehmer, but a feature-complete
trio requires it. It depends on the engine's general multiply. Its GCD is quadratic until
`issues/large-gcd-and-jacobi.md` lands: tens of minutes at the top of the range, which is tolerable
for a first release.

## Outcome / vision

`mps -t p1 p --b1 B1 [--b2 B2]` runs stage 1 and, optionally, stage 2. It checkpoints both stages in
the shared `.mps` format, reports any factor found (re-checked) or the bounds searched, and sizes
stage 2 to the memory it is given. Without explicit bounds it picks them by a stated rule that weighs
success probability against the cost of the primality test it might save, and prints that
probability. Stage 1 is a left-to-right exponentiation, so it can carry a Gerbicz–Li check.

## Sketch of the acceptance criteria

- **R1** — For the P-1 boundary cases in the known-answer tables, a run SHALL find the factor exactly
  when B1 and B2 are at or above the case's bounds, and SHALL miss it when B1 is one below. For
  example, M1022191: B1 ≥ 15887 and B2 ≥ 2050333.
- **R2** — Stage 1 alone SHALL find a factor whose k is B1-smooth.
- **R3** — A reported factor SHALL be re-checked with the reference big integer before printing
  (§ Invariants 1).
- **R4** — WHEN a run is interrupted in either stage, it SHALL resume from its checkpoint with the
  same outcome.
- **R5** — WHERE bounds are chosen automatically, `mps` SHALL print the bounds and the estimated
  probability of finding a factor.
- **R6** — Stage 2 SHALL run within a memory limit given on the command line, reporting the limit
  and its effect on the plan.

## Notes

- The Mac's 36 GB is unified memory shared with the GPU. Stage 2 near the largest exponents is
  memory-bound there, so the limit in R6 matters.
- The test cases need no GCD: x ≡ 1 (mod q) for the known factor q confirms that a stage would have
  found it.
- References: J. M. Pollard, "Theorems on factorization and primality testing", *Proc. Cambridge
  Philos. Soc.* 76, 1974, doi:10.1017/S0305004100049252; P. L. Montgomery, "Speeding the Pollard and
  elliptic curve methods of factorization", *Mathematics of Computation* 48(177), 1987,
  doi:10.1090/S0025-5718-1987-0866113-7; P. L. Montgomery and R. D. Silverman, "An FFT extension to
  the P−1 factoring algorithm", *Mathematics of Computation* 54(190), 1990, doi:10.2307/2008514;
  P. L. Montgomery and A. Kruppa, "Improved stage 2 to P±1 factoring algorithms", ANTS VIII,
  LNCS 5011, 2008, doi:10.1007/978-3-540-79456-1_12.
- Open from `math-notes-and-references` (its P5 ledger, 2026-10-12). Pollard 1974 refused every fetch
  and has no open copy, so `doc/p-1.md` cites it without a locator, as the origin of the method, and
  attributes no stage 2 to it. Montgomery and Silverman's abstract says Pollard's paper stated the
  algorithm in theoretical terms that use FFT techniques. Read Pollard 1974 before attributing a
  stage 2 to it or building from it, and add a located citation to the note. The faster stage 2s
  (Montgomery 1987, Montgomery–Silverman 1990, Montgomery–Kruppa 2008) were read only in their
  abstracts; a stage 2 built from one reads it in full.
- Found by: factory adoption review, 2026-10-11.
