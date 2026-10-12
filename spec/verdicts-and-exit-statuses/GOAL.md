# GOAL — Say what was found: a result type, one verdict line, exit statuses and `--json`

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** verdicts-and-exit-statuses
- **kind:** feature
- **appetite:** small

## Problem

A run never says what it found. All of this is pre-existing on `main` (`899b3de`).

- The three tests return `bool` (`src/math.hpp:21-24`), and the stubs behind them return `true`
  whatever they were asked (`src/math.cpp:62-80`). `test_all` joins them with `and`
  (`src/math.cpp:83-88`). A `bool` can't carry what the tests answer: trial factoring and P-1 find
  a factor or don't, Lucas–Lehmer proves prime or composite, and PRP says probable prime or
  composite.
- The dispatch throws the value away (`src/main.cpp:40-57`). `mps 31` prints three log lines on
  stderr, nothing on stdout, and exits 0, which reads as success although no test decided anything.
- Nothing checks that the exponent is prime. PVAL is validated as a decimal integer from 2 to
  999,999,999 (`cli-and-sieve-fixes`), but `mps 33` runs every test, although 2^3 − 1 already
  divides M_33. `-s/--skip-ptest` is documented as "Suppress initial prime test on PVAL"
  (`src/main.hpp:42`) and parsed into `skip_ptest` (`src/cli.cpp:153-156`, `src/cli.hpp:96`), but
  nothing reads it.
- The exit statuses (`enum Status`, `src/main.hpp:18-25`) name only success and classes of failure:
  0, then 1, 2, 10, 20 and 30. None tells prime from composite.
- Fleet use needs a machine-readable record per result, and burn-in and the fleet reports
  (`ROADMAP.md` Part III) need its field names fixed early. Today there is only log text on stderr.

§ Invariants 1 (verdicts are earned) governs what any of this may claim.

## Outcome / vision

Every test returns one result type, and the verdict line, the exit status and the JSON record are
three views of it. A composite exponent is answered before any test runs, naming the factor
2^a − 1. Each completed test prints one line on stdout saying what it found, and `-j/--json` turns
those lines into one JSON object each. The exit status tells prime, probable prime, composite, no
verdict and error apart.

The tests' arithmetic stays as it is: stubs that decide nothing. They now say so, so **`mps 31`,
which exits 0 today, will print three no-verdict lines and exit 42**, and every check that pins
today's 0 for a prime exponent changes with it (`tests/cli.sh`). `mps 999999999`, which exits 0
today, will report M_999999999 composite with the factor 2^3 − 1 and exit 40.

## Acceptance criteria (the contract)

*The tests* are trial factoring, P-1 and Lucas–Lehmer (`-t` names `tf`, `p1`, `ll`). *The exponent
check* is `mps`'s check that PVAL is prime, run before any test. A *result* is what a test or the
exponent check produces; its *outcome* is one of **prime**, **probable prime**, **composite**, **no
verdict** and **error**. A *decisive* result is prime, probable prime or composite. *The status
table* is:

| Status | Meaning |
|---|---|
| 0 | prime, or a run that gives no verdict by design (`--help`, `--version`, `-p`) |
| 1, 2, 10, 20, 30 | the existing error classes of `enum Status`, unchanged |
| 40 | composite |
| 41 | probable prime |
| 42 | no verdict |
| 50–59 | reserved for burn-in's outcomes; none is defined or produced here |

- **R1** — `test_quick_factor`, `test_p_minus_one`, `test_lucas_lehmer` and `test_all` SHALL each
  return the same result type, never `bool`. A result SHALL hold exactly one outcome, SHALL name the
  test that produced it, and SHALL carry the factor for a composite by factor, the 64-bit residue
  for a composite by residue, and the reason for a no verdict. The verdict line (R6), the exit
  status (R8) and the JSON record (R10) SHALL each be derived from results alone.
- **R2** — WHEN trial factoring, P-1 or Lucas–Lehmer runs, it SHALL return no verdict with a reason
  stating that the test is not implemented yet, and SHALL never return prime, probable prime or
  composite. Each test's start line on stderr is unchanged.
- **R3** — IF PVAL is composite and `-s/--skip-ptest` is absent, THEN `mps` SHALL run no test (no
  test's start line appears on stderr), SHALL report one composite result for M_PVAL from the
  exponent check naming the factor 2^a − 1 with a, the smallest prime factor of PVAL, given exactly,
  and SHALL exit 40. Before reporting, `mps` SHALL confirm that a divides PVAL. `make check` SHALL
  show this for at least 4 (a = 2), 33 (a = 3), 999,999,999 (a = 3) and 999,002,449 = 31,607²
  (a = 31,607, the largest prime factor trial division up to 31,622 must reach), under every `-t`
  name.
- **R4** — WHEN PVAL is prime, the exponent check SHALL print nothing on stdout and the requested
  tests SHALL run. `make check` SHALL show this for at least 2, 3, 31 and 999,999,937.
- **R5** — WHERE `-s/--skip-ptest` is given, `mps` SHALL run the requested tests on any valid PVAL,
  composite or prime, and SHALL print no exponent-check result. `mps -s 33` SHALL run all three
  tests and exit 42.
- **R6** — WHEN a test completes, or the exponent check finds PVAL composite, `mps` SHALL print
  exactly one line on stdout for that result, naming the exponent as `M<p>`, the test (or the
  exponent check), the outcome, the factor or residue for a composite or the reason for a no
  verdict, and the program as `mps` with its `APP_VERSION`. Without `--json`, stdout SHALL hold these lines and nothing else; log text SHALL stay on
  stderr. The plan chooses the wording and where on the line the program version goes.
- **R7** — WHEN `-t all` runs (or `-t` is absent), `mps` SHALL run trial factoring, P-1 and
  Lucas–Lehmer in that order and stop after the first decisive result, printing one line per
  completed test in the order they ran. `mps 31` SHALL print three no-verdict lines, for `tf`, `p1`
  and `ll` in that order, and exit 42.
- **R8** — `mps` SHALL exit with the status the status table gives for its result: 0 for prime, 40
  for composite, 41 for probable prime, 42 for no verdict. WHEN a run gives several results, the
  status SHALL be the decisive result's, or 42 if none decided. `--help`, `--version` and `-p` SHALL
  still exit 0, the error classes SHALL keep their values, and no run SHALL exit with a status from
  50 to 59. `make check` SHALL show the status for a result of every outcome, constructed directly
  where no test can produce it yet.
- **R9** — IF a result's outcome is error, or a run yields both a prime and a composite result for
  the same exponent, THEN `mps` SHALL print no verdict line and no JSON record for it, SHALL report
  the failure on stderr, and SHALL exit non-zero with one of the existing error classes: 30 for a
  prime and a composite together. `make check` SHALL show both, with constructed results.
- **R10** — WHERE `-j/--json` is given, `mps` SHALL print, in place of each verdict line, exactly one
  JSON object on one line of stdout, and every such line SHALL parse as JSON. Each object SHALL hold
  `exponent` (a number); `worktype` (the test's `-t` name `tf`, `p1` or `ll`, or `ptest` for the
  exponent check); `status` (`prime`, `probable-prime`, `composite` or `no-verdict`); `factors` (an
  array of exact strings) when composite by factor; `res64` when composite by residue; a field the
  plan names holding the reason when no verdict; `program`, exactly
  `{"name": "mps", "version": "<APP_VERSION>"}`; and a field the plan names holding the record's own
  format version, the integer 1. `program.version` SHALL stay the program's version, never the
  format's. The exponent check's factor SHALL be written
  exactly and machine-readably as 2^a − 1, in a form the plan chooses. Log text SHALL stay on
  stderr, the exit status SHALL equal the run's status without `-j`, and an error SHALL write no
  JSON. `-j` SHALL be a boolean flag that groups with the others (`-jd`, `-sj`).
- **R11** — `mps --help` SHALL list `-j, --json`, describe `-s, --skip-ptest` as skipping the
  exponent check, and list the status table: 0, 1, 2, 10, 20, 30, 40, 41, 42 and the reserved
  50–59, each with its meaning. The parser, the help text and the usage line SHALL change in the
  same commit.
- **R12** — The commit that adds the result format SHALL change `AGENTS.md` § Invariants so that
  both sentences that bind a checkpoint or result format name "a checkpoint or result format of
  `mps`": the high-blast-radius paragraph ("A cycle that adds … a checkpoint or result format of
  `mps`, … adds its paths here in the same commit") and § Conventions ("A change to a checkpoint or
  result format of `mps` bumps that format's version in the same commit"). The cluster job's
  `result.json`, versioned by its own header, is then plainly outside both. The same
  commit SHALL add to the high-blast-radius list the paths of the new result format and of the
  driver that prints verdicts.
- **R13** — `make check` SHALL pass and SHALL carry a check for each of R1–R12. No check under
  `tests/` SHALL still expect `mps PVAL` to exit 0 for a valid PVAL without `-p`. Among them,
  `make check` SHALL show the program version on every verdict line, and the format-version field,
  holding 1, and `program` on every JSON record.

## Non-goals (no-gos)

- The tests' arithmetic: trial factoring, P-1 and Lucas–Lehmer stay stubs, and PRP stays absent. The
  probable-prime outcome and status 41 exist so the type needn't change when a PRP test arrives.
- Burn-in's own outcomes (`issues/burn-in-mode.md`), beyond reserving 50–59.
- A no-test mode: the exponent check already answers a composite exponent before any test.
- Printing a large factor in decimal: that waits for the reference big integer
  (`issues/reference-big-integer.md`).
- JSON records for errors.
- Checkpoints: `-f` still sets a path nothing reads or writes.
- The logger-state seed (`issues/logger-state-per-translation-unit.md`).
- Submitting results to GIMPS, or matching its record beyond reusing its field names.
- A `README.md` usage section: the README documents no options today, so the coupled core's README
  leg has nothing to change (as in `cli-and-sieve-fixes`).

## Clarifications

The author delegated every gate in this effort to the foreman on 2026-10-11; these are the
foreman's decisions under that delegation.

- **Q:** One cycle, or a pilot and follow-ups? — **A:** One cycle. The result type, the verdict
  line, the exit statuses and the JSON record are four views of one design, and burn-in and the fleet
  reports need the field names fixed early (resolved 2026-10-12).
- **Q:** What does a test return? — **A:** One result type for every test, whose outcome is exactly
  one of prime (only from a complete Lucas–Lehmer test ending in a zero residue), probable prime
  (only from a complete PRP test ending in the prime residue; none exists yet), composite (with a
  factor re-checked exactly, or with a nonzero final residue from a run whose every error check
  passed), no verdict (the test finished without deciding, or is not implemented yet), or error
  (never a verdict: reported, with a non-zero exit). A result names its test and carries the factor,
  the residue or the reason. The stubs stay stubs and report no verdict honestly. Confirmed against
  `src/math.hpp:21-24` and `src/math.cpp:62-88` (resolved 2026-10-12).
- **Q:** Does `mps` check the exponent? — **A:** Yes, by trial division before any test; PVAL ≤
  999,999,999, so divisors up to 31,622 suffice. If p is composite with smallest prime factor a,
  then 2^a − 1 divides 2^p − 1 and 1 < 2^a − 1 < 2^p − 1, so M_p is composite. The exact re-check
  § 1 asks for is that a divides p, since 2^p ≡ 2^(p mod a) ≡ 1 (mod 2^a − 1). The factor is named
  exactly but symbolically, with a given. `--skip-ptest` skips the check, and a prime exponent prints
  nothing for it. Confirmed: `skip_ptest` is parsed and never read (resolved 2026-10-12).
- **Q:** What is the verdict line? — **A:** One line on stdout per completed test, naming `M<p>`,
  the test, the outcome, and the factor, residue or reason; the plan chooses the wording. Logs stay
  on stderr. `-t all` runs tf, p1 and ll in order and stops at the first decisive verdict (resolved
  2026-10-12).
- **Q:** Which exit statuses? — **A:** The status table above. The existing classes keep their
  values (confirmed against `src/main.hpp:18-25`), 41 is apart from 0 because a probable prime is not
  a proof, 50–59 are reserved for burn-in, `--help` lists the table, and a prime and a composite for
  the same exponent is an internal error, 30 (resolved 2026-10-12).
- **Q:** What does `--json` write? — **A:** `-j/--json`, a groupable boolean flag (`-j` is free:
  `src/cli.hpp:65-74`), writes one JSON object per result per line on stdout in place of the verdict
  lines, with GIMPS's field names where they apply (`exponent`, `worktype`, `status`, `factors`,
  `res64`, `program`) plus a no-verdict reason; the plan names that field and the form of 2^a − 1.
  The exit status is unchanged by the flag, and an error writes no JSON (resolved 2026-10-12).
- **Q:** What changes in the same commit? — **A:** The parser, `--help` and the README's usage, which
  documents no options today. `AGENTS.md` § Invariants, in the commit that adds the result format:
  the same-commit sentence names "a checkpoint or result format of `mps`", and the new format's and
  the verdict-printing driver's paths join the high-blast-radius list (resolved 2026-10-12).
- **Q:** § Conventions says a change to a result format bumps that format's version, and
  § Invariants 1 says every result names the program version; the record had no format version and
  the verdict line no program version. Where do they live? — **A:** The JSON record gets one more
  field, named by the plan, holding its own format version: an integer starting at 1, bumped in the
  same commit as any change to the record. `program.version` stays the program's version, not the
  format's. The verdict line names the program as well (`mps` and `APP_VERSION`), so every printed
  result meets § 1 on its own; the plan chooses where on the line (resolved 2026-10-12).
- **Q:** Which `AGENTS.md` sentences change, and does the README gain usage? — **A:** Both sentences
  that bind a checkpoint or result format: the high-blast-radius paragraph and § Conventions'
  version-bump rule each say "a checkpoint or result format of `mps`"; the cluster job's
  `result.json` has its own versioned `format` field under its header's rules. The README stays as
  it is: it documents no options, as `cli-and-sieve-fixes` decided (resolved 2026-10-12).
- **Q:** What is out of scope? — **A:** Everything in Non-goals (resolved 2026-10-12).
- **Q:** Two readings of R8 and R9 for runs `mps` cannot produce yet. The plan raised them at the
  PLAN/TECH sign-off. — **A:**
  1. A probable prime and a composite for the same exponent are a contradiction, like a prime and a
     composite. `mps` prints neither and exits 30 (R9).
  2. A run that holds an error result prints no decisive verdict at all: no prime, probable prime
     or composite line or record. That holds even for a test that completed before the error
     (§ Invariants 1: a run whose checks fail never prints a verdict). No-verdict lines already
     printed for earlier tests stay, and the run exits with the error's class. In this sense R6's
     "WHEN a test completes … SHALL print" yields to R9 and § 1.

  Neither changes what any run of today's `mps` prints. `-t all` stops at the first decisive
  result, and a single test gives one result (resolved 2026-10-12).

## Related materials

- Seed: [`issues/verdicts-and-exit-statuses.md`](../../issues/verdicts-and-exit-statuses.md);
  `ROADMAP.md` Part I, "Say what was found: a result type, one verdict line, exit statuses and
  `--json`"
- Previous cycle: [`spec/cli-and-sieve-fixes/GOAL.md`](../cli-and-sieve-fixes/GOAL.md), whose
  non-goals left this half for its own cycle
- Test entry points (coupled core, high-blast-radius): `src/math.hpp`, `src/math.cpp`, the dispatch
  in `src/main.cpp`
- Command-line surface (coupled core): `src/main.hpp` (usage, help, `enum Status`, `APP_VERSION`),
  `src/cli.hpp`, `src/cli.cpp`
- Checks: `tests/check_cli.cpp`, `tests/check_math.cpp`, `tests/cli.sh`
- `AGENTS.md` § Invariants 1, the high-blast-radius list, and § Conventions
- Later consumers: `issues/burn-in-mode.md`, `issues/fleet-runs-and-reports.md`,
  `issues/lucas-lehmer-test.md`, `issues/prp-gerbicz-test.md`, `issues/trial-factoring.md`,
  `issues/pminus1-factoring.md`
