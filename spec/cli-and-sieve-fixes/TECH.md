---
slug: cli-and-sieve-fixes
title: Options that parse as documented, and a sieve right at its edges
kind: fix
appetite: small
status: done
branch: fix/cli-and-sieve-fixes
base: main
current_phase: done
last_updated: '2026-10-12'
phases:
- id: P1
  name: The sieve right at its edges, and -p listing 2
  status: done
  satisfies:
  - R8
  - R9
  - R10
  depends_on: []
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/cli-and-sieve-fixes/verify/p1.sh && sh spec/cli-and-sieve-fixes/remediation/f1.sh
- id: P2
  name: PVAL and -p N validated; -- ends the options
  status: done
  satisfies:
  - R6
  - R7
  - R8
  - R10
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/cli-and-sieve-fixes/verify/p2.sh
- id: P3
  name: The options parse as --help says
  status: done
  satisfies:
  - R1
  - R2
  - R3
  - R4
  - R5
  - R10
  depends_on:
  - P2
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/cli-and-sieve-fixes/verify/p3.sh && sh spec/cli-and-sieve-fixes/remediation/f1.sh
    && sh spec/cli-and-sieve-fixes/remediation/f2.sh
- id: P4
  name: Check audit, the follow-up seed, deferral ledger, cluster runs
  status: done
  satisfies:
  - R10
  - R11
  depends_on:
  - P3
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/cli-and-sieve-fixes/verify/p4.sh && sh spec/cli-and-sieve-fixes/remediation/f1.sh
    && sh spec/cli-and-sieve-fixes/remediation/f2.sh
review:
  last_reviewed_commit: 2a21ffc86eed7a27072debf13573db25c4bb88f4
  verdict: approved
  blocked_reason: ''
  cycle: 2
---
# TECH.md — Options that parse as documented, and a sieve right at its edges

The **context engine and finite-state machine** for building this fix. The YAML frontmatter above is
the resume ground-truth (read it from the cycle worktree with
`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/cli-and-sieve-fixes/TECH.md`); the
per-phase checklists below are the work. `code-build` executes the next actionable phase, runs its
`verify:` command, updates state via
`uv run --with pyyaml python "$KB/bin/factory/set_phase.py" …`, and makes one atomic code+state commit.

- **Vision / requirements (locked):** [`GOAL.md`](GOAL.md) — R-IDs are the contract.
- **Authoritative design:** [`PLAN.md`](PLAN.md).
- **Backing research:** [`research/00-digest.md`](research/00-digest.md) + briefs.

## Frontmatter field reference

- `status` (top): `planned | in_progress | blocked | in_review | done`.
- `appetite`: `small` — caps phase count and build-iteration budget (circuit breaker).
- `branch`, `base`: the cycle's branch and the profile's `base`.
- phase `status`: `pending | in_progress | done | blocked`
- `satisfies`: GOAL R-IDs this phase delivers (traceability anchor for `code-review`).
- `depends_on`: phase ids that must be `done` first.
- `parallel`: `false` throughout: every phase touches the coupled core (the command-line surface
  and the test entry points) or follows one that does.
- `hammerable`: `false` throughout: each phase is a correctness phase, and R10 is a gate on gates.
- `hill`: `uphill` → `crest` → `downhill`.
- `attempts`: durable failed-verify counter, bumped by `set_phase.py --phase P<n> --record-attempt`.
- `verify`: the exact command that proves the phase.
- `review.cycle`: completed review passes.

## Conventions (apply to every phase)

- `AGENTS.md` is the constitution; walk the § Invariants sections PLAN § 3 lists. `src/math.cpp` is
  a high-blast-radius path.
- One phase per `code-build` invocation; one atomic commit with the code and the `TECH.md` state,
  subject `[fix] Build cli-and-sieve-fixes P<n>: …`, no trailers.
- Same-commit rules: the parser (`src/cli.*`) and the help text (`src/main.hpp`) change together
  (P3); the README has no usage to change (GOAL non-goal). No R#/P# ids in `src/` or `tests/`.
- **The gates are fixed.** `spec/cli-and-sieve-fixes/verify/` is reviewed as of the plan commit;
  review checks it is unchanged (PLAN § 6). If a gate is wrong, stop and escalate rather than edit it.
- The gates run on macOS and need clang++, g++-16, GNU make 3.81 at `/usr/bin/make`, git, rsync and
  awk; P4 adds python3 and the site file. Each prints `GATE FAIL: <reason> (R-ID)` and exits 1 on the
  first failure, or ends with a `Pn gate: green (…)` line. Each is cumulative: it re-proves every
  earlier phase.
- **What the gates read of the tree** (PLAN § 5, first risk): the exact help text; the
  `cli::Interface` fields `pval`, `pval_number`, `test_case`, `opt_test_case`, `file_path`,
  `opt_file_path`, `show_primes`, `skip_ptest`, `debug_mode`, `detailed_mode`; the enumerators
  `TEST_ALL`, `TEST_FACTOR`, `TEST_P_MINUS_ONE`, `TEST_LUCAS_LEHMER`; the free function
  `cli::parse_args(argc, argv)`; `math.hpp` unchanged, and `main.cpp` calling each of its four test
  functions once; a PVAL error message holding `PVAL '<value>'`; `-<c>` in an unknown flag's
  message; `Duplicate use of -<c>/` warnings. The checks parse through `cli::parse_args(argc, argv)`, and `tests/cli.sh` passes
  the environment through to `bin/mps` (unsetting named variables is fine; `env -i` is not), because
  the gates inject defects through both.
- **Every check is one condition.** In `tests/cli.sh`, `expect NAME [ a ] && [ b ]` counts only
  `[ a ]`; write two `expect` lines. Keep `planned` exact.

---

## Phase P1 — The sieve right at its edges, and -p listing 2
**Satisfies:** R8, R9, R10 (the sieve's checks) · **Depends on:** —
**Goal:** `prime_sieve(n)` returns n entries, true exactly at the primes below n, for every n ≥ 0,
writing nothing outside them; `mps -p N` lists every prime below N, 2 included; `make check` pins
both with no caveat left in `tests/check_math.cpp`.

- [x] `src/math.cpp`: `prime_sieve` and `display_primes` as PLAN § 2 "The sieve" describes: start
      all-false, entry 2 only when n > 2, odd i ≥ 3, strike odd multiples from i·i while
      `i * i < n`; list from 2. Doc blocks at the top of each body; the sieve's cites Crandall and
      Pomerance, *Prime Numbers: A Computational Perspective*, 2nd ed., 2005, ch. 3. Nothing else in
      `src/math.*` changes.
- [x] `tests/check_math.cpp`: n = 0, 1, 2, 3 exactly (size and every entry); every index from 0 for
      the existing n; the squares check kept; `display_primes(n)`'s output exact for small n
      (redirect `std::cout`), including n ≤ 2 printing nothing. Replace the header's caveat with
      the hardened invocation from PLAN § 2 ("writes nothing outside them").
- [x] `tests/cli.sh`: `-p 0`, `-p 1`, `-p 2` exit 0 with empty stdout; `-p 3`, `-p 4`, `-p 30`,
      `-p 31` and `-p 32` exactly, 2 included, trailing newlines included (PLAN § 2, "The checks");
      drop the "a leading 2 is accepted too" branch and its comment; `planned` updated. The header's
      command-line caveat stays until P3, which fixes the rest.

**Reopened 2026-10-12 for review cycle 1, F1** (`REVIEW.md`). R9 names `make check`, so the
plain check build must itself see a write outside the sieve's entries; PLAN § 3 deviation 1, which
left that to a hardened rebuild named in a comment, is overruled. The class: every object `bin/check`
links is built without the libraries' bounds assertions, so `make check` cannot see an in-word
stray write, and every live text that tells the reader to add flags for it. The predicate is the
build itself (`make -n -B check`: each `bin/check` compile carries the assertions), and
`remediation/f1.sh` evaluates it by restoring the stray write; the text sweep found the header of
`tests/check_math.cpp` and the build block of `AGENTS.md`, and nothing in `ci/` or `README.md`.

- [x] `Makefile`: `bin/check` links its own object set under `lib/check/` (the modules from the
      same `MODULES`, and `tests/*.cpp`), compiled with `BOUNDS` (`-D_GLIBCXX_ASSERTIONS
      -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE`, the documented hardened run's
      tokens) ahead of the IEEE flags, which stay last before `-c`. `bin/mps` and `lib/*.o` keep
      their flags. The `lib/check/` rules come before `lib/%.o`, so make 3.81, which takes the first
      pattern with a buildable source, and 4.x, which takes the shortest stem, both pick them; the
      old `lib/%.o: tests/%.cpp` rule goes. `make clean` removes `lib/check/*.o` and `*.d`.
- [x] `tests/check_math.cpp`: the header says what is now true: `make check` builds the checks with
      the bounds assertions; libc++ checks from its fast mode, libstdc++ checks `vector<bool>` from
      GCC 15, and an older libstdc++ sees the values only. The hardened invocation goes.
- [x] `AGENTS.md` § Build and run: `make check` builds `bin/check` with the bounds assertions, its
      objects in `lib/check/`.
- [x] `src/math.*` untouched.
- **Verify (reopened):** `verify/p1.sh` then `remediation/f1.sh` — plain `make -B check` green on
  clang++ and g++-16 with one summary per half and no warnings, red under the stray write for
  n = 1, 2 and for n = 2 alone with both programs built, green under the guard written `2 < n`;
  `bin/mps` compiled without bounds assertions; the IEEE flags last on every compile. `verify/p3.sh`
  re-run green (82 mutants). F1's gate was seen red against the old Makefile first.
- **Verify:** `sh spec/cli-and-sieve-fixes/verify/p1.sh` — `make -B check` green on clang++ and
  g++-16 without warnings; `mps -p N` exact for N in 0…4, 30, 31, 32, 100, 1000, 7920 and every `-p`
  spelling, on both builds; π(10^8) by count and last line; the sieve probe over n = 0…3000 and five
  large n, and over n = 0…64 under the bounds assertions, behind the canary; `make check` red under
  each of the 15 sieve and listing mutants (the listing without its 2 failing `tests/cli.sh`
  itself) and under `d179aa4`'s sieve; `make check` rebuilt with the bounds assertions green on both
  compilers and red under the old in-word write for n = 1 and n = 2; no caveat in
  `tests/check_math.cpp`; `src/math.hpp` and `README.md` unchanged; no spec ids in source.
- **Touches:** `src/math.cpp`, `tests/check_math.cpp`, `tests/cli.sh`; reopened: `Makefile`,
  `tests/check_math.cpp`, `AGENTS.md`.

## Phase P2 — PVAL and -p N validated; -- ends the options
**Satisfies:** R6, R7, R8 (N ≤ 2 through a validated N), R10 (the PVAL checks) · **Depends on:** P1
**Goal:** a PVAL that is not one or more ASCII digits from 2 (0 under `-p`) to 999,999,999 is an
argument error naming PVAL and the value, exit 1, with no test run and nothing on stdout; leading
zeros are allowed; `--` ends the options without becoming PVAL.

- [x] `src/cli.hpp`, `src/cli.cpp`: `PVAL_MAX`, `pval_number`, `set_pval` with `std::from_chars`
      and the message in PLAN § 2 "PVAL"; drop the 9-character check; the default `file_path` set
      after validation, from the string as typed.
- [x] The argument loop: `--` sets positional-only and is not pushed; a lone `-` is positional.
      The option walks themselves stay for P3.
- [x] `src/main.cpp`: no `std::stoi`; `display_primes(opt.pval_number)` and
      `pval = opt.pval_number`.
- [x] `tests/check_cli.cpp`: the PVAL table at parse level (rejected without `-p`: `abc`, `31abc`,
      `3.5`, `" 31"`, `"31 "`, `+31`, `""`, `0`, `1`, `00`, `1000000000`, a value past 2^64, `-1`
      through `--`; rejected under `-p`: `abc`, `1000000000`, `""`, a value past 2^64; accepted: `2`, `999999999`, `0000000031` (number 31, string
      kept), `-p 0`, `0 -p`, `-p 999999999`), each error holding `PVAL '<value>'`.
- [x] `tests/cli.sh`: each bad PVAL exits 1, names PVAL and the value, has no "Uncaught exception"
      and no `Running` line; `-p abc` and `-p 1000000000` exit 1 with empty stdout; the old
      "a 10-digit PVAL exits 1" check folds into this; `planned` updated.
- **Verify:** `sh spec/cli-and-sieve-fixes/verify/p2.sh` — all of P1's gate, plus: 20 bad PVALs and
  17 bad `-p` values through `--` (junk, signs, whitespace, a non-ASCII digit, past the bound and
  past 2^64), each with exit 1, `PVAL '<value>'`, empty stdout, no "Uncaught exception" and no
  `Running`; good PVALs (leading zeros, the bound) running the three tests; `-p 0`, `-p 000`; the
  parse probe at the bound and on order; `make check` red under each of the 14 PVAL mutants.
- **Touches:** `src/cli.hpp`, `src/cli.cpp`, `src/main.cpp`, `tests/check_cli.cpp`, `tests/cli.sh`.

## Phase P3 — The options parse as --help says
**Satisfies:** R1, R2, R3, R4, R5, R10 · **Depends on:** P2
**Goal:** `--help` lists exactly the options that parse, in every spelling it states; each `-t`
name runs exactly its test; any other value is an argument error naming it; grouped flags act as
given separately; `--file` and `-f=` set exactly the path and create nothing; no file under `tests/`
calls these defects known.

- [x] `src/cli.hpp`: `TestCase` without `TEST_NONE`; `test_by_name` with exactly the seven names in
      PLAN § 2, in the table style of § Code style.
- [x] `src/cli.cpp`: the short-word walk and the long-option split of PLAN § 2 "The argument loop
      and the options", one setter per valued option, the unified missing- and empty-value
      messages, duplicate warnings kept; `set_file_path` records only (drop `std::ofstream` and
      `<fstream>`).
- [x] `src/main.hpp`: the help text exactly as PLAN § 2 "The help text" gives it, in the same commit
      as the parser (same-commit rule).
- [x] `src/main.cpp`: `case cli::TEST_ALL`, no `default`.
- [x] `src/ansi.cpp`: `colorize_usage` colours an option only at the start of the text or after a
      space, `[` or `(` (PLAN § 2, § 3 deviation 3).
- [x] `tests/check_cli.cpp`: the `describe` comparison across the five spellings of both options
      (`no-such-dir/x=y.mps`), `--file out.mps -t ll 31`; each name and alias selecting its test, and
      no `-t` selecting all; R3's values (`qf`, `quick`, `factor`, `pm`, `p-min`, `none`, `""`, an
      unknown name) in every spelling, each error quoting the value; groups against their separated
      flags, `-hv` the help, `-vh` the version, `-hx` the help, `-dx` and `-xh` errors naming the stray
      character. Replace the header's caveat.
- [x] `tests/cli.sh`: `--help` holds each name with its alias, `(default: 'all')`, the five spellings
      and "below PVAL"; each name's whole stderr; `31` runs all three; `--file out.mps -t ll 31` runs
      Lucas–Lehmer alone; drop the header's "Only behaviour that is right today" caveat; `planned`
      updated.

**Reopened 2026-10-12 for review cycle 1, F2** (`REVIEW.md`). R1 says the help states the five
spellings of a valued option *once*, and `tests/cli.sh` only asked that each be present. The class:
every part of R1's grammar statement a duplicate would repeat, which is each of the five spellings,
bare of the punctuation around them so a reworded copy counts too. The sweep found the help checked
only in `tests/cli.sh`; `tests/check_cli.cpp` holds the spellings as parse inputs, not as help text.
The help in `src/main.hpp` is right and pinned by the frozen gates, so `src/` is untouched.

- [x] `tests/cli.sh`: a `once` helper beside `has` and `lacks`, built from `case` alone; one
      `expect` per spelling (`-x VALUE`, `-xVALUE`, `-x=VALUE`, `--long VALUE`, `--long=VALUE`)
      that the help holds it exactly once; `planned` 121 → 126. Seen red first: a second `-xVALUE`
      in `-f`'s line, and a second `--long=VALUE` in the grammar sentence, each fail one check.
- **Verify (reopened):** `verify/p3.sh`, then `remediation/f1.sh` and `remediation/f2.sh` — all of
  P3's gate on the cli count of 126, P1's F1 gate re-proved, and `make check` red with the grammar
  sentence given twice.
- **Verify:** `sh spec/cli-and-sieve-fixes/verify/p3.sh` — all of P2's gate, plus: the exact help
  on both spellings and both builds, the aliases whole in colour; each name and alias, two
  spellings each, with exact stderr; `31` with all three; R3's 12 values (`=ll` among them) in all
  five spellings; the 50 spelling combinations alike in the parse probe; `-f`'s `=` handling;
  missing and empty values; 11 group pairs alike, stray characters named; `-dd` and `-ss` matching
  their separated forms on stderr; `--`, a lone `-`; no file created by any run or probe;
  `make check` red under each of 32 option mutants, 15 help edits and 4 dispatch edits; no caveat
  in any file under `tests/`.
- **Touches:** `src/cli.hpp`, `src/cli.cpp`, `src/main.hpp`, `src/main.cpp`, `src/ansi.cpp`,
  `tests/check_cli.cpp`, `tests/cli.sh`; reopened: `tests/cli.sh`.

## Phase P4 — Check audit, the follow-up seed, deferral ledger, cluster runs
**Satisfies:** R10 (the audit), R11 · **Depends on:** P3
**Goal:** the final tree passes every gate at once; the follow-up cycle is seeded and indexed; the
pushed head passes `make check` on the three GCC cluster targets, recorded as redacted evidence.

**Graded by the orchestrator** for the cluster runs: the evidence lives in
`spec/cli-and-sieve-fixes/evidence/`, which the blind reviewer cannot see; the orchestrator grades
it from the job ids, `sacct` and the session's MCP transcript. R11's seed and entry live in
`issues/` and `ROADMAP.md`, which the reviewer sees.

**Order matters.** The tested commit's code must equal `HEAD`'s outside `spec/`, `issues/` and
`ROADMAP.md`. So any code the audit changes is committed first, as
`[fix] Build cli-and-sieve-fixes P4: <what>` with the phase still `in_progress`; then push, run,
and make the phase commit with the evidence and the state.

- [x] Audit: walk R1–R9 against `tests/` and confirm each has a check of its own, one condition per
      `expect`; no file under `tests/` describes the fixed defects as known. Fix and commit anything
      found (above).
- [x] R11: `issues/verdicts-and-exit-statuses.md` from the ISSUE template, `status: unshaped`,
      `kind: feature`, carrying the GOAL's Non-goals follow-up scope: the seed's R4–R6 (the
      composite-exponent answer, one verdict line with an exit status per outcome, `--json`); the
      exit-status table, with room for burn-in's "matched the expected answer"
      (`issues/burn-in-mode.md`); the result type every test returns; the JSON field names;
      `--skip-ptest`, parsed and never used; a no-test mode, if one is wanted. Evidence carried over
      from `issues/results-and-cli-fixes.md`, not copied verbatim. Its `ROADMAP.md` entry directly
      after this cycle's ("Say what was found: …", seed `issues/results-and-cli-fixes.md`), which
      stays `adopted:` until `code-roadmap` retires it after landing.
- [x] Deferral ledger: walk P1–P4 of this file for "do not fix here", "known limitation", "left as
      a follow-up" and any deferral the build introduced; each needs an `issues/<slug>.md` from the
      ISSUE template and a `ROADMAP.md` entry. None is planned besides R11's. Every `issues/*.md`
      matches exactly one `**Seed:**` line.
- [x] Run P3's gate and the site-private scan (`sh spec/cli-and-sieve-fixes/verify/p4.sh` fails only
      on the missing evidence at this point), then push: `git push origin fix/cli-and-sieve-fixes`,
      fast-forward only (authorized by the foreman for this phase). Never force.
- [x] Submit the pushed head on `x86-64-avx512`, `x86-64-avx2` and `aarch64-gb10` through the
      cluster MCP server, with the site recipe the foreman hands over (accounts, QoS, paths; never
      committed): write the job from the pushed commit's `ci/test-commit.sh` to
      `<scratch>/mpsearch/jobs/` without its final newline and check its `git hash-object` before
      `sbatch`; `sbatch` with `script_path="<file> <hash> <target>"`, the target file's resources and
      `output` in that directory (cycle 2's recipe, `ci/test-commit.sh`'s header). Limits: ≤ 30 min,
      ≤ 8 cores, ≤ 16 GB, at most two queued per cluster (one on the GB10 cluster), output only under
      `<scratch>/mpsearch/`. Resubmit a run that fails for a reason outside the tree (preemption, a
      node fault); record only runs that test the tree.
- [x] Evidence, each piece through `sh spec/cli-and-sieve-fixes/verify/redact.sh IN OUT`:
      `evidence/<target>.json` (the run's `result.json`), `evidence/<target>.out` (the scheduler
      output) and `evidence/<target>.check.log` (the run directory's `check.log`) for the three
      targets; `evidence/jobs.tsv`, a header `label job_id state exit_code` and one tab-separated
      line per job from `sacct`. Submission times and options go in the phase commit's body, with no
      account, QoS or cluster names.
- **Built 2026-10-12.** Audit: R1–R9 each have checks of their own in `tests/check_cli.cpp`,
  `tests/check_math.cpp` and `tests/cli.sh`, one condition per `expect`, and no file under `tests/`
  calls the defects known; no code or test changed, so the tested commit is P3's `8b6d8b1`. R11:
  `issues/verdicts-and-exit-statuses.md`, its entry directly after this cycle's. Ledger: no
  deferral in P1–P4 besides R11's; 28 seeds, 28 `**Seed:**` lines. Cluster runs of `8b6d8b1`,
  each `COMPLETED 0:0`, `make check` 20 + 121 with GCC 14: job 16965684 (`x86-64-avx512`),
  44166569 (`x86-64-avx2`), 474567 (`aarch64-gb10`); one submission each, none refused or resubmitted.
- **Rerun 2026-10-12, after review cycle 1.** P1's F1 fix (1ed28d7) and P3's F2 fix (439f35e)
  changed the code, so the runs above no longer cover `HEAD`. The tested commit is now P3's
  `439f35e`, pushed as `fix/cli-and-sieve-fixes` after the site-private scan, which was first seen
  to catch a planted site name. Cluster runs of `439f35e`, each `COMPLETED 0:0`, `make check`
  20 + 126 with GCC 14: job 16966256 (`x86-64-avx512`), 44169091 (`x86-64-avx2`), 474571
  (`aarch64-gb10`); one submission each, none refused or resubmitted. The evidence and `jobs.tsv`
  are replaced; the seed, its entry and the ledger are unchanged. The gate, with both remediation
  gates, was green on its first run.
- **Verify:** `sh spec/cli-and-sieve-fixes/verify/p4.sh` — all of P3's gate on the final tree; the
  R11 seed (`status: unshaped`, the follow-up scope) and its entry directly after this cycle's; the
  ledger; three `result.json` files that pass the job's format check, every step passed, GCC 13 or
  newer, the target's architecture, `<cluster>` and `<host>`, distinct job ids matching `jobs.tsv`'s
  `COMPLETED 0:0`; one tested commit, on `origin/fix/cli-and-sieve-fixes`, containing `origin/main`,
  whose code equals `HEAD`'s outside `spec/`, `issues/` and `ROADMAP.md`, and whose
  `ci/test-commit.sh` is the script each run printed; each check log's summaries equal to this
  tree's counts; the site-private scan of the cycle's files and history.
- **Touches:** `issues/verdicts-and-exit-statuses.md`, `ROADMAP.md`,
  `spec/cli-and-sieve-fixes/evidence/*`; `tests/*` only if the audit finds a gap.

---

## How `code-build` drives this

1. `next_phase.py` prints the next actionable phase.
2. Pre-flight: the cycle worktree is clean, on `branch`, and `base` is reachable.
3. Execute every `[ ]` in the phase (consult `PLAN.md` / `research/` for detail).
4. Run the phase's `verify:` command — never advance on a checkbox alone.
5. Amend this file freely if reality diverges (regenerate frontmatter with `set_phase.py`; note the
   amendment in the commit body). STOP and escalate only on a **`GOAL.md` contradiction** or a gate
   that is wrong.
6. Mark the phase `done`, advance `current_phase`, `--touch`; one `[fix]` commit; stop and report.
