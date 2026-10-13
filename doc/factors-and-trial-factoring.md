# The form of factors, and trial factoring

## The result

**Theorem (the form of factors).** Let $p$ be an odd prime, and let $q$ be a prime factor of $M_p = 2^p - 1$. Then

1. $q = 2kp + 1$ for some integer $k \ge 1$, and
2. $q \equiv \pm 1 \pmod 8$.

**Corollary.** Every divisor $d > 1$ of $M_p$, prime or not, satisfies $d \equiv 1 \pmod{2p}$ and $d \equiv \pm 1 \pmod 8$. In terms of $k = (q - 1)/(2p)$, part 2 says $k \equiv 0$ or $k \equiv 3p \pmod 4$: two of the four classes of $k$ modulo 4.

The form was already "well known" to [Cole1903, p. 136](references.md#cole1903), who used it to sift candidates for the factors of $M_{67}$, and GIMPS states it as the starting point of its trial factoring [GIMPSMath](references.md#gimpsmath).

**Examples.** <!-- check: mersenne(11) == 23 * 89 and 23 == 2 * 1 * 11 + 1 and 89 == 2 * 4 * 11 + 1 and 23 % 8 == 7 and 89 % 8 == 1 --> $M_{11} = 23 \cdot 89$, with $23 = 2 \cdot 1 \cdot 11 + 1 \equiv 7$ and $89 = 2 \cdot 4 \cdot 11 + 1 \equiv 1 \pmod 8$<!-- /check -->. <!-- check: mersenne(29) == 233 * 1103 * 2089 and isprime(233) and isprime(1103) and isprime(2089) and 233 == 2 * 4 * 29 + 1 and 1103 == 2 * 19 * 29 + 1 and 2089 == 2 * 36 * 29 + 1 and 233 % 8 == 1 and 1103 % 8 == 7 and 2089 % 8 == 1 --> $M_{29} = 233 \cdot 1103 \cdot 2089$, a product of three primes, with $233 = 2 \cdot 4 \cdot 29 + 1$, $1103 = 2 \cdot 19 \cdot 29 + 1$ and $2089 = 2 \cdot 36 \cdot 29 + 1$, which are 1, 7 and 1 modulo 8<!-- /check -->. <!-- check: 11 % 4 == 3 and 1 % 4 == 1 and 4 % 4 == 0 and 29 % 4 == 1 and 19 % 4 == 3 and 36 % 4 == 0 --> Here $p = 11 \equiv 3 \pmod 4$, and $k = 1, 4$ lie in the classes 1 and 0 modulo 4; $p = 29 \equiv 1 \pmod 4$, and $k = 4, 19, 36$ lie in the classes 0, 3 and 0<!-- /check -->.

**The checks.** <!-- check: factor_form(100, 2**20) == 27 --> Trying every prime $q \le 2^{20}$, not only those of the form, against $M_p$ for every odd prime $p \le 100$ finds 27 prime factors, and each has the form the theorem states<!-- /check -->. A larger published factor: <!-- check: mersenne(67) == 193707721 * 761838257287 and isprime(193707721) and divides_mersenne(193707721, 67) and has_factor_form(193707721, 67) and isprime(761838257287) and divides_mersenne(761838257287, 67) and has_factor_form(761838257287, 67) --> Cole found $M_{67} = 193707721 \cdot 761838257287$ ([Cole1903, p. 137](references.md#cole1903)); both factors are prime, both divide $M_{67}$, and both have the form<!-- /check -->, <!-- check: 193707721 == 2 * 1445580 * 67 + 1 and 761838257287 == 2 * 5685360129 * 67 + 1 and 193707721 % 8 == 1 and 761838257287 % 8 == 7 --> $193707721 = 2 \cdot 1445580 \cdot 67 + 1 \equiv 1$ and $761838257287 = 2 \cdot 5685360129 \cdot 67 + 1 \equiv 7 \pmod 8$<!-- /check -->.

### Trial factoring

Trial factoring looks for a factor of $M_p$ below a bound $2^B$ by testing candidates $q$ directly. The theorem limits the candidates to $q = 2kp + 1$, $k = 1, 2, 3, \dots$, with $q \equiv \pm 1 \pmod 8$.

**The test of one candidate.** $q$ divides $M_p$ exactly when $2^p \equiv 1 \pmod q$. The power is computed left to right, one bit of $p$ at a time from the most significant, by squaring and, for each 1 bit, doubling ([CrandallPomerance2005, § 9.3.1](references.md#crandallpomerance2005)):

```text
x ← 1
for each bit b of p, from the most significant to the least:
    x ← x · x mod q
    if b is 1:
        x ← 2 · x mod q
report q as a candidate factor when x is 1
```

Doubling is a shift and at most one subtraction of $q$, so the cost is about $\lfloor \log_2 p \rfloor$ squarings modulo $q$. GIMPS works the same example [GIMPSMath](references.md#gimpsmath): <!-- check: pow(2, 1, 47) == 2 and pow(2, 2, 47) == 4 and pow(2, 5, 47) == 32 and pow(2, 11, 47) == 27 and pow(2, 23, 47) == 1 and 47 == 2 * 1 * 23 + 1 --> for $p = 23$ and $q = 47 = 2 \cdot 1 \cdot 23 + 1$, the leading bits of $p$ read 1, 2, 5, 11 and 23 in turn, and $x$ runs through 2, 4, 32, 27 and 1 modulo 47<!-- /check -->, so 47 divides $M_{23}$.

**The candidates, in classes.** Whether $q = 2kp + 1$ is $\pm 1 \bmod 8$, or divisible by a small odd prime $r \ne p$, depends only on $k$ modulo 4, or modulo $r$. So the values of $k$ split into residue classes modulo $4 \cdot 3 \cdot 5 \cdot 7 \cdot 11$, and a class either always gives such a $q$ or never does. <!-- check: 4 * 3 * 5 * 7 * 11 == 4620 and 2 * 2 * 4 * 6 * 10 == 960 --> For $p > 11$, of the $4 \cdot 3 \cdot 5 \cdot 7 \cdot 11 = 4620$ classes of $k$ modulo 4620, $2 \cdot 2 \cdot 4 \cdot 6 \cdot 10 = 960$ survive<!-- /check -->: two classes modulo 4 by the corollary, and $r - 1$ modulo each $r = 3, 5, 7, 11$, since exactly one class of $k$ modulo $r$ makes $r$ divide $2kp + 1$. Each surviving class is an arithmetic progression in $k$, searched on its own.

**The sieve.** Within a class, a candidate with a prime factor $r$ smaller than itself cannot be a prime factor of $M_p$. For each small prime $r$ not dividing $2p$, the $k$ in the class with $r \mid 2kp + 1$ again form one residue class modulo $r$, so a sieve of Eratosthenes over the progression strikes them out ([CrandallPomerance2005, § 3.2](references.md#crandallpomerance2005)), keeping a candidate that equals $r$ itself. GIMPS sieves its candidates in this way, by every prime up to a fixed bound, and clears those that are $\pm 3 \bmod 8$ [GIMPSMath](references.md#gimpsmath). A survivor may still be composite: testing it costs time, never correctness, since a composite that divides $M_p$ is still a factor, and its prime factors, of the same form by the theorem, lie in classes of their own.

**The cost.** The candidates $q$ in one bit level, $2^b \le q < 2^{b+1}$, come from about $2^{b-1}/p$ values of $k$. <!-- check: 1 * 2 * 4 * 6 * 10 * 4620 == 960 * 2 * 3 * 5 * 7 * 11 --> The classes keep the fraction $\frac{1}{2} \cdot \frac{2}{3} \cdot \frac{4}{5} \cdot \frac{6}{7} \cdot \frac{10}{11} = \frac{960}{4620}$ of them<!-- /check -->. The sieve by the primes $r$ from 13 to a bound $y$ keeps about $\prod (1 - 1/r)$ of those, and each survivor costs about $\log_2 p$ squarings of numbers below $2^{b+1}$. So a bit level costs about twice the one before it, the whole search to $2^B$ costs about as much as its last level, and a larger $p$ makes each level cheaper. GIMPS reports from past data that a factor between $2^x$ and $2^{x+1}$ is found with probability about $1/x$, and chooses the depth for each exponent so that the time spent factoring stays below the expected time saved on the primality tests [GIMPSMath](references.md#gimpsmath).

**Modular reduction.** A deep search needs exact products of numbers of $B$ bits reduced modulo $q$. Two methods avoid a division by $q$ in each step, at the price of a precomputation that serves every step with the same $q$:

- **Montgomery's method** keeps each residue $A$ as $\lambda A \bmod q$, for a constant $\lambda$ prime to $q$, usually a power of the word base. Addition is unchanged, and the product of two such residues is reduced to $\lambda A B \bmod q$ by multiplying by $\lambda^{-1}$ modulo $q$, which needs no division by $q$ when $\lambda$ is a power of the word base ([Montgomery1985, the abstract](references.md#montgomery1985); [BrentZimmermannMCA2010, § 2.4.2](references.md#brentzimmermannmca2010); [CrandallPomerance2005, § 9.2.1](references.md#crandallpomerance2005)).
- **Barrett's method** precomputes an approximation to $1/q$, so that the quotient, and then the remainder, of a product by $q$ take one multiplication each and a few corrections ([BrentZimmermannMCA2010, § 2.4.1](references.md#brentzimmermannmca2010)). It is described in [Barrett1987](references.md#barrett1987), as [BrentZimmermannMCA2010, § 2.9](references.md#brentzimmermannmca2010) records.

## Proof

**Lemma (2 modulo an odd prime).** Let $q$ be an odd prime. Then $2^{(q-1)/2} \equiv 1 \pmod q$ if $q \equiv \pm 1 \pmod 8$, and $2^{(q-1)/2} \equiv -1 \pmod q$ if $q \equiv \pm 3 \pmod 8$.

*Proof.* Since $q$ is odd, $q^2 - 1 = (q - 1)(q + 1)$ is a product of two consecutive even numbers, one of them divisible by 4, so $8 \mid q^2 - 1$. The multiplicative group of the field $\mathbb{F}_{q^2}$ is cyclic of order $q^2 - 1$, so it holds an element $\zeta$ of order 8. Then $\zeta^4$ has order 2, and the only element of order 2 in a field of odd characteristic is $-1$, so $\zeta^4 = -1$. Put $\tau = \zeta + \zeta^{-1}$. Then

$$\tau^2 = \zeta^2 + 2 + \zeta^{-2} = 2 + \zeta^{-2} (\zeta^4 + 1) = 2.$$

In characteristic $q$, $(u + v)^q = u^q + v^q$, so $\tau^q = \zeta^q + \zeta^{-q}$, which depends only on $q \bmod 8$. If $q \equiv \pm 1 \pmod 8$, then $\tau^q = \zeta + \zeta^{-1} = \tau$. If $q \equiv \pm 3 \pmod 8$, then $\tau^q = \zeta^3 + \zeta^{-3}$, and since $\zeta^3 = \zeta^4 \zeta^{-1} = -\zeta^{-1}$ and $\zeta^{-3} = \zeta^{-4} \zeta = -\zeta$, $\tau^q = -\tau$. Now $\tau \ne 0$, because $\tau^2 = 2$ and $q$ is odd, so dividing by $\tau$ gives $\tau^{q-1} = 1$ or $-1$ in the two cases. Finally $\tau^{q-1} = (\tau^2)^{(q-1)/2} = 2^{(q-1)/2}$, an element of $\mathbb{F}_q$, where $1 \ne -1$. $\square$

The lemma is the second supplement to the law of quadratic reciprocity, in the form of Euler's criterion; quadratic residues and the Legendre symbol are the subject of [CrandallPomerance2005, § 2.3.1](references.md#crandallpomerance2005). It is proved here so that the theorem rests on nothing but Fermat's little theorem and the arithmetic of finite fields.

**The theorem.** $M_p$ is odd, so $q$ is odd. Since $q \mid 2^p - 1$, $2^p \equiv 1 \pmod q$, and the multiplicative order $d$ of 2 modulo $q$ divides $p$. As $p$ is prime, $d = 1$ or $d = p$, and $d = 1$ would mean $q \mid 2 - 1$, which is impossible. So $d = p$. By Fermat's little theorem $2^{q-1} \equiv 1 \pmod q$, so $d = p$ divides $q - 1$. Also $q - 1$ is even and $p$ is odd, so $2p \mid q - 1$, and $q = 2kp + 1$ with $k \ge 1$, because $q > 1$. This proves part 1.

For part 2, $p + 1$ is even, so with $x = 2^{(p+1)/2}$,

$$x^2 = 2^{p+1} = 2 \cdot 2^p \equiv 2 \pmod q.$$

$q$ does not divide $x$, a power of 2, so Fermat's little theorem gives $2^{(q-1)/2} \equiv x^{q-1} \equiv 1 \pmod q$. By the lemma, $q \equiv \pm 1 \pmod 8$. $\square$

**The corollary.** A divisor $d > 1$ of $M_p$ is a product of prime factors of $M_p$, each $\equiv 1 \pmod{2p}$ and $\equiv \pm 1 \pmod 8$ by the theorem, and both sets of residues are closed under multiplication. For the classes of $k$: $p$ is odd, so $p^2 \equiv 1 \pmod 4$. Then $q \equiv 1 \pmod 8$ exactly when $8 \mid 2kp$, that is, $4 \mid k$; and $q \equiv 7 \pmod 8$ exactly when $2kp \equiv 6 \pmod 8$, that is, $kp \equiv 3 \pmod 4$, or $k \equiv 3p \pmod 4$ after multiplying by $p$. $\square$

**The classes.** For an odd prime $r$ not dividing $2p$, $2p$ is invertible modulo $r$, and $r \mid 2kp + 1$ exactly when $k \equiv -(2p)^{-1} \pmod r$: one class of $k$ modulo $r$. The moduli $4, 3, 5, 7, 11$ are pairwise coprime, so by the Chinese remainder theorem the classes modulo their product are the combinations of classes modulo each, and the surviving count is the product of the surviving counts, for every $p$ that is none of 3, 5, 7 and 11.

**The ladder.** Let $n_j$ be the number formed by the leading $j$ bits of $p$. Each step maps $x \equiv 2^{n_j}$ to $x^2 \equiv 2^{2 n_j}$, and then to $2^{2 n_j + 1}$ when the next bit is 1, which is $2^{n_{j+1}}$ in both cases. After the last bit, $x \equiv 2^p \pmod q$. $\square$

## What mps uses

Today trial factoring is a stub that decides nothing. `test_quick_factor` in `src/math.cpp` logs that it is running and returns a no-verdict result, `Result::no_verdict`, saying trial factoring is not implemented yet. `-t tf` runs it alone, and `test_all` runs it first, before `test_p_minus_one` and `test_lucas_lehmer`, through `run_in_order`. It is separate from the exponent check, `check_exponent`, which runs before any test and finds the factors that a composite exponent gives ([the Mersenne-numbers note](mersenne-numbers.md)); trial factoring is for a prime $p$.

The planned test will search the candidates $q = 2kp + 1$ below a chosen bound, class by class, keeping only the classes the theorem and the small primes allow, and sieving each class before any power is computed. It will test each survivor with the left-to-right ladder for $2^p \bmod q$, in exact multi-word arithmetic with Montgomery's or Barrett's reduction, and it will report a factor only after an exact re-check that it divides $M_p$.

## Sources

- [Cole1903, p. 136](references.md#cole1903): the form of factors, already well known to Cole, used to sift candidates for $M_{67}$.
- [Cole1903, p. 137](references.md#cole1903): the factorization of $M_{67}$.
- [CrandallPomerance2005, § 2.3.1](references.md#crandallpomerance2005): quadratic residues and the Legendre symbol.
- [CrandallPomerance2005, § 3.2](references.md#crandallpomerance2005): sieving.
- [CrandallPomerance2005, § 9.3.1](references.md#crandallpomerance2005): binary ladders for powering.
- [CrandallPomerance2005, § 9.2.1](references.md#crandallpomerance2005): Montgomery's method.
- [Montgomery1985, the abstract](references.md#montgomery1985): multiplication modulo $N$ without division by $N$.
- [Barrett1987](references.md#barrett1987): Barrett's reduction.
- [BrentZimmermannMCA2010, § 2.4.1](references.md#brentzimmermannmca2010): Barrett's algorithm.
- [BrentZimmermannMCA2010, § 2.4.2](references.md#brentzimmermannmca2010): Montgomery's multiplication.
- [BrentZimmermannMCA2010, § 2.9](references.md#brentzimmermannmca2010): where each of the two methods was first described.
- [GIMPSMath](references.md#gimpsmath): the form of factors, the powering example, the sieve, and how deep GIMPS factors.

## Provenance

The theorem, the lemma, the corollary, the count of classes and the ladder's correctness are proved in full above, from Fermat's little theorem and the arithmetic of finite fields. The form of factors is classical ([Cole1903, p. 136](references.md#cole1903)), and the published factor is [Cole1903, p. 137](references.md#cole1903). The planned search will be built from these, from the ladder of [CrandallPomerance2005, § 9.3.1](references.md#crandallpomerance2005) and the sieve of [CrandallPomerance2005, § 3.2](references.md#crandallpomerance2005), from the reductions of [Montgomery1985](references.md#montgomery1985) and [Barrett1987](references.md#barrett1987) as [BrentZimmermannMCA2010, § 2.4.1](references.md#brentzimmermannmca2010) and [BrentZimmermannMCA2010, § 2.4.2](references.md#brentzimmermannmca2010) present them, and from the strategy [GIMPSMath](references.md#gimpsmath) documents, and from nothing drawn from another program.
