# Research 03 — The sieve at its edges, and checks that prove R8–R10

Worker brief, 2026-10-12. Everything below was observed in scratch copies of `HEAD` with Apple clang 21 (libc++) and Homebrew g++-16.2 (libstdc++), GNU make 3.81. Libstdc++ 12–15 headers were read from the gcc-mirror release branches.

## 1. Today's defects, observed

The old sieve returns these values (`prime_sieve(n)`, entries from 0), at -O0 and at -O3 on both compilers:

| n | result |
|---|---|
| 0 | crash (see below) |
| 1 | `0`, then bit 2 of the storage word is set outside the vector's range |
| 2 | `0 1`: entry 1 is true, and index 2 is written out of range |
| 3 | `0 1 1`: entry 1 is true |
| 5 | `0 1 1 1 0` |

Entry 1 is true for every n ≥ 2, because the even loop starts at 0 and 1 is never cleared.

- **`mps -p 0` ends in "std::bad_alloc" because of undefined behaviour, not because of `std::stoi`.** `stoi("0")` returns 0 and `size_t(0)` is fine. The empty vector's storage pointer is null, so `is_prime[2] = true` writes through it. Clang -O3 therefore assumes n > 0 and deletes libc++'s `n > 0` guard around allocation. The asm shows `((n - 1) >> 6) + 1` words, which is 2^58 words (2^61 bytes) for n = 0, so `operator new` throws `bad_alloc`. Other builds do other things: g++-16 -O3 and clang -O0 crash with SIGSEGV (exit 139), and g++-16 -O0 aborts on a libstdc++ assertion. The cluster's GCC 14 would most likely segfault rather than throw.
- **`mps -p 1` and `-p 2`** exit 0 and print nothing. The stray bit lands inside the 64-bit word that was allocated, so neither the output, a value check nor ASan sees it. ASan (`-fsanitize=address`) catches n = 0 only, as a null dereference.
- **`display_primes`** loops `for (i = 3; i < n; i += 2)`, so it never prints 2. Today `-p 3` prints nothing and `-p 4` prints `3`.
- The loop bound `i * i < n` is already correct: a composite j < n has a prime factor p with p² ≤ j ≤ n − 1.

## 2. Corrected sieve (design altitude; proven in scratch)

```cpp
std::vector<bool> prime_sieve(size_t n) {
	//
	// Primes below n
	//
	// Returns n entries, entry i true exactly when i is prime, by the sieve of Eratosthenes
	// (Crandall and Pomerance, "Prime Numbers: A Computational Perspective", 2nd ed., 2005,
	// ch. 3, "Sieving"). Every index written is below n, so n < 3 returns n false entries.
	// Only odd multiples of an odd i are struck, from i * i: a smaller multiple has a smaller
	// prime factor and is already struck, and i * i < n cannot overflow for any n a vector holds.

	std::vector<bool> is_prime(n, false);

	// 2 is the only even prime
	if (n > 2)
		is_prime[2] = true;

	for (size_t i = 3; i < n; i += 2)
		is_prime[i] = true;

	for (size_t i = 3; i * i < n; i += 2)
		if (is_prime[i])
			for (size_t j = i * i; j < n; j += 2 * i)
				is_prime[j] = false;

	return is_prime;
}
```

`display_primes` becomes `for (size_t i = 2; i < n; i++) if (is_prime[i]) std::cout << i << std::endl;` with a two-line doc block.

- **Bounds:** every write is guarded by `< n`, and `is_prime[i]` is read only when `i * i < n`, which implies `i < n`.
- **Overflow:** for n ≤ 999,999,999 the largest i is about 31,623, so `i * i` and `j + 2i` stay far below 2^64.
- **Counts:** the corrected sieve prints π(10⁸) = 5,761,455 and π(10⁹) = 50,847,534 lines for `-p 100000000` and `-p 999999999`, matching the published values.
- **Speed:** `-p 999999999` takes 24 s, mostly system time spent on 50M `std::endl` flushes. Using `'\n'` would be faster; it is optional and out of scope.
- **Hardening:** the corrected sieve is clean under every hardening mode tested, on both compilers, with no warnings.
- **Citation:** I'm confident of Crandall & Pomerance, 2nd ed. (Springer, 2005), ch. 3. I believe the section is §3.2 "Sieving", but the builder should confirm it before citing a section number. No page numbers.

## 3. Observing "writes nothing outside them"

The old sieve, built with each mode (exit status for n = 0 / 1 / 2 / 3):

| build | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| clang -O1, no hardening | 139 | 0 | 0 | 0 |
| libc++ `_LIBCPP_HARDENING_MODE_FAST` | 133 (silent trap) | 133 | 133 | 0 |
| libc++ `_EXTENSIVE` | 133 | 133 | 133 | 0 |
| libc++ `_DEBUG` | 134, prints "Hardening assertion __n < size()" | 134 | 134 | 0 |
| g++-16 `-D_GLIBCXX_ASSERTIONS` | 134, prints "Assertion '__n < this->size()'" | 134 | 134 | 0 |
| g++-16 `-D_GLIBCXX_DEBUG` | 134 | 134 | 134 | 0 |
| clang `-fsanitize=address` | 134 | 0 | 0 | 0 |

- **Apple libc++** checks `vector<bool>::operator[]` from FAST mode up.
- **libstdc++ 12, 13 and 14 do not check `vector<bool>::operator[]` under `_GLIBCXX_ASSERTIONS`.** In those versions it is a bare `{ return begin()[__n]; }`. The check arrived in GCC 15, and GCC 15+ also enables assertions by default at -O0.
- **On GCC 14** only `_GLIBCXX_DEBUG` catches the write: `debug/vector` applies `__glibcxx_check_subscript`. That mode changes the ABI, and the return type of `prime_sieve` is not part of its mangled name, so every object must be built with it (`make -B` rebuilds all of them). It is fine for a one-off rebuild of the whole tree, not for mixing objects.

### Weighing the options

- **(a) Harden the whole build.** This adds bounds checks to future hot arithmetic, and it touches flags that § Invariants 2 owns. Reject.
- **(b) Separate hardened objects for `bin/check`.** `bin/check` would then test different code from what `bin/mps` ships. That breaks the premise of future differential tests (Inv. 2: the shipped -O3/IEEE code is what gets checked). It also adds a second object dir, pattern rules, deps and a `clean` scope. Reject.
- **(c) Recommended.** `make check` stays on the shipped flags and pins sizes and every value. The phase gate rebuilds the copy hardened (`make -B CXXFLAGS='-O1 -Wall -Wextra -D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE' check`, with clang++ and g++-16) and requires it to pass.
  - The two defines are inert on the other library, `$(IEEE)` still follows on every compile, and the build is warning-free.
  - The gate also runs a **canary**: it compiles `std::vector<bool> v(1); v[2] = true;` with the same CXX and CXXFLAGS and requires a non-zero exit. This proves the toolchain really checks, so a GCC-14-like library cannot make the hardened run hollow.
  - **Trade-off:** plain `make check` on CI cannot see an in-word write for n = 1 or 2. That leg rests on construction (every write is `< n`), review, and the gate's evidence.
- **(d) Simpler alternatives fail.** `.at()` in the sieve makes the leg visible in plain `make check` everywhere, but it costs a compare per write, and the gate's mutants could only prove it by grepping the builder's code. Reading past `size()` through iterators is UB itself.

## 4. Non-hollow checks (prototyped; drafts in scratch)

### `tests/check_math.cpp`

- Drop the header caveat.
- Add "sieve below four exactly": for n = 0..3, `expect_equal(size, n)`, then each entry against the table `{}`, `{F}`, `{F,F}`, `{F,F,T}`. Use `expect_equal(bool(v[i]), bool(e[i]), "prime_sieve(n)[i]")`; `vector<bool>` itself isn't streamable.
- In "sieve matches trial division", start the n list at `0, 1, 2, 3, 4, 5, 9, …` and the index loop at 0.
- Keep the squares check.
- The `require(size == n)` before indexing keeps the check itself in range.

### `tests/cli.sh`

- Drop the "leading 2 is accepted too" branch and the header line "Only behaviour that is right today is pinned".
- For n in 0, 1 and 2: expect "exits 0" and "prints nothing" (`[ -z "$out" ]`).
- For `3:2`, `4:2⏎3` and `30:2⏎3⏎…⏎29`: expect "exits 0" and exact equality.
- `planned` goes from 14 to 24.
- After each `run -p`, set `got="exit $status, stdout '$out'"`, because `run` keeps only the first line.
- `$(…)` strips trailing newlines, so `2` and `2⏎` are indistinguishable. That is acceptable for "one per line".

Result: clean on clang++ and g++-16, `check: 7 passed, 0 failed` and `cli: 24 passed, 0 failed`.

## 5. Gate mutation testing

The gate writes its **own** complete `src/math.cpp`: a correct reference and mutants, each implementing `math.hpp`'s exact signatures with the three log lines and the `test_all` chain. It swaps each into a fresh `copy_tree`, runs `make -B`, and asserts:

- the mutant **builds** (otherwise fail with "the gate's API assumption no longer holds", as P2 does);
- `make check` is **red**;
- the log matches a named line.

The reference must be green both plain and hardened. That proves the mutants differ only by their defect.

Prototype results:

| mutant (one defect) | today's suite | new suite, plain | new suite, hardened |
|---|---|---|---|
| m0: `d179aa4:src/math.cpp` verbatim (`git show`) | **green (missed)** | red: ` FAIL sieve below four exactly` (clang); g++ crashes at n = 0 (last line `check sieve below four exactly`); cli ` FAIL -p 3 prints the primes below 3` | red |
| m1: entry 1 true | **missed** | red: ` FAIL sieve below four exactly`, `check: 5 passed, 2 failed` | red |
| m2: size 3 when n < 3 | **missed** | red: same as m1 | red |
| m3: display starts at 3 | **missed** | red: `cli: 21 passed, 3 failed`, ` FAIL -p 3 prints the primes below 3` | red |
| m4: `if (n > 0) is_prime[2] = n > 2;` (in-word write only) | missed | **green, as expected** | red: bin/check dies after `check sieve below four exactly` with no `check:` line; ` FAIL -p 1 exits 0`, ` FAIL -p 2 exits 0` |
| m5: `i * i + 1 < n` (square at the bound) | red | red: ` FAIL sieve marks squares at the bound` | red |

Pinning m0 to the commit keeps the gate valid after landing; `origin/main` will move. m4 is the only mutant that needs the hardened run, and the canary is what makes that run trustworthy.

## Recommendations

1. Replace `prime_sieve` and `display_primes` with §2's design: initialize to false, set 2 when n > 2, set odd i ≥ 3, strike from i·i while `i * i < n`, and display from 2. Change nothing else in `src/math.*` and keep `math.hpp` unchanged.
2. Do not add hardening defines to the Makefile, and do not add a second object dir. The Makefile is unchanged by R9.
3. Rewrite `tests/check_math.cpp` and `tests/cli.sh` as §4 describes; `planned=24`. Remove every "known wrong" caveat (R10).
4. The phase gate:
   - requires plain `make -B check` green with clang++ and g++-16;
   - rebuilds hardened with `CXXFLAGS='-O1 -Wall -Wextra -D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE'` and requires green with no warnings;
   - first runs the out-of-range canary with those flags and requires a non-zero exit.
5. The gate writes its own reference `math.cpp` and mutants m1–m5, and uses `git show d179aa4:src/math.cpp` as m0. Each mutant must build and turn `make check` red with the named line in §5's table: m4 under the hardened build only, and the rest under the plain build. The reference must be green both ways.
6. The gate asserts the published counts: `bin/mps -p 100000000` prints 5,761,455 lines, ending in `99999989`.

## Open questions

- R9 says "`make check` SHALL show this", and "this" arguably includes "writes nothing outside". Under recommendation 4, `make check` shows sizes and values; the out-of-range leg is shown by the gate's hardened `make check` run. Does the foreman accept that reading, or should the hardened invocation be documented (for example in `AGENTS.md` or a comment in the Makefile) as the way to prove it?
- Should `display_primes` switch to `'\n'` while it is being edited? About 14 s of system time at the bound.
- Should the gate also try `-D_GLIBCXX_DEBUG`, so the hardened leg can be reproduced on the cluster's GCC 14? It works on g++-16 (m4 goes red).
