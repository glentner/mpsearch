---
status: unshaped
kind: feature
appetite: small
---

# Say what was found: a result type, one verdict line, exit statuses and `--json`

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

A run never says what it found. The three tests return `bool` (`src/math.hpp:21-24`), and the
dispatch in `src/main.cpp:40-57` throws the value away, so `mps 31` prints three log lines on
stderr and exits 0 whatever the tests answered. The tests answer different questions: trial
factoring finds a factor or doesn't; Lucas–Lehmer proves prime or composite; PRP says probable
prime or composite. A `bool` carries none of that. `test_all` joins the three with `and`
(`src/math.cpp:83-88`), and the author's uncommitted 2024 trial factoring returned `true` both for
"factor found" and for "exponent not prime", which shows how little a `bool` can say.

Nothing checks that the exponent is prime. PVAL is now validated as a decimal integer from 2 to
999,999,999 (`cli-and-sieve-fixes`), but `mps 33` runs every test on a composite exponent, although
the answer is known at once: if p = a·b, then 2^a − 1 divides M_p. `-s/--skip-ptest` is documented
as "Suppress initial prime test on PVAL" (`src/main.hpp:42`) and parsed (`src/cli.cpp:154-156`), but
nothing reads `skip_ptest`, because there is no such test to skip.

The exit statuses (`enum Status`, `src/main.hpp:18-25`) name classes of failure only: 0, then
argument, configuration, file, runtime and unknown errors. None tells prime from composite.

Fleet use needs a machine-readable record per result. Today there is only coloured log text on
stderr.

## Why it was deferred

Pre-existing on `main` since 2024. It was half of the factory adoption review's seed, which put the
parser bugs and the result type together and was retired when its cycle landed. That cycle,
`cli-and-sieve-fixes`, took the mechanical half (the options, PVAL, the sieve) and left this half,
the load-bearing design, to a cycle of its own (its GOAL's non-goals, 2026-10-11). Every test cycle
after it needs a result type to return and a verdict line to grade, so it stays early on the
roadmap.

## Outcome / vision

`mps -t ll 31` prints one verdict line, "M31 is prime", and exits with a status that tells prime,
composite and error apart. A composite exponent is answered at once, naming the factor 2^a − 1.
With `--json`, each result is one JSON object on stdout with the exponent, the test, the outcome,
the factor or residue, and the program version, and the log stays on stderr.

## Sketch of the acceptance criteria

- **R1** — IF the exponent is composite and `--skip-ptest` is absent, THEN `mps` SHALL report M_p
  composite and name the factor 2^a − 1, without running the requested test.
- **R2** — WHEN a test completes, `mps` SHALL print one verdict line naming the exponent, the test
  and the outcome, and exit with a status that distinguishes prime or probable prime, composite,
  and error.
- **R3** — WHERE `--json` is given, `mps` SHALL write each result as one JSON object per line on
  stdout, and log text SHALL stay on stderr.
- **R4** — Every test SHALL return the same result type, and the verdict line, the exit status and
  the JSON record SHALL all be derived from it.

## Notes

- **The exit-status table.** Shaping fixes it. The verdict statuses must not collide with the
  error classes already in `enum Status` (`ERR_ARG` is 1). Burn-in (`issues/burn-in-mode.md`) will
  want "matched the expected answer" as an outcome of its own, so leave room for it.
- **The result type.** Shaping fixes it: a three-way outcome (prime or probable prime; composite
  with a factor; composite by residue) plus an error state, returned by every test. § Invariants 1
  (verdicts are earned) governs what each outcome may claim.
- **The JSON field names.** Shaping decides them. GIMPS's result lines use `exponent`, `worktype`,
  `status`, `res64`, `residue-type`, `fft-length`, `shift-count`, `errors` and `program`. Reusing
  the names that apply makes `mps` records familiar, but submitting to GIMPS is not a goal
  (`ROADMAP.md`).
- **`--skip-ptest`** is parsed and never used today. The composite-exponent check (R1) gives it
  something to skip; if shaping drops that check instead, the option goes with it, and the parser
  and the help text change together (§ Conventions).
- **A no-test mode.** `-t none` was dropped by `cli-and-sieve-fixes` because it ran all three tests.
  If a mode that runs no test is wanted (only the composite check, say), it comes back here, with
  its name in `--help` and the parser in the same commit.
- **Invariants to amend in the same commit.** When this cycle adds `mps`'s own result format, make
  `AGENTS.md` § Invariants' same-commit sentence say "a checkpoint or result format of `mps`", so
  the test job's `result.json` (`ci/test-commit.sh`) is plainly outside it. Then add the new result
  format's paths to the high-blast-radius list.
- **Coupled core.** The parser, the help text and README usage change together (§ Conventions); the
  README documents no options today.
- Related: `spec/cli-and-sieve-fixes/GOAL.md` (its non-goals, and its Seed link to the retired
  parent seed), `issues/burn-in-mode.md`, `issues/lucas-lehmer-test.md`,
  `issues/prp-gerbicz-test.md`, `issues/trial-factoring.md`.
- Found by: `cli-and-sieve-fixes` P4 (R11), split from the factory adoption review's seed of
  2026-10-11.
