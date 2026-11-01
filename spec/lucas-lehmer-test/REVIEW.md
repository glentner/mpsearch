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

## Review cycle 2 — changes-requested (2026-11-01)

- **Reviewed commit:** 7bd10396e671f797d81e8ba1e14100e51a4602e9 (code equal to `df877de`, the tested commit)  ·  **Base:** main (`bcb6fdf`)
- **Mode:** a fresh blind pass over the full spec-excluded diff (the default), not scoped to cycle 1's findings. The reviewer was barred from `REVIEW.md`, `remediation/`, PLAN, TECH, `verify/` and `evidence/`.
- **Disclosure (reviewer's own):** while checking a same-commit rule it ran `git show 1e820ba` and saw that commit's message, which names cycle 1's F1–F4. Its mutation runs were already finished, and none of its findings depend on it.
- **Cycle 1's findings:** F1 (every refusal reason), F2 (the verify row), F3 (symbol 0) and F4 (line references) are each graded passing by this pass's own evidence: R14 on every reason combination by CLI, the verify row against the Makefile, mutant L22 (a zero symbol accepted) killed, and no stale line references found.

### Verification run

- `make check` on clang and `CXX=g++-16`: `check: 116 passed, 0 failed`, `cli: 644 passed, 0 failed`, `ll: 48 passed, 0 failed`. GCC gave 0 warnings.
- `make check-long`: 45.3 s on clang and 48.8 s on GCC (budget 900 s).
- R16: least of 6 interleaved runs against `origin/main` in a scratch clone gave 4.968 s against 8.443 s, so +3.48 s (budget +5 s). Two stray idle-loop shells held about one core each during all timing runs; base and head were interleaved, so both saw the same load.
- `MPS_GCC=g++-16 sh ci/engine-gate.sh` is green on 7bd1039: clang 493 s, GCC 543 s, 5 digests equal. Compare mode exits 1 on a changed digest, a FAIL line, a missing `ll:` or digest, and a red summary. It exits 2 on an unreadable log and on a fake GCC or clang.
- `bin/mps -t ll` on M216091 (prime, 108.8 s, three correct progress lines); shifts across five runs of 31; and R14 on 2, `-s 4`, `-s 9`, `-s 33`, 20381701, `-s 20381697`, `-s 20381698`, 999999937, `-s 999999999` and `-s 999999998`.
- The Jacobi symbol checked against the reviewer's own Euler-criterion and factorization oracle on 11,286 cases. `generate.py jacobi` reproduces the vectors byte for byte. `jacobi_of`, `unshifted`, `degenerate` and `confirms_prime` were checked on 3,895 shifted LL residues.
- A build that aborts if any `lucas::Options` field is set passes all of cli.sh and ll.sh, so the hooks are unreachable from `bin/mps`.
- 67 source mutants of `src/lucas.cpp` and `src/mp.cpp`'s Jacobi, each under clang and g++-16. 7 equivalent (L01, L06, L14, L47, L50, M08, M09), each with a reason. Two findings below.

### Requirement → evidence matrix

R1–R12 and R14–R18 ✅, each by the commands above and the reviewer's matrix (kill lists per R-ID). R13 ✅ in behaviour (correct lines on M216091), with its fields not pinned (F5). R5 ✅, with one borrow path in the symbol's helper unpinned (F6). No scope creep, and no § Invariants concern.

### Findings

#### [LOW/CONFIRMED] F5 — R13's progress fields are not pinned
- **Where:** `src/lucas.cpp:273`, `:275`, `:277`; `tests/check_lucas.cpp` (`the test logs its progress`)
- **Failure scenario:** three mutants survive `make check` under both compilers:
  - deleting `line_largest = 0;` makes every later line repeat the run's largest distance, not the largest since the last line;
  - deleting `line_time = now;` makes the per-iteration time measure from the run's start;
  - an ETA of `each * last` instead of `each * (last - i)`.

  The check asserts only that each field's words appear.
- **Evidence:** the reviewer's mutant runs under clang and g++-16. A driver at M9941 with a 0.3 distance at iteration 100: the real code goes 0.3000, 0.1875, 0.1562; the first mutant gives 0.3000 on every line through iteration 9864.
- **Touches:** R13. **Class:** every value a progress line reports is pinned to what R13 says it means: the iteration count, the time per iteration since the last line, the time left, and the largest distance since the last line. **Route (surviving mutants):** a sweep over every statement of `src/lucas.cpp` that feeds a reported value (the progress line, the run report's counters and fields, the log of distances), not only its decisions. Each mutant runs under clang and g++-16, with equivalent mutants recorded and why.

#### [LOW/CONFIRMED] F6 — the Jacobi vectors miss a borrow path in `subtract_in_place`
- **Where:** `src/mp.cpp:568` (`subtract_in_place`, :556); `tests/vectors/jacobi.txt`, `tests/vectors/generate.py`
- **Failure scenario:** the mutant `borrow = below;` (dropping a borrow carried through a limb where x equals y) survives `make check` under both compilers. On 41 crafted 3-limb pairs it disagrees with a reciprocity oracle on 28. The first is `jacobi(600000000000000050000000000000005, 700000000000000050000000000000003)`: −1, mutant +1. The real code agrees on all 41.
- **Reach:** random residues mod M_p take this path with probability about 2⁻⁶⁴ per limb, so no real run is affected. It is reachable, and `src/mp.cpp` is on the high-blast-radius list.
- **Touches:** R5. **Class:** the vectors reach every carry and borrow path of the symbol's multi-limb helpers (`subtract_in_place`, `compare_limbs`, the strip and swap steps): equal limbs, a borrow carried across one or more equal limbs, a borrow into the top limb. The vectors are written by their command and recomputed by an independent oracle. **Route (surviving mutant):** a sweep over every decision and arithmetic statement of the Jacobi code path in `src/mp.cpp`, under both compilers, with equivalent mutants recorded and why.

### Human-gate triggers

- **Correction (foreman, 2026-11-01, after remediation 2).** § Invariants' gate is triggered by a CONFIRMED finding *in* a listed file, whatever its fix touches. This record first read it as being about the fix's paths. Read correctly, three more findings trigger it besides cycle 1's F1, and each is signed off below by the foreman under the standing autonomy decision (2026-10-11-mpsearch-autonomous), 2026-11-01:
  - **Cycle 1 F3** (`src/lucas.cpp`): fixed by a check and a mutant sweep only (`1e820ba`); `src/lucas.cpp` itself changed only for F1. Signed off.
  - **Cycle 2 F5** (`src/lucas.cpp`): fixed in `tests/check_lucas.cpp` only (`7efc00b`); `src/lucas.cpp` is unchanged since `1e820ba`. Signed off.
  - **Cycle 2 F6** (`src/mp.cpp`; the fix is in `tests/vectors/`, also listed): `8ea53d6` and `0c45bc3` append 32 records to `jacobi.txt` through `generate.py`, each symbol computed by its Python reciprocity loop and asserted to reach its limb path. No existing record or expected value changes: the only removed line is the old trailer. The C++ `jacobi` agrees on all 493 records. `src/mp.cpp` is unchanged by this round (no `src/` change since `b7c7db6`). Signed off.
- Cycle 1's F1 trigger: signed off (above) and still on record for publish.
