# Research digest — cli-and-sieve-fixes

Three briefs, all prototyped in scratch on this Mac (Apple clang 21 with libc++, Homebrew g++-16 with
libstdc++, GNU make 3.81): [01 — parser grammar](01-parser-grammar.md),
[02 — PVAL validation](02-pval-validation.md), [03 — the sieve and its checks](03-sieve-and-checks.md).
Trigger 2 fired (`src/math.cpp` is on the high-blast-radius list), so the fan-out ran; it was kept to
three codebase briefs. Decisions, with each cross-brief difference resolved once:

1. **The sieve** (03 §2). `prime_sieve(n)` starts all-false, sets entry 2 only when n > 2, sets every
   odd i ≥ 3, then strikes odd multiples from i·i while `i * i < n`. Every write is below n, so n < 3
   returns n false entries and nothing outside them is touched. `display_primes` loops from 2.
   `src/math.hpp` and the three test stubs do not change. The comment cites Crandall and Pomerance,
   *Prime Numbers: A Computational Perspective*, 2nd ed., 2005, ch. 3; a section number only if the
   builder confirms it. `std::endl` → `'\n'` is out of scope.
2. **Order** (02 §3). The sieve lands before PVAL validation: once `-p 0` validates, the old sieve
   crashes on it (exit 139 on GCC, `bad_alloc` from clang's optimizer on the out-of-range write).
3. **"Writes nothing outside them"** (03 §3). Plain `make check` cannot see the old defect for
   n = 1 and 2: the stray bit lies inside the storage word, so no value check and no ASan sees it.
   The libraries' bounds assertions do: libc++ from its fast hardening mode, libstdc++ under
   `_GLIBCXX_ASSERTIONS` from GCC 15 (GCC 13–14 only under `_GLIBCXX_DEBUG`). *Resolved:* the
   Makefile stays as it is (hardening the shipped build puts checks in future hot arithmetic, which
   § Invariants 2 owns; a second object directory would check code that does not ship). `make check`
   pins sizes and every entry for n = 0…3; the gates rebuild `make check` with both libraries'
   assertions and require it green, behind a canary that proves the assertions trap; and
   `tests/check_math.cpp`'s header names that invocation, so anyone can make `make check` show it.
4. **PVAL** (02). `std::from_chars` into `long unsigned pval_number`, identical on both libraries:
   it rejects signs, whitespace and non-ASCII digits, accepts any run of leading zeros, and reports
   overflow; the condition is `ec == std::errc()`, `ptr == end` and the bounds. A new
   `Interface::set_pval`, after the argument loop (so `0 -p` and `-p 0` agree), replaces the 9-character
   check from `257acb7`. `cli::PVAL_MAX = 999'999'999` is stated once. `pval` keeps the string as
   typed and the default checkpoint path stays `pval + ".mps"`, set after validation. The message is
   `Invalid PVAL '<str>': expected a decimal integer from <min> to 999999999`, plus
   ` with -p/--primes` under `-p`. `main` loses both `std::stoi` calls.
5. **The parser** (01 §2). A short word is walked one character at a time: `t`/`f` take the rest of
   the word (one leading `=` dropped) or the next word; `h`/`v` end the run at once; `p s d D` set
   their flags; any other character is `Unrecognized option: -<c>`. Long options split at the first
   `=` and reach the same handlers; `=` on a boolean long option is an error. A valued option inside
   a group (`-dtll`) falls out of the walk and is left unpinned. Missing values read
   `Missing value for -t/--test` / `-f/--file`; an empty `-t` is R3's error naming `''`; an empty
   `-f` is `Empty value for -f/--file`. Duplicate-use warnings stay.
6. **`--` and a lone `-`** (01 §1, 02 §1). Both briefs found `--` pushed as PVAL (`mps -- 31`
   fails; `mps --` exits 30). *Resolved:* `--` ends the options and is not PVAL, and a lone `-` is a
   positional (a bad PVAL). Both land with PVAL validation (P2), because R6's checks need `--` to
   hand `-1` to PVAL.
7. **`--file`** (01 §4). `set_file_path` records the path and nothing else: no `std::ofstream`, no
   file created, no "Could not open file". This is the GOAL's non-goal read literally ("`-f` sets the
   path and nothing reads or writes the file yet"), and it keeps `make check` and the gates from
   creating `out.mps` or `=out.mps`.
8. **Names and dispatch** (01 §3). Seven names in `test_by_name`; `TEST_NONE` goes; the `main.cpp`
   switch gets `case cli::TEST_ALL` and no `default`, so `-Wswitch` flags a missed enumerator.
9. **Help** (01 §5). The text is fixed in PLAN § 2 (01's draft, with "Print the primes below
   PVAL."). *Resolved* 01's open question: `colorize_usage` colours `-minus`, `-factor` and
   `-lehmer` inside the new aliases, splitting them in a terminal, so its option pattern matches only
   at the start of a word (`src/ansi.cpp`). Without it R1's aliases read broken wherever colour is on.
10. **Checks** (01 §6, 02 §5, 03 §4). A `describe` string of every parsed field, compared across all
    five spellings of both options with a path that would expose the old `=` bug
    (`no-such-dir/x=y.mps`); whole-stderr comparisons in `tests/cli.sh`, so one test cannot pass while
    all three run; one `expect` per condition; exact stdout for `-p`, trailing newlines included;
    the parse-level unit check alone for the `-p` upper bound (the run would print 50M lines).
11. **Gate mutants.** *Resolved* between 03 §5 (a gate-written replacement `math.cpp`) and the
    parser's needs: the gates interpose on `cli::parse_args`, `prime_sieve` and `display_primes` by
    renaming the tree's definitions with a prepended `#define`, so the defects are injected around
    the builder's own code whatever its shape; the help's defects are edits to `src/main.hpp`; and
    03's m0, `d179aa4:src/math.cpp` verbatim, must turn `make check` red too.
