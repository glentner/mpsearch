# 02 — The exponent check and § 1

Scope: the trial-division exponent check, its exact re-check under `AGENTS.md` § Invariants 1, how
2^a − 1 is written, and how the check meets `-s`, `-t` and `-p`. Line numbers are against the
worktree at `491f635`. Reference numbers were computed with an independent Python script
(`spf.py`, odd trial division with exact integers; listed under "Sources" below).

## 1. Algorithm

**Recommend plain trial division: 2, then odd d while d ≤ p / d.** Do not reuse
`prime_sieve(31'623)` (`src/math.cpp:19-45`).

- Correctness argument fits in one comment. The first d > 1 that divides p is prime, because any
  prime factor of a composite d would divide p and be found first. So dividing by odd composites
  is harmless, and "first divisor found" is "smallest prime factor". If no d ≤ √p divides p, p is
  prime and the function returns p.
- The sieve version needs an allocation, a size argument that must be ≥ 31,623 and must stay
  coupled to `PVAL_MAX` (`src/cli.hpp:48`), and an index loop. It saves about 12k divisions
  (3,401 primes ≤ 31,622 against 15,811 trial divisors), which is microseconds. The simpler
  version wins.
- Bound: isqrt(999,999,999) = 31,622, with 31,622² = 999,950,884 ≤ 999,999,999 < 31,623² =
  1,000,014,129. The largest prime ≤ 31,622 is **31,607**, so 999,002,449 = 31,607² is the
  composite with the largest smallest prime factor in range. That makes it the right extreme
  case (R3).
- Overflow: write the condition as `d <= p / d`. It cannot overflow for any `long unsigned` p, so
  the function needs no precondition tied to `PVAL_MAX`. `d * d <= p` is also safe here. The loop
  exits with d ≤ 31,623, so d² ≤ 1,000,014,129 < 2³² − 1. That fits even in a 32-bit
  `unsigned long` (ILP32), and the targets are LP64 (64-bit `long`) anyway. Prefer `p / d`
  because its safety does not depend on `PVAL_MAX`.
- Cost: at most 1 + 15,810 divisions for a prime p near 10⁹. Negligible.

Shape (in `src/math.*`, coupled core and high-blast-radius):

```cpp
long unsigned smallest_prime_factor(const long unsigned p);   // p >= 2; returns p when p is prime
```

`p` must be ≥ 2. `main` validates PVAL ≥ 2 without `-p` (`src/cli.cpp:211-220`). Make the
function return 0 or throw for p < 2 rather than loop. Checks can then call it with 0 and 1 and
pin that behavior.

## 2. The re-check under § 1

§ 1 (`AGENTS.md:143`) asks that 2^p ≡ 1 (mod q) be re-checked exactly, with q = 2^a − 1. Here
that test is *equivalent* to `p % a == 0`, not just sufficient. Write r = p mod a, so
0 ≤ r < a. Then 2^p = 2^(qa + r) ≡ 2^r (mod 2^a − 1). For a ≥ 2, 2^r ≤ 2^(a−1) < 2^a − 1, so
2^r ≡ 1 holds exactly when r = 0. Composite M_p also needs 1 < 2^a − 1 < 2^p − 1, which is
1 < a < p. The comment should cite the identity x^a − 1 | x^(ka) − 1 from a textbook (e.g.
Crandall & Pomerance, *Prime Numbers*, 2nd ed., 2005, §1.3.1 on Mersenne numbers). The plan
should confirm the section number before citing it.

**Make the guarantee visible in the one function that builds a composite from the exponent
check**, independent of the search:

```cpp
// The composite result for M_p from the exponent factor a, or an error result
Result composite_by_exponent(const long unsigned p, const long unsigned a) {
	if (not (1 < a and a < p and p % a == 0))           // order matters: a > 1 before p % a
		return error_result(..., "exponent factor re-check failed");
	return composite_result(p, WORKTYPE_PTEST, {"2^" + std::to_string(a) + "-1"});
}

std::optional<Result> check_exponent(const long unsigned p) {
	const long unsigned a = smallest_prime_factor(p);
	if (a == p)
		return std::nullopt;                             // p is prime: no result, tests run
	return composite_by_exponent(p, a);
}
```

Points for the plan:

- **Order matters in the guard.** `1 < a` must be evaluated before `p % a`, so that a = 0 cannot
  divide by zero. `and` short-circuits, so write the guard as one expression in this order.
- **A prime exponent returns no result** (`std::optional`, or whatever the result-type brief
  picks). It must never return a result with outcome *prime*. Under § 1 that outcome comes only
  from a complete Lucas–Lehmer test, and a prime exponent says nothing about M_p. Returning *no
  verdict* would also be wrong: it would feed R8's "42 if none decided" logic and invite a printed
  line that R4 forbids.
- **This is the only place that writes the `2^a-1` string.** The factor string is built only
  after the guard passes, so no unchecked factor can reach the result.
- **Should the guard also prove that a is prime? Recommend no.** The composite verdict holds for
  any proper divisor a. § 1 asks for divisibility, and adding a primality test puts a second
  search inside the guard that a reviewer must also trust. That a is the *smallest prime* factor
  (R3) is a property of `smallest_prime_factor`, so pin it with checks:
  - A known-answer table in `tests/check_math.cpp`, each row citing the Python command that
    recomputes it (§ 3).
  - A differential sweep for every n in [2, 100,000). `smallest_prime_factor(n) == n` must hold
    exactly when `prime_sieve` marks n prime. Otherwise the result must equal the least prime
    index i with n % i == 0, an independent method already in the file
    (`tests/check_math.cpp:28-36`).
  - Guard checks: `composite_by_exponent` called with (33, 1), (33, 33), (33, 5), (33, 0) and
    (4, 4) yields an error result, and (33, 11), a valid but non-smallest divisor, yields a
    composite. That last case shows the guard checks § 1 and not R3.

## 3. Reference numbers (verified)

| p | smallest prime factor a | M_p | note |
|---|---|---|---|
| 2, 3, 31 | — | tests run | R4; 2 and 3 are the edge where a == p from the first step |
| 999,999,937 | — | tests run | R4; largest prime ≤ `PVAL_MAX` |
| 4 | 2 | composite, 2^2-1 | R3 |
| 33 | 3 | composite, 2^3-1 | R3 |
| 999,999,999 | 3 | composite, 2^3-1 | R3; today pinned to exit 0 at `tests/cli.sh:233-241` |
| 999,002,449 = 31,607² | 31,607 | composite, 2^31607-1 | R3; no prime < 31,607 divides it (checked) |
| 9, 25, 49, 961 = 31² | 3, 5, 7, 31 | composite | prime squares: the loop must test d with d² = p |
| 999,999,998 | 2 | composite | even, near the bound |
| 999,999,986 = 2 · 499,999,993 | 2 | composite | 2 · prime |
| 998,812,807 = 31,601 · 31,607 | 31,601 | composite | second-largest semiprime of large primes |
| 998,623,201 = 31,601² | 31,601 | composite | next prime square down |
| 999,999,929 | — | tests run | next prime below 999,999,937 |

The prime squares (9, 25, 49, 961, 31,601², 31,607²) are the off-by-one guard: a `d < p / d`
or `d * d < p` bug misses each of them. For pins in `tests/cli.sh`, use 4, 33, 999,999,999 and
999,002,449, plus 9 for that boundary. The rest belong in `check_math.cpp`, where they are cheap.

## 4. How 2^a − 1 is written

**Recommend `2^a-1` with a in decimal, no spaces (`"2^3-1"`, `"2^31607-1"`), in both the JSON
`factors` array and the verdict line.** It matches `^2\^[1-9][0-9]*-1$`.

- `"2^3 - 1"`: the spaces break whitespace tokenization of the verdict line. Rejected.
- `"M3"`: collides visually with the `M<p>` exponent token R6 puts on the same line
  (`M33 … factor M3`). Rejected.
- Decimal when small (for example a ≤ 63 fits in 64 bits): gives two forms for one field, so
  consumers must handle both, and only small a would get the decimal. Printing a large factor in
  decimal is a GOAL non-goal that waits for `issues/reference-big-integer.md`. Rejected.
- One form everywhere means the verdict line and the JSON record show the same string,
  derivable from the result alone (R1).

**Trade-off.** In GIMPS's JSON results, as their public documentation describes them, `factors`
holds decimal strings. A consumer that reuses a GIMPS parser would choke on `2^3-1`. Mitigations:

- The record's `worktype` is `ptest`, which no GIMPS record carries, so a consumer can dispatch on
  it before reading `factors`.
- A later decimal form is a format change: it bumps the format-version field (§ Conventions).

The plan should state this in the format's documentation comment.

## 5. `-s/--skip-ptest` and what a prime exponent prints

- `-s` means `check_exponent` is not called at all, so no exponent-check result exists and none is
  printed (R5). The flag is parsed at `src/cli.cpp:153-156` into `skip_ptest`
  (`src/cli.hpp:96`) and read nowhere today. `mps -s 33` runs tf, p1 and ll, gets three
  no-verdict results and exits 42. `-s` on a prime p behaves as it does without `-s`.
- Help text (R11): replace `Suppress initial prime test on PVAL.` (`src/main.hpp:42`) with, for
  example, `Skip the exponent check (that PVAL is prime).` Keep it within the 80-column layout
  of `src/main.hpp:41-49`.
- **Prime PVAL: nothing on stdout (R4), and recommend nothing on stderr.**
  `tests/cli.sh:178,183,188` compare stderr *exactly* to the test start lines. An info line would
  break them.
- **Do not add a `logging::debug` call inside `src/math.cpp`.** Under
  `issues/logger-state-per-translation-unit.md`, `math.cpp`'s copy of `level` is zero-initialized
  to `DEBUG` (`src/log.hpp:46-52`, `:93-94`, `:177-179`). A debug line there would therefore
  *always* print and break the exact-stderr checks. If a debug line is wanted, emit it from
  `src/main.cpp`, where `-d` sets the level (`src/main.cpp:30-31`).
- A composite PVAL: R3 forbids any test start line. Recommend no extra stderr text either. The
  verdict line on stdout says it all.

## 6. `-t` names

`test_by_name` (`src/cli.hpp:51-60`) has `all`, `tf`, `trial-factor`, `p1`, `p-minus-1`, `ll`
and `lucas-lehmer`, and an absent `-t` defaults to `TEST_ALL` (`src/cli.hpp:87`). The exponent
check must run *before* the `switch (opt.test_case)` (`src/main.cpp:40`) and independently of it.
Then one placement covers all 8 cases. The `tests/cli.sh` loop at `:169-179` already iterates
these 7 names plus the bare form at `:181`. Extend it with composites 4, 33, 999,999,999 and
999,002,449: exit 40, stderr lacks `Running`, and stdout is exactly one line naming
`2^a-1` / `ptest`. That is 8 × 4 = 32 runs, cheap.

## 7. `-p`

`main` returns at `src/main.cpp:33-36`, after `display_primes` and before PVAL reaches any test.
Insert the exponent check between `:36` and `:40`. `-p` then never runs it, which is required:
PVAL may be 0 or 1 with `-p` (`src/cli.cpp:212`), and `smallest_prime_factor` must not be
reached with p < 2. `tests/cli.sh:156` already pins composite PVALs with `-p` (4, 30, 32 → exit
0, prime listing), so the ordering is guarded.

## Recommendations

1. Add `smallest_prime_factor(p)` to `src/math.*`: 2, then odd `d <= p / d`; return p when p is
   prime; reject p < 2. Cite trial division (Crandall & Pomerance 2005, ch. 3) in its comment.
2. Build composite exponent-check results only through `composite_by_exponent(p, a)`. Its guard,
   `1 < a and a < p and p % a == 0` in that order, is the § 1 exact re-check. A failed guard
   returns an error result (exit 30 path per R9). The comment proves the equivalence with
   2^p ≡ 1 (mod 2^a − 1).
3. `check_exponent(p)` returns no result for a prime p. Never *prime*, never *no verdict*.
4. Pin smallest-prime-ness with checks, not the guard: a known-answer table (§ 3, with
   source commands), a sieve-differential sweep for n < 100,000, and guard-refusal checks with
   constructed bad (p, a).
5. Write the factor as `2^a-1` in both the line and `factors`. Document the decimal-consumer
   trade-off and that a decimal form would be a format-version bump.
6. Call the exponent check in `main` after the `-p` return and before the test switch, skipped
   when `opt.skip_ptest` is set. Print nothing for a prime exponent, and log nothing from
   `math.cpp`.
7. Update `tests/cli.sh:233-241` (999999999 now exits 40 and runs no test) and the `-s` help
   text at `src/main.hpp:42` in the same commit as the parser and usage (R11, § Conventions).

## Sources

- Python 3, scratch `c4-plan/r02/spf.py`: `spf(n)` returns 2 if n is even, else the first odd
  `d` with `d*d <= n` and `n % d == 0`, else n.
  - `math.isqrt(999_999_999) == 31622`
  - the primes ≤ 31,622 are 3,401 in number, the largest 31,607, then 31,601 and 31,583
  - `spf(999_002_449) == 31607`, and no prime < 31,607 divides it
  - `(2**p - 1) % (2**a - 1) == 0` for p ∈ {4, 9, 25, 33, 961}
  - `pow(2, 999_002_449, 2**31607 - 1) == 1`
