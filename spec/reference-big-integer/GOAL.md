# GOAL — A reference big integer to check every engine against

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** reference-big-integer
- **kind:** feature
- **appetite:** big

## Problem

`mps` has no arithmetic on numbers wider than 64 bits. All of this is pre-existing on `main`
(`c9b82ff`).

- `BigInt` (`src/mp.hpp:19-31`, `src/mp.cpp`) keeps decimal digits in a `std::vector<uint16_t>` and
  can only load and print them. Nothing uses it: `src/io.hpp:19`, `src/math.hpp:16`,
  `src/math.cpp:18` and `src/main.cpp:18` include `mp.hpp`, and no file outside `src/mp.*` names
  `BigInt`.
- Every test needs arithmetic modulo M_p = 2^p − 1 on p-bit numbers: Lucas–Lehmer, PRP, the P-1
  stages, and re-checking a trial-factoring hit. `test_lucas_lehmer` is a stub that returns no
  verdict (`src/math.cpp:88-95`), and there is no PRP test.
- The fast engines that come later (`issues/ibdwt-transform-engine.md`, and the integer and GPU
  engines after it) are floating-point or modular, with weights and carries. They fail in ways that
  still produce plausible residues. § Invariants 2 requires every engine to be tested against the
  reference multiple-precision arithmetic before it may produce a verdict, so the reference has to
  exist first, and it has to be simple enough to be obviously right.
- The author's uncommitted 2024 Lucas–Lehmer test reached for GMP (`mpz_class`, from a Rosetta Code
  snippet). It doesn't compile, since GMP is neither included nor linked, and it can't land: GMP
  conflicts with § Invariants 4 and 5, and Rosetta Code's content is under the GNU FDL, which is
  incompatible with the GPL (§ Invariants 4).

## Outcome / vision

`BigInt` becomes a binary-limb, non-negative integer written for clarity first: 64-bit limbs, with
`unsigned __int128` for limb products. It replaces the decimal-digit stub and keeps its name. It
provides comparison, addition, subtraction and shifts; schoolbook and Karatsuba multiplication and
squaring; reduction modulo M_p by folding (x mod M_p = (x & M_p) + (x >> p), repeated); general
reduction modulo any positive modulus, and modular exponentiation; conversion to and from decimal
and hexadecimal strings; and the 64-bit residue (Res64) in the form `src/result.cpp` already prints.

On top of it sit a reference Lucas–Lehmer test and a reference base-3 PRP test for small exponents.
They return residues, run in `make check`, and are the known answer the Lucas–Lehmer and PRP cycles
and every engine are graded against. They are not wired into `mps -t …` or the verdict path.

Everything is checked against Python's arbitrary-precision integers, through vectors that Python
generated once and that are committed with the command that produced them. `make check` reads the
vectors and never runs Python. `doc/` gains the mathematics of the reference arithmetic.

## Acceptance criteria (the contract)

Terms used below:

- **B** is the exponent bound of the reference tests. The plan fixes it, aiming at 10,000, as high
  as R10's budget allows.
- **The vectors** are the expected values Python's arbitrary-precision integers computed, committed
  under `tests/` with the exact command that produced them (R8).
- **The prime list** is the known Mersenne prime exponents up to B, with its source per
  § Invariants 3 ([OEIS-A000043](../../doc/references.md#oeis-a000043) is already in the
  bibliography).

The criteria:

- **R1** — For every vector of random operands up to about 2^20 bits, `BigInt`'s comparison, sum,
  difference, left and right shifts, product and square SHALL equal the vector. Products and
  squares SHALL be checked by schoolbook and by Karatsuba separately. The plan fixes the operand
  sizes, and they SHALL include sizes on both sides of the Karatsuba threshold, at and across limb
  boundaries, and shift counts of 0 and multiples of 64.
- **R2** — Reduction modulo M_p by folding, general reduction modulo a positive modulus m, and
  modular exponentiation SHALL equal the vectors. Each result SHALL be fully reduced: in [0, M_p)
  or [0, m). Folding SHALL be checked on inputs up to (M_p − 1)^2 and on the edge values 0, M_p,
  2^p and 2^p · M_p, and general reduction with m = 1.
- **R3** — For every vector operand and edge value (0, 1, 2^64 − 1, 2^64, and M_p for the vectors'
  exponents), conversion to decimal and to hexadecimal SHALL equal the vector's strings, and
  parsing each string SHALL give back the same `BigInt`. Parsing the canonical string of a value
  and printing it SHALL give back the same string; the plan defines the canonical forms.
- **R4** — Res64 of a value x modulo M_p SHALL be the low 64 bits of x mod M_p, the fully reduced
  residue in [0, M_p), printed as 16 uppercase, zero-padded hexadecimal digits, the form
  `src/result.cpp` uses for `res64` (`hex64`, `src/result.cpp:98`). It SHALL equal the vectors,
  and SHALL be `0000000000000000` for x = M_p and for x = 0. Exponents p < 64 SHALL be among those
  checked.
- **R5** — IF a subtraction would go below zero, a modulus is zero, a string is not a number in its
  base, or a reference test is given an exponent outside its theorem's domain (odd p ≥ 3:
  `doc/lucas-lehmer.md`, "The hypotheses"; `doc/prp-and-gerbicz.md`, Theorem 2), THEN the call
  SHALL report an error and SHALL NOT return a value.
- **R6** — For every exponent p in the prime list with 3 ≤ p ≤ B, the reference Lucas–Lehmer test
  SHALL end in s_{p−2} mod M_p = 0. For every exponent in a set of prime p ≤ B with M_p composite,
  fixed by the plan, spread over the range and committed with the vectors, its full final residue
  SHALL be nonzero and equal the vector.
- **R7** — For every exponent p in the prime list with 3 ≤ p ≤ B, the reference base-3 PRP test
  SHALL end in the residue that marks a probable prime. For every exponent in R6's composite set,
  its full final residue SHALL equal the vector. The residue it returns is either
  x_p = 3^(2^p) mod M_p (Theorem 2 of `doc/prp-and-gerbicz.md`, 9 for a probable prime) or
  GIMPS's type-1 residue 3^(M_p − 1) mod M_p (1 for a probable prime); the plan picks one, and the
  PRP note states which.
- **R8** — The prime list SHALL carry its source, and every vector file the exact command that
  produced it (§ Invariants 3). Running that command SHALL reproduce the committed file byte for
  byte.
- **R9** — `make check` SHALL read the committed vectors and SHALL NOT run Python or any tool beyond
  what it uses today. `bin/mps` SHALL link no new library (§ Invariants 5), and its command-line
  behaviour SHALL be unchanged: `tests/cli.sh` passes without edits.
- **R10** — `make check`, run with nothing to rebuild, SHALL take no more than a few seconds longer
  than at `c9b82ff` on the same machine. The plan fixes the budget in seconds and records the
  baseline it measured (0.85 s on an Apple-silicon Mac at shaping, 2026-10-12).
- **R11** — `make check` SHALL carry a check for each of R1–R7, and for R8's source and command
  being present with the prime list and every vector file, and SHALL exit non-zero when any of them
  fails. What `make check` cannot observe of itself (R8's reproduction, R9, R10, R12) the plan's
  gates verify and record.
- **R12** — In the commit that implements it, `doc/` SHALL gain the mathematics of the reference
  arithmetic, as a new note or as sections of existing notes (the plan decides which): folding
  modulo M_p and why it terminates fully reduced, Karatsuba multiplication and squaring, and the
  Res64 definition. Each SHALL state its result, what `mps` uses, its sources (in
  `doc/references.md`, each entry checked against the work) and its provenance. The "What mps uses"
  sections of `doc/lucas-lehmer.md` and `doc/prp-and-gerbicz.md` SHALL name the reference tests.
  Every concrete number the new text states SHALL sit between check or cited markers, as
  `doc/README.md` requires, and each check SHALL evaluate true.

## Non-goals (no-gos)

- Wiring the reference tests or `BigInt` into the command line or the verdicts: `mps -t …`,
  `run_in_order` and `Result` are untouched, and § Invariants 1's driver with them. The
  Lucas–Lehmer and PRP cycles of Part II do that, with their error checks and checkpoints.
- The transform engines and their balanced variable-base digit form. The IBDWT cycle defines that
  form and its round-trip with `BigInt` (`issues/ibdwt-transform-engine.md`).
- Performance work beyond what R10 needs.
- Signed integers, unless the plan shows that a check needs them.
- The published GIMPS residues, and a table of known answers beyond the prime list: the known-answer
  cycle adds them and cross-checks them against this type (`issues/known-answer-tables.md`).
- Printing the exponent check's factor in decimal. It stays symbolic, `2^a-1`, since a change would
  change the record format.
- GMP or any other big-number library, linked or consulted, as code or as an oracle
  (§ Invariants 4 and 5).
- The Jacobi symbol, GCD and modular inverse, which the Part II tests and
  `issues/large-gcd-and-jacobi.md` need, unless the plan shows a check here needs one.

## Clarifications

Geoffrey delegated every gate in this effort to the foreman on 2026-10-11. Each answer below is the
foreman's decision under that delegation, confirmed against the code and the seeds at `c9b82ff`.

- **Q:** One cycle, or a pilot and follow-ups? — **A:** One cycle, appetite big, as the seed says.
  R1–R12 exceed the size circuit-breaker's 8–10; the split was considered and declined (resolved
  2026-10-12).
- **Q:** What is the type? — **A:** A binary-limb, non-negative integer written for clarity first,
  with 64-bit limbs and `unsigned __int128` for products, which GCC and clang support on every
  target in view (Apple clang; GCC 14 on the cluster targets in `ci/targets/`). The class keeps the
  name `BigInt`, and the decimal-digit stub is replaced, since nothing uses it (resolved 2026-10-12).
- **Q:** What does it provide? — **A:** Comparison, addition, subtraction and shifts; schoolbook and
  Karatsuba multiplication and squaring; reduction modulo 2^p − 1 by folding; general reduction
  modulo any positive modulus, and modular exponentiation (to re-check a trial-factoring hit and to
  run small PRP tests); conversion to and from decimal and hexadecimal strings; Res64, the low 64
  bits of the fully reduced residue in [0, M_p), in the 16-uppercase-hex form `src/result.cpp`
  uses (resolved 2026-10-12).
- **Q:** What runs on top of it? — **A:** A reference Lucas–Lehmer test and a reference base-3 PRP
  test, returning residues, for small exponents. `make check` exercises them; they are not wired
  into `mps -t …` or the verdict path, and their speed is whatever clarity allows within R10
  (resolved 2026-10-12).
- **Q:** What are the checks, and what is the independent method? — **A:** Known Mersenne prime
  exponents up to a bound the plan fixes, near 10,000 if the time allows, end in a zero residue;
  composite exponents in that range reproduce independently computed residues; products, squares
  and reductions of random operands up to about 2^20 bits equal independently computed vectors;
  every conversion round-trips; Res64 is exact. The independent method is Python's
  arbitrary-precision integers, used only to generate vectors, committed under `tests/` with the
  exact command; `make check` never runs Python (§ Invariants 5). The published GIMPS residues are
  the known-answer cycle's (resolved 2026-10-12).
- **Q:** What is out of scope? — **A:** Everything in Non-goals but the last bullet, which shaping
  added to hold the operations to the list above (resolved 2026-10-12).
- **Q:** Where does the mathematics go? — **A:** `doc/` gains a note on the reference arithmetic,
  or existing notes gain sections, in the commit that implements it, with provenance; the plan
  decides which (resolved 2026-10-12).
- **Q:** Do the reference tests cover p = 2? — **A:** No. Confirming the answer on the checks
  against `doc/lucas-lehmer.md` ("The hypotheses") and `doc/prp-and-gerbicz.md` (Theorem 2): both
  theorems need an odd p ≥ 3. s_0 = 4 is not divisible by M_2 = 3, and 3 is the base itself, so
  M_2 is prime by inspection. R6 and R7 start at p = 3, and R5 makes p outside the domain an error.
  A refinement from the notes, not a change of scope (shaping, 2026-10-12).
- **Q:** Which residue does the reference PRP test return? — **A:** Either x_p or the type-1
  residue, the plan's choice, stated in the PRP note (R7). It is not material: each is one
  multiplication from the other, and the type's modular exponentiation computes either directly.
  The type-1 residue is what GIMPS publishes and what `issues/prp-gerbicz-test.md` plans to
  report, so it would make the known-answer cycle's cross-check direct (shaping, 2026-10-12).
- **Q:** Where the seed and these answers differ, which holds? — **A:** The answers. The seed's
  transform digit form (its R4) moved to the IBDWT cycle; its oracle "Python's integers or GMP"
  became Python only; its `test/` is `tests/`, the directory that exists; its R2 against the
  known-answer tables became vectors computed here, since that cycle comes after this one
  (shaping, 2026-10-12).

## Related materials

- Seed: `issues/reference-big-integer.md`; `ROADMAP.md`, Part I, "A reference big integer to check
  every engine against".
- Downstream seeds that grade against this type: `issues/known-answer-tables.md`,
  `issues/ibdwt-transform-engine.md`, `issues/lucas-lehmer-test.md`,
  `issues/prp-gerbicz-test.md`.
- Source: `src/mp.hpp`, `src/mp.cpp` (the stub), `src/result.cpp` (`hex64`, the Res64 form),
  `src/math.cpp` (the stub tests and the exponent check), `Makefile` (the `check` target).
- Notes: `doc/mersenne-numbers.md`, `doc/lucas-lehmer.md`, `doc/prp-and-gerbicz.md`,
  `doc/references.md`, `doc/README.md` (the claim markers).

## Notes

- **Human gate.** `src/mp.*` is on the high-blast-radius list (§ Invariants): a CONFIRMED review
  finding there needs Geoffrey's sign-off before the cycle lands, and the plan runs research.
- **New high-blast-radius paths.** The vectors and the prime list are expected values under
  § Invariants 3, so they are append-only, and the commit that creates them adds their paths to the
  high-blast-radius list ("a table of known answers adds its paths here in the same commit"). If
  the reference tests land in `src/` rather than `tests/`, their paths join it too. The plan
  decides where they live; `src/math.hpp` is coupled core.
- **One prime list.** `issues/known-answer-tables.md` (its R3) wants a single table that every test
  reads the prime exponents from. That cycle comes after this one, so this cycle holds the list,
  with its source, where the known-answer cycle can take it over.
- **Vector size.** Operands of 2^20 bits are about 256 KB each in hexadecimal. How many vectors at
  that size, and whether some are committed as a deterministic generator plus digests, is the
  plan's to bound.
- **One Res64 form.** `hex64` is file-local in `src/result.cpp`. R4 asks for the same form, and
  § Conventions says to point to a rule rather than restate it; the plan decides how the two share
  it.
- **Citations.** Karatsuba has no entry in `doc/references.md` yet. The claim checks evaluate with
  `notes.py claims` in `spec/math-notes-and-references/verify/`, a landed cycle's record;
  `issues/doc-claims-standing-check.md` moves that evaluator out later.
- Clean room (§ Invariants 4): the algorithms come from the mathematics and the published papers,
  cited where they are used. No big-number or Mersenne program's source is opened.
