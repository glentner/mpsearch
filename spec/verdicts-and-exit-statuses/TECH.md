---
slug: verdicts-and-exit-statuses
title: 'Say what was found: a result type, one verdict line, exit statuses and --json'
kind: feature
appetite: small
status: done
branch: feature/verdicts-and-exit-statuses
base: main
current_phase: done
last_updated: '2026-10-12'
phases:
- id: P1
  name: Results, the verdict line, the record and the statuses
  status: done
  satisfies:
  - R1
  - R2
  - R6
  - R7
  - R8
  - R9
  - R10
  - R12
  - R13
  depends_on: []
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/verdicts-and-exit-statuses/verify/p1.sh && sh spec/verdicts-and-exit-statuses/remediation/f1.sh
- id: P2
  name: The exponent check, and -s honoured
  status: done
  satisfies:
  - R3
  - R4
  - R5
  - R13
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/verdicts-and-exit-statuses/verify/p2.sh && sh spec/verdicts-and-exit-statuses/remediation/f1.sh
    && sh spec/verdicts-and-exit-statuses/remediation/f2.sh
- id: P3
  name: -j/--json, the help and the status table
  status: done
  satisfies:
  - R10
  - R11
  - R13
  depends_on:
  - P2
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/verdicts-and-exit-statuses/verify/p3.sh
- id: P4
  name: Check audit, deferral ledger, cluster runs
  status: done
  satisfies:
  - R13
  depends_on:
  - P3
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/verdicts-and-exit-statuses/verify/p4.sh && sh spec/verdicts-and-exit-statuses/remediation/f1.sh
    && sh spec/verdicts-and-exit-statuses/remediation/f2.sh
review:
  last_reviewed_commit: 4be6c3a16490caa7323e34d584fa2a1cc383fcb1
  verdict: approved
  blocked_reason: ''
  cycle: 2
---
# TECH.md — Say what was found: a result type, one verdict line, exit statuses and `--json`

The **context engine and finite-state machine** for building this feature. The YAML frontmatter
above is the resume ground-truth (read it from the cycle worktree with
`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/verdicts-and-exit-statuses/TECH.md`);
the per-phase checklists below are the work. `code-build` executes the next actionable phase, runs
its `verify:` command, updates state via
`uv run --with pyyaml python "$KB/bin/factory/set_phase.py" …`, and makes one atomic code+state
commit.

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
- `parallel`: `false` throughout: every phase touches the coupled core (the command-line surface or
  the test entry points) or follows one that does.
- `hammerable`: `false` throughout: each phase is a correctness phase, and R13 is a gate on gates.
- `hill`: `uphill` → `crest` → `downhill`.
- `attempts`: durable failed-verify counter, bumped by `set_phase.py --phase P<n> --record-attempt`.
- `verify`: the exact command that proves the phase.
- `review.cycle`: completed review passes.

## Conventions (apply to every phase)

- `AGENTS.md` is the constitution; walk the § Invariants sections PLAN § 3 lists. `src/math.hpp` and
  `src/math.cpp` are high-blast-radius paths, and from P1 so are `src/result.*` and `src/main.cpp`.
  § 1 (Verdicts are earned) is a human gate: keep its guarantees as PLAN § 2 builds them, so a
  reviewer sees them at a glance (a composite only through `composite_by_exponent`'s re-check; no
  verdict from a stub or an error; verdicts held until `report_run` knows the run is sound).
- One phase per `code-build` invocation; one atomic commit with the code and the `TECH.md` state,
  subject `[feature] Build verdicts-and-exit-statuses P<n>: …`, no trailers.
- Same-commit rules: `AGENTS.md`'s R12 changes in P1's commit, with `src/result.*`; the parser
  (`src/cli.*`), the help and the usage (`src/main.hpp`) in P3's commit, every help line included
  (R11); README has no usage (GOAL non-goal). The record carries `record_version` from its first
  commit. No R#/P# ids in `src/` or `tests/`.
- **The gates are fixed.** `spec/verdicts-and-exit-statuses/verify/` is reviewed as of the plan
  commit; each gate refuses to run if it differs (PLAN § 6). If a gate is wrong, stop and escalate
  rather than edit it.
- The gates run on macOS and need clang++, g++-16, GNU make 3.81 at `/usr/bin/make`, git, rsync, awk
  and python3; P4 adds the site file. Each prints `GATE FAIL: <reason> (R-ID)` and exits 1 on the
  first failure, or ends with a `Pn gate: green (…)` line. Each is cumulative. Run a gate after the
  phase's changes are in the working tree; it reads uncommitted files as a clone would hold them.
- **What the gates read of the tree** (PLAN § 5): the line, record, help and usage text of PLAN § 2;
  the names and signatures of PLAN § 2; each wrapped function defined out of line, at file scope,
  with its name on the definition's first line, and the call graph PLAN § 2 gives (the printer
  through `verdict_line`, `json_record` and `status_of`; `run_in_order` through `is_decisive`;
  `test_all` through `run_in_order`; `check_exponent` through `smallest_prime_factor` and
  `composite_by_exponent`); `cli::Interface::json_mode` and `skip_ptest`; the two `AGENTS.md` list
  lines exactly. The checks drive `report_result`, `report_run` and `status_of`; `tests/cli.sh`
  passes the environment through to `bin/mps` (unsetting named variables is fine; `env -i` is not).
- **Every check is one condition.** In `tests/cli.sh`, `expect NAME [ a ] && [ b ]` counts only
  `[ a ]`; write two `expect` lines. Keep `planned` exact. Statuses are compared as literal numbers.
- **No `logging::debug`** in `src/math.cpp` or `src/result.cpp`: their logger copies sit at `DEBUG`
  (`issues/logger-state-per-translation-unit.md`), so it would always print.

---

## Phase P1 — Results, the verdict line, the record and the statuses
**Satisfies:** R1, R2, R6, R7, R8, R9, R10 (the record), R12, R13 (their checks) · **Depends on:** —
**Goal:** every test returns a `Result`; `mps PVAL` prints one no-verdict line per test as it
completes and exits 42; the record, the statuses and the printer's failures exist and are checked on
constructed results; `AGENTS.md` names the new format and its paths.

- [x] `src/result.hpp`, `src/result.cpp` (new module): `Outcome`, `Worktype`, `name_by_worktype`,
      `class Result` with its six named constructors and accessors, `RECORD_VERSION`,
      `is_decisive`, `verdict_line`, `json_record` (with the escaper; empty for an error),
      `status_of` (a `switch`, never a map), `report_result`, `report_run`, as PLAN § 2 gives them.
      SPDX header, doc blocks, no `ansi.hpp`. `json_record`'s doc block names the record's format,
      `record_version`, and `2^a-1` as the factor form.
- [x] `src/main.hpp`: `enum Status` gains `COMPOSITE = 40`, `PROBABLE_PRIME = 41`,
      `NO_VERDICT = 42`; nothing else in the file changes in P1 (the help moves in P3).
- [x] `src/math.hpp`, `src/math.cpp`: the three stubs return `Result::no_verdict` with PLAN's
      reasons, their start lines unchanged; `TestFunction`, `ResultSink`, `run_in_order`, `test_all`.
- [x] `src/main.cpp`: after the `-p` return, the dispatch feeds every result to `report_result` and
      returns `report_run(run, false, std::cout)` (P3 passes `opt.json_mode`).
- [x] `Makefile`: `result` joins `MODULES`.
- [x] `AGENTS.md`, in this commit (R12): the two sentences reworded; the high-blast-radius list gains
      the two lines PLAN § 2 gives; § Map gains `src/result.*`.
- [x] `tests/check_result.cpp` (new) and `tests/check_math.cpp` (the stubs): PLAN § 2 "The checks",
      its P1 parts, the JSON validator and its own rejection check included.
- [x] `tests/cli.sh`: every `exits 0` for a non-`-p` PVAL becomes 42 (lines 177, 182, 187, 216 and
      the 233–241 loop, where `0000000031` exits 42; `999999999` keeps running the tests until P2 and
      is pinned there); each prime exponent's three lines exactly; each `-t` name's line; R12 read from
      `AGENTS.md`; no ESC on stdout under `FORCE_COLOR=1`; `planned` updated.
- **Reopened 2026-10-12 for review cycle 1, F1** (a hostile reason breaks the verdict line or the
  record). The class is every text a result carries into a line or a record, not only the reason
  the review drove:
  - [x] `src/result.cpp`: `verdict_line` puts the reason, for a no verdict and for an error's
        stderr line alike, through `line_text`, which writes each byte below 0x20, and DEL, as
        `\x` and two lowercase hex digits and leaves every other byte as it is.
  - [x] `src/result.cpp`: `json_record` puts every string through `json_text`, which replaces
        `json_string`: the same escapes, well-formed UTF-8 unchanged, and U+FFFD for each byte
        that begins or continues no well-formed sequence (RFC 3629's table, in a `utf8_length`
        helper). `RECORD_VERSION` stays 1, and the doc block says why. `src/result.hpp` and the
        rest of `src/` are unchanged.
  - [x] `tests/check_result.cpp`: four checks drive hostile reasons through `verdict_line` and
        `json_record`: newline, CR, tab, ESC, NUL, DEL and 0x1f escaped exactly on both lines; the
        control bytes reading back from the record; well-formed UTF-8 exact on the line and in the
        record; each kind of malformed UTF-8 (0xff, a lone continuation, a cut-short sequence, a
        lead before ASCII, an overlong form, a surrogate, beyond U+10FFFF) giving U+FFFD per bad
        byte, in a record the checks' own UTF-8 reader accepts. `check:` goes from 44 to 48.
- **Verify:** `sh spec/verdicts-and-exit-statuses/verify/p1.sh` — `make -B check` green on clang++
  and g++-16 without warnings; PLAN § 6's P1 drives, probe and `AGENTS.md` check; `make check` red
  under each P1 mutant and edit (about 64); the R13 sweep; no spec ids. Since the reopen, then
  `sh spec/verdicts-and-exit-statuses/remediation/f1.sh`: hostile reasons on both compilers, and
  `make check` red with either fix's body swapped out.
- **Touches:** `src/result.hpp`, `src/result.cpp`, `src/main.hpp`, `src/main.cpp`, `src/math.hpp`,
  `src/math.cpp`, `Makefile`, `AGENTS.md`, `tests/check_result.cpp`, `tests/check_math.cpp`,
  `tests/cli.sh`.

## Phase P2 — The exponent check, and -s honoured
**Satisfies:** R3, R4, R5, R13 (their checks) · **Depends on:** P1
**Goal:** a composite PVAL is answered before any test by one line naming 2^a − 1, a its smallest
prime factor, re-checked exactly, exit 40, under every `-t` name; a prime PVAL prints nothing for the
check; `-s/--skip-ptest` runs the tests on any PVAL.

- [x] `src/math.hpp`, `src/math.cpp`: `smallest_prime_factor`, `composite_by_exponent` (the guard
      `1 < a and a < p and p % a == 0`, in that order, and the proof in its doc block),
      `check_exponent` returning `std::optional<Result>`, as PLAN § 2 gives them. Trial division cited
      to Crandall and Pomerance, 2005, ch. 3. Nothing logged.
- [x] `src/main.cpp`: unless `opt.skip_ptest`, `check_exponent(pval)` before the `-t` switch; a
      result is the run's only result.
- [x] `tests/check_math.cpp`: the `smallest_prime_factor` sweep and known-answer table, each row with
      its recomputing command; `check_exponent`; `composite_by_exponent`'s refusals and (33, 11).
- [x] `tests/cli.sh`: R3's four exponents (and 9) under no `-t` and all seven names: the line, exit
      40, no start line; R4's four primes; `-s` and `--skip-ptest` on composites and a prime;
      `999999999` now exits 40 and runs no test; `planned` updated.
- [x] The help is untouched here: the `-s` wording moves with the parser in P3 (R11).
- **Reopened 2026-10-12 for review cycle 1, F2 and F3** (the map still called `src/math.*` the
  sieve and the stub tests; `make check` did not pin that the exponent check goes through its
  re-check):
  - [x] F3, `src/math.hpp`: `using FactorSearch = long unsigned (*)(const long unsigned p);` and
        `check_exponent_by(p, search)`, declared beside `check_exponent`.
  - [x] F3, `src/math.cpp`: `check_exponent_by` holds the check's body (`a = search(p)`; no
        result when `a == p`; otherwise `return composite_by_exponent(p, a);`, the one such line
        in the file), its doc block saying a search can be wrong and the re-check stands between
        it and a verdict. `check_exponent(p)` keeps its signature and doc block, defined in its
        own right, and returns `check_exponent_by(p, smallest_prime_factor)`, so the frozen
        gates' wrapped search still reaches it by that symbol.
  - [x] F3, `tests/check_math.cpp`: `the_exponent_check_re_checks_its_search`, with four `static`
        searches that lie about 33: 0, 1 and 5 each give an error with no factor and nothing
        decisive; 11 gives `2^11-1`; under `smallest_prime_factor`, every known answer agrees with
        `check_exponent`. `check:` goes from 48 to 49.
  - [x] F2, `AGENTS.md` § Map: the `src/math.*` line names the exponent check, its re-check
        `composite_by_exponent`, the stub tests and `run_in_order`.
- **Verify:** `sh spec/verdicts-and-exit-statuses/verify/p2.sh` — all of P1's gate, plus PLAN § 6's
  P2 drives (the 2,000-exponent sweep included), probe and the re-check drive; `make check` red under
  each exponent-check mutant (about 85 in all). Since the reopen, then both remediation gates:
  `remediation/f1.sh`, and `remediation/f2.sh` (the map line; `check_exponent_by` against lying
  searches on clang++ and g++-16; `make check` red with the re-check bypassed).
- **Touches:** `src/math.hpp`, `src/math.cpp`, `src/main.cpp`, `tests/check_math.cpp`,
  `tests/cli.sh`; since the reopen, `AGENTS.md`.

## Phase P3 — -j/--json, the help and the status table
**Satisfies:** R10, R11, R13 (their checks) · **Depends on:** P2
**Goal:** `-j/--json` prints one JSON record per result in place of each line, with the same status
and stderr, and groups like any flag; `--help` and the usage say exactly what PLAN § 2 gives, in the
same commit as the parser.

- [x] `src/cli.hpp`, `src/cli.cpp`: `{"--json", 'j'}`, `bool json_mode = false;`, `case 'j'` with
      its duplicate warning.
- [x] `src/main.hpp`, in this commit (R11): `APP_USAGE` and `APP_HELP` exactly as PLAN § 2 gives
      them (`[-j]`, the `-s` wording, the `-j` line, the `Exit status:` table).
- [x] `src/main.cpp`: `opt.json_mode` reaches `report_result` and `report_run`.
- [x] `src/ansi.cpp`: `colorize_usage`'s header pattern `^[A-Za-z][A-Za-z ]*:` (PLAN § 3, deviation 1).
- [x] `tests/check_cli.cpp`: `json_mode` from `-j`, `--json` and groups; `--json=1` rejected; the
      `describe` helper reports `json_mode`.
- [x] `tests/cli.sh`: `-j` records byte-exact for P1's and P2's runs with the same statuses and
      stderr; `-jd`, `-sj`; the help's `-j` line, `-s` wording and every status row; `mps` alone
      prints the usage exactly; no ESC in records under `FORCE_COLOR=1`; `planned` updated.
- **Verify:** `sh spec/verdicts-and-exit-statuses/verify/p3.sh` — all of P2's gate, plus PLAN § 6's
  P3 drives and the same-commit check; `make check` red under the flag mutants and the help edits
  (about 93 in all).
- **Touches:** `src/cli.hpp`, `src/cli.cpp`, `src/main.hpp`, `src/main.cpp`, `src/ansi.cpp`,
  `tests/check_cli.cpp`, `tests/cli.sh`.

## Phase P4 — Check audit, deferral ledger, cluster runs
**Satisfies:** R13 (the audit) · **Depends on:** P3
**Goal:** the final tree passes every gate at once; every deferral has a home; the pushed head
passes `make check` on the three cluster targets, recorded as redacted evidence.

**Graded by the orchestrator** for the cluster runs: the evidence lives in
`spec/verdicts-and-exit-statuses/evidence/`, which the blind reviewer cannot see; the orchestrator
grades it from the job ids, `sacct` and the session's MCP transcript.

**Order matters.** The tested commit's code must equal `HEAD`'s outside `spec/`, `issues/` and
`ROADMAP.md`. So any code the audit changes is committed first, as
`[feature] Build verdicts-and-exit-statuses P4: <what>` with the phase still `in_progress`; then
the pre-push checks, the push and the runs, and the phase commit with the evidence and the state.

- [x] Audit: walk R1–R12 against `tests/` and confirm each has a check of its own, one condition per
      `expect`, statuses as literal numbers. Fix and commit anything found (above).
- [x] Deferral ledger: walk P1–P4 of this file, and the build's own commits, for "do not fix here",
      "known limitation", "left as a follow-up" and any deferral the build introduced; each needs an
      `issues/<slug>.md` from the ISSUE template and a `ROADMAP.md` entry. None is planned. Every
      `issues/*.md` matches exactly one `**Seed:**` line; this cycle's seed stays
      `adopted:verdicts-and-exit-statuses`.
- [x] Pre-push: `sh spec/verdicts-and-exit-statuses/verify/prepush.sh` (the P3 gate on the final
      tree, the ledger, no uncommitted code, the site-private scan of files and history) must be
      green. `prepush.sh scan` runs the scan alone; see it catch a planted site name once, then
      remove the plant.
- [x] Push: `git push origin feature/verdicts-and-exit-statuses`, fast-forward only (authorized by the
      foreman for this phase). Never force.
- [x] Submit the pushed head on `x86-64-avx512`, `x86-64-avx2` and `aarch64-gb10` through the cluster
      MCP server, with the site recipe the foreman hands over (accounts, QoS, paths; never committed):
      write the job from the pushed commit's `ci/test-commit.sh` to `<scratch>/mpsearch/jobs/` without
      its final newline and check its `git hash-object` before `sbatch`; `sbatch` with
      `script_path="<file> <hash> <target>"`, the target file's resources and `output` in that
      directory (cycle 3's recipe, `ci/test-commit.sh`'s header). Limits: ≤ 30 min, ≤ 8 cores,
      ≤ 16 GB, at most two queued per cluster (one on the GB10 cluster), output only under
      `<scratch>/mpsearch/`. Resubmit a run that fails for a reason outside the tree (preemption, a
      node fault); record only runs that test the tree.
- [x] Evidence, each piece through `sh spec/verdicts-and-exit-statuses/verify/redact.sh IN OUT`:
      `evidence/<target>.json` (the run's `result.json`), `evidence/<target>.out` (the scheduler
      output) and `evidence/<target>.check.log` (the run directory's `check.log`) for the three
      targets; `evidence/jobs.tsv`, a header `label job_id state exit_code` and one tab-separated
      line per job from `sacct`. Submission times and options go in the phase commit's body, with no
      account, QoS or cluster names.
- **Built 2026-10-12.** Audit: one gap, R8's error classes keeping their values (`ERR_CFG` and
  `ERR_FILE` are produced by no run, so moving either left `make check` green);
  `the_status_table_keeps_its_values` in `tests/check_result.cpp` pins all nine `enum Status`
  values, committed in 7ff16a8 and seen red once with `ERR_CFG` moved. Ledger: no deferral.
  `prepush.sh` green; the scan caught a planted site name once. Pushed 7ff16a8 and tested it:
  Slurm jobs 16968730 (`x86-64-avx512`), 44172871 (`x86-64-avx2`) and 474589 (`aarch64-gb10`),
  each `COMPLETED 0:0`, every step passed, `check: 44` and `cli: 530`, GCC 14.1, 14.4 and 14.2.
- **Rerun 2026-10-12, after review cycle 1.** F1 (e88f30b) and F2–F3 (22911ab) changed the code,
  so the runs above no longer cover `HEAD`; they stay as history. `prepush.sh` green; the scan
  caught a planted site name once. Pushed 22911ab (fast-forward from df9bd39) and tested it, with
  the job script's blob unchanged: Slurm jobs 16969378 (`x86-64-avx512`), 44179712
  (`x86-64-avx2`) and 474595 (`aarch64-gb10`), each `COMPLETED 0:0`, every step passed,
  `check: 49` and `cli: 530`, GCC 14.1, 14.4 and 14.2. The evidence is replaced; the gate now
  runs both remediation gates after `verify/p4.sh`.
- **Verify:** `sh spec/verdicts-and-exit-statuses/verify/p4.sh` — everything `prepush.sh` checks
  but the uncommitted-code rule, plus the evidence of PLAN § 6: three passed runs with GCC 13 or
  newer and the committed script, on one pushed commit whose code equals `HEAD`'s outside `spec/`,
  `issues/` and `ROADMAP.md`; check logs whose summaries equal this tree's counts; `sacct` agreeing;
  the scan covering the evidence.
- **Touches:** `spec/verdicts-and-exit-statuses/evidence/*`; `tests/*` only if the audit finds a
  gap; `issues/`, `ROADMAP.md` only for a deferral the build introduced.

---

## How `code-build` drives this

1. `next_phase.py` prints the next actionable phase.
2. Pre-flight: the cycle worktree is clean, on `branch`, and `base` is reachable.
3. Execute every `[ ]` in the phase (consult `PLAN.md` / `research/` for detail).
4. Run the phase's `verify:` command — never advance on a checkbox alone.
5. Amend this file freely if reality diverges (regenerate frontmatter with `set_phase.py`; note the
   amendment in the commit body). STOP and escalate only on a **`GOAL.md` contradiction** or a gate
   that is wrong.
6. Mark the phase `done`, advance `current_phase`, `--touch`; one `[feature]` commit; stop and
   report.
