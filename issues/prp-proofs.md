---
status: unshaped
kind: feature
appetite: big
---

# PRP proofs: a result anyone can verify cheaply

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

A PRP result is only as trustworthy as the hardware that ran it. The Gerbicz check makes an
undetected error rare, but a second party who wants certainty still has to repeat the whole test.

Pietrzak's verifiable delay function changes that. To prove y = x^(2^T), the prover sends the
midpoint μ = x^(2^(T/2)). A verifier's random challenge (replaced by Fiat–Shamir hashing) folds the
claim into one of half the length, and halving k times leaves the verifier T/2^k squarings. GIMPS
adopted this in Prime95 30.3 (2020) in place of double-checking. Certification runs 128–1,024× faster
than the test, for proof power 7 to 10. The costs grow with power. At p = 10^8, power 8 needs 3.2 GB
of temporary disk per worker and yields a 113 MB proof; power 10 needs 12.8 GB and 138 MB.

For `mps`, proofs would let a burn-in on one node be verified on another at a 1/256 to 1/1,024
fraction of the cost. That is a cheap cross-node check for exponents outside the known-answer tables.
They are also the last piece of a modern PRP implementation. They catch honest errors, not a
malicious prover.

## Why it was deferred

Not needed for the definition of done, which is Lucas–Lehmer on the known primes, or for burn-in,
which compares against known answers.

## Outcome / vision

- `mps -t prp p --proof POWER` saves the midpoint residues during the test and writes a proof file
  beside the result, in a versioned, documented format.
- `mps verify FILE` checks a proof and reports the cost relative to a full test.
- Disk use is bounded and printed before the run starts.

## Sketch of the acceptance criteria

- **R1** — WHEN a PRP run with proofs finishes, `mps verify` SHALL accept its proof and confirm the
  final residue.
- **R2** — IF any residue in the proof is altered, THEN `mps verify` SHALL reject the proof.
- **R3** — Verification SHALL cost about T/2^POWER squarings, as measured by `mps bench`.
- **R4** — WHERE the temporary disk a power needs exceeds the limit given, `mps` SHALL refuse to
  start, or lower the power and say so.
- **R5** — The proof format SHALL be versioned and documented in `doc/`.

## Notes

- GIMPS's proof file layout and hash construction are not documented in prose that was reachable
  on 2026-10-11, beyond a JSON block carrying version 2, power, hashsize 64 and an MD5. Matching it
  would need a specification from GIMPS's maintainers, never their code (§ Invariants 4). Otherwise
  `mps` defines its own format. Shaping decides.
- References: K. Pietrzak, "Simple verifiable delay functions", ITCS 2019,
  doi:10.4230/LIPIcs.ITCS.2019.60; B. Wesolowski, "Efficient verifiable delay functions",
  *J. Cryptology* 33, 2020, doi:10.1007/s00145-020-09364-x; D. Li and Y. Gallot, "An efficient
  modular exponentiation proof scheme", arXiv:2209.15623, 2022; Prime95 `readme.txt` (proof power,
  disk and certification costs).
- Found by: factory adoption review, 2026-10-11.
