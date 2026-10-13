# Deferral ledger for math-notes-and-references

Written at P5, 2026-10-12, from a walk of `TECH.md` P1–P5 (the checklists and their amendments),
the build commits `209a3d2`, `c8f476f`, `92e76d6`, `592e7cd` and `68e42a7`, and PLAN § 2 and § 5.
Rewalked after review cycle 1 (`REVIEW.md`, F1–F4) over the commits `37404dd` and `6f704d4`, on
2026-10-12, and after review cycle 2 (G1, G2) over `3fb59ca`, on 2026-10-12.
Each deferral names its home: a later algorithm's open question goes in that algorithm's seed, and
work no seed covers gets a seed of its own and a `ROADMAP.md` entry.

## Deferrals

- **The exhaustive search over NTT primes below $2^{32}$.** `doc/number-theoretic-transforms.md`
  states Lemma 1's criterion and makes no claim about the other primes (P4 amendment; since review
  cycle 1 it no longer names the seed's search): no check function can run it inside a gate. Home: `issues/ntt-transform-engine.md`, Notes, which now
  asks that cycle to rerun the search as a recorded check, by the note's Lemma 1, before relying on
  it. The same bullet records that the note's exact bound for the $M_{31} \times M_{61}$ pair stops
  short of the seed's "34.5 bits per word exactly", and that Pollard 1971, Reed–Truong 1975 and
  Miller–Reed–Truong 1981 were read only in their abstracts.
- **The IBDWT error bounds that are not theorems.** `doc/weighted-transforms.md` states Crandall
  and Fagin's error growth as their conjecture and the Crandall–Mayer–Papadopoulos sizing as a
  heuristic (P4 amendment); only Percival's bound is proved, and it does not reach the lengths
  practice uses. Which estimate to size by, and the measured rounding error against it, is the
  engine's question. Home: `issues/ibdwt-transform-engine.md`, Notes, with Sorensen et al. 1987,
  read only in its abstract and cited without a locator, to be read before the engine builds on it.
- **Pollard 1974, unread.** Paywalled with no open copy (P3 amendment); `doc/p-1.md` cites it
  without a locator as the origin of the method and attributes no stage 2 to it. Home:
  `issues/pminus1-factoring.md`, Notes: read it before attributing or building a stage 2 from it, and
  read the faster stage 2s in full, which the note cites from their abstracts.
- **Möller 2008, unread.** Paywalled (added in P2, its Verified line says so); `doc/lucas-lehmer.md` and `doc/p-1.md` cite it
  without a locator for subquadratic GCD. Home: `issues/large-gcd-and-jacobi.md`, Notes: that cycle
  builds its GCD from this paper, so it reads it and adds a located citation to both notes.
- **The Gerbicz check's non-invertible blind spot** (review cycle 1, F1; restated at review cycle 2,
  G1; 2026-10-12). `doc/prp-and-gerbicz.md` Theorem 5 proves that, with checks spaced, a residue
  that becomes zero passes every later check, and so does one that is zero modulo a factor of $M_p$
  and still correct modulo its cofactor (prime to that factor); either is caught only by the check on
  the block that made it. Wrong modulo the cofactor instead, the error shows there and a single
  wrong block is caught by the next check. The planned PRP test must treat a residue that is zero or
  not invertible as an error at every check, or check every block; designing that guard is the
  test's work. Home: `issues/prp-gerbicz-test.md`, Notes, which also flags that its R3 and its
  blind-spot list omit the case, and since G1 states the two kinds as above.
- **A PRP proof modulo $M_p$ is not sound as the note's recursion stands** (review cycle 2, G2,
  2026-10-12). `doc/prp-and-gerbicz.md`, "The sign of $\mu$", proves that modulo $M_p$, where $-1$
  has order 2 and the low order assumption is therefore false, a prover who sends $-\mu$ has a false
  claim accepted with probability $1 - 2^{-h}$ over $h$ halvings. The remedy, a group where squaring
  is one-to-one (as Pietrzak's signed quadratic residues) or a fixed sign, is the PRP-proof cycle's
  to design and prove. Home: `issues/prp-proofs.md`, Notes. No new seed, no `ROADMAP.md` change.
- **No standing check of the notes.** The check spans, the citation rules and R6's accounting are
  enforced only by this cycle's gates under `verify/`, which become records when it merges; PLAN § 5
  ("The markers bind future edits") leaves the checks to no one, and `doc/README.md` points at
  `verify/claims.py` inside the cycle record. Home: a new seed, `issues/doc-claims-standing-check.md`,
  with its `ROADMAP.md` entry in Part I, "A standing check for the notes' numbers and citations".

Paywalled works cited without a locator that need no later reading:

- **Lehmer 1935 and Bruce 1993** (P2 amendment). `doc/lucas-lehmer.md` proves the theorem in full
  and locates it in Lehmer 1930; these two are cited only as shorter published proofs, for what their
  titles state.
- **Barrett 1987** (P3 amendment). The reduction is cited with locators to
  `BrentZimmermannMCA2010`, read in the authors' version, which the trial-factoring cycles can build
  from.

## Added works

The seeds' starting set, as PLAN § 2's table keeps it, is in `doc/references.md` in full: 23 works,
with Crandall and Pomerance 2005 among them. PLAN § 2 dropped Wesolowski 2020, Prime95's `undoc.txt`
and `readme.txt`, Schönhage 1971, Stehlé and Zimmermann 2004, and the mfaktc and mfakto READMEs, each
for the reason given there. Five works are added beyond the starting set:

- `RFC3629` and `RFC8259` (P1): `src/result.cpp` cites them; PLAN § 2 names them as additions.
- `OEIS-A000043` (P1, used from P2): the published Mersenne prime exponents the checks compare with;
  PLAN § 2 names it as an addition.
- `Cole1903` (P3): the factors of $M_{67}$ and the form of factors as already known in 1903, read in
  full in Project Euclid's copy.
- `BrentZimmermannMCA2010` (P3): Barrett's and Montgomery's reductions and their attribution, read
  in the authors' version, since Barrett 1987 is open only as an abstract that does not describe the
  method.
