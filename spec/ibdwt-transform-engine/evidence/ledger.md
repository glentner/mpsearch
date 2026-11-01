# Deferral ledger for ibdwt-transform-engine

Written at P4, 2026-10-31, from a walk of `TECH.md` P1–P4 (the checklists and their build
amendments), the build commits `35d6d25`, `b29714d` and `77a99b0` and their bodies, GOAL's Non-goals
and Clarifications, PLAN § 3 (the deviations) and § 5 (the risks), and the P4 audit. Neither the
checklists nor the commits say "do not fix here", "known limitation" or "left as a follow-up", and no
build amendment defers anything; the entries below are the three TECH P4 names, and two the walk of
PLAN and the audit found. Each names its home: work a seed already covers goes in that seed's Notes,
and work no seed covers would get a seed of its own and a `ROADMAP.md` entry (none needed one, so
`ROADMAP.md` is as the GOAL sign-off left it). Every `issues/*.md` has exactly one `**Seed:**` line in
`ROADMAP.md`, and this cycle's seed stays `adopted:ibdwt-transform-engine`.

The P4 audit walked R1–R14 against `src/`, `tests/`, `doc/`, `AGENTS.md` and the `Makefile`, each to
its check or its gate step (PLAN § 2.9), and found no gap in what the cycle builds, so it committed
no fix. It found one stale sentence outside the cycle's files, the second deferral below.

## Deferrals

- **The plain, zero-padded product** (TECH P4; GOAL R14). The engine multiplies modulo M_p only;
  the half-GCD's product of operands of varying sizes, unweighted and zero-padded with no wrap, is
  not in it, so `ROADMAP.md` Part VIII's "depends on the transform engine's general multiply" is
  half true. Home: `issues/large-gcd-and-jacobi.md`, Notes ("deferred from
  `ibdwt-transform-engine`").
- **The real-signal transform** (TECH P4; PLAN § 3, the fourth deviation). The weighted signal is
  transformed as complex values with zero imaginary parts, twice the work of a real-signal or
  right-angle transform, for one code path at every length. Home:
  `issues/cpu-transform-throughput.md`, Notes ("deferred from `ibdwt-transform-engine`").
- **The suite as built, and the digests nothing compares after the merge** (TECH P4; GOAL R9 and
  R12). The standing check's seed now names the suite (`make check-engine`), what it runs, and its
  time on this Mac at P4; and that the clang and GCC residue digests R9 asks to agree are compared
  only by this cycle's gates, so the standing check should run both compilers and compare them.
  Home: `issues/engine-differential-standing-check.md`, Notes ("deferred from
  `ibdwt-transform-engine`").
- **A stale "What mps uses" in the integer-transform note** (the P4 audit). `doc/number-theoretic-transforms.md`
  says `mps` has no arithmetic wider than 64 bits and no transform of any kind; the first has been
  false since `reference-big-integer`, the second since this cycle. PLAN § 2.1 keeps every other
  file as `0bc49cb`'s, and the gate enforces it, so the cycle leaves the sentence alone. Home:
  `issues/ntt-transform-engine.md`, Notes ("deferred from `ibdwt-transform-engine`"): that cycle
  rewrites the section.
- **No check isolates the magnitude refusal** (PLAN § 5). The envelope refuses every input the
  checks and the gates build before an output reaches 2^51, so deleting the magnitude refusal leaves
  both tiers green (the gates' `no-guard` defect); it stays as defence in depth. Home:
  `issues/lucas-lehmer-test.md`, Notes ("deferred from `ibdwt-transform-engine`"), the first cycle
  to act on the engine's refusals.

### Homed at the GOAL sign-off

Already in their seeds' Notes since 2026-10-22, listed here so the ledger is whole: the multiply by a
small constant (`issues/pminus1-factoring.md`); lengths above 2^20 and odd m above 15
(`issues/cpu-transform-throughput.md`); the driver's fault policy (`issues/lucas-lehmer-test.md`,
`issues/prp-gerbicz-test.md`); the standing check itself (`issues/engine-differential-standing-check.md`
and its `ROADMAP.md` entry).

### Considered, no action

- **The guards are evidence, not proof** (PLAN § 5). The checksums and the envelope leave no wrong
  squaring in either sweep, and an adversarial input could still be built. The note says so, and the
  Jacobi and Gerbicz checks of `issues/lucas-lehmer-test.md` and `issues/prp-gerbicz-test.md` are
  the backstop those seeds already carry. Nothing is deferred.
- **False refusals** (PLAN § 5). A refusal is safe, never wrong, and none happened in the chains; the
  driver's move to a larger length is the fault policy already homed. Nothing is deferred.
- **Measurement on another machine** (PLAN § 5). The table check reads `src/lengths.txt`, not the
  machine, so `make check` holds elsewhere; only the cycle's gates compare the measurement byte for
  byte on this Mac. Nothing is deferred.
- **The engine tier's margin** (PLAN § 5). Within R12's 900 s at P4: 459 s alone on clang
  with nothing to rebuild (the seed's bullet), and the gate's own time beside other work under
  `## Times`. The first lever,
  should a later engine come near it, is the structured fractions above 2^14 words, never a length R2,
  R4 or R14 names. Nothing is deferred.

## Times
