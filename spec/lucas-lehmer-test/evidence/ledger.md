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

R16's two times are quoted from dispatch A's pre-push run (`verify/prepush.sh`, green on `2530c06`,
the pushed and tested commit, whose code equals this tree's), not from a second run; the check steps
are from the three runs' `result.json`, all of `2530c06`, each passing 116 checks, 644 smoke-test
cases and 48 `ll` cases with GCC 14.

- make check: make check 5.013 s at bcb6fdf, 8.484 s now, +3.471 s (budget +5 s; least of 5 interleaved runs each; load 8.2)
- make check-long: 47 s on clang++ (budget 900 s)
- `x86-64-avx512`: check step 58 s
- `x86-64-avx2`: check step 121 s
- `aarch64-gb10`: check step 196 s

## Landing gate

From the same pre-push run: `ci/engine-gate.sh` on HEAD, the pushed and tested commit.

- engine-gate: green on 2530c06a79a9d3c83a4f08ba5612f3a89a3af359: make check-engine under Apple clang version 21.0.0 (clang-2100.3.34.2) and g++-16 (Homebrew GCC 16.2.0) 16.2.0, 5 residue digests equal

The pre-push lines, verbatim:

```
prepush: times for ## Times: - make check: make check 5.013 s at bcb6fdf, 8.484 s now, +3.471 s (budget +5 s; least of 5 interleaved runs each; load 8.2); - make check-long: 47 s on clang++ (budget 900 s)
prepush: landing gate for ## Landing gate: - engine-gate: green on 2530c06a79a9d3c83a4f08ba5612f3a89a3af359: make check-engine under Apple clang version 21.0.0 (clang-2100.3.34.2) and g++-16 (Homebrew GCC 16.2.0) 16.2.0, 5 residue digests equal
```

The gate's time on this Mac, from P3's full gate (its run mode on a commit of P3's tree, whose code
equals `2530c06`'s): 500 s under clang and 556 s under g++-16, one after the other, about 18
minutes. The pre-push line above carries no time.
