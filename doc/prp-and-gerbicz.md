# Fermat probable primes, and the Gerbicz check

## The result

**Theorem 1 (Fermat's little theorem).** If $q$ is prime and $q \nmid a$, then $a^{q-1} \equiv 1 \pmod q$.

**The Fermat test.** Let $N > 1$ be odd and $\gcd(a, N) = 1$. If $a^{N-1} \not\equiv 1 \pmod N$, then $N$ is composite, by Theorem 1. If $a^{N-1} \equiv 1 \pmod N$, then $N$ is a probable prime to base $a$, and nothing is proved: a composite $N$ that passes is a Fermat pseudoprime to base $a$ ([CrandallPomerance2005, § 3.4.1](references.md#crandallpomerance2005)). A "composite" from a correct computation is certain; a "probable prime" is not a proof, and a Mersenne probable prime is confirmed by the Lucas–Lehmer test ([the Lucas–Lehmer note](lucas-lehmer.md)).

**Theorem 2 (the base-3 test of $M_p$).** Let $p \ge 3$ be odd and $N = M_p = 2^p - 1$. Put $x_0 = 3$ and $x_{k+1} = x_k^2 \bmod N$, so that $x_k \equiv 3^{2^k}$. Then $3^{N-1} \equiv 1 \pmod N$ if and only if $x_p \equiv 9 \pmod N$.

So the test is $p$ squarings modulo $M_p$ starting from 3, and one comparison with 9: $M_p$ is a base-3 probable prime when they agree, and composite when they don't. <!-- check: pow(3, 2**7, 127) == 9 and mersenne(7) == 127 and isprime(127) --> For $p = 7$, $3^{2^7} \equiv 9 \pmod{127}$, and $M_7 = 127$ is prime<!-- /check -->. Every published Mersenne prime passes, that is, gives $3^{M_p - 1} \equiv 1 \pmod{M_p}$: <!-- check: prp3_known(11213) == 22 --> all 22 odd exponents $p \le 11213$ in [OEIS-A000043, the data section](references.md#oeis-a000043) do<!-- /check -->. ($M_2 = 3$ is the base itself, and the test says nothing about it.)

**Theorem 3 (base 2 says nothing).** For every prime $p$, $2^{M_p - 1} \equiv 1 \pmod{M_p}$, whether $M_p$ is prime or not. <!-- check: mersenne(11) == 2047 and 2047 == 23 * 89 and pow(2, 2047 - 1, 2047) == 1 and pow(3, 2047 - 1, 2047) == 1013 --> For example, $M_{11} = 2047 = 23 \cdot 89$ is composite, yet $2^{2046} \equiv 1 \pmod{2047}$, while $3^{2046} \equiv 1013 \not\equiv 1$<!-- /check -->. Hence base 3, the smallest base with something to say.

**Theorem 4 (the Gerbicz check).** Keep the notation of Theorem 2, and fix a block length $L \ge 1$. Let

$$d_0 = x_0, \qquad d_{t+1} = d_t \cdot x_{(t+1)L} \bmod N \quad (t \ge 0),$$

the running product of the residues at the ends of the blocks. Then for every $t \ge 0$,

$$d_{t+1} \equiv x_0 \cdot d_t^{\,2^L} \pmod N.$$

A run that keeps the product as it goes can test this identity at any block boundary, at the price of $L$ extra squarings. Let $y_0 = x_0, y_1, y_2, \dots$ be the residues a run actually holds at the ends of the blocks, and $D_t = y_0 y_1 \cdots y_t \bmod N$ the product it keeps. The *check at block $t$* compares $D_{t+1}$ with $x_0 \cdot D_t^{\,2^L}$; block $t$ is the $L$ squarings that lead from $y_t$ to $y_{t+1}$. A run may check every block, or only some of them.

**Theorem 5 (what a check establishes; the project's own).** Let $N > 1$ be odd and $\gcd(x_0, N) = 1$, as holds for $N = M_p$ and $x_0 = 3$ (the proof of Theorem 2 shows $\gcd(9, M_p) = 1$). Suppose the products $D_t$, and each check's own squarings, are computed without error.

1. *Invertible residues.* If $y_0, \dots, y_{t+1}$ are invertible modulo $N$, write $e_j = y_{j+1} \cdot y_j^{-2^L}$ for the error of block $j$, which is 1 exactly when the block squared $y_j$ correctly $L$ times. Then the check at block $t$ passes if and only if $e_0 e_1 \cdots e_t \equiv 1 \pmod N$.
2. *A residue that becomes zero.* If $y_j \equiv 0 \pmod N$ for some $j \ge 1$, then the check at every block $t \ge j$ passes, whatever the residues after $y_j$ are.
3. *The first residue that is not invertible.* Let $q$ be a prime factor of $N$, and $j \ge 1$ the first index with $q \mid y_j$. Then the check at block $j - 1$ fails, and the check at every block $t \ge j$ holds modulo $q$.
4. *The product as a witness.* $D_{t+1}$ is invertible modulo $N$ exactly when $y_0, \dots, y_{t+1}$ all are. So a check at block $t$ that passes with $D_{t+1}$ invertible establishes $e_0 e_1 \cdots e_t \equiv 1$.

For a prime $N$, the only residue that is not invertible is 0, and parts 2 and 3 say that exactly one check can see a residue that becomes zero: the check at the block that made it.

**What the check catches, by where the checks are placed.**

- **At every block.** The first wrong block is caught by its own check, whatever its error: by part 1 when its result is invertible, since the blocks before it are right, and by part 3 when it is not. A run that goes back to its last passed check on a failure therefore never carries a wrong block past its own check.
- **At some blocks only**, the others unchecked:
  - while the residues stay invertible, the check at block $t$ sees the product $e_0 \cdots e_t$ of the errors of every block up to $t$, checked or not. A single wrong block $j$ fails every check at a block $t \ge j$, with certainty, so the first check at or after it catches it. Errors in two or more blocks pass a check whenever their product up to that check is 1;
  - a residue that becomes zero in block $j - 1$ is caught only when block $j - 1$ is itself checked (part 3). Otherwise every later check passes (part 2), and the run ends wrong;
  - for a composite $N$, a residue that shares a prime factor $q$ with $N$, made in a block that is not checked, is invisible to every later check modulo $q$ (part 3).

<!-- check: mersenne(7) == 127 and pow(3, 2**2, 127) == 81 and 3 * 81 % 127 == 116 and 3 * pow(3, 2**2, 127) % 127 == 116 --> For example, take $p = 7$, $M_7 = 127$ and $L = 2$: a correct run holds $y_0 = 3$ and $y_1 = 3^{2^2} = 81$, so $D_1 = 3 \cdot 81 \equiv 116$, and the check at the first block passes, since $3 \cdot 3^{2^2} \equiv 116$ as well<!-- /check -->. <!-- check: pow(0, 2, 127) == 0 and 3 * pow(116, 2**2, 127) % 127 == 108 --> Now let a fault set the residue to 0 at squaring 3, inside the second block: the later block ends hold $0^2 = 0$, so $D_2 = D_3 = 0$, and the check at the second block fails, since $3 \cdot 116^{2^2} \equiv 108 \pmod{127}$<!-- /check -->. <!-- check: 3 * pow(0, 2**2, 127) % 127 == 0 --> The check at the third block compares $D_3 = 0$ with $3 \cdot 0^{2^2} \equiv 0 \pmod{127}$, and passes<!-- /check -->. <!-- check: 0**2 == 0 and 0 != 9 --> A run that checks only the first and third blocks passes both checks and ends, after the tail squaring, on $0^2 = 0$ rather than 9<!-- /check -->: it calls the prime $M_7$ composite.

**It cannot detect:**

- with checks at some blocks only, a residue that becomes zero in a block that is not itself checked; for a prime $N$ that is any residue that is not invertible, and for a composite $N$, a residue that shares a factor with $N$ escapes every later check modulo that factor (Theorem 5, parts 2 and 3);
- with checks at some blocks only, errors in two or more blocks whose errors multiply to exactly 1 by the next check;
- the squarings after the last check, including the tail: $p$ is prime, so no $L$ with $2 \le L < p$ divides it, and the last $p \bmod L$ squarings never close a block;
- what lies outside the chain: the starting value, the final comparison with 9, the residue a run reports, and its input and output;
- errors that are consistent with the arithmetic. The identity holds in any commutative ring, so a run that computes consistently modulo the wrong number, or with any other error that respects addition and multiplication, passes every check.

**The guard a test needs.** A test that checks only some blocks must treat a residue that is zero or not invertible as an error at every check: by part 4 it suffices to test the product $D_{t+1}$, which is invertible exactly when every residue up to $y_{t+1}$ is. For a prime $N$ that test is $D_{t+1} \not\equiv 0$; for any $N$ it is $\gcd(D_{t+1}, N) = 1$. The alternative is to check every block. Either way, a check that passes then establishes $e_0 \cdots e_t \equiv 1$.

Theorem 5 says which errors pass a check, not how often hardware makes them, so no probability of an undetected error follows from it. <!-- cited --> GIMPS states its own figures: it describes the check as almost completely eliminating the chance that a hardware error corrupts a primality test, says that results are 99.999+% reliable, though double-checking is still needed to guard against program error or forged results, and says that with the check GIMPS is extremely unlikely to miss a new Mersenne prime on the first test, a big improvement over the previous 1 or 2% chance ([GIMPSMath, Section PRP](references.md#gimpsmath))<!-- /cited -->. <!-- cited --> For Lucas–Lehmer tests it gives a historical error rate of around 1.5% when no serious error was reported during the run, and about 50% when one was ([GIMPSMath, Section Double-Checking](references.md#gimpsmath))<!-- /cited -->. The page gives no derivation of these figures. [LiGallot2022, § 1.1](references.md#ligallot2022) describes the check as saving the block-end residues and testing the product of all the block identities at once instead of each one, and [LiGallot2022, § 2](references.md#ligallot2022) extends it to any exponent, weighting each identity by a random exponent.

<!-- check: gerbicz_ok(127, 10, 5) and gerbicz_detects(127, 10, 5, 1) and gerbicz_detects(127, 10, 5, 23) and gerbicz_detects(127, 10, 5, 50) and 10 * 5 == 50 --> On $M_{127}$, checked at every block, 5 blocks of $L = 10$ squarings, 50 in all, pass every check when computed correctly, and a fault of $+1$ added after squaring 1, 23 or 50 fails a check<!-- /check -->.

### Proofs of a PRP result

A run can also leave a certificate that its final residue is right, which a verifier checks much faster than a second run. Pietrzak's protocol proves a claim $(x, T, y)$, that $y = x^{2^T}$ in a group $G$, by halving $T$ ([Pietrzak2019, § 3.1](references.md#pietrzak2019)). One round, on a claim $(x, T, y)$:

1. if $T = 1$, the verifier accepts exactly when $y = x^2$, and the rounds end;
2. otherwise the prover sends $\mu = x^{2^{T/2}}$, which needs $T$ even;
3. the verifier rejects unless $\mu \in G$, and otherwise picks a random $r$ among the integers below $2^\lambda$, for a security parameter $\lambda$;
4. both form $x' = x^r \mu$ and $y' = \mu^r y$; if $y = x^{2^T}$, then $y' = x'^{\,2^{T/2}}$. If $T/2$ is even, the next claim is $(x', T/2, y')$. If $T/2$ is odd, the next claim is $(x', T/2 + 1, y'^2)$, whose time is even again ([Pietrzak2019, § 3.1, Step 4](references.md#pietrzak2019)).

Step 4 prints the odd case's new time as $(T+1)/2$, which is not an integer for an even $T$; the identity $y'^2 = x'^{\,2^{T/2+1}}$ fixes it at $T/2 + 1$. The recursion needs these hypotheses:

- **The group.** $G$ is commutative, and the verifier can tell whether a value lies in it. A true claim then stays true at every round. Pietrzak's group is the signed quadratic residues modulo $N$.
- **Squaring an odd half.** Step 4's squaring keeps every false claim false only when squaring is one-to-one on $G$. It is on any group of odd order, and so on Pietrzak's, of order $p'q'$ for primes $p', q' \ge 2^\lambda$ with $N = (2p' + 1)(2q' + 1)$ ([Pietrzak2019, Theorem 1](references.md#pietrzak2019)), once $\lambda \ge 2$ makes both odd. It is not on the residues prime to $M_p$, whose order is even: $y'$ and $-y'$ have the same square. There the exact rewrite $(x', t, y') \to (x'^2, t - 1, y')$ serves at an odd time $t > 1$ instead, and needs no hypothesis on squaring. It does not remove the sign from the halving itself (below).
- **Termination.** From an even $T \ge 4$ the next time is $T/2$ or $T/2 + 1$, both below $T$, and from $T = 2$ it is 1, where the rounds end. Step 4's odd case must not be applied at $T/2 = 1$: it would return to $T = 2$, and the rounds would never end.
- **An odd start.** Step 2 needs an even $T$. For the PRP test of $M_p$, $T = p$ is odd, so the claim $x_p = 3^{2^p}$ is first rewritten exactly as $x_p = 9^{2^{p-1}}$, with the even time $p - 1$; the verifier forms $9 = 3^2$ itself. This first step, the exact rewrite above and the stop at $T/2 = 1$ are the project's own.

**The sign of $\mu$ (the project's own).** Modulo $M_p$ the sign defeats the halving itself, not only the squaring of an odd half, and the exact rewrite does not help. Let $N > 2$ be odd, let $G$ be the residues prime to $N$, and let a prover hold the false claim $(x, T, y)$ with $x \in G$, $T$ even and $y = -x^{2^T}$. In Step 2 it sends $-\mu$ in place of $\mu = x^{2^{T/2}}$; this lies in $G$, so Step 3 does not reject it. Step 4 then forms $x' = x^r (-\mu)$ and $y' = (-\mu)^r y$, and

$$y' = (-1)^{r+1} \, x'^{\,2^{T/2}}.$$

For an odd $r$ the next claim is true, and the prover goes on honestly to acceptance. For an even $r$ it is false again, in the same form $y' = -x'^{\,2^{T/2}}$, and the prover sends the negated midpoint again. The exact rewrite at an odd time keeps either form, and at $T = 1$ the verifier rejects the false one, since $-x^2 \ne x^2$. So the prover is accepted unless every $r$ is even. When each $r$ is drawn uniformly among the integers below $2^\lambda$, with $\lambda \ge 1$, and independently of the others, the false claim is accepted with probability exactly $1 - 2^{-h}$ over $h$ halvings. From the odd start the time $p - 1$ takes $h = \lfloor \log_2(p - 1) \rfloor$ halvings, so the false claim $y = -3^{2^p}$ about the last residue of the test of $M_p$ is accepted with probability above $1 - 2/(p - 1)$. <!-- check: 127 - 1 == 126 and 2**6 <= 126 and 126 < 2**7 and 2**6 - 1 == 63 and 2**6 == 64 --> For $p = 127$, the time $127 - 1 = 126$ satisfies $2^6 \le 126 < 2^7$, so the rounds make 6 halvings, and the false claim is accepted with probability $63/64$<!-- /check -->.

<!-- cited --> When $N$ is the product of two safe primes, and under the theorem's other conditions, a verifier accepts a false $y$ with probability at most $3 \log(T) / 2^\lambda$ ([Pietrzak2019, Theorem 1](references.md#pietrzak2019))<!-- /cited -->. A modulus $M_p$ need not meet these hypotheses. Pietrzak takes safe primes so that the group has no subgroup of small order, which his proof of soundness requires ([Pietrzak2019, § 2.3](references.md#pietrzak2019)). For a general modulus the soundness of the scheme rests on an unproved assumption, the low order assumption ([LiGallot2022, § 1.1](references.md#ligallot2022)): that an element of order below $2^\lambda$ is hard to find among the residues prime to the modulus ([LiGallot2022, § 4](references.md#ligallot2022)). Modulo $M_p$ the assumption is not unproved but false: $-1$ has order 2, below $2^\lambda$ for every $\lambda \ge 2$, and the sign of $\mu$ above is a prover using exactly that element. So nothing here makes a proof modulo $M_p$ by this recursion sound. How to make one sound is for the cycle that builds PRP proofs to design and prove. One direction is a group in which squaring is one-to-one: Pietrzak's protocol computes in the signed quadratic residues $QR^+_N$ ([Pietrzak2019, § 3](references.md#pietrzak2019)), of odd order for his modulus (the hypotheses above). Another is a fixed sign, which $QR^+_N$ also has: its elements are the absolute values of the quadratic residues, with residues represented from $-(N-1)/2$ to $(N-1)/2$, so a value and its negative are one element ([Pietrzak2019, § 2.1](references.md#pietrzak2019)). Whether either serves for $M_p$ is that cycle's question. The Gerbicz–Pietrzak scheme for exponents that are powers of 2 makes a certificate of $O(\log((\log n)/B))$ residues for an exponent $n$ and a block length $B$, verified in $B$ squarings ([LiGallot2022, § 1.1](references.md#ligallot2022)), and [LiGallot2022, § 2 and § 3](references.md#ligallot2022) generalises it to any exponent. GIMPS adds such proofs to its PRP tests [GIMPSMath](references.md#gimpsmath).

## Proof

**Theorem 1.** The nonzero residues modulo $q$ are $1, 2, \dots, q - 1$. Multiplying each by $a$ permutes them: $ak \equiv ak'$ gives $q \mid a(k - k')$, so $q \mid k - k'$ and $k = k'$. So the products agree, $\prod_k (ak) \equiv \prod_k k$, that is, $a^{q-1} \prod_k k \equiv \prod_k k \pmod q$. The product is not divisible by the prime $q$, so it cancels. $\square$

**The Fermat test.** If $N$ were prime, $a^{N-1} \equiv 1$ by Theorem 1. So $a^{N-1} \not\equiv 1$ proves $N$ composite. $\square$

**Theorem 2.** By induction, $x_k \equiv 3^{2^k}$, so $x_p \equiv 3^{2^p} = 3^{N+1} = 9 \cdot 3^{N-1}$. Since $p$ is odd, $2^p \equiv (-1)^p = -1 \pmod 3$, so $N \equiv 1 \pmod 3$ and $\gcd(9, N) = 1$. Multiplying by the inverse of 9, $x_p \equiv 9$ if and only if $3^{N-1} \equiv 1$. $\square$

**Theorem 3.** <!-- check: mersenne(2) == 3 and 2**(3 - 1) == 4 and 4 % 3 == 1 --> For $p = 2$, $M_2 = 3$ and $2^{M_2 - 1} = 2^{3 - 1} = 4 \equiv 1 \pmod 3$<!-- /check -->. For an odd prime $p$, Theorem 1 gives $2^{p-1} \equiv 1 \pmod p$, so $p$ divides $2^p - 2 = M_p - 1$; write $M_p - 1 = mp$. Since $2^p = M_p + 1 \equiv 1 \pmod{M_p}$, $2^{M_p - 1} = (2^p)^m \equiv 1 \pmod{M_p}$. $\square$

**Theorem 4, the identity.** By Theorem 2's recurrence, $x_{k + L} \equiv x_k^{2^L}$ for every $k$. By induction, $d_t \equiv \prod_{j=0}^{t} x_{jL}$. Then

$$x_0 \, d_t^{\,2^L} \equiv x_0 \prod_{j=0}^{t} x_{jL}^{2^L} \equiv x_0 \prod_{j=0}^{t} x_{(j+1)L} = \prod_{j=0}^{t+1} x_{jL} \equiv d_{t+1}. \qquad \square$$

**Theorem 5, part 1.** With $D_t = y_0 \cdots y_t$ and $y_{j+1} = e_j \, y_j^{2^L}$, all invertible,

$$\frac{x_0 \, D_t^{\,2^L}}{D_{t+1}} = \frac{y_0 \prod_{j=0}^{t} y_j^{2^L}}{y_0 \prod_{j=0}^{t} y_{j+1}} = \prod_{j=0}^{t} \frac{y_j^{2^L}}{y_{j+1}} = \frac{1}{e_0 e_1 \cdots e_t},$$

using $y_0 = x_0$. So the check passes exactly when the product of the block errors is 1. A single wrong block $j$ leaves the product equal to $e_j \ne 1$ at every check from $t = j$ on. The identity uses only the ring operations, so it holds as well for any consistent computation in another commutative ring, which is why consistent errors pass.

**Theorem 5, part 2.** For $t \ge j$, both $D_t = D_{j-1} \, y_j \cdots y_t$ and $D_{t+1}$ have the factor $y_j \equiv 0$, so $D_{t+1} \equiv 0$ and $x_0 \, D_t^{\,2^L} \equiv 0$, since $2^L \ge 1$. The two sides agree.

**Theorem 5, part 3.** The prime $q$ divides none of $y_0, \dots, y_{j-1}$, so it does not divide their product $D_{j-1}$, nor its powers. It does not divide $x_0$, since $\gcd(x_0, N) = 1$. So $q \nmid x_0 \, D_{j-1}^{\,2^L}$, while $q \mid y_j \mid D_j$: the two sides of the check at block $j - 1$ differ modulo $q$, hence modulo $N$, and it fails. For $t \ge j$, $q$ divides $D_t$ and $D_{t+1}$, so both sides are 0 modulo $q$.

**Theorem 5, part 4.** A product of invertible residues is invertible. Conversely, if $ab$ has the inverse $c$, then $a \cdot (bc) = 1$, so each factor of an invertible product is invertible. With part 1 this gives the last sentence. When $N$ is prime the residues modulo $N$ form a field, in which every residue but 0 is invertible. $\square$

**What the check catches, by placement.** At every block: let block $t$ be the first wrong block, so that $y_0, \dots, y_t$ are right, hence powers of $x_0$ and invertible. If $y_{t+1}$ is invertible, then $e_0 = \dots = e_{t-1} = 1 \ne e_t$, and the check at block $t$ fails by part 1. If it is not, some prime $q \mid N$ divides $y_{t+1}$ and none of $y_0, \dots, y_t$, and the check at block $t$ fails by part 3 with $j = t + 1$. The statements for checks at some blocks only are parts 1 to 3, read at the blocks checked. $\square$

**The recursion of a PRP proof.** Step 4: $x'^{\,2^{T/2}} = x^{r 2^{T/2}} \mu^{2^{T/2}} = \mu^r \, x^{2^{T/2} \cdot 2^{T/2}} = \mu^r x^{2^T}$, which is $\mu^r y = y'$ when $y = x^{2^T}$; squaring both sides gives $y'^2 = x'^{\,2^{T/2+1}}$. The exact rewrite is $x^{2^t} = (x^2)^{2^{t-1}}$, which with $x = 3$ and $t = p$ gives the odd start. A finite commutative group of odd order has no $u \ne 1$ with $u^2 = 1$, since the order of such a $u$ would be 2, and the order of an element divides the order of the group; so squaring is one-to-one on it, since $a^2 = b^2$ gives $(a b^{-1})^2 = 1$. Modulo $M_p$, $-1 \ne 1$ and $(-1)^2 = 1$, so the residues prime to $M_p$ have even order, and $y'$ and $-y'$ are distinct with the same square. For an even $T \ge 4$, $T/2 + 1 < T$. $\square$

**The sign of $\mu$.** Since $T/2 \ge 1$, the exponent $2^{T/2}$ is even, so $(-\mu)^{2^{T/2}} = \mu^{2^{T/2}} = x^{2^T}$, and $x^{r 2^{T/2}} = \mu^r$. Hence $x'^{\,2^{T/2}} = x^{r 2^{T/2}} (-\mu)^{2^{T/2}} = \mu^r x^{2^T}$, while $y' = (-1)^r \mu^r \cdot (-x^{2^T}) = (-1)^{r+1} \mu^r x^{2^T}$: the two differ by the factor $(-1)^{r+1}$. The rewrite $x^{2^t} = (x^2)^{2^{t-1}}$ holds with either sign in front. For $u \in G$, $-u = u$ would give $N \mid 2u$, impossible for an odd $N > 1$ and $u$ prime to $N$; so $-1 \ne 1$, the claim $y = -x^{2^T}$ is false, and at $T = 1$, $-x^2 \ne x^2$. Before the first odd $r$ every claim has the false form, and the verifier rejects at $T = 1$ if no $r$ is odd. The first odd $r$ makes the claim true, and a true claim stays true at every later round, by the hypothesis on the group, so the verifier accepts. Since $2^\lambda$ is even for $\lambda \ge 1$, each $r$ is even with probability exactly $1/2$, and $h$ independent draws are all even with probability $2^{-h}$. The count of halvings: write $\ell(t)$ for the number of binary digits of $t$. A halving of an even $t$ drops its last digit, a 0, so $\ell(t/2) = \ell(t) - 1$. The rewrite of an odd $t > 1$ clears its last digit, a 1, which is not its leading digit, so $\ell(t - 1) = \ell(t)$. The rounds run from $T = p - 1$ to $T = 1$, and $\ell(1) = 1$, so they make $h = \ell(p - 1) - 1 = \lfloor \log_2(p - 1) \rfloor$ halvings, whatever the $r$. Then $2^{h+1} > p - 1$ gives $2^{-h} < 2/(p - 1)$. $\square$

## What mps uses

The PRP test does not exist yet. `-t` offers trial factoring, P-1, Lucas–Lehmer and all three (`tf`, `p1`, `ll`, `all`), and none of them computes a Fermat test. The result type already has the outcome a probable prime will carry, `Result::probable_prime`, and no test returns it today.

The planned test will use Theorem 2: $p$ squarings modulo $M_p$ from 3, and a probable-prime verdict only when a complete run ends in 9. It will run the Gerbicz check of Theorem 4 at intervals, keep saved state only after a check passes, and go back to the last such state when one fails. Because it checks only some blocks, at every check it will treat a product $D_{t+1}$ that is zero, or not invertible, as a failure, by Theorem 5; the alternative is to check every block. It will cover what the check cannot see with checks of their own: the squarings after the last block, and the final comparison. A proof of the result, as sketched above, may follow.

## Sources

- [CrandallPomerance2005, § 3.4.1](references.md#crandallpomerance2005): Fermat pseudoprimes.
- [OEIS-A000043, the data section](references.md#oeis-a000043): the published Mersenne prime exponents.
- [LiGallot2022, § 1.1](references.md#ligallot2022): the Gerbicz check, and the Gerbicz–Pietrzak proof scheme with its size, cost and conditional soundness.
- [LiGallot2022, § 2](references.md#ligallot2022): the check extended to any exponent, with random weights.
- [LiGallot2022, § 2 and § 3](references.md#ligallot2022): the proof scheme for any exponent.
- [LiGallot2022, § 4](references.md#ligallot2022): the low order assumption, as its soundness proof states it.
- [Pietrzak2019, § 3.1](references.md#pietrzak2019): the halving protocol.
- [Pietrzak2019, § 3.1, Step 4](references.md#pietrzak2019): the next claim, for an even and an odd half.
- [Pietrzak2019, Theorem 1](references.md#pietrzak2019): its soundness when the modulus is a product of two safe primes, and the order of its group.
- [Pietrzak2019, § 2.1](references.md#pietrzak2019): the signed quadratic residues, the absolute values of the quadratic residues.
- [Pietrzak2019, § 2.3](references.md#pietrzak2019): safe primes, so that the group has no subgroup of small order, as the soundness proof requires.
- [Pietrzak2019, § 3](references.md#pietrzak2019): the protocol computes in the signed quadratic residues.
- [GIMPSMath, Section PRP](references.md#gimpsmath): the check as GIMPS describes it, and its reliability figures for PRP tests.
- [GIMPSMath, Section Double-Checking](references.md#gimpsmath): its historical error rates for Lucas–Lehmer tests.
- [GIMPSMath](references.md#gimpsmath): the PRP test, the Gerbicz check and proofs as GIMPS uses them.

## Provenance

Theorems 1 to 3 are elementary and proved in full above. The Gerbicz check is described in [LiGallot2022, § 1.1](references.md#ligallot2022) and credited to Robert Gerbicz by [GIMPSMath](references.md#gimpsmath); its identity is proved here from the definitions. Theorem 5, what a check does and does not establish for each placement of the checks, and the guard it calls for, are the project's own, proved here from the definitions. The proof scheme is [Pietrzak2019, § 3.1](references.md#pietrzak2019) and [LiGallot2022, § 2 and § 3](references.md#ligallot2022); the odd start, the exact rewrite at an odd time, the stop at a half of 1, and the sign of $\mu$ in the halving with the probability it gives a false claim are the project's own. The planned test will be built from these papers and documentation, and from nothing drawn from another program.
