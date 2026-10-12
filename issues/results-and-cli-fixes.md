---
status: adopted:cli-and-sieve-fixes
kind: fix
appetite: small
---

# Say what was found: verdicts, exit statuses, and the options that don't parse

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

A run never says what it found. The three tests return `bool` and `main` throws the value away
(`src/main.cpp:40-56`), so `mps 31` prints three log lines and exits 0. The three tests answer
different questions: trial factoring finds a factor or doesn't; Lucas–Lehmer proves prime or
composite; PRP says probable prime or composite. A `bool` can't carry that. The author's uncommitted
2024 work shows the strain: its trial factoring returns `true` both for "factor found" and for
"exponent not prime", and `test_all` joins the three with `or`.

The parser has bugs, all reproduced on `257acb7`:

- `--file PATH` calls `set_test_case` instead of `set_file_path` (`src/cli.cpp:118`).
  `mps --file out.mps 31` fails with "Unrecognized option value 'out.mps' for -t/--test".
- The help text offers `-t` values `tf`, `p1` and `ll` (`src/main.hpp:43`). The parser accepts
  `none`, `all`, `qf`, `quick`, `factor`, `pm`, `p-min`, `ll` and `lucas-lehmer`
  (`src/cli.hpp:47-58`), so `-t tf` and `-t p1` are rejected.
- `-t=NAME` and `-f=PATH` can't be reached: the `starts_with(opt, "-t")` branch matches first and
  passes `=NAME` on as the value (`src/cli.cpp:168-178`, `188-198`).
- A non-numeric PVAL escapes as an uncaught `std::invalid_argument`. `mps abc` exits 30
  (`ERR_UNKNOWN`) with "Uncaught exception: stoi: no conversion" instead of 1 (`ERR_ARG`)
  (`src/main.cpp:38`).
- Nothing checks that the exponent is prime. `--skip-ptest`, `file_path` and `TEST_NONE` are parsed
  and never used.
- `mps -p N` omits 2.

The sieve behind `-p` is wrong at its edges (`src/math.cpp:19-40`):

- For n < 3, `prime_sieve(n)` writes `is_prime[2]` out of range: the vector holds n entries and the
  "special case" line sets index 2 unconditionally. `mps -p 1` and `mps -p 2` exit 0 after the
  out-of-range write, and `mps -p 0` exits 30 (`ERR_UNKNOWN`) with "Uncaught exception:
  std::bad_alloc".
- `prime_sieve(n)[1]` is true: the even pass clears index 0 and nothing clears index 1.

Fleet use needs a machine-readable record per result. Today there is only coloured log text on
stderr.

## Why it was deferred

Pre-existing on `main` since 2024. It is early on the roadmap because every test cycle after it
needs a result type to return and a verdict line to grade.

## Outcome / vision

`mps -t ll 31` prints one verdict line, "M31 is prime", and exits with a status that tells prime,
composite and error apart. A composite exponent is answered at once: if p = a·b then 2^a − 1 divides
M_p. With `--json`, each result is one JSON object on stdout with the exponent, the test, the
outcome, the factor or residue, and the program version. The options `--help` lists are exactly the
options that parse.

## Sketch of the acceptance criteria

- **R1** — Every `-t` name in `--help` SHALL parse and select that test, and no name absent from
  `--help` SHALL parse.
- **R2** — Every documented spelling of every option (`-f PATH`, `-fPATH`, `--file PATH`,
  `--file=PATH`, and so on) SHALL set that option.
- **R3** — IF PVAL is not a decimal integer within the supported range, THEN `mps` SHALL print an
  argument error and exit with `ERR_ARG`.
- **R4** — IF the exponent is composite and `--skip-ptest` is absent, THEN `mps` SHALL report M_p
  composite and name the factor 2^a − 1, without running the requested test.
- **R5** — WHEN a test completes, `mps` SHALL print one verdict line naming the exponent, the test
  and the outcome, and exit with a status that distinguishes prime or probable prime, composite,
  and error.
- **R6** — WHERE `--json` is given, `mps` SHALL write each result as one JSON object per line on
  stdout, and log text SHALL stay on stderr.
- **R7** — `mps -p N` SHALL list every prime below N, 2 included.
- **R8** — WHEN `prime_sieve(n)` is called with any n ≥ 0, it SHALL return n entries, true exactly
  at the primes below n, and SHALL write nothing outside them.

## Notes

- Shaping fixes the exit-status table. Burn-in (`issues/burn-in-mode.md`) will want "matched the
  expected answer" as its own status, so leave room for it.
- Shaping fixes the result type: a three-way outcome (prime or probable prime; composite with a
  factor; composite by residue) plus an error state, carried by every test.
- Coupled core: the parser, the help text and README usage change together (§ Conventions).
- Shaping decides the JSON field names. GIMPS's result lines use `exponent`, `worktype`, `status`,
  `res64`, `residue-type`, `fft-length`, `shift-count`, `errors` and `program`. Reusing the names
  that apply makes `mps` records familiar, but submitting to GIMPS is not a goal (`ROADMAP.md`).
- Found by: factory adoption review, 2026-10-11.
