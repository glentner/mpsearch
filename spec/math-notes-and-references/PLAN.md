# PLAN — The mathematics, written down: notes and references

> **Status:** Draft for review · **Last updated:** 2026-10-12
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). Backing detail is in
> [`research/`](research/00-digest.md), one brief on the lean path (§ 4). Every design element
> traces to a GOAL R-ID.

## 1. Summary

`doc/` gains an index, a bibliography and seven notes, one per algorithm family, in GitHub Markdown
with `$…$` math. `AGENTS.md` gains the `doc/` line in § Map and the same-commit rule in
§ Conventions. Nothing under `src/`, `tests/`, `ci/` or the `Makefile` changes.

What makes the cycle more than prose is how its claims are held:

- **Numbers.** Every concrete number a note states sits in a check span, an HTML comment carrying a
  small integer expression that must evaluate to True. The integers the text shows must match the
  integers the expression writes or computes. R7's five computations are functions the expressions
  call.
- **Published values.** A value taken from a work sits in a cited span, with a located citation.
- **References.** Each entry is checked over the network against its DOI's metadata, or its page.
- **The clean room.** No code fences but `text` pseudocode, no C in prose or pseudocode, no URLs
  outside the bibliography and no code hosts in it, and no HTML that could hide anything.

Five phases. P1 lays the index, the bibliography, the convention and the Mersenne-numbers note. P2
writes Lucas–Lehmer and PRP, P3 the form of factors and P-1, and P4 the two transform notes. P5 is
the audit and the deferral ledger. The gates are frozen at this commit, take under a minute each,
and were proven on prototype notes (§ 4). There are no cluster runs, since no code changes (§ 6).

## 2. Design

### Files

| Path | Change | R-IDs |
|---|---|---|
| `doc/README.md` | new: the index | R1 |
| `doc/references.md` | new: the bibliography, with each entry's verification record | R5, R6 |
| `doc/mersenne-numbers.md` | new (P1) | R2, R3, R7 |
| `doc/lucas-lehmer.md` | new (P2) | R2, R4, R7 |
| `doc/prp-and-gerbicz.md` | new (P2) | R2, R7 |
| `doc/factors-and-trial-factoring.md` | new (P3) | R2, R7 |
| `doc/p-1.md` | new (P3) | R2, R7 |
| `doc/weighted-transforms.md` | new (P4) | R2, R7 |
| `doc/number-theoretic-transforms.md` | new (P4) | R2, R7 |
| `AGENTS.md` | one line in § Map, one bullet in § Conventions | R9 |
| `spec/math-notes-and-references/evidence/` | the records: `claims.txt`, `references.txt`, `cited.md`, `ledger.md` | R5, R7 |

Nothing else changes. The gates allow only `doc/`, `AGENTS.md`, this spec directory, `issues/` and
`ROADMAP.md` (the last two for deferrals) to differ from `61f5ce7`. The profile's `make check` must
pass with the base's counts (55 checks, 644 cli expectations). R1's file names are the table's.

### Every note

- **Title and sections.** One `# ` title, then exactly these five `## ` sections in this order:
  `## The result`, `## Proof`, `## What mps uses`, `## Sources`, `## Provenance` (R2). `### `
  subsections are free. No section is empty.
- **The result** states the theorem with every hypothesis.
- **Proof.** In full when short; otherwise a sketch that names where the full proof is published,
  by a located citation.
- **What mps uses** names the code in backticks and says what it does today. The gate requires:

  | Note | Must name | Must say |
  |---|---|---|
  | `mersenne-numbers.md` | `src/math.cpp`, `check_exponent`, `composite_by_exponent`, `smallest_prime_factor` | — |
  | `lucas-lehmer.md` | `src/math.cpp`, `test_lucas_lehmer` | "stub", "decides nothing" |
  | `prp-and-gerbicz.md` | — | "does not exist yet" |
  | `factors-and-trial-factoring.md` | `src/math.cpp`, `test_quick_factor` | "stub", "decides nothing" |
  | `p-1.md` | `src/math.cpp`, `test_p_minus_one` | "stub", "decides nothing" |
  | `weighted-transforms.md` | — | "does not exist yet" |
  | `number-theoretic-transforms.md` | — | "does not exist yet" |

  Every backticked `src/…` path must exist, and every backticked snake_case name with an underscore
  must be a function `src/` defines. Name planned code in plain words, not backticks. Inline code
  names code and never writes it: at most six words, each an identifier, a path, an option or a
  command word (`Result::prime`, `-t ll`, `make check`); no operators, braces or semicolons. The section
  says what the planned code will use, in the terms of the note's result.
- **Sources** lists every work the note cites, one citation per line, and the note cites nothing it
  doesn't list there.
- **Provenance** names the papers and documentation the algorithm is, or will be, built from, by
  citation only (R2, R8). It names no program's source, and holds no URL.

### Citations

- **Keys.** Each work has a key such as `CrandallFagin1994`, `OEIS-A000043` or `RFC3629`: a capital,
  then letters, digits and hyphens. Its entry in `doc/references.md` is the heading `### Key`, so
  GitHub gives it the anchor `#key`, lowercased.
- **The citation** is a link to that anchor: `[Key](references.md#key)`. A located citation puts the
  locator after a comma inside the brackets: `[Lehmer1930, Theorem 5.4](references.md#lehmer1930)`.
  A bracketed key that isn't such a link fails the gate.
- **The locator** is one or more parts joined by commas or "and", each one of:
  - § 1.3, p. 12 or pp. 12–14;
  - Theorem, Lemma, Corollary, Proposition, Definition, Section, Chapter, Equation, Table,
    Algorithm, Example, Exercise, Remark, Figure, Appendix, Step, Property or Fact (or eq., ch.,
    Thm., Prop., Sec., Fig., Alg.), followed by its number;
  - "the data section" or "the abstract".

  Free text is refused, so a locator can't carry a claim.
- **Locators.** A citation that attributes a specific statement to a work carries a locator: a
  theorem, section, equation, page or table. Every located citation gets a row in
  `evidence/cited.md`, saying where the builder confirmed it and what it says (§ 6).
- **Accessible copies only.** A builder confirms a locator in an accessible copy: the publisher's page
  or PDF, an open-access copy, arXiv, or the author's own page, never a pirate mirror. A statement that
  can't be confirmed in an accessible copy is cited to a work where it can be, or is derived in the
  note.

### `doc/references.md` (R5, R6)

It opens `# References`, gives a line on how entries are checked, then one entry per work:

```text
### Lehmer1930

- **Authors:** D. H. Lehmer
- **Title:** An extended theory of Lucas' functions
- **Venue:** Annals of Mathematics (2) 31(3), 419–448
- **Year:** 1930
- **DOI:** [10.2307/1968235](https://doi.org/10.2307/1968235)
- **Verified:** 2026-10-13 against the DOI's metadata at doi.org and JSTOR's landing page …: …
```

- **Fields.** Exactly these six, in this order. `DOI` is replaced by `URL: <https://…>` when the work
  has no DOI. Authors are written "Initials Family", joined by commas and "and". Venues are written in
  full, as the publisher gives them, with no abbreviations.
- **Year** is the print volume's year. Where the DOI's metadata carries only an earlier online date,
  the Verified line says so and names the print year's source (the gate looks for the word "print").
- **Verified** opens with the ISO date it was checked and says what it was checked against. Where the
  DOI's metadata lacks a field (no authors, no date), it names a second source by URL that shows it.
  That is R5's verification record. It lives beside the entry, where the blind reviewer can grade it.
- **Machine check.** `verify/refs.py` resolves every DOI at `https://doi.org` (CSL JSON by content
  negotiation, answered by Crossref and DataCite). It requires:
  - a paper, book, chapter, report or standard, never software or a dataset;
  - the same title, ignoring case, spacing and punctuation (a subtitle may stand apart in the
    metadata, or be absent from it for a book);
  - the same author family names, with consistent initials;
  - the year among the metadata's dates. The year after is accepted only when the metadata has no
    print date and Verified says "print".
  - at least half the significant words of the container in the venue (the publisher's, only when
    there is no container).

  **A field the metadata lacks** (authors, year, title) is checked in the second source the Verified
  line names by URL, which the gate fetches. Where that page carries citation metadata (Highwire or
  Dublin Core tags), the field must equal it; otherwise it must appear in the page.

  **A URL entry** must answer HTTP 200 and carry the entry's title. A title of three words or fewer
  must be in the page's `<title>`. Its authors and year must appear in the page.

  The findings go to `evidence/references.txt`, which the gate compares.

**The bibliography (R6).** The seeds' lists are the starting set. Each work is kept and cited by the
note named below, or dropped for the reason given. The gate checks the kept ones in P5.

| Work (seed) | Kept in | Or dropped because |
|---|---|---|
| Crandall and Pomerance 2005 (trial-factoring; `src/math.cpp`) | mersenne-numbers, factors, lucas-lehmer (the Jacobi symbol's laws) | — |
| Lehmer 1930; Lehmer 1935; Bruce 1993 (lucas-lehmer) | lucas-lehmer | — |
| GIMPS, "The Math" (lucas-lehmer, prp, trial-factoring) | lucas-lehmer, prp-and-gerbicz, factors | — |
| Li and Gallot 2022; Pietrzak 2019 (prp, prp-proofs) | prp-and-gerbicz | — |
| Wesolowski 2020 (prp-proofs) | — | The PRP note sketches only Pietrzak's halving proof, the scheme `issues/prp-proofs.md` plans; Wesolowski's construction is not planned. |
| Prime95 `undoc.txt`, `readme.txt` (prp, prp-proofs) | — | They document one program's settings, residue numbering and proof costs, which no note's mathematics needs; costs are a GOAL non-goal. |
| Pollard 1974; Montgomery 1987; Montgomery and Silverman 1990; Montgomery and Kruppa 2008 (pminus1) | p-1 | — |
| Möller 2008; Brent and Zimmermann 2010 (large-gcd) | lucas-lehmer (the Jacobi check's cost), p-1 (the GCD's cost) | — |
| Schönhage 1971; Stehlé and Zimmermann 2004 (large-gcd) | — | The notes state only the cost of a GCD or Jacobi symbol at large $p$. Möller 2008 presents and analyses Schönhage's algorithm, and Brent and Zimmermann 2010 gives the Jacobi bound. Choosing a GCD algorithm is `issues/large-gcd-and-jacobi.md`'s work. |
| Crandall and Fagin 1994; Percival 2003; Crandall, Mayer and Papadopoulos 2003; Sorensen et al. 1987 (ibdwt) | weighted-transforms | — |
| Pollard 1971; Reed and Truong 1975; Miller, Reed and Truong 1981; Harvey 2014 (ntt) | number-theoretic-transforms; Crandall and Fagin 1994 there too | — |
| Montgomery 1985; Barrett 1987 (trial-factoring, gpu-trial-factoring) | factors | — |
| mfaktc and mfakto READMEs (gpu-trial-factoring) | — | Reachable only as repository file views, which § Invariants 4 forbids opening; GPU trial factoring isn't one of the seven families. |

- **Additions.** RFC 3629 and RFC 8259, which `src/result.cpp` cites (§ 4). OEIS A000043, the
  published list of Mersenne prime exponents the checks compare with. A builder may add a work a note
  needs, such as Cole 1903 for the factors of $M_{67}$, with its own verification record, and lists it
  in `evidence/ledger.md` under "Added works".
- **Cited at least once.** Every entry is cited by a note or by a `src/` comment. The gate counts a
  `src/` comment's `(Author, "Title", …)` or `RFC NNNN` as a citation of the matching entry, matched
  by title, or by "RFC NNNN" in the entry's Venue.

### Claims (R7)

**Check spans.** A concrete number in a note stands inside a check span:

```text
<!-- check: (2**15 - 1) % (2**5 - 1) == 0 and 2**15 - 1 == 32767 --> $2^{15} - 1 = 32767$ is divisible by $2^5 - 1$<!-- /check -->
```

- **The expression** is a restricted Python expression: integer literals, `+ - * // % **`,
  comparisons, `and`/`or`/`not`, lists and tuples, and calls to the functions below. Powers are
  `**`, never `^`. There are no names, floats, strings or comments: only letters, digits, spaces and
  `+-*/%(),[]<>=!` may appear. It must evaluate to exactly `True`.
- **The tie between text and check.** Every integer the visible text shows must be a literal of the
  expression or the scalar value of one of its subexpressions. The elements of a list a function
  returns don't count: they must be written as literals. Every literal of the expression other than
  0 and 1 must be shown in the text. So a note that changes its number fails, and a check can't verify
  some other number.
  - **What counts as an integer:** decimal tokens; hex residues, 16 hex digits in either case or
    `0x…`; and Unicode digits, folded by NFKC. Digits inside a word (`Res64`, `B1`, `A000043`) are
    not integers.
  - **Signs:** a sign touching the digits after a space, an opening bracket, `=`, a comma or `$` is
    part of the number. `$J = -1$` shows $-1$ and must match a literal `-1`; `$2^p - 1$` shows 1.
  - **What doesn't count:** a one-digit subscript names a variable ($s_0$, $B_2$), and so do
    citation links' texts and every link's target. Math, inline code and fences all count.
  - **How to write a span.** Keep it to the one statement, with no blank line, and write numbers
    without separators. Leave a space between a marker and adjacent `$` math, so GitHub renders the
    math.
- **The functions** (`verify/claims.py`): `gcd`, `pow`, `len`, `isprime` (proved, never guessed: it
  fails rather than guess), `primes`, `factor`, `jacobi`, `order`, `mersenne`, `res64`, `lucas_seq`,
  `ll_residue`, `ll_exponents`, `jacobi_sweep`, `jacobi_first_zero`, `prp3`, `prp3_known`,
  `gerbicz_ok`, `gerbicz_detects`, `divides_mersenne`, `has_factor_form`, `factor_form`, `pm1`,
  `dwt_widths` and `dwt_product_ok`. Each checks its arguments' ranges and the facts it relies on.

**Cited spans.** A number a published work states, which the checks can't compute (a bound, a
constant, a record exponent), stands in `<!-- cited -->…<!-- /cited -->` with a located citation
inside.

**Nothing else.** An integer of absolute value 100 or more, or a hex residue, outside both kinds of
span fails the gate, in prose, math, inline code and fences alike. Smaller numbers outside spans are
the review's to grade (§ 6). No other HTML comment or tag, no link-reference definition and no HTML
entity may appear in `doc/`.

**Writing spans that pass.** Keep a span to the numbers it checks. For example, "for every odd prime
$p \le 521$, no iteration gives another symbol" can carry `jacobi_sweep(521) == []`. A sentence that
also says "$1 \le i \le p - 2$" and "$-1$" shows 1, 2 and $-1$, which that check doesn't write. A
standard's number ("IEEE 754") or a year in prose is a number too: rephrase, or cite it in a cited
span.

**R7's required checks.** Each note named here must hold a check span calling the function, at least
at the bound given (the function refuses a smaller one):

| Note | Call | What it checks |
|---|---|---|
| lucas-lehmer | `ll_exponents(B)`, $B \ge 2500$ | Lucas–Lehmer on every odd prime $p \le B$. The zero residues must be exactly OEIS A000043's terms up to $B$, with 2 from $M_2 = 3$. Each composite is confirmed again by a failed base-3 Fermat test. The span cites `OEIS-A000043`. |
| lucas-lehmer | `jacobi_sweep(B)`, $B \ge 521$ | $J(s_i - 2 \mid M_p)$ for every odd prime $p \le B$ and every $1 \le i \le p - 2$, unshifted and in three shifted runs ($k_0 = 1, (p-1)/2, p-1$, doubling mod $p$). Each case not $-1$ is returned, and the call fails unless every such case is 0, shifts agree, and R4's hypothesis excludes it. Equally, it fails if the hypothesis excludes a case whose symbol is $-1$. |
| lucas-lehmer | `jacobi_first_zero(B)`, $B \ge 25$ | The same over every odd $p \le B$, prime or not, so the zero case shows: the first $(p, i)$ with symbol 0. |
| prp-and-gerbicz | `prp3_known(B)`, $B \ge 11213$ | $3^{M_p - 1} \equiv 1$ for every published odd exponent $p \le B$: 22 to 11213. $M_2 = 3$ is the base itself and is skipped. The span cites `OEIS-A000043`. |
| factors-and-trial-factoring | `factor_form(B, Q)`, $B \ge 100$, $Q \ge 2^{20}$ | Every prime $q \le Q$ dividing $M_p$, odd prime $p \le B$, found by trying every prime, is $2kp + 1$ and $\pm 1 \bmod 8$. |
| factors-and-trial-factoring | `isprime(q)`, `divides_mersenne(q, p)`, `has_factor_form(q, p)` in one span, $q > 2^{20}$ | A published factor (Cole's $761838257287 \mid M_{67}$, say), with a located citation of the work that publishes it. |
| p-1 | `pm1(p, B1, B2)` returning a proper factor | One worked example end to end: stage 1 with $E = 2p \prod r^{e}$, $r^e \le B_1$, and base 3, then stage 2 over primes $B_1 < r \le B_2$. $M_{67}$ with $B_1 = 30$, $B_2 = 3000$ finds Cole's $193707721$ in stage 2 only. |
| mersenne-numbers | a check showing a number of 10 or more | At least one worked example of the theorem. |

The published exponents the gate compares with are OEIS A000043's first 26 terms, to 23209. They
are held in `verify/claims.py` beside their source and the date they were read, and no check reaches
past them.

**The record.** `notes.py report --write` writes `evidence/claims.txt`. It gives the command, and each
check span's file, line and expression, with what each required function found. The gate recomputes
it and requires the committed record to match, so it never goes stale.

### The notes' content

What each note states and cites. The builder writes the prose and the proofs; the gate holds the
structure and the numbers.

- **`mersenne-numbers.md` (R3).**
  - **The result:** $a \mid p \Rightarrow 2^a - 1 \mid 2^p - 1$, for integers $a, p \ge 1$, and the
    corollary that $M_p$ prime implies $p$ prime.
  - **Proof:** the geometric-sum factorization, in full.
  - **What mps uses:** the exponent check. `check_exponent` takes `smallest_prime_factor`'s $a$ for
    a composite $p$, and `composite_by_exponent` re-checks $2^p \equiv 1 \pmod{2^a - 1}$ exactly
    before reporting the factor $2^a - 1$. A prime $p$ says nothing, and the tests run.
  - **Sources:** Crandall and Pomerance (with a locator). The sieve and trial division `src/` uses
    may be mentioned with the same citation.
- **`lucas-lehmer.md` (R4).**
  - **The result:** for an odd prime $p$, $M_p$ is prime if and only if $s_{p-2} \equiv 0$, with
    $s_0 = 4$ and $s_{i+1} = s_i^2 - 2$. The test needs $p$ odd, and $M_2 = 3$ is prime by
    inspection.
  - **Proof:** a sketch citing Bruce 1993 and Lehmer 1930 and 1935, each with a locator the builder
    confirms. Which work proves which direction is the builder's to confirm in the work and state; the
    plan asserts neither.
  - **The Jacobi invariant**, `### The Jacobi invariant (the project's own derivation)` under Proof,
    derived in full. Its paragraphs open with `**Hypotheses.**` and
    `**What the symbol means for a run.**`. The derivation research brief 01 § 3 settled:
    - **The product.** For $i \ge 1$, $s_i - 2 = 12 \prod_{k=0}^{i-2} s_k^2$ as integers, from
      $s_{k+1} + 2 = s_k^2$.
    - **The residues.** For odd $p \ge 3$, $M_p \equiv 7 \pmod 8$ and $M_p \equiv 1 \pmod 3$.
    - **Hence** $J(s_i - 2 \mid M_p) = J(3 \mid M_p) \prod_k J(s_k \mid M_p)^2$. This is $-1$ when
      no $s_k$ with $k \le i - 2$ shares a factor with $M_p$, and 0 otherwise, never $+1$. $J(3 \mid
      M_p) = -1$ by reciprocity.
    - **The shift.** $J(2 \mid M_p) = 1$, so a shift $2^k$ changes nothing. The note says exactly
      what a shifted run computes: it stores $r = s_i 2^k$, so its symbol is
      $J(r - 2^{k+1} \mid M_p) = J(2^k (s_i - 2) \mid M_p)$, never $J(r - 2 \mid M_p)$, which is
      not $-1$ in general. That is the quantity the sweep checks.
    - **The hypotheses** are $p$ odd and $p \ge 3$; primality of $p$ isn't needed. The
      `**Hypotheses.**` paragraph states both conditions: $p$ odd, and the common-factor condition.
      For a composite $M_p$, a prime factor of $M_p$ dividing some $s_j$ turns the symbol to 0 from
      $i = j + 2$ on.
    - **For a run:** $+1$ is an arithmetic error, always. 0 is an error, or a genuine common factor:
      the run takes the GCD of the stored residue with $M_p$ and reports a factor only after the
      exact re-check (§ Invariants 1). $-1$ is consistent, and catches about half of random errors.
    - **The laws used** (reciprocity, the supplements, multiplicativity) are cited with a locator.
  - **The checks:** `ll_exponents`, `jacobi_sweep` and `jacobi_first_zero`, as above.
  - **The cost:** the cost of the symbol at large $p$ (about one GCD) may be noted, citing Möller and
    Brent–Zimmermann.
  - **What mps uses:** `test_lucas_lehmer` is a stub that decides nothing. The planned test will use
    the theorem, the shift and the Jacobi check.
- **`prp-and-gerbicz.md`.**
  - **The result:** Fermat's little theorem; the base-3 PRP test of $M_p$; why base 3 (base 2 is
    useless, since $2^{M_p - 1} \equiv 1$ for every prime $p$). It may also state the form
    $3^{2^p} \equiv 9$ GIMPS describes [GIMPSMath].
  - **The Gerbicz check:** with $d_{t+1} = d_t \cdot x_{(t+1)L}$, the identity
    $d_{t+1} = x_0 \cdot d_t^{2^L}$, proved in full, and what it detects.
  - **Proofs of PRP:** a short paragraph on Pietrzak's halving proof, citing Pietrzak 2019 and Li and
    Gallot 2022.
  - **The check:** `prp3_known`. `gerbicz_ok` and `gerbicz_detects` serve any worked example.
  - **What mps uses:** the PRP test does not exist yet.
- **`factors-and-trial-factoring.md`.**
  - **The result:** every prime factor $q$ of $M_p$, $p$ an odd prime, is $2kp + 1$ and
    $\pm 1 \bmod 8$. Proved in full: the order of 2 mod $q$ is $p$, and 2 is a quadratic residue
    mod $q$.
  - **Trial factoring:** sieving candidates $k$, and $2^p \bmod q$ by left-to-right square and
    multiply, with Montgomery and Barrett reduction named for later work. GIMPS's factoring limits are
    cited, never copied as a table.
  - **The checks:** `factor_form`, and the published factor.
  - **What mps uses:** `test_quick_factor` is a stub that decides nothing, beside the exponent check.
- **`p-1.md`.**
  - **The result:** if $q \mid N$ and $q - 1 \mid E$ then $q \mid \gcd(a^E - 1, N)$, from Fermat.
    For $M_p$, $q - 1 = 2kp$, so $E$ takes the factor $2p$.
  - **Stages:** stage 1 and the prime-by-prime stage 2, sketched with Pollard 1974. Faster stage 2s
    are cited, Montgomery 1987, Montgomery–Silverman 1990 and Montgomery–Kruppa 2008, without
    detail.
  - **The check:** `pm1`.
  - **What mps uses:** `test_p_minus_one` is a stub that decides nothing.
- **`weighted-transforms.md`.**
  - **The result:** squaring modulo $2^p - 1$ as a weighted cyclic convolution with the
    irrational-base digits $\lceil pj/N \rceil$ (Crandall–Fagin). `dwt_widths` and
    `dwt_product_ok` check its integer identity.
  - **Error bounds:** Percival's bound and the Crandall–Mayer–Papadopoulos heuristic, stated and
    cited, with constants in cited spans. Real-input transforms are cited to Sorensen et al.
  - **What mps uses:** the engine does not exist yet. `src/mp.cpp`'s `BigInt` is a stub and may be
    named.
- **`number-theoretic-transforms.md`.**
  - **The result:** a length-$N$ cyclic convolution over a field with a primitive $N$-th root of
    unity, exact (Pollard 1971). The choice of prime: $q - 1$ divisible by the length. Fields of
    characteristic a Mersenne prime (Reed–Truong, Miller–Reed–Truong). Harvey's lazy reduction.
    Weighting carries over from the weighted note.
  - **Checks:** `isprime`, `factor` and `order` serve the worked numbers, such as a prime
    $2^{64} - 2^{32} + 1$ and its root of unity.
  - **What mps uses:** the engine does not exist yet.

### The index (`doc/README.md`, R1)

- A `# ` title.
- One list line per note: `- [Title](file.md): what it covers`. Each existing note is listed exactly
  once, and no missing file is linked.
- A link to `references.md`.
- The clean-room rule in one sentence that links `../AGENTS.md` and names § Invariants 4, without
  restating it.
- A line saying each numeric claim carries the check that verified it in an HTML comment, and where
  this cycle's record defines the functions. It is said in words: the literal marker can't appear in
  the index.
- No numbers of 100 or more, and no claims.

### Math and Markdown (R1)

- **Delimiters.** Math is written only as `$…$` inline, on one line, and `$$…$$` displayed. `\(`,
  `\[` and ```` ```math ```` fail.
- **Nothing math-like outside them:** no backslash command, `^`, `_{` or `x_y` outside math, inline
  code or link targets. Code names go in backticks.
- **Fences.** The only fence is ```` ```text ````, for pseudocode, which assigns with `←` and never
  reads as C: no `;` line ends, braces, `x[i] =`, `->`, `::`, `++`, `//`, C types or keywords.
- **Prose** never reads as C either: `#include`, `->`, `::`, `/*`, `++` (except "C++"), `) {`, C
  types. Arrows are `→`.
- **No images, no raw HTML, no placeholder words** (`TODO`, `TBD`, `FIXME`, `XXX`).

### The clean room (R8)

The gates enforce what text can show; the process covers the rest.

- **Enforced:**
  - the fence and pseudocode rules above;
  - no address in any `doc/` file but `references.md`, whether a URL, a `www.` name or a bare
    domain. In `references.md`, no code host (GitHub, GitLab, SourceForge, Bitbucket, Codeberg,
    sr.ht), no source or archive file (`.c` … `.asm`, `.zip`, `.tar.gz` …), and no software or
    dataset DOI;
  - inline code that names code and never writes it, and prose with no C: casts, `unsigned`,
    compound assignments, `x = …;`, `for (…;`, `return …;` and the like;
  - in `references.md`, no fence, no line in an entry other than its six fields, and the same math
    and prose rules as the notes;
  - no "source code" and no unbackticked source-file name in a note;
  - any paragraph naming a Mersenne or big-number program (Prime95, mprime, Mlucas, gpuowl, PRPLL,
    CUDALucas, mfaktc, mfakto, GMP, GMP-ECM, gwnum, Glucas, LLR, PFGW, Genefer, y-cruncher) cites a
    reference;
  - no HTML comment but the two span markers, whose expressions admit only the grammar above.
- **Process:** builders and the planner never open another program's source (§ Invariants 4). A
  program's documentation is cited as documentation. Provenance names papers and documentation.

### `AGENTS.md` (R9)

- **§ Map.** After the `src/result.*` line, this line:

  ```text
  - `doc/`: the mathematics, one note per algorithm family, with `README.md` (the index) and
    `references.md` (the bibliography, each entry checked against the work).
  ```

- **§ Conventions.** After "Comments cite the paper or theorem…", this bullet:

  ```text
  - A cycle that implements or changes an algorithm updates that algorithm's note in `doc/` (its
    result, what `mps` uses, its provenance) in the same commit.
  ```

No other section changes; the gate compares every other section with `61f5ce7`, and old lines in
those two may not be lost or reordered. A bullet may wrap onto indented continuation lines, as
these do.

### Requirement → design map

| R-ID | Design element(s) that satisfy it |
|------|-----------------------------------|
| R1 | `doc/` holds exactly the index, the bibliography and the seven notes (gated per phase). The index lists each note once and cites § Invariants 4 by link. The math and Markdown rules are gated. |
| R2 | The five sections, exact and in order. "What mps uses" names the code and the stub or absent status. Sources cover every citation, and Provenance cites. |
| R3 | `mersenne-numbers.md`: the theorem, its proof, the corollary, and the exponent check naming `check_exponent` and `composite_by_exponent` (gated), plus a worked check. The review grades the proof. |
| R4 | `lucas-lehmer.md`: the marked derivation with its two labelled paragraphs (gated). `jacobi_sweep` and `jacobi_first_zero` check the invariant and its hypothesis both ways. The review grades the derivation. |
| R5 | Six fields per entry, the Verified record beside each, `refs.py` against doi.org or the page, and `evidence/references.txt`. |
| R6 | Every citation resolves; every entry is cited by a note or by `src/`; the `src/` works have entries; the starting set is kept or dropped as the table says (P5). |
| R7 | Check spans tied to their text; cited spans; no stray large number; the required checks and bounds; `evidence/claims.txt` recomputed by the gate. Independent Python, never `bin/mps`. |
| R8 | The enforced rules under "The clean room", and the process. |
| R9 | The two `AGENTS.md` lines; every other section held to `61f5ce7`. |

## 3. Invariant gate (AGENTS.md constitution check)

Walked against the GOAL's intent before the design (gate #1) and against this design after it
(gate #2). The sections touched:

- **High-blast-radius paths:** none touched. The design writes only `doc/`, `AGENTS.md` and this
  spec directory, and the gates hold every other path equal to `61f5ce7`. Trigger 2 doesn't fire.
- **§ 1 Verdicts are earned:** not touched; no code changes. The Lucas–Lehmer note describes a run's
  response to a Jacobi symbol in § 1's own terms: a factor is reported only after the exact re-check.
- **§ 2 Arithmetic is checked:** not touched.
- **§ 3 Known answers are fixed points:** not touched in `mps`. The gate's own published values
  (OEIS A000043's terms) carry their source and the date they were read, in § 3's spirit. Notes quote
  known values only as checked or cited examples (GOAL non-goal on the tables).
- **§ 4 Clean room:** the cycle's centre. Citations go to papers, books, RFCs, OEIS and program
  documentation only. The gates enforce what text can show (§ 2), and nobody opens a program's
  source. The mfaktc and mfakto READMEs are dropped because verifying them would mean opening a
  repository file view.
- **§ 5 Dependency-free core:** the checks are Python 3 with the standard library, live in
  `spec/…/verify/`, and never link into `bin/mps`.
- **§ Conventions:**
  - "Point to a rule; don't restate it": the index cites § Invariants 4 by link.
  - The new same-commit rule is stated once, in § Conventions.
  - No R#/P# ids in `src/` (untouched) or in `doc/`.

### Deviation justifications

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| Check and cited markers, as HTML comments, inside `doc/` notes | They tie each number to the computation that checks it, so a note that changes its claim fails its check (R7, the foreman's direction). They are invisible on GitHub. | A claims list kept in `spec/` that quotes the notes goes stale silently, and can't tell a changed number from a moved one. Plain prose with a separate script can't tie a check to the sentence it checks. |
| The checks' functions live in this cycle's `spec/…/verify/claims.py`, which the notes' markers name | R7 puts the checks in the cycle's record, outside `make check`. § Invariants 5 keeps outside tools with the tests or the record, never in `doc/`. | Moving the checks into `tests/` and `make check` would change the `Makefile` and `tests/`, a GOAL non-goal unless argued. A later cycle that edits a note evaluates its spans with `notes.py claims doc/<note>.md`, which needs nothing of this cycle's footprint, or writes its own check. The index says where the functions are. |

## 4. Rabbit holes (resolved)

The lean path applies: `appetite: small`, `kind: docs`, and trigger 2 doesn't fire. The cycle's
real research is its content, which the phases carry: verifying citations, and settling and checking
each statement. Plan-time probes, in [`research/01-citations-and-checks.md`](research/01-citations-and-checks.md),
settled what could blow the appetite:

- **Can a gate check references at all?** Yes. doi.org answers CSL JSON for Crossref and DataCite
  DOIs alike. The gate tolerates online-first years, books without authors, missing dates and odd
  spellings, and makes the builder name a second source wherever a field is missing. Two DOIs the
  planner wrote from memory resolved to unrelated papers, which the gate rejects. Crossref's search
  found the right ones.
- **`src/` cites more than the GOAL says.** `src/result.cpp` cites RFC 3629 and RFC 8259 beside
  Crandall–Pomerance in `src/math.cpp`. R6's wording ("the works `src/` comments cite today") covers
  them: they get entries, cited by `src/` alone. No R-ID bends; the GOAL's Problem undercounts.
- **The Jacobi invariant**, derived and checked (§ 2). The symbol is $-1$ or 0, never $+1$, for every
  odd $p \ge 3$. It is 0 exactly when an earlier $s_k$ shares a factor with $M_p$. That never happens
  for prime $p \le 521$, but it happens from $i = a$ whenever a prime $M_a$ divides $M_p$, which odd
  composite $p$ show. So the sweep reports no exception for primes. The required `jacobi_first_zero`
  shows the zero case and checks the hypothesis both ways, so the record isn't vacuous.
- **Bounds and time.** LL to 2500 in 4 s, the sweep to 521 in 4 s, PRP to 11213 in 5 s, the factor
  form in 1 s. 4500 for LL would take 39 s, so 2500 is the floor, not a target.
- **A worked P-1 example** exists with a published factor: $M_{67}$, Cole's $193707721$, found in
  stage 2 only.
- **`isprime` never guesses.** Below $10^{12}$ it uses trial division. Above, a Miller–Rabin witness
  proves compositeness, and primality is proved by Lucas's converse of Fermat on a fully factored
  $q - 1$. Otherwise it fails and the claim must be cited. No unverified constant enters.
- **The gates, proven.** On prototype notes for all seven families in a scratch clone, the final
  gates were green: P1 in 19 s, P2 in 34 s, P5 in 45 s. About 100 injected defects each failed: the
  66 of brief § 4, and the review's attacks of § 4.1, rerun after its fixes. One residual is
  recorded in § 5.
- **The adversarial design review (trigger 3) ran:** § 4.1.

### 4.1 Adversarial design review

A fresh reviewer (effort xhigh) had GOAL.md, this design and the gates. It attacked a copy of the
prototype, with every attack committed as a builder would and the records regenerated. It confirmed
the mathematics of every check function:

- Lucas–Lehmer, and `jacobi`;
- the shifted-run recurrence, and the two-way hypothesis test, which matches the
  $12 \prod s_k^2$ derivation;
- `isprime`, which never returns True for a composite;
- `factor_form`'s independence of the form, and its count of 27, which agrees with the published
  factorizations;
- `pm1`, the Gerbicz identity and the DWT identity;
- the 26 OEIS terms.

It found six ways a false or borrowed note passed green, and the design absorbed each:

- **H1. `prp3_known` counted $p = 2$ untested** (and $M_2 = 3$ fails base 3). It now skips and
  doesn't count it: 22 to 11213.
- **H2. The elements of a list a function returned counted as computed,** so
  `len(primes(1000)) == 168` let the text claim 523. Only scalar values count now.
- **H3. Signs weren't tied:** $J = +1$ beside a check of $-1$ passed. Signs are now part of the
  number in both the text and the expression.
- **H4. Numbers in fences and inline code escaped the spans.** The stray-number rule and the tie now
  read them.
- **H5. Code passed the clean-room rules:** a C loop in inline code, C statements in prose, and
  anything at all in `references.md`. Inline code now names code only; `C_PROSE` gained casts,
  `unsigned`, compound assignments, `x = …;`, `for (…;` and `return …;`. `references.md` gets the
  fence, prose and math rules, and its entries hold only their fields.
- **H6. `refs.py` was too tolerant:**
  - a field the metadata lacks rested on any URL in the Verified line, unfetched;
  - URL entries were matched on the title anywhere in the page;
  - the publisher counted as a venue.

  Second sources are now fetched and matched against the page's citation metadata, or the page's
  text. A short URL-entry title must be in the page's `<title>`. The publisher stands for the venue
  only without a container.

And six MEDIUM:

- **M1. Hidden text:** a `#` comment inside a check, link-reference definitions and HTML entities.
  The expression's characters are whitelisted; the other two are refused.
- **M2. `refs.py` gaps:** software DOIs, a print-year tolerance that applied when the metadata
  already had the print year, and initials never checked. All three are fixed.
- **M3. R4's shift was ambiguous,** and the Hypotheses paragraph could omit the common-factor
  condition. § 2 now pins both, and § 6 names them as review items.
- **M4. A false red:** R9's bullet as printed here wraps, and failed. Continuation lines now join.
- **M5. The deviation table overclaimed** that a later cycle could rerun `notes.py`. A `claims`
  mode now evaluates one note's spans alone.
- **M6. The freeze check trusted the commit subject.** It now fails when the plan commit follows a
  build commit, or any later commit touches `verify/`.

**From the LOW list**, the design took:

- lowercase and `0x` hex, Unicode digits and entities, all now numbers;
- a locator grammar;
- a refusal of bare `www.` and domain names in notes;
- a worked example in the Mersenne note, which must now show a number of at least 10;
- `OEIS-A000043` required in the two published-list spans;
- the `\\[2pt]` false red, fixed;
- the advice on narrow spans (§ 2).

**Left to the review,** and said so in § 5 and § 6:

- one-digit subscripts;
- prose and wording games ("no longer a stub … decides nothing");
- what a cited span says;
- small numbers;
- paraphrase in prose or pseudocode.

## 5. Risks & open questions

- **The gates can't judge truth in words.** A check ties numbers, not prose: "193707721 is prime"
  beside a check that it is a factor passes the gate. Proofs, the derivation's completeness, whether
  a cited work says what the note attributes to it, and small numbers outside spans are graded by the
  blind review (§ 6), with `evidence/cited.md` as the orchestrator's record.
- **Presence is weaker than metadata.** A second source without citation metadata (Springer's book
  pages), and a URL entry, are matched by presence: each author and the year must appear somewhere in
  the page. The review demonstrated the residual: OEIS dated 1999 passes, because "1999" is on the
  page. There are two URL entries (GIMPS, OEIS) and one such book. The orchestrator reads their
  Verified lines and `references.txt` rows against the pages.
- **One-digit subscripts aren't tied** ("$s_3 = 194$" beside a check of $s_2$ passes), because they
  name variables. The review grades them.
- **The network.** `refs.py` needs doi.org and the URL entries' hosts. An outage fails a gate; rerun
  it later. Metadata can change: a changed record fails the comparison with
  `evidence/references.txt`, and the builder looks before rewriting it.
- **Crossref can be wrong.** The gate compares against it, so a metadata error forces either a
  matching entry or an escalation. The builder records a disagreement in Verified, with the
  publisher's page as the second source. A gate that refuses a correct entry is a stop-and-escalate,
  never an edit.
- **Paywalls.** Some works (IEEE, Springer chapters, JSTOR) may not be readable in full. A located
  citation needs an accessible copy, so such a work is cited for what its abstract or an accessible
  copy shows, or the statement moves to an accessible work.
- **The markers bind future edits.** A later cycle that edits a note keeps or replaces the markers.
  The R9 convention requires the update, not the checks. The index says where the functions are.
- **GitHub rendering** of math next to a comment marker is not gated. Builders preview a note's
  rendering where they can and leave a space between marker and math.

## 6. Verification strategy

There is no throwaway environment, and none is needed: the cycle runs no `mps` command.

**The gates** (`verify/`, frozen at this commit; each refuses to run if `verify/` differs from it).
`sh spec/math-notes-and-references/verify/p<N>.sh` runs, in order, and stops at the first failure
with `GATE FAIL: <reason>`, or ends `P<N> gate: green`:

1. **The freeze check:** `verify/` equals the last plan commit, no build commit precedes that
   commit, and no later commit touches `verify/`.
2. **`notes.py gate N`:**
   - `claims.py`'s self-test: `isprime` against a sieve and known composites, `jacobi` against
     Euler's criterion and multiplicativity, the sweep's zero case on $M_9$, P-1 on $M_{67}$, the
     Gerbicz check, the DWT identity, and the evaluator's refusals;
   - the footprint and `AGENTS.md` (R9);
   - `doc/` holding exactly the phase's files;
   - placeholders;
   - `references.md`'s format and URLs;
   - each note's sections, uses, citations, links, math and Markdown, clean-room rules, check and
     cited spans, and required checks;
   - the index;
   - R6's citation accounting (every entry cited by a note or `src/`; `src/` works present; from P5
     the starting set);
   - `evidence/cited.md` against the located citations;
   - `evidence/claims.txt` against the recomputed record.
3. **`refs.py`:** every entry against doi.org or its page; `evidence/references.txt` against the
   result.
4. **`make check`** on a copy of the tree: 55 checks and 644 cli expectations passed, as on the base.

P5 adds the starting set and the ledger: every `issues/*.md` indexed exactly once in `ROADMAP.md`,
this cycle's seed still adopted, and `evidence/ledger.md` with its `## Deferrals` section.

**Computed, and left to the review:**

| Claim | The gate (computed) | The review (judged) |
|---|---|---|
| A number in a note | its check is True, and the text shows exactly its numbers, signed | the words around it say what the check checks; numbers below 100 outside spans; one-digit subscripts |
| R7's five checks | run at their bounds, against independent methods and the published list | — |
| A cited number or statement | a located citation in a cited span; a row in `cited.md` | the work says it, at that locator |
| A reference | the DOI is a paper, book, chapter or standard and resolves to that title, those authors and initials, that year, a matching venue; missing fields match the second source's citation metadata, or appear in it; a URL entry carries its title, authors and year | where only presence was checked (URL entries; second sources without metadata), that the page really gives that author and year: the orchestrator, from Verified and `references.txt` |
| A proof or derivation | its sections and labels exist | sound and complete; the Hypotheses paragraph names $p$ odd and the common-factor condition; the shifted symbol is $J(r - 2^{k+1} \mid M_p)$ |
| R2's uses section | names the code, which exists, and says stub or absent | accurate about what the code does, with no wording that says the phrase and means its opposite |
| R8 | no code (fences, inline code, prose), address, host, file name, software DOI or hidden text | no paraphrase of a program's source, in prose or in pseudocode |
| R9 | the two lines, nothing else changed | wording |

**Graded by the orchestrator:** the records in `spec/math-notes-and-references/evidence/`, which the
blind reviewer cannot see: `claims.txt` (R7's commands and outputs), `references.txt` (R5's machine
check), `cited.md`, `ledger.md`. Each check span's expression is also visible in `doc/` to the blind
reviewer.

**Run times** (this Mac, prototype notes, final gates): P1 19 s, P2 34 s, P5 45 s; P3 and P4
fall between. `make check` is 14 s of each, and the network check 10–20 s.

**No cluster runs.** The cycle changes no code, so a cluster job would build and check the same
commit `main` already passed on all three targets; its evidence couldn't differ. P5 is the audit and
the ledger, with no push. Landing is `code-publish`'s.

---

*Backing research: [`research/00-digest.md`](research/00-digest.md).*
