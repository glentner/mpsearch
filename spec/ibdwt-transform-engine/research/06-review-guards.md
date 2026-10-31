# 06 — After the review: the aliasing monitor, the guards, the tail

**Question.** The adversarial design review (foreman, 2026-10-22) showed the round-off distance
silently accepts wrong squarings of structured inputs, a protocol the gates did not enforce, and a
tail at large lengths. What guard makes the engine refuse those inputs without refusing ordinary
LL/PRP residues, what sizing fixes the tail, and what is the evidence? Inputs: the review's tools
and logs (its alias, past, peak, dist and below tools, rebuilt against the revised prototype), the
planner's own studies in `<git dir>/plan-proto/guard/` and `regress/`. Mathematics only from
Theorem 1 of `doc/weighted-transforms.md` and Parseval's identity; no program's source.

## 1. Why the distance aliases

The distance sees |v − round(v)| on each output. A true error e in (1/2, 1) reads as 1 − e. For
pseudorandom inputs the errors across the N outputs are spread, so before any output passes one
half many others sit between 7/16 and 1/2 and the threshold fires. For structured inputs (constant
digits, alternating, short periods, tones) the outputs, and their errors, are nearly alike: they
pass one half together and all read small. Above about 2^48 a double's spacing is 1/16 or coarser,
so a shared error of 0.625–1.25 reads as 0.25–0.4375. The review counted 99 silent wrong squarings
of constant and alternating digits c·(half − 1) at table maxima up to 3,328 words; the planner's
sweep of 9 families × 20 fractions at every length up to 4,096 found 56 at the old maxima.

## 2. Candidate guards, measured

For every structured input that squared wrong and passed the distance, the ratio of a metric to
its largest value over pseudorandom digits at the same (N, p) (`guard/rms.cpp`, `guard/sums.cpp`):

| Guard | Smallest ratio over wrong-accepted inputs | Notes |
|---|---|---|
| outputs' rms (energy) | 2^0.84 (period 4, N = 52) | by Parseval, the product spectrum's size over N; sets the error scale |
| spectral peak max\|X_k\|² (the review's suggestion) | 2^1.03 | slightly better at the minimum, same family |
| exact checksums, characters of order 1, 2, 3, 4 | exact | missed only period-8 sawtooths (an error periodic in 8 is orthogonal to those characters) |
| checksums, then rms ≤ 2^(1/2) × envelope | **0 left** | every input the checksums missed had rms ≥ 2^2.02 |

The checksums (§ 3) are exact: a correct squaring always agrees, so they never refuse a right
answer, and they see every error pattern with a component at those characters. The envelope covers
the energetic remainder. Together on the planner's sweep: no wrong squaring accepted.

## 3. The checksum identity

Theorem 1: z_k = Σ_{i+j≡k (mod N)} x_i y_j 2^(e_ij), e_ij = f_i + f_j − f_k ∈ {0, 1}. Since
0 ≤ f_k < 1, e_ij = 1 exactly when f_i + f_j ≥ 1, i.e. n_i + n_j ≥ N for n_j = N c_j − p j (an
integer in [0, N)). For u a root of unity of order d | N, u^((i+j) mod N) = u^i u^j, so

    Σ_k u^k z_k = (Σ_i u^i x_i)(Σ_j u^j y_j) + Σ_i u^i x_i · S(N − n_i),   S(t) = Σ_{j : n_j ≥ t} u^j y_j.

S is a suffix sum over buckets of n_j: O(N). Everything is taken in Z[u] modulo 2^64: d = 1 and 2
in the integers, d = 4 in Z[i], d = 3 in Z[ω] with ω² = −1 − ω. Unsigned wraparound is defined in
C++, and int64 digits convert exactly.

## 4. Evidence on the revised prototype

- **The review's sweep** (its alias tool, constant and alternating, 40 fractions, every length up to
  4,096 at the new maxima): 0 wrong accepted, 4,681 refused, 1,719 squared right. The same sweep
  with the checksums and the envelope switched off: 47 wrong accepted. So the margin alone does not
  fix the aliasing; the guards do.
- **Real chains, not refused.** LL from 4 and PRP from 3 at every length's largest exponent: up to
  2^24 / N squarings each up to 65,536 words, 128 above; 899,830 squarings in all, no refusal of
  any kind, 2 distances past 0.35 (largest 0.375).
- **The tail** (the review's dist tool at the new maxima): 2^20 and 7·2^17, 500 fresh and 500 LL
  squarings each, and 65,536 with 10,000 each: none past 0.35, none past 7/16 (was 1.6 % and 4.4 %
  past 0.35). Off-grid exponents below a maximum (its below tool): none past 0.35.
- **Default tier:** the structured check refused 590 and squared 1,714 at the sample lengths.
- **Rerun on the final prototype (2026-10-31):** the alias sweep and the chains rebuilt against the
  committed engine give the same counts: 0 wrong accepted, 4,681 refused, 1,719 right; 288 chains,
  899,830 squarings, no refusal, 2 past 0.35 (largest 0.375).

## 5. The sizing change

64 squarings a probe (was 16) at least, still 2^22 outputs; the table takes the probe 2 grid points
(1/8 bit a word) below the last that held. P_max 20,316,160 (was 20,512,768); every length still
assigned exponents; P_max stays twice R3's floor. Cost 2,530 s CPU (16 minutes wall on three
workers), mostly the large lengths.

## 6. Recommendation

As the PLAN now says: monitor order magnitude → distance → checksums → envelope, each a
`roundoff_error`; the envelope recorded per length in the table from the probe at its maximum,
`RMS_MARGIN` = 2^(1/2); the pieces internal; the protocol constants pinned and every probe's counts
in its data line; the gate's own measurement at the maxima.
