# Weighted transforms: squaring modulo $2^p - 1$ by the IBDWT

## The result

Throughout, $p \ge 2$ is the exponent, $M_p = 2^p - 1$, and $N$ is the transform length, an integer with $1 \le N \le p$. Write $\beta = p/N$ for the average number of bits in a word.

**Positions and widths.** For $0 \le j \le N$ put $c_j = \lceil pj/N \rceil$, so that $c_0 = 0$ and $c_N = p$. Word $j$, for $0 \le j < N$, holds the bits from $c_j$ to $c_{j+1} - 1$, and its width is $s_j = c_{j+1} - c_j$. Every width is $\lfloor \beta \rfloor$ or $\lceil \beta \rceil$, so at least 1; when $\beta$ is not an integer, exactly $p \bmod N$ words have the larger width. A residue modulo $M_p$ is held as

$$X = \sum_{j=0}^{N-1} x_j \, 2^{c_j},$$

the variable-base representation of [CrandallFagin1994, eq. (6.5) and eq. (6.6)](references.md#crandallfagin1994). The digits are **balanced** when $-2^{s_j - 1} \le x_j < 2^{s_j - 1}$ for every $j$, the balanced-variable form of [CrandallFagin1994, eq. (3.6)](references.md#crandallfagin1994), which their Mersenne-mod routines use to cut convolution errors ([CrandallFagin1994, p. 316](references.md#crandallfagin1994)). Every residue modulo $M_p$ has a balanced representation.

**Weights.** Put $f_j = c_j - pj/N$, so that $0 \le f_j < 1$ and $f_0 = 0$, and let the weight of word $j$ be

$$a_j = 2^{f_j} = 2^{\lceil pj/N \rceil - pj/N}, \qquad 1 \le a_j < 2,$$

as in [CrandallFagin1994, eq. (6.7)](references.md#crandallfagin1994). Then $X = \sum_j (a_j x_j) \, 2^{pj/N}$: the weighted digits are digits in the irrational base $2^{p/N}$, whose $N$-th power is $2^p \equiv 1 \pmod{M_p}$. That is why a cyclic convolution of length $N$ performs the reduction modulo $M_p$ by itself.

**Theorem 1 (squaring and multiplying modulo $M_p$ by a weighted cyclic convolution).** Let $X = \sum_j x_j 2^{c_j}$ and $Y = \sum_j y_j 2^{c_j}$ with any integer digits. For $0 \le i, j, k < N$ with $i + j \equiv k \pmod N$, let $w_{ij} = 1$ if $i + j \ge N$ and $w_{ij} = 0$ otherwise, and put $e_{ij} = c_i + c_j - c_k - p \, w_{ij}$. Then:

1. $e_{ij} \in \{0, 1\}$, and $a_i a_j = a_k \, 2^{e_{ij}}$;
2. the integers $z_k = \sum_{i + j \equiv k \ (\mathrm{mod}\ N)} x_i y_j \, 2^{e_{ij}}$, for $0 \le k < N$, satisfy

$$XY \equiv \sum_{k=0}^{N-1} z_k \, 2^{c_k} \pmod{M_p};$$

3. $a_k z_k = \sum_{i + j \equiv k \ (\mathrm{mod}\ N)} (a_i x_i)(a_j y_j)$, that is, $z = a^{-1} \bigl( (a x) * (a y) \bigr)$, where $*$ is the cyclic convolution of length $N$ and the products with $a$ and $a^{-1}$ are taken word by word.

So the product modulo $M_p$ is a cyclic convolution of length $N$ of the weighted words, unweighted, and then a carry pass that brings the $z_k$ back to digits of widths $s_k$. This is the irrational-base discrete weighted transform (IBDWT) of [CrandallFagin1994, § 6](references.md#crandallfagin1994), as [Percival2003, § 3](references.md#percival2003) restates it. Multiplying two $p$-bit numbers by an unweighted cyclic convolution needs a zero-padded signal of twice the length, and then a separate reduction ([CrandallFagin1994, § 4](references.md#crandallfagin1994)); the weights remove the padding and halve the transform length ([CrandallFagin1994, the abstract](references.md#crandallfagin1994); [Percival2003, § 1](references.md#percival2003)). The words have about $\beta$ bits whatever $p$ is, so $p$ need not be a multiple of $N$, and $N$ can be a convenient, highly composite length ([Percival2003, § 3](references.md#percival2003)).

**Computing the convolution.** By the convolution theorem, $(u * v) = \mathrm{DFT}^{-1}\bigl(\mathrm{DFT}(u) \cdot \mathrm{DFT}(v)\bigr)$ over any field holding a primitive $N$-th root of unity; it is proved in [the note on number-theoretic transforms](number-theoretic-transforms.md), and over the complex numbers, with $\omega = e^{2\pi i/N}$, it is [CrandallFagin1994, eq. (2.15)](references.md#crandallfagin1994). A fast Fourier transform computes each transform in $O(N \log N)$ operations. The weighted words $a_j x_j$ are real, so real-signal FFTs serve, which [CrandallFagin1994, p. 310](references.md#crandallfagin1994) recommends, naming the split-radix real-signal transforms of [Sorensen1987](references.md#sorensen1987). In floating point the computed values $z'_k$ are inexact, and each is rounded to the nearest integer; the result is exact as long as every error is below one half ([CrandallFagin1994, eq. (4.4)](references.md#crandallfagin1994)). One squaring, as the algorithm of [CrandallFagin1994, § 3](references.md#crandallfagin1994) arranges it:

```text
square X modulo M_p, held as balanced digits x_0 … x_(N−1):
    for each j: u_j ← a_j · x_j
    U ← the DFT of u, by an FFT of length N
    for each k: V_k ← U_k · U_k
    v ← the inverse DFT of V
    for each k: z_k ← the integer nearest v_k / a_k
    carry: rewrite the sum of z_k · 2^(c_k) as balanced digits of widths s_k,
           adding the carry out of the top word into word 0, since 2^p ≡ 1
```

**A worked example**, from [CrandallFagin1994, § 6](references.md#crandallfagin1994). <!-- check: dwt_widths(37, 4) == (10, 9, 9, 9) and 37 % 4 == 1 --> For $p = 37$ and $N = 4$ the widths are 10, 9, 9 and 9, and $37 \bmod 4 = 1$ word is the wider<!-- /check -->. <!-- check: 78314567209 == 553 + 93 * 2**10 + 381 * 2**19 + 291 * 2**28 --> The residue $78314567209 = 553 + 93 \cdot 2^{10} + 381 \cdot 2^{19} + 291 \cdot 2^{28}$ has the digits 553, 93, 381 and 291<!-- /check -->, here all nonnegative. <!-- check: 4 * 10 - 37 * 1 == 3 and 4 * 19 - 37 * 2 == 2 and 4 * 28 - 37 * 3 == 1 --> The positions are $c_1 = 10$, $c_2 = 19$ and $c_3 = 28$, and $4 c_j - 37 j$ is 3, 2 and 1 for $j = 1, 2, 3$, so the weights are $1, 2^{3/4}, 2^{1/2}, 2^{1/4}$<!-- /check -->. <!-- check: 1 + 3 == 4 and 2 + 2 == 4 and 10 + 28 - 37 == 1 and 19 + 19 - 37 == 1 --> In $z_0$ the pairs $(1, 3)$, $(3, 1)$ and $(2, 2)$ wrap past $N = 4$, each with $e = 1$, since $c_1 + c_3 - 37 = 10 + 28 - 37$ and $c_2 + c_2 - 37 = 19 + 19 - 37$ are both 1<!-- /check -->, so <!-- check: 553**2 + 2 * 2 * 93 * 291 + 2 * 381**2 == 704383 --> $z_0 = 553^2 + 2 \cdot 2 \cdot 93 \cdot 291 + 2 \cdot 381^2 = 704383$<!-- /check -->. <!-- check: 78314567209**2 % (2**37 - 1) == (704383 + 324600 * 2**10 + 523365 * 2**19 + 463578 * 2**28) % (2**37 - 1) --> With the other outputs, $78314567209^2 \equiv 704383 + 324600 \cdot 2^{10} + 523365 \cdot 2^{19} + 463578 \cdot 2^{28} \pmod{2^{37} - 1}$<!-- /check -->, the identity of Theorem 1, before the carry pass. In exact integer arithmetic, <!-- check: dwt_product_ok(521, 16, 20) and dwt_product_ok(4423, 64, 5) and dwt_product_ok(127, 10, 20) --> the identity holds on 20 pseudorandom pairs for $p = 521$ and $N = 16$, on 5 for $p = 4423$ and $N = 64$, and on 20 for $p = 127$ and $N = 10$<!-- /check -->, with every $e_{ij}$ in $\{0, 1\}$.

### Error bounds

A floating-point squaring is right only when every computed $z'_k$ lies within one half of the integer $z_k$. What is proved about that, and what is only estimated, differ by a wide margin.

**The size of the outputs** (a step of the project's own, proved below). With balanced digits, every weighted word satisfies $|a_j x_j| < 2^{\beta}$. Hence $|z_k| < N \, 2^{2\beta}$ for every $k$, and the Euclidean norm of the weighted signal is $|a x| < \sqrt{N} \, 2^{\beta}$.

**Percival's bound, a theorem.** <!-- cited --> Let $N = 2^n$, and let $\epsilon$ bound the relative error of each floating-point addition, subtraction and multiplication, $\beta'$ the error of each computed root of unity $w^k$, $w = e^{2\pi i/N}$, and $\delta$ a bound both on the relative error of each computed weight and on the error of each computed inverse weight, with $|a_k| \ge 1$. Then the FFT computes the cyclic convolution $z = x * y$ of two complex vectors with $|z' - z|_\infty < |x| \, |y| \, \bigl( (1 + \epsilon)^{3n} (1 + \epsilon\sqrt5)^{3n+1} (1 + \beta')^{3n} - 1 \bigr)$, and the weighted transform computes $z = a^{-1}((a x) * (a y))$ with $|z' - z|_\infty < |a x| \, |a y| \, \bigl( (1 + \epsilon)^{3n} (1 + \epsilon\sqrt5)^{3n+4} (1 + \beta')^{3n} (1 + \delta)^3 - 1 \bigr)$, where $|\cdot|$ is the Euclidean norm ([Percival2003, Theorem 5.1 and Theorem 6.1](references.md#percival2003))<!-- /cited -->. Percival writes $\beta$ for the error of the roots of unity; it is $\beta'$ here, to keep it apart from $\beta = p/N$. The proof models a radix-2 FFT, whose $n$ passes each replace pairs $(z_0, z_1)$ by $(z_0 + w z_1, z_0 - w z_1)$. <!-- cited --> In IEEE double precision $\epsilon = 2^{-53}$, and with correctly rounded roots of unity $\beta' = \epsilon/\sqrt2$ ([Percival2003, p. 391 and p. 392](references.md#percival2003))<!-- /cited -->. The bound is for the worst input, and the worst inputs are far worse than random ones: <!-- cited --> inputs such as $x_j = A e^{2\pi i j/N}$ give errors much larger than random inputs do, running parallel to the bound at a nearly constant gap below it ([Percival2003, § 7](references.md#percival2003))<!-- /cited -->.

For Mersenne numbers, <!-- cited --> Percival derives that with perfect weights and roots of unity, FFTs of length $2^{18}$ complex values are safe for exponents under 7 million and of length $2^{20}$ for exponents under 26 million, where GIMPS used limits of about 10 million and 40 million for the same lengths ([Percival2003, p. 392](references.md#percival2003))<!-- /cited -->. A transform sized by the theorem is never wrong for want of precision, but it is longer than practice uses: <!-- cited --> in this example a rigorously safe multiplication would add about 50% to the run time ([Percival2003, p. 392](references.md#percival2003))<!-- /cited -->.

**Crandall and Fagin's estimates, conjectures.** <!-- cited --> For floating point with $Q$ mantissa bits and digits of size $W$, they suggest that the error obeys $e_n < c \, 2^{-Q} W^2 N^{3/2} \log N$ with a constant $c$ near 1 for standard digits, and $e_n < c' \, 2^{-Q} W^2 N \log N$ for balanced digits, and they call both conjectural, based only on a finite set of experiments ([CrandallFagin1994, eq. (4.5) and eq. (4.6)](references.md#crandallfagin1994))<!-- /cited -->.

**The Crandall–Mayer–Papadopoulos heuristic, an estimate.** Modelling the transform outputs and the rounding errors as random walks, with balanced digits essential to the model, they estimate the largest convolution output, and require the expected largest error to stay below one quarter. <!-- cited --> With $\epsilon$ the unit roundoff, $B_{\mathrm{mant}} = -\log_2 \epsilon$, a word size $W$ and a constant $C$ of order one, the requirement reads $\epsilon C \frac{W^2}{4} (N \log_2 N)^{1/2} (\log \log N)^{3/2} < \frac14$, or, in logarithms, $2 \log_2 W + \log_2 C + \frac12 [\log_2 N + \log_2 \log_2 N] + \frac32 [\log_2 \log \log N] < B_{\mathrm{mant}}$ ([CrandallMayerPapadopoulos2003, § 3.1.1, eq. (7) and eq. (8)](references.md#crandallmayerpapadopoulos2003))<!-- /cited -->. <!-- cited --> For $B_{\mathrm{mant}} = 53$ and length $2^{20}$ it predicts words of roughly $20 - \log_2 C$ bits, $C = 2$ matches the roughly 19 bits their tests allowed, and each doubling of the length costs roughly 0.25 bits a word ([CrandallMayerPapadopoulos2003, § 3.1.1](references.md#crandallmayerpapadopoulos2003))<!-- /cited -->. It is a heuristic, fitted to experiment through $C$, and proves nothing about any input. Percival disputes its random-walk model, while noting that the resulting estimate is within a factor of $\sqrt{\log \log N}$ of correct ([Percival2003, § 4](references.md#percival2003)).

**What follows for a run.** Percival's theorem does not cover the transform lengths that practice uses at a given exponent, as his own comparison above shows: those lengths come from the heuristic and from measurement, and a run must watch its own rounding. The largest distance $\max_k |z'_k - \mathrm{round}(z'_k)|$ costs one comparison per word, and a distance near one half means the result can no longer be trusted. The Jacobi check of [the Lucas–Lehmer note](lucas-lehmer.md) and the Gerbicz check of [the PRP note](prp-and-gerbicz.md) detect errors that slip past it, each in its own way and to its own extent, and an exact transform ([the number-theoretic transforms note](number-theoretic-transforms.md)) avoids rounding altogether.

## Proof

**The widths.** Write $pj/N = c_j - f_j$ with $0 \le f_j < 1$. Then $s_j = c_{j+1} - c_j = \lceil \beta - f_j \rceil$, and $\beta - f_j$ lies in $(\beta - 1, \beta]$, where the ceiling takes only the values $\lfloor \beta \rfloor$ and $\lceil \beta \rceil$ (only $\beta$, when $\beta$ is an integer). Since $\beta \ge 1$, every width is at least 1. The widths sum to $c_N - c_0 = p$; if $t$ words have the width $\lceil \beta \rceil = \lfloor \beta \rfloor + 1$, then $p = N \lfloor \beta \rfloor + t$, so $t = p \bmod N$.

**Balanced representations exist.** Put $x_j = d_j - 2^{s_j - 1}$. As the $d_j$ range over $0 \le d_j < 2^{s_j}$, the sum $\sum_j d_j 2^{c_j}$ takes each value from 0 to $2^p - 1$ exactly once, since it is the binary expansion cut into words. So the balanced sums $\sum_j x_j 2^{c_j}$ take $2^p$ consecutive integer values, $-H$ to $2^p - 1 - H$ with $H = \sum_j 2^{s_j - 1 + c_j}$, and $2^p$ consecutive integers meet every residue class modulo $M_p = 2^p - 1$.

**Theorem 1.** Let $i + j = k + w_{ij} N$. Substituting $c_m = pm/N + f_m$,

$$e_{ij} = c_i + c_j - c_k - p \, w_{ij} = \frac{p (i + j - k - w_{ij} N)}{N} + f_i + f_j - f_k = f_i + f_j - f_k.$$

The left side is an integer and the right lies in $(-1, 2)$, so $e_{ij} \in \{0, 1\}$, and $a_i a_j / a_k = 2^{f_i + f_j - f_k} = 2^{e_{ij}}$. For part 2, $2^{c_i} 2^{c_j} = 2^{c_k} \, 2^{e_{ij}} \, 2^{p w_{ij}} \equiv 2^{c_k} 2^{e_{ij}} \pmod{M_p}$, since $2^p \equiv 1$. Expanding $XY = \sum_{i,j} x_i y_j 2^{c_i + c_j}$ and grouping the terms by $k$ gives $XY \equiv \sum_k z_k 2^{c_k}$. For part 3, $(a_i x_i)(a_j y_j) = a_k \, 2^{e_{ij}} x_i y_j$, and summing over the pairs with $i + j \equiv k$ gives $a_k z_k$. $\square$

[CrandallFagin1994, § 6](references.md#crandallfagin1994) state the same: each convolution term is $x_j y_k$ times a power of 2 whose exponent is 0 or 1, so the weighted convolution has integer components. The argument above is written out in full for the note.

**The size of the outputs** (the project's own derivation). For balanced digits $|x_j| \le 2^{s_j - 1}$, so

$$|a_j x_j| \le 2^{f_j + s_j - 1} = 2^{c_{j+1} - pj/N - 1} < 2^{p(j+1)/N - pj/N} = 2^{\beta},$$

using $c_{j+1} < p(j+1)/N + 1$. By part 3, $|z_k| \le a_k^{-1} \sum_{i + j \equiv k} |a_i x_i| \, |a_j y_j| < N \, 2^{2\beta}$, since $a_k \ge 1$ and the sum has $N$ terms. The Euclidean norm is $|a x| = (\sum_j |a_j x_j|^2)^{1/2} < \sqrt{N} \, 2^{\beta}$. $\square$

**Percival's theorems** are proved in [Percival2003, § 5 and § 6](references.md#percival2003). In outline: a complex addition has relative error below $\epsilon$ and a complex multiplication below $\epsilon\sqrt5$; each of the $n$ radix-2 passes then grows the Euclidean norm of the error by a bounded factor; the pointwise product is bounded by the Cauchy–Schwarz inequality; and the inverse transform adds errors of the same order. The weighted form adds the error of applying the weights before and after. The heuristic of Crandall, Mayer and Papadopoulos has no proof: it is an estimate from random-walk statistics, checked against experiment, as the sections above say.

## What mps uses

The weighted-transform engine does not exist yet. Today `mps` has no arithmetic on numbers wider than 64 bits: `BigInt` in `src/mp.cpp` keeps decimal digits and can only load and print them, and no test squares anything modulo $M_p$.

The planned engine will use Theorem 1 as stated: balanced digits of widths $s_j$, the weights $a_j$, a cyclic convolution of length $N$ by double-precision real FFTs, rounding and a carry pass that wraps the top carry into word 0. It will size each length by the heuristic and by measurement rather than by Percival's bound, report the largest rounding distance of every squaring, and refuse to return a residue when that distance passes a threshold. It will be tested against a reference big integer, and the shift and the Lucas–Lehmer subtraction will be folded into the carry pass.

## Sources

- [CrandallFagin1994, the abstract](references.md#crandallfagin1994): weighted transforms remove zero-padding and shorten the FFT for arithmetic modulo Mersenne numbers.
- [CrandallFagin1994, § 3](references.md#crandallfagin1994): the weighted-convolution multiplication algorithm.
- [CrandallFagin1994, § 4](references.md#crandallfagin1994): FFT multiplication with zero-padding.
- [CrandallFagin1994, § 6](references.md#crandallfagin1994): Mersenne numbers and irrational bases, with the worked example.
- [CrandallFagin1994, eq. (2.15)](references.md#crandallfagin1994): the weighted convolution through DFTs.
- [CrandallFagin1994, eq. (3.6)](references.md#crandallfagin1994): the balanced-variable representation.
- [CrandallFagin1994, eq. (4.4)](references.md#crandallfagin1994): the rounding requirement.
- [CrandallFagin1994, eq. (4.5) and eq. (4.6)](references.md#crandallfagin1994): the conjectured error growth.
- [CrandallFagin1994, eq. (6.5) and eq. (6.6)](references.md#crandallfagin1994): the variable-base representation and its widths.
- [CrandallFagin1994, eq. (6.7)](references.md#crandallfagin1994): the weights.
- [CrandallFagin1994, p. 310](references.md#crandallfagin1994): real-signal FFTs for the convolution.
- [CrandallFagin1994, p. 316](references.md#crandallfagin1994): balanced digits in the Mersenne-mod routines.
- [Sorensen1987](references.md#sorensen1987): real-valued FFT algorithms.
- [Percival2003, § 1](references.md#percival2003): the weighted transform halves the transform length.
- [Percival2003, § 3](references.md#percival2003): the irrational-base representation and weights, restated.
- [Percival2003, § 4](references.md#percival2003): earlier error estimates, and a critique of the random-walk model.
- [Percival2003, § 5 and § 6](references.md#percival2003): the proofs of the error bounds.
- [Percival2003, Theorem 5.1 and Theorem 6.1](references.md#percival2003): the error bounds for the FFT and the weighted transform.
- [Percival2003, p. 391 and p. 392](references.md#percival2003): the constants for IEEE double precision.
- [Percival2003, p. 392](references.md#percival2003): the safe lengths for Mersenne numbers, against GIMPS's limits.
- [Percival2003, § 7](references.md#percival2003): inputs whose errors run parallel to the bound, far above those of random inputs.
- [CrandallMayerPapadopoulos2003, § 3.1.1, eq. (7) and eq. (8)](references.md#crandallmayerpapadopoulos2003): the sizing heuristic.
- [CrandallMayerPapadopoulos2003, § 3.1.1](references.md#crandallmayerpapadopoulos2003): its constant fitted to experiment, and its trend with the length.

## Provenance

Theorem 1 is the irrational-base weighted transform of [CrandallFagin1994](references.md#crandallfagin1994), proved in full here; the bound on the outputs is the project's own. The error bounds are [Percival2003](references.md#percival2003), the conjectures [CrandallFagin1994](references.md#crandallfagin1994), and the sizing heuristic [CrandallMayerPapadopoulos2003](references.md#crandallmayerpapadopoulos2003); real-signal transforms are [Sorensen1987](references.md#sorensen1987), as Crandall and Fagin cite them. The planned engine will be built from these papers, and from nothing drawn from another program.
