# REVIEW — One logger state for the whole program

> Adversarial QA by `code-review`, run in an isolated/clean context (a subagent; where the runtime
> has none, `code-review`'s sequential fallback). The correctness pass grades the branch diff against
> [`GOAL.md`](GOAL.md) + the repo's `AGENTS.md` § Invariants **only** — it does not see
> `PLAN.md`/`TECH.md` (avoids grading-its-own-homework / plan-sycophancy). Every finding cites an
> **executed** command, not an assertion.

- **Reviewed commit:** 33309c19878c55035d9630fc2e097229ee679adf  ·  **Base:** main (`origin/main` 9a41271)  ·  **Date:** 2026-10-12
- **Verdict:** changes-requested
- **Cycle:** 1 of ≤3 — mirrors `review.cycle` in `TECH.md` (escalate to human on non-convergence)

**Mode: one fresh reviewer at xhigh,** blind to everything under `spec/` but GOAL.md, building and mutating in its own clone.
- The diff touches the coupled core only through two lines in `src/cli.cpp`'s `check_flag`, and the rubric keeps the debate variant for high-risk changes. So the orchestrator ran one reviewer.
- The reviewer was told the rubric's Scope bar: a surviving mutant is a finding only when it breaks an R-ID's stated check or behaviour `bin/mps` can reach.

## Verification run

- **The profile's verify.** `make -B check` with clang++ and with `CXX=g++-16` gives `check: 55 passed, 0 failed` and `cli: 549 passed, 0 failed`, with no warnings.
- **R6.** 732 runs of `bin/mps` (122 invocations × 6 environments: pipe, NO_COLOR, FORCE_COLOR, both, `TASK_ID=abc`, `TASK_ID=""`), each from an empty directory, match `9a41271` byte for byte in stdout, stderr and status. 424 of them have coloured stderr. Eight runs under a real tty match too.
- **R1–R3.** 59 `-D` cases (long and short forms, groups, repeats, after `--`, as a value) have the detailed or default style the left-to-right rule gives. `TASK_ID` set, unset and empty each stamp as R3 says.
- **R5.** `nm -C bin/mps` shows exactly one symbol each for the five state variables.
- **Leakage.** `make check`'s output, under clang, gcc, and `TASK_ID=zz FORCE_COLOR=1`, holds no ESC byte, date stamp, host name or task stamp. A runner probing the state after every check found no change after any of the 55.
- **Mutation, in the reviewer's clone.** Mutations of the state's storage, the parser's application, the task and host stamps, and the default style all turn `make check` red, except F1 below and the observations.
- **Orchestrator** (out of the reviewer's scope):
  - **The cluster evidence:** three jobs, one per target, on `5f37892`, whose code equals `33309c1`'s outside `spec/`, `issues/` and `ROADMAP.md`.
    - Each is `COMPLETED 0:0`, with all five steps passed and check 55/0, cli 549/0.
    - No check log holds a detailed line or a node name.
  - The orchestrator re-ran P1–P3's gates and got green. `verify/` is unchanged since the plan commit `edc7409`.

## Requirement → evidence matrix

| R-ID | Implemented by (file/commit) | Verified how | Status |
|------|------------------------------|--------------|--------|
| R1   | `inline` state (5f53359); `check_flag` applies `-D` (5f37892) | the detailed cases above; the per-file mutants are killed | ✅ |
| R2   | the parser's order (5f37892) | the default-style cases before `-D`; the pre-scan and value mutants are killed | ✅ behaviour · ❌ `-D` after `--` unpinned (F1) |
| R3   | the shared task id and host (5f53359) | `TASK_ID` set, unset and empty; four stamp mutants killed | ✅ |
| R4   | the shared level (5f53359, 5f37892) | the cross-unit level check; three mutants killed | ✅ |
| R5   | `inline` state (5f53359) | `nm`; the address check | ✅ |
| R6   | default style unchanged | 732 + 8 runs identical to `9a41271` | ✅ |
| R7   | `tests/` | both compilers green; the old caveat removed; no spec ids | ✅ |

**Unmapped changes:** the AGENTS.md map line for `tests/log_peer.*`, the seed's `adopted:` status, and test refactors. Nothing substantive.

## Findings

Severity: **CRITICAL** (a violation of any numbered section of § Invariants is auto-CRITICAL,
**including lettered subsections**; a § Conventions violation is **HIGH**) · **HIGH** · **MEDIUM** ·
**LOW**. Verdict: **CONFIRMED** (reproduced) vs **PLAUSIBLE** (suspected, needs human triage). Only
CONFIRMED findings auto-loop to `code-build`.

### [LOW/CONFIRMED] F1 — no check pins a `-D` after `--` to the default style
- **Where:** `src/cli.cpp:54-55`, the `positional_only` branch. `tests/cli.sh`'s left-to-right block doesn't cover `--`.
- **Failure scenario:**
  - Make the parser apply a `-D` it reads after `--`, by inserting `if (opt == "-D") logging::set_style(logging::DETAILED);` before `positional.push_back(opt);` in that branch.
  - `make -B check` stays green at 55/0 and 549/0.
  - `bin/mps` changes as a result: `TASK_ID=abc NO_COLOR=1 mps 31 -- -D` prints its "Unexpected positional argument: -D" error with the detailed stamp, where the head prints it plain.
  - After `--`, `-D` is a positional word, never the flag (R2 and the left-to-right rule).
- **Evidence:** the reviewer's mutant and the two runs above. The shipped code is right: `-- -D 31`, `31 -- -D` and `-- -D` all print in the default style.
- **Touches:** R2, R7. Not human-gated: `src/cli.cpp` and `tests/` are off the high-blast-radius list.

**Observations, not findings** (no R-ID's stated check or reachable behaviour breaks):
- **The `-d` half of the left-to-right rule.** A `-d` pre-scan and a `-d` taken from a value (`-f -d`) survive. `bin/mps` emits no debug message, so neither is reachable. R4's stated cross-unit check holds.
- **The end-of-run guard.** "the logger's state is as the run began" can be masked when `SavedLogger` stops restoring the style, because later checks reset it. A per-check probe found nothing at head.
- **Equivalent mutants:** `logging::init()` setting DEBUG, and `main.cpp`'s now-redundant calls after parsing removed.
- **Pre-existing:** with colour on and `TASK_ID` empty, the detailed style emits an empty `\e[2m\e[0m` segment.
- **Link order.** `log_peer.cpp`'s "runs last" relies on static-initialisation order, which the standard leaves unspecified. It holds under both compilers.

## Human-gate triggers

- **Not triggered.** F1 lies in `src/cli.cpp` and `tests/`, and no `[human gate]` invariant is touched. The gates hold `src/main.cpp`, `src/math.*` and `src/result.*` byte-equal to `9a41271`.

## Remediation (orchestrator's routing)

- **F1 → P2 reopened.**
  - `make check` pins, through `bin/mps`, that a `-D` after `--` changes nothing: `mps 31 -- -D`, `mps -- -D 31` and `mps -- -D` print in the default style, byte-equal to `9a41271`. One condition per `expect`.
  - Gate: `verify/p2.sh`, then `remediation/f1.sh`, which inserts the reviewer's mutant and requires `tests/cli.sh` itself to go red.
- **P3 reopened,** because the tests change. The cluster runs are redone on the new pushed head.

## Optional completeness sub-pass (separate reviewer; may see TECH.md)

- Not run.

## Review cycle 2 — escalated to Geoffrey (2026-10-12)

- **Reviewed commit:** 85409a8fde03dc4ce95a67504d9b10f89950e811. The remediation is `1d75723`, which adds three `tests/cli.sh` expectations, and `85409a8`, the P3 reruns.
- **Mode:** scoped to F1 and regressions. One fresh reviewer ran at xhigh, blind to everything under `spec/` but GOAL.md.
- **Status:** escalated. The review didn't converge on its second cycle: one further CONFIRMED LOW finding, the sibling of F1.

### Verification run

- **F1, verified for `-D` in every variant.** Each of these turns `make check` red, with `tests/cli.sh` itself failing:
  - the reviewer's mutant;
  - `-D` or `--detailed` handled in the `positional_only` branch;
  - any word after `--`;
  - `check_flag` reached after `--`;
  - a pre-scan at `--`, or of the words after it;
  - a deferred application.
- **The new expectations hold `9a41271`'s bytes.** `planned` is exact at 552, each expectation tests one condition, and `make check`'s output leaks no stamp or host name.
- **`make -B check`:** green on clang++ and g++-16 at 55/0 and 552/0, with no warnings.
- **Orchestrator:** three cluster jobs, one per target, on `1d75723`, whose code equals `85409a8`'s. Each is `COMPLETED 0:0` with check 55/0 and cli 552/0. P3's gate and `remediation/f1.sh` were re-run green.

### Findings

- **[LOW/CONFIRMED] F2 — the fix pins `-D` after `--`, but not `--detailed`.**
  - **Where:** `tests/cli.sh:321-329`; the mutation site is `src/cli.cpp:54`.
  - **Mutants that survive:**
    - M2 applies `--detailed` read after `--`;
    - M6 still parses long options after `--`.
  - Under either, `make -B check` stays green, and `TASK_ID=abc NO_COLOR=1 mps 31 -- --detailed` prints its error with the detailed stamp. The head prints it plain.
  - **Touches:** R2, R7.
  - **Cause:** the orchestrator's F1 routing named the instance (`-D`), not the class: "every flag after `--` is a positional word".
- **Observation (pre-existing, outside the cycle except for `--detailed`):** M6 also survives at `9a41271`. No check anywhere pins that long options after `--` are positional words, so `31 -- --json` and `-- --help` are unpinned too. That gap dates from `cli-and-sieve-fixes`, where `--` began ending the options.
- **Observations, not findings:** two mutants can't show through `bin/mps` and stay green:
  - `-d`/`--debug` setting the level after `--`, because no debug message exists;
  - `detailed_mode` set after `--`, because every such run ends in an argument error before `main.cpp` reads it.

### Human-gate triggers

- **Not triggered:** F2 lies in `tests/` and `src/cli.cpp`. The review loop reached its second cycle without converging, so the orchestrator escalated to Geoffrey.

### Geoffrey's decision (2026-10-12)

- **"One last fix round."** F2 is remediated as a class: every flag after `--`, short or long, valued or not, is a positional word, and `make check` pins it through `bin/mps`.
  - That covers the pre-existing gap where long options after `--` (`--json`, `--help`) were unpinned.
  - The next review is cycle 3, the last the loop allows.

## Review cycle 3 — escalated to Geoffrey (2026-10-12)

- **Reviewed commit:** 854ed03524619f03332e206a4b862310f62a6c28. The remediation is `a9500c1` (a 23-word loop after `--`, in two positions) and `854ed03` (the P3 reruns).
- **Mode:** scoped to F2 and regressions. One fresh reviewer ran at xhigh, blind to everything under `spec/` but GOAL.md.
- **Status:** escalated. Review cycle 3 is the loop's bound, and it found two further CONFIRMED LOW findings, siblings of F2.

### Verification run

- **F2, verified for the first word after `--`.** Each of these turns `make check` red, with `tests/cli.sh` itself failing:
  - M2, in both forms;
  - `-D`;
  - M6, long options;
  - short options;
  - a dropped `else`;
  - valued options;
  - `--help`, `--version`, `-h` and `-v` acting;
  - a second `--` consumed;
  - the `--` branch reordered;
  - groups;
  - pre-scans;
  - `-p`;
  - `-D` after a second positional.
- **The new expectations hold `9a41271`'s bytes:** all 92 pass against its `bin/mps`. `planned` is exact at 644, each expectation tests one condition, no variable clobbers `name`, and `make check` leaks no stamp or host name.
- **`make -B check`:** green on clang++ and g++-16 at 55/0 and 644/0, with no warnings.
- **Orchestrator:** three cluster jobs, one per target, on `a9500c1`, whose code equals `854ed03`'s. Each is `COMPLETED 0:0` with check 55/0 and cli 644/0. P3's gate and both remediation gates were re-run green.

### Findings

- **[LOW/CONFIRMED] FA — the class is closed only for the first word after `--`** (`tests/cli.sh:335-345`; the mutation site is `src/cli.cpp:54-56`). Two parser mutants survive `make -B check`:
  - **M13,** where `--` covers only the next word: `mps -- 31 --help` prints the help and exits 0; `-- 31 -j` and `-- 31 -D` act too.
  - **M12,** where a second `--` stays positional but turns options back on: `mps 31 -- -- --help` prints the help.

  `9a41271` and the head both answer every case with exit 1 and a plain "Unexpected positional argument". Touches R2 and R7.
- **[LOW/CONFIRMED, contrived] FB — no check gives a flag both before and after `--`.**
  - Mutants that keep the word positional but also apply a quiet flag (`-j`, `-s`) survive.
  - `mps -j 31 -- -j` then gains a "Duplicate use of -j/--json" warning that `9a41271` doesn't print (R6's "none that doesn't SHALL appear").
  - Each such mutant needs code written for that one flag.

### Human-gate triggers

- **Not triggered:** both findings are in `tests/` and `src/cli.cpp`.
- The review loop reached its bound, three cycles, without converging, so the orchestrator escalated to Geoffrey.

### Geoffrey's decision (2026-10-12)

- **"Land now, seed a follow-up."**
  - FA and FB are accepted as residual coverage gaps. The behaviour is correct, verified against `9a41271`'s bytes.
  - They are seeded as `issues/end-of-options-checks.md`, with its `ROADMAP.md` entry at the end of Part I (6988a3e). The seed asks for one check, generated from the parser's flag list, that pins `--` for every flag in every position.
- **Verdict: approved** by Geoffrey's decision at the loop's bound. No human gate was triggered in any cycle.
