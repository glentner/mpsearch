# 01 — The weighted transform: digits, weights, carry, subtraction

**Question.** What exactly must the engine compute, step by step, and what does each step rely on?
Scope: the arithmetic of Crandall–Fagin's IBDWT as `doc/weighted-transforms.md` states it, the
balanced digit form and its round-trip with `BigInt`, the carry, and the Lucas–Lehmer subtraction
at any bit. Not the FFT (02) or the sizing (03).

## Sources read (clean room: papers and the repo's notes only)

| Source | Where | What it backs |
|---|---|---|
| Crandall and Fagin 1994, *Math. Comp.* 62(205) | the scan of the published pages on the second author's site (the URL `doc/references.md` records), pp. 309, 310, 315, 316 rendered and read | Algorithm W, steps (1)–(6) (p. 309); real-signal FFTs, the two-signals-at-once trick, right-angle convolution and PFA for non-powers of 2 (p. 310); eq. (6.4)–(6.9) and Mersenne step (1) (p. 315); balanced-variable digits "to cut down the overall convolution errors", the q = 37, N = 4 example, eq. (6.10)–(6.15) (p. 316) |
| `doc/weighted-transforms.md` (f956a7c) | whole note | Theorem 1 and its proof (e_ij ∈ {0, 1}, a_i a_j = a_k 2^(e_ij), z = a⁻¹((ax) * (ay))); widths; existence of balanced representations; the output bound \|z_k\| < N 2^(2β) |
| `doc/lucas-lehmer.md`, "The shift" | § The result | a shifted run stores 2^k s, subtracts 2^((2k+1) mod p), doubles k mod p |
| `doc/prp-and-gerbicz.md`, Theorem 2 | — | x_p = 3^(2^p) = 9 × the type-1 residue mod M_p |
| `doc/reference-arithmetic.md`, `src/mp.*` | — | `BigInt` (64-bit limbs, little-endian, `from_limbs` drops high zero limbs), `reduce_mersenne`, `res64`, `divide`; `mp.cpp` uses `unsigned __int128`, which R11 forbids in the engine |

No program's source was opened.

## Findings

1. **Positions and weights are exact rationals before rounding.** c_j = ⌈pj/N⌉ in integers
   ((pj + N − 1) / N); f_j = (N c_j − p j)/N with an integer numerator in [0, N). So
   a_j = exp2(num/N) and a_j⁻¹ = exp2(−num/N) are each one rounding of the fraction plus one call
   (Percival's δ). Computing a⁻¹ as 1/a adds a rounding; computing it directly is as cheap.
   For p = 37, N = 4 the widths are 10, 9, 9, 9 and the weights 1, 2^(3/4), 2^(1/2), 2^(1/4)
   (CF p. 316) — a pinned check.
2. **Theorem 1 holds for any integer digits**, balanced or not; balance only shrinks the outputs
   (CF p. 316). So an engine that skips the balance step still returns correct residues at small
   sizes and fails only by round-off at the table's sizes. A check that only compares residues
   cannot see a missing balance step near the bottom of a length's range: the digits themselves
   must be checked in their range (R1's "digits at both ends of their balanced range").
3. **The balanced range covers 2^p consecutive integers [−H, 2^p − 1 − H] for 2^p − 1 classes,** so
   exactly one class has two balanced representations (its two ends differ by M_p) and every other
   class has one. In particular the integer M_p is *not* a balanced vector's value (H > 0), so R1's
   "a digit vector that represents M_p" is a vector outside the balanced range, e.g. the plain
   digits 2^(s_j) − 1 (all ones): `from_digits` must accept any digits that fit, not only balanced
   ones, and read all ones as 0. `to_digits(M_p)` (allowed: R1's x ranges to M_p − 1, and M_p is
   accepted as the same class as 0) balances to zeros.
4. **The carry.** Round, then for each word t = z_j + carry, digit = t mod 2^(s_j) in
   [−2^(s_j−1), 2^(s_j−1)), carry = (t − digit)/2^(s_j) exactly (C++20 fixes two's complement and
   arithmetic `>>`, so `&` and `>>` on negative int64 are defined). The carry out of the top word is
   worth 2^p ≡ 1 and goes into word 0; after one pass it is small, and a ±1 ripples at most twice
   round (all-max or all-min digits), so a bounded loop with a "does not settle" error is honest.
   int64 suffices: \|v\| < 2^51 (03) and digits < 2^50 (MAX_WIDTH).
5. **The subtraction at any bit** b: 2^b = 2^(c_j) 2^(b − c_j) for the word with c_j ≤ b < c_(j+1),
   so subtract 2^(b − c_j) from the rounded z_j before the carry. That is all a shifted LL step
   needs; the engine never tracks the shift (GOAL Clarifications). b = 0, 1 and p − 1 hit the
   first word, the first word's second bit and the top word: R4's three named positions.
6. **The product** is the same pipeline with two forward transforms (Algorithm W, steps 2–4); a
   square uses one. Aliasing (x × x) is safe when the result is assigned after the outputs exist.
7. **PRP type-1 final rows** (R5): p squarings from 3 give x_p = 3^(2^p) mod M_p; the type-1 residue
   is x_p/9 mod M_p. 9 is invertible for odd p (3 ∤ M_p), so add M_p until 9 divides, then
   `divide` on `BigInt` — R5 allows the division in the reference arithmetic.

## Recommendation

Pin the pieces as separate free functions — positions, weights, inverse weights, convolve,
convolve_square, roundoff, carry, to_digits, from_digits — so each is checked against its
definition (Theorem 1's exact integers for small N; `BigInt` values of digit vectors) and each can
be wrapped by the gates. `from_digits` accepts any digits below 2^62 in size; `to_digits` refuses
more than p bits. No `__int128` in the engine (R11).
