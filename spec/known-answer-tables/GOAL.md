# GOAL — The known answers: exponents and residues, each with its source

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** known-answer-tables
- **kind:** feature
- **appetite:** small

## Problem

Every verdict test, every engine's differential test and every burn-in run compares against an
expected value. § Invariants 3 makes those values fixed points: append-only, each with its source,
never edited to make a test pass. The rule has no home yet beyond one file. Without a home, a format
and a loader, each engine cycle in Parts II and IV invents its own constants, and a wrong constant
hides exactly the hardware fault the burn-in exists to catch.

"Prime" is the weakest known answer. A Lucas–Lehmer residue of zero can come from a zeroed buffer,
and the Gerbicz check can't see a self-consistent arithmetic bug. The strong known answers are
non-zero residues: the final residues of composite exponents, and the residues after a fixed number
of iterations. The second kind lets a short run on a huge exponent, at whatever transform length and
shift an engine picks, still end on a known answer. That is what the engine cycles test against
before full runs are affordable, and what node acceptance compares against.

What exists on `main` today (`c41950c`), all pre-existing:

- `tests/vectors/mersenne-exponents.txt` holds the 22 Mersenne prime exponents from 2 to 9941. Its
  source is OEIS-A000043, and its command (`tests/vectors/generate.py mersenne-exponents`) embeds
  the terms and exits non-zero unless a Lucas–Lehmer sweep in Python over every odd prime up to
  10,000 finds exactly the odd ones. The sources and command are per file, not per row.
- `tests/check_reference.cpp` (`listed_primes`, and the checks after it) runs the reference
  Lucas–Lehmer and PRP tests on **every** exponent from 3 on in that file. Appending the 30 known
  exponents above 10,000 to it as it stands would make `make check` run `BigInt` on M136279841.
- `tests/vectors/generate.py:445-447` points at the seed for the published Res64 of M1237 and M1277,
  since `doc/references.md` has no entry for the GIMPS reports. `tests/vectors/reference.txt` holds
  their full Lucas–Lehmer residues, computed by Python and reproduced by `reference::lucas_lehmer`.
- No published residue, partial-run residue or burn-in fingerprint is recorded anywhere.
- `make check` with nothing to rebuild took 3.49–3.50 s at `d3636cb` on an Apple-silicon Mac with
  clang, against 1.12–1.14 s before the reference big integer (`spec/reference-big-integer/REVIEW.md`).

## Outcome / vision

A small set of plain-text tables, readable by people and loaded by one C++ module, holds every
known answer the later cycles grade against. Every row carries its source: a published result with
its retrieval date, or the exact command that recomputes it by a method independent of every `mps`
engine. `make check` refuses a row without a source and a key with two answers.

The tables hold three kinds of answer this cycle: the Mersenne prime exponents, the final residues of
composite exponents, and residues after a stated number of iterations. The burn-in fingerprint gets
its row format and its rule, two engines that share no arithmetic, and waits for those engines.
A residue after k iterations is keyed by what determines it (test, exponent, start, k) and not by
how a run computed it, so one row answers a run at any transform length and any shift, on any
device class.

## Acceptance criteria (the contract)

- **R1** — The known answers SHALL live in plain-text tables in one directory, one table per kind of
  answer, and `make check` SHALL read them through a single C++ module that a later cycle can link
  into `bin/mps` without a second parser. No other code in the repo outside `spec/` SHALL parse
  them.
- **R2** — Every row SHALL resolve to exactly one source: a published result, cited by a
  `doc/references.md` entry and the date it was retrieved; the exact command that recomputes it
  by a method independent of every `mps` engine; or, for a fingerprint row only, the agreeing runs
  R10 lists. Each published source SHALL have a
  `doc/references.md` entry checked against the work, as the existing entries are.
- **R3** — IF a row is malformed, or has no source, or one key carries two different values and
  neither row supersedes the other, or a fingerprint row names fewer than two distinct engines,
  THEN `make check` SHALL fail and name the table and the row. The plan's gates SHALL show each of
  these failures with a deliberate edit in a scratch copy.
- **R4** — The tables SHALL only grow. Against `origin/main`, the cycle's diff of a table that already
  exists SHALL add records after the existing ones and change none (header and trailer lines
  excepted). A correction SHALL be a new row that names the row it supersedes and why, and R3's
  conflict check SHALL treat the superseded row as replaced.
- **R5** — The Mersenne prime exponent table SHALL list every exponent GIMPS lists on its retrieval
  date, ascending (52 at shaping, 2 through 136,279,841). It SHALL keep the 22 records of
  `tests/vectors/mersenne-exponents.txt` with their values and order unchanged, and SHALL be the
  only file `make check` reads the exponents from. Rows above 10,000 SHALL cite the published list,
  and the table's command SHALL reproduce it byte for byte without claiming to recompute them.
- **R6** — IF a listed exponent exceeds 10,000, THEN `make check` SHALL NOT run a reference test on it.
  The reference Lucas–Lehmer and PRP checks SHALL still cover every listed exponent from 3 to 9941.
- **R7** — The tables SHALL hold the final Lucas–Lehmer Res64 and the PRP type-1 Res64 of composite
  M_p: M1237 → `DF5E6622F83F5BE2` and M1277 → `5613A480590E78BA` (LL), M28603 →
  `F899E5F1E2B40323` (PRP type 1), and at least one LL and one PRP type-1 row in each decade of
  exponent from 10^3 up to the largest known exponent, wherever the GIMPS reports hold an accepted
  result. A published row SHALL be accepted only when two or more results at different shifts or
  from different programs agree and none carries an error code, and it SHALL record how many
  agreed.
- **R8** — The tables SHALL hold residues after a stated number of iterations, each keyed by test,
  exponent, start value and iteration count, and by neither transform length nor shift. They SHALL
  include s_100 of M3888509 → `71E61322CCFB396C` from its published source, and computed LL and
  PRP rows at one or more exponents in each decade from 10^3 to 10^7.
- **R9** — For every published row whose full residue is in `tests/vectors/reference.txt` (M1237
  and M1277 at least), `make check` SHALL show that the row's Res64 equals the low 64 bits of that
  residue. For every other row a Python command can recompute (M28603, s_100 of M3888509, the
  computed rows of R8), the table's command SHALL recompute it with Python's integers and exit
  non-zero on any mismatch.
- **R10** — A fingerprint row SHALL carry R8's key and the Res64, and for each agreeing run the
  engine, the transform length and the shift. A fingerprint and an R8 row with the same key and
  different values SHALL fail R3's conflict check. This cycle adds the format and its checks, and
  no fingerprint rows.
- **R11** — `make check` SHALL NOT run Python or any tool beyond what it uses today. `bin/mps`'s
  command-line behaviour SHALL be unchanged: `tests/cli.sh` passes without edits. With nothing to
  rebuild, `make check` SHALL take no more than 0.5 s longer than at `c41950c` on the same machine;
  the plan records the baseline it measured.
- **R12** — In the commit that creates them, each new table path SHALL join the high-blast-radius
  list, keeping `tests/vectors/mersenne-exponents.txt` on it if that file moves; `AGENTS.md` § Map
  SHALL name the tables and their loader; and the comment above `COMPOSITE` in
  `tests/vectors/generate.py` SHALL point at the residue table instead of the seed.

## Non-goals (no-gos)

- Known factors and the P-1 boundary cases (the seed's M11, M23, M29, M141900347, M141900047,
  M1022191, M1003417 and M141933599 rows). They move to `issues/known-factor-tables.md` (Clarifications).
- Fingerprint rows themselves: no engine exists yet. The integer transform cycle
  (`issues/ntt-transform-engine.md`) is the first that can add one.
- The burn-in: its profiles, the exponents, lengths and iteration counts it picks, and its walltimes
  (`issues/burn-in-mode.md`). It appends the rows it needs.
- Any change to `bin/mps` or its command line, and how an installed `mps` finds or embeds the
  tables (`issues/burn-in-mode.md`, the install entry of Part III).
- PRP residue types other than type 1, cofactor tests, and the ranks of the known primes, including
  the provisional rank of the 52nd.
- Reference tests in `make check` above 10,000, and any change to `src/mp.*` or `src/reference.*`.
- Network access from `make check` or from a table's command; mersenne.ca, which challenges scripts.
- The copies of the exponent list in landed records (`spec/*/verify/`). The doc-claims standing
  check (`issues/doc-claims-standing-check.md`) can read the table when it moves that evaluator out.
- Folding the existing vector families (`tests/vectors/reference.txt` and the rest) into the tables.
- GMP or any other big-number library as an oracle. Python's integers are the independent method.

## Clarifications

Answers proposed by a delegated shaping run and confirmed by the foreman at GOAL sign-off.

- **Q:** One cycle, or a pilot and follow-ups? — **A:** A pilot and one follow-up. The seed's five
  kinds of answer and its mechanism come to about fifteen criteria. The factors and P-1 boundary
  cases serve only Part VIII (trial factoring, P-1, the GCD), not the engines or the burn-in that
  come first, so they move to the follow-up seed `issues/known-factor-tables.md`, ahead of trial
  factoring. The seed's factor evidence and its two notes (the factor report lists found
  factors only and pages at 5,000 rows; a P-1 row is checked by x ≡ 1 mod q) went into that seed,
  recorded at sign-off, before this cycle's seed can be retired. This cycle keeps the slug (confirmed by the foreman, 2026-10-17).
- **Q:** Appetite? — **A:** Small, as the seed says. Twelve criteria sit above the soft eight to ten,
  but R11 and R12 hold constraints and same-commit duties rather than new behaviour, and the repo's
  small cycles have run ten to thirteen (confirmed by the foreman, 2026-10-17).
- **Q:** What keys a residue after k iterations, and a fingerprint? — **A:** The test, the exponent,
  the start value and the iteration count. An exact run's unshifted residue after k iterations is
  fixed by those four, and the transform length and shift change how a run computes it, not what it
  is (§ Invariants 2, its last point). The length and shift are recorded per agreeing run as
  provenance (R10). So a run at any length and a fresh shift looks up one row, fingerprints from two
  device classes check each other, and a fingerprint checks against a computed row of the same key.
  This refines the seed's R4, which listed length and shift among the fields (confirmed by the foreman, 2026-10-17).
- **Q:** Do Python's integers and the reference `BigInt` count as the two engines a fingerprint
  needs? — **A:** No. A fingerprint is a row agreed by two `mps` engines that share no arithmetic,
  with their lengths and shifts. Values from Python and `BigInt` are computed rows (R8). They share
  the key space, so the first fingerprints are checked against them (confirmed by the foreman, 2026-10-17).
- **Q:** How many rows in each transform-length band? — **A:** Bands of exponent by decade, as a
  proxy. The lengths each engine uses aren't measured until the transform cycle
  (`issues/ibdwt-transform-engine.md`). Published final residues: one LL and one PRP type-1 row or
  more per decade from 10^3 up (R7). Computed partial residues: one exponent or more per decade from
  10^3 to 10^7, where Python's integers finish in minutes (R8). The plan picks the exponents and the
  iteration counts. Engine cycles append rows for the bands they measure (confirmed by the foreman, 2026-10-17).
- **Q:** What is the table format? — **A:** One table per kind of answer (R1), each row naming its
  source. The syntax is the plan's choice; extending the existing `# mps vectors` format and its
  reader is the obvious candidate. The plan also decides where the directory lives, and whether
  `tests/vectors/mersenne-exponents.txt` moves into it or stays put and grows. A move keeps every
  record and its order (R5) and the old path on the high-blast-radius list (R12) (confirmed by the foreman, 2026-10-17).
- **Q:** Where does the loader live? — **A:** Where a later `mps` command can link it without a
  second parser (R1), so not only under `tests/`. Nothing in `bin/mps` calls it this cycle, as with
  `src/reference.*` today (confirmed by the foreman, 2026-10-17).
- **Q:** What does a table's command do with published rows? — **A:** It embeds them, as the
  current command embeds the OEIS terms, and recomputes with Python's integers what Python can
  reach (R9). It doesn't fetch, so it reproduces the file byte for byte offline (confirmed by the foreman, 2026-10-17).
- **Q:** What makes a published residue acceptable? — **A:** The seed's rule (R7): two or more
  results at different shifts or from different programs agree, and none carries an error code. The
  rows come from mersenne.org's plain-text reports (`report_LL`, `report_prp`, `txt=1`), which accept
  scripted requests (confirmed by the foreman, 2026-10-17).
- **Q:** Where may a published partial-run residue come from? — **A:** From published documentation
  only, such as the Mlucas README for s_100 of M3888509. A self-test table that appears only in a
  program's source is out of reach (§ Invariants 4). If the plan can't find s_100 of M3888509 in
  documentation, R8 takes it as a computed row (100 squarings in Python) instead (confirmed by the foreman, 2026-10-17).
- **Q:** How is the timing budget handled? — **A:** This cycle gets its own: 0.5 s over `c41950c`
  (R11). The checks are a parse of a few hundred rows and a comparison with residues already loaded;
  every expensive recomputation runs in the table's command, outside `make check`. Together with
  `reference-big-integer`'s +2.4 s, that stays under the +3.0 s that cycle allowed over `c9b82ff`
  (confirmed by the foreman, 2026-10-17).
- **Q:** Which residue types? — **A:** Lucas–Lehmer, and PRP type 1, the type-1 residue
  `reference::prp3` returns and `issues/prp-gerbicz-test.md` plans to report. The type-1 residue and
  x_p agree only modulo M_p (`doc/prp-and-gerbicz.md`), so every row names its type (confirmed by the foreman, 2026-10-17).
- **Q:** Where the seed and these answers differ, which holds? — **A:** The answers. The seed's
  `test/` is not fixed (the plan places the directory); its R3 "the only place" covers the repo
  outside landed `spec/` records; its R5 and R6 became R4 and R7; its R3 gained R6, because
  `make check` runs the reference tests on every listed exponent; the factors moved out (confirmed by the foreman, 2026-10-17).

## Related materials

- Seed: `issues/known-answer-tables.md`; `ROADMAP.md`, Part I, "The known answers: exponents,
  residues and factors, each with its source".
- Last cycle: `spec/reference-big-integer/` (its GOAL's notes "One prime list" and R10; its REVIEW's
  R10 timings and finding F2).
- Downstream seeds that grade against these tables: `issues/ibdwt-transform-engine.md`,
  `issues/lucas-lehmer-test.md`, `issues/prp-gerbicz-test.md`, `issues/benchmark-mode.md`,
  `issues/burn-in-mode.md`, `issues/ntt-transform-engine.md`, `issues/cuda-backend.md`.
- Source: `tests/vectors/mersenne-exponents.txt`, `tests/vectors/generate.py` (`COMPOSITE`,
  `OEIS_A000043`, `every_operand()`), `tests/vector_reader.*`, `tests/check_reference.cpp`,
  `tests/vectors/reference.txt`, `doc/references.md` (`OEIS-A000043`, `GIMPSMath`).
- Published sources: GIMPS, "List of Known Mersenne Prime Numbers", https://www.mersenne.org/primes/;
  GIMPS reports, https://www.mersenne.org/report_LL/ and https://www.mersenne.org/report_prp/;
  E. Mayer, Mlucas README (self-test residues).

## Notes

- **Human gate.** The tables are expected values (§ Invariants 3) and `tests/vectors/` is on the
  high-blast-radius list, so the plan runs research and a CONFIRMED finding needs a human decision.
- **The append-order trap** (the seed, from `reference-big-integer` review cycle 3).
  `tests/vectors/hex.txt` and `decimal.txt` hold one record per operand of `every_operand()` in
  `tests/vectors/generate.py`, in its order, with the `COMPOSITE` exponents' M_p last. If this cycle
  adds a composite exponent to `COMPOSITE` or a new operand to any family there, its records land
  before existing ones, against R4. A new operand goes at the very end of `every_operand()`'s list.
- **The fingerprint rule and § Invariants 3.** Whether two `mps` engines that share no arithmetic
  satisfy "the command that recomputes it with an independent method" is open for Geoffrey before
  the first fingerprint row lands. No row lands this cycle, so it doesn't block.
