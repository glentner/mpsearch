# REVIEW — The Lucas–Lehmer test on the engine, with a shift and its error checks

> Adversarial QA by `code-review`, run in an isolated/clean context (a subagent). The correctness
> pass grades the branch diff against [`GOAL.md`](GOAL.md) + the repo's `AGENTS.md` § Invariants
> **only** — it does not see `PLAN.md`/`TECH.md`. Every finding cites an **executed** command, not an
> assertion.

- **Reviewed commit:** 92d08c7af5e35f5e6edef022fd17de7363348183  ·  **Base:** main (`bcb6fdf`)  ·  **Date:** 2026-11-01
- **Verdict:** changes-requested
- **Cycle:** 1 of ≤3 — mirrors `review.cycle` in `TECH.md`
- **Contract drift check:** GOAL.md changed after the shape commit in `cc62474` (sign-off) and
  `02b43c5` (three clarifications ruled at plan, recorded in GOAL's Clarifications); the reviewer
  graded the GOAL as it stands at the reviewed commit.

## Verification run

Commands the reviewer executed and their outcomes:

- `make check` (clang, the profile's `verify`) and `make check CXX=g++-16` → `check: 116 passed, 0
  failed`, `cli: 644 passed, 0 failed`, `ll: 48 passed, 0 failed` on both.
- `make check-long` → `ll: 62 passed, 0 failed`; 45.6 s on clang, 48.8 s on g++-16 (budget 900 s).
- R16's budget: `make check` interleaved over 5 rounds against `origin/main` built in a scratch clone:
  4.97 s at `bcb6fdf`, 8.41–8.43 s here, +3.45 s (budget +5 s).
- `MPS_GCC=g++-16 sh ci/engine-gate.sh` → green on 92d08c7 (clang 496 s, GCC 542 s, 5 digests equal);
  compare mode on hand-made logs: a flipped digest, a missing `ll:` summary, a red `cli:`, an unknown,
  duplicate or malformed label each exit 1; a missing log or a `g++` that is clang exit 2.
- `bin/mps -t ll` on primes, composites and refusals (`216091`, `1279` ×5 for distinct shifts,
  `-s 9`, `-s 999999998`, `-s 20381697`, `20381701`, `2`), with `-j` records.
- The `jacobi` vectors against the reviewer's own Python symbol (461/461), `generate.py jacobi`
  reproducing the file byte for byte, and C++ `jacobi` on 3,163 further inputs up to M44497.
- A driver of its own over `lucas::Options`: corruptions, refusals (the real engine at a 1e-9
  threshold), zeros at the last iteration, a symbol-0 state; 200 random single corruptions each of
  M1279 and M1277 (about half caught, the Jacobi check's documented limit).
- 19 source mutants of `src/lucas.cpp`, `src/mp.cpp` and `src/math.cpp` under `make check`: 18 killed.
- Exact runs at every prime p < 700: no check misfires on a correct run.

## Requirement → evidence matrix

| R-ID | Implemented by | Verified how | Status |
|------|----------------|--------------|--------|
| R1 | `src/lucas.*`, `tests/ll.sh`, `make check`/`check-long` | ll 48 (all 21 odd primes ≤ 10,000) and long 62 (all 27 ≤ 100,000); a lying binary fails ll.sh | ✅ |
| R2 | same | 1237, 1277, 10303, 100069: composite, exit 40, Res64 equal to the reviewer's Python | ✅ |
| R3 | `lucas::test` shift | five runs of 1279, five shifts; `check_lucas` at two shifts; fixed-shift mutant killed | ✅ |
| R4 | `jacobi_of`, schedule | 32 checks a run, the last on the final residue; symbol 0 fails at runtime (M83 driver) but no check pins it (F3) | ✅ (F3) |
| R5 | `jacobi` in `src/mp.*`, `tests/vectors/jacobi.txt` | 461/461 against own Python, byte-for-byte regeneration, 3,163 extra inputs | ✅ |
| R6 | recovery | one failure recovered, a 4th errors with status 20; mutants killed | ✅ |
| R7 | `degenerate` | ±1, ±2, 0 at checked iterations caught; mutants killed | ✅ |
| R8 | `confirms_prime` | a false zero caught at M1237, a true one passes at M1279 | ✅ |
| R9 | replay | the real engine keeps its residue on refusal; replay at the same iteration, length, shift | ✅ |
| R10 | length retry | moves to length 60 with correct answers; error after 3 retries, status 20 | ✅ |
| R11 | distances | 12 counted, 8 logged, the rest folded; record fields present | ✅ |
| R12 | `RunReport`, views, `RECORD_VERSION` 2 | `-j` records; version bump in `cb33399` with the record change | ✅ |
| R13 | progress | 3 lines about 30 s apart on 216091 | ✅ |
| R14 | refusals | all cases but an even p > P_max, which gets no P_max reason (F1) | ❌ (F1) |
| R15 | `run_in_order` | stand-ins with the error first, middle, last; mutant killed | ✅ |
| R16 | hooks, budget, `cli.sh`, long tier | hooks only in `tests/`; +3.45 s; 644 kept count, only LL-decided expectations changed; both compilers | ✅ |
| R17 | profile row, `ci/engine-gate.sh` | green run, red and exit-2 controls (above) | ✅ |
| R18 | AGENTS.md, notes | high-blast line in `cb33399`; notes and check markers verified | ✅ |

Unmapped changes (possible scope creep): none of substance (the JSON reader's numbers,
`compare_limbs`, `cli.sh` reading P_max, error records under `-j` each serve R5, R12, R14 or a
Clarification).

The cluster runs are graded by the orchestrator: `evidence/` holds three targets on `2530c06`
(whose code equals the reviewed commit's), each `COMPLETED 0:0`, 116 + 644 + ll 48.

## Findings

### [MEDIUM/CONFIRMED] F1 — an even exponent past P_max gets no reason naming P_max
- **Where:** `src/lucas.cpp:152-153`; `doc/lucas-lehmer.md:128`
- **Failure scenario:** `bin/mps -s -t ll 999999998` → `no verdict, the Lucas-Lehmer test applies to
  odd exponents of 3 or more`, exit 42; P_max is not named. R14: any p > P_max SHALL get a reason
  naming P_max. The note's "a p both past P_max and composite gets both reasons" is false for even p.
- **Evidence:** the command above, against `bin/mps -s -t ll 20381697` naming both reasons; no check
  refuses an even p > P_max.
- **Touches:** R14. **Class:** every refusal names every reason that applies to its p, in every
  combination (even or below 3, past P_max, odd composite), and the note says the same.

### [HIGH/CONFIRMED] F2 — the profile's `verify` row no longer describes `make check`
- **Where:** `AGENTS.md:141`
- **Failure scenario:** since `cb33399` `make check` runs three steps (`bin/check`, `tests/cli.sh`,
  `tests/ll.sh`); the row still reads "the checks and the smoke test, both always run". § Conventions:
  the profile's verify changes in the same commit as the make target it names. The command itself is
  still right, so no gate is weakened. (A § Conventions violation grades HIGH; the reviewer offered
  MEDIUM–HIGH.)
- **Evidence:** `Makefile:73-74` against `AGENTS.md:141`.
- **Touches:** § Conventions. **Class:** every profile row and § Build and run line that describes a
  make target says what that target runs.

### [LOW/CONFIRMED] F3 — no check pins that a Jacobi symbol of 0 fails the check
- **Where:** `src/lucas.cpp:287`; `tests/check_lucas.cpp`
- **Failure scenario:** the mutant `symbol != -1` → `symbol == 1` (0 passes) leaves `check: 116
  passed`, `ll: 48 passed`. The code is right today (M83 with 167 | s − 2: recovered once, error with
  status 20 when held), but nothing would catch the regression.
- **Evidence:** the mutant's `make check` output; the reviewer's driver output.
- **Touches:** R4. **Route (surviving mutant):** pin symbol 0 as a failed check, then a mutant sweep
  over every decision in `src/lucas.cpp` (refusals, schedule, degenerate set, the Jacobi acceptance,
  the confirmation, the failure and retry limits, restore), each mutant under clang and g++-16, with
  equivalent mutants recorded and why.

### [LOW/CONFIRMED] F4 — a seed cites a line the diff moved
- **Where:** `issues/verdict-path-guards.md:28`
- **Failure scenario:** it cites `line_text` at `src/result.cpp:99`; since `cb33399` it is at
  `src/result.cpp:130`.
- **Evidence:** `grep -n line_text src/result.cpp` → 130; 99 at `bcb6fdf`.
- **Touches:** documentation accuracy. **Class:** every line reference in a seed this cycle's diff
  moved; prefer naming the function over a line number.

## Human-gate triggers

- **Triggered by F1:** `src/lucas.cpp` is on § Invariants' high-blast-radius list (added by R18).
  Signed off by the foreman under the standing autonomy decision (2026-10-11-mpsearch-autonomous),
  2026-11-01: the fix narrows a refusal's reason text and changes no verdict, exit status or
  arithmetic. Recorded here; a clean later cycle still needs this sign-off on record before publish.
- F2, F3 and F4 touch no listed path and no `[human gate]` section.

## Optional completeness sub-pass

Not run.
