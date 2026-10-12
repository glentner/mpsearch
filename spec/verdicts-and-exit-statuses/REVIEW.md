# REVIEW — Say what was found: a result type, one verdict line, exit statuses and `--json`

> Adversarial QA by `code-review`, run in an isolated/clean context (a subagent; where the runtime
> has none, `code-review`'s sequential fallback). The correctness pass grades the branch diff against
> [`GOAL.md`](GOAL.md) + the repo's `AGENTS.md` § Invariants **only** — it does not see
> `PLAN.md`/`TECH.md` (avoids grading-its-own-homework / plan-sycophancy). Every finding cites an
> **executed** command, not an assertion.

- **Reviewed commit:** df9bd39c5984279cbc0efee8d7e8eec8244b6e9f, pinned for the record at 08d9b4c (a seed note, no code)  ·  **Base:** main (`origin/main` 899b3de)  ·  **Date:** 2026-10-12
- **Verdict:** changes-requested
- **Cycle:** 1 of ≤3 — mirrors `review.cycle` in `TECH.md` (escalate to human on non-convergence)

**Mode: the debate variant.** The diff touches the coupled core (the command-line surface and the test entry points) and adds high-blast-radius paths.
- Two fresh reviewers ran at xhigh, one arguing ship and one arguing block.
- Each saw only GOAL.md, the spec-excluded diff, the rubric and AGENTS.md.
- Each built, ran and mutated in its own clone of the reviewed commit. The worktree was clean on both hand-backs.

**Contract drift check.** GOAL.md changed after its shape commit in `1545e54`. That commit records two readings of R8/R9 that the plan raised; the foreman confirmed them at PLAN/TECH sign-off under the author's delegation. No R-ID text changed.

## Verification run

- **The profile's verify.** `make -B check` (clang++) and `make -B CXX=g++-16 check` both give `check: 44 passed, 0 failed` and `cli: 530 passed, 0 failed`, with no warnings (both reviewers).
- **`bin/mps` driven from empty directories** against independent trial-division oracles:
  - block side: 687 exponents × 6 flag sets, 4,122 runs, with 0 mismatches in status, exact lines, `json.loads` on every record, a as the smallest prime factor, and no test start line on a composite exponent;
  - ship side: 245 exponents × 8 `-t` forms × with and without `-s` × three output modes, with 0 failures.
- **The exponent check.** A C++ probe over 8,013,601 exponents (2–3M, 997M–999,999,999, every q·r near 31,622, 2M random) found 0 smallest-factor mismatches. Python's exact `pow(2, p, 2^a − 1) == 1` agrees with the `a | p` re-check on all 79,401 pairs tried, and 2,256,003 refusal pairs gave 0 bad results.
- **The printer, through constructed results.** Every outcome sequence up to length 4 (3,110 cases, both modes) gave:
  - status 20 when the run holds an error;
  - 30 for a prime or probable prime with a composite;
  - otherwise the first decisive result's status, or 42;
  - never a decisive line or record on stdout under an error or a contradiction.
- **Mutation, in the reviewers' clones.** 30 block-side and 69 ship-side mutations of R1–R12 turn `make check` red. The exceptions are F3 below, a reversed `status_of` order that no run can reach, and the "Exit status:" bolding.
- **Orchestrator** (out of the reviewers' scope):
  - **The cluster evidence:** three jobs, one per target, on `7ff16a8`, whose code equals `df9bd39`'s outside `spec/`, `issues/` and `ROADMAP.md`.
    - Each is `COMPLETED 0:0` in `sacct`.
    - Each `result.json` passed all five steps with GCC 14, giving check 44/0 and cli 530/0.
    - Each job printed the committed script's blob.
  - The orchestrator re-ran P1–P4's gates and got green; `verify/` is unchanged since the plan commit 43c129e.

## Requirement → evidence matrix

Both reviewers verified every row. The cluster runs, which no R-ID names, were graded by the orchestrator.

| R-ID | Implemented by (file/commit) | Verified how | Status |
|------|------------------------------|--------------|--------|
| R1   | `src/result.*`; the tests return `Result` (b579562) | no `bool` test remains; the private constructor won't compile from outside; line, status and record are built only from results | ✅ |
| R2   | the stubs (b579562) | every stub gives "not implemented yet" with its start line unchanged; mutations to prime, composite or probable prime are red | ✅ |
| R3   | `smallest_prime_factor`, `composite_by_exponent`, `check_exponent` (7ef8d5d) | the sweeps and probes above, under every `-t` form; mutations of the re-check, the bound and the cofactor are red | ✅ behaviour · ❌ re-check wiring unpinned (F3) |
| R4   | `check_exponent` (7ef8d5d) | primes print nothing for the check and run the tests | ✅ |
| R5   | the dispatch (7ef8d5d) | `mps -s 33` gives three no-verdict lines and exit 42; mutations ignoring `-s` are red | ✅ |
| R6   | `verdict_line` (b579562) | byte-exact lines in the sweeps; version-drop mutations, one per outcome, are red | ✅ for every reachable result · ❌ hostile reasons (F1) |
| R7   | `run_in_order`, `test_all` (b579562) | the order and the stop; mutations are red | ✅ |
| R8   | `status_of`, `enum Status` (b579562, 7ff16a8) | the printer probe; every value pinned as a literal number | ✅ |
| R9   | `report_result`, `report_run` (b579562) | the printer probe; mutations printing on an error or a contradiction are red | ✅ |
| R10  | `json_record`, `-j` (b579562, e684a56) | `json.loads` on every record in the sweeps and the probe; record-shape mutations are red | ✅ for every reachable result · ❌ invalid UTF-8 (F1) |
| R11  | the help and usage, with the parser (e684a56) | the help read; help and usage mutations are red; one commit | ✅ |
| R12  | `AGENTS.md` (b579562) | both sentences say "of `mps`"; `src/result.*` and `src/main.cpp` are on the list; AGENTS.md mutations are red | ✅ |
| R13  | `tests/` | green on both compilers and three cluster targets; nothing still expects exit 0 | ❌ (F1, F3) |

**Unmapped changes:** none beyond the build's needs: `result` in `MODULES`, the "Exit status:" header bolding (PLAN deviation 1, unchecked and cosmetic) and the seed's `adopted:` status.

## Findings

Severity: **CRITICAL** (a violation of any numbered section of § Invariants is auto-CRITICAL,
**including lettered subsections**; a § Conventions violation is **HIGH**) · **HIGH** · **MEDIUM** ·
**LOW**. Verdict: **CONFIRMED** (reproduced) vs **PLAUSIBLE** (suspected, needs human triage). Only
CONFIRMED findings auto-loop to `code-build`.

Neither reviewer found a way for today's `mps` to print an unearned verdict, or any verdict despite an error or a contradiction.

### [LOW/CONFIRMED] F1 — a hostile reason breaks the verdict line or the record
- **Where:** `src/result.cpp:182-183` (`verdict_line` puts the reason in as it is) and `:112-135` (`json_string` passes bytes from 0x80 up unchanged).
- **Failure scenario:**
  - A reason holding a newline turns one result into two lines on stdout, and the second can read as a verdict: a reason of `"…:\nM31 ll: prime"` prints `M31 ll: prime (mps 0.1.0)` as line 2 and exits 42. This breaks R6's "exactly one line … for that result" and verdict integrity (§ Invariants 1).
  - A reason holding a byte sequence that isn't valid UTF-8 gives a record that doesn't parse, breaking R10's "every such line SHALL parse as JSON".
  - No reason `mps` builds today does either. Every test after this cycle builds results, and an error's reason is the natural place for an exception's text.
- **Evidence:** block side.
  - A clone whose trial-factoring reason held `\nM31 ll: prime` printed that line, exit 42.
  - A probe printing `json_record(Result::no_verdict(…, "bad\xff"))` made `json.loads` raise `UnicodeDecodeError … 0xff`.
  - Only the JSON side's escaping of quotes, backslashes and control bytes is checked (`tests/check_result.cpp:460-476`).
- **Touches:** R6, R10, R13; § Invariants 1. **Human gate:** `src/result.cpp` is on the high-blast-radius list.

### [LOW/CONFIRMED] F2 — the map still calls `src/math.*` the sieve and the stub tests
- **Where:** `AGENTS.md:39`.
- **Failure scenario:** the operating manual misleads the next agent. The exponent check (`smallest_prime_factor`, `composite_by_exponent`, `check_exponent`) and `run_in_order` now live there. The exponent check is the only code that produces a composite verdict today.
- **Evidence:** block side, `grep -ciE 'exponent check|check_exponent|run_in_order' AGENTS.md` → 0.
- **Touches:** documentation hygiene. The § Invariants gate itself is intact, since § 1 covers `src/math.*`.

### [LOW/CONFIRMED] F3 — `make check` doesn't pin that the exponent check goes through its re-check
- **Where:** `src/math.cpp:191`, `return composite_by_exponent(p, a);`.
- **Failure scenario:**
  - Replace that call with a direct `Result::composite_by_factor(WORK_PTEST, p, "2^a-1")`, and `make check` stays green. `tests/check_math.cpp:270` checks the guard function on its own, not that the exponent check uses it.
  - Today the search is right, so nothing observable changes. A later refactor could drop the § 1 re-check, the defence against a wrong factor search, silently. Only this cycle's frozen gates, which inject a lying search, catch it.
- **Evidence:** ship side, mutation `check_exponent_bypass`: GREEN, `check=44/0 cli=530/0`. With the search broken to return non-divisors, the real path refuses: `mps 33` exits 20, prints nothing on stdout, and says "the exponent factor 5 fails its exact re-check".
- **Touches:** R3 ("Before reporting, `mps` SHALL confirm that a divides PVAL"), R13; § Invariants 1. **Human gate:** the fix lands in `src/math.*`.

### [MEDIUM/PLAUSIBLE] P1 — `-t all` keeps running after an error — deferred
- **Where:** `src/math.cpp:104-113` (`run_in_order`); `src/result.cpp:303-304`.
- **Scenario:** after a test returns an error, the later tests still run and print their no-verdict lines, then the run exits 20 (block side, by editing a clone's stub).
- **Triage:** the orchestrator, under the author's delegation, deferred it.
  - **Not a contract defect:** R7 stops only at a decisive result, an error is not decisive, R9 forbids a line for the error itself, and Clarification 2 forbids any decisive verdict, which holds.
  - **Real cost later:** once Lucas–Lehmer is real, a failed run shouldn't spend days on the tests after it.
  - **Home:** a note in `issues/lucas-lehmer-test.md` (08d9b4c), the first cycle with a long-running test. The stop rule and its checks change together there.

**Observations, not findings:**
- `status_of` taking the last decisive result instead of the first stays green, but no run can hold two decisive results.
- The "Exit status:" bolding is unchecked, and cosmetic.
- `-dé` and the like name a raw byte in their error, which is still an argument error.

## Human-gate triggers

- **Triggered.**
  - F1 is in `src/result.cpp`, and F3's fix lands in `src/math.*`. Both are high-blast-radius paths.
  - Neither is reachable from any input to today's `mps`: both are defence in depth at the verdict path.
- The gate holds until publish. Geoffrey's sign-off is required, and recorded here, before landing.

## Remediation (orchestrator's routing)

- **F1 → P1 reopened.**
  - `verdict_line` puts the reason through `line_text(const std::string& text)`. It replaces each byte below 0x20, and DEL, with a visible escape, and leaves everything else as it is.
  - `json_record` puts every string through `json_text(const std::string& text)`, which replaces `json_string`. It escapes as today, passes valid UTF-8 through, and writes U+FFFD for each byte that cannot start or continue a valid UTF-8 sequence.
  - Both are defined at file scope in `src/result.cpp`, each with its name on a first line ending in `{` and a body ending at a line that is exactly `}`. `src/result.hpp` is unchanged.
  - `make check` drives hostile reasons through `verdict_line` and `json_record`.
  - Valid input encodes as before, so `record_version` stays 1.
  - Gate: `verify/p1.sh`, then `remediation/f1.sh`.
- **F2 and F3 → P2 reopened.**
  - `math.hpp` gains `using FactorSearch = long unsigned (*)(const long unsigned p);` and `check_exponent_by(p, search)`, which holds the check's body. `check_exponent(p)` returns `check_exponent_by(p, smallest_prime_factor)`.
  - `make check` drives `check_exponent_by` with searches that lie: 0, 1 or a non-divisor must give an error, and a larger divisor its own factor.
  - The § Map line names the exponent check and `run_in_order`.
  - Gate: `verify/p2.sh`, then both remediation gates.
- **P4 reopened.** The code changes, so the cluster runs are redone on the new pushed head. Its gate runs both remediation gates after `verify/p4.sh`.

## Optional completeness sub-pass (separate reviewer; may see TECH.md)

- Not run.

## Review cycle 2 — escalated to Geoffrey (2026-10-12)

- **Reviewed commit:** 1c0d728c40220f4af1bb6e8b580701c8d9ac6093. The remediation is `e88f30b` (F1), `22911ab` (F2, F3) and `1c0d728` (the P4 reruns).
- **Mode:** scoped to F1–F3 and regressions, in the debate variant. The remediation touches the coupled core and high-blast-radius paths.
  - Two fresh reviewers ran at xhigh, one arguing ship and one arguing block, each in its own clone.
  - Both were blind to everything under `spec/` but GOAL.md.
- **Status:** escalated. The review didn't converge: three further CONFIRMED LOW findings, all in high-blast-radius paths. The human gate holds, so the next step is Geoffrey's.

### Verification run

- **`make -B check`** is green on clang++ and g++-16, with 49 checks and 530 cli checks and no warnings (both reviewers).
- **No regressions:**
  - `df9bd39` and `1c0d728` agree byte for byte in stdout, stderr and status across 3,077 and 195 invocations (ship and block sides).
  - About 70k valid-UTF-8 strings give identical records, so `record_version` rightly stays 1.
  - § Invariants 2 holds: `bin/mps`'s flags are unchanged, and the IEEE flags are on every compile.
- **F1, behaviour:**
  - Through a probe against `result.hpp`, 183,469 and 317,050 hostile reasons (every byte, every byte pair, every scalar value, every malformed kind, fuzz) produced 0 failures. Every line is one line with no control byte. Every record is UTF-8 and parses, with the reason equal to an independent per-byte U+FFFD oracle and valid text kept exactly.
  - The real `bin/mps`, with hostile reasons instrumented in, gives the same.
  - Undoing either half of the fix turns `make check` red.
- **F2:** verified. The map line matches the code.
- **F3, behaviour:**
  - The real `mps` refuses every lying search: 0, 1, 5, 7, 34, 66, 2³²−1, 2⁶⁴−1, 2p and 3p each exit 20 with nothing on stdout.
  - A sweep of 2,513,434 exponents agrees with an Eratosthenes oracle.
  - The cycle-1 mutation, and every weakening of the re-check's guard, turn `make check` red.
- **Orchestrator:**
  - **The cluster reruns:** three jobs, one per target, on `22911ab`, whose code equals `1c0d728`'s.
    - Each is `COMPLETED 0:0`, with all five steps passed and check 49/0, cli 530/0.
    - Each job printed the committed script's blob.
  - The orchestrator re-ran P4's stored gate and both remediation gates, and got green.

### Findings

Each is LOW and CONFIRMED by executed mutation or probe. None changes any output of today's `mps`; each is regression protection at the verdict path.

- **G1 — F3's bypass passes `make check` one call up** (`src/math.cpp:198-208`, `src/main.cpp:54`). Both reviewers found it.
  - **Mutants that stay green:**
    - `check_exponent` rewritten to build the composite itself (M2, F3b);
    - the composite built inline in `main.cpp` (M6, F3i);
    - the re-check skipped when the search is `smallest_prime_factor` (F3h).
  - **Why:** the new check pins `check_exponent_by`, not the function `bin/mps` runs.
  - **What it lets through:** with F3b and a search fault at 1,000,001 = 101 × 9901, which lies outside the sieve check's range, `mps 1000001` printed a false `composite, factor 2^3-1` and exited 40.
  - **Why it's hard to pin:** a guard that never fires can't be observed on the production path without injecting a fault into the binary. This cycle's frozen gates do that by link-time interposition.
- **G2 — the UTF-8 half of F1 is under-pinned** (`src/result.cpp:154-176`, `utf8_length`; `tests/check_result.cpp:601-609`). Both reviewers found it.
  - **Mutants that stay green:**
    - E0 accepting a second byte of 0x80–0x9F;
    - F0 accepting 0x80–0x8F;
    - leads F5–F7 accepted;
    - only the second byte checked.
  - Each writes a record `json.loads` rejects, and the last can put a raw newline in one.
  - The malformed table has no 3- or 4-byte overlong, no F5–F7 lead, and no bad third or fourth byte.
- **G3 — the verdict line writes the factor as it is** (`src/result.cpp:266`; the contradiction line on stderr, `:405-408`). Block side.
  - A factor holding a newline splits the line. The record escapes it.
  - Only `composite_by_exponent` builds a factor today, and it writes `2^<digits>-1`.

**Observations, not findings:**
- DEL stays raw in the record, which is valid JSON, and the line escapes it.
- U+2028 and U+0085 pass through raw, which is one line by POSIX.
- The map names modules, not each function.

### Human-gate triggers

- **Triggered,** as in cycle 1. G1–G3 lie in `src/math.cpp`, `src/main.cpp` and `src/result.cpp`.
- The review loop reached its second cycle without converging, so the orchestrator escalated to Geoffrey instead of reopening phases.

### Geoffrey's decision (2026-10-12)

- **On cycle 1's human gate:** "Roger. Seems reasonable. Proceed." This approved the routed fixes for F1 and F3, and landing once the next review was clean with the fixes as described.
- **On cycle 2's escalation:** "Land now, defer gaps."
  - G1–G3 are accepted as residual coverage gaps. None changes the output of any run of today's `mps`, and both reviewers verified the behaviour behind each exhaustively.
  - They are deferred to `issues/lucas-lehmer-test.md` (4be6c3a), whose cycle brings the first real verdicts and reworks the verdict path.
- **Verdict: approved,** by the human gate's decision. The human-gate triggers of both cycles are signed off.
