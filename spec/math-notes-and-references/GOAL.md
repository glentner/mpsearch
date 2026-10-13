# GOAL — The mathematics, written down: notes and references

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** math-notes-and-references
- **kind:** docs
- **appetite:** small

## Problem

`mps` has no written mathematics. The author's uncommitted 2024 `src/math.cpp` carries the TODO
"Add comprehensive listing of notes and references: 1) Math proofs 2) Algorithms and methods", and
nothing on `main` (`61f5ce7`) answers it: there is no `doc/`, and the only citation in the code is
Crandall and Pomerance's *Prime Numbers* (2005), in two comments in `src/math.cpp` (the sieve and
`smallest_prime_factor`).

The clean-room rule (`AGENTS.md` § Invariants 4) makes this load-bearing. Every algorithm in `mps`
must come from the mathematics and the published papers, never from another program's source. The
cycles ahead (the reference big integer, the weighted transform, Lucas–Lehmer, PRP, trial
factoring, P-1, the integer transform) each need one place where the result they implement is
stated, its proof given or cited, and its papers listed with checked citations, so a builder starts
from the paper. Today each seed in `issues/` carries its own partial reference list, and one result
the Lucas–Lehmer check depends on, the Jacobi invariant, has no citable published statement at all
(`issues/lucas-lehmer-test.md`).

What exists today is small: the exponent check (`check_exponent`, `composite_by_exponent` in
`src/math.cpp`), which reports M_p composite by the factor 2^a − 1 when a divides p, and three test
stubs (`test_quick_factor`, `test_p_minus_one`, `test_lucas_lehmer`) that return no verdict.

## Outcome / vision

A `doc/` folder, written in GitHub-flavoured Markdown with LaTeX math, holds an index, one short
note per algorithm family, and a bibliography. Each note states its result precisely, proves it or
sketches and cites the proof, says what `mps` uses or will use and where, and records its
provenance. Results with no published statement are derived in full and marked as the project's
own. Every citation resolves to a bibliography entry that was checked against the work itself, and
every numeric claim was checked by computation or traced to a published statement. From this cycle
on, a cycle that implements or changes an algorithm updates that algorithm's note in the same
commit.

## Acceptance criteria (the contract)

- **R1** — `doc/` SHALL hold `README.md`, `references.md`, and one note for each of these seven
  algorithm families (file names are the plan's):
  1. Mersenne numbers, and why p must be prime;
  2. the Lucas–Lehmer test;
  3. Fermat PRP and the Gerbicz check;
  4. the form of factors (q = 2kp + 1, q ≡ ±1 (mod 8)), and trial factoring;
  5. Pollard's P-1;
  6. weighted transforms (the IBDWT) and their error bounds;
  7. number-theoretic transforms.

  `doc/README.md` SHALL list each note in one line with a link, and SHALL state the clean-room rule
  in one sentence that cites `AGENTS.md` § Invariants 4 rather than restating it. Every file in
  `doc/` SHALL be GitHub-flavoured Markdown, with math written only as `$…$` inline and `$$…$$`
  displayed.
- **R2** — Each note SHALL have these five sections:
  - **the result**, stated precisely, hypotheses included;
  - **its proof**, in full when short, otherwise a sketch that names where the full proof is
    published;
  - **what `mps` uses or will use**, naming the source file and function where code exists today,
    and saying plainly when the code is a stub that decides nothing, or does not exist yet;
  - **its sources**, cited by `doc/references.md` key;
  - **its provenance**: the papers and documentation the algorithm is (or will be) built from.
- **R3** — The Mersenne-numbers note SHALL state and prove that 2^a − 1 divides 2^p − 1 whenever a
  divides p, and conclude that M_p prime implies p prime. It SHALL describe the exponent check
  `mps` runs before any test, citing `check_exponent` and `composite_by_exponent` in
  `src/math.cpp`.
- **R4** — WHERE a note relies on a result with no citable published statement, the note SHALL
  derive it in full and mark the derivation as the project's own. The Lucas–Lehmer note SHALL so
  derive the Jacobi invariant: with s_0 = 4 and s_{i+1} = s_i² − 2 (mod M_p), J(s_i − 2 | M_p) = −1
  for every i ≥ 1, unchanged when the residue is multiplied by any power of 2 (a shift). The
  derivation SHALL state every hypothesis it needs (on p, and on any common factor of an s_j with a
  composite M_p, which would make the symbol 0), and the note SHALL say what a symbol other than −1
  means for a run.
- **R5** — Each entry in `doc/references.md` SHALL have a key and give its author(s), title, venue,
  year, and a DOI or stable URL. Each entry SHALL be verified to exist and to match: its DOI or URL
  resolves to that work, and the authors, title, venue and year agree with what it resolves to. A
  verification record SHALL say, for each entry, what it was checked against and when. No entry
  rests on recall alone.
- **R6** — `doc/references.md` SHALL hold exactly the works the notes and the code cite: every entry
  is cited at least once, by a note or by a comment in `src/`, and every citation in a note resolves
  to an entry. The works `src/` comments cite today SHALL have entries. The reference lists in the
  algorithm seeds (Related materials) are the starting set; the plan records any work it drops from
  them and why.
- **R7** — Each concrete numeric statement in a note (a worked example, a factor, a residue, a count,
  a list of exponents) SHALL be either checked by a recorded computation or cited to a published
  source that states it. The checks SHALL include at least:
  - the Lucas–Lehmer test on every prime exponent up to a small bound the plan sets, finding exactly
    the published Mersenne prime exponents in that range, and composite for the rest;
  - the Jacobi invariant of R4 for every odd prime p up to a small bound and every iteration, with
    and without a shift, reporting each case where the symbol is not −1 and whether R4's stated
    hypotheses exclude it;
  - the form q = 2kp + 1, q ≡ ±1 (mod 8), on known factors of Mersenne numbers;
  - the base-3 Fermat PRP property on known Mersenne primes;
  - one worked P-1 example, end to end.

  Each check's command and output SHALL be recorded in the cycle's record so the reviewer can rerun
  it. A check SHALL use an independent method that never links into `bin/mps`
  (§ Invariants 5), and a published value it compares against SHALL carry its source. The checks
  are this cycle's gates and stay out of `make check`, unless the plan argues otherwise.
- **R8** — No file in `doc/` SHALL quote, translate or paraphrase the source code of another
  Mersenne or big-number program, and no provenance section SHALL name such source as a basis
  (§ Invariants 4). Citing those programs' documentation, release notes and published timings is
  allowed.
- **R9** — `AGENTS.md` § Conventions SHALL gain the same-commit rule: a cycle that implements or
  changes an algorithm updates that algorithm's note in `doc/` (its result, what `mps` uses, its
  provenance) in the same commit. `AGENTS.md` § Map SHALL name `doc/`.

**Review scope.** `doc/` lies outside `spec/`, and the blind review grades it: the notes'
mathematics (each result is stated correctly, each proof or sketch is sound, each project
derivation is complete) and their citations (each cited work exists, matches its entry, and says
what the note attributes to it).

## Non-goals (no-gos)

- Implementing any algorithm, or changing `src/`; `tests/` and the `Makefile` change only if the
  plan puts the R7 checks in `make check`.
- Performance: no timings, tuning or throughput claims beyond what a note cites.
- Full proofs where a short sketch and a citation suffice.
- GIMPS's history and the project's own story.
- Notes for work no `ROADMAP.md` entry plans (ECM, PrimeNet reporting).
- Rendering `doc/`, or a documentation site.
- The known-answer tables (`issues/known-answer-tables.md`); a note quotes known values only as
  examples, each checked or sourced per R7.

## Clarifications

All answers below are the foreman's decisions under Geoffrey's delegation of every gate in this
effort (2026-10-11).

- **Q:** Markdown with LaTeX-style math, or plain text? — **A:** GitHub-flavoured Markdown, math as
  `$…$` inline and `$$…$$` displayed, which GitHub renders. No separate plain-text copy.
  (resolved 2026-10-12)
- **Q:** Layout? — **A:** `doc/README.md` is the index: one line per note, plus the clean-room rule
  in one sentence citing § Invariants 4. One note per algorithm family, the seed's seven.
  `doc/references.md` is the bibliography. The plan names the files. (resolved 2026-10-12)
- **Q:** What sections does each note have? — **A:** The result, stated precisely; its proof, in
  full when short, otherwise sketched and cited; what `mps` uses or will use, and where in the code
  when the code exists; its sources, by `doc/references.md` key; its provenance, papers and
  documentation only, never another program's source. The Mersenne-numbers note covers the
  exponent check `mps` already does and cites the code. (resolved 2026-10-12)
- **Q:** What about results with no citable published statement? — **A:** Derived in full in the
  note and marked as the project's own; the Lucas–Lehmer Jacobi invariant is one. (resolved
  2026-10-12)
- **Q:** What makes a reference acceptable? — **A:** Author(s), title, venue, year, DOI or stable
  URL; each verified to exist and to match, with the DOI resolving to that work; none from memory
  alone. The bibliography holds exactly the works the notes and the code cite: each cited at least
  once, every citation resolving to an entry. (resolved 2026-10-12)
- **Q:** Are the notes' claims checked by computation? — **A:** Yes, wherever a claim can be checked
  numerically; the GOAL requires it, with the examples in R7. The plan decides how. The checks are
  the cycle's gates, not part of `make check`, unless the plan argues otherwise. (resolved
  2026-10-12)
- **Q:** Add the "update the note in the same commit" rule to § Conventions? — **A:** Yes, in this
  cycle. (resolved 2026-10-12)
- **Q:** What is out of scope? — **A:** Implementing any algorithm; performance; full proofs where a
  sketch and citation suffice; GIMPS's history and the project's story; notes for work no roadmap
  entry plans; rendering or a documentation site. (resolved 2026-10-12)
- **Shaping choices, not asked** (open to change at the GOAL gate): the seeds' reference lists are a
  starting set, not a quota, since answer 5 limits the bibliography to cited works (R6); `doc/`
  joins `AGENTS.md` § Map beside the convention (R9), as `src/`, `tests/` and `ci/` are;
  results from planned entries that serve a family (the GCD and Jacobi cost at large p, PRP proofs)
  may appear in that family's note, at the depth the plan chooses, but no eighth note is required.
  (2026-10-12)

## Related materials

- Seed: `issues/math-notes-and-references.md`; `ROADMAP.md` → Part I, "The mathematics, written
  down: notes and references".
- Rules: `AGENTS.md` § Invariants 1–5 and § Conventions.
- Code the notes describe: `src/math.cpp`, `src/math.hpp` (the sieve, `smallest_prime_factor`,
  `check_exponent`, `composite_by_exponent`, the three stubs); `src/result.hpp` (what a test
  reports).
- Seeds whose reference lists are the bibliography's starting set:
  `issues/ibdwt-transform-engine.md`, `issues/lucas-lehmer-test.md`, `issues/prp-gerbicz-test.md`,
  `issues/prp-proofs.md`, `issues/trial-factoring.md`, `issues/pminus1-factoring.md`,
  `issues/large-gcd-and-jacobi.md`, `issues/ntt-transform-engine.md`,
  `issues/gpu-trial-factoring.md` (Barrett 1987).
