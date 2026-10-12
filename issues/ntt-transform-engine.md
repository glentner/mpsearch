---
status: unshaped
kind: feature
appetite: big
---

# Exact integer transforms for hardware without fast FP64

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The FP64 weighted transform (`issues/ibdwt-transform-engine.md`) needs fast double precision, and
several targets lack it:

- **Apple GPUs.** Metal has no `double`.
- **TPUs.** No FP64.
- **FP64-poor NVIDIA parts.** The A10, L40S, GB10 and the RTX and RTX PRO cards issue a handful of
  FP64 results per clock per SM against 128 FP32. Single precision alone carries only a few bits
  per word at these lengths.

A number-theoretic transform (NTT) does the same convolution exactly, in modular arithmetic. The
weighted form also needs an N-th root of 2 in the field, for the IBDWT weights, and that requirement
decides which fields are usable:

- **Gaussian fields over Mersenne primes, GF(M31²) and GF(M61²).** The order of 2 is 31 or 61, so
  the N-th root of 2 is itself a power of 2. The weights become bit rotations, with no tables, and
  reduction is a shift and an add. Paired by the CRT (modulus ≈ 2^92), they hold 34.5 bits per word
  exactly, or about 39 statistically, at 4M words. That fits the largest known exponent
  (136,279,841) in 4M words, against 7.5M for FP64.
- **The prime 2^64 − 2^32 + 1.** The weighted transform works only for N dividing
  2^26·5·17·257·65537, so there are no lengths with a factor of 3. It holds 20 bits per word at
  2^23, so it is second-tier, but it is simple on a CPU with 64-bit multiplies.
- **Ordinary NTT primes below 2^32.** None admits an N-th root of 2 for N above 2^16 (exhaustive
  search, 2026-10-11). Three-prime 32-bit NTTs need 2× zero padding and move about 2.3× more DRAM
  bytes per bit than the Mersenne-prime pair. Not recommended.

This is no longer untrodden ground. In 2026 PRPLL added a GF(M31²)×GF(M61²) transform for CUDA and
OpenCL. On the largest known exponent it reports 156 µs per iteration on an RTX PRO 6000 Blackwell,
against 850 µs for its own FP64 path on the same card. It also has an FP64+GF(M31²) hybrid, its
fastest path on H100 and A100, where the exact low 31 bits relax the FP64 round-off budget.
Statistical (beyond worst-case) operation is the norm there and needs a guard check.

The target arithmetic favours the Mersenne-prime fields. On the M4 Max GPU (measured 2026-10-11),
32-bit multiply runs at 1.60 T/s and `mulhi` at 0.81 T/s. A GF(2^31 − 1) modular multiply by shift
and add reaches 0.52 T/s, twice a 31-bit Montgomery multiply at 0.26 T/s.

## Why it was deferred

The FP64 engine and its tests come first (Part II). This engine opens the Mac's GPU
(`issues/metal-backend.md`) and the FP64-poor NVIDIA parts (`issues/cuda-backend.md`), so it is built
and proven on the CPU first, where it is easy to debug. On the CPU it also becomes a second, exact
engine that shares no arithmetic with the FP64 one, which is what the burn-in fingerprints need
(`issues/known-answer-tables.md`, R4).

## Outcome / vision

- A CPU implementation of the engine contract over GF(M31²)×GF(M61²): scalar, portable, exact by
  default, with a statistical mode guarded by a range check on the CRT reconstruction. Its length
  table follows from the bounds, and the derivation is in `doc/`.
- A scalar single-field exact engine (GF(M61²) or 2^64 − 2^32 + 1) as a slow, simple test oracle.
- Hybrid hooks (FP64+M31, FP32+M31, FP32+M31+M61) designed into the contract, so the GPU cycles can
  add them.

The CPU NTT is a correctness tool and a GPU stepping stone, not a CPU production path. On CPUs, the
FP64 FFT stays faster.

## Sketch of the acceptance criteria

- **R1** — For every exponent in the known-answer tables up to the shaping bound, the integer engine
  SHALL produce the same residues as the FP64 engine and the reference big integer.
- **R2** — For each supported length, the maximum exponent in exact mode SHALL follow from the
  worst-case convolution bound, with the derivation in `doc/`.
- **R3** — IF a statistical-mode reconstruction falls outside its guard range, THEN the engine SHALL
  raise an error instead of returning a residue.
- **R4** — The carry and CRT reconstruction SHALL be tested across the full range of bits per word
  the length table allows, low end included.
- **R5** — The engine SHALL implement the same contract as the FP64 engine, so every test and
  profile runs on either without change.

## Notes

- Shaping decides whether statistical mode is allowed for verdicts, or only with the Gerbicz or
  Jacobi check on.
- A published lesson: PRPLL fixed wrong residues in its 96-bit carry at low bits per word. Hence R4.
- References: R. Crandall and B. Fagin (1994) for the weighted transform; J. M. Pollard, "The fast
  Fourier transform in a finite field", *Mathematics of Computation* 25(114), 1971; I. S. Reed and
  T. K. Truong, "The use of finite fields to compute convolutions", *IEEE Trans. Inf. Theory* 21(2),
  1975; R. Miller, I. S. Reed and T. K. Truong, "A theorem for computing primitive elements in the
  field of complex integers of a characteristic Mersenne prime", *IEEE TASSP* 29(1), 1981;
  D. Harvey, "Faster arithmetic for number-theoretic transforms", *J. Symbolic Comput.* 60, 2014.
- Found by: factory adoption review, 2026-10-11.
