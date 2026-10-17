# Research 01: algorithms and their sources

Scope: one algorithm per operation, chosen to be clear first and fast enough, each with a published source checked this session. Clean room held: only MCA (arXiv:1004.4710, PDF fetched and read), publisher and catalogue pages, the GIMPS math page, and this repo's notes were read. No program source was opened. Prototypes (Python and C++, on arm64 with `-O2`) are in the session scratchpad and are not committed.

## Verified locators

| Claim | Locator | Status |
|---|---|---|
| Schoolbook product, Θ(mn) | MCA §1.3.1, Alg. 1.2 BasecaseMultiply, Thm 1.3.1 | read |
| Karatsuba (subtractive), recurrence, threshold of 10 to 100 words; additive variant named | MCA §1.3.2, Alg. 1.3 KaratsubaMultiply, Thm 1.3.2 | read |
| Unbalanced operands, padding variants | MCA §1.3.5 | read |
| Squaring: naive squaring halves the products; Karatsuba squaring threshold about 30 words | MCA §1.3.6 | read |
| Long division, normalization, at most 2 corrections | MCA §1.4.1, Alg. 1.6 BasecaseDivRem, Thm 1.4.1 (citing Knuth "Theorem 4.3.1.B") | read |
| q* ≤ β+1; the clamp case; correction probability | MCA Exercises 1.19, 1.20 | read |
| Division by one word | MCA §1.4.7 | read |
| Radix conversion, quadratic | MCA §1.7.1, Alg. 1.23 IntegerInput, 1.24 IntegerOutput | read |
| Radix conversion, divide and conquer | MCA §1.7.2, Alg. 1.25, 1.26 | read |
| Special moduli β^n ± 1 | MCA §2.4.4 | read |
| Left-to-right binary exponentiation (e ≥ 1, top bit 1) | MCA §2.6.1, Alg. 2.12 LeftToRightBinaryExp | read |
| Karatsuba–Ofman 1962 bibliographic data | MCA ref. [136]: "Multiplication of multi-digit numbers on automata (in Russian). Doklady Akad. Nauk SSSR, 145(2):293–294, 1962. Translation in Soviet Physics-Doklady 7 (1963), 595–596" | read in MCA only |
| Same, primary record | MathNet.ru `dan26729` | **not reached**: three timeouts. A search index shows vol. 145, no. 2, pp. 293–294, 1962, authors A. Karatsuba and Yu. Ofman, English title "Multiplication of many-digital numbers by automatic computers". This is second-hand. |
| Karatsuba–Ofman DOI | Crossref bibliographic query, Scite | **none found** |
| Knuth TAOCP vol. 2, 3rd ed. | Knuth's page (taocp.html): "Third Edition (Reading, Massachusetts: Addison-Wesley, 1997), xiv+762pp. ISBN 0-201-89684-2". InformIT: published 4 Nov 1997, © 1998, ISBN-13 978-0-201-89684-8. MCA [143] says 1998. | read |
| Knuth section titles | InformIT TOC: "Multiple Precision Arithmetic" › "The Classical Algorithms", "Modular Arithmetic", "How Fast Can We Multiply?"; "Radix Conversion"; "Evaluation of Powers" | titles read, **numbers inferred** from the order (4.3.1, 4.3.2, 4.3.3; 4.4; 4.6.3). MCA confirms "4.3.1.B" and "§4.3.3.C". Algorithm M and Algorithm D **not confirmed**. |
| Res64 = low 64 bits of the final value; "mod 2^P−1 … rotates the p-th bits and above to the least significant bits" | GIMPSMath, Section Double-Checking | read |
| Folding theorem in Crandall–Pomerance §9.2.3 (Thm 9.2.12?) | Springer pages behind login; no open TOC | **not verified**. Do not cite. |

## 1. Schoolbook multiplication and squaring

Multiplication follows MCA Alg. 1.2: for each b_j, multiply-accumulate A·b_j into C at offset j, with the carry in an `unsigned __int128`. For squaring, recommend the symmetric form, kept as three plain passes: the cross sum Σ_{i<j} a_i a_j β^{i+j}, a left shift by 1, then adding the diagonal Σ a_i² β^{2i}. MCA §1.3.6 is the source ("only about half of the products a_i b_j need to be computed"). There are two reasons. First, squaring then has its own algorithm, so R1's separate squaring check means something. Second, the reference tests are squaring-bound. Measured: 10,000 schoolbook 157×157-limb products (p ≈ 10,000) take 0.19 s. A Lucas–Lehmer run at p = 9941 is about 0.19 s, and the 21 known odd prime exponents up to 9941 sum to about 2.15× that, so LL plus PRP is about 0.8 s at full-product cost and about half that with symmetric squaring. Pitfall: the doubled cross sum needs one extra bit, so shift the whole number, not limb by limb, without a carry-out.

## 2. Karatsuba multiplication and squaring

**Identity, as the note should state it.** For k ≥ 1, write a = a1·β^k + a0 and b = b1·β^k + b0 with 0 ≤ a0, b0 < β^k. Then

ab = z2·β^{2k} + z1·β^k + z0, where z0 = a0·b0, z2 = a1·b1, and z1 = (a0 + a1)(b0 + b1) − z0 − z2.

For squaring (b = a): z0 = a0², z2 = a1², z1 = (a0 + a1)² − z0 − z2.

*Proof sketch.* Expand: (a0 + a1)(b0 + b1) = a0b0 + a0b1 + a1b0 + a1b1, so z1 = a0b1 + a1b0 exactly. Substituting gives the schoolbook expansion of (a1β^k + a0)(b1β^k + b0). This is an identity in the integers, valid for every k and every size. Since z1 = a0b1 + a1b0 ≥ 0, both subtractions stay non-negative, so the unsigned BigInt subtraction never takes its below-zero error path. That is the additive variant (MCA §1.3.2 names it; it evaluates at x = 0, +1, ∞).

**Additive vs subtractive.** MCA presents the subtractive variant and prefers it for in-place word arrays because it avoids the carry. For a value-level, non-negative BigInt written for clarity, **recommend additive**. It needs no signs: the subtractive form needs s_A·s_B and a conditional add or subtract of |a0−a1|·|b0−b1|, which is the classic pitfall. The price is the carry: a0 + a1 can have k + 1 limbs, and the recursive call simply receives a (k+1)-limb operand. (Subtractive squaring is also sign-free, since (a0−a1)² = |a0−a1|². Additive keeps one identity for both.)

**Split and unequal sizes.** Split at k = ⌈max(n_a, n_b)/2⌉ limbs, taking the high part as 0 when an operand has ≤ k limbs. This is implicit padding, and MCA §1.3.5 shows it is correct but not optimal. Fall back to schoolbook when min(n_a, n_b) < cutoff, which also absorbs very unbalanced inputs cheaply. **Termination:** recursive sizes are ≤ k + 1 = ⌈n/2⌉ + 1, which is < n only for n ≥ 4 (at n = 3 the size does not shrink). So the internal cutoff must be ≥ 4.

**Threshold.** Expose `karatsuba_multiply(a, b, cutoff)` and `karatsuba_square(a, cutoff)`, with cutoff clamped to ≥ 4. Then R1 runs Karatsuba with cutoff 4, so recursion is deep even on small operands, and runs the dispatcher with the tuned threshold. MCA puts the threshold at 10 to 100 words, about 30 for squaring. An allocating implementation will sit higher, so measure in the 32 to 64 range. At 2^20 bits (16,384 limbs) schoolbook takes 0.22 s per product (measured), and Karatsuba should be about 10× less. R1's vectors need sizes at threshold − 1, threshold and threshold + 1, odd limb counts, n_a ≠ n_b with n_b ≤ k (high part zero), and operands whose a0 + a1 carries out (all-ones limbs).

## 3. Folding modulo M_p = 2^p − 1

**Theorem.** 2^p ≡ 1 (mod M_p), so for x ≥ 0, f(x) := (x mod 2^p) + ⌊x / 2^p⌋ ≡ x (mod M_p), since x = ⌊x/2^p⌋·2^p + (x mod 2^p).

**Termination.** If x ≥ 2^p, then h = ⌊x/2^p⌋ ≥ 1 and x − f(x) = h(2^p − 1) ≥ M_p > 0. So "while x > M_p: x ← f(x)" strictly decreases a non-negative integer and stops with x ≤ M_p. The loop cannot stop at M_p < x < 2^p, because no integer lies there. The final step maps x = M_p to 0, giving [0, M_p).

**Bound.** For x < 2^{2p}: f(x) ≤ M_p + (2^p − 1) = 2M_p. For M_p < x ≤ 2M_p, h = 1 and x mod 2^p = x − 2^p ≤ M_p − 1, so the second fold lands in [0, M_p − 1]. So there are **at most 2 folds** for x < 2^{2p}, which covers M_p², (M_p − 1)² and 2^p·M_p. Python check over p ∈ {3, 5, 7, 13, 31, 61, 63, 64, 65, 127, 128, 521, 4423}, with random x < 2^{2p} and the edge values, found max folds = 2. At p = 61: 0 → 0 folds, result 0; M_p → 0 folds, then mapped to 0; 2^p → 1 fold, result 1; 2^p·M_p → 1 fold to M_p, then 0; (M_p−1)² → 2 folds, result 1. A general x of n bits takes about ⌈n/p⌉ folds.

**Non-limb boundaries.** Let q = p / 64 and r = p mod 64. The low part is limbs [0, q) plus, if r ≠ 0, limb q masked with (1<<r) − 1. The high part is a right shift by q limbs, then by r bits. Pitfall: when r = 0, `w >> 64` and `w << (64 − r)` are undefined behaviour in C++, so branch on r = 0. For p < 64, q = 0 and the whole residue sits in one limb. Test p ∈ {3, 31, 61, 63, 64, 65, 127, 128} so that r takes the values 0, 1 and 63.

## 4. General reduction (division)

**Recommend MCA Alg. 1.6 BasecaseDivRem** (Knuth's classical algorithm in substance, as MCA's citation of Theorem 4.3.1.B shows):

- Normalize by s = clz(top limb). Pitfall: s = 0 must not shift by 64.
- Use the one-word estimate q* = ⌊(a_{n+j}β + a_{n+j−1}) / b_{n−1}⌋, clamped to β − 1.
- Correct by **compare-before-subtract**: compute T = q·B·β^j, and while T > A, decrement q and set T −= B·β^j; then A −= T. Nothing ever goes negative, and Theorem 1.4.1 bounds the loop at 2.
- Handle a one-limb divisor by short division (MCA §1.4.7's setting, done with `__int128` / 64).

Skip Knuth's two-limb refinement of q̂. With it, the add-back is rare: about 2/β by Knuth's estimate, which this session could not verify, and none occurred in 3,000 random cases in simulation. Without it, every correction path runs on random data. In a simulation of 22,272 quotient digits on random operands, one correction occurred in 25% of steps and two in 0.9%.

**Cost.** Bit-serial shift-subtract on 2^21 / 2^20 bits is about 2^20 quotient bits × 16,384-limb subtractions, about 3·10^10 operations, which is too slow. BasecaseDivRem is O(n(m+1)) ≈ 16,384² ≈ 2.7·10^8, about 0.3 s like one schoolbook product. That is fine for a handful of large vectors.

**Vectors to force each path** (checked in Python against `divmod`):

- Clamp, q* ≥ β (MCA Ex. 1.19): B = 2^63·β + 1 and A = B·β − 1, that is A = `0x80000000000000000000000000000000FFFFFFFFFFFFFFFF`.
- Two corrections: take them from a seeded random search in the generator, which also asserts the count.
- Add-back, if Knuth's refined q̂ is used instead: B = (h1·β + h0)·β + (β − 1) with h1 ≥ 2^63, and A = q̂·(h1·β + h0)·β. The truncated estimate is exact but the low limb of B makes it 1 too big. This fires with s = 0, and still fires after scaling both by 2^−5 (s = 5).
- Also: s = 0 and s = 63 (top limb 1); A < B; A = B; A = B − 1; m = 0; m = 1 → 0; m = 0 → error.

## 5. Modular exponentiation

Use MCA Alg. 2.12 (left-to-right binary), reducing the base first. Alg. 2.12 assumes e ≥ 1 with a leading 1 bit, so special-case e = 0 → 1 mod m, which is 0 when m = 1. Take the m = 1 → 0 shortcut before anything else.

## 6. Decimal and hexadecimal

Parse with MCA Alg. 1.23 in chunks of 19 digits: A ← A·10^19 + chunk, a one-limb multiply-add, which is cheap. Print with Alg. 1.24 using B = 10^19: repeated one-limb division, each chunk zero-padded to 19 digits except the most significant.

**Measured cost: 1.62 s for one 2^20-bit operand** (16,614 chunks). Halving the radix to 10^9 on 32-bit halves gives the same 1.56 s, because `__int128` / `u64` is a library call on arm64. This collides with R3 ("every vector operand") and R10. The plan's options:

- (a) Keep decimal vectors at ≤ about 2^16 bits. Cost is quadratic, so about 6 ms each.
- (b) Implement Alg. 1.26 FastIntegerOutput on top of BasecaseDivRem: one split by 10^{19k}, then recursion.

Hex is linear (4 bits per digit) at any size.

**Canonical forms (recommend):**

- Decimal: digits only, no sign, no leading zeros, "0" for zero.
- Hex: uppercase (matching `hex64` and R4), no prefix, no leading zeros, "0".
- Parsing: accepts leading zeros (a Res64 string must parse) and both hex cases. It rejects empty strings, signs, whitespace, `0x`, and any other character (R5).

## 7. Res64, LL and PRP

Res64 is the low 64 bits of x mod M_p in [0, M_p), printed by `hex64` (GIMPSMath, Double-Checking: "the low order 64 bits of the final S_{P-2} value"). For p < 64 it is the residue itself.

LL, as in `doc/lucas-lehmer.md` (Theorem Lucas–Lehmer, Lehmer1930 Thm 5.4): s_0 = 4, then p − 2 times "square, subtract 2, reduce mod M_p", for odd p ≥ 3. Pitfall: when the folded s² mod M_p is 0 or 1, add M_p before subtracting 2, or the unsigned subtraction errors. The p = 3 step 4² = 16 → 2 → 0 is the boundary case.

PRP, as in `doc/prp-and-gerbicz.md` Theorem 2: x_0 = 3, x_{k+1} = x_k² mod N, and 3^{N−1} ≡ 1 ⇔ x_p ≡ 9. Its proof gives x_p ≡ 9·3^{N−1}. **Recommend type-1, R = 3^{M_p−1}.** M_p − 1 = 2^p − 2 is binary 1…10 (p − 1 ones, then a 0). So Alg. 2.12 starting from x = 3 does p − 1 squarings, each of the first p − 2 followed by ×3 (a one-limb multiply and a fold). Python confirmed R = pow(3, M_p − 1, M_p) and x_p = 9R mod M_p for 15 exponents from 3 to 521, with R = 1 for every prime M_p among them.

**Decisive pitfall for x_p:** at p = 3, x_p = 3^8 mod 7 = **2**, not 9, because 9 is not reduced mod 7. A literal "== 9" test fails on a prime. Type-1's prime value, 1, has no such edge.

## 8. New entries for `doc/references.md`

Fields as in the existing entries: Authors, Title, Venue, Year, DOI or URL, Verified.

- **Knuth1997**
  - Authors: D. E. Knuth
  - Title: The Art of Computer Programming, Volume 2: Seminumerical Algorithms
  - Venue: Addison-Wesley, Reading, Massachusetts, third edition
  - Year: 1997 (© 1998; MCA writes 1998)
  - URL: Knuth's taocp page
  - Verified: as in the table. Cite §4.3.1 only through MCA's "Theorem 4.3.1.B", or as "the section The Classical Algorithms". Do not cite "Algorithm D" by letter unless someone confirms it in a copy.
- **KaratsubaOfman1962**
  - Authors: A. Karatsuba and Yu. Ofman
  - Title: Multiplication of multidigit numbers on automata (Russian)
  - Venue: Doklady Akademii Nauk SSSR 145(2), 293–294; English translation in Soviet Physics Doklady 7 (1963), 595–596
  - DOI: none found
  - URL: MathNet.ru dan26729
  - Verified: today it can say only "against MCA ref. [136]". The references.md rule ("checked against the work itself") is **not met** until MathNet.ru is reached.
- **FIPS 180-4** (Secure Hash Standard), only if vector digests use SHA-256. Not verified here.

## Recommendations

1. Schoolbook multiply per MCA Alg. 1.2. Symmetric schoolbook squaring (MCA §1.3.6) as three plain passes.
2. Additive Karatsuba for both product and square, split at ⌈max/2⌉, implicit zero high part, cutoff clamped to ≥ 4. Tests run with cutoff 4. Tune the dispatcher in the 32 to 64 range.
3. Fold with "while x > M_p: fold", then M_p → 0. The doc note proves termination and the ≤ 2-fold bound itself (project's own derivation) and cites GIMPSMath and MCA §2.4.4 as context only.
4. Branch on p mod 64 = 0 in fold and shifts (no shift by 64). Test p ∈ {3, 31, 61, 63, 64, 65, 127, 128}.
5. Division: MCA Alg. 1.6 with normalization, compare-before-subtract correction, short division for one-limb divisors. Use the crafted clamp vector and seeded two-correction vectors.
6. Modular exponentiation: MCA Alg. 2.12 with e = 0 and m = 1 special-cased.
7. Conversion: MCA Alg. 1.23 and 1.24 with radix 10^19. Bound decimal vectors near 2^16 bits, or add Alg. 1.26, since one 2^20-bit to-decimal costs 1.6 s.
8. Canonical: decimal minimal; hex uppercase, minimal, no prefix. Parsing tolerant of leading zeros and hex case, strict otherwise.
9. PRP returns type-1 3^{M_p−1} (prime ⇒ 1) via p − 1 squarings with ×3. This avoids the p = 3 "x_p = 2, not 9" trap.
10. Add Knuth1997 and KaratsubaOfman1962. Retry MathNet.ru before the Karatsuba entry claims verification.
