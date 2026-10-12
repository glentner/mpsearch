# Research digest — verdicts-and-exit-statuses

Three briefs, prototyped in scratch on this Mac (Apple clang 21, Homebrew g++-16, GNU make 3.81):
[01 — the result type and the driver](01-result-and-driver.md),
[02 — the exponent check and § 1](02-exponent-check.md),
[03 — JSON records and the checks](03-json-and-checks.md). Trigger 2 fired: the test signatures in
`src/math.hpp` and `src/math.cpp` change, and both are on the high-blast-radius list. Trigger 3 fired
too (R13 makes `make check` coverage a deliverable); its adversarial review is recorded in PLAN § 4.
Decisions, with each cross-brief difference resolved once:

1. **One new module, `src/result.hpp`/`src/result.cpp`** (01, 03). `src/main.cpp` is not in the
   Makefile's `MODULES`, so `bin/check` cannot link it: the type, the line, the record, the status
   and the printer go in the module, `result` joins `MODULES`, and `tests/check_result.cpp` is picked
   up by the wildcard. The tests and the exponent check stay in `src/math.*`; the dispatch stays thin
   in `src/main.cpp`.
2. **The type** (01). `class Result` with a private constructor and six named constructors
   (`prime`, `probable_prime`, `composite_by_factor`, `composite_by_residue`, `no_verdict`, `error`),
   read through accessors. *Resolved against 01's `namespace result`:* the enumerators are prefixed
   instead (`OUTCOME_PRIME` …, `WORK_TF` …), as `cli::TestCase` prefixes `TEST_`; the global
   `enum Status` gains `COMPOSITE` 40, `PROBABLE_PRIME` 41 and `NO_VERDICT` 42 beside them without
   a clash, and nothing needs qualifying. Worktypes are `tf`, `p1`, `ll` and `ptest`; a constructed
   probable prime names `ll` in the checks (no PRP test or worktype exists yet; 03's open point).
3. **Outcome to status is a `switch`, never a map** (01): a missing map key reads as 0, which is
   prime. An error result is `ERR_RUN` (20, "runtime error", as `main` already maps
   `std::runtime_error`); a prime or probable prime with a composite is `ERR_UNKNOWN` (30). The GOAL
   names only prime with composite; a probable prime with a composite is the same contradiction and
   takes the same class. Otherwise the first decisive result's status, else 42.
4. **Stream the no-verdict lines, hold the verdicts.** *Resolved between 01's streaming `Reporter`
   and 03's decide-then-print `report`:* two free functions over the run's results.
   `report_result(run, result, json, out)` prints a no verdict at once (unless a verdict is already
   held), sends an error to stderr, and holds a decisive result; `report_run(run, json, out)` takes
   `status_of(run)` and prints the held verdicts only when it is neither 20 nor 30. Streaming alone
   would print the first of a prime and a composite before the second arrived (R9, § 1); deciding at
   the end alone would print `tf`'s line only after `ll` ran (R6, R7). In a real run the decisive
   result is last, so holding it costs nothing. Free functions rather than 01's class, so the gates
   can wrap them. An error is not decisive (`run_in_order` runs on past it), its line goes only to
   stderr, and it has no record (`json_record` of an error is empty).
5. **`test_all(p, completed)`** returns the deciding (or last) result and hands each completed one
   to the sink `completed` as it returns (01), so it returns `Result` (R1) and the driver still
   prints one line per completed test as it completes (R7). The stop rule lives once, in `run_in_order(p, tests, completed)`, which the checks
   drive with constructed tests: the stubs never decide, so nothing else can show the stop.
6. **The exponent check** (02). `smallest_prime_factor(p)`: 2, then odd `d <= p / d` (no overflow
   for any p), p when prime, 0 below 2. `composite_by_exponent(p, a)` is the only place that writes
   `2^a-1`, behind `1 < a and a < p and p % a == 0` in that order; anything else is an error result.
   `p % a == 0` is exactly 2^p ≡ 1 (mod 2^a − 1). `check_exponent(p)` returns `std::optional<Result>`,
   empty for a prime p: never prime, never no verdict. It is called after the `-p` return and before
   the test switch, unless `-s`. The GOAL's numbers all check out (31,607 is the largest prime ≤
   31,622; no smaller prime divides 999,002,449).
7. **Notation** (02): `2^a-1`, decimal a, no spaces, in the line and in `factors`. A decimal factor
   is a later format change that bumps `record_version`.
8. **The line** (plan's choice): `M<p> <worktype>: <outcome>[, <detail>] (mps <version>)`, the
   exponent from the parsed number (`0000000031` is `M31`).
9. **The record** (03): spaced like Python's `json.dumps` (`", "`, `": "`), keys in R10's order,
   `exponent`, `worktype`, `status`, then `factors` | `res64` | `reason`, then `program` and
   `record_version` (not `format`, which the cluster job's `result.json` already uses). `res64` is 16
   uppercase hex digits. A correct escaper (quote, backslash, every byte below 0x20).
10. **Logging** (01, 02): no `logging::debug` in `math.cpp` or `result.cpp` (their logger copies sit
    at `DEBUG`, `issues/logger-state-per-translation-unit.md`), and nothing on stderr from the
    exponent check. `report` reports a failure with `logging::critical`.
11. **Colour** (03): stdout records are never coloured; the result module does not include
    `ansi.hpp`.
12. **The checks** (03): a strict test-only JSON validator in `tests/check_result.cpp`, with its own
    rejection check, plus byte-exact lines in `tests/cli.sh`. Statuses no binary can reach (0, 41,
    20, 30) are shown on constructed results through the printer and `status_of`. `tests/cli.sh` lines
    177, 182, 187, 216 and the 233–241 loop change.
13. **R12** (01): the list gains `src/result.hpp`, `src/result.cpp` and `src/main.cpp` (the dispatch
    that decides which tests run and hands their results to the printer), with the two reworded
    sentences and a § Map line, in the commit that adds the module.
