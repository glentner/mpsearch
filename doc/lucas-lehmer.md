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
- **$p$ prime.** For an odd composite $p$, $M_p$ is composite (see [the Mersenne-numbers note](mersenne-numbers.md)), and the proof of sufficiency below shows that $M_p$ then never divides $s_{p-2}$. So the equivalence holds for every odd $p \ge 3$. `mps` applies the test only to prime exponents: after its exponent check, or, when `-s` skips that, by refusing a composite one.

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
- **0 is a failed check.** For a prime $M_p$ it cannot occur at $i \le p - 2$. For a composite $M_p$ it can be genuine only when some $s_k$ with $k \le i - 2$ shares a prime factor with $M_p$, which the sweep above never finds at the prime exponents it covers. `mps` treats 0 as a failed error check, like $+1$, and takes no factor from it: a factor would need $\gcd(r - 2^{k+1}, M_p)$ and the exact re-check of [AGENTS.md, § Invariants 1](../AGENTS.md), which `issues/large-gcd-and-jacobi.md` holds. A genuine 0 would therefore end a run in an error, never in a wrong verdict.
- **$-1$ is consistent, not proof.** Suppose an error replaces $s_i$ by some $s_i'$ and the run then continues correctly. The product identity holds from the next step on, so every later symbol is $J(s_i'^2 - 4 \mid M_p)$, unless a later value shares a factor with $M_p$. If the erroneous value behaves as a random residue, that symbol is $-1$ about half the time, as exactly half the nonzero residues modulo a prime are not squares (the proof of Lemma 1). So a check catches such an error about half the time, and checking more often catches it sooner, not more often. What remains is caught by comparing the final residue with a second run at a different shift, as GIMPS does for its double checks [GIMPSMath](references.md#gimpsmath). `mps` does not make that run itself (`issues/known-primes-campaign.md`).

### Degenerate states and the prime confirmation (the project's own derivation)

**Degenerate states.** The map $x \mapsto x^2 - 2$ fixes 2 and $-1$, sends 1 to $-1$ and $-2$ to 2, and sends 0 to $-2$. So a run that holds one of $0, \pm 1, \pm 2$ stays among them and, two steps later at most, holds 2 or $-1$ for good: a buffer zeroed before the end ends at $-2$ or 2, while one zeroed at the end fakes a zero residue. For a prime $M_p$ none of them is a genuine $s_i$ with $i < p - 2$: from $\pm 1$ or $\pm 2$ the final residue would be $-1$ or 2, not the 0 the theorem gives, and an earlier 0 is ruled out by the consequence above. For a composite $M_p$ this argument does not rule them out, and a run that met one would end in an error, never in a wrong verdict.

**What the Jacobi check misses.** With $M = M_p$ for an odd $p \ge 3$, $J(2 \mid M) = 1$ and $J(3 \mid M) = -1$ (the invariant's derivation), and $J(-1 \mid M) = -1$: by Lemma 1, $J(-1 \mid q)$ is $-1$ exactly for the prime factors $q \equiv 3 \pmod 4$, and since $M \equiv 3 \pmod 4$ an odd number of its prime factors, counted with multiplicity, are $3 \bmod 4$. So on the symbol of $s - 2$:

- $s = 0$ gives $J(-2 \mid M) = J(-1 \mid M) J(2 \mid M) = -1$, and passes;
- $s = 1$ gives $J(-1 \mid M) = -1$, and passes;
- $s = -2$ gives $J(-4 \mid M) = J(-1 \mid M) J(2 \mid M)^2 = -1$, and passes;
- $s = 2$ gives $J(0 \mid M) = 0$, and fails;
- $s = -1$ gives $J(-3 \mid M) = J(-1 \mid M) J(3 \mid M) = 1$, and fails.

<!-- check: mersenne(7) == 127 and jacobi(-2, mersenne(7)) == -1 and jacobi(-1, mersenne(7)) == -1 and jacobi(-4, mersenne(7)) == -1 and jacobi(0, mersenne(7)) == 0 and jacobi(-3, mersenne(7)) == 1 -->For $p = 7$, the symbols of $-2$, $-1$, $-4$, 0 and $-3$ modulo $M_7 = 127$ are $-1$, $-1$, $-1$, 0 and 1<!-- /check -->. The Jacobi check alone passes the states 0, 1 and $-2$, so a run needs a check of its own for them.

**The prime confirmation.** If $s_{p-2} \equiv 0$, then $s_{p-3}^2 \equiv 2 \pmod{M_p}$, and $2 \equiv 2^{p+1} = (2^{(p+1)/2})^2$. A zero residue proves $M_p$ prime (sufficiency), and in the field $\mathbb{F}_{M_p}$ the only square roots of $c^2$ are $\pm c$, so $s_{p-3} \equiv \pm 2^{(p+1)/2} \pmod{M_p}$. For a composite $M_p$ no $s_{p-3}$ is $\pm 2^{(p+1)/2}$, since its square less 2 would be 0 and make $M_p$ prime; so a zero residue that a fault made, after a genuine $s_{p-3}$ of a composite $M_p$, always fails the confirmation. A run at shift $k$ holds $r \equiv \pm 2^{(k + (p+1)/2) \bmod p}$, or compares after undoing the shift. <!-- check: lucas_seq(7, 4) == 111 and mersenne(7) - 2**4 == 111 and (7 + 1) // 2 == 4 -->For $p = 7$, $s_4 \equiv 111 = 127 - 2^4$, with $4 = (7 + 1)/2$<!-- /check -->.

### The cost of the check

The Jacobi symbol of two $n$-bit integers can be computed in time $O(M(n) \log n)$, where $M(n)$ is the cost of multiplying two $n$-bit integers ([BrentZimmermann2010, the abstract](references.md#brentzimmermann2010)). A symbol on a $p$-bit residue thus costs about a logarithmic factor more than one squaring, against the $p - 2$ squarings of the test, so a run can check it at intervals and at the end. The GCD that would take a factor from a zero symbol can also be computed in subquadratic time [Moller2008](references.md#moller2008); `mps` takes no factor from it (`issues/large-gcd-and-jacobi.md`).

## What mps uses

**The entry.** `-t ll` runs `test_lucas_lehmer` in `src/math.cpp` alone after the exponent check (which `-s` skips), and `test_all` runs it last, after `test_quick_factor` and `test_p_minus_one`, through `run_in_order`, which stops at the first decisive result and ends the run at an error: later tests neither run nor print, and the run exits 20. `test_lucas_lehmer` calls `lucas::test` in `src/lucas.cpp` with no shift given, so it chooses one. Before any squaring the test refuses three cases with no verdict, exit 42, and a reason: $p$ even or below 3, since the theorem is for odd $p$ and $p = 2$ has no iteration; $p$ past P_max, the largest exponent the length table holds (`src/lengths.cpp`), which the reason names; and an odd composite $p$, reachable only with `-s`, since the test runs on prime exponents. A $p$ that is both past P_max and composite gets both reasons. Otherwise the test runs on the FP64 engine of [the weighted-transforms note](weighted-transforms.md) (`src/ibdwt.cpp`) at the shortest supported length that holds $p$.

**The shift.** Each run draws its shift $k_0$ uniformly from $0 \le k_0 < p$, from the system's entropy (`std::random_device`), so it differs between runs, and reports it as its `shift`. The run holds $r = 2^k s_i \bmod M_p$, starting from $2^{k_0} \cdot 4$; each step is one `square_subtract`, which squares and subtracts $2^{(2k+1) \bmod p}$, and then sets $k \leftarrow 2k \bmod p$, as "The shift" above derives. Every comparison, check and Res64 is taken on the unshifted residue $s = 2^{(p - k) \bmod p} r \bmod M_p$, a rotation of the $p$ bits. The checks give a shift through `lucas::Options`; `mps` has no shift option (`issues/burn-in-mode.md`).

**The checks and their schedule.** The test checks every residue whose iteration is a multiple of $\lceil (p - 2) / G \rceil$, with $G$ = `GOOD_STATES` (32), and the final residue $s_{p-2}$, so a run checks about `GOOD_STATES` (32) residues. At each, in this order:

1. the degenerate check, on the unshifted $s$: it fails if $s$ is $1$, $-1$, $2$ or $-2$ modulo $M_p$, or 0 before the last iteration;
2. the Jacobi check, on a residue the degenerate check passed: $J(r - 2^{(k+1) \bmod p} \mid M_p)$ by `jacobi` in `src/mp.cpp` ([the reference-arithmetic note](reference-arithmetic.md)), which "The shift" shows equals $J(s - 2 \mid M_p)$; $-1$ passes, and $+1$ and 0 fail;
3. at the last iteration, when $s$ is 0, the prime confirmation: the test keeps $s_{p-3}$, unshifted, and it must be $\pm 2^{(p+1)/2}$.

The Jacobi check alone passes 0, 1 and $-2$ (derived above), so the degenerate check runs first and catches them, and the confirmation catches a zero made at the last iteration. A residue that passes is the run's new good state: the residue as held, its iteration and its shift, kept in memory and nowhere else (the checkpoint file is `issues/checkpoint-and-resume.md`). Each Jacobi symbol costs $O(p^2)$ bit operations by the binary algorithm, so the run's checks, a fixed number of them, grow no faster with $p$ than its $p - 2$ squarings do; a passing check catches a random error only about half the time ("What the symbol means for a run"), and the next check has the same chance again.

**Recovery.** A failed check sends the run back to its last good state, $s_0$ at its shift when there is none yet, and it redoes the iterations after it at the length it is running at. A run recovers from `CHECK_FAILURES` (3) failed checks; one more ends it in an error that names the check, the iteration and the count, with no verdict, exit 20.

**Refusals.** The engine refuses a squaring with `roundoff_error` and leaves the residue as it was. The test replays that squaring once, from that residue at the same length; if the replay is accepted, the run goes on and counts one transient fault. If the replay is refused too, the run returns to its last good state and goes on at the next larger supported length that holds $p$, on a new engine. A run moves to a larger length at most `LENGTH_RETRIES` (3) times: a refused replay with no larger length left, or past that limit, ends it in an error that names $p$, the length and the distance, with no verdict, exit 20. Any other error of the engine ends the run in an error at once, never replayed or retried.

**Round-off distances.** An accepted squaring whose round-off distance exceeds 0.35, the length table's sizing distance (`ibdwt::SIZING_DISTANCE`), is counted; the first `DISTANCES_LOGGED` (8) of a run are logged one by one with the iteration, the length and the distance, and the rest in one line at the end. The result reports the count and the largest distance of the run.

**Progress.** Every `PROGRESS_SECONDS` (30.0) seconds the test logs the iteration, the time per iteration, the estimated time to finish and the largest round-off distance since the last progress line.

**The result.** The test reports prime only when the residue of a complete run is 0 and every check passed, composite with the Res64 of a nonzero final residue only from such a run, and otherwise an error. Its verdict line and its JSON record (`RECORD_VERSION`, now 2, in `src/result.hpp`) name the test, the exponent, each length the run ran at, the shift $k_0$, the Res64 for a verdict, the Jacobi checks run and failed, the degenerate-state and prime-confirmation failures, the transient faults, the length retries, the count of distances past 0.35 and the largest distance, and the program version; an error carries the same counts, as far as the run got. The exit statuses are 0 for prime, 40 for composite, 20 for an error and 42 for no verdict.

**What checks it.** `make check` runs `tests/ll.sh`, which runs `bin/mps -t ll` on every odd exponent of `tests/known/mersenne-exponents.txt` and every `ll` row of `tests/known/final-residues.txt` up to the bounds its header states. `tests/check_lucas.cpp` runs `lucas::test` on the known answers at two given shifts, 0 among them, and proves each check, each recovery and each length retry by injection, through hooks in `lucas::Options` that corrupt a residue, refuse a squaring or report a distance; `bin/mps` never sets them. `tests/vectors/jacobi.txt` holds the vectors of `jacobi`.

**The reference test.** `reference::lucas_lehmer` in `src/reference.cpp` computes $s_{p-2} \bmod M_p$ from $s_0 = 4$ for odd $p \ge 3$ on the reference arithmetic of [the reference-arithmetic note](reference-arithmetic.md), squaring and folding modulo $M_p$ each step, and returns the residue, in $[0, M_p)$; any other $p$ is an error. `make check` runs it on the published Mersenne prime exponents, where it must end in 0, and on prime exponents whose $M_p$ is composite, against residues Python's integers computed. The test above never calls it and never reads `tests/known/`: its answer comes from its own run, and the checks compare that with the tables.

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
- [ShallitSorenson1993](references.md#shallitsorenson1993): the binary algorithm for the Jacobi symbol the run checks, written up in [the reference-arithmetic note](reference-arithmetic.md).

## Provenance

The theorem is [Lehmer1930, Theorem 5.4, p. 443](references.md#lehmer1930), and its proof here is written from the definitions, with Euler's criterion proved and reciprocity taken from [BrentZimmermann2010, § 1](references.md#brentzimmermann2010). The Jacobi invariant, the shifted symbol and what each value means for a run are the project's own derivation, from the same laws. The shifted start follows the practice [GIMPSMath](references.md#gimpsmath) documents, and the cost of the check is [BrentZimmermann2010, the abstract](references.md#brentzimmermann2010). The degenerate states and the prime confirmation are the project's own derivation, from the theorem and Lemma 1. The test in `src/lucas.cpp` is built from these papers and from the notes, this one, [the weighted-transforms note](weighted-transforms.md) and [the reference-arithmetic note](reference-arithmetic.md); its schedule, its limits and its recovery are the project's own design, and nothing in it is drawn from another program.
