# 03 — The format, the loader, and what they touch

Scope: R1, R3–R6, R10–R12. Codebase reading of `c41950c` (`tests/vector_reader.*`,
`tests/check_reference.cpp`, `tests/check_mp.cpp`, `tests/vectors/generate.py`, `Makefile`,
`AGENTS.md`, `src/exc.hpp`, `src/mp.hpp`).

## Where things stand

- `tests/vector_reader.*` is test-only: it reports through `check::require`, which stops a check, so
  `bin/mps` can't link it. R1's loader must live in `src/` and throw instead, as `src/mp.*` throws
  `mp_error` from `mps_error` (`src/exc.hpp`).
- The vector format's records carry no per-row field for a source; the sources are file-level
  `# source:` lines. Its trailer digest needs SHA-256, which lives in `tests/`.
- `tests/check_reference.cpp`, `listed_primes()`, runs both reference tests on every record of
  `mersenne-exponents` from 3 on (R6's trap). `the_prime_list_carries_its_source` pins the old
  command and the `OEIS-A000043` source line.
- `vectors::STEMS` lists `mersenne-exponents`, and `check_mp.cpp`'s header check iterates `STEMS`,
  so the vector reader parses the exponent file today: R1 forbids that once it is a table.
- `generate.py`'s `mersenne-exponents` family is the only user of `OEIS_A000043` and `odd_primes`;
  it is not in `every_operand()`, so removing it changes no other family's output.

## Decisions

1. **One directory, `tests/known/`**, three tables: `mersenne-exponents.txt`, `final-residues.txt`,
   `partial-residues.txt`, and one command, `tests/known/generate.py <stem>`. Fingerprints are rows
   of the partial table: GOAL Clarification 3 gives them R8's key and the same key space, so the
   conflict R10 asks for is the table's own conflict check, with no cross-table rule.
2. **The exponent table moves** to `tests/known/mersenne-exponents.txt` and keeps its 22 record
   lines **byte for byte**, then gains 30. R5 asks for values and order; R4, read against the old
   path as a table that already exists, asks for unchanged records. Byte-identical records satisfy
   both readings, so the per-row source can't be a column added to those lines.
3. **Sources by group.** A `# from …` line declares a source; each row's source is the nearest
   `# from` line above it, so every row resolves to exactly one (R2), and a row above the first one
   has none (R3). Three kinds: `published <Key> retrieved <date>`, `computed` (the table's command,
   line 2), `runs` (a fingerprint: the row lists its runs). The 22 rows up to 9941 are `computed`
   (the command's sweep recomputes them; M_2 = 3 by trial division), the 30 above `published
   GIMPSPrimes` (R5).
4. **No digest in the trailer**: `# end <n> rows`. The count catches a dropped last row; the gates'
   byte-for-byte reproduction catches an edit. The loader then needs no SHA-256, and `bin/mps` none.
5. **The loader**, `src/known.*`, a module in `MODULES` like `reference` (nothing in `bin/mps`
   calls it): `known::parse(path, text)` and `known::load(path)`; `known_error` from `mps_error`,
   its message `<path>:<line>: <reason>`, which names the table and the row (R3). The same rules
   for every table: format, sources, supersession, conflicts, fingerprint engines.
6. **Corrections (R4)**: `supersedes=<Res64> because <reason>` names the earlier row of the same key
   by its value. Line numbers would move whenever a header line changes, which R4 allows.
7. **`make check` tests the loader on its own samples**, not only on the real tables. A loader that
   stopped refusing conflicts would otherwise pass `make check` on valid tables forever (a gate
   failing in the green direction). The gates' mutants show each refusal check bites.
8. **R6**: `listed_primes()` reads the exponent table through the loader and keeps 3 ≤ p ≤ 10,000.
9. **R1 in the vector reader**: `mersenne-exponents` leaves `STEMS` and the `mersenne` op leaves
   `fields_by_op`; `generate.py` loses the family (its sweep moves to the table command). The
   old path stays on the high-blast-radius list (R12).
10. **The R9 check** reads `reference.txt` through the vector reader and the final table through the
    loader, and compares `hex64(res64(residue, p))` with each row whose test and p it holds.

## Rejected

- *Grow the exponent file in place in `tests/vectors/`*: every table would then live in
  `tests/vectors/` (R1's one directory) beside vector families with another reader and format.
- *A source column on every row*: changes the 22 record lines (decision 2).
- *A fingerprint table of its own, empty*: a cross-table conflict rule for no rows.
- *Supersession by line number*: header edits move every line (decision 6).
- *Keeping SHA-256 trailers*: a second copy of SHA-256 in `src/`, for what reproduction already shows.

## Open edges

- The exponent table's rows carry no value, so no two can conflict; strictly ascending makes each
  p appear once. A correction to it (an exponent withdrawn) has no row form yet.
- R5's ascending order and R4's append-only order collide if a Mersenne prime is ever found below
  136,279,841 (the 52nd's rank is provisional). Not this cycle's case; a question for the GOAL owner.
- How an installed `mps` finds the tables is `issues/burn-in-mode.md`'s (Non-goals); the loader
  takes a path.
