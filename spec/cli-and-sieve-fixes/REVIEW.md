# REVIEW — Options that parse as documented, and a sieve right at its edges

> Adversarial QA by `code-review`, run in an isolated/clean context (a subagent; where the runtime
> has none, `code-review`'s sequential fallback). The correctness pass grades the branch diff against
> [`GOAL.md`](GOAL.md) + the repo's `AGENTS.md` § Invariants **only** — it does not see
> `PLAN.md`/`TECH.md` (avoids grading-its-own-homework / plan-sycophancy). Every finding cites an
> **executed** command, not an assertion.

- **Reviewed commit:** c84bbecca02b4e9b26f40ee3e15584276254c974  ·  **Base:** main (`origin/main` d179aa4)  ·  **Date:** 2026-10-12
- **Verdict:** changes-requested
- **Cycle:** 1 of ≤3 — mirrors `review.cycle` in `TECH.md` (escalate to human on non-convergence)

**Mode: the debate variant.** The diff touches the coupled core: the command-line surface (`src/cli.*`, the help in `src/main.hpp`) and the test entry points (the dispatch in `src/main.cpp`, `src/math.cpp`).
- Two fresh reviewers ran at xhigh, one arguing ship and one arguing block.
- Each saw only GOAL.md, the spec-excluded diff, the rubric and AGENTS.md.
- Each built, ran and mutated in its own clone of the reviewed commit. The worktree was clean on both hand-backs.
- They agree on F1. F2 comes from the block side alone; it reproduced, and nothing on the ship side contradicts it.

## Verification run

- **The profile's verify.** `make -B check` (clang++) and `make -B CXX=g++-16 check` both give `check: 20 passed, 0 failed` and `cli: 121 passed, 0 failed`, with no warnings (both reviewers).
- **The hardened build** named in `tests/check_math.cpp` is green on both compilers: `make -B CXXFLAGS='-O1 -Wall -Wextra -D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE' check`.
- **`bin/mps` driven from empty directories.** The throwaway environment is none, and no run left a file. The reviewers drove:
  - every `-t` name and alias, and plain `31`, comparing the exact start lines;
  - R3's values (`qf quick factor pm p-min none '' LL TF '=ll' ' ll'`) in all five spellings;
  - the five spellings of `-t` and `-f`;
  - groups against their separated flags;
  - PVALs: junk, signs, whitespace, `0x1f`, `1e3`, non-ASCII digits, the 2^32 and 2^64 neighbours, and up to 10,000 leading zeros;
  - `-p N` against independent trial-division lists, byte for byte, for N = 0–300 and up to 1,000,003 on both compilers;
  - `-p 999999999`, which prints 50,847,534 lines ending at 999,999,937, matching π(10^9);
  - colour on and off.
- **Mutation in the reviewers' clones.** Each of these turns `make check` red: dropping 2 from the listing, moving the PVAL bounds, re-adding `none` or `qf`, removing the `=` strip, changing grouping order, removing stray-flag rejection, and misrouting the dispatch. The two exceptions are F1 and F2.
- **The orchestrator's checks,** outside the reviewers' scope:
  - **The cluster evidence:** three jobs, one per target, on `8b6d8b1`, whose code equals `c84bbec`'s outside `spec/`, `issues/` and `ROADMAP.md`.
    - Each is `COMPLETED 0:0` in `sacct`.
    - Each `result.json` passed all five steps with GCC 14, giving check 20/0 and cli 121/0.
    - Each job printed the committed script's blob.
  - The orchestrator re-ran `verify/p4.sh` and got green. `verify/` is unchanged since the plan commit 7b2e1e1.

## Requirement → evidence matrix

Both reviewers verified every row. The cluster runs, which no R-ID names, were graded by the orchestrator.

| R-ID | Implemented by (file/commit) | Verified how | Status |
|------|------------------------------|--------------|--------|
| R1   | `src/main.hpp` help; `src/ansi.cpp` colour (8b6d8b1) | help read exactly; `-h` equals `--help`; the colour help with escapes stripped equals the plain help; help edits fail `tests/cli.sh` | ✅ behaviour · ❌ "states once" unpinned (F2) |
| R2   | `src/cli.hpp` names; `src/main.cpp` dispatch (8b6d8b1) | each name and alias, and plain `31`: exact start lines, exit 0; dispatch mutations fail checks | ✅ |
| R3   | `set_test_case`; `TEST_NONE` removed (8b6d8b1) | rejected values in every spelling: exit 1 with the value quoted; re-adding `none` or `qf` fails a check | ✅ |
| R4   | `check_option_short`/`_long`, `set_value`, `pop_value`, `set_file_path` (8b6d8b1) | five spellings of `-t` and `-f`; `--file out.mps -t ll 31` runs Lucas–Lehmer alone; `=`-strip mutations fail four checks each | ✅ |
| R5   | `check_option_short`, `check_flag` (8b6d8b1) | groups match their separated flags; stray characters exit 1; `-hx` gives the help and `-vh` the version; mutations fail checks | ✅ |
| R6   | `set_pval` with `std::from_chars`; `--` (4430a20) | bad PVALs exit 1 naming PVAL, with no test run; leading zeros and the bound accepted; the same under g++-16 | ✅ |
| R7   | `set_pval` under `-p` (4430a20) | bad `-p` values exit 1 with empty stdout; `-p 0`, `0 -p`, `-p 000` exit 0 | ✅ |
| R8   | `display_primes` (cb52678) | `-p N` byte-exact against independent lists; `-p 999999999` | ✅ |
| R9   | `prime_sieve` (cb52678) | sizes and entries pinned for n = 0–3 and every index from 0; probes clean under the bounds assertions on both compilers; plain `make check` blind to an in-word stray write (F1) | ✅ code · ❌ shown by `make check` (F1) |
| R10  | `tests/` (cb52678 … 8b6d8b1) | green on both compilers; no caveats; every mutation caught except F1's and F2's | ❌ (F1, F2) |
| R11  | `issues/verdicts-and-exit-statuses.md`, `ROADMAP.md` (c84bbec) | seed `unshaped`; entry directly after the parent's; carries every non-goal; its cited lines check out | ✅ |

**Unmapped changes** (possible scope creep). All of these are recorded as PLAN deviations, and none conflicts with a non-goal:
- `--` ends the options, and a lone `-` is PVAL (`src/cli.cpp`). This is how a signed PVAL reaches R6's check.
- The `colorize_usage` regex (`src/ansi.cpp`). It keeps R1's hyphenated aliases whole in colour.
- `set_file_path` only records the path. It creates no file and rejects an empty value, matching the GOAL's "nothing reads or writes the file yet". The ship side notes that no check pins "parsing creates no file". The frozen gates do; the contract doesn't ask for it.
- A long flag given a value is an error ("Option --NAME takes no value").
- The adopted seed's status bookkeeping.

## Findings

Severity: **CRITICAL** (a violation of any numbered section of § Invariants is auto-CRITICAL,
**including lettered subsections**; a § Conventions violation is **HIGH**) · **HIGH** · **MEDIUM** ·
**LOW**. Verdict: **CONFIRMED** (reproduced) vs **PLAUSIBLE** (suspected, needs human triage). Only
CONFIRMED findings auto-loop to `code-build`.

### [MEDIUM/CONFIRMED] F1 — `make check` can't see the sieve write outside its entries for n = 1 or 2
- **Where:**
  - `tests/check_math.cpp:6-14` (the header) and `:42` ("sieve below four exactly");
  - `Makefile`: the check build compiles no bounds assertions;
  - the code it guards is `src/math.cpp:33`.
- **Failure scenario:**
  - A later edit restores the old write of entry 2 for n = 1 or 2, for example with `if (n > 0)` or `if (n >= 2)`.
  - The bit lands inside the vector's storage word, so the profile's `make check` stays green on clang++ and g++-16.
  - R9 requires `make check` to show that the sieve writes nothing outside its entries for n = 0 through 3. Today only the hardened command in the test file's header shows it.
- **Evidence** (both reviewers):
  - Edit `s/if (n > 2)/if (n > 0)/` in `src/math.cpp`, then run `make -B check`: `check: 20 passed, 0 failed`, `cli: 121 passed, 0 failed`, exit 0 on clang++ and g++-16. `if (n >= 2)` gives the same.
  - The header's hardened command on the same edit exits 2. libc++ fails on `vector<bool>::operator[] index out of bounds`, and libstdc++ on `stl_bvector.h:1158 … Assertion '__n < this->size()' failed`.
  - The unchanged sieve passes the hardened build on both compilers.
- **Severity, reconciled:** the block side graded HIGH and the ship side MEDIUM. MEDIUM stands: both sides confirm the shipped behaviour is right, and the gap is in how strong the check is that R9 names.
- **Touches:** R9, R10.
  - PLAN recorded this as its deviation 1: the hardened rebuild was meant to stand in for the plain one.
  - R9 names `make check`, so a plan cannot move the demonstration elsewhere. The fix belongs in the check build, not in the contract.

### [LOW/CONFIRMED] F2 — `make check` doesn't pin that the help states the five spellings once
- **Where:** `tests/cli.sh:124-127` (`has "$help" "$part"`).
- **Failure scenario:** a help that gives the grammar sentence twice still passes.
- **Evidence:** block side.
  - After a second copy of the sentence in `APP_HELP`, `bin/mps --help | grep -c 'Give a valued option'` → `2`.
  - `make -B check` still gives 20/0 and 121/0.
- **Touches:** R1 ("SHALL state once"), R10.

Probed and dropped by the reviewers, recorded as observations rather than findings:
- `mps -1` and `mps -p -1` give "Unrecognized option: -1". Under the documented grammar that word is an option, and `-- -1` names PVAL.
- `-dé` gives an error ending in a lone `\303` byte. It is still an argument error with exit 1.

## Human-gate triggers

- **Not triggered.**
  - Neither CONFIRMED finding is in a high-blast-radius file. Both are in the checks (`tests/check_math.cpp`, `tests/cli.sh`) and the check build.
  - Both reviewers verified `src/math.cpp` correct, with probes over n = 0–5000 and large n under the bounds assertions.
  - No `[human gate]` invariant is violated.
- The block side noted that F1 guards `src/math.cpp`. The orchestrator read § Invariants' "a CONFIRMED finding in any of these files" by the finding's own location.
- The remediation must leave `src/math.*` as it is. If it touches them, this trigger is re-evaluated.

## Remediation (orchestrator's routing)

- **F1 → P1 reopened.**
  - `make check` itself shows R9: the check build (`bin/check` and the objects it links) compiles with the standard libraries' bounds assertions.
  - `bin/mps` keeps its flags, and every compile keeps the IEEE flags last (§ Invariants 2).
  - Gate: `verify/p1.sh` and then `remediation/f1.sh`.
- **F2 → P3 reopened.** Add a check that the help states the spellings once. Gate: `verify/p3.sh` and then `remediation/f2.sh`.
- **P4 reopened.** The code changes, so the cluster runs are redone on the new pushed head. Its gate adds both remediation gates after `verify/p4.sh`.

## Optional completeness sub-pass (separate reviewer; may see TECH.md)

- Not run.

## Review cycle 2 — approved (2026-10-12)

- **Reviewed commit:** 2a21ffc86eed7a27072debf13573db25c4bb88f4. The remediation is `1ed28d7` (F1), `439f35e` (F2) and `2a21ffc` (the P4 reruns).
- **Mode:** scoped to verifying the remediation of F1 and F2 and checking it for regressions.
  - One fresh reviewer ran at xhigh, blind to everything under `spec/` but GOAL.md.
  - It built and mutated in scratch clones under `/usr/bin/make` 3.81 and never built in the worktree, which stayed clean.
  - The debate variant was not rerun: the remediation touches the `Makefile` and `tests/`, which are outside the coupled core.
- **Verdict:** approved, with no findings.

### Verification run

- **F1, verified as a class.** Each case below was a mutation in the reviewer's clone, built with plain `make check` and no flags added.
  - **Red on clang++ and g++-16:**
    - the guard written as `if (n > 0)`, `if (n >= 2)` or `if (n > 1)`;
    - a sieve loop running to `i <= n`;
    - a write at index n, and at n + 64, for n > 64;
    - out-of-range reads in `display_primes` and in the sieve;
    - a read past the end of `remaining` in `src/cli.cpp`, which the unhardened `bin/mps` passed and `bin/check` stopped.
  - In every red case `bin/check` stopped at the named check through the library's assertion: libc++ trapped, and libstdc++ reported `stl_bvector.h:1158 … '__n < this->size()'`.
  - **Control:** the same edit with `make -B BOUNDS= check` is green, so the red comes from the assertions.
  - **Boundary, an observation and not a finding:** a write through a `vector<bool>` iterator is not checked by either library. The sieve indexes with `operator[]`.
- **§ Invariants 2.**
  - `make -n -B bin/mps` is byte-identical to `main`'s on clang++ and g++-16: no assertion macros, no sanitizers.
  - `-std=c++20 -fno-fast-math -ffp-contract=off` comes last on every compile and link, `-O1` override included.
  - The check objects add only the two library macros.
- **F2, verified as a class.**
  - The whole sentence given twice gives `cli: 121 passed, 5 failed`.
  - Each spelling given once more on its own fails exactly its own check.
  - `planned` is 126, and dropping a check from the loop is caught (`FAIL ran 125 of 126 planned expectations`).
- **Regressions: none found.**
  - Each object is built by its intended rule under make 3.81.
  - Incremental builds rebuild exactly the dependent objects in each set, and adding or removing a test file relinks `bin/check`.
  - `make clean` empties `lib/` and `bin/`, and plain `make` builds `bin/mps` alone.
  - A new module lands in both object sets.
  - `-O1` overrides, `make -j8 check`, and `tests/cli.sh` under dash and bash are all green.
  - There are no warnings on either compiler, and the `AGENTS.md` lines and the `tests/check_math.cpp` header describe the build truthfully.
  - The reviewer noted that make 3.81 compares timestamps to the whole second, so a source saved within the second of its object reads as up to date. `main` has the same limit with its single object set; the diff did not introduce it.
- **`make check`** is green on both compilers: `check: 20 passed, 0 failed` and `cli: 126 passed, 0 failed`.
- **Orchestrator** (out of the reviewer's scope):
  - **The cluster reruns:** three jobs, one per target, on `439f35e`, whose code equals `2a21ffc`'s outside `spec/`, `issues/` and `ROADMAP.md`.
    - Each is `COMPLETED 0:0` in `sacct`.
    - Each `result.json` passed all five steps with GCC 14, giving check 20/0 and cli 126/0.
    - Each job printed the committed script's blob.
  - The orchestrator re-ran P4's stored gate (`verify/p4.sh`, `remediation/f1.sh`, `remediation/f2.sh`) and got green.
  - `verify/` is unchanged since the plan commit, and `remediation/` since `b4b0add`.

### Requirement → evidence (changes since cycle 1)

| R-ID | Change | Verified how | Status |
|------|--------|--------------|--------|
| R1   | `tests/cli.sh`: each of the five spellings once | duplicate-sentence and per-spelling mutations each fail exactly their checks | ✅ |
| R9   | `Makefile`: the check build carries the bounds assertions; `tests/check_math.cpp` header | the stray-write and out-of-range mutations above are red under plain `make check` on both compilers | ✅ |
| R10  | both of the above | F1 and F2 now caught; `make check` green on both compilers and on three cluster targets | ✅ |

The other R-IDs are unchanged since cycle 1 and remain ✅.

### Human-gate triggers

- **Not triggered.** No CONFIRMED finding in either cycle lies in a high-blast-radius file, and the remediation left `src/` untouched since `c84bbec`. No `[human gate]` invariant is violated; § 2 was checked above.
