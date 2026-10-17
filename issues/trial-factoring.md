---
status: unshaped
kind: feature
appetite: big
---

# Trial factoring to a chosen bit depth

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Trial factoring is the cheapest of the three tests and the first one run on any exponent. At
`257acb7` it was a stub that returned `true`; since `verdicts-and-exit-statuses` it returns no
verdict. The author's uncommitted 2024 work shows the right idea:

- walk q = 2kp + 1;
- keep only q ≡ ±1 (mod 8);
- test whether 2^p ≡ 1 (mod q) by square-and-double.

But the arithmetic breaks early. `square *= square` overflows 64 bits once q ≥ 2^32, `1 << --bit`
is a 32-bit shift (undefined for bit ≥ 31), and the walk stops at a ten-million-entry sieve.

Useful depths are far beyond that. Prime95's CPU defaults factor exponents up to 77.9M to 2^70, and
up to 153.4M to 2^73. GPU factoring goes about 4 bits deeper: 2^77 near 142M. 96-bit arithmetic
covers every depth GIMPS uses (≤ 2^81). Each bit level from 2^b to 2^(b+1) finds a factor with
probability about 1/b.

Throughput comes from a known structure. Split k into residue classes modulo 4620 = 4·3·5·7·11.
Only 960 of those classes can give q ≡ ±1 (mod 8) with q free of 3, 5, 7 and 11, and each class is
an independent progression that can be checkpointed on its own. Sieving each class by small primes,
up to about 40,000, removes roughly 95% of candidates before any modular exponentiation. The same
structure is what GPU kernels parallelise later (`issues/gpu-trial-factoring.md`).

## Why it was deferred

Pre-existing as a stub. It needed a result type that can carry a factor, which
`verdicts-and-exit-statuses` landed on 2026-10-12 (`spec/verdicts-and-exit-statuses/`). It also
needed the reference big integer (re-checking a hit, § Invariants 1), which landed on 2026-10-16
(`spec/reference-big-integer/`), so that constraint is cleared too.

## Outcome / vision

`mps -t tf p --bits B` searches every candidate q = 2kp + 1 below 2^B and handles each surviving class
exactly once. Modular arithmetic is exact for q up to at least 2^96: one 64-bit word with 128-bit
products, then multi-word Montgomery or Barrett. The run reports every factor found, each re-checked,
or "no factor below 2^B". It also reports candidates per second for `mps bench`. Start and end depths
make a depth range resumable.

## Sketch of the acceptance criteria

- **R1** — WHEN M_p has a known factor below 2^B in the known-answer tables, `mps -t tf p --bits B`
  SHALL report it. That includes M11 → 23, M29 → 233, M67 → 193707721, the 64-bit-boundary factor
  of M141900347 and the 77-bit factor of M141900047.
- **R2** — WHEN the tables show no factor below 2^B, the run SHALL report none, and its counters
  SHALL show each of the 960 classes tested exactly once.
- **R3** — The modular arithmetic SHALL be exact for every q below 2^B, for B up to at least 96.
- **R4** — A factor SHALL be re-checked with the reference big integer before it is printed.
- **R5** — The run SHALL report the candidates tested and the rate.

## Notes

- Shaping decides the default depth for an exponent: a GIMPS-style table by exponent range, or an
  explicit `--bits` only.
- GIMPS's current trial-factoring result format adds a CRC32 checksum (mfaktc 0.24.0 and later). It
  matters only if results are ever submitted.
- References: the form of factors (q = 2kp + 1, q ≡ ±1 mod 8) is classical: R. Crandall and
  C. Pomerance, *Prime Numbers: A Computational Perspective*, 2005; GIMPS, "The Math" (factoring
  limits and the sieve). P. L. Montgomery, "Modular multiplication without trial division",
  *Mathematics of Computation* 44(170), 1985; P. Barrett, CRYPTO '86, LNCS 263, 1987.
- Found by: factory adoption review, 2026-10-11 (the author's 2024 work-in-progress).
