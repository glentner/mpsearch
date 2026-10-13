# Brief 01 — the citations and the checks, probed at plan time

Plan-time probes on this Mac, 2026-10-12: Python 3.9.6 (`/usr/bin/python3`), the network, and a
scratch clone of the cycle branch with prototype notes. Nothing here is a verification record: R5's
record is the builders', written when each entry lands. This brief says what a builder will meet.

## 1. DOIs: the metadata route works, and recall fails

`https://doi.org/<doi>` with `Accept: application/vnd.citationstyles.csl+json` returns CSL JSON for
Crossref DOIs (AMS, JSTOR, Wiley, CUP, Springer, IEEE, Elsevier, RFC Editor) and DataCite DOIs (arXiv,
LIPIcs). Every DOI the seeds give resolved to the work the seed names (HTTP 200, title and authors as
the seed says): Crandall–Fagin 1994, Percival 2003, Crandall–Mayer–Papadopoulos 2003, Lehmer 1930,
Lehmer 1935, Bruce 1993, Pietrzak 2019, Wesolowski 2020, Pollard 1974, Montgomery 1987,
Montgomery–Silverman 1990, Montgomery–Kruppa 2008, Möller 2008.

The seeds give no DOI for nine works. The planner's own guesses from memory for two of them,
Reed–Truong 1975 and Miller–Reed–Truong 1981, resolved to unrelated papers (a Kalman-filtering paper
and a hypothesis-testing paper). Crossref's bibliographic search then found
`10.1109/tit.1975.1055352` and `10.1109/tassp.1981.1163501`, whose titles match the seeds'. This is
the R5 failure the gate exists for: a DOI written from recall is checked, or it fails.

What the metadata does that the gate must tolerate (each seen at plan time):

- **Online year before print year.** Percival 2003 and Crandall–Mayer–Papadopoulos 2003 carry only
  2002 dates (published online), Möller 2008 only 2007; the volumes are 2003 and 2008. Lehmer 1935
  and Bruce 1993 carry a later online date (2016, 2018) beside the print year.
- **Missing fields.** Crandall–Pomerance 2005 (a Springer book) has no authors and the title
  "Prime Numbers" without its subtitle; Barrett 1987 has no date at all; RFC 8259 has no author.
- **Odd fields.** Reed–Truong 1975's second author has family name "Treiu-Kien Truong"; Brent–
  Zimmermann 2010's title is "An O(M(n) logn) Algorithm for the Jacobi Symbol"; Lehmer 1935's issue
  is 3 although its DOI says 2.

URL-only works answer a plain `urllib` GET with HTTP 200 and carry their titles: GIMPS "The Math"
(`https://www.mersenne.org/various/math.php`, title "GIMPS - The Math - PrimeNet") and OEIS A000043.
OEIS's JSON record (`https://oeis.org/search?q=id:A000043&fmt=json`) lists the exponents 2 … 82589933
(51 terms); the gate holds the first 26, to 23209.

## 2. `src/` cites three works, not one

The GOAL's Problem says the only citation in the code is Crandall and Pomerance. `src/result.cpp`
(landed in `ea98d39`, before `61f5ce7`) also cites RFC 3629 (Yergeau, "UTF-8, a transformation
format of ISO 10646", 2003, § 4) and RFC 8259 (2017, §§ 7 and 8). R6 ("the works `src/` comments cite
today SHALL have entries") covers them as worded, so no R-ID bends: both get entries, cited by `src/`
only. Their DOIs are `10.17487/RFC3629` and `10.17487/RFC8259`.

## 3. The computations: bounds and timings (pure Python, this Mac)

| Check | Bound | Time | Result |
|---|---|---|---|
| Lucas–Lehmer on every prime $p \le B$, plus base-3 Fermat on each composite | 2500 | 4 s | 17 exponents, equal to A000043's terms up to 2281; 350 composite, each confirmed by Fermat |
| the same | 4500 | 39 s | 20 exponents |
| Jacobi symbol of $s_i - 2$, every odd prime $p \le B$, every $i$, unshifted and three shifted runs | 521 | 4 s | no case other than $-1$ |
| the same over every odd $p$, prime or not | 99 | 0.05 s | 0 from $i = a$ on whenever $M_a$ divides $M_p$: $(9,3)$, $(15,3)$, $(25,5)$, $(49,7)$, … |
| base-3 Fermat on the published primes up to $B$ | 11213 | 5 s | all 23 pass; 23209 takes 17 s more |
| every prime $q \le 2^{20}$ dividing $M_p$, odd prime $p \le 100$, by trying all primes | — | 1 s | 27 factors, each $2kp+1$ and $\pm1 \bmod 8$ |

A search for a factor $q$ of a composite $M_p$ (prime $p < 1000$, $q = 2kp + 1$, $k < 200000$)
dividing some $s_j$ found none. Such a $q$ needs $\omega = 2 + \sqrt 3$ to have 2-power order modulo
$q$, which is rare. So for prime exponents the zero case of R4 is real but unseen at these bounds; the
odd composite exponents show it, as the derivation predicts.

The Jacobi derivation the checks confirm (the builder writes it in full): as integers,
$s_i - 2 = 12 \prod_{k=0}^{i-2} s_k^2$ for $i \ge 1$, from $s_{k+1} + 2 = s_k^2$. For odd $p \ge 3$,
$M_p \equiv 7 \pmod 8$ and $M_p \equiv 1 \pmod 3$, so $J(2 \mid M_p) = 1$ and
$J(3 \mid M_p) = -J(M_p \mid 3) = -1$. Hence $J(s_i - 2 \mid M_p) = -1$ when no $s_k$ with
$k \le i - 2$ shares a factor with $M_p$, and $0$ otherwise; and a shift multiplies the argument by
a power of 2, which leaves it unchanged. Primality of $p$ is not needed.

A worked P-1 example: $M_{67}$ with $B_1 = 30$, $B_2 = 3000$. Stage 1 gives $\gcd = 1$; stage 2 gives
Cole's factor $193707721 = 2^3 \cdot 3^3 \cdot 5 \cdot 67 \cdot 2677 + 1$ (Cole 1903,
`10.1090/S0002-9904-1903-01079-9`, which resolves). $B_2 = 2000$ finds nothing.

## 4. The gates, prototyped

The frozen gates (`verify/`) ran against prototype notes for all seven families in a scratch clone.
Before the adversarial review, P1 was green in 16 s, P2 in 34 s, and P4 and P5 in 39 s (each
including `make check`, 14 s, and the network check of every reference). 47 injected defects of P1's
tree and 19 of the later notes each failed for the intended reason. Among them: a false number shown beside a true check, a check that drops or
smuggles a number, a number outside any span, a C fence, C in prose, a hidden comment, a code-host
URL, a wrong year, author, title or DOI, a stale record and an edited gate. The review's attacks and the
final timings are in PLAN § 4 and § 4.1.
