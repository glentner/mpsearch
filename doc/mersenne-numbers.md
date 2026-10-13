# Mersenne numbers, and why the exponent must be prime

## The result

For a positive integer $n$, the Mersenne number is $M_n = 2^n - 1$. Mersenne primes, the Mersenne numbers that are prime, are the subject of [CrandallPomerance2005, § 1.3.1](references.md#crandallpomerance2005).

**Theorem.** Let $a$ and $p$ be positive integers.

1. If $a \mid p$, then $2^a - 1 \mid 2^p - 1$.
2. Conversely, if $a \ge 2$ and $2^a - 1 \mid 2^p - 1$, then $a \mid p$.

**Corollary.** Let $p$ be a positive integer. If $M_p$ is prime, then $p$ is prime. For a composite $p$ it names a factor: if $a$ is any divisor of $p$ with $1 < a < p$, then $2^a - 1$ is a proper factor of $M_p$, and $M_p$ is composite.

The corollary's converse is false. <!-- check: primes(11) == [2, 3, 5, 7, 11] and isprime(mersenne(2)) and isprime(mersenne(3)) and isprime(mersenne(5)) and isprime(mersenne(7)) and mersenne(11) == 2047 and 2047 == 23 * 89 --> $M_p$ is prime for $p = 2, 3, 5, 7$, but the next prime, $p = 11$, gives $M_{11} = 2047 = 23 \cdot 89$<!-- /check -->. A prime exponent is necessary for a Mersenne prime, never sufficient: it only makes $M_p$ worth testing.

## Proof

**Part 1.** Let $a \mid p$, and write $p = ak$ with $k$ a positive integer. For every integer $x$,

$$(x - 1) \sum_{j=0}^{k-1} x^j = \sum_{j=1}^{k} x^j - \sum_{j=0}^{k-1} x^j = x^k - 1,$$

since the two sums share every term but $x^k$ and $x^0 = 1$. Put $x = 2^a$. Then $x^k = 2^{ak} = 2^p$, and

$$2^p - 1 = (2^a - 1) \sum_{j=0}^{k-1} 2^{aj}.$$

The sum is a positive integer, so $2^a - 1$ divides $2^p - 1$. For example, <!-- check: 15 == 3 * 5 and 2**15 - 1 == 32767 and 32767 == 7 * 31 * 151 and 2**3 - 1 == 7 and 2**5 - 1 == 31 and 2**15 - 1 == (2**5 - 1) * (2**10 + 2**5 + 1) --> $15 = 3 \cdot 5$, so $2^{15} - 1 = 32767 = 7 \cdot 31 \cdot 151$ is divisible by $2^3 - 1 = 7$ and by $2^5 - 1 = 31$, and $2^{15} - 1 = (2^5 - 1)(2^{10} + 2^5 + 1)$<!-- /check -->.

**Part 2.** Let $a \ge 2$, and divide: $p = ka + r$ with integers $k \ge 0$ and $0 \le r < a$. Since $2^a \equiv 1 \pmod{2^a - 1}$,

$$2^p = (2^a)^k \, 2^r \equiv 2^r \pmod{2^a - 1}.$$

Now $1 \le 2^r \le 2^{a-1}$, and $2^{a-1} < 2^a - 1$ because $a \ge 2$. So $2^r$ is its own least residue modulo $2^a - 1$, and it is 1 only when $r = 0$. Hence $2^p \equiv 1 \pmod{2^a - 1}$, which says exactly that $2^a - 1 \mid 2^p - 1$, holds if and only if $r = 0$, that is, if and only if $a \mid p$. This proves Part 2, and gives Part 1 again for $a \ge 2$. For example, <!-- check: 2**4 - 1 == 15 and 2**6 - 1 == 63 and 63 % 15 != 0 and 6 % 4 != 0 --> $2^4 - 1 = 15$ does not divide $2^6 - 1 = 63$, since 4 does not divide 6<!-- /check -->. The hypothesis $a \ge 2$ is needed: $2^1 - 1 = 1$ divides every $2^p - 1$.

**The corollary.** Let $M_p$ be prime. <!-- check: mersenne(1) == 1 and not isprime(1) --> $M_1 = 1$ is not prime<!-- /check -->, so $p \ge 2$. Suppose $p$ were composite. Then it has a divisor $a$ with $1 < a < p$, and $2^a - 1$ divides $M_p$ by Part 1. Since $2 \le a < p$ and $2^n - 1$ increases with $n$, $1 < 2^a - 1 < 2^p - 1 = M_p$. So $M_p$ has a divisor strictly between 1 and itself and is not prime, a contradiction. Hence $p$ is prime. The same argument proves the form that names a factor: for a composite $p$, any divisor $1 < a < p$ gives the proper factor $2^a - 1$ of $M_p$.

## What mps uses

`mps` applies the corollary to its exponent before any test runs: the exponent check, `check_exponent` in `src/math.cpp`, which `src/main.cpp` calls first unless `-s` skips it. The exponent is at least 2, since the option parser refuses smaller ones.

- **The search.** `smallest_prime_factor` finds the smallest prime factor $a$ of $p$ by trial division ([CrandallPomerance2005, § 3.1](references.md#crandallpomerance2005)): it tries 2, then every odd $d$ with $d \le p / d$, so that $d^2 \le p$ without overflow. The first $d$ that divides $p$ is prime, because a smaller prime factor of $d$ would divide $p$ and have been found first. When no $d$ divides $p$, $p$ has no prime factor up to $\sqrt{p}$ and is prime, and the search returns $p$ itself.
- **A prime exponent.** When $a = p$, the corollary says nothing about $M_p$. The check gives no result at all, and the tests run.
- **A composite exponent.** When $a \ne p$, `check_exponent_by` hands $a$ to `composite_by_exponent`, the only place the check writes a factor. It reports $M_p$ composite by the factor $2^a - 1$ only after the exact re-check that $2^p \equiv 1 \pmod{2^a - 1}$. By Part 2 that congruence holds, for $a \ge 2$, exactly when $a \mid p$, so the function decides it in exact machine integers, without computing $2^p$: it requires $1 < a < p$ and $a \mid p$. Then $2^a - 1$ is a proper factor by the corollary, and the verdict is a composite from the exponent check, its only result for the run.
- **A wrong search.** The search is passed in to `check_exponent_by`, and its answer is trusted no further than the re-check. An $a$ that is 0 or 1, or does not divide $p$, gives an error, never a composite.

The sieve behind `-p`, `prime_sieve` (which `display_primes` prints), is the sieve of Eratosthenes ([CrandallPomerance2005, § 3.2](references.md#crandallpomerance2005)). It is not part of the exponent check.

## Sources

- [CrandallPomerance2005, § 1.3.1](references.md#crandallpomerance2005): Mersenne primes.
- [CrandallPomerance2005, § 3.1](references.md#crandallpomerance2005): trial division, as `smallest_prime_factor` does it.
- [CrandallPomerance2005, § 3.2](references.md#crandallpomerance2005): sieving, and the sieve of Eratosthenes `prime_sieve` uses.

## Provenance

The theorem, its converse and the corollary are elementary, and are proved in full above from the geometric sum and division with remainder. The exponent check is built from them and from trial division as [CrandallPomerance2005, § 3.1](references.md#crandallpomerance2005) describes it. The sieve comes from [CrandallPomerance2005, § 3.2](references.md#crandallpomerance2005). Nothing here is drawn from another program.
