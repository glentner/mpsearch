# REVIEW — Squaring modulo 2^p − 1 with a weighted transform (FP64, portable)

> Adversarial QA by `code-review`, run in an isolated context (a fresh subagent at extra-high
> effort). The correctness pass graded the branch diff against [`GOAL.md`](GOAL.md) and the repo's
> `AGENTS.md` § Invariants **only**; it did not see `PLAN.md`, `TECH.md`, `research/`, the gates or
> the evidence. Every finding cites an **executed** command.

- **Reviewed commit:** b3e2ac5b4974adfa8b009915de0772a209f5875e  ·  **Base:** main (`0bc49cb`)  ·  **Date:** 2026-10-31
- **Verdict:** approved
- **Cycle:** 1 of ≤3

**Contract drift check.** `GOAL.md` changed after its shape commit in two commits: the sign-off
(`1758fd3`) and the dated R12 clarification moving the timing base from `f956a7c` to `0bc49cb`, the
Apache-2.0 relicense, whose code is identical (`2805d85`, before any build commit). Both are
legitimate; the review graded the GOAL as it stands, confirmed by the foreman under the standing
autonomy decision (2026-10-11).

## Verification run

Every run was in scratch clones; nothing was built in the cycle worktree. Clang is Apple clang 21;
GCC is g++-16 (16.2.0).

- `make check` (clang, and `CXX=g++-16`) → `check: 103 passed, 0 failed`, `cli: 644 passed, 0
  failed`, 0 warnings on both.
- `make check-engine` (clang) → green, 7:39.55 wall (budget 15:00); GCC's `bin/check engine` →
  green, 7:58. All five engine-tier residue digests are identical across the two compilers.
- R12 default tier, nothing to rebuild, three alternating runs: base `0bc49cb` 3.97, 3.82, 3.81 s;
  head 5.11, 5.03, 5.01 s: about +1.2 s against +5 s (load 5–6). A fake failing engine tier makes
  `make check-engine` exit non-zero.
- `bin/measure --table src/lengths.txt` byte-identical to both maps in `src/lengths.cpp`; seven
  `bin/measure --probe` re-runs (including 1048576 / 20,381,696) and two length re-scans reproduce
  their lines byte for byte. 144 rows, monotone, every chosen probe ≤ 0.35 (worst 0.3125),
  P_max 20,381,696.
- 20 injected mutations; 19 turn the checks red (`no-ones-to-zero`, `to-digits-top-bit`, `no-m13`,
  `table-plus-one`, `measurement-over`, `subtract-low-half-only`, `subtract-off-by-one-odd`,
  `report-half`, `no-distance-refusal`, `square-refusal-clobbers`, `holds-no-upper`,
  `nondeterministic`, `product-is-square`, `product-refusal-clobbers`, `radix-11-root`,
  `no-cube-checksum`, `no-checksum-refusal`, `no-rms-refusal`, and the round-trip family). The one
  survivor is F1.
- Beyond the checks: 176,400 structured inputs at every length to 65,536 (95,007 refused, 0 squared
  wrong); lockstep chains against the next length (N = 983,040 for 1,000 steps at p = 19,046,400;
  N = 16,384 for 50,000; N = 1,024 for 300,000): 0 refusals, 0 disagreements. With the checksums
  removed, length 20 / p 468 / a period-4 full-range input squares wrong with no refusal; with them,
  it is refused.
- `bin/mps` output and exit status match the base for 11 sample invocations; `tests/cli.sh`
  untouched and passing.

## Requirement → evidence matrix

| R-ID | Implemented by | Verified how | Status |
|------|----------------|--------------|--------|
| R1 | `src/ibdwt.cpp` (`to_digits`, `from_digits`) | 640 + 2,880 round trips, 0 disagree; two mutations red | ✅ (reviewer) |
| R2 | `src/fft.cpp`, `src/lengths.cpp` | exactly the 144 lengths; both ends of all 144 squared (288, 0 disagree); `no-m13` red | ✅ (reviewer) |
| R3 | `src/lengths.*`, `src/measure.cpp`, `src/lengths.txt` | independent table check; probe re-runs byte-identical; two mutations red | ✅ (reviewer) |
| R4 | `src/ibdwt.cpp` (`square`, `square_subtract`) | 2,880 squarings, 0 disagree; 3.75M driver squarings vs BigInt, 0 wrong; two mutations red | ✅ (reviewer) |
| R5 | `tests/engine_support.cpp`, `tests/check_engine.cpp` | 60 partial, 15 final, 27 LL zeros, 0 disagree | ✅ (reviewer) |
| R6 | `src/ibdwt.cpp` (`roundoff`) | 864/864 reported = own distance; `report-half` red | ✅ (reviewer) |
| R7 | `src/ibdwt.cpp` (monitor) | 848 refusals name p, N, distance, threshold, residue unchanged; two mutations red | ✅ (reviewer) |
| R8 | `src/ibdwt.cpp` (`require_held`) | refusal checks; messages name both numbers; `holds-no-upper` red | ✅ (reviewer) |
| R9 | whole engine | repeats (864); byte-identical runs; clang/GCC digests and 224/224 residues identical | ✅ (reviewer) |
| R10 | Makefile `$(IEEE)` on every rule | grep for pragmas, attributes, overrides: none | ✅ (reviewer) |
| R11 | new sources | no intrinsics, asm, threads, extensions; both compilers, 0 warnings | ✅ (reviewer) |
| R12 | `tests/check.*`, Makefile `check-engine` | default tier +1.2 s / +5 s; engine tier 7:40 / 15:00; covers every odd m, shifted, product | ✅ (reviewer) |
| R12 (the plan records its baseline) | `PLAN.md` "R12 baseline" | read: the f956a7c baseline (3.754 s least of 10) and the prototype's interleaved timing on `0bc49cb` | ✅ (orchestrator) |
| R13 | `AGENTS.md`, `doc/weighted-transforms.md`, `doc/references.md` | commit `35d6d25` adds all eight paths with their high-blast-radius lines, § Map and § Build and run | ✅ (reviewer) |
| R14 | `src/ibdwt.cpp` (`multiply`) | 576 products, 0 disagree; operands unchanged on refusal; two mutations red | ✅ (reviewer) |
| Cluster runs (P4) | `evidence/` | three passed runs of `d7d0feb` (code = HEAD's): 17668927, 44875732, 483059; 103 + 644 on each; sacct COMPLETED 0:0; redacted | ✅ (orchestrator) |

Unmapped changes: none that matter. The checksum and envelope refusals go beyond R7's distance
refusal and serve § Invariants 2 (the control above shows the checksums are load-bearing); a
consequence is that some structured residues are refused (all 384 constant or alternating inputs
above 2^14 words), which no R-ID requires to square. `bin/measure LENGTH …` and the runner's tiers
serve R3 and R12; the `issues/` and `ROADMAP.md` edits record deferrals.

## Findings

### [LOW/CONFIRMED, out of Scope] No check isolates the magnitude refusal
- **Where:** `src/ibdwt.cpp:492-494`
- **Failure scenario:** deleting the refusal of an output that is not finite or reaches 2^51 leaves
  `bin/check` (103/0) and `bin/check engine` (10/0) green.
- **Evidence:** the reviewer's mutation `no-magnitude-refusal` in a scratch clone.
- **Touches:** § Invariants 2 (a human-gate section; `src/ibdwt.cpp` is on the high-blast-radius list).
- **Triage (foreman):** out of Scope. It is hardening that no R-ID or invariant asks for, not a
  correctness defect. Without the refusal no wrong residue results: a non-finite output fails the
  distance test (`roundoff_error`), and a finite output of 2^51 or more is refused by `carry` as
  `engine_error` before anything is assigned. The difference is only which error class reports it,
  on inputs the envelope refuses first in every case the checks or the reviewer's 176,400-input
  search built. Its home already exists: `issues/lucas-lehmer-test.md`, the Notes bullet "No check
  isolates the magnitude refusal" (committed in `88c1d69`, before the reviewed commit), for the
  first cycle that acts on the engine's refusals, where the error class matters to a retry.

Prose noted, not a finding: `ROADMAP.md` and `issues/engine-differential-standing-check.md`'s
Problem say the full suite takes "about 15 minutes" (its budget); it measured about 7.7 minutes, and
the same seed's Notes give the measured 459 s.

## Human-gate triggers

- No CONFIRMED defect of this cycle. F1 touches a § Invariants 2 path but is out of Scope (above).
- This cycle adds the first arithmetic engine, a § Invariants 2 [human gate] change. Under the
  standing autonomy decision (2026-10-11-mpsearch-autonomous) the foreman judges human gates:
  signed off, on the differential tests against the reference arithmetic on the known answers
  (R5), the measured round-off with refusal as a fault (R6, R7), IEEE-strict compilation (R10) and
  determinism across runs and compilers (R9), all executed above. Reported to Geoffrey with the
  publish.

## Optional completeness sub-pass

Not run (not requested).
