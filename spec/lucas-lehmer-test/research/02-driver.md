# Research 02 — The Lucas–Lehmer driver on the FP64 engine

Scope: GOAL R3, R4, R6–R11, R13, R14 and R16's injection clause. A new module `src/lucas.hpp`/`src/lucas.cpp`, namespace `lucas`. `test_lucas_lehmer` (`src/math.cpp:88`) maps its outcome onto a `Result`. Everything below was read at the cycle base. The arithmetic was checked in Python for every shift k₀ of every p in {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 61, 89, 107, 127}, and the timings were taken on a scratch build (§ 7).

## 1. The shift arithmetic

The engine supplies everything the shift needs. `square_subtract(engine, x, bit)` takes any bit < p (`src/ibdwt.hpp:171`). The engine "chooses no shift and keeps nothing between calls" (`src/ibdwt.hpp:26`). The checks' `chain` already runs a shifted Lucas–Lehmer recurrence and rotates the result back on `BigInt` (`tests/engine_support.cpp:658`–`683`). The driver uses the same formulas.

Notation: i is the number of squarings done, k_i is the shift after them, and r_i ≡ 2^(k_i)·s_i (mod M_p), with 0 ≤ r_i < M_p as `from_digits` returns it (`src/ibdwt.hpp:163`).

- **Start.** Given k₀ in [0, p), r₀ = 4·2^(k₀) mod M_p = **2^((k₀+2) mod p)**, a single power of two built exactly on `BigInt` as `shift_left(BigInt(1), (k0 + 2) % p)`. It has at most p bits, so `to_digits` accepts it (`src/ibdwt.hpp:156`). For p = 3 and k₀ = 1 it is 2^0 = 1 = 8 mod 7.
- **Step.** `square_subtract(engine, x, (2*k + 1) % p)`, then `k = 2*k % p`. With p < 2^25 nothing overflows `uint64_t`. Since 2 is invertible mod p, a shift of 0 stays 0 for the whole run and a nonzero shift never reaches 0.
- **Final shift.** For prime p, k_(p−2) = 2^(p−2)·k₀ ≡ k₀·(p+1)/2 (mod p), by Fermat. This is a cheap assertion for the checks to pin; it was verified for all the p and k₀ above.
- **Unshift.** s = `reduce_mersenne(shift_left(r, (p - k) % p), p)`, as `rotated` does (`tests/engine_support.cpp:662`). r is zero exactly when s is, so the zero test can be made on r. Res64 is `res64(s, p)` (`src/mp.hpp:86`).
- **Jacobi argument.** a = r − 2^((k+1) mod p) mod M_p, and J(a | M_p) = J(s_i − 2 | M_p) (`doc/lucas-lehmer.md:88`–`92`). On `BigInt`: let t = 2^((k+1)%p); a = r − t if r ≥ t, else r + M_p − t. A result of a = 0 gives symbol 0, which is a failure (R4). When k = p − 1, t = 2^0 = 1.
- **Degenerate values (R7).** s ∈ {1, −1, 2, −2} exactly when r ∈ {2^k, M−2^k, 2^((k+1)%p), M−2^((k+1)%p)}. Also r = 0 at a checked iteration i < p − 2 is a failure.
- **Prime confirmation (R8).** Since 2 ≡ 2^(p+1), the condition s_(p−3) ≡ ±2^((p+1)/2) becomes, under the shift, **r_(p−3) ∈ {2^e, M_p − 2^e} with e = (k_(p−3) + (p+1)/2) mod p**. If s_(p−3) ≡ ±2^((p+1)/2), then s_(p−2) = 0. So a final zero that was injected on a composite exponent always fails the confirmation, because the genuine s_(p−3) does not square to 2. Python agreed for every p and k₀ above: the confirmation holds exactly when s_(p−2) = 0.
- **Indexing.** s_i is the value after i squarings. The final value is s_(p−2), and a state's iteration lies in [0, p−2].
- **p = 3:** one squaring. Length 1 holds 3 to 24 (`src/lengths.cpp:29`), and `holds` needs max(3, N) ≤ p (`src/lengths.hpp:60`), so length 1 holds p = 3. A run at p = 3 and N = 1 ended in 0. Here r_(p−3) = r₀, and (p+1)/2 = 2, so the confirmation compares r₀ = 2^(k₀+2) with itself and passes by construction. The degenerate set for M = 7 is {1, 2, 5, 6}, and s₀ = 4 is outside it.
- **p = 5:** three squarings at N = 1. s₂ = 194 mod 31 = 8 = 2^3 = 2^((5+1)/2).

**Recommendation:** put this arithmetic in pure `BigInt` free functions in `lucas`, so the checks can grade it without the engine:

- `BigInt power_of_two(std::uint64_t e, std::uint64_t p)`
- `BigInt unshift(const BigInt& r, std::uint64_t k, std::uint64_t p)`
- `BigInt jacobi_argument(const BigInt& r, std::uint64_t k, std::uint64_t p)`
- `bool degenerate(const BigInt& r, std::uint64_t k, std::uint64_t p, bool last)`
- `bool confirms_prime(const BigInt& before_last, std::uint64_t k, std::uint64_t p)`

Use the shifted forms that the doc derives (`doc/lucas-lehmer.md:100`), with one check that J(r − 2^(k+1)) equals J(s − 2) computed on the unshifted value.

## 2. Good states and the schedule

The symbol checks do not catch every degenerate state. J(s − 2) passes for s = −2 (J(−4) = J(−1) = −1, because M_p ≡ 3 mod 4), for s = 1 (J(−1) = −1) and for s = 0 (J(−2) = −1). It fails only for s = 2 (symbol 0) and s = −1 (J(−3) = +1). R7's explicit test is therefore needed, not redundant.

A zero at any iteration j < p − 2, checked or not, cannot escape:

- 0 → −2 → 2 → 2 …, and 1 → −1 → −1 …, so the set {±1, ±2} is absorbing.
- The next check, or the final one, therefore sees a degenerate value. A zero at p − 3 gives s_(p−2) = −2, which the final check catches.

```cpp
struct GoodState {
	std::uint64_t iteration;   // squarings done
	std::uint64_t shift;       // k at that iteration
	BigInt residue;            // r = 2^k s mod M_p, 0 <= r < M_p
	std::uint64_t length;      // the length it was checked at (reported; a restore reloads at the current length)
};
```

- **Where checks run.** At every iteration i = L, 2L, … below p − 2, and at the final i = p − 2. The interval is L = `check_interval(p, checks)` = max(1, ⌈(p−2)/checks⌉). The schedule counts iterations and never time, so timing cannot change what is checked (§ Invariants 2).
- **What a check does.** A check at i reads r = `from_digits(engine, x)`, which leaves x unchanged, and runs the degenerate test and the Jacobi test. If both pass, then `good = {i, k, r, N}` and no `to_digits` is needed. The final check runs the same two tests, with 0 allowed, and on 0 it also runs the confirmation.
- **Snapshot before the last squaring.** At i = p − 3 the driver takes the snapshot `before_last = from_digits(...)`, after any hook (§ 4), for R8. It is not a good state and not a Jacobi check. For p = 3 the snapshot is r₀.
- **i = 0 is good by construction.** s₀ − 2 = 2 and J(2 | M_p) = +1: the invariant holds only for i ≥ 1 (`doc/lucas-lehmer.md:62`). The initial state is built exactly on `BigInt`, is not Jacobi-checked and is not counted. It is the first good state. Its value 4 lies outside {±1, ±2} for every p ≥ 3.
- **Cost per check, measured.** `from_digits` takes 2.5 µs at 10,303 and 20 µs at 100,069. `from_digits` + `to_digits` together take 4 µs and 41 µs. Against one squaring (27 µs and 205 µs) both are negligible, and the symbol dominates. A throwaway textbook binary Jacobi on `BigInt` (Cohen, *A Course in Computational Algebraic Number Theory*, 1993, Alg. 1.4.10, timing only) took:

  | p | one symbol | in squarings |
  |---|---|---|
  | 10,303 | 2.5 ms | about 93 |
  | 44,497 | 26 ms | — |
  | 86,243 | 90 ms | — |
  | 100,069 | 122 ms | about 600 |

  That is about 0.6–0.9% of a run per check at 10k–100k, and about 8% at p = 1279, where squarings are cheap. The symbol is quadratic, as is p squarings at quasi-linear cost each, so the ratio stays roughly flat with p. The real algorithm is another brief's.

**Recommendation:** `CHECKS_PER_RUN = 10` (about 6–9% overhead at the long tier's sizes), settable through `Options::checks`. The checks can set `checks = p` to check every iteration at small p. Export `check_interval` so a check can aim an injection at a check iteration.

## 3. Recovery and its limits

The engine leaves the residue untouched when it refuses. `monitored` throws before `squared` assigns `x = carry(...)` (`src/ibdwt.cpp:472`–`503`, `:595`–`607`). A `carry` that throws ("does not settle", `src/ibdwt.cpp:344`) is an `engine_error`, not a `roundoff_error`, and it leaves x untouched too.

The loop, in prose:

- **R9.** Catch `roundoff_error` from a squaring, log it, and call `square_subtract` once more on the same x with the same engine. If that is accepted, count `transient_faults++`, then continue.
- **R10.** If the replay is also refused, count `length_retries++`. If the count passes `LENGTH_RETRY_LIMIT`, or `next_length(p, N)` finds no supported n > N with `holds(n, p)`, return an error. Otherwise build `Engine(p, n)`, set x = `to_digits(new_engine, good.residue)`, i = good.iteration, k = good.shift, and stay at n for the rest of the run (append n to `lengths`).
  - Use `holds` and not an assumption: the table is monotone at this base, but `holds` is the contract (`src/lengths.cpp:359`).
- **R6.** On a failed check, count the failure against the check that failed. If the run's total failures pass `FAILURE_LIMIT`, return an error. Otherwise restore the good state at the current length.
- **Other errors.** Any other `engine_error` becomes an error result (exit 20) with its message. It is never retried and never escapes to `main`, whose catch-all would bypass the result's fields and `-t all`'s stop rule.

Larger supported lengths that hold p. `holds` needs p ≥ N, and length 1 holds only up to 24:

| p | `length_for` | larger holders | count |
|---|---|---|---|
| 31 | 2 | 3, 4, …, 15, 16, 18, 20, …, 30 | 21 larger, 22 in all |
| 521 | 24 | 26, 28, 30, 32, 36, …, 448, 480, 512 | 36 larger |
| 1279 | 56 | 60, 64, 72, …, 960, 1024, 1152 | 35 larger |
| 3 | 1 | 2, 3 | 2 larger |

**Recommendation:**

- `FAILURE_LIMIT = 3`: the fourth failed error check in a run is an error.
- `LENGTH_RETRY_LIMIT = 3`.
- Each refusal gets exactly one replay. Transients get no limit, because every accepted replay makes progress, but they are counted.
- Counts are per run and never reset.
- Both R10 error branches can be checked: at p = 3, a refusal at every length runs out of lengths after two retries (1 → 2 → 3). At p = 1277 it reaches the retry limit.

Error reasons. `roundoff_error::what()` already reads "exponent p at length N: why (round-off distance d, threshold t)" (`src/ibdwt.cpp:488`–`490`), so R10 reuses it:

- R10, no larger length: `the round-off refusal at iteration I reproduced and no larger length holds the exponent: <what()>`
- R10, retry limit: `the round-off refusal at iteration I reproduced after 3 length retries, the limit: <what()>`
- R6: `4 error checks failed, past the limit of 3; the last was the Jacobi check at iteration I (symbol +1)`. The last clause takes one of the forms `the degenerate-state check at iteration I (residue -2)`, `the prime confirmation`, or `the Jacobi check … (symbol 0)`.

## 4. The injection seam (R16)

**Recommendation:** an `Options` struct whose test-only hooks are `std::function` members that `bin/mps` never sets.

```cpp
namespace lucas {
	struct Step { std::uint64_t iteration; std::uint64_t shift; const ibdwt::Engine& engine; bool checked; };
	struct Hooks {
		// before every engine squaring, replays included; may throw roundoff_error
		std::function<void(const Step& next)> before_square;
		// after every accepted squaring, before the check or snapshot at that iteration; may change x
		std::function<void(const Step& done, ibdwt::Residue& x)> after_square;
	};
	struct Options {
		std::optional<std::uint64_t> shift;            // none: chosen at random (R3)
		std::uint64_t checks = CHECKS_PER_RUN;
		double progress_seconds = PROGRESS_SECONDS;
		std::optional<std::uint64_t> progress_every;   // iterations; the checks set it, and it overrides the clock
		double sizing_limit = ibdwt::SIZING_DISTANCE;  // see § 5
		Hooks hooks;                                   // the checks' only
	};
	Outcome run(std::uint64_t p, const Options& options = {});
}
```

`test_lucas_lehmer` calls `lucas::run(p, lucas::Options{})`, and nothing in `src/cli.*` or `getenv` reaches `Hooks`. A review can show that with a grep: only `tests/` assigns a hook. An abstract stepper interface would do the same job with a virtual class and a factory, and buys nothing over two `std::function`s.

How each injection is made:

- **A corruption the Jacobi check is sure to catch.** Inject at a check iteration, setting `x = to_digits(engine, add(power_of_two((k+1)%p, p), BigInt(1)))`. Then r − 2^(k+1) = 1 and J(1) = +1 whatever the factorization of M_p. The value is not degenerate for p ≥ 5. A corruption at a non-check iteration is caught with probability of only about ½ (`doc/lucas-lehmer.md:104`), so R6's "one failure counted" must not rely on one.
  - "Once": a lambda with a captured `bool fired`.
  - "Every attempt": no flag. The redo hits the same iteration and ends at the fourth failure.
- **Zeroed residue.** Use `x.digits.assign(engine.length(), 0)`.
  - At j < p − 2: either the check at j sees 0, or a later check sees ±2.
  - At j = p − 2: this must be on a composite exponent, because zeroing a prime's final residue gives the right answer. Use 1237, 1277, or p = 11. R8's confirmation then fails, since the snapshot at p − 3 is genuine.
- **Refusals.** The hook throws a real `roundoff_error(msg, p, N, 0.46875, engine.threshold())`; the constructor is public (`src/ibdwt.hpp:62`). It throws before the engine is called, so the residue is untouched by construction, and the replay calls the hook again.
  - Once: a flag.
  - At one length but not the next: throw when `engine.length() == length_for(p)`.
  - At every length: throw always.

## 5. R11 and R13: round-off and progress

- **R11.** After every accepted squaring, update `largest_distance = max(...)`. If d > `sizing_limit`, count `past_sizing++`.
  - Logging: the first such squaring at each length gets its own `logging::warning`. Later ones are folded into one warning at the next progress line and into the final count.
  - Distances are never past 0.35 in the known-answer runs: the largest measured was 0.25 at 44,497 (§ 7). A check therefore needs a lever. Lowering `Options::sizing_limit` (for example to 0.0625 at p = 1279, whose largest distance is 0.156) exercises the real path with real distances and no fake value. Keep the constant `ibdwt::SIZING_DISTANCE` for `bin/mps`.
- **R13.** Use a time-based interval for `bin/mps` with `PROGRESS_SECONDS = 60`. Reading `steady_clock` once per iteration costs about 20 ns, against at least 0.25 µs per squaring. `progress_every` gives the checks deterministic lines.
  - Time per iteration is measured over the interval since the last line. The estimate is the remaining iterations times that rate.

Proposed wording, all through `logging::` with variadic arguments (`src/log.hpp:193`–`233`):

- `info`: `Running Lucas-Lehmer test`. This is unchanged: `tests/cli.sh:169`–`207` compares stderr exactly to the three start lines, for p = 2, 3, 31 and 999999929 among others.
- `debug`: `Lucas-Lehmer test of M_p at length N, shift k, a check every L iterations`. The shift is random, so it must not appear in default-level stderr.
- `info`: `Iteration 43000 of 86241 (49.9%): 136 us an iteration, about 5.9 s to go, largest round-off distance 0.156 since the last line`
- `warning`: `Round-off distance 0.3672 at iteration 12345 at length 4096, past the sizing limit 0.35`, and the folded form `7 more squarings past the sizing limit 0.35 since the last line, the largest 0.3812`
- `warning`: `Round-off refused at iteration I: <what()>; replaying the squaring`. Then `info`: `The replay was accepted: a transient fault`, or `warning`: `The replay was refused too: back to iteration J at length N2`.
- `warning`: `The Jacobi check failed at iteration I (symbol +1): back to iteration J` (and its degenerate and confirmation forms).

A clean run under 60 s logs only the start line. The R14 refusals log nothing more, so `tests/cli.sh`'s stderr expectations hold.

## 6. R14: refusals before any squaring

There is no P_max function. `largest_exponent(LARGEST_LENGTH)` returns 20,381,696 (`src/lengths.cpp:172`). That equals the table's maximum only because the table is monotone, which a scan at this base confirms. The Non-goals keep `src/lengths.*` unchanged.

**Recommendation:** `lucas::largest_exponent()` = the maximum of `ibdwt::largest_exponent(n)` over `supported_lengths()`. The order of tests in `lucas::run`, before any `Engine` is built:

- p < 3 or p even: no verdict, `the Lucas-Lehmer test applies to odd exponents from 3`. With the exponent check on, only p = 2 reaches this (`PVAL ≥ 2`, `src/cli.cpp:218`); with `-s`, any even p does.
- p > P_max: no verdict, `the length table holds exponents up to 20381696`. `tests/cli.sh:200` already runs `mps 999999929`.
- **Odd composite p, reachable only with `-s`: refuse with no verdict too**, `the Lucas-Lehmer test runs on prime exponents (p has the factor a)`, using `smallest_prime_factor`. The doc shows that a zero symbol is genuine at p = 9, 15, 21, 25 (`doc/lucas-lehmer.md:96`). R4 would then turn a correct run into "an error check failed 4 times", which misreports a hardware fault. This is beyond R14's letter, so it is the planner's call.

## 7. Cost (an M2 Pro Mac, Apple clang 21; residues agreed with `tests/known/`)

`p − 2` × `square_subtract` at `length_for(p)`, shifted by p/3 + 1, from `to_digits` to `from_digits`:

| p | N | µs/squaring, `make` | µs/squaring, BOUNDS | run, `make` | largest distance |
|---|---|---|---|---|---|
| 1279 | 56 | 2.49 | 3.1 | 3 ms | 0.156 |
| 9941 | 448 | 17.8 | 23.3 | 0.18 s | 0.219 |
| 10303 | 480 | 27.1 | 36.0 | 0.28 s | 0.094 |
| 44497 | 2048 | 69.7 | 94.8 | 3.1 s | 0.250 |
| 86243 | 4096 | 136.5 | 184.0 | 11.8 s | 0.156 |
| 100069 | 5120 | 204.6 | ≈276 (×1.35) | 20.5 s | 0.023 |

Sums:

- **`make check`'s set:** the 21 odd primes up to 9941, plus 1237, 1277 and 10303. One pass takes 0.76 s on `make` and 1.00 s with BOUNDS. R3's pairs (a given shift plus 0) make it 2.0 s. Adding about 11 symbols per run gives about 2.4 s.
  - Injection checks at p ≤ 1279 cost milliseconds each.
  - That is about half of R16's 5 s. Run the R1/R2 checks at a nonzero shift and add only a shift-0 run per exponent; never use three runs.
- **The long tier:** the odd primes up to 86243 plus 100069. A pass takes 39.5 s on `make` and about 53 s with BOUNDS. That leaves wide room within 15 minutes, even doubled.
- Lengths with odd factor 15 are slow per word: 19937 at 960 takes 53 µs, against 35 µs for 21701 at 1024. GCC was not timed.

## Risks

- The random shift appears in the verdict line, so `tests/cli.sh`'s stdout comparisons for `-t ll` and `-t all` must mask it. The checks pin shifts only through `Options::shift`.
- A shift of 0 is drawn with probability 1/p (⅓ at p = 3). That is harmless: R3 asks for a fresh shift, not a nonzero one. Use `std::random_device`.
- R8's injection is meaningful only on a composite exponent. R6's "exactly one failure" holds only for a corruption at a check iteration.
- Hooks live in a public struct. Their only defence is that `bin/mps` never sets them, which is grep-checkable, and the plan should say so explicitly.
- A deterministic silent wrong squaring repeats on every redo at the same length and ends in R6's error, not in a length change. That is § Invariants 1-correct but costs four redos. Moving length after repeated failures is a possible refinement, not required.
- Jacobi cost dominates checks at small p, and the textbook algorithm grows quadratically. Past the long tier it needs `issues/large-gcd-and-jacobi.md`.
