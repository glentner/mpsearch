# The Lucas–Lehmer test

## The result

For a positive integer $p$, let $M_p = 2^p - 1$, and define the integers $s_i$ by

$$s_0 = 4, \qquad s_{i+1} = s_i^2 - 2 \quad (i \ge 0).$$

<!-- check: 4**2 - 2 == 14 and 14**2 - 2 == 194 and 194**2 - 2 == 37634 --> Squaring and subtracting 2 each time, the sequence begins $4, 14, 194, 37634$<!-- /check -->, and it grows doubly exponentially.

**Theorem (Lucas–Lehmer).** Let $p$ be an odd prime. Then $M_p$ is prime if and only if $M_p$ divides $s_{p-2}$.

This is [Lehmer1930, Theorem 5.4, p. 443](references.md#lehmer1930), which states it for $N = 2^n - 1$, $n$ an odd prime, as the divisibility of the $(n-1)$-st term of the series that begins with $s_0$. [CrandallPomerance2005, § 4.2.1](references.md#crandallpomerance2005) presents it as the Lucas–Lehmer test.

**The test.** Divisibility by $M_p$ depends only on residues, so the test computes $s_{p-2} \bmod M_p$: it starts from 4 and repeats $p - 2$ times "square, subtract 2, reduce modulo $M_p$". $M_p$ is prime exactly when the final residue is 0. <!-- check: mersenne(7) == 127 and lucas_seq(7, 0) == 4 and lucas_seq(7, 1) == 14 and lucas_seq(7, 2) == 67 and lucas_seq(7, 3) == 42 and lucas_seq(7, 4) == 111 and lucas_seq(7, 5) == 0 and 7 - 2 == 5 --> For $p = 7$, $M_7 = 127$, and modulo 127 the residues of $s_i$ for $i = 0, 1, 2, 3, 4, 5$ are $4, 14, 67, 42, 111, 0$: the last, with $5 = 7 - 2$, is zero, so $M_7$ is prime<!-- /check -->. <!-- check: mersenne(11) == 2047 and 2047 == 23 * 89 and ll_residue(11) == 1736 --> For $p = 11$, $M_{11} = 2047 = 23 \cdot 89$, and the final residue is $s_9 \equiv 1736 \pmod{2047}$, not zero<!-- /check -->.

**The hypotheses.**

- **$p$ odd.** <!-- check: mersenne(2) == 3 and isprime(3) and 4 % 3 == 1 --> For $p = 2$, $M_2 = 3$ is prime, but $s_0 = 4 \equiv 1 \pmod 3$ is not divisible by 3<!-- /check -->. The theorem excludes $p = 2$, and $M_2$ is prime by inspection.
- **$p$ prime.** For an odd composite $p$, $M_p$ is composite (see [the Mersenne-numbers note](mersenne-numbers.md)), and the proof of sufficiency below shows that $M_p$ then never divides $s_{p-2}$. So the equivalence holds for every odd $p \ge 3$. `mps` applies the test only to prime exponents, after its exponent check.

**The published list.** <!-- check: ll_exponents(2500) == [2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281] and len(ll_exponents(2500)) == 17 and len(primes(2500)) == 367 and 367 - 17 == 350 --> Run on every prime $p \le 2500$, with $M_2 = 3$ taken by inspection, the test finds $M_p$ prime for exactly the 17 exponents $2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281$, which are the terms of [OEIS-A000043, the data section](references.md#oeis-a000043) up to 2500. For the other 350 of the 367 primes up to 2500 the residue is nonzero, and each of those $M_p$ also fails a base-3 Fermat test<!-- /check -->.

## Proof

The proof below is complete. [Lehmer1930, Theorem 5.4, p. 443](references.md#lehmer1930) gives the original proof of both directions, the necessity and the sufficiency, from Lehmer's theory of Lucas functions. Shorter published proofs are [Lehmer1935](references.md#lehmer1935) and [Bruce1993](references.md#bruce1993).

**Notation.** For an odd prime $q$ and an integer $a$, $J(a \mid q)$ is the Legendre symbol: 0 if $q \mid a$, 1 if $a$ is a nonzero square modulo $q$, and $-1$ otherwise. For an odd $n > 0$ with prime factors $q_1 q_2 \cdots q_m = n$, repeated as often as they divide $n$, the Jacobi symbol is $J(a \mid n) = \prod_j J(a \mid q_j)$ ([CrandallPomerance2005, § 2.3.1](references.md#crandallpomerance2005)). From the definition, $J(a \mid n)$ depends only on $a \bmod n$, is multiplicative in $a$, and is 0 exactly when $\gcd(a, n) > 1$; otherwise it is $\pm 1$. The law of reciprocity holds: for odd positive $a$ and $b$, $J(b \mid a) = (-1)^{(a-1)(b-1)/4} J(a \mid b)$ ([BrentZimmermann2010, § 1](references.md#brentzimmermann2010)).

**Lemma 1 (Euler's criterion).** For an odd prime $q$ and $a \not\equiv 0 \pmod q$, $a^{(q-1)/2} \equiv J(a \mid q) \pmod q$.

*Proof.* The integers modulo $q$ form a field $\mathbb{F}_q$. Put $c = a^{(q-1)/2}$; then $c^2 = a^{q-1} = 1$ by Fermat's little theorem, so $c = \pm 1$. If $a = b^2$, then $c = b^{q-1} = 1$. Each nonzero square has exactly two square roots, $b$ and $-b$, so there are $(q-1)/2$ nonzero squares, all of them roots of $x^{(q-1)/2} - 1$, which has no more roots than its degree, so for a non-square $a$, $c \ne 1$ and $c = -1$. $\square$

**Lemma 2.** Let $\omega = 2 + \sqrt 3$ and $\bar\omega = 2 - \sqrt 3$, so that $\omega \bar\omega = 1$. Then $s_i = \omega^{2^i} + \bar\omega^{2^i}$ for every $i \ge 0$.

*Proof.* For $i = 0$, $\omega + \bar\omega = 4 = s_0$. If it holds for $i$, then

$$s_i^2 - 2 = \omega^{2^{i+1}} + 2 (\omega \bar\omega)^{2^i} + \bar\omega^{2^{i+1}} - 2 = \omega^{2^{i+1}} + \bar\omega^{2^{i+1}}. \qquad \square$$

Both directions work in a ring of the form $R_q = \mathbb{F}_q[x] / (x^2 - 3)$ for an odd prime $q$, writing $\sqrt 3$ for the class of $x$. Its elements are $a + b \sqrt 3$ with $a, b \in \mathbb{F}_q$, so it has $q^2$ elements; $\omega \bar\omega = 1$ holds in it, and the integer $s_i$ maps to $\omega^{2^i} + \bar\omega^{2^i}$.

**Sufficiency: if $M_p \mid s_{p-2}$ and $p \ge 2$, then $M_p$ is prime.** Suppose $M_p \mid s_{p-2}$ but $M_p$ is composite. Let $q$ be its least prime factor; $q$ is odd and $q^2 \le M_p$. In $R_q$, $\omega^{2^{p-2}} + \bar\omega^{2^{p-2}} = 0$. Multiplying by $\omega^{2^{p-2}}$ and using $\omega \bar\omega = 1$ gives $\omega^{2^{p-1}} = -1$, so $\omega^{2^p} = 1$. Thus $\omega$ is a unit of $R_q$, and its order divides $2^p$ but not $2^{p-1}$, since $-1 \ne 1$ in $R_q$ for odd $q$: the order is exactly $2^p$. The units of $R_q$ exclude 0, so there are at most $q^2 - 1$ of them, and $2^p \le q^2 - 1$. Then $q^2 > 2^p - 1 = M_p$, a contradiction. (For $p = 2$ the hypothesis fails, since no odd prime divides $s_0 = 4$, so the statement holds trivially.)

**Necessity: if $p \ge 3$ is odd and $q = M_p$ is prime, then $q \mid s_{p-2}$.**

- *The residues of $q$.* Since $p \ge 3$, $2^p \equiv 0 \pmod 8$, so $q \equiv 7 \pmod 8$, and in particular $q \equiv 3 \pmod 4$. Since $p$ is odd, $2^p \equiv (-1)^p = -1 \pmod 3$, so $q \equiv 1 \pmod 3$.
- *2 is a square modulo $q$*: $2^p \equiv 1$, and $p + 1$ is even, so $2 \equiv 2^{p+1} = (2^{(p+1)/2})^2$. By Lemma 1, $2^{(q-1)/2} \equiv 1$.
- *3 is not a square modulo $q$.* By reciprocity, $J(3 \mid q) = (-1)^{(q-1)/2} J(q \mid 3) = -J(1 \mid 3) = -1$, since $(q-1)/2$ is odd. By Lemma 1, $3^{(q-1)/2} \equiv -1$.
- *The field.* Since 3 is not a square, $x^2 - 3$ is irreducible over $\mathbb{F}_q$, and $R_q$ is a field. In characteristic $q$, $z \mapsto z^q$ is a ring homomorphism (the binomial theorem), it fixes $\mathbb{F}_q$ (Fermat), and $\sqrt 3^{\,q} = 3^{(q-1)/2} \sqrt 3 = -\sqrt 3$. So $(a + b\sqrt 3)^q = a - b \sqrt 3$.
- *The computation.* Let $\tau = 1 + \sqrt 3$. Then $\tau^2 = 4 + 2\sqrt 3 = 2\omega$, and $\tau^{q+1} = \tau \cdot \tau^q = (1 + \sqrt 3)(1 - \sqrt 3) = -2$. Hence, with $(q+1)/2 = 2^{p-1}$,

$$\omega^{2^{p-1}} = \omega^{(q+1)/2} = \frac{\tau^{q+1}}{2^{(q+1)/2}} = \frac{-2}{2 \cdot 2^{(q-1)/2}} = -1.$$

- *The conclusion.* Then $\omega^{2^{p-2}} \bigl(\omega^{2^{p-2}} + \bar\omega^{2^{p-2}}\bigr) = \omega^{2^{p-1}} + 1 = 0$, and $\omega$ is invertible, so $s_{p-2} = 0$ in $R_q$. The integer $s_{p-2}$ maps into the copy of $\mathbb{F}_q$ inside $R_q$, and to 0 there, so $q \mid s_{p-2}$. $\square$

**A consequence used below.** If $q$ is any odd prime and $q \mid s_k$, then $\omega^{2^{k+1}} = -1$ in $R_q$, by the sufficiency argument with $k$ in place of $p - 2$, so $\omega$ has order exactly $2^{k+2}$ there. For a prime $q = M_p$, the order of $\omega$ is $2^p$ (from the necessity argument, $\omega^{2^{p-1}} = -1$), so $q \mid s_k$ forces $k = p - 2$: no earlier $s_k$ is divisible by a prime $M_p$.

### The Jacobi invariant (the project's own derivation)

No published statement of this invariant was found, so it is derived here in full.

**Statement.** Let $p \ge 3$ be odd and $M = M_p$. For every $i \ge 1$, $J(s_i - 2 \mid M)$ is $-1$ if no $s_k$ with $0 \le k \le i - 2$ shares a prime factor with $M$, and 0 otherwise. It is never $+1$. When $M_p$ is prime it is $-1$ for every $1 \le i \le p - 2$. A run that stores $r = 2^k s_i \bmod M$ computes $J(r - 2^{k+1} \mid M)$, which equals $J(s_i - 2 \mid M)$.

**The product.** For $k \ge 1$, $s_k + 2 = s_{k-1}^2$, so

$$s_{k+1} - 2 = s_k^2 - 4 = (s_k - 2)(s_k + 2) = (s_k - 2) \, s_{k-1}^2.$$

Since <!-- check: 14 - 2 == 12 --> $s_1 - 2 = 14 - 2 = 12$<!-- /check -->, induction on $i$ gives, as integers,

$$s_i - 2 = 12 \prod_{k=0}^{i-2} s_k^2 \qquad (i \ge 1),$$

the product being empty for $i = 1$. <!-- check: 194 - 2 == 12 * 4**2 and 37634 - 2 == 12 * 4**2 * 14**2 --> For example, $s_2 - 2 = 194 - 2 = 192 = 12 \cdot 4^2$ and $s_3 - 2 = 37634 - 2 = 37632 = 12 \cdot 4^2 \cdot 14^2$<!-- /check -->.

**The residues of $M$.** As in the necessity proof, but without assuming $M$ prime: $p \ge 3$ gives $M \equiv 7 \pmod 8$, and $p$ odd gives $M \equiv 1 \pmod 3$, so $M \equiv 7 \pmod{24}$. In particular $\gcd(12, M) = 1$.

**The symbols of 2, 3 and 12.**

- $J(2 \mid M) = 1$: $2 \equiv (2^{(p+1)/2})^2 \pmod M$, so 2 is a square modulo every prime factor $q_j$ of $M$, and each $J(2 \mid q_j) = 1$.
- $J(3 \mid M) = -1$: by reciprocity, $J(3 \mid M) = (-1)^{(M-1)/2} J(M \mid 3) = -J(1 \mid 3) = -1$, since $(M-1)/2 = 2^{p-1} - 1$ is odd.
- So $J(12 \mid M) = J(2 \mid M)^2 \, J(3 \mid M) = -1$.

**The invariant.** By multiplicativity,

$$J(s_i - 2 \mid M) = J(12 \mid M) \prod_{k=0}^{i-2} J(s_k \mid M)^2 = -\prod_{k=0}^{i-2} J(s_k \mid M)^2.$$

Each factor $J(s_k \mid M)^2$ is 1 when $\gcd(s_k, M) = 1$ and 0 otherwise. So the symbol is $-1$ when no $s_k$ with $k \le i - 2$ shares a prime factor with $M$, and 0 otherwise; it is never $+1$. Only residues enter, so the same holds for the residues $s_k \bmod M$ a run computes. When $M$ is prime, no $s_k$ with $k < p - 2$ is divisible by $M$ (the consequence above), so for $1 \le i \le p - 2$ every $k \le i - 2$ qualifies and the symbol is $-1$.

**The shift.** A shifted run stores $r_i = 2^{k_i} s_i \bmod M$ for a shift $k_i$, and since $2^p \equiv 1 \pmod M$, shifts are taken modulo $p$. One step computes $r_i^2 - 2^{2k_i + 1} = 2^{2k_i}(s_i^2 - 2) = 2^{2k_i} s_{i+1}$, so it subtracts the single power $2^{(2k_i + 1) \bmod p}$ and sets $k_{i+1} = 2k_i \bmod p$. The run cannot form $s_i - 2$ without undoing the shift; what it can form is

$$r_i - 2^{(k_i + 1) \bmod p} \equiv 2^{k_i} (s_i - 2) \pmod M,$$

and $J(r_i - 2^{k_i + 1} \mid M) = J(2 \mid M)^{k_i} J(s_i - 2 \mid M) = J(s_i - 2 \mid M)$. The symbol $J(r_i - 2 \mid M)$ is a different quantity and is not $-1$ in general.

**Hypotheses.** The derivation needs exactly two things. First, $p$ odd and $p \ge 3$: that gives $M \equiv 7 \pmod 8$ and $M \equiv 1 \pmod 3$, hence $J(2 \mid M) = 1$ and $J(3 \mid M) = -1$; primality of $p$ is not needed. Second, the common-factor condition: the value $-1$ at iteration $i$ requires that no $s_j$ with $j \le i - 2$ shares a prime factor with $M_p$. For a composite $M_p$, once a prime factor $q$ of $M_p$ divides some $s_j$, the symbol is 0 for every $i \ge j + 2$, and stays 0. For a prime $M_p$ the condition holds at every $i \le p - 2$.

The zero case is real. If $p$ is odd and composite with an odd prime factor $a$ for which $M_a$ is prime, then $M_a \mid M_p$, and $M_a \mid s_{a-2}$ by the theorem, so the symbol is 0 from $i = a$ on, or earlier. <!-- check: mersenne(3) == 7 and mersenne(9) % 7 == 0 and mersenne(15) % 7 == 0 and mersenne(21) % 7 == 0 and 14 % 7 == 0 and mersenne(5) == 31 and mersenne(25) % 31 == 0 and 37634 % 31 == 0 --> For $p = 9, 15, 21$, the factor $M_3 = 7$ of $M_p$ divides $s_1 = 14$, so the symbol is 0 from $i = 3$ on; for $p = 25$, the factor $M_5 = 31$ divides $s_3 = 37634$, so it is 0 from $i = 5$ on<!-- /check -->. For prime $p$, a zero at some $i \le p - 2$ needs a prime factor $q$ of a composite $M_p$ in whose ring $R_q$ the element $\omega$ has order $2^{j+2}$ for some $j \le p - 4$, by the consequence above.

**The checks.** Each case was computed directly, symbol by symbol, unshifted and in three shifted runs that start from the shifts $k_0 = 1$, $(p-1)/2$ and $p - 1$, and each case not equal to $-1$ was tested against the hypothesis both ways. <!-- check: jacobi_sweep(521) == [] --> The sweep over every odd prime $p \le 521$ and every iteration finds no exception<!-- /check -->: every symbol is $-1$, and the shifted runs agree with the unshifted one. <!-- check: jacobi_first_zero(25) == [(9, 3), (15, 3), (21, 3), (25, 5)] --> Over every odd $p \le 25$, prime or not, the exponents with a zero symbol, each with the first $i$ where it appears, are $(p, i) = (9, 3), (15, 3), (21, 3), (25, 5)$<!-- /check -->; the sweep finds a 0 exactly where the common-factor condition predicts one.

**What the symbol means for a run.** A run checks $J(r - 2^{k+1} \mid M_p)$ on its stored residue $r$ and shift $k$.

- **$+1$ is an arithmetic error, always.** Exact arithmetic never produces it, whether $M_p$ is prime or not. The run goes back to its last state that passed.
- **0 is an error, or a common factor.** For a prime $M_p$ it cannot occur at $i \le p - 2$, so it is an error. For a composite $M_p$ it may be genuine: then $g = \gcd(r - 2^{k+1}, M_p) > 1$. The run reports a factor only if $g$ passes the exact re-check of [AGENTS.md, § Invariants 1](../AGENTS.md), $1 < g < M_p$ and $2^p \equiv 1 \pmod g$ in exact integers. Otherwise it treats the 0 as an error.
- **$-1$ is consistent, not proof.** Suppose an error replaces $s_i$ by some $s_i'$ and the run then continues correctly. The product identity holds from the next step on, so every later symbol is $J(s_i'^2 - 4 \mid M_p)$, unless a later value shares a factor with $M_p$. If the erroneous value behaves as a random residue, that symbol is $-1$ about half the time, as exactly half the nonzero residues modulo a prime are not squares (the proof of Lemma 1). So a check catches such an error about half the time, and checking more often catches it sooner, not more often. What remains is caught by comparing the final residue with a second run at a different shift, as GIMPS does for its double checks [GIMPSMath](references.md#gimpsmath).

### The cost of the check

The Jacobi symbol of two $n$-bit integers can be computed in time $O(M(n) \log n)$, where $M(n)$ is the cost of multiplying two $n$-bit integers ([BrentZimmermann2010, the abstract](references.md#brentzimmermann2010)). A symbol on a $p$-bit residue thus costs about a logarithmic factor more than one squaring, against the $p - 2$ squarings of the test, so a run can check it at intervals and at the end. The GCD that a zero symbol calls for can also be computed in subquadratic time [Moller2008](references.md#moller2008).

## What mps uses

Today the test is a stub that decides nothing. `test_lucas_lehmer` in `src/math.cpp` logs that it is running and returns a no-verdict result, `Result::no_verdict`, saying Lucas–Lehmer is not implemented yet. `-t ll` runs it alone after the exponent check (which `-s` skips), and `test_all` runs it last, after `test_quick_factor` and `test_p_minus_one`, through `run_in_order`.

A reference test sits beside the stub: `reference::lucas_lehmer` in `src/reference.cpp` computes $s_{p-2} \bmod M_p$ from $s_0 = 4$ for odd $p \ge 3$ on the reference arithmetic of [the reference-arithmetic note](reference-arithmetic.md), squaring and folding modulo $M_p$ each step, and returns the residue, in $[0, M_p)$; any other $p$ is an error. `make check` runs it on the published Mersenne prime exponents, where it must end in 0, and on prime exponents whose $M_p$ is composite, against residues Python's integers computed. It is the known answer the planned test will be graded against. It is not wired into `-t`, and `test_lucas_lehmer` is still a stub that decides nothing.

The planned test will use the theorem for odd prime $p$ that pass the exponent check, and $M_2 = 3$ by inspection. It will compute $s_{p-2} \bmod M_p$ from $s_0 = 4$, report prime only when the residue of a complete run is 0, and report composite with the nonzero final residue only from a run whose every check passed. It will run with a per-run shift as derived above: store $2^k s_i$, subtract $2^{(2k+1) \bmod p}$ each step, and double $k$ modulo $p$. It will check $J(r - 2^{k+1} \mid M_p)$ at intervals and at the end, treating $+1$ as an error, 0 as an error unless the GCD gives a factor that passes the exact re-check, and $-1$ as consistent.

## Sources

- [Lehmer1930, Theorem 5.4, p. 443](references.md#lehmer1930): the theorem, and the original proof of both directions.
- [Lehmer1935](references.md#lehmer1935): a later proof of the theorem.
- [Bruce1993](references.md#bruce1993): a short proof of the theorem.
- [CrandallPomerance2005, § 4.2.1](references.md#crandallpomerance2005): the Lucas–Lehmer test.
- [CrandallPomerance2005, § 2.3.1](references.md#crandallpomerance2005): quadratic residues, and the Legendre and Jacobi symbols.
- [BrentZimmermann2010, § 1](references.md#brentzimmermann2010): the identities of the Jacobi symbol, reciprocity among them.
- [BrentZimmermann2010, the abstract](references.md#brentzimmermann2010): the cost of the Jacobi symbol.
- [Moller2008](references.md#moller2008): subquadratic GCD.
- [OEIS-A000043, the data section](references.md#oeis-a000043): the published Mersenne prime exponents.
- [GIMPSMath](references.md#gimpsmath): the test as GIMPS runs it, with a shifted start and double checks of the final residue.

## Provenance

The theorem is [Lehmer1930, Theorem 5.4, p. 443](references.md#lehmer1930), and its proof here is written from the definitions, with Euler's criterion proved and reciprocity taken from [BrentZimmermann2010, § 1](references.md#brentzimmermann2010). The Jacobi invariant, the shifted symbol and what each value means for a run are the project's own derivation, from the same laws. The shifted start follows the practice [GIMPSMath](references.md#gimpsmath) documents, and the cost of the check is [BrentZimmermann2010, the abstract](references.md#brentzimmermann2010). The planned test will be built from these and from nothing drawn from another program.
