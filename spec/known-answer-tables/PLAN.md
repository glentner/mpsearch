# PLAN — The known answers: exponents and residues, each with its source

> **Status:** Draft for review · **Last updated:** 2026-10-17
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). Backing detail is in
> [`research/`](research/). Every design element traces to a GOAL R-ID.

## 1. Summary

Three plain-text tables in a new directory, `tests/known/`, written by one Python command,
`tests/known/generate.py`, and read by one C++ module, `src/known.*`: the Mersenne prime exponents
(the old `tests/vectors/mersenne-exponents.txt`, moved with its 22 record lines byte for byte and
grown to 52), the final residues of composite exponents (eleven published rows), and the residues
after k iterations (one published row and nineteen computed, fingerprints among them in time).
Each row's source is the nearest `# from …` line above it. The loader refuses a table at the first
rule it breaks and names the table and the row; `make check` tests the loader on tables of its own
and then reads the real ones. Four phases: the format, the loader and the exponents; the final
residues; the partial residues; then the audit, the ledger, the push and the cluster runs. Frozen
gates parse the tables themselves, pin every row, probe the loader against their own model of the
format, inject bugs into it, edit the tables in scratch copies, and time `make check`.

## 2. Design

### Files

| Path | Change | Phase | R-IDs |
|---|---|---|---|
| `src/known.hpp`, `src/known.cpp` | new: the loader | P1 | R1, R3, R4, R10 |
| `Makefile` | exactly `MODULES  := ansi cli io known mp math reference result`; nothing else | P1 | R1, R11 |
| `tests/known/generate.py` | new: the command, one stem per table | P1–P3 | R2, R5, R7–R9 |
| `tests/known/mersenne-exponents.txt` | the moved exponent table | P1 | R2, R4, R5 |
| `tests/known/final-residues.txt` | new | P2 | R2, R7, R9 |
| `tests/known/partial-residues.txt` | new | P3 | R2, R8–R10 |
| `tests/vectors/mersenne-exponents.txt` | deleted (moved) | P1 | R1, R5 |
| `tests/vectors/generate.py` | the `mersenne-exponents` family removed (P1); the comment above `COMPOSITE` points at `tests/known/final-residues.txt` (P2); no other code line changes | P1, P2 | R1, R12 |
| `tests/vector_reader.cpp` | `mersenne-exponents` leaves `STEMS`, `mersenne` leaves `fields_by_op`; nothing else | P1 | R1 |
| `tests/check_known.cpp` | new: the checks below | P1–P3 | R1–R5, R7, R9, R10 |
| `tests/check_reference.cpp` | `listed_primes()` reads the exponent table through the loader, from a pinned path constant, 3 ≤ p ≤ 10,000; `the prime list carries its source` goes (its job moves to `check_known.cpp`) | P1 | R5, R6 |
| `spec/known-answer-tables/equivalent-mutants.txt` | the builder's list of point mutants of `src/known.cpp` the probe cannot kill, each with its argument (§ 6, step 7) | P1–P3 | R3 |
| `doc/references.md` | entries `GIMPSPrimes` (P1), `GIMPSReportLL`, `GIMPSReportPRP` (P2), `MlucasREADME` (P3); the opening sentence names the tables (P1) | P1–P3 | R2 |
| `doc/README.md` | the bibliography sentence names the tables, as the bibliography's own does (P1) | P1 | R2 |
| `AGENTS.md` | § Map and the high-blast-radius list, below | P1–P3 | R12 |
| `issues/…`, `ROADMAP.md` | the deferral ledger | P4 | — |

Everything else stays as `c41950c` has it: every other file in `src/`, `tests/` and
`tests/vectors/`, `tests/cli.sh`, `ci/`, `README.md`, every note in `doc/`. `bin/mps` links
`known.o`, which nothing in it calls (R11), as it links `reference.o` today.

### The format (pinned)

LF line ends, printable ASCII (0x20–0x7E), no empty line, fields separated by single spaces.

```
table   = magic command *note *(source *row) trailer
magic   = "# mps known 1" LF
command = "# command: python3 tests/known/generate.py " stem " > tests/known/" stem ".txt" LF
note    = "# " text LF                ; not "# from ", not "# end "; only before the first source
source  = "# from " origin ": " text LF
origin  = "published " key " retrieved " date      ; key: a letter, then letters, digits, "-"
        | "computed"                                ; the table's own command recomputes the rows
        | "runs"                                    ; fingerprints: each row lists its runs
row     = op 1*(SP field) *(SP attribute) [SP "supersedes=" res64 " because " text] LF
trailer = "# end " count " rows" LF                 ; the last line; count = the rows in the table
res64   = 16 of [0-9A-F]                            ; hex64's form
date    = YYYY-MM-DD                                ; month 01-12, day 01-31
```

A row's source is the nearest source line above it; a row with none above it has no source.

| stem | op | fields | attributes | key |
|---|---|---|---|---|
| `mersenne-exponents` | `mersenne` | p | none; rows ascend strictly | p |
| `final-residues` | `ll`, `prp1` | p, res64 | `agreed=<n>`, n ≥ 2: on every row under a published source, and on no other | op, p |
| `partial-residues` | `ll`, `prp` | p, start, k, res64 | `run=<engine>:<length>:<shift>`, engine `[a-z0-9-]+`, length ≥ 1, shift < p: two or more with two or more distinct engines on every row under `# from runs:`, none on any other | op, p, start, k |

- p is a prime below 2^32 (2 allowed only in the exponent table); numbers are decimal with no
  leading zero; k ≥ 1.
- `ll` (final): the low 64 bits of s_(p−2) mod M_p, s_0 = 4. `prp1`: of 3^(M_p − 1) mod M_p, the
  type-1 residue. `ll` (partial): of s_k, s_0 = start, s_(i+1) = s_i² − 2 mod M_p. `prp`: of
  start^(2^k) mod M_p, the x_k of `doc/prp-and-gerbicz.md`, Theorem 2. All unshifted: the key is
  what determines the value (GOAL, Clarifications), and a run's length and shift go in its `run=`.
- **Corrections (R4).** `supersedes=<V> because <reason>` replaces the earliest row above it with
  the same key and Res64 V that no other row has replaced. A row naming no such row is refused,
  and so is one naming a row only below it. A correction is never weaker than what it replaces
  (design review F3): a fingerprint (a row under `# from runs:`) supersedes nothing, and in the
  final table a computed row may not replace a published one with p ≥ 200,000, which the command
  does not recompute (`RECOMPUTED_BELOW` in `src/known.cpp`, the same bound as the command's). A
  published row may replace any row. This tightens § Invariants 3 only; loosening it, so that
  fingerprints may correct other rows, waits on Geoffrey's ruling on fingerprint independence
  (GOAL, Notes).
- **No final residue in the partial table** (design review F9): `ll p 4 k` with k = p − 2 is
  s_(p−2), the final Lucas–Lehmer residue, and the loader refuses it there, so the two tables can't
  hold two answers for one quantity. The PRP tables need no such rule: x_k = 3^(2^k) is never the
  type-1 residue 3^(M_p − 1), and the low 64 bits of one don't give the other's.
- **Conflicts (R3, R10).** After corrections, two rows of one key with different Res64 are refused
  at the later one. Rows that agree are allowed (a published row and a fingerprint corroborating it).
- **One refusal, in a fixed order** (corrected by design review F10). The loader refuses at the
  first of these that fails, in this order: (1) the text is nonempty and ends in LF, else the last
  line (line 1 when empty); (2) every line is nonempty printable ASCII, the first that is not, top to
  bottom; (3) the magic line; (4) at least three lines, else the last; (5) the command line; (6) the
  last line is a trailer in form; (7) the lines between, top to bottom: a source line's form, `# end`
  before the last line, a note after a source, a `#` line without its space, a row with no source
  above it, a row's fields and attributes; (8) the trailer's count; (9) the exponents' order; (10)
  the corrections, top to bottom; (11) the conflicts, at the later row of the first pair. So a
  single defect is refused at its line, and a table with several is refused at the first by this
  order (a defective row and a missing trailer: the trailer).

### The loader (`src/known.hpp`, pinned)

```cpp
class known_error: public mps_error { … };   // what(): "<path>:<line>: <reason>", or "<path>: does not open"

namespace known {
	extern const std::vector<std::string> TABLES;   // the tables' paths from the repository root, in
	                                                // the order exponents, final, partial, as each exists
	enum class Origin {PUBLISHED, COMPUTED, RUNS};
	struct Source {Origin origin; std::string reference; std::string retrieved; std::string text; std::size_t line;};
	struct Run {std::string engine; std::uint64_t length; std::uint64_t shift;};
	struct Row {
		std::size_t line; std::string op; std::uint64_t exponent, start, iterations; std::string res64;
		std::uint64_t agreed; std::vector<Run> runs; std::string supersedes, reason;
		std::size_t source; bool superseded;
	};
	struct Table {std::string path, stem; std::vector<std::string> notes; std::vector<Source> sources; std::vector<Row> rows;};
	Table parse(const std::string& path, const std::string& text);
	Table load(const std::string& path);   // reads the file and returns parse(path, its text)
}
```

`RECOMPUTED_BELOW = 200'000` is a named constant in `src/known.cpp`. `parse` and `load` are defined
out of line at file scope in `src/known.cpp`, each once, its name on the definition's first line (`known::Table known::parse(const std::string& path, const
std::string& text) {`), so the gates can wrap them. `load` calls `parse`. The basename of `path`
must be the command's `<stem>.txt`. Helpers are the builder's. No `check.hpp`, no I/O beyond
reading the file, no logging; the module links into `bin/mps` unchanged when a later cycle calls it.

### The tables' contents (pinned; [01](research/01-published-sources.md), [02](research/02-computed-rows-and-cost.md))

**`mersenne-exponents.txt`.** Under `# from computed: …` (the sweep): the 22 rows `mersenne 2` …
`mersenne 9941`, byte for byte the record lines of `c41950c`'s `tests/vectors/mersenne-exponents.txt`,
in order. Then `# from published GIMPSPrimes retrieved 2026-10-17: …` and the 30 rows 11213 …
136279841. The command embeds the 52 exponents GIMPS listed on 2026-10-17, recomputes the ones up
to 10,000 (M_2 = 3 by trial division, every odd prime up to 10,000 by the Lucas–Lehmer test in
Python's integers) and exits non-zero unless they are exactly the list's; above 10,000 it writes
the list and claims nothing (R5).

**`final-residues.txt`.** Under `# from published GIMPSReportLL retrieved 2026-10-17: …`, then
`# from published GIMPSReportPRP retrieved 2026-10-17: …`:

```
ll 1237 DF5E6622F83F5BE2 agreed=2          prp1 28603 F899E5F1E2B40323 agreed=2
ll 1277 5613A480590E78BA agreed=3          prp1 1000193 CA0B0B10C609A027 agreed=4
ll 10303 3DD943A77EC9238B agreed=3         prp1 10000019 9DD181D73794E731 agreed=4
ll 100069 1ECC98522D52594C agreed=5        prp1 100005533 ADF278E588B2CDCE agreed=2
ll 1000193 8F5354551BC18373 agreed=5
ll 10000139 76BB6A7CE0BCA62B agreed=6
ll 100000007 F9042256B193FAA0 agreed=12
```

The acceptance rule as research applied it (R7): every 64-bit residue for the exponent agrees, two
or more from different shifts or programs, no result with an error code or in a Bad section, PRP all
type 1 base 3; `agreed` counts the agreeing 64-bit residues. One LL row per decade from 10^3 to
10^8 (10^8 meaning up to 136,279,841); PRP rows wherever the reports hold an accepted result, which
on 2026-10-17 excludes 10^3 and 10^5 (none there). The command recomputes every row with
p < 200,000 (LL by folding; PRP type 1 by x ← 3x² mod M_p, p − 2 times, then x²) and exits non-zero
on a mismatch; the notes say the larger rows rest on the reports' agreement.

**`partial-residues.txt`.** Under `# from published MlucasREADME retrieved 2026-10-17: …`, the row
`ll 3888509 4 100 71E61322CCFB396C`. Then under `# from computed: …` these nineteen, each the
low 64 bits as the command computes them by folding, with k ≥ ⌈log2 p⌉ + 8 so each has been reduced
modulo M_p eight times or more:

```
ll 1277 4 100 EE3E8EF89BAFBDCD       prp 1277 3 100 C6D7DFE3FCAEDC31
ll 1277 4 1000 3D43F5D86DECC8F5      prp 1277 3 1000 36F834F2D5842CF5
ll 10303 4 100 443391E534C210F2      prp 10303 3 100 088D6D3D863CB0C7
ll 10303 4 1000 7841BFAB20749B83     prp 10303 3 1000 3C30472597BE7B6C
ll 10303 4 10000 FB3AC33F5A26CC71    prp 10303 3 10000 65CE30B46EF0AA1B
ll 100069 4 100 03DFC5B4407ED178     prp 100069 3 100 05DC064E3050BCD0
ll 100069 4 1000 7AA9194B9ED5BBBC    prp 100069 3 1000 D87224FAC22ABDEF
ll 1000193 4 100 F1234B20ACFD642F    prp 1000193 3 100 44DEBFC2AA1782BA
                                     prp 3888509 3 100 79FA4A9B6D077D31
ll 10000019 4 32 3DC2608FC9C15BE6    prp 10000019 3 32 B6DF55F5DE80E01A
```

Decades 10^3, 10^4, 10^5, 10^6 and 10^7 itself, so R8 holds whether or not its range counts the
decade that starts at 10^7. Each exponent has a published final row (an engine can run the short
and the full test on one p). The command embeds every value, recomputes each, and exits non-zero on
a mismatch (R9). No fingerprint row (R10); the notes describe the form.

**Row order** within a table is the builder's, except the exponents (ascending). Notes and the
text after each source's colon are the builder's; the gates pin the source kinds, keys and dates.
Each embedded Res64 of a row the command recomputes appears in `tests/known/generate.py` exactly
once as a literal (the gates change each one to show the command refuses it).

### The command (`tests/known/generate.py`)

Python 3.8+, standard library only, run from the repository root; one stem per call, the table to
stdout through `sys.stdout.buffer`; no network, no `random`, integers only; the code style of
`tests/vectors/generate.py`. Under `python3` (3.9) here the exponents take about 150 s, the final
residues about 215 s (LL 100069 is 200 s of it), the partial residues about 190 s.

**Pinned for the gates' trace** (design review F1; § 6, step 1). Three module-level functions do
all the arithmetic, and the table functions call them through the module's names, so the gates can
wrap them: `lucas_lehmer(p, start, k)` (s_k mod M_p), `squarings(p, start, k)` (start^(2^k) mod M_p)
and `prp_type1(p)` (3^(M_p − 1) mod M_p). The sweep calls `lucas_lehmer(p, 4, p − 2)` for every odd
prime up to 10,000; a final row with p < 200,000 calls `lucas_lehmer(p, 4, p − 2)` or
`prp_type1(p)`; every partial row calls `lucas_lehmer(p, start, k)` or `squarings(p, start, k)`.
`main(argv)` writes the table to `sys.stdout.buffer`. On a mismatch the command exits 1 through
`sys.exit` (never a traceback) with a message on stderr naming the row: its op and every number of
its key (the sweep names each exponent it finds that the list lacks, or the list gives that it
doesn't find).

### The checks (pinned labels; R1–R7, R9, R10)

| file | label | phase |
|---|---|---|
| `check_known.cpp` | `the loader reads a table it is given` | P1 |
| | `the loader refuses a malformed table` | P1 |
| | `the loader refuses a row without a source` | P1 |
| | `the loader refuses two answers for one key` | P1 |
| | `the loader honours supersession` | P1 |
| | `the loader refuses a fingerprint with fewer than two engines` | P1 |
| | `every known-answer table loads` | P1 |
| | `every published source has a bibliography entry` | P1 |
| | `exponents above 10000 cite a published list` | P1 |
| | `final residues agree with the reference residues` | P2 |
| | `final residues are of composite Mersenne numbers` | P2 |
| `check_reference.cpp` | the five checks it keeps, labels unchanged | — |

- **The loader's own tables.** The first six checks build tables in memory (the magic line, the
  command, a body, the trailer) and call `known::parse`. Each refusal check expects `known_error`
  whose message starts `<path>:<line>: ` for the defect's line. Between them they show every rule
  of the format table: the envelope (empty text, no final LF, another magic, another stem's command,
  a wrong count, no trailer, a row after it, a note after a source), each field (Res64 length and
  case, a leading zero, a composite or out-of-range p, `ll 2`, an op of another table, a missing
  field, a double space, a tab), each attribute (`agreed=1`, a published final row without it, a
  computed row with it, an unknown one, a correction without `because` or a `because` without one),
  each source form (a bad key, a bad date, a missing date, empty text, an unknown kind), exponents
  repeated or out of order, k = 0, a shift of p, a run without its shift; rows above every source
  in each table; two answers in the final and partial tables, and a fingerprint against a computed
  row (R10); a correction and a chain of them accepted, the replaced rows marked, the reason kept,
  and a correction naming no row, a row of another key or a row only below it refused, and a
  computed correction of a published final row with p ≥ 200,000 refused while a published one is
  accepted (F3); one run, two runs of one engine, a fingerprint with no runs, runs on a computed
  row, a fingerprint that supersedes (F3); `ll p 4 p−2` in the partial table (F9); and the positive
  reading of each table kind. The samples sit in the check the table above names: the R10 sample in
  `the loader refuses two answers for one key`; the correction samples, the weak one included, in
  `the loader honours supersession`; runs on a computed row and a fingerprint that supersedes in the
  fingerprint check; the envelope, field, attribute and source samples, repeated or unordered
  exponents and the final residue in the partial table in the malformed check. The gates' bugs (§ 6)
  each expect one of these checks to fail.
- **`every known-answer table loads`** loads each path of `known::TABLES` and, on `known_error`,
  records a failure whose detail is the message, so the output names the table and the row (R3).
- **`every published source has a bibliography entry`**: each published source's key is a
  `### <key>` heading of `doc/references.md` (R2).
- **`exponents above 10000 cite a published list`**: rows above 10,000 published, rows up to it
  computed (R5).
- **`final residues agree with the reference residues`** (R9): each unsuperseded final row (a
  correction counts, the row it replaced does not; design review F4) whose
  test and p `tests/vectors/reference.txt` holds (`ll`, and `prp` as `prp1`) equals
  `hex64(res64(<residue>, p))`; at least two compared (1237, 1277).
- **`final residues are of composite Mersenne numbers`**: no final row's p is in the exponent table.
- **R6.** `listed_primes()` returns the exponent table's p with 3 ≤ p ≤ 10,000 (a named constant),
  loaded through `known::load` from `static const std::string EXPONENTS = "tests/known/mersenne-exponents.txt";`.
  The tables' paths appear in `src/` and `tests/` only in `src/known.*` and in such pinned
  constants (`EXPONENTS`, `FINAL`, `PARTIAL`) in `check_known.cpp` and `check_reference.cpp` (F5).
  The Lucas–Lehmer and PRP checks of listed primes run on exactly those 21, and nothing in
  `make check` runs a reference test above 10,000.

### Where the paths go (`AGENTS.md`; R12)

The high-blast-radius list keeps every line it has, in order, and gains these after them, each in
the commit that creates its path, and no others:

```
- `tests/known/` (the tables of known answers and their command: expected values)                          P1
- `src/known.hpp`, `src/known.cpp` (the tables' loader)                                                     P1
- `tests/known/mersenne-exponents.txt` (the Mersenne prime exponents)                                       P1
- `tests/known/final-residues.txt` (the final residues of composite exponents)                              P2
- `tests/known/partial-residues.txt` (the residues after a stated number of iterations, and the fingerprints)  P3
```

§ Map gains a line beginning ``- `src/known.cpp`, `src/known.hpp`:``, and its `tests/` bullet names
`known/`, `known/generate.py` and each table present (`mersenne-exponents.txt`, …), and no longer
calls `vectors/mersenne-exponents.txt` the exponents. `known::TABLES` lists the tables present, in
that order, so it gains a path in P2 and in P3.

### Requirement → design map

| R-ID | Design element(s) that satisfy it |
|------|-----------------------------------|
| R1 | `tests/known/`, one table per kind; `src/known.*` in `MODULES`, the only parser outside `spec/` (the vector reader drops the stem; `generate.py` only writes) |
| R2 | Source lines; each row's nearest; `published <key> retrieved <date>` with a bibliography entry; `computed` (the command); `runs` (fingerprints); the bibliography check |
| R3 | The loader's refusals and `known_error`'s `<path>:<line>`; `every known-answer table loads`; the gates' scratch-copy edits |
| R4 | Record lines kept byte for byte; `supersedes=` and its conflict rule; the gates' prefix check against `origin/main` |
| R5 | The exponent table's 52 rows, its two sources, ascending; the command's sweep and byte-for-byte reproduction; `listed_primes()` reads only it |
| R6 | `listed_primes()` bounded at 10,000; the gates' call trace |
| R7 | The eleven final rows, `agreed=`, the acceptance rule in research and the command's notes |
| R8 | The partial table's key (op, p, start, k) and its twenty rows |
| R9 | `final residues agree with the reference residues`; the command's recomputation and its exit status |
| R10 | `run=` and `# from runs:`; the engine count; the conflict rule shared with every row |
| R11 | No new tool in `make check`; `tests/cli.sh` and `bin/mps`'s output unchanged; +0.5 s gated |
| R12 | The list and § Map lines in the creating commits; the `COMPOSITE` comment |

## 3. Invariant gate (AGENTS.md constitution check)

Walked before research (from the GOAL) and again on this design.

- **High-blast-radius paths.** The cycle touches `tests/vectors/` (the generator, the moved file)
  and adds `tests/known/` and `src/known.*`, which join the list. A CONFIRMED finding there needs
  Geoffrey's decision (GOAL, Notes).
- **§ 1 Verdicts are earned.** Untouched: no verdict, no change to `src/math.*`, `src/main.cpp` or
  `src/result.*`. The tables hold answers; nothing decides from them yet.
- **§ 2 Arithmetic is checked.** The partial key leaves out length and shift, as the invariant's
  last point says a result does not depend on them. No engine changes; `src/mp.*` and
  `src/reference.*` are untouched (GOAL, Non-goals).
- **§ 3 Known answers are fixed points.** The cycle builds the home this section names. Every row
  carries one source; the exponent records keep their bytes; corrections are new rows; each new
  path joins the list in its commit. The moved file's path stays on the list. The fingerprint rule
  (two `mps` engines) is open for Geoffrey before the first fingerprint (GOAL, Notes); no row lands.
- **§ 4 Clean room.** Sources are GIMPS's list and reports and the Mlucas README, all
  documentation and results; no program's source was opened in research, and the builder opens
  none. The README's text is GFDL; the tables take one number from it, cited, and no prose.
- **§ 5 Dependency-free core.** `known.o` needs only the standard library. Python stays a test
  oracle run by hand.
- **§ Conventions.** No R#/P# ids in `src/` or `tests/`; the code style; `verify` (`make check`)
  unchanged; the exponent bound is a named constant in each check file that needs it. No algorithm
  changes, so no note in `doc/` changes; the bibliography carries no claims, so the marker rule of
  `doc/README.md` does not reach the entries.

### Deviation justifications

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| 1. Sources as group lines, not a column | The 22 moved records keep their bytes (R4 read against the old path, R5) | A source column changes every one of those lines |
| 2. Two named bounds of 10,000 (`check_known.cpp`, `check_reference.cpp`) | R5's border and R6's bound are separate rules that share a value | One shared constant would need a header in `tests/` for one number; the gates hold both to 10,000 |
| 3. The recompute bound, 200,000, in both `src/known.cpp` and `tests/known/generate.py` | The loader refuses a computed correction of a published row the command does not recompute (F3), so it needs the command's bound | The loader can't read the Python script, and a shared file for one number is heavier than the rule; the probe and the gates' command trace hold both to 200,000, and a change to one is a same-commit obligation in the other (§ Conventions) |
| 4. `make check`, the probe and the point mutants all test the loader | A loader that stopped refusing passes `make check` on valid tables (the green direction); the probe and the point mutants show `make check`'s own samples are not the only guard | Testing only the real tables leaves every refusal unexercised after the cycle lands |

## 4. Rabbit holes (resolved)

- **What the published sources hold, and which rows they back** →
  [01](research/01-published-sources.md): the list's 52 exponents, eleven accepted final rows, the
  README's s_100, all retrieved 2026-10-17.
- **Where Python runs out** → [02](research/02-computed-rows-and-cost.md): 2.7 s per squaring at
  10^7; k ≥ ⌈log2 p⌉ + 8; recomputation below 200,000; the commands overlap in the gates.
- **The format against R4 and R5** → [03](research/03-format-loader-integration.md): group sources,
  byte-identical records, no digest, corrections by value.
- **A loader that stops refusing passes `make check` on valid tables** → `make check` tests the
  loader on its own tables; the gates' mutants show each refusal check bites.
- **R11** → baseline 3.52 s (clang++) and 6.13 s (g++-16); the prototype ran +0.01 s and within the
  g++ noise (±0.3 s run to run).
- **Adversarial design review** (trigger 3), run by the foreman on 2026-10-17 against the GOAL, this
  PLAN, the drafted gates and the prototype, with defective copies and 145 point mutants of the
  loader. Twelve findings, all folded in; each attack, re-applied to the revised prototype, is red
  under the revised gates (the dry runs below):
  1. **F1 (HIGH): the recomputation check was hollow.** A command embedding every answer in a
     hidden copy and recomputing nothing passed the full gate, because changing a literal made the
     copy disagree too. Now the gate runs the command from inside (`command.py`): its three pinned
     functions are traced, every row it must recompute must call one, every answer is sabotaged in
     turn and every embedded Res64 changed in turn, and each must end in `sys.exit(1)` naming the
     row (a traceback, a timeout or 0 is red); and `tables.py recompute` computes every recomputed
     row a second time, apart from the builder's code. The review's hollow command now fails the
     trace, and one that calls the functions but ignores them fails the first sabotage.
  2. **F2 (MEDIUM): the probe missed rules.** Single-token fuzzing of every row, numeric fields
     with signs, letters and 20-digit overflow, key, date and engine forms at their boundaries,
     `# end` before a source, each table's attributes in the others in their full form, a correction
     above its target. Of the review's point mutants that survived, all re-applied ones die; the new
     point-mutant step (§ 6, step 7) leaves five, argued equivalent.
  3. **F3 (MEDIUM): a fingerprint could silence R10 by superseding.** Fingerprints supersede
     nothing; a computed row may not replace a published final residue the command doesn't
     recompute. Loosening waits on Geoffrey (§ 5).
  4. **F4:** the R9 check uses the unsuperseded row: a wrong correction of 1237 is red, a
     correction away and back is green.
  5. **F5:** a second parser of the exponent table passed. The `drop-9941` wrapper shows the
     reference checks read only through the loader, and a table's path may appear only in
     `src/known.*` and the pinned constants.
  6. **F6:** the probe prints `known::TABLES` and requires the phase's list.
  7. **F7:** `AGENTS.md` is compared byte for byte outside the pinned lines and bullets.
  8. **F8:** only `spec/known-answer-tables/` is free in `spec/`; no `#include` reaches outside
     `src/` and `tests/`; the process-start grep is wider; the binaries name no interpreter path.
  9. **F9:** `ll p 4 p−2` is refused in the partial table, so the two tables can't disagree on
     the final Lucas–Lehmer residue.
  10. **F10:** § 2's refusal order now says what the readers do.
  11. **F11:** the green line says when R4 had no prior table; the standing append-only check is a
      new seed in the ledger; the vector reader and generator are compared byte for byte.
  12. **F12:** research 01's short-residue widths corrected (12, 15 and 16 bits; 10061's
      rejection was over-strict) and 28603's single-machine pair noted, its independent check
      being Python's.

## 5. Risks & open questions

- **Ascending against append-only** (recorded risk; the foreman's ruling at the design review). If
  GIMPS ever lists a Mersenne prime below 136,279,841 (the 52nd's rank is provisional), R5's
  ascending order and R4's append-only rule can't both hold. A GOAL amendment then, and only then.
- **A correction to the exponent table** has no row form (its rows carry no value): a recorded risk,
  by the same ruling. The format's magic line versions it when one is needed.
- **Fingerprints may not correct.** F3's tightening holds until Geoffrey rules on whether two `mps`
  engines that share no arithmetic count as an independent method (GOAL, Notes).
- **The command is tested from inside.** Its trace (§ 6, step 1) pins three function names and its
  failure messages; a command that computed by some other route would be refused by the gate, not by
  the GOAL. A builder who needs another shape stops and escalates; the gates are frozen.
- **The reports change.** A later retrieval may show more results, or an error code on a row taken
  here. The rows cite their retrieval date; a change is a superseding row, never an edit.
- **R7's "different programs".** Research counted Prime95 and John Sweeney's Mac program, Mlucas,
  CUDALucas, gpuowl and prpll as programs; versions of one program are not. Every row also has two
  distinct shifts or more, so it meets the rule on shifts alone.
- **Gate time.** About 12 minutes for a full gate in the dry runs; the commands overlap the builds.
  P4's gate has an `audit` mode for its own checks alone.
- **The gates' model of the format is the planner's**, as is the prototype's loader: the probe shows
  they agree, not that both read § 2 right. The adversarial review is where that gets attacked.

## 6. Verification strategy

Each phase's `verify:` is `sh spec/known-answer-tables/verify/pN.sh`. The gates are frozen at the
plan commit: each first requires `verify/` to equal the latest commit whose subject starts
`[feature] Plan known-answer-tables`, with nothing untracked there, and that commit to come before
any change to `src/`, `tests/`, `doc/`, `ci/`, the Makefile, `AGENTS.md` or `README.md`. They run
on this Mac (clang++, g++-16, `/usr/bin/make` 3.81, `python3`, `python3.14`, git, rsync, perl,
otool, strings), are **cumulative** (P*n* runs every check of P1–P*n*; none requires a phase's exact
file set), print `GATE FAIL: <reason>` and exit 1 at the first failure, or one green line. The
gate's scratch directory is removed by Python's `shutil` when it ends, and a tamper's table by
`os.remove`, each only after checking that its target lies inside the gate's own `mktemp`
directory. **Footprint:** run one gate at a time, never two together. At its peak a gate runs the
builds beside six table commands (three traced, three under `python3.14`), seven vector families,
four recompute processes and, later, three point-mutant builds and probes; a run with six
point-mutant workers was killed by the host for memory, so the step uses three. In order:

1. **starts the commands** in the background, from copies of the root: each table's command
   **traced** under `python3` from inside (`command.py trace`: it imports the tree's
   `tests/known/generate.py`, wraps the three pinned functions, runs `main`, records every call and
   its answer; the table it writes must equal the committed one byte for byte, and every row the
   command must recompute must have called its function), and run as its header says under
   `python3.14`; the seven remaining vector families under `python3`; and, from P2, `tables.py
   recompute` in four processes, a second computation of every final row below 200,000 and every
   partial row, written apart from the builder's (Python's `%` and `pow` where they finish in
   seconds, the gate's own reduction above), against PLAN § 2's pinned values;
2. **builds** a clone-like copy per compiler with `make -B check`: green, no warnings, every pinned
   label run and passed, the removed label gone, the same counts on both;
3. **holds the footprint** (`files.py`): every file outside § 2's table equals `c41950c`'s, `spec/`
   included except `spec/known-answer-tables/`; the Makefile differs by the one `MODULES` line;
   `tests/vector_reader.cpp` is `c41950c`'s with the op line gone and the pinned `STEMS` line, byte
   for byte; `tests/vectors/generate.py` is `c41950c`'s without the family's block and its
   `FAMILIES` entry, byte for byte but for the comment above `COMPOSITE`;
   `tests/vectors/mersenne-exponents.txt` is gone; new files only where § 2 puts them. And
   `wording`: no spec ids in `src/` or `tests/`; no name there of a way to start a process (`system`,
   `popen`, the exec family, `posix_spawn`, `fork`, `vfork`, `wordexp`, as a call or a pointer); no
   `#include` reaching outside `src/` and `tests/`; a table's path only in `src/known.*` and the
   pinned path constants; no network or subprocess module in the command;
4. **R11**: `bin/mps` and `c41950c`'s print the same bytes and exit alike on 26 runs covering every
   path, under `NO_COLOR` and `FORCE_COLOR`; `otool -L` lists the same libraries; `make check`
   stays green with `python`, `python3`, every `python3.N` and `uv` first on `PATH` as recording
   stubs, and none is called; and `bin/check` and `bin/mps` name no interpreter or tool path
   (`strings`), which an absolute `/usr/bin/python3` would need. (`sandbox-exec` is deprecated and
   was not used.)
5. **the tables** (`tables.py check`, its own reader of § 2's grammar): magic, command, sources,
   rows, trailer; the pinned rows of each phase's tables, each under its pinned source kind, key and
   date; the 22 exponent records byte for byte and first, against `origin/main`'s old file; every
   table that exists on `origin/main` a prefix of its new body (R4), the green line saying so when
   there is none yet; the decades of R7 and R8; the wrap rule;
6. **the probe** (`probe.cpp`, `probe.py`): a driver built against `src/known.cpp` prints
   `known::TABLES`, which must be the phase's tables in order (F6), and answers about 7,800 tables at P3:
   the real ones and the gate's samples, each unchanged or with one edit of every kind in § 2's
   rules, every token of a row replaced by each of about forty values (signs, letters, overflow at
   20 digits, the 2^32 and 2^64 edges, Res64 near-misses, attribute names), keys and dates at their
   boundaries (months 01 and 12, days 01 and 31, `2026-10-170`), engines at theirs, `# end` before a
   source, each table's attributes in the others, a correction above its target, weak corrections,
   a fingerprint that supersedes, the final residue in the partial table (F2, F3, F9). Accepted or
   refused must agree with `tables.py`, a refusal at the same line, an acceptance with the same
   sources, rows and replaced rows; a crash of the driver is a failure;
7. **the point mutants** (`pointmut.py`): every single-line `if` of `src/known.cpp` made never true
   and every comparison flipped, from its first static helper on (about 180), each built with the
   probe: each must disagree with `tables.py`, or be listed in
   `spec/known-answer-tables/equivalent-mutants.txt` with the reason no table can tell it from the
   original. Three at a time, about 2.5 minutes. The prototype's five survivors, each argued there: an empty
   token (refused at the same line by every field and attribute rule), p > 2^32 for p ≥ 2^32 (2^32 is
   even), a `#` line without its space (refused at the same line as a row), the weak-correction bound
   at 200,000 exactly (not prime), and `load`'s open check (an unopened file reads as empty text,
   refused at line 1 with its path named);
8. **the bugs** (`mutant.cpp`): `known::parse` and the two reference tests renamed and wrapped. The
   pass-through control run is green, and its trace shows both reference tests called on every
   listed p from 3 to 9941 and on none above 10,000 (R6); with `drop-9941` (the wrapper drops
   `mersenne 9941` from what `known::parse` reads) neither reference test sees 9941, so the reference
   checks read the exponents through the loader and nothing else (F5). Each bug is the wrapper
   rewriting a table's text before the real loader sees it, as a loader without one rule would read
   it; `make check` must go red in the named check with the wrapper having changed a text;
9. **the scratch-copy edits** (R3): in the wrapped copy, one table edited per run by `tables.py
   tamper`, which names the line: `make check` red in the named check and, for each refusal, the
   output naming `tests/known/<stem>.txt:<line>`; and as controls, valid edits that must leave it
   green;
10. **the documents** (`docs.py`): `AGENTS.md` is `c41950c`'s but for the five pinned list lines, the
    `src/known` bullet and the `tests/` bullet of § Map, byte for byte (F7), each list line present
    exactly when its path is; the `COMPOSITE` comment from P2; the bibliography's entries for the
    phase, each with its six fields, its URL, a Verified line dated 2026-10-17 or later, the headings
    sorted, the old entries unchanged; the two sentences naming the tables;
11. **waits for the commands**: the traced table and the `python3.14` table each equal to the
    committed one; each vector family unchanged; every second computation agreeing; then, from the
    trace's record, so each run takes a fraction of a second, **the refusals** (`command.py refuse`,
    F1): for every row the command recomputes, its function's answer sabotaged (one bit off, or a
    Lucas–Lehmer zero made one and back), and every embedded Res64 of those rows changed in the
    source; for the sweep, each listed exponent up to 10,000 and the unlisted 9973 sabotaged. Each
    run must exit 1 through `sys.exit` (a traceback, a timeout or 0 is red) with a message naming the
    row's op and key. A command that embeds its answers and never recomputes passes no part of this;
12. **R11's time**, last, once nothing runs in the background: warm `make check`, the least of five
    interleaved runs (ten when the first five miss), the tree's against `c41950c`'s in the same run,
    with each compiler: at most **+0.5 s** (baseline at planning: 3.52 s clang++, 6.13 s g++-16).

**The bugs and the tampers by phase** (each phase's gate runs its own and every earlier one's):

| Phase | Bugs (mutant ids → the check that must fail) | Tampers |
|---|---|---|
| P1 | `source-default` → no source; `conflict-blind`, `fingerprint-blind` → two answers; `supersede-blind`, `weak-correction` → supersession; `one-engine`, `runs-anywhere`, `runs-supersede` → fingerprint; `res64-case`, `count-blind`, `ascending-blind`, `prime-blind`, `agreed-blind`, `origin-blind`, `tail-blind`, `final-in-partial` → malformed | exponents: missing, a source removed, out of order, a composite p, a miscount, an unknown key, the published border moved |
| P2 | — | final: missing, a source removed, a conflict, a 15-digit Res64, `agreed=1`, 1237's Res64 changed, a row at 9941, a computed correction of 10000139 (F3), a wrong correction of 1237 (R9 red, F4); green controls: a valid correction of 10303, 1237 corrected away and back (F4) |
| P3 | — | partial: missing, k = 0, one engine, one engine twice, a fingerprint against a computed row, a fingerprint that supersedes (F3), `ll 1277 4 1275` (F9); green controls: a valid correction, a valid fingerprint |

**Gate time** (this Mac, the revised gates on the prototype): see the dry runs below.

**P4 and the cluster runs.** `verify/prepush.sh` is everything P4 runs before the push: the P3 gate
on the final tree, the deferral ledger, no uncommitted code, and the site-private scan of the
cycle's files and of `origin/main...HEAD`'s messages and patches; `sh prepush.sh scan` runs the
scan alone. `verify/p4.sh` runs the P3 gate and then P4's own checks: the ledger; each table's
list line in the commit that added the table (R12, from `git log`); three `result.json` files that
pass the job's format check, every step passed, GCC 13 or newer, redacted, job ids matching
`jobs.tsv`'s `COMPLETED 0:0`, one tested commit on `origin/feature/known-answer-tables` containing
`origin/main` whose code equals `HEAD`'s outside `spec/`, `issues/` and `ROADMAP.md`, each check
log's counts equal to this tree's, each check step's seconds under `## Times` against cycle 7's;
the scan over the evidence. **`sh p4.sh audit` runs P4's own checks without the cumulative gate**,
so a failure there costs a minute, not fifteen. `cluster.sh` and `redact.sh` are cycle 7's with
this cycle's paths and ledger. P4 is built to run as **two dispatches**: (A) the audit, the ledger,
`prepush.sh`, the push and the three submissions, stopping with the job ids; (B) the evidence, its
redaction, `## Times`, `p4.sh` and the phase commit.

**The ledger** (P4): Notes bullets with the words "deferred from `known-answer-tables`" in
`issues/known-factor-tables.md` (the factor table as a fourth stem of this format and loader),
`issues/burn-in-mode.md` (the loader takes a path; how an installed `mps` finds the tables), and
`issues/doc-claims-standing-check.md` (the exponent list now lives in the table); a new seed,
`issues/known-answer-standing-check.md`, with its `ROADMAP.md` entry and the same words: a
standing check that every table only grows against `origin/main`, which this cycle's gates check
once and no later cycle's gate will unless it asks (design review F11); every issue
matched one-to-one with a `**Seed:**` line; this cycle's seed still `adopted:`;
`evidence/ledger.md` with `## Deferrals` and `## Times`.

**Dry runs before commit** (2026-10-17, this Mac, one gate at a time): a prototype built from this
plan, in clones outside the tree, with the gates as committed. On genuine phase trees, one commit
per phase: `p1.sh` on P1 green (89 + 644 checks; the probe on 5,831 tables; 176 of 181 point
mutants killed, 5 argued; 16 bugs, 7 table edits; R11 +0.01 s clang++, +0.03 s g++-16; 8.5
minutes); `p2.sh` on P2 green (91 + 644; 6,704 tables; 18 edits; R11 −0.00 s, +0.07 s; 9.9
minutes); `p3.sh` on the final state green (91 + 644; 25 rows recomputed a second way; 47
sabotages and 25 changed literals refused; 7,767 tables; 27 edits; R11 +0.00 s, −0.01 s; 10.9
minutes). P4's R12 history check green on P3; `prepush.sh scan` clean. Every design-review attack,
re-applied to the revised prototype, red under these gates (§ 4). Before the review, the first
dry run caught a gate bug (a wrapper that dropped a row without recounting the trailer), and the
prototype's own `make check` caught a correction's reason not kept; after it, the phase-1 tree
caught a probe without a final row above the recompute bound, so P1's point mutants of the
correction-strength rule survived; the probe's final sample now has one.
---

*Backing research: [`research/00-digest.md`](research/00-digest.md).*
