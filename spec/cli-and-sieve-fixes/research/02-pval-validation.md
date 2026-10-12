# Research 02: validating PVAL and `-p N`

Scope: R6, R7, and the parse half of R8. Measured on `1516ae4`. The tree was built in scratch with Apple clang 21 (libc++), and a prototype was built with both clang and g++-16 16.2 (libstdc++).

## 1. Behaviour today

`NO_COLOR=1`. Stdout was empty in every row.

| args | exit | stderr, first line |
|---|---|---|
| `abc`, `''`, `٣` (U+0663) | 30 | `critical [mps] Uncaught exception: stoi: no conversion` |
| `31abc`, `3.5`, `' 31'`, `'31 '`, `+31`, `0`, `1`, `2` | 0 | `info [mps] Running quick factor test` (runs the test on the stoi prefix) |
| `1000000000`, `0000000031` | 1 | `PVAL must be less than ~2^30 (9 digits)` (wrongly rejects `0000000031`) |
| `-- -1` | 1 | `Unexpected positional argument: -1` (see below) |
| `-1`, `-p -1` | 1 | `Unrecognized option: -1` |
| `-p 0` | 30 | `Uncaught exception: std::bad_alloc` |
| `-p 1`, `-p 2` | 0 | (empty; out-of-range write in the sieve) |
| `-p abc` | 30 | `Uncaught exception: stoi: no conversion` |
| `-p 1000000000` | 1 | the 9-digit message |

**Adjacent defect.** `--` is pushed into `positional` itself (`src/cli.cpp`, `if (opt == "--" or positional_only)`), so `mps -- 31` fails with "Unexpected positional argument: 31", and `-- -1` never reaches PVAL validation. GOAL does not name this defect (see Open questions).

## 2. `std::from_chars` vs a hand-rolled loop

I parsed 24 inputs into `unsigned long` with `std::from_chars`. libc++ and libstdc++ gave **byte-identical** results:

- Valid: `0`, `1`, `2` and `999999999` give `ec == errc()` with every character consumed. So do `0000000031` (gives 31), 30 zeros then `31` (gives 31), and `000000000000` (gives 0).
- `invalid_argument` with nothing consumed: `abc`, `''`, `' 31'`, `'\t31'`, `+31`, `-31`, `-0` and `٣`. For unsigned types, from_chars rejects both signs, skips no whitespace, and reads ASCII digits only, independent of locale.
- `ec == errc()` but `ptr != end`: `31abc`, `3.5`, `'31 '`, `0x1f` (gives 0) and `1e3` (gives 1). The `ptr == end` check is what catches trailing junk.
- `result_out_of_range` with every digit consumed and the value untouched: `18446744073709551616` and longer.

**Recommend `std::from_chars`.** It is standard, needs no library, and is already verified on both standard libraries. One condition covers every case: `ec != std::errc() or ptr != last or value < min or value > PVAL_MAX`. The empty string needs no special case because it is `invalid_argument`. GCC has had integer `from_chars` since GCC 8, so GCC 13/14 is fine. A hand-rolled loop is about as short, but it has to handle the empty string and the overflow itself. It brings no gain, because from_chars already rejects exactly the characters a loop would.

## 3. Where the check lives, and what it stores

I prototyped this in scratch: `make check` passed, and both compilers built it with no warnings under the repo's flags.

- **Put it in the parser.** Add `void Interface::set_pval(const std::string& str)`, modelled on `set_test_case` and `set_file_path`. Call it after the argument loop, where `pval = positional[0];` and the 9-character check stand today. At that point `show_primes` is final, so `mps 0 -p` and `mps -p 0` behave the same. The parse-level check below pins that order.
- **Fields.** Keep `std::string pval;` exactly as typed: `check_cli.cpp` and the default file path use it. Add `long unsigned pval_number = 0;`. On LP64, `long unsigned` is the same type as `size_t`, so it feeds both `display_primes(size_t)` and the tests' `long unsigned`. Add the constant `static const long unsigned PVAL_MAX = 999'999'999;` in `cli.hpp` inside `namespace cli`, next to `TestCase`, in UPPER_SNAKE with a digit separator. Build the message from the constant so the bound is stated once.
- **`main.cpp`.** Change `display_primes(std::stoi(opt.pval))` to `display_primes(opt.pval_number)`, and `const long unsigned pval = std::stoi(opt.pval);` to `= opt.pval_number;`. After that, `main` holds no `stoi`, and nothing in it can throw `invalid_argument` or `out_of_range` for PVAL.
- **Includes.** Add `<charconv>` and `<system_error>` (for `std::errc`) under `// Standard libs` in `cli.cpp`. `cli.hpp` uses `std::vector` without including `<vector>`, and the prototype added it. That is optional hygiene.
- **Message.** Follow the existing quoting style (`Unrecognized option value 'X' for -t/--test`):
  - without `-p`: `Invalid PVAL 'abc': expected a decimal integer from 2 to 999999999`
  - with `-p`: `Invalid PVAL 'abc': expected a decimal integer from 0 to 999999999 with -p/--primes`

  The quotes make `''`, `' 31'` and `'31 '` visible. Prototype output (`critical [mps] Invalid PVAL ' 31': …`): every row in §1 except `--`/`-1` became exit 1. `2`, `999999999`, `0000000031` and 30 zeros plus `31` each ran the tests (exit 0).
- **Sequencing hazard.** With validation alone in place, `-p 0` and `-p 000000000000` now reach `prime_sieve(0)`, which segfaulted (exit 139) in the prototype. Land the sieve fix (R8/R9) before or in the same commit as the PVAL change. Never land PVAL validation first.

## 4. Default checkpoint path

Keep `file_path = pval + ".mps"` using the string as typed: `0000000031` gives `0000000031.mps`. GOAL lists the default checkpoint path as a non-goal, and nothing reads that file yet. The default path must be set after `set_pval` succeeds, as it is today, so that a bad PVAL never yields a path. One pre-existing quirk is out of scope: `-f out.mps abc` still creates `out.mps` before PVAL is rejected, because `set_file_path` opens the file while parsing.

## 5. Checks without hollow assertions

**`tests/check_cli.cpp`** (parse level, fast, holds the upper bounds). Add a static helper that returns the caught `argument_error` message, or `""` if nothing was thrown. A missing throw then fails the check instead of passing it.

- Rejected without `-p`: `abc`, `31abc`, `3.5`, `" 31"`, `"31 "`, `+31`, `""`, `0`, `1` and `1000000000`. For each, the message is non-empty, contains `PVAL`, and contains `'<value>'`.
- Rejected with `-p`: `abc` and `1000000000`, under the same three conditions.
- Accepted: `{"mps","2"}` gives `pval_number == 2`. `{"mps","999999999"}` gives `999'999'999`. `{"mps","0000000031"}` gives 31 and `pval == "0000000031"`. `{"mps","-p","0"}` gives 0. `{"mps","-p","999999999"}` gives `999'999'999` with `show_primes`. This is the only place the `-p` upper bound is exercised; never run it, because it would allocate about 125 MB and print about 50M lines.
- Order: `{"mps","0","-p"}` is accepted, and `{"mps","0"}` is rejected.

**`tests/cli.sh`** (end to end). `run` and `run_stderr` each capture one stream. A single built-in-only capture of both streams with a separate status needs a temp file, so run the binary twice. It is deterministic, and the stub runs take about 2 ms (`mps 999999999` measured at 0.002 s). For each bad value in a loop (`abc 31abc 3.5 ' 31' 0 1 1000000000 ''`):

1. `run_stderr "$v"`, then expect `status -eq 1`.
2. `case $err in *"Uncaught exception"*) bad`.
3. Expect `*"PVAL '$v'"*`, which names both PVAL and the value.
4. Expect no `*"Running "*` start line.

For `-p abc` and `-p 1000000000`, also `run -p "$v"`: expect status 1 and `[ -z "$out" ]`.

The accept side: `run_stderr 999999999` and `run_stderr 0000000031` each exit 0 and log `Running Lucas-Lehmer test`. Bump `planned` by the exact count, and delete the header's "Only behaviour that is right today" caveat (R10). The existing "a 10-digit PVAL exits 1" check is superseded by the `1000000000` row.

## Recommendations

1. Validate PVAL in a new `cli::Interface::set_pval(const std::string&)`, called after the argument loop. It replaces the 9-character check from `257acb7`.
2. Parse with `std::from_chars` into `long unsigned pval_number`. Reject unless `ec == std::errc()`, `ptr == str.data() + str.size()`, and `min <= value <= PVAL_MAX`, where `min` is 0 with `-p` and 2 without.
3. Add `static const long unsigned PVAL_MAX = 999'999'999;` in `namespace cli` (`cli.hpp`), and build the message from it.
4. Keep `std::string pval` as typed. The default `file_path = pval + ".mps"` is unchanged and still set after validation.
5. Message: `Invalid PVAL '<str>': expected a decimal integer from <min> to 999999999`, with the suffix ` with -p/--primes` when `-p` is set. Throw `cli::argument_error`, which `main` already maps to `ERR_ARG` on stderr.
6. Remove both `std::stoi` calls from `main.cpp`, and use `opt.pval_number`.
7. Land the sieve fix no later than this change, because a valid `-p 0` otherwise reaches the out-of-range sieve write and crashes.
8. Hold the boundaries with the parse-level checks in §5. The `-p 999999999` acceptance is unit-only, and the end-to-end checks in `cli.sh` assert the exit status, `PVAL '<v>'`, no "Uncaught exception", no "Running", and empty stdout for `-p`.

## Open questions

- Should this cycle fix `--` being pushed as a positional? It is a one-line change (`positional_only = true; continue;` without the push). Without it, `mps -- 31` fails and a negative PVAL can't be tested as a PVAL error. `mps -1` is still an argument error ("Unrecognized option: -1"), which arguably satisfies R6 because it never reaches PVAL. Since R4 says the options parse as documented, the fix fits the cycle's theme, but GOAL doesn't name it.
