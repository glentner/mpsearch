# Research 01 — the Jacobi symbol on `BigInt`, and its vectors (R4, R5, R18)

## 1. Algorithm

**Recommendation: the binary algorithm (Shallit and Sorenson 1993), on two local limb vectors in `src/mp.cpp`.** It uses only subtraction, right shift and comparison, which `BigInt` already has (`src/mp.hpp:63`–`69`), and needs no division.

The candidates, from the literature:

- **Classical, by reciprocity and remainder.** Crandall and Pomerance, *Prime Numbers: A Computational Perspective*, 2nd ed., 2005, § 2.3.1, **Algorithm 2.3.5 (Calculation of Legendre/Jacobi symbol)**. The loop: reduce a mod m; while a ≠ 0, halve a while it is even, negating t when m ≡ 3 or 5 (mod 8); swap a and m, negating t when both are ≡ 3 (mod 4); then a ← a mod m. Return t if m = 1, else 0. *Locator to verify:* I could not open the book. The algorithm number comes from memory. A retailer's table of contents puts § 2.3.1 "Quadratic residues" on pp. 96–98 (§ 2.3.2 starts on p. 99), so the algorithm sits on p. 97 or 98. The builder must confirm the number and the page in the book before citing them. `CrandallPomerance2005` is already in `doc/references.md:77`–`84`, but its Verified line covers only the front matter.
- **Binary (Stein-style).** J. Shallit and J. Sorenson, "A binary algorithm for the Jacobi symbol", *ACM SIGSAM Bulletin* 27(1), 4–11, January 1993, DOI 10.1145/152379.152384. I checked the DOI's Crossref metadata (this title and authors, the volume, issue and pages, 1993-01). The full text was unreachable from here: the ACM and author copies sit behind a bot wall. Brent and Zimmermann 2010 (arXiv:1004.2091v2, which `doc/references.md:15`–`21` records as read) describes it on p. 8 (Remark 3 after Theorem 2, and footnote 5). It calls the algorithm "Algorithm Binary Jacobi", built on Stein's binary gcd, needing the sign of a − n to decide an interchange. They also report that the quadratic binary algorithm is faster in practice than their own quadratic and cubic 2-adic ones.
- **2-adic (right-to-left).** Brent and Zimmermann 2010, § 1.1: Algorithm 1.1 BinaryDividePos and Algorithm 1.2 CubicBinaryJacobi, which I read in arXiv v2. It is slower in practice by the authors' own Remark 3, and its decisions depend only on low bits. **Recommended as the second Python oracle** (§ 4), because it shares no step with either of the other two.

The binary algorithm in the form to implement. For odd n > 0 and a ≥ 0: a ← a mod n (one `reduce`, a copy when a < n), t ← 1. While a ≠ 0:
1. Strip the z trailing zero bits of a, negating t when z is odd and n mod 8 ∈ {3, 5}.
2. If a < n, swap them, negating t when both are ≡ 3 (mod 4).
3. a ← a − n, which is even.

At the end, return t if n = 1, else 0. Every step keeps t·J(a | n): J(2 | n) = (−1)^((n²−1)/8), reciprocity, and J(a − n | n) = J(a | n). These are the identities `doc/lucas-lehmer.md:28` already cites from BrentZimmermann2010 § 1. It ends with n = gcd(a₀, n₀).

*Termination bound (the project's own, one line):* after step 2, a ≥ n, so (a − n)/2 < a/2. Each pass therefore lowers bitlen(a) + bitlen(n) by at least 1, and the passes number at most bitlen(a mod n) + bitlen(n) ≤ 2p.

**In place or on `BigInt`?** In place. Copy `reduce(a, n).limbs()` and `n.limbs()` into two local `std::vector<std::uint64_t>` of equal capacity. Then work on raw pointers with live lengths, swapping the pointers, which is an O(1) swap. A subtraction pass and a shift pass run over the live limbs, trimming the length as the top limbs zero. That is one allocation per call, against three allocations per pass for `subtract` (it also compares first, `src/mp.cpp:134`), `shift_right` (`src/mp.cpp:177`) and `from_limbs`. About 45 lines: one public free function, `static` helpers, house style.

**References entry needed: yes, `ShallitSorenson1993`.** It can be verified against the DOI's metadata as above. The algorithm-level description should be verified against BrentZimmermann2010's arXiv v2, p. 8, Remark 3 and footnote 5, unless the builder can read the paper itself. If only the metadata can be checked, cite the paper without a locator, as `doc/references.md` does for Barrett1987. The proof in the note then stands on its own (§ 5).

## 2. Cost

**Measured** on an M2 Pro Mac, Apple clang 21, `-O3 -std=c++20 -fno-fast-math -ffp-contract=off`, linked against the repo's own `src/mp.cpp`. Operands were uniform random residues mod M_p. All three implementations agreed on every case, and they matched a Python reciprocity function and Euler's criterion on samples at 89, 1237 and 1279.

| one symbol J(a \| M_p) | p = 10,303 | p = 100,069 |
|---|---|---|
| binary, in place (recommended) | 0.78 ms | 65 ms |
| binary, in place, subtract and shift fused in one pass | 0.67 ms | 55 ms |
| binary on `BigInt` ops (`subtract`/`shift_right`/`compare`) | 2.0 ms | 111 ms |
| classical on `BigInt` ops (`reduce` each step) | 2.5 ms | 119 ms |

Quadratic: ×9.7 in bits gives ×84 in time (≈ 6.5·10⁻¹² s·p²); in place, 0.022 ms at 1279, 0.16 ms at 4423, 0.73 ms at 9941. The checks build `src/` with `BOUNDS` (`Makefile:13`, `:57`), libc++ hardening. On raw pointers the in-place times were the same with it on (0.77 / 64 ms). Fusing gains 15%: not worth the extra branch.

**Schedule.** Assumption, as given: a squaring costs about 30 µs at length ~512 (p = 10,303) and about 400 µs at length ~4096 (p = 100,069). A run is then about 10,303 × 30 µs ≈ 0.31 s and about 100,069 × 0.4 ms ≈ 40 s. For 10% overhead, checks ≈ 0.1 × p × t_sq / t_J:

| | 10,303 | 100,069 |
|---|---|---|
| in place | ≈ 40 | ≈ 61 |
| `BigInt` binary | ≈ 15 | ≈ 36 |
| `BigInt` classical | ≈ 12 | ≈ 34 |

**Recommend a fixed 32 checks per run.** That is a check every ⌈(p − 2)/32⌉ iterations (at least 1), plus the final residue, 33 symbols. In place this costs about 8% at 10,303 and about 5% at 100,069. The per-check fraction is c·p/t_sq(p), and t_sq shrinks less than linearly at small lengths, so smaller p sit near or below that. On the classical `BigInt` route the same count costs about 25% at 10,303, which is why in place matters at small p.

Make the schedule iteration-based, never time-based. The Jacobi count is a reported field (R12), and a timed schedule would make it vary between runs (§ Invariants 2, "Timing never changes a result").

`make check` load: about 33 × Σ t_J over R1 and R2's exponents. The sum is dominated by 9689, 9941 and 10303, about 2.7 ms in all, so about 0.09 s per sweep, and about 0.2 s with R3's second shift. That is well inside R16's 5 s. Excluded: converting the engine's digits to `BigInt` and forming r − 2^(k+1) (the driver's costs).

## 3. Signature and domain

```cpp
int jacobi(const BigInt& a, const BigInt& n);   // in src/mp.hpp after power_mod
```

- Returns −1, 0 or 1. `a` is any non-negative value, including a ≥ n, since J depends on a mod n. `n` is odd and positive. J(a | 1) = 1 for every a, J(0 | 1) included, and the algorithm gives this by itself (the loop never runs, and n = 1).
- `n` = 0 throws `mp_error("modulus zero")`, the existing text (`src/mp.cpp:431`). An even `n` throws `mp_error("even modulus")`. In both cases nothing is returned. Extend the domain list in `mp_error`'s comment (`src/mp.hpp:24`–`25`).
- Negative arguments are the caller's: −x is M − x. The driver forms J(r − 2^((k+1) mod p) | M_p) as `subtract(add(r, M), pow2)` when r < pow2. That is the driver brief's concern; note it in PLAN.
- Checks in `tests/check_mp.cpp`: one "Jacobi symbols match the vectors" entry in `check_by_name` (`:505`), following `modular_powers_match_the_vectors` (`:478`). Add to `errors_are_reported_never_values` (`:265`): `jacobi(5, 0)`, `jacobi(0, 0)`, `jacobi(5, 2)`, and a multi-limb even modulus (`u:1-300`).

## 4. Vectors: a `jacobi` family

**Format.** Stem `jacobi`, giving `tests/vectors/jacobi.txt` with the standard envelope. The generator gains `"jacobi": jacobi` in `FAMILIES` (`tests/vectors/generate.py:490`). The reader gains `"jacobi"` in `STEMS` (`tests/vector_reader.cpp:58`) and `{"jacobi", "OOJ"}` in `fields_by_op` (`:35`). A new field kind `J` matches exactly `-1`, `0` or `1`, added to `is_field` (`:302`) and to the kinds comment (`:33`). One op serves Mersenne and general moduli:

```
jacobi <a> <n> <symbol>          e.g.  jacobi x:3 u:0-1237 -1
```

M_p is written `u:0-p`, the form `hex`/`decimal` already use. `x:` operands have no length cap (`is_operand`, `:242`), so full residues of up to 10,303 bits are legal, as `reference.txt` already shows. Leave `jacobi` out of `OPERAND_FIELDS`/`every_operand()` (`generate.py:411`–`424`), so `hex.txt` and `decimal.txt` keep every line.

**Mersenne cases: 15 per exponent, for 24 exponents.** The exponents are the 21 odd ones up to 10,000 in `tests/known/mersenne-exponents.txt` (3 … 9941), plus 1237, 1277 and 10303. With k = (p − 1)/2:

| # | a | symbol | why |
|---|---|---|---|
| 1–4 | `x:0`, `u:0-p` (M), `u:1-(p+1)` (2M), `u:p-2p` (2^p·M) | 0 | a ≡ 0, shares M; a ≥ n |
| 5 | `x:1` | 1 | |
| 6 | `u:p-(p+1)` (2^p ≡ 1) | 1 | a > M |
| 7 | `u:k-(k+1)` (2^k) | 1 | J(2 \| M) = 1 |
| 8 | `u:1-p` (M − 1 ≡ −1) | −1 | M ≡ 3 (mod 4) |
| 9 | `x:3` | −1 | J(3 \| M) = −1 |
| 10 | `x:c` (12 = s₁ − 2) | −1 | the invariant at i = 1 |
| 11–12 | `r:p:S₊`, `r:p:S₋` | +1, −1 | dense random, literal seeds |
| 13 | `x:` s_i − 2 mod M, i = max(1, (p−1)/2) | −1 | a real LL residue |
| 14 | `x:` 2^k s_i − 2^(k+1) mod M, same i | −1 | the shifted quantity the driver forms |
| 15 | s_(p−2) − 2 mod M: `u:0-1,2-p` (M − 2) for prime M_p, `x:` for composite | −1 | the final check before a verdict |

Rows 7–10 guarantee both signs by theorem (`doc/lucas-lehmer.md`, "The symbols of 2, 3 and 12"), with no search needed. Rows 11–12 add dense operands, with seeds found once by searching from 1000p up and written as literals in a table, as `TWO_CORRECTIONS` does (`generate.py:339`–`357`). The generator asserts the sign of each. The seeds I found, as (S₊, S₋):

3: (3001, 3000), 5: (5001, 5000), 7: (7001, 7000), 13: (13000, 13003), 17: (17000, 17004), 19: (19002, 19000), 31: (31000, 31002), 61: (61000, 61001), 89: (89000, 89003), 107: (107000, 107005), 127: (127001, 127000), 521: (521000, 521004), 607: (607000, 607001), 1279: (1279000, 1279002), 2203: (2203002, 2203000), 2281: (2281000, 2281001), 3217: (3217003, 3217000), 4253: (4253001, 4253000), 4423: (4423004, 4423000), 9689: (9689001, 9689000), 9941: (9941000, 9941001), 1237: (1237000, 1237004), 1277: (1277003, 1277000), 10303: (10303000, 10303001).

Four consecutive seeds are not enough: at 4423, seeds 4423000–4423003 all give −1. Rows 13–14 came out −1 for all 24 exponents, composites included, as the invariant predicts.

**General odd moduli, about 110 records**, for coverage the Mersenne moduli cannot give: a proper common factor, square factors, n = 1, and a > n with multi-limb n.

- n = 1 with a ∈ {`x:0`, `x:1`, `r:200:s`}: all give 1.
- Every a in [0, n) for n ∈ {9, 15, 45}. These include J(2 | 9) = 1 although 2 is not a square mod 9, and the zeros of a square factor.
- Composite Mersenne moduli with small factors, taken from the `reference` family's `COMPOSITE` list: M₁₁ = 23·89, M₂₃ = 47·178481, M₂₉ = 233·1103·2089, M₃₇ = 223·616318177. Use a = q (0), a = q·(random) as `x:` (0), and one random a of each sign. The generator finds the factors by searching q = 2kp + 1 and asserts q | M_p.
- The real zero of the LL invariant (`doc/lucas-lehmer.md:98`, `jacobi_first_zero(25)`): s_i − 2 mod M_p for (p, i) = (9, 3), (15, 3), (21, 3), (25, 5) gives 0, and at i − 1 gives −1.
- Multi-limb odd n: n = `r:640:s` with s chosen so n is odd (asserted), a = `r:3000:s′`; n = q² for an odd 300-bit q, with a = q·r (0) and a random a.

**Symbol 0 on M₁₂₃₇, M₁₂₇₇, M₁₀₃₀₃.** I searched q = 2kp + 1 with q ≡ ±1 (mod 8) for 2^p ≡ 1 (mod q), for k ≤ 1.5·10⁷. That means q below about 3.7·10¹⁰ (1237), 3.8·10¹⁰ (1277) and 3.1·10¹¹ (10303), about 8–10 s each. **There is no factor in that range for any of the three.** Their symbol-0 cases are therefore rows 1–4 (a ≡ 0 mod M_p, which shares the factor M_p itself). A proper common factor is covered by M₁₁ … M₃₇ above. Don't take a factor from a published table: that would be a cited number, and the general moduli already cover the path.

**The independent computation.** Every record runs the classical reciprocity algorithm in Python (Crandall–Pomerance Algorithm 2.3.5's loop, the same as `spec/math-notes-and-references/verify/claims.py:138`). That is a different algorithm from the C++ binary one. It must also agree with:
- **Euler's criterion for a prime M_p:** a^(2^(p−1)) ≡ J·a (mod M_p), from (M − 1)/2 + 1 = 2^(p−1). This takes p − 1 folded squarings, as `lucas_lehmer` folds (`generate.py:453`): 0.47 s at 9941, against 3.1 s for `pow(a, (M−1)//2, M)`. For a ≡ 0 the symbol must be 0.
- **Brent–Zimmermann CubicBinaryJacobi for a composite or general n.** Reduce to an even positive b by (b | a) = (b + a | a) for odd b. I tested it against the classical loop on 3000 random pairs with no mismatch.
- **The value by construction**, where a row's "why" gives one.

The sources line names all three. A prototype (6 rows per exponent) ran in 15.5 s: 96 records, 26 KB. **Estimated whole family:** about 470 records, about 45 KB (in line with `arith.txt` at 56 KB), and about 30 s to generate. C++ check time is under 0.3 s even on the slow route.

## 5. What `doc/reference-arithmetic.md` gains

It is the note of `src/mp.*`, so the symbol's write-up goes here, and `doc/lucas-lehmer.md` cites it (R18). Its structure is The result / Proof (one `###` per algorithm) / What mps uses / Sources / Provenance (`:3`, `:17`, `:71`, `:79`, `:92`).

- **The result** (after "Division, reduction and powers", `:13`). Add a paragraph **"The Jacobi symbol."** For odd n > 0 and a ≥ 0, `jacobi` gives J(a | n), defined as in the Lucas–Lehmer note's Notation (point to `doc/lucas-lehmer.md:28`, don't restate it). It is 0 exactly when gcd(a, n) > 1, J(a | 1) = 1, and the binary algorithm needs no division. These are definitions and theorem values, so no markers.
- **Proof, `### The Jacobi symbol`.** State the invariant t·J(a | n) and the three identities, citing BrentZimmermann2010 § 1. Show that the loop ends with n = gcd, and give the termination bound of at most bitlen(a mod n) + bitlen(n) passes, each linear in limbs, so quadratic. Add one worked example in a check marker, J(12 | M₇) = −1, which is the invariant at s₁ for p = 7. The trace: 12 → 3; swap to (127, 3) with t = −1; 124 → 31; 28 → 7; 4 → 1; swap to (3, 1); 2 → 1; 0; n = 1, so −1. Marker: `<!-- check: jacobi(12, mersenne(7)) == -1 -->`, using `claims.py`'s `jacobi` and `mersenne`. Optionally add `<!-- check: jacobi(2, 9) == 1 and all(x * x % 9 != 2 for x in range(9)) -->` to show that +1 does not mean a square for composite n.
- **Cost.** State the theorem-level bound only. Measured times are about instances but cannot be checked by `claims.py` or cited. Follow the `KARATSUBA_THRESHOLD` precedent (`:49`, `src/mp.hpp:33`–`37`): numbers measured on this code go in a code comment or PLAN, not the note. The check schedule is described in `doc/lucas-lehmer.md` "What mps uses" (R18), as a rule rather than a timing.
- **What mps uses** (`:73`). Add a sentence on `jacobi`: binary, on two local limb vectors, `mp_error` for an even or zero modulus. Fix `:75`'s "on the published Mersenne prime exponents" to what `tests/check_reference.cpp:6`–`9`, `:56` run since `known-answer-tables`: the odd exponents from 3 to 10,000 in `tests/known/mersenne-exponents.txt`, read through `src/known.*`.
- **Sources:** ShallitSorenson1993 (the algorithm); BrentZimmermann2010 § 1 (the identities); CrandallPomerance2005 § 2.3.1, Algorithm 2.3.5 (the classical algorithm, the vectors' oracle); BrentZimmermann2010 § 1.1, Algorithms 1.1 and 1.2 (the second oracle).
- **Provenance:** the binary algorithm is Shallit and Sorenson's. The proof is written out from the identities, and the termination bound and the in-place limb form are the project's own.
- **Same-commit obligations** ("Point to a rule"): add the Jacobi symbol to `doc/README.md:8`'s index line, and replace `doc/lucas-lehmer.md:114`'s "0 as an error unless the GCD gives a factor" (GOAL Q on symbol 0). The header comment of `src/mp.cpp:6`–`8` says "from Brent and Zimmermann, Modern Computer Arithmetic", so extend it to name Shallit and Sorenson.

## Risks

- **Citations not fully verified here.** The Crandall–Pomerance algorithm number and page come from memory: verify both in the book, or cite § 2.3.1 alone. Shallit–Sorenson's text was unreachable, so its algorithm-level locator must be verified or dropped.
- **A passing check doesn't mean a good state.** A corrupted state passes a Jacobi check with probability about ½ and then becomes a "good state". The interval sets the detection latency, not the protection. This is already stated in `doc/lucas-lehmer.md`; the plan should not claim more.
- **`jacobi` is on the verdict path in `src/mp.*`** (high blast radius). Its vectors and the error cases must land in the same commit.
