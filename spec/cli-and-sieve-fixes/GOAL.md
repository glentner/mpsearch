# GOAL — Options that parse as documented, and a sieve right at its edges

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** cli-and-sieve-fixes
- **kind:** fix
- **appetite:** small

## Problem

The command line doesn't do what `--help` says. All of these are pre-existing on `main` (`d179aa4`);
the faulty lines date from 2024, before the factory.

- `--help` offers the `-t` names `tf`, `p1` and `ll` (`src/main.hpp:43`). The parser's table holds
  `none`, `all`, `qf`, `quick`, `factor`, `pm`, `p-min`, `ll` and `lucas-lehmer`
  (`src/cli.hpp:47-58`), so `-t tf` and `-t p1` are rejected, and six names nobody documents parse.
  `none` selects nothing that runs differently from `all`.
- `mps --file out.mps 31` fails with "Unrecognized option value 'out.mps' for -t/--test"
  (`src/cli.cpp:113-119`).
- `-t=NAME` and `-f=PATH` never reach their own branches: the bare `-t` and `-f` prefix branches
  match first and take `=NAME` or `=PATH` as the value (`src/cli.cpp:168-198`). `-f=out.mps` sets the
  path `=out.mps`, and parsing creates a file by that name.
- Short flags can't be grouped, although the usage line advertises `[-hv]`: `-dD` is "Unrecognized
  option". `-hv` works only because `-h` matches by prefix (`src/cli.cpp:154-158`).
- PVAL is checked only for length: at most nine characters (`src/cli.cpp:77-79`, from `257acb7`,
  "Add upper bound on PVAL"), so the largest PVAL accepted is 999,999,999. `main` converts it with
  `std::stoi` (`src/main.cpp:34`, `38`). `mps abc` exits 30 (`ERR_UNKNOWN`) with "Uncaught exception:
  stoi: no conversion" instead of 1 (`ERR_ARG`); `mps 31abc` silently tests 31; `mps 0` and `mps 1`
  run the tests on exponents that are not prime candidates.
- `mps -p N` lists the primes below N but leaves out 2 (`src/math.cpp:43-50`). The help says "Print
  primes up to PVAL" (`src/main.hpp:41`), which reads as inclusive; the code, `AGENTS.md` ("primes
  below 100") and the smoke test all mean below.
- The sieve behind `-p` is wrong at its edges (`src/math.cpp:19-40`). For n < 3, `prime_sieve(n)`
  sets index 2 of a vector with fewer than three entries: `mps -p 1` and `mps -p 2` write out of
  range, and `mps -p 0` exits 30 with "Uncaught exception: std::bad_alloc". Its entry for 1 is true.

The checks work around these: `tests/check_math.cpp`, `tests/check_cli.cpp` and `tests/cli.sh` each
say they pin only what is right today, the sieve check starts at n = 3 and index 2, and the smoke
test accepts `-p 30` with or without its 2. Every later cycle builds on this command line and grades
itself with these checks, so the bugs should go before the result type and the tests arrive.

## Outcome / vision

The options `--help` lists are exactly the options that parse, in every spelling `--help` states.
A bad PVAL is an argument error with exit status 1, never an uncaught exception. `mps -p N` lists
every prime below N, 2 included, for any N from 0 up, and `make check` pins all of it with no
caveats left in `tests/`.

## Acceptance criteria (the contract)

*Valued options* are `-t/--test` and `-f/--file`. *Boolean flags* are `-h`, `-v`, `-p`, `-s`, `-d`
and `-D`. *An argument error* is a message on stderr and exit status 1 (`ERR_ARG`), with no
"Uncaught exception" text. "The bound" is 999,999,999, today's nine-digit limit.

- **R1** — `mps --help` SHALL list for `-t` the names `tf`, `p1`, `ll` and `all`, name `all` as the
  default, and list the long aliases `trial-factor`, `p-minus-1` and `lucas-lehmer` beside the names
  they stand for. It SHALL state once that a valued option is given as `-x VALUE`, `-xVALUE`,
  `-x=VALUE`, `--long VALUE` or `--long=VALUE`, and SHALL describe `-p` as printing the primes below
  PVAL.
- **R2** — WHEN `mps -t NAME 31` is run with a name or alias R1 lists, `mps` SHALL run that test
  alone (all three for `all`, and when `-t` is absent) and exit 0. Each test's start line on stderr
  ("Running quick factor test", "Running P-1 test", "Running Lucas-Lehmer test") shows which ran.
- **R3** — IF `-t` is given any value R1 does not list, including `qf`, `quick`, `factor`, `pm`,
  `p-min`, `none` and the empty string, THEN `mps` SHALL raise an argument error naming the value.
- **R4** — Each valued option SHALL accept all five spellings R1 states, and each SHALL set the
  option to exactly VALUE. `mps --file PATH -t ll 31` SHALL run the Lucas–Lehmer test alone and
  exit 0, and `make check` SHALL show every spelling of `-t` and `-f` parsing to the same value.
- **R5** — WHEN boolean flags are grouped in one word (`-dD`, `-pd`, `-sdD`), `mps` SHALL behave as
  if they were given separately, in the same order. A group holding a character that is not a
  boolean flag SHALL raise an argument error, unless an earlier flag in the group already ended the
  run (`-h`, `-v`).
- **R6** — IF, without `-p`, PVAL is anything but one or more ASCII decimal digits whose value is
  from 2 to the bound (`abc`, `31abc`, `3.5`, ` 31`, `0`, `1`, `1000000000`), THEN `mps` SHALL raise
  an argument error that names PVAL and run no test. Leading zeros do not count against the bound.
- **R7** — IF, with `-p`, PVAL is anything but one or more ASCII decimal digits whose value is from 0
  to the bound, THEN `mps` SHALL raise an argument error that names PVAL and print nothing on stdout.
- **R8** — WHEN `mps -p N` is run with a valid N, it SHALL print every prime below N, 2 included,
  one per line in ascending order, with nothing else on stdout, and exit 0. For N ≤ 2 (0, 1 and 2
  included) stdout SHALL be empty.
- **R9** — WHEN `prime_sieve(n)` is called with any n ≥ 0, it SHALL return n entries, true exactly
  at the primes below n, and SHALL write nothing outside them. `make check` SHALL show this for every
  n from 0 through 3 and for every index from 0 up, besides the ranges it checks today.
- **R10** — `make check` SHALL pass and SHALL carry a check for each of R1–R9. No file under `tests/`
  SHALL still describe these defects as known, or accept a `-p` listing with or without its 2.
- **R11** — The cycle's last phase SHALL add `issues/verdicts-and-exit-statuses.md`, `status:
  unshaped`, and its `ROADMAP.md` entry directly after this cycle's entry, carrying the follow-up
  scope listed in Non-goals.

## Non-goals (no-gos)

- **The follow-up cycle,** seeded by this cycle's last phase as `issues/verdicts-and-exit-statuses.md`
  with its `ROADMAP.md` entry right after this one (R11). It carries the seed's R4–R6 (the
  composite-exponent answer, one verdict line with an exit status per outcome, and `--json`); the
  seed's Notes on the exit-status table, with room for burn-in's "matched the expected answer"; the
  result type every test returns; the JSON field names; and `--skip-ptest`, which is parsed and
  never used. A no-test mode, if one is wanted, comes back there.
- A `README.md` usage section: the README documents no options today, so the coupled core's README
  leg has nothing to change.
- The help text for `-s/--skip-ptest`, and the default checkpoint path: unchanged.
- Grouping a valued option with boolean flags (`-dt ll`, `-dtll`): neither required nor forbidden.
- Names for a PRP test: they come with that test's cycle.
- The logger-state seed (`issues/logger-state-per-translation-unit.md`).
- Any change to the tests' arithmetic, or to what the stubs return.
- Checkpoints: `-f` sets the path and nothing reads or writes the file yet.

## Clarifications

The author delegated every gate for this effort to the foreman; these are the foreman's decisions.

- **Q:** The seed holds mechanical fixes and a load-bearing design. One cycle or two? — **A:** Two.
  This cycle is the pilot and takes the seed's R1–R3 and R7–R8; the result type, verdicts, exit
  statuses, the composite-exponent answer and `--json` are the follow-up (resolved 2026-10-11).
- **Q:** Which `-t` names? — **A:** `--help` and the parser agree on one list. The canonical names
  are `tf`, `p1`, `ll` and `all`, with `all` the default, and each test has one long alias listed in
  `--help`: `trial-factor`, `p-minus-1`, `lucas-lehmer`. `qf`, `quick`, `factor`, `pm`, `p-min` and
  `none` stop parsing; `none` has no behaviour today (`src/main.cpp:40-56` sends it to `test_all`).
  A future PRP test adds its own names in its own cycle (resolved 2026-10-11).
- **Q:** Which option spellings? — **A:** Every valued option accepts `-x VALUE`, `-xVALUE`,
  `-x=VALUE`, `--long VALUE` and `--long=VALUE`, and `--help` states that grammar once. Boolean
  short flags may be grouped, as the usage line's `[-hv]` already promises; shaping reads that as a
  requirement on `mps` (R5) and defines a group as its flags given separately, in order. The README
  documents no usage, so the parser and the help text change together and the README stays as it
  is (resolved 2026-10-11).
- **Q:** What is a valid PVAL? — **A:** A decimal integer from 2 up to the existing bound, and
  anything else is an argument error with `ERR_ARG`, never an uncaught exception. The bound from
  `257acb7` is a nine-character limit (`src/cli.cpp:77`), so its value is 999,999,999. Shaping makes
  it a bound on the value, with leading zeros allowed, and a sign or whitespace rejected (resolved
  2026-10-11).
- **Q:** What does `-p N` list? — **A:** The primes below N, for any decimal integer N from 0 up to
  the same bound. The code already means below (`display_primes` stops before n), so the help's "up
  to PVAL" changes to "below PVAL" (resolved 2026-10-11).
- **Q:** What must the sieve do? — **A:** `prime_sieve(n)` returns n entries for every n ≥ 0, true
  exactly at the primes below n, and writes nothing outside them; `mps -p N` lists them all, 2
  included (resolved 2026-10-11).
- **Q:** What is out of scope? — **A:** The logger-state seed, any change to the tests' arithmetic,
  checkpoints, and everything in the follow-up (resolved 2026-10-11).

## Related materials

- Seed: [`issues/results-and-cli-fixes.md`](../../issues/results-and-cli-fixes.md); `ROADMAP.md`,
  "Say what was found: verdicts, exit statuses, and the options that don't parse"
- Command-line surface (coupled core): `src/main.hpp` (usage, help, `enum Status`), `src/cli.hpp`,
  `src/cli.cpp`, the dispatch in `src/main.cpp`
- Sieve: `src/math.cpp` (`prime_sieve`, `display_primes`), a high-blast-radius path (`AGENTS.md`
  § Invariants)
- Checks: `tests/check_cli.cpp`, `tests/check_math.cpp`, `tests/cli.sh`
- Defect origin: pre-factory (`c78cef3`, `2a7075a`, `38a77e6`, `257acb7`, 2024)
- Burn-in's future status: `issues/burn-in-mode.md`
