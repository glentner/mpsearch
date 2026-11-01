# Deferral ledger for lucas-lehmer-test

Written at P4, 2026-11-01, from a walk of `TECH.md` P1–P4 (the checklists and their build
amendments), the build commits `44c1284`, `cb33399`, `a1e488d` and `2530c06` and their bodies, and
GOAL's Non-goals. Neither the checklists nor the commits say "do not fix here", "known limitation" or
"left as a follow-up", and no build amendment defers anything; the entries below are the three TECH
P4 names. Each is a Notes bullet in a seed that already covers the work ("deferred from
`lucas-lehmer-test`", written in P4's dispatch A, `2530c06`), so no new seed was filed and
`ROADMAP.md` is as the GOAL sign-off left it. Every `issues/*.md` has exactly one `**Seed:**` line in
`ROADMAP.md`, and this cycle's seed stays `adopted:lucas-lehmer-test`.

The P4 audit (dispatch A) walked R1–R18 against `src/`, `tests/`, `doc/`, `ci/`, `AGENTS.md` and the
`Makefile`, each to its check or its gate step (PLAN § 2.9), and found no gap, so it committed no fix.

Review cycle 1 (`REVIEW.md`, F1–F4) reopened P2 and P4. P2's remediation (`1e820ba`) changed
`src/lucas.cpp`, `tests/check_lucas.cpp`, the note, the profile's verify row and three seeds' line
references; P4's dispatch A of that cycle (`df877de`) audited what it touched against R1–R18 and
§ Invariants and found no gap and no new deferral, so the entries below and `ROADMAP.md` are
unchanged. That round's times, landing gate and cluster runs, of `df877de`, are superseded by
review cycle 2's.

Review cycle 2 (`REVIEW.md`, F5 and F6) reopened P1, P2 and P4. Both fixes are tests and vectors
only, with no `src/` change: P1's remediation (`8ea53d6`, F6) added 32 Jacobi pairs to
`tests/vectors/jacobi.txt`, written by `tests/vectors/generate.py`, and named them in
`tests/check_mp.cpp`'s comment; P2's (`7efc00b`, F5) pinned R13's progress line and the class's
other reported values in `tests/check_lucas.cpp`. P4's dispatch A of that cycle audited what they
touched against R1–R18 and § Invariants and found one gap, fixed in `0c45bc3` before the pre-push:
a comment in `tests/vectors/generate.py` named the review that found a pair, a spec reference in a
product file; the sentence is gone and the vectors are unchanged. It found no new deferral, so the
entries below and `ROADMAP.md` are unchanged. The times, the landing gate and the cluster runs
below are all of `0c45bc3`.

## Deferrals

- **The Jacobi check's schedule at the top of the range** (TECH P4). The test checks J(s − 2 | M_p)
  at `GOOD_STATES` points of every run with the quadratic binary symbol; near P_max each symbol is a
  long computation, so the schedule there waits on the subquadratic symbol. Home:
  `issues/large-gcd-and-jacobi.md`, Notes ("deferred from `lucas-lehmer-test`").
- **The state to persist, as built** (TECH P4). The good states (iteration, shift, residue, length)
  are held in memory only; a failed check redoes up to 1/`GOOD_STATES` of the run, and the checkpoint
  file persists the same state. Home: `issues/checkpoint-and-resume.md`, Notes ("deferred from
  `lucas-lehmer-test`").
- **The landing gate as built** (TECH P4). `ci/engine-gate.sh`, the profile's `landing gate` row and
  what they compare; its time on this Mac is under `## Landing gate` below. Home:
  `issues/engine-differential-standing-check.md`, Notes ("deferred from `lucas-lehmer-test`").

### Homed at the GOAL sign-off

Already in their seeds since the shaping, listed so the ledger is whole: the checkpoint file, resume
and `ll` runs past the long tier (`issues/checkpoint-and-resume.md`); the verdict path's guards G1–G3
(`issues/verdict-path-guards.md`); a cumulative `make check` time baseline
(`issues/check-time-baseline.md`); a factor from a Jacobi symbol of 0 and a symbol that scales
(`issues/large-gcd-and-jacobi.md`); a Gerbicz-protected Lucas–Lehmer test
(`issues/prp-gerbicz-test.md`); an isolating check for the engine's magnitude refusal
(`issues/engine-differential-standing-check.md`); a shift option and a second shifted run
(`issues/burn-in-mode.md`, `issues/known-primes-campaign.md`); the engine's arithmetic, lengths and
speed (`issues/cpu-transform-throughput.md`, `issues/benchmark-mode.md`).

## Times

R16's two times are quoted from review cycle 2's dispatch A pre-push run (`remediation/prepush.sh`,
the frozen `verify/prepush.sh` under P2's wrapper, green on `0c45bc3`, the tested commit, on the
pushed branch, whose code equals this tree's), not from a second run; the check steps are from the
three runs' `result.json`, all of `0c45bc3` with `ci/test-commit.sh` blob `c4b1f28`, each passing
116 checks, 644 smoke-test cases and 48 `ll` cases with GCC 14. Each run was `COMPLETED 0:0` on its
first submission. The earlier rounds' runs, of `2530c06` and `df877de`, are superseded: each review
cycle's remediation changed the tree after them.

- make check: make check 4.974 s at bcb6fdf, 8.783 s now, +3.809 s (budget +5 s; least of 5 interleaved runs each; load 4.4)
- make check-long: 46 s on clang++ (budget 900 s)
- `x86-64-avx512`: check step 74 s
- `x86-64-avx2`: check step 115 s
- `aarch64-gb10`: check step 211 s

## Landing gate

From the same pre-push run: `ci/engine-gate.sh` on HEAD as it then was, `0c45bc3`, the tested
commit.

- engine-gate: green on 0c45bc31db7156c26bb9b0686438d622cf4038e7: make check-engine under Apple clang version 21.0.0 (clang-2100.3.34.2) and g++-16 (Homebrew GCC 16.2.0) 16.2.0, 5 residue digests equal

The pre-push lines, verbatim:

```
prepush: times for ## Times: - make check: make check 4.974 s at bcb6fdf, 8.783 s now, +3.809 s (budget +5 s; least of 5 interleaved runs each; load 4.4); - make check-long: 46 s on clang++ (budget 900 s)
prepush: landing gate for ## Landing gate: - engine-gate: green on 0c45bc31db7156c26bb9b0686438d622cf4038e7: make check-engine under Apple clang version 21.0.0 (clang-2100.3.34.2) and g++-16 (Homebrew GCC 16.2.0) 16.2.0, 5 residue digests equal
```

The gate's time on this Mac, from P3's full gate (its run mode on a commit of P3's tree, whose code
equals `2530c06`'s): 500 s under clang and 556 s under g++-16, one after the other, about 18
minutes. It was not measured again on `df877de` or `0c45bc3`: neither round's pre-push line carries
a time, and both remediations added check cases (review cycle 1's to `tests/check_lucas.cpp`,
review cycle 2's to it and to `tests/vectors/jacobi.txt`, which `tests/check_mp.cpp` reads), all of
which `make check-engine` runs, so the time on `0c45bc3` may differ somewhat.
