---
status: unshaped
kind: feature
appetite: big
---

# PRP with the Gerbicz error check

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

`mps` has no Fermat probable-prime (PRP) test. PRP matters here for two reasons.

**Detection.** Start from x_0 = 3 and square p times: x_p = 3^(2^p) = 3^(M_p + 1), and M_p is a
probable prime if and only if x_p = 9. Robert Gerbicz (2017) showed that this chain can be checked as
it runs. Let d_k be the product of the residues saved every L iterations. Because
x_{(j+1)L} = x_{jL}^(2^L), the identity d_{k+1} = x_0 · d_k^(2^L) holds, and it can be verified with
L extra squarings. An arithmetic error anywhere in the block breaks the identity except with
negligible probability. With L = 1000 and a comparison every 10^6 iterations the cost is about 0.2%,
Prime95's documented default. On a mismatch the run reloads the last verified state and redoes the
interval. Prime95 says the check "almost guarantees a correct result" even on flaky hardware. That
is what a burn-in wants.

**Practice.** GIMPS stopped issuing first-time Lucas–Lehmer tests on 2021-04-08. Production is PRP
with this check plus a proof, and LL only confirms a probable prime. PRP needs only squaring modulo
M_p: no GCD and no Jacobi symbol. It is the first test to build on the engine.

The check has blind spots, and they define what the known-answer tests must still cover:

- **Self-consistent bugs.** Arithmetic that is wrong but consistent, such as a wrong modulus, wrong
  weights or a carry bug, passes the check.
- **The tail.** The iterations after the last full block are uncovered, because p is prime and never
  a multiple of L.
- **Outside the chain.** The seed, the final multiplication by 9⁻¹, Res64 extraction and I/O are
  never checked.

## Why it was deferred

Never started. It depends on the transform engine (`issues/ibdwt-transform-engine.md`) and shares the
checkpoint format with `issues/lucas-lehmer-test.md`.

## Outcome / vision

`mps -t prp p` runs a base-3 PRP test with the Gerbicz check and a per-run shift on the selected
engine. It reports GIMPS's type-1 residue, r = 3^(M_p − 1) = x_p · 9⁻¹ mod M_p, which is 1 for a
prime (Res64 `0000000000000001`). GIMPS stores PRP results with type 1 and base 3, so `mps`'s
residues compare directly with the published ones. Save files are written only from verified state.
The run records the checks passed and the rollbacks taken, and the tail and finalization get their
own check. The Gerbicz check makes PRP the default arithmetic of every burn-in profile
(`issues/burn-in-mode.md`).

## Sketch of the acceptance criteria

- **R1** — For every known Mersenne prime exponent up to the bound set at shaping, the test SHALL
  report a probable prime.
- **R2** — For composite exponents in the known-answer tables, the type-1 Res64 SHALL match the
  published residue: M28603 → `F899E5F1E2B40323`, M58199 → `1EF552BF36C40C97`,
  M63703 → `9B67A35DA6C9F042`.
- **R3** — WHEN an error is injected into the residue or into d during a run, the next Gerbicz check
  SHALL detect it, and the run SHALL resume from the last verified state and finish with the correct
  residue.
- **R4** — IF checks keep failing past a limit, THEN the run SHALL report an error, exit non-zero,
  and print no verdict.
- **R5** — The iterations after the last full block, and the final multiplication by 9⁻¹, SHALL be
  covered by a check of their own.
- **R6** — The result SHALL record the block size, the compare interval, the checks passed and the
  rollbacks taken.
- **R7** — At the default interval, the Gerbicz check SHALL cost no more than about 0.5% of the run's
  time, as measured by `mps bench`.

## Notes

- Shaping decides whether to support type 5 and PRP-CF, which are cofactor tests on Mersenne numbers
  with known factors. GIMPS takes the type-5 residue modulo the full number, not the cofactor.
- PRP proofs are a separate entry, `issues/prp-proofs.md`.
- References: GIMPS, "The Math", https://www.mersenne.org/various/math.php; Prime95 `undoc.txt`
  (residue types, compare interval); D. Li and Y. Gallot, "An efficient modular exponentiation proof
  scheme", arXiv:2209.15623, 2022 (the check generalised to any exponent); K. Pietrzak, "Simple
  verifiable delay functions", ITCS 2019, doi:10.4230/LIPIcs.ITCS.2019.60.
- Found by: factory adoption review, 2026-10-11.
