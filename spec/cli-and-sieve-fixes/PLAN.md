# PLAN — Options that parse as documented, and a sieve right at its edges

> **Status:** Draft for review · **Last updated:** 2026-10-12
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). Backing detail is in
> [`research/`](research/). Every design element traces to a GOAL R-ID.

## 1. Summary

Four slices, each with a frozen gate. First the sieve, which every later `-p` check stands on: it
writes only below n, lists 2, and is right for n = 0…3. Then PVAL, parsed once in the parser with
`std::from_chars` into a number `main` uses directly, and with `--` ending the options. Then the
options themselves: one name table, one help text, and a parser that walks a short word character
by character, so the five spellings, grouped flags and `--help` agree. The last slice audits the
checks, seeds the follow-up cycle, and proves the pushed head on the three GCC cluster targets. The
Makefile does not change; the gates rebuild `make check` with the standard libraries' bounds
assertions, and inject defects around the builder's own code to show every requirement has a check
that fails.

## 2. Design

### Files

| Path | Change | R-IDs |
|---|---|---|
| `src/math.cpp` | `prime_sieve` and `display_primes` rewritten; the stubs unchanged | R8, R9 |
| `src/math.hpp` | unchanged (the gates hold it to `origin/main`) | — |
| `src/cli.hpp` | `TestCase` without `TEST_NONE`; seven-name `test_by_name`; `PVAL_MAX`; `pval_number`; `set_pval` | R1–R7 |
| `src/cli.cpp` | the argument loop, the short and long option walks, `set_pval`, `set_file_path` | R2–R7 |
| `src/main.hpp` | the `-p` and `-t` help lines and the closing grammar paragraph | R1 |
| `src/main.cpp` | no `std::stoi`; `case cli::TEST_ALL`, no `default` | R2, R6–R8 |
| `src/ansi.cpp` | `colorize_usage` colours an option only at the start of a word | R1 |
| `tests/check_math.cpp`, `tests/check_cli.cpp`, `tests/cli.sh` | checks for R1–R9; caveats removed | R10 |
| `issues/verdicts-and-exit-statuses.md`, `ROADMAP.md` | the follow-up seed and its entry | R11 |

No Makefile, README or `AGENTS.md` change. `README.md` documents no usage (GOAL non-goal), so the
coupled core's README leg has nothing to change.

### The sieve (R8, R9; [brief 03](research/03-sieve-and-checks.md))

`prime_sieve(n)` builds `n` entries, all false; sets entry 2 when `n > 2`; sets every odd `i ≥ 3`
below n; then, for odd `i` with `i * i < n`, strikes `j = i*i, i*i + 2i, …` below n. Every write is
guarded by `< n`, and `i * i` stays below 10^9 for any accepted PVAL, so nothing overflows.
`display_primes(n)` prints each `i` from 2 below n whose entry is true, one per line, and nothing
else. Its doc block cites the sieve of Eratosthenes as Crandall and Pomerance, *Prime Numbers: A
Computational Perspective*, 2nd ed., 2005, ch. 3 (a section number only if confirmed). Signatures,
the three stubs and `test_all` are untouched (GOAL non-goal: the tests' arithmetic).

**"Writes nothing outside them."** For n = 1 and 2 the old stray write lands inside the vector's
storage word, which no value check and no ASan can see; only the standard library's bounds
assertions catch it (libc++ from its fast hardening mode; libstdc++ from GCC 15 under
`_GLIBCXX_ASSERTIONS`). The Makefile stays as it is (§ 3, deviation 1): `make check` pins the size
and every entry for n = 0…3 and every index from 0, and `tests/check_math.cpp`'s header names the
hardened invocation that makes it show the bounds too:
`make -B CXXFLAGS='-O1 -Wall -Wextra -D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE' check`.
The gates run exactly that, with both compilers, behind a canary.

### PVAL (R6, R7; [brief 02](research/02-pval-validation.md))

- `cli.hpp`: `static const long unsigned PVAL_MAX = 999'999'999;` in `namespace cli`; the field
  `long unsigned pval_number = 0;` beside `std::string pval` (kept as typed).
- `Interface::set_pval()` runs after the argument loop, once `show_primes` is final, so `0 -p` and
  `-p 0` agree. It parses `pval` with `std::from_chars` (`<charconv>`, `<system_error>`) and throws
  `argument_error` unless `ec == std::errc()`, every character was consumed, and the value lies in
  [2, `PVAL_MAX`], or [0, `PVAL_MAX`] under `-p`. The message, built from the constant:
  `Invalid PVAL '<pval>': expected a decimal integer from <min> to 999999999`, with
  ` with -p/--primes` appended under `-p`. It names PVAL and quotes the value, so `''` and `' 31'`
  are visible.
- The default `file_path = pval + ".mps"` is set after `set_pval` succeeds, from the string as typed
  (GOAL non-goal: the default checkpoint path).
- `main.cpp`: `display_primes(opt.pval_number)` and `const long unsigned pval = opt.pval_number;`.
  No `std::stoi` remains, so no PVAL reaches "Uncaught exception".

### The argument loop and the options (R2–R5; [brief 01](research/01-parser-grammar.md))

- **Loop.** After `--`, every word is positional. `--` itself sets that mode and is *not* pushed
  (today it becomes PVAL). A word starting `--` is a long option; a word of two or more characters
  starting `-` is a short word; anything else, a lone `-` included, is positional. One positional is
  PVAL; none or more than one is an argument error, as today.
- **Short word.** Copy the word (`pop_opt` overwrites `opt`), then walk it from index 1:
  `t` or `f` takes the rest of the word with one leading `=` removed, or, when nothing follows,
  the next word, and ends the walk; `h` throws the help and `v` the version at once, so later
  characters are never read (`-hx` is the help, `-xh` an error); `p s d D` set their flags in order;
  any other character is `argument_error("Unrecognized option: -<c>")`. A valued option inside a
  group (`-dtll`, `-dt ll`) falls out of this walk and is not pinned (GOAL non-goal).
- **Long option.** Split at the first `=`. `--test` and `--file` take the text after `=` (possibly
  empty) or, without `=`, the next word, through the same `set_test_case` / `set_file_path`. A
  boolean long option given `=…` is an argument error; an unknown one is `Unrecognized option`.
- **Messages.** A missing value is `Missing value for -t/--test` or `Missing value for -f/--file`,
  whatever the spelling. An empty `-t` value reaches `set_test_case` and gets R3's error naming
  `''`. An empty `-f` value is `Empty value for -f/--file`. Duplicate-use warnings stay.
- **`set_file_path`** records the path and nothing else: no `std::ofstream`, no file, no "Could not
  open file" (§ 3, deviation 2). `<fstream>` leaves `cli.cpp`.
- **Names.** `test_by_name` holds exactly `all`, `tf`, `trial-factor`, `p1`, `p-minus-1`, `ll`,
  `lucas-lehmer`; `TEST_NONE` goes; an unknown name is
  `Unrecognized option value '<name>' for -t/--test` as today. `main.cpp`'s switch gets
  `case cli::TEST_ALL: test_all(pval); break;` and no `default`, so `-Wswitch` catches a missed
  enumerator.

### The help text (R1)

`mps --help` prints exactly this (under `NO_COLOR`); only the `-p` and `-t` lines and the closing
paragraph differ from `main`, and the usage line, `-s`, `-f` and the rest are unchanged:

```
Usage:
  mps [-hv] [-p] PVAL [-s] [-t NAME] [-f PATH] [-d] [-D]
  Test if number is Mersenne prime.

Arguments:
  PVAL                    Mersenne prime exponent.

Options:
  -p, --primes            Print the primes below PVAL.
  -s, --skip-ptest        Suppress initial prime test on PVAL.
  -t, --test        NAME  One of 'tf' ('trial-factor'), 'p1' ('p-minus-1'),
                          'll' ('lucas-lehmer') or 'all' (default: 'all').
  -f, --file        PATH  File path for checkpointing.
  -d, --debug             Show debug messages.
  -D, --detailed          Show detailed logging messages.
  -v, --version           Show version and exit.
  -h, --help              Show this message and exit.

  Give a valued option as -x VALUE, -xVALUE, -x=VALUE, --long VALUE or
  --long=VALUE. Short flags may be grouped (-dD).
```

`colorize_usage` (`src/ansi.cpp`) matches an option only at the start of the text or after a space,
`[` or `(`, so the hyphens inside `'p-minus-1'`, `'trial-factor'` and `'lucas-lehmer'` are not
coloured as options and the aliases read whole in a terminal (§ 3, deviation 3).

### The checks (R10)

What the gates require of `make check`, beyond passing:

- Every one of R1–R9 has a check that fails when that behaviour breaks (§ 6, the mutants). No file
  under `tests/` describes these defects as known or accepts `-p` with or without its 2.
- Unit checks parse through `cli::parse_args(argc, argv)`, the free function, and never construct
  `cli::Interface` and call its member; `tests/cli.sh` passes the environment through to `bin/mps`
  (it may unset named variables, as today, but not run under `env -i`). The gates inject defects
  through both.
- Recommended shapes (briefs 01 § 6, 02 § 5, 03 § 4): a `describe(Interface)` string of every field,
  compared across all five spellings of `-t` and `-f` with the path `no-such-dir/x=y.mps`; R3's
  rejected values in every spelling, with the quoted value in the message; grouped flags against
  the same flags given separately; the PVAL table at parse level, including `-p 999999999` (never
  run end to end); in `tests/cli.sh`, whole-stderr comparisons per `-t` name, the bad PVALs with
  their exit status, message and no `Running` line, and `-p N` output compared exactly, trailing
  newlines included (`$( )` strips them, so capture with a sentinel or check `display_primes` by
  redirecting `std::cout`). One `expect` per condition: `expect NAME [ a ] && [ b ]` counts only
  `[ a ]`. `planned` matches the count.

### Requirement → design map

| R-ID | Design element(s) that satisfy it |
|------|-----------------------------------|
| R1   | The help text above (names, aliases beside them, `all` the default, the grammar stated once, `-p` "below PVAL"); `colorize_usage` keeps the aliases whole |
| R2   | Seven-name `test_by_name`; `case cli::TEST_ALL` and no `default`; the stubs' start lines unchanged |
| R3   | `set_test_case` rejects any other value, the empty string included, naming it; `TEST_NONE` gone |
| R4   | The short walk (`-x VALUE`, `-xVALUE`, `-x=VALUE`) and the long split (`--long VALUE`, `--long=VALUE`) feeding one setter per option; `--file` calls `set_file_path`; `set_file_path` only records |
| R5   | The short walk sets flags in order, `h`/`v` ending the run, any other character an argument error |
| R6   | `set_pval` with `std::from_chars`, [2, `PVAL_MAX`], the message naming PVAL; `--` ends the options; no `std::stoi` in `main` |
| R7   | `set_pval`'s [0, `PVAL_MAX`] bound under `-p`, before anything prints |
| R8   | `display_primes` from 2; the validated `pval_number` reaches it; n ≤ 2 prints nothing |
| R9   | `prime_sieve` writes only below n; checks for n = 0…3 and every index from 0; the hardened `make check` |
| R10  | The checks above; the caveat lines in `tests/check_cli.cpp`, `tests/check_math.cpp` and `tests/cli.sh` removed; the gates' mutants |
| R11  | P4 adds `issues/verdicts-and-exit-statuses.md` (`status: unshaped`) and its `ROADMAP.md` entry right after this cycle's |

## 3. Invariant gate (AGENTS.md constitution check)

Checked against `AGENTS.md` § Invariants before research, and again after this design was drafted
and after the adversarial review was folded in.

- **High-blast-radius paths.** The change touches `src/math.cpp` (`prime_sieve`,
  `display_primes`) and no other listed path; `src/math.hpp` is held unchanged. A CONFIRMED review
  finding there needs Geoffrey's decision before landing. The list does not widen: no engine,
  verdict-printing driver, checkpoint or result format, or answer table is added.
- **§ 1 Verdicts are earned.** Untouched: no verdict is printed, the stubs and `test_all` are as
  they are, and the dispatch changes only which names reach which stub. The sieve feeds `-p` only.
- **§ 2 Arithmetic is checked.** No engine. The flags that compile numeric code are unchanged; the
  bounds assertions live only in the gates' copies (deviation 1).
- **§ 3 Known answers.** No expected value is added to the tree. The P1 gate checks π(10^8) =
  5,761,455, a published count, inside the gate.
- **§ 4 Clean room.** The sieve comes from the mathematics, cited to Crandall and Pomerance; the
  parser and `std::from_chars` are our own and the standard library's.
- **§ 5 Dependency-free core.** `<charconv>` is the standard library; nothing links in.
- **§ Conventions.** The parser and the help text change in one commit (P3); the README has no usage
  to move (GOAL non-goal). No R#/P# ids in `src/` or `tests/` (the gates grep). The sieve's comment
  cites its source. New code follows the code style (tabs, `not`/`and`/`or`, `<value>_by_<key>`
  tables, doc blocks at the top of a body, `UPPER_SNAKE` constants with digit separators).

### Deviation justifications

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| 1. R9's "writes nothing outside them" is shown by `make check` rebuilt with the libraries' bounds assertions (the gates run it; `tests/check_math.cpp` names the command), not by the default build | For n = 1 and 2 the stray bit sits inside the storage word: no value check, no ASan, only the library's assertions see it | Hardening the Makefile's flags adds bounds checks to every future hot loop and edits flags § Invariants 2 owns; a separate hardened object set for `bin/check` would check code that does not ship; `.at()` in the sieve adds a compare per write and cannot be told apart from a guard by a check |
| 2. `-f` no longer opens or creates the file while parsing; "Could not open file" goes | R4 sets the option to exactly VALUE, and the GOAL says nothing reads or writes the file yet; probing it creates `out.mps` (or `=out.mps`) wherever `make check` or a user runs | Keeping the probe makes every spelling check create files and couples parsing to the filesystem; the checkpoint cycle can check writability where it writes |
| 3. Three parser and help defects beyond the GOAL's list are fixed: `--` became PVAL, a lone `-` was an unknown option, and `colorize_usage` split the new hyphenated aliases | Each sits in the code this cycle rewrites; `--` is how R6's checks hand `-1` to PVAL; the colour split is introduced by R1's own text | Leaving them would need issues of their own for one-line fixes in code already under change, and would ship a help that reads broken in a terminal |

## 4. Rabbit holes (resolved)

- **Why `mps -p 0` ends in `bad_alloc`** → undefined behaviour, not `stoi`: the write to entry 2 of
  an empty vector lets clang drop the allocation's `n > 0` guard and request 2^61 bytes; GCC
  segfaults ([03](research/03-sieve-and-checks.md)).
- **Seeing an in-word out-of-range write** → only the libraries' bounds assertions; libc++ from
  fast mode, libstdc++ from GCC 15 (the clusters' GCC 14 would need `_GLIBCXX_DEBUG`); a canary
  makes the hardened run trustworthy ([03](research/03-sieve-and-checks.md)).
- **`from_chars` across libraries** → identical on libc++ and libstdc++ for every input tried,
  signs, whitespace, Arabic-Indic digits, `0x1f`, leading zeros and overflow included
  ([02](research/02-pval-validation.md)).
- **Sequencing** → the sieve lands before PVAL validation, or a valid `-p 0` crashes
  ([02](research/02-pval-validation.md)).
- **The parser's shape** → one character walk for short words, one split for long ones, feeding the
  existing setters; getopt behaviour for valued options in a group falls out unasked
  ([01](research/01-parser-grammar.md)).
- **Gates that do not depend on the builder's code** → the gates rename the tree's `parse_args`,
  `prime_sieve` and `display_primes` with a prepended `#define` and link `verify/mutant.cpp` in
  their place, so each defect is injected around whatever the builder wrote; a pass-through control
  proves the wrappers transparent ([00](research/00-digest.md), item 11).
- **Adversarial design review** (trigger 3): a fresh reviewer read the GOAL, this design and the
  drafted gates, built a variant with two parser defects and a weakened check suite, and got P3
  green on it (68 mutants caught). Thirteen findings; all were folded into the gates before commit:
  1. *PVAL past 2^64 was never tried*, so a wrapping hand-rolled parse passed: both bad-PVAL lists
     now hold 2^64, 2^64 + 31, a zero-padded and a 40-digit overflow, with `v-wrap` and `pv-wrap`
     mutants;
  2. *a lenient `-p` check could survive* beside a strict one elsewhere: under `d-no2`, the `cli`
     half of `make check` itself must fail;
  3. *nothing had to pin "that test alone"* at the dispatch: four edit mutants send each `case` in
     `main.cpp` to another stub;
  4. *the `-t` spelling mutants were caught for the wrong reason* (an invalid name): each now swaps
     the value for another valid name;
  5. *`prime_sieve(0)` and `(1)` needed no check*: the small-n mutants are one per n (sizes for 0, 1,
     2; entries for 1, 2, 3; the stray write for 1 and 2 separately);
  6. *the PVAL mutants fired on "Missing positional argument: PVAL"*: they fire only on a bad PVAL's
     own error (`PVAL '` with one positional);
  7. *aliases could be swapped between names*, and two spellings had no help mutant: an alias-swap
     edit and deletions of `-x VALUE, ` and `--long VALUE or`;
  8. *a value starting with `=`* lost all its `=`s in a variant: `-f==x`, `--file==x`, `-f==` and
     R3's `=ll` are pinned;
  9. *R3 and R7 ran narrower lists than the GOAL*: R3's values run in all five spellings, and one
     bad-PVAL list serves both modes;
  10. *over-pinning*: the pinned surface (§ 5) now names the `TestCase` enumerators and the
      messages the gates read;
  11. *gates read the working tree but are judged on `HEAD`*: each gate first requires `verify/` to
      match the plan commit with nothing untracked, and P4 requires a clean code tree;
  12. *probes ran in the caller's directory*, `p3.sh` listed six test files by name, and
      subdirectories under `tests/` broke the caveat walk: probes run in an empty directory that
      must stay empty, and the walk covers every file at any depth;
  13. *R5's coverage was thin*: a `g-drop-info` mutant (`-hx` becomes an error), and `-dd`/`-ss` must
      match `-d -d`/`-s -s` on stderr, duplicate warnings included.

## 5. Risks & open questions

- **The gates pin the design's surface**: the exact help text; the `cli::Interface` field names the
  probes read (`pval`, `pval_number`, `test_case`, `opt_test_case`, `file_path`, `opt_file_path`,
  `show_primes`, `skip_ptest`, `debug_mode`, `detailed_mode`) and the enumerators `TEST_ALL`,
  `TEST_FACTOR`, `TEST_P_MINUS_ONE`, `TEST_LUCAS_LEHMER` (names unchanged); `math.hpp`'s signatures
  and the dispatch calling `test_quick_factor(`, `test_p_minus_one(`, `test_lucas_lehmer(` and
  `test_all(` once each in `main.cpp`; the PVAL message's `PVAL '<value>'`; an unknown short flag's
  message naming `-<c>`; `Duplicate use of -<c>/` warnings; a lone `-` as a bad PVAL; an empty
  `-f` as an error; and the two constraints on the checks in § 2. A builder who needs otherwise
  stops and escalates; the gates are frozen.
- **The wrapper trick** compiles `cli.cpp` with `parse_args` renamed, so the class `Interface` is
  seen with two different member names in two translation units. It is an ODR violation in
  principle; both compilers link and run it, and the control run (no mutant, green) is the check.
- **The hardened leg is local.** The clusters' GCC 14 lacks `vector<bool>` assertions; the cluster
  runs prove plain `make check` on GCC and Linux, the gates prove the bounds on the Mac.
- **Gate time.** P3 and P4 take about three minutes on this Mac (two compilers, 82 mutants, the
  hardened rebuilds, `-p 100000000`).
- **Cluster queues.** Two targets wait for idle nodes and the GB10 cluster runs one job at a time; P4
  has no time-critical step.
- **Retiring this cycle's seed.** `issues/results-and-cli-fixes.md` stays `adopted:` with its entry
  until `code-roadmap` retires both after landing; P4's new entry goes directly after it.

## 6. Verification strategy

Each phase's `verify:` is `sh spec/cli-and-sieve-fixes/verify/pN.sh`. The gates are frozen at the
plan commit: review checks `git diff --quiet <plan commit> HEAD -- spec/cli-and-sieve-fixes/verify`.
They run on this Mac and need clang++, g++-16, GNU make 3.81 at `/usr/bin/make`, git, rsync and awk;
P4 adds python3 and the site file. Each gate is cumulative, so a later phase cannot break an
earlier one's behaviour, and each:

1. **builds** a clone-like copy per compiler with `make -B CXX=… check`, green, no warnings, both
   summaries non-zero;
2. **drives `bin/mps`** in an empty directory with a clean environment and checks exit statuses and
   exact stdout and stderr: `-p N` against an awk trial-division list for N from 0 to 7,920 and every
   `-p` spelling; π(10^8) by line count and last line; bad and good PVALs with and without `-p`; the
   exact help; each `-t` name's exact start lines; R3's values in three spellings; grouped flags;
   and that no run leaves a file behind;
3. **probes** the tree's own modules with gate programs (`probe-sieve.cpp`: every n from 0 to 3,000
   and five large n against trial division or a gate sieve, and n up to 64 under the bounds
   assertions; `probe-parse.cpp`: every field after `cli::parse_args`, compared across the 50
   spelling combinations, the groups and their separated forms);
4. **proves `make check` pins each requirement**: through `verify/mutant.cpp`, each injected
   defect (wrong listings; sieve entries and sizes, one small n at a time; junk, out-of-range and
   wrapping PVALs accepted; messages that drop PVAL; a test run or stdout before an error; each `-t`
   name mapped wrong; each rejected name accepted; each spelling's value swapped for another; groups
   truncated, reversed or cleaned) must turn `make check` red with a failed check and must have been
   reached, and a listing without its 2 must fail `tests/cli.sh` itself; edits to `src/main.hpp`'s
   help and to `src/main.cpp`'s dispatch must turn it red; `d179aa4`'s sieve verbatim must turn it
   red; and `make check` rebuilt with the bounds assertions must be green, and red under the old
   in-word write for n = 1 and for n = 2. 17 mutants at P1, 31 at P2, 82 at P3 and P4;
5. **greps** `tests/` for the old caveats and `src/` and `tests/` for spec ids, and holds
   `src/math.hpp` and `README.md` to `origin/main`;
6. **refuses to run** unless `verify/` matches the plan commit with nothing untracked in it.

P4 adds R11's seed and entry, the deferral ledger, and the cluster evidence: three runs of one
pushed commit, through the cluster MCP server, each `COMPLETED 0:0` with GCC 13 or newer, the
commit's own `ci/test-commit.sh`, and a check log whose summaries equal the local counts; the
tested commit's code equal to `HEAD`'s outside `spec/`, `issues/` and `ROADMAP.md`; and the
site-private scan of files and history. Cycle 2's negative control is not repeated: this cycle does
not change the job.

**Dry runs before commit**, on scratch clones of this branch with `verify/` committed as the plan
commit. Each gate went green on a prototype of the design at its own phase (P1 in about 70 s, P2 in
80 s, P3 in 200 s), and P4 green on synthetic evidence (190 s). Today's tree turns P1 red
(`mps -p 0`, exit 30); a P1-only tree turns P2 red (`--` taken as PVAL); the reviewer's variant
(a wrapping PVAL parse, `=`-stripping, a weakened suite), green under the first draft, turns P3 red
(`mps -- 18446744073709551647` exits 0). A prototype suite without its R1 help checks and its n < 4
sieve check left the matching mutants green, and the gate named them. An uncommitted code change
turns P4 red, and an edited or added file in `verify/` stops every gate at once.
The freeze check finds the plan commit by its subject, `[fix] Plan cli-and-sieve-fixes…`; a revised
plan keeps that prefix.

---

*Backing research: [`research/00-digest.md`](research/00-digest.md).*
