# 02 — The transform: lengths m·2^k, accuracy, determinism, cost

**Question.** Which FFT, for N = m·2^k ≤ 2^20 with odd m ≤ 15 (R2), in portable standard C++20
(R11), accurate enough that the length table is decent, deterministic (R9), and fast enough for
R12's budgets? Scope: algorithm choice, twiddles, complex vs real-signal, measured cost. Not the
sizing rule (03).

## Sources read

| Source | Where | What it backs |
|---|---|---|
| Cooley and Tukey 1965, *Math. Comp.* 19(90) | abstract and opening paragraph, via scite's record of the DOI (AMS refused: HTTP 403) | N = r_1 r_2 index decomposition; operation count; in-place for N = 2^m. Cited without a locator, like other 403'd works in `doc/references.md` |
| Crandall and Fagin 1994 | p. 310 (rendered scan) | real-signal FFT recommended for power-of-2 N; "two real signals at once" via x + iy; right-angle convolution; PFA for products of small primes |
| Percival 2003 | `doc/weighted-transforms.md`'s located statements (Theorem 5.1, 6.1; p. 391–392) | error grows with the error β′ of each computed root; correctly rounded roots give β′ = ε/√2: compute each root on its own, never by recurrence |
| Sorensen et al. 1987 | scite metadata and abstract only (closed) | real-valued split-radix FFTs exist and save about half; **not used**, so the Clarification's "read it first if the plan follows it" does not trigger |

## Findings

1. **Complex transform of the real weighted signal, full length N.** The simplest correct choice:
   one code path for every N (odd N ≤ 15 included, which a half-length real packing can't take),
   Percival's model (a complex FFT) applies directly, and the imaginary parts are simply zero on
   input. Cost is about twice a real-signal transform. Measured on the prototype (clang, -O3, IEEE
   flags, M2 Mac, scalar): one squaring at N = 2^20 ≈ 0.13 s; at 15,360 ≈ 1.0 ms; at 3,072 ≈ 0.1 ms.
   The heavy tier fits R12 at this speed (04). Real-signal and right-angle tricks are speed work for
   `issues/cpu-transform-throughput.md`.
2. **Mixed radix by decimation in time** (Cooley–Tukey): n = r·m; r sub-transforms of the samples r
   apart, then n/r length-r transforms of twiddled values w_n^(qk) y_q[k] written back to the same r
   places. Radices: the odd factor once (as a length-m pass by the definition, O(m²) per group, m ≤
   15), then 4s, then one 2 if k is odd. ω_4 = −i forward and +i inverse are exact. Recursion depth
   ≤ 12; out-of-place input copy plus in-place combine.
3. **Roots of unity, each computed alone.** 2πk/N reduced exactly in integers to octant o and
   remainder r (8k = oN + r), then φ = (π/4)(r/N) or (π/4)((N − r)/N) in odd octants, so `cos` and
   `sin` see only [0, π/4]; the octant's symmetry places the value. Axes and diagonals come out
   exact or correctly placed. One table of N roots per plan (16 MB at 2^20).
4. **Explicit real arithmetic, not `std::complex`.** `operator*` of `std::complex<double>` under
   -fno-fast-math may take the Annex G NaN-recovery path, and libc++ and libstdc++ differ in how
   they write it. A two-double struct with (ac − bd, ad + bc) fixes the operation order on both
   compilers; with -ffp-contract=off nothing fuses.
5. **Determinism.** IEEE flags (`Makefile:8`) are already on every object. Same build → same bits
   (verified: repeated squarings and the measurement's probes reproduce exactly). Across clang and
   GCC on this Mac both call the same libm, so even distances agree in practice; R9 only needs the
   residues to agree, which holds whenever rounding is right. The engine tier prints residue
   digests so the gates compare them.
6. **Accuracy at work.** At each length's table maximum the outputs reach about 2^49.5 and the
   distance crosses 0.35 (03): the transform's error is near the double's resolution at those
   magnitudes, not dominated by twiddle error. When p is a multiple of N all weights are 1 and the
   distance dips (visible in the data as low points at integer bits per word).

## Recommendation

`src/fft.{hpp,cpp}`: `fft::Plan(n)` with `forward`/`inverse` (unnormalized), `fft::root(k, n)`,
`fft::is_supported(n)`, a two-double `fft::Complex`. Cite Cooley–Tukey 1965 (new entry
`CooleyTukey1965`) and Percival 2003 in the comments. No real-signal transform this cycle.
