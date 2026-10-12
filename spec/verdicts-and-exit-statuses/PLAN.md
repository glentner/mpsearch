# PLAN — Say what was found: a result type, one verdict line, exit statuses and `--json`

> **Status:** Draft for review · **Last updated:** 2026-10-12
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). Backing detail is in
> [`research/`](research/). Every design element traces to a GOAL R-ID.

## 1. Summary

One new module, `src/result.*`, holds the result type and its three views: the verdict line, the
JSON record and the exit status, with a printer that streams each no-verdict line as its test
completes and holds every verdict until the run is known to hold no error and no contradiction.
The tests return that type and say honestly that they decide nothing; `test_all` runs them in order
and stops at the first decisive result. Then the exponent check answers a composite exponent with
the factor 2^a − 1, re-checked exactly, before any test runs; then `-j/--json` switches the lines to
records, and the help lists the statuses. The last phase audits the checks, keeps the deferral
ledger, and proves the pushed head on the three cluster targets. Frozen gates drive `bin/mps`, probe
the modules directly, and inject defects around the builder's code to show that `make check` pins
each requirement.

## 2. Design

### Files

| Path | Change | R-IDs |
|---|---|---|
| `src/result.hpp`, `src/result.cpp` | new: `Outcome`, `Worktype`, `Result`, `is_decisive`, `verdict_line`, `json_record`, `status_of`, `report_result`, `report_run` | R1, R6, R8–R10 |
| `src/math.hpp`, `src/math.cpp` | the tests return `Result`; `run_in_order`, `test_all`; the exponent check | R1–R5, R7 |
| `src/main.hpp` | `Status` 40, 41, 42; the usage line, `-s`, `-j` and the status table in the help | R8, R11 |
| `src/main.cpp` | the dispatch: exponent check unless `-s`, the tests, the printer | R3–R10 |
| `src/cli.hpp`, `src/cli.cpp` | `-j/--json` into `json_mode` | R10 |
| `src/ansi.cpp` | `colorize_usage` bolds a header with a space (`Exit status:`) | R11 |
| `Makefile` | `result` joins `MODULES` | R1 |
| `AGENTS.md` | § Map line; the two sentences and the high-blast-radius list (R12) | R12 |
| `tests/check_result.cpp` (new), `tests/check_math.cpp`, `tests/check_cli.cpp`, `tests/cli.sh` | the checks | R13 |

`README.md` documents no options and does not change (GOAL non-goal). `bin/mps` gains no dependency.

### The result type (`src/result.hpp`)

- `enum Outcome { OUTCOME_PRIME, OUTCOME_PROBABLE_PRIME, OUTCOME_COMPOSITE, OUTCOME_NO_VERDICT,
  OUTCOME_ERROR };` and `enum Worktype { WORK_TF, WORK_P1, WORK_LL, WORK_PTEST };` with
  `name_by_worktype` giving `tf`, `p1`, `ll`, `ptest`. Prefixed enumerators, as `cli::TestCase`
  prefixes `TEST_`, so `enum Status`'s new `COMPOSITE`, `PROBABLE_PRIME` and `NO_VERDICT` sit beside
  them in the global scope.
- `class Result`: a private constructor and six named constructors, so exactly one outcome is set
  and each carries what it must:

  ```cpp
  static Result prime(Worktype worktype, long unsigned p);
  static Result probable_prime(Worktype worktype, long unsigned p);
  static Result composite_by_factor(Worktype worktype, long unsigned p, const std::string& factor);
  static Result composite_by_residue(Worktype worktype, long unsigned p, std::uint64_t res64);
  static Result no_verdict(Worktype worktype, long unsigned p, const std::string& reason);
  static Result error(Worktype worktype, long unsigned p, const std::string& reason);
  ```

  Read through `outcome()`, `worktype()`, `exponent()`, `factors()` (exact strings),
  `res64()` (`std::optional<std::uint64_t>`) and `reason()`. Members are private, never public
  `const` (that deletes assignment). Helpers in the header are `inline`: a `static` one fails the
  checks' `-Werror=unused-function`.
- `static const int RECORD_VERSION = 1;` in `result.hpp`, the record's own format version.
- Free functions, the surface the checks and the gates use:

  ```cpp
  bool is_decisive(const Result& result);
  std::string verdict_line(const Result& result);           // one line, no newline
  std::string json_record(const Result& result);            // one object, no newline
  int status_of(const std::vector<Result>& results);        // the run's exit status
  void report_result(std::vector<Result>& run, const Result& result, bool json, std::ostream& out);
  int report_run(const std::vector<Result>& run, bool json, std::ostream& out);
  ```

  Each of these, and each function in § 2's math signatures, is declared in its header and defined
  out of line in its `.cpp`, at file scope, with its name on the definition's first line; and the
  code calls them rather than repeating them: the printer formats through `verdict_line` and
  `json_record` and returns `status_of(run)`; `run_in_order` asks `is_decisive`; `test_all` calls
  `run_in_order`; `check_exponent` calls `smallest_prime_factor` and `composite_by_exponent`. The
  gates wrap these definitions (§ 5).

### The verdict line (R6)

`M<p> <worktype>: <outcome>[, <detail>] (mps <APP_VERSION>)`, with the exponent written from the
parsed number (`mps 0000000031` says `M31`). The outcome is `prime`, `probable prime`, `composite`,
`no verdict`. The detail is `factor <f>` for a composite by factor,
`res64 <16 uppercase hex digits>` for a composite by residue, and the reason for a no verdict:

```
M31 tf: no verdict, trial factoring is not implemented yet (mps 0.1.0)
M31 p1: no verdict, P-1 is not implemented yet (mps 0.1.0)
M31 ll: no verdict, Lucas-Lehmer is not implemented yet (mps 0.1.0)
M33 ptest: composite, factor 2^3-1 (mps 0.1.0)
M31 ll: prime (mps 0.1.0)
M31 ll: probable prime (mps 0.1.0)
M999999937 ll: composite, res64 0123456789ABCDEF (mps 0.1.0)
```

(The last three are constructed; the residue is synthetic and is never paired with a real
composite exponent, § Invariants 3.) An error's line, `M31 ll: error, <reason> (mps 0.1.0)`, is the
text `report_result` sends to stderr; it never reaches stdout. Never coloured: `result.cpp` does not include `ansi.hpp`.

### The JSON record (R10)

One object per line, keys in R10's order, spaced as Python's `json.dumps` writes (`", "` and
`": "`), so R10's `program` text appears byte for byte. `factors` only for a composite by factor,
`res64` only for a composite by residue, `reason` only for a no verdict. An error has no record
(GOAL non-goal): `json_record` of an error is the empty string, and the printer never asks for it.
`record_version` is the
record's format version, the integer 1 (named apart from the cluster job's `result.json` `format`):

```
{"exponent": 31, "worktype": "tf", "status": "no-verdict", "reason": "trial factoring is not implemented yet", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
{"exponent": 33, "worktype": "ptest", "status": "composite", "factors": ["2^3-1"], "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
{"exponent": 31, "worktype": "ll", "status": "prime", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
{"exponent": 31, "worktype": "ll", "status": "probable-prime", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
{"exponent": 999999937, "worktype": "ll", "status": "composite", "res64": "0123456789ABCDEF", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
```

Strings go through one escaper (`"` and `\` backslashed, every byte below 0x20 as `\u00XX`), so a
hostile reason still gives one valid line. `json_record`'s doc block names the format, says
`record_version` is bumped with any change to the record (§ Conventions), and notes that `factors`
holds `2^a-1`, not decimal: a decimal form waits for the big integer and would be a format change.

### Statuses and the printer (R8, R9)

- `enum Status` (`src/main.hpp`) gains `COMPOSITE = 40`, `PROBABLE_PRIME = 41`, `NO_VERDICT = 42`;
  the existing values do not move, and nothing produces 50–59.
- `status_of(results)`: a prime or probable prime together with a composite → `ERR_UNKNOWN` (30);
  else any error → `ERR_RUN` (20); else the first decisive result's status through a `switch`
  (prime 0, probable prime 41, composite 40), never a map, whose missing key would read as 0, prime;
  else 42.
- The printer has two calls, and the run's results are its only state:
  - `report_result(run, result, json, out)`, as each result arrives: appends it to `run`; an error
    is reported on stderr with `logging::critical` (its line text, which names the exponent, the
    test and the reason) and prints nothing on stdout; a no verdict prints its line or record at
    once, unless a decisive result is already held; a decisive result prints nothing yet.
  - `report_run(run, json, out)`, when the run ends: computes `status_of(run)`; on 20 or 30 it
    prints nothing more (a contradiction is reported on stderr) and returns the status; otherwise
    it prints, in order, every result from the first decisive one on, and returns the status.
- So a no-verdict line appears when its test completes (R6, R7), a verdict appears only once the run
  is known to hold no error and no contradiction (§ 1: a run whose checks fail never prints a
  verdict; R9: neither of a prime and a composite prints), and the lines keep the order the tests
  ran. In a real run the decisive result is the last (`-t all` stops there, and a single test gives
  one result), so holding it costs nothing.

### The tests and `-t all` (R1, R2, R7)

```cpp
using TestFunction = Result (*)(const long unsigned p);
using ResultSink = std::function<void(const Result&)>;
Result test_quick_factor(const long unsigned p);
Result test_p_minus_one(const long unsigned p);
Result test_lucas_lehmer(const long unsigned p);
Result run_in_order(const long unsigned p, const std::vector<TestFunction>& tests, const ResultSink& completed);
Result test_all(const long unsigned p, const ResultSink& completed);
```

- Each stub keeps its start line (`logging::info("Running quick factor test")` and so on, R2) and
  returns `Result::no_verdict` with the reasons above: `trial factoring is not implemented yet`,
  `P-1 is not implemented yet`, `Lucas-Lehmer is not implemented yet`.
- `run_in_order` calls each test in order, hands its result to `completed` as soon as it returns,
  and stops after the first decisive one (an error is not decisive: the next test runs, and the
  run still exits 20); it returns the last result handed over. `main.cpp` passes
  a sink that calls `report_result`; the checks pass one that collects. `test_all` is
  `run_in_order(p, {test_quick_factor, test_p_minus_one, test_lucas_lehmer}, completed)`. The stop
  rule lives only there, and the checks drive it with constructed tests, since the stubs never
  decide.

### The exponent check (R3–R5; [brief 02](research/02-exponent-check.md))

```cpp
long unsigned smallest_prime_factor(const long unsigned p);   // p itself when prime; 0 below 2
Result composite_by_exponent(const long unsigned p, const long unsigned a);
std::optional<Result> check_exponent(const long unsigned p);
```

- `smallest_prime_factor`: 2, then odd d while `d <= p / d` (no overflow for any p); the first
  divisor found is prime. Its comment cites trial division (Crandall and Pomerance, *Prime Numbers:
  A Computational Perspective*, 2nd ed., 2005, ch. 3).
- `composite_by_exponent(p, a)` is the only code that writes `2^a-1`. It returns
  `Result::composite_by_factor(WORK_PTEST, p, "2^<a>-1")` only when `1 < a and a < p and p % a == 0`
  (in that order, so a = 0 never divides), and otherwise `Result::error(WORK_PTEST, p, …)`. Its
  comment carries the proof: with r = p mod a, 2^p ≡ 2^r (mod 2^a − 1) and 2^r < 2^a − 1 for r < a,
  so 2^p ≡ 1 exactly when a | p (the § 1 re-check), and 1 < a < p gives 1 < 2^a − 1 < 2^p − 1.
- `check_exponent(p)`: `a = smallest_prime_factor(p)`; empty when `a == p`; else
  `composite_by_exponent(p, a)`. A prime exponent yields no result, so it prints nothing (R4), and
  nothing in `math.cpp` logs beyond the stubs' start lines.
- `src/main.cpp`, after the `-p` return: unless `opt.skip_ptest`, `check_exponent(pval)`; if it
  yields a result, that is the run's only result and no test runs (R3); otherwise the `-t` switch
  runs one test or `test_all`, every result going through `report_result`. Then
  `return report_run(run, opt.json_mode, std::cout);`.
  `--help`, `--version` and `-p` are unchanged and exit 0.

### `-j/--json` and the help (R10, R11)

`flag_by_option` gains `{"--json", 'j'}`, `cli::Interface` gains `bool json_mode = false;`, and
`check_flag` sets it with a `Duplicate use of -j/--json` warning, so it groups like `-d` (`-jd`,
`-sj`), and `--json=…` is an error as for every flag. In the same commit the usage line and the help
read exactly (under `NO_COLOR`):

```
Usage:
  mps [-hv] [-p] PVAL [-s] [-t NAME] [-j] [-f PATH] [-d] [-D]
  Test if number is Mersenne prime.

Arguments:
  PVAL                    Mersenne prime exponent.

Options:
  -p, --primes            Print the primes below PVAL.
  -s, --skip-ptest        Skip the exponent check (that PVAL is prime).
  -t, --test        NAME  One of 'tf' ('trial-factor'), 'p1' ('p-minus-1'),
                          'll' ('lucas-lehmer') or 'all' (default: 'all').
  -j, --json              Print each result as one JSON object per line.
  -f, --file        PATH  File path for checkpointing.
  -d, --debug             Show debug messages.
  -D, --detailed          Show detailed logging messages.
  -v, --version           Show version and exit.
  -h, --help              Show this message and exit.

  Give a valued option as -x VALUE, -xVALUE, -x=VALUE, --long VALUE or
  --long=VALUE. Short flags may be grouped (-dD).

Exit status:
  0       Prime, or no verdict by design (--help, --version, -p).
  1       Wrong command-line invocation.
  2       Configuration error.
  10      File I/O or formatting error.
  20      Runtime error.
  30      Unexpected internal failure.
  40      Composite.
  41      Probable prime.
  42      No verdict.
  50-59   Reserved for burn-in.
```

`APP_USAGE` gains `[-j]` in the same place. Every help and usage change lands in P3, in one commit
with the parser (R11), the `-s` line included, although `-s` starts working in P2. `colorize_usage`'s header pattern becomes
`^[A-Za-z][A-Za-z ]*:`, so `Exit status:` is bold like the other headers (only headers start a
line).

### AGENTS.md (R12), in the commit that adds `src/result.*`

- High-blast-radius paragraph: "A cycle that adds an arithmetic engine, a test driver that prints a
  verdict, a checkpoint or result format of `mps`, or a table of known answers adds its paths here
  in the same commit."
- § Conventions: "A change to a checkpoint or result format of `mps` bumps that format's version in
  the same commit."
- The list gains two lines, exactly `` - `src/result.hpp`, `src/result.cpp` `` (the result format
  and the printer) and `` - `src/main.cpp` `` (the dispatch that decides which tests run and hands
  their results to the printer).
- § Map gains `src/result.cpp`, `src/result.hpp`: the result type, the verdict line, the JSON
  record and the exit status.

### The checks (R13)

What the gates require of `make check`, beyond passing (§ 6 lists the mutants):

- `tests/check_result.cpp`: each named constructor's line and record, exactly (an error's line, and
  no record for it); the status of a result of every outcome through `status_of` **and** through
  `report_result` and `report_run`, with `json` false and true, against the literal numbers (0, 41,
  40, 42, 20, 30), never the enumerators, which an edit could move; R9's two failures on
  constructed results in both modes (a prime with a composite, either order, and a probable prime
  with a composite: 30, stdout empty, the contradiction reported on stderr; an error: 20, no line or
  record for it, its reason on stderr), and an error after a composite printing no verdict; the stop
  and the order through `run_in_order` with constructed tests, stopping on a prime, a probable prime
  and a composite alike, and each result handed over before the next test runs; `test_all(31, …)`'s
  three results; a strict test-only JSON validator (objects, arrays, strings and their escapes,
  integers, literals; rejecting duplicate keys, trailing commas, leading zeros, fractions, raw
  control characters and trailing text) applied to every record, with its own check that it rejects
  each bad case; a hostile reason (`"`, `\`, `\n`, `\x01`) still one valid line.
- `tests/check_math.cpp`: `smallest_prime_factor` against the sieve for every n below 100,000 and a
  known-answer table (4, 9, 25, 33, 49, 961, 999,999,999, 999,999,998, 998,623,201 = 31,601²,
  999,002,449 = 31,607², and the primes 2, 3, 31, 999,999,929, 999,999,937), each row with the
  command that recomputes it; `check_exponent` empty for those primes and `2^a-1` for the
  composites; `composite_by_exponent`'s refusals ((33, 0), (33, 1), (33, 5), (33, 33), (4, 4)) and
  its acceptance of a non-smallest divisor (33, 11); the stubs never decisive.
- `tests/check_cli.cpp`: `-j` and `--json` set `json_mode`, groups with it (`-jd`, `-sj`) alike their
  separated flags, `--json=x` rejected.
- `tests/cli.sh`, through `bin/mps`, since only it can show "under every `-t` name" and the binary's
  statuses: every non-`-p` PVAL run's status and stdout exactly (42 for primes and for `-s`, 40 for
  composites); `mps 31`'s three lines in order; R3's four exponents under no `-t` and each of the
  seven `-t` names, with no start line on stderr; R4's four primes; `-s` on composites; `-j`
  byte-exact records and statuses equal to the plain run's, `-j` in groups; `--help` holding
  `-j, --json`, the `-s` wording and every status row; `mps` alone printing the usage exactly; no
  ESC on stdout under `FORCE_COLOR=1`; and R12, by reading `AGENTS.md` as it reads `src/main.hpp`:
  the two list lines whole and both reworded sentences. One condition per `expect`; `planned`
  exact.
- No check under `tests/` expects `mps PVAL` to exit 0 without `-p`; the gates sweep the names and
  the run-to-status pairs.

### Requirement → design map

| R-ID | Design element(s) that satisfy it |
|------|-----------------------------------|
| R1   | `Result` with six named constructors; the stubs, `test_all` and `run_in_order` return it; `verdict_line`, `json_record`, `status_of` read only results |
| R2   | The stubs return `Result::no_verdict` with "… is not implemented yet"; start lines unchanged |
| R3   | `check_exponent` before the `-t` switch; `smallest_prime_factor`; `composite_by_exponent`'s guard; `2^a-1` |
| R4   | `check_exponent` empty for a prime p; nothing logged |
| R5   | `opt.skip_ptest` skips `check_exponent` in `main.cpp` |
| R6   | `verdict_line`; `report_result`/`report_run` print one per result on `std::cout`; logs stay on `std::cerr` |
| R7   | `run_in_order`'s stop; `test_all`'s order; one line per completed result |
| R8   | `status_of`'s switch; `Status` 40–42; `--help`/`--version`/`-p` paths unchanged |
| R9   | `status_of`'s 30 and 20; verdicts held until `report_run` knows the run is sound; an error never printed |
| R10  | `json_record`; `-j/--json` → `json_mode` → `report_result`/`report_run`; `record_version`; the escaper |
| R11  | The help and usage above, all in P3's commit with the parser |
| R12  | The `AGENTS.md` changes above, in P1's commit with `src/result.*`; `tests/cli.sh` reads them |
| R13  | The checks above; the gates' mutants and sweeps |

## 3. Invariant gate (AGENTS.md constitution check)

Checked against `AGENTS.md` § Invariants before research, and again after this design was drafted
and after the adversarial review was folded in.

- **High-blast-radius paths.** The change touches `src/math.hpp` and `src/math.cpp` and adds
  `src/result.*`; R12 lists those and `src/main.cpp` in P1's commit. A CONFIRMED review finding in any
  of them needs Geoffrey's decision before landing; the design keeps § 1's guarantees checkable at a
  glance (below) to make that review short.
- **§ 1 Verdicts are earned.** No code path returns prime or probable prime: the stubs return no
  verdict and only the checks construct the others. The one composite is the exponent check's, built
  only by `composite_by_exponent` behind the exact re-check `p % a == 0` with 1 < a < p. An error
  result and a contradiction print no verdict (verdicts are held until `report_run` has the status)
  and exit non-zero. Every
  printed result names the test, the exponent and the program version; the transform length, shift,
  residue and error-check counts "where they apply" apply to no stub, and `res64` is carried for a
  composite by residue.
- **§ 2 Arithmetic is checked.** No engine; trial division of the exponent is exact integer
  arithmetic on `long unsigned`. Compile flags unchanged.
- **§ 3 Known answers.** No table of expected values enters the tree; the checks' rows each carry the
  command that recomputes them, and constructed residues are synthetic and paired with a prime.
- **§ 4 Clean room.** Trial division and the divisibility of 2^a − 1 come from the mathematics,
  cited to Crandall and Pomerance. GIMPS's field names come from the GOAL, not from any program.
- **§ 5 Dependency-free core.** The JSON is written by hand; the validator lives in `tests/`.
- **§ Conventions.** The parser, usage and help change together (P3); README has no usage. The record
  carries its version from its first commit. No R#/P# ids in `src/` or `tests/`. Code style: tabs,
  `not`/`and`/`or`, `<value>_by_<key>` tables, doc blocks at the top of a body, SPDX headers on the
  new files.

### Deviation justifications

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| 1. `colorize_usage`'s header pattern widens to admit a space | R11's status table needs a header, and "Exit status:" would be the only plain one in colour | A one-word header ("Statuses:") reads oddly; leaving it unbolded looks broken in a terminal |

## 4. Rabbit holes (resolved)

- **Where the logic lives** → a module, because `bin/check` cannot link `main.cpp`
  ([01](research/01-result-and-driver.md)).
- **`test_all` returning one result yet printing three lines** → it hands each completed result to
  the caller's sink as it returns ([00](research/00-digest.md), item 5).
- **Showing the stop with stubs that never decide** → `run_in_order` with constructed tests.
- **The logger's per-unit state** → nothing new logs below `critical` outside `main.cpp`
  ([01](research/01-result-and-driver.md), [02](research/02-exponent-check.md)).
- **"Every line parses as JSON" with a POSIX shell only** → a test-only C++ validator that is itself
  checked, plus byte-exact lines in `tests/cli.sh` ([03](research/03-json-and-checks.md)).
- **The re-check § 1 asks for** → `p % a == 0` is exactly 2^p ≡ 1 (mod 2^a − 1)
  ([02](research/02-exponent-check.md)).
- **Adversarial design review** (trigger 3): a fresh reviewer at xhigh read the GOAL and this
  design (not the research) and prototyped against it: 3 HIGH, 4 MEDIUM, 8 LOW. All are folded in:
  1. *Calls inside one file hid the re-check and the stop* (H1): renaming a function with a
     prepended `#define` renames its callers in that file too, so a `check_exponent` that built its
     composite without `composite_by_exponent`, or a `test_all` without a stop, passed. The gates
     now rename only each definition, so every call reaches the wrappers, and § 2 pins the call
     graph. Two drives prove main's path: with the exponent search claiming 5 for 33, `mps 33` exits
     20 and prints nothing; with trial factoring finding a factor, `mps 31` prints one line, starts
     no other test and exits 40. A variant whose `check_exponent` skips the re-check is red at P2.
  2. *R12 had no check in `make check`* (H2), though R13 asks one for each of R1–R12:
     `tests/cli.sh` reads `AGENTS.md`, and four edits to it must turn `make check` red.
  3. *"Every" clauses pinned only by mutants that break everything at once* (H3, cycle 3's F2 class):
     the version, `record_version` and `program` mutants run once per outcome; the
     exponent check is skipped once per `-t` spelling (absent included) and once per R3 exponent; a
     prime exponent is answered composite once per R4 prime; every mutant whose clause is about
     `bin/mps` must fail `tests/cli.sh` itself. A suite looping R3 over five of the eight spellings
     is red at P2.
  4. *R9's stderr and the json side of the printer* (M1): the probe and the checks run every printer
     case in both modes; mutants silence the contradiction and the error on stderr, print a
     contradiction's records, and change a status only under `-j`.
  5. *Statuses against enumerators* (M2): the checks compare literal numbers; three edits move
     `PROBABLE_PRIME`, `ERR_UNKNOWN` and `NO_VERDICT`.
  6. *Help edits caught for the wrong reason, and the usage unchecked* (M3): each edit names a string
     only the help holds and must fail `tests/cli.sh`; `[-j]` is dropped from `APP_USAGE` alone; the
     checks run `mps` with no arguments.
  7. *Inline helpers can't be wrapped* (M4): the wrapped functions are out of line (§ 2), and
     `is_decisive` is wrapped too, with mutants for prime, probable prime and no verdict.
  8. LOW: the error's line and its missing record are specified (L1); the hostile record is compared
     by value, since escapes may be spelled two ways (L2); `run_in_order` continues after an error
     (L3); the two readings of R8/R9 that no run of `mps` can reach move from the deviation table to
     § 5 for the foreman (L4); every help change moves to P3, and the gate checks the parser, help and
     usage share a commit (L5); a gate drive rebuilds with another `APP_VERSION` (L6); the start
     lines and `-j` inside a group get mutants (L7); the no-exit-0 sweep's patterns are fixed and
     allow `-p`, help, version and `mps` alone (L8).

## 5. Risks & open questions

- **Two readings for the foreman** (the review's L4). They concern runs `mps` cannot produce, so no
  observable behaviour of `mps` depends on them, but they read R-ID text, so the foreman may want
  them as GOAL clarifications rather than plan choices:
  1. A probable prime together with a composite exits 30, as R9 says for a prime with a composite.
     No PRP test exists, so only constructed results reach it.
  2. A run holding an error prints no verdict at all, even one from an earlier test (§ 1: a run
     whose error checks fail never prints a verdict), where R6 alone would print that test's line.
     `-t all` stops at the first verdict and a single test gives one result, so no run of `mps`
     holds both.
- **The gates pin the design's surface**: the line and record text above; the help and usage; the
  names and signatures in § 2 (`Result`'s named constructors and `outcome()`, `is_decisive`,
  `verdict_line`, `json_record`, `status_of`, `report_result`, `report_run`, `run_in_order`,
  `test_all`, the three stubs, `TestFunction`, `ResultSink`, `smallest_prime_factor`,
  `composite_by_exponent`, `check_exponent`); the enumerators `OUTCOME_*`, `WORK_*` and the
  `Status` names; `cli::Interface::json_mode` and `skip_ptest`; `cli::parse_args(argc, argv)`; each
  wrapped function defined out of line at file scope with its name on its first line, and the call
  graph of § 2; the two `AGENTS.md` list lines. The checks drive `report_result`, `report_run` and
  `status_of`, and `tests/cli.sh` passes the environment through to `bin/mps` (unsetting named
  variables is fine). A builder who needs otherwise stops and escalates; the gates are frozen.
- **The wrapper trick** renames each wrapped definition in a copy of the tree and links the gate's
  interposer in its place, so every call, from `main.cpp`, the checks or the same file, reaches the
  wrapper. `parse_args` is still renamed by a prepended `#define`, as in cycle 3 (an ODR violation
  in principle; the control run, green, is the check).
- **The logger's critical line from `result.cpp`** is in the default style whatever `-D` says, and
  at `-O0` a shared template instance could take either unit's state
  ([01](research/01-result-and-driver.md)); the build is `-O3`, and the seed
  `issues/logger-state-per-translation-unit.md` owns the cure.
- **Gate time**: P1 about 2½ minutes on this Mac, P2 about 3, P3 and `prepush.sh` about 4 (two
  compilers, the 2,000-exponent sweep, about 100 mutants at `-O0`, eleven rebuilt edits).
- **Cluster queues** as in the previous cycle; P4 has no time-critical step.
- **This cycle's seed** stays `adopted:` with its entry until `code-roadmap` retires both after
  landing.

## 6. Verification strategy

Each phase's `verify:` is `sh spec/verdicts-and-exit-statuses/verify/pN.sh`. The gates are frozen at
the plan commit (each first requires `verify/` to match the commit whose subject starts
`[feature] Plan verdicts-and-exit-statuses`, with nothing untracked), run on this Mac (clang++,
g++-16, GNU make 3.81 at `/usr/bin/make`, git, rsync, awk, python3), and are cumulative. Each prints
`GATE FAIL: <reason> (R-ID)` and exits 1 at the first failure. Each:

1. **builds** a clone-like copy per compiler with `make -B check`: green, no warnings, both summaries
   non-zero and equal across compilers;
2. **drives `bin/mps`** in an empty directory with a clean environment, comparing exit statuses,
   stdout and stderr exactly, and parsing every record with Python (duplicate keys and `NaN`
   rejected, ASCII only, then byte-equal to `json.dumps` of the expected object);
3. **probes** the tree's modules with `verify/probe.cpp` against the gate's own expectations;
4. **proves `make check` pins each requirement**: through `verify/mutant.cpp`, each injected defect
   must turn `make check` red with a failed check (in `tests/cli.sh` itself where the clause is about
   `bin/mps`), and must have been reached; edits to `AGENTS.md`, `src/main.hpp` and `src/math.cpp`
   likewise; and two drives of the mutant build prove `main.cpp`'s path runs through the stop and the
   re-check;
5. **sweeps** `tests/` for a run with a PVAL and without `-p`, help or version that expects exit 0,
   and `src/`/`tests/` for spec ids; checks R12's and R11's same-commit rules (on the commit when it
   exists, else on the working tree against `HEAD`).

**What each gate drives** (cumulative; P1's runs use prime exponents, whose results no later phase
changes):

- **P1** (R1, R2, R6–R10, R12, R13): `mps 2`, `3`, `31`, `999999937`, each plain and with `-t all`:
  exactly the three no-verdict lines and the three start lines, exit 42; `mps 0000000031` says `M31`;
  `mps 31` with both streams merged: each line right after its start line; each of the six other
  `-t` names on 31: its one line and its one start line; `--help`, `--version`, `-p 30` exit 0, an
  unknown option and a bad PVAL exit 1; no ESC on stdout under `FORCE_COLOR=1` (and some on stderr,
  so the check is not vacuous). The probe: every outcome's line, record, `is_decisive` and status,
  through `status_of` and the printer in both modes; an error's line and its empty record; thirteen
  runs ([no verdict, error], [error], [composite, error], [error, composite], [prime, composite],
  [composite, prime], [probable prime, composite], [no verdict, prime, composite], [no verdict, no
  verdict, prime], [no verdict, composite], [no verdict, probable prime], three no verdicts, none):
  status, stdout and stderr; a no verdict printing at once and a verdict held; `run_in_order` on six
  constructed sequences (each result handed over before the next test runs; stops on prime,
  probable prime and composite; continues past an error); `test_all(31)`; the stubs on seven
  exponents; six records parsed by Python, the hostile one by value. `AGENTS.md`: the two whole list
  lines and both sentences, in the commit that adds `report_run` to `src/result.cpp`. With
  `APP_VERSION` changed to `9.9.9-gate`, `make check` green and every line naming it.
- **P2** (R3–R5): R3's four exponents and 9 under no `-t` and all seven names: exactly the `ptest`
  line, exit 40, no start line; every p from 2 to 2,000 with `-t tf` against awk trial division;
  R4's four primes and 999,999,929 with the tests' lines; `-s` and `--skip-ptest` on 4, 33,
  999,999,999 and 31: the tests' lines and start lines, exit 42; `-s -t ll 33`; `-p 33 -s` exit 0.
  The probe: `smallest_prime_factor` for every n below 200,000 and the R3 numbers (with
  31,601² and 31,601·31,607); `check_exponent`'s records, and nothing for seven primes;
  `composite_by_exponent` refusing (33, 0), (33, 1), (33, 5), (33, 33), (33, 66), (4, 4), (1, 1),
  (0, 0), (2, 1) and accepting (33, 11) and (999002449, 31607).
- **P3** (R10, R11): P1's and P2's runs again with `-j` and `--json`: records parsed strictly and
  byte-equal, the status and stderr of the plain run; `-jd`, `-dj`, `-sj 33`, `-js 33`, `31 -j`,
  `-jt ll 31`, `-j -j 31` (one warning); `--json=1` exits 1 with nothing on stdout; `-j -p 30` lists
  the primes; `--help` and `-h` exactly as § 2 gives them, `mps` alone exactly the usage;
  `Exit status:` bold under `FORCE_COLOR=1`, and the coloured help equal to the plain one once the
  escapes are removed; with `APP_VERSION` changed, every record naming it; the parser, help and usage
  in one commit.

**The mutants** (about 100 at P3; each must turn `make check` red with a failed check, and must have
been reached):

- *Status* (P1): no verdict exits 0, or 50; composite exits 42; prime exits 41; probable prime exits
  0; a contradiction exits 40; an error exits 0; a composite exits 42 under `-j` only; and three
  edits moving `PROBABLE_PRIME`, `ERR_UNKNOWN` and `NO_VERDICT` in `enum Status`.
- *Printer* (P1): a contradiction prints its lines, or its records; an error prints its line; a
  verdict prints at once; `json` ignored; a contradiction silent on stderr; an error silent on stderr.
- *Line and record* (P1): for each of no verdict, prime, probable prime, composite by factor and by
  residue: the line without the program version, the record without `record_version`, the record
  without `program`; and once each: a wrong program version, the reason dropped, the exponent off by
  one, "no verdict" worded "composite", invalid JSON, `record_version` 2, `program.version` replaced,
  `exponent` as a string, the worktype swapped, a status word misspelled, no `reason`.
- *Tests* (P1): `test_all` reordered; `run_in_order` not stopping, stopping after the first test, or
  handing results over only at the end; `is_decisive` false for a prime or a probable prime, or true
  for a no verdict; a stub returning prime, composite, or an empty reason; each start line edited;
  four edits of `AGENTS.md` (each list line, each sentence).
- *Exponent check* (P2): the factor's a replaced by the next prime; the largest prime factor in
  place of the smallest; the guard removed; prime squares missed; no answer for each of R3's four
  exponents; a composite answer for each of R4's four primes; the check skipped under each of the
  eight `-t` spellings; `-s` ignored.
- *Flag and help* (P3): `-j` ignored, ignored inside a group, or always on; the help's `-j` line,
  `42` row, `50-59` row and `-s` wording; `[-j]` dropped from `APP_USAGE` alone.

**Pre-push and P4.** `verify/prepush.sh` is everything P4's checklist runs before the push: the P3
gate on the final tree, the deferral ledger (every `issues/*.md` matched one-to-one with a
`**Seed:**` line, every issue `TECH.md` or `ROADMAP.md` names present, this cycle's seed still
`adopted:`), no uncommitted code, and the site-private scan of the cycle's files and of
`origin/main...HEAD`'s messages and patches. `sh verify/prepush.sh scan` runs the scan alone.
`verify/p4.sh` runs the same and then checks the evidence: three `result.json` files that pass the
job's format check, every step passed, GCC 13 or newer, the target's architecture, `<cluster>` and
`<host>` redacted, distinct job ids matching `jobs.tsv`'s `COMPLETED 0:0`; one tested commit, on
`origin/feature/verdicts-and-exit-statuses`, containing `origin/main`, whose code equals `HEAD`'s
outside `spec/`, `issues/` and `ROADMAP.md`, and whose `ci/test-commit.sh` is the script each run
printed; each check log's summaries equal to this tree's counts; the scan covering the evidence.
`verify/redact.sh IN OUT` redacts each piece first. There is no negative control: the job does not
change. `cluster.sh` and `redact.sh` are cycle 3's, with this cycle's paths.

**Dry runs before commit**, on a scratch clone of this branch with `verify/` committed as the plan
commit and a prototype of the whole design (its own checks: 32 in `bin/check`, 378 in
`tests/cli.sh`) left uncommitted, as a builder's tree is when the gate runs:

- The prototype is green under P1 (64 mutants, about 2½ minutes), P2 (85, about 3) and P3 (93,
  about 4). `prepush.sh scan` is clean on it and red with a planted site name.
- Today's tree turns P1 red (`mps 2` exits 0, want 42).
- Three defective variants, each green under its own `make check`, turn the gates red: a
  `check_exponent` that builds its composite without `composite_by_exponent` (P2: `mps 33` under the
  lying search exits 40, want 20); a `test_all` of three plain calls with no stop (P1: `mps 31` with
  a deciding trial factoring prints three lines); a suite looping R3 over five of the eight
  `-t` spellings (P2: the check skipped under `-t trial-factor` leaves `make check` green).
- The first gate drafts caught two weaknesses of the prototype's own checks (an `AGENTS.md` list line
  matched inside § Map, and the hostile record over-pinned to one escape spelling) and two of the
  gates (the R13 pairing sweep matched the suite's own summary line; the R12 needle matched § Map);
  each was fixed before commit.
- `p4.sh` runs its gate, ledger and sweeps on the prototype and stops at the missing evidence; its
  evidence block is cycle 3's, which passed on that cycle's real runs.

The freeze check finds the plan commit by its subject, `[feature] Plan verdicts-and-exit-statuses…`;
a revised plan keeps that prefix.

---

*Backing research: [`research/00-digest.md`](research/00-digest.md).*
