# Research 03 — time and portability

Measured 2026-10-12 on the Apple M4 Max (Apple clang 21.0.0, Homebrew GCC 16.2.0, GNU make 3.81),
in a scratch clone of `f0a5158`. `src/`, `tests/` and `Makefile` are identical to `c9b82ff`
(`git diff --stat c9b82ff..HEAD -- src tests Makefile` is empty), so this is the R10 baseline.
Prototypes are clean-room scratch code (schoolbook, Karatsuba, folding, Knuth's Algorithm D from
TAOCP vol. 2 § 4.3.1), built with the check flags `-O3 -Wall -Wextra -std=c++20 -fno-fast-math
-ffp-contract=off` plus both bounds macros. They were checked against Python's integers: LL and type-1 PRP
residues for p = 3, 11, 23, 29, 61, 67, 127, 131, 521, 1009, 2203, 3001, 4001 (every compiler,
threshold and indexing variant), decimal output at 300 limbs, Algorithm D by q·b + r = u and r < b.
Every prime-list exponent to 10,000 ended in LL 0 and PRP 1.

## 1. Baseline (R10)

Warm `make check` (nothing to rebuild), 11 runs each:

| compiler | median | min–max |
|---|---|---|
| clang++ (Makefile default) | **0.88 s** | 0.84–0.96 |
| g++-16 | **3.83 s** | 3.74–3.90 |

Almost all of it is `tests/cli.sh`: `bin/check` runs in 0.01 s and the make no-op takes 0.00 s.
cli.sh starts `bin/mps` 368 times. Under g++-16 each start costs about 10 ms instead of 2.7 ms
(200 × `--help`: 2.0 s vs 0.54 s), because the binary loads Homebrew's `libstdc++.6.dylib`. This is
dynamic-loader cost, not arithmetic. The GOAL's 0.85 s matches clang.

**Budget proposal.** Measure R10 on the Mac with the Makefile default (clang++): **≤ 3.9 s**, which
is the 0.88 s baseline plus 3.0 s. Record g++-16 too, with the same +3.0 s allowance (≤ 6.8 s),
because its baseline is dominated by process startup and an absolute figure would mislead. The new
arithmetic costs about the same on both compilers (below), so a delta compares them fairly.

## 2. Reference LL + PRP cost → B

The prototype runs LL (s₀ = 4, p − 2 squarings) and type-1 PRP (p − 1 squarings, ×3 after the
first p − 2). It squares with schoolbook or Karatsuba (allocating `std::vector` halves, written for
clarity first) and reduces by folding. Totals are LL + PRP over the prime list 3 ≤ p ≤ B:

| B | clang school | clang Kara 48 | gcc school | gcc Kara 48 |
|---|---|---|---|---|
| 5,000 (19 exps) | 0.054 s | 0.050 | 0.044 | 0.046 |
| 10,000 (21 exps) | 0.44 s | 0.35 | 0.34 | 0.31 |
| 20,000 (23 exps) | 2.32 s | 1.53 | 1.73 | 1.36 |

At 10,000, 9689 and 9941 take 0.18 s and 0.19 s of the 0.44 s. Cost grows as p³, so doubling B
costs about 5×.

**Composite set** (prime p ≤ B with M_p composite; costs are at threshold 32, clang / gcc):

| B = 10,000 | N = 8 | 16 | 24 | 32 |
|---|---|---|---|---|
| evenly spaced | 0.53 / 0.49 | 0.95 / 0.88 | 1.35 / 1.27 | 1.80 / 1.65 |
| log-spaced (11 … B) | 0.21 / 0.20 | 0.28 / 0.26 | 0.34 / 0.32 | 0.44 / 0.41 |

An evenly spaced set is mostly large p, so it costs 4–8 times as much as the prime list. A
log-spaced set covers every scale, including p < 64 for R4, for about 0.35 s. The 24-point
log-spaced set (22 distinct primes) is 11 23 29 47 59 83 113 157 211 283 383 509 691 929 1249 1669
2273 3049 4111 5527 7433 9973. Prime list plus this set at B = 10,000: **0.76 s clang / 0.72 s gcc**
at threshold 32, and **0.63 / 0.57** at 48. At B = 20,000 the same shape costs about 3.6 s, which is
over budget.

**Bounds hardening.** Both macros are active: an out-of-range `operator[]` aborts under libc++
(exit 133) and under libstdc++ (assertion, 134). Plain `vector::operator[]` against raw `.data()`
pointers in the inner loop:

- clang: no measurable difference (0.434 vs 0.423 s schoolbook LL+PRP; Karatsuba variants within
  noise).
- gcc: 16% (0.338 vs 0.283 s).
- 2^20-bit schoolbook multiply: 0.213 vs 0.207 s (clang), 0.152 vs 0.142 s (gcc).

A clarity-first `operator[]` implementation costs at most about 0.06 s of the whole suite, so
raw pointers are not worth it.

## 3. R1's big products (16,384 limbs = 2^20 bits)

| operation | clang | gcc |
|---|---|---|
| schoolbook multiply | 0.21 s | 0.15 s |
| schoolbook square | 0.10–0.11 s | 0.07 s |
| Karatsuba multiply (threshold 32 / 48) | 0.025 / 0.022 | 0.024 / 0.019 |
| Karatsuba square (32 / 48) | 0.019 / 0.014 | 0.018 / 0.013 |
| Algorithm D, 32,769 ÷ 16,384 limbs | 0.27 s | 0.27–0.28 s |
| to decimal, repeated ÷ 10^19 | **1.6–1.7 s** | **2.3 s** |
| from decimal, ×10^19 + chunk | 0.07 s | 0.07 s |

Schoolbook and Karatsuba agreed on every product and square.

**Decimal conversion is the hazard.** The single-limb division chain is serial and O(n²), with
about 1.4·10⁸ 128÷64 divisions through a library call. Neither a compile-time-constant divisor
(1.58 s / 2.25 s) nor 32-bit half-limbs with ÷10⁹ (1.42 s on both) rescues it. One 2^20-bit
decimal conversion uses about half of R10's +3 s. R3 says "every vector operand", so the plan
should either:

- (a) give R3 its own operand set with decimal vectors capped at about 2^17 bits (2,048 limbs:
  about 25 ms / 35 ms, estimated n²-scaled from the measurement), keeping 2^20-bit operands
  hex-only, which is O(n); or
- (b) convert divide-and-conquer through the general division (split by 10^(19·2^k)). This is
  estimated at 0.3–0.5 s for 2^20 bits from the Algorithm D figure, at the price of more code.

(a) is the clarity-first choice.

**General reduction.** One 2^21-by-2^20-bit division costs 0.27 s. Use at most one at full size.
Modular exponentiation should stay at small sizes; it is O(bits · n²).

**Affordable count.** Each 2^20-bit pair checked both ways costs:

- one product (schoolbook plus Karatsuba): about 0.24 s clang / 0.17 s gcc;
- one square (both ways): about 0.13 / 0.09 s.

Sizes at or below 2^18 bits cost 1/16 as much, so they are effectively free. **Two 2^20-bit
products and two 2^20-bit squares** cost about 0.75 s clang / 0.55 s gcc. With LL/PRP (0.63–0.76
s), one full-size Algorithm D (0.27 s) and capped decimal, the suite adds about 1.7–1.8 s on clang,
which leaves about 1.2 s of headroom inside +3 s. A third pair at full size is affordable
(+0.37 s) but erodes that margin.

**Karatsuba threshold.** Flat between 32 and 128 limbs at 2^20 bits, where 48 and 64 were best on
both compilers. In LL at 156 limbs it was 0.35 s at 48–128 against 0.44 s at 24–32. Below 24 it is
slower: 16 costs 0.69 s in LL and 1.5× more at 2^20 bits. Recommend **48**. R1's sizes either side
of it are then cheap: 47/48/49 limbs and 95/96/97.

## 4. `unsigned __int128` portability

- **Documented support.** GCC 14 documents `__int128` / `unsigned __int128` as an extension "for
  targets which have an integer mode wide enough to hold 128 bits" (GCC 14.2 manual, § 6.9
  "128-bit Integers", <https://gcc.gnu.org/onlinedocs/gcc-14.2.0/gcc/_005f_005fint128.html>).
  x86-64 and aarch64 both qualify. The same page notes there are no `__int128` integer literals
  where `long long` is narrower. Build values by shift and or, e.g. `(u128)hi << 64 | lo`.
- **Warnings.** It compiles warning-free under `-Wall -Wextra -std=c++20` on both compilers, and
  `constexpr` / `static_assert` on `u128` products work.
- **`-Wpedantic` (not used by the Makefile).** clang stays silent. GCC warns "ISO C++ does not
  support '__int128'" at every spelling. One `__extension__ typedef unsigned __int128 u128;` in a
  single place silences the typedef, but direct spellings elsewhere still warn. Rule: spell it
  once, behind an alias.
- **No iostream support.** `std::cout << u128` is an ambiguous overload on both compilers, and
  `printf` has no format for it. Print through the limbs.
- **Traits.** `std::is_integral_v`, `is_unsigned_v` and `numeric_limits<u128>` (digits 128) are
  specialized under both `-std=c++20` and `gnu++20` with libc++ and GCC 16's libstdc++. GCC 16's
  `<type_traits>` does this with `__STRICT_ANSI__`-specific code (line 503). GCC 14's behaviour
  under strict `-std=c++20` was not verified here, so **do not depend on traits or
  `numeric_limits` for `u128`**.
- **Builtins vs `<bit>`.** `__builtin_add_overflow` and `__builtin_clzll` work on both, but
  `__builtin_clzll(0)` is undefined. C++20's `std::countl_zero` (defined for 0, returning 64) and
  `std::bit_width` cover normalization and bit length, so recommend them. Write carries as
  `u128` sums, as the prototype does, rather than with overflow builtins. No intrinsics are needed.

## 5. Cluster nodes

These are rough single-thread estimates for 64×64→128 multiply-add loops relative to the M4 Max,
not measured (no jobs were submitted):

- Zen 4 EPYC 9654, ≤3.7 GHz: about 0.6–0.7×.
- Zen 3 EPYC 7763, ≤3.5 GHz: about 0.5–0.6×.
- GB10 Cortex-X925, about 3.9 GHz: about 0.8–0.9×.

Expect the new checks to take about 1.5–2× the Mac's seconds there. x86-64's hardware 128÷64
divide may make decimal and division relatively cheaper. All targets are within the 30-minute
limit.

R10 says "on the same machine", so measure and gate it on the Mac only. `ci/test-commit.sh` records
each step's seconds in `result.json`. Its `check` step runs `make $make_vars check` right after the
`build` step built only `bin/mps`, so it includes compiling the `lib/check/` objects and `bin/check`.
Record those seconds per target for information, never as a gate.

## Recommendations

- **B = 10,000**: 21 prime exponents, LL + PRP in 0.35–0.44 s. 20,000 would cost about 5× and
  break the budget.
- **Composite set**: about 24 log-spaced points from 11 to B (22 distinct primes, including p < 64),
  about 0.3 s. Avoid an evenly spaced set (16 points already cost 0.9 s).
- **R10 budget**: ≤ 3.9 s with clang++ on the Mac, which is the 0.88 s baseline (0.84–0.96) plus
  3.0 s. Also record g++-16: baseline 3.83 s, same +3.0 s allowance. Cluster seconds are recorded
  for information only.
- **Karatsuba threshold**: 48 limbs (flat from 32 to 128; 16 is worse).
- **2^20-bit vectors**: two products and two squares, each checked by schoolbook and Karatsuba
  (about 0.75 s clang), at most one full-size general reduction (0.27 s). Everything else at
  ≤ 2^18 bits.
- **Decimal**: cap decimal vectors at about 2^17 bits. One 2^20-bit conversion by repeated ÷10^19
  costs 1.6–2.3 s.
- **Indexing**: keep `std::vector::operator[]`. Hardening costs ≤ 16% (gcc) and about 0 (clang).
- **Portability**: one `unsigned __int128` alias, no literals, no iostream, no reliance on traits or
  `numeric_limits` for it; `<bit>` (`std::countl_zero`, `std::bit_width`) over `__builtin_clzll`.
