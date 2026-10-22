# GOAL — Squaring modulo 2^p − 1 with a weighted transform (FP64, portable)

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** ibdwt-transform-engine
- **kind:** feature
- **appetite:** big

## Problem

Every primality test `mps` will run spends its time in one operation: squaring a p-bit residue
modulo M_p = 2^p − 1. The only arithmetic on `main` today (`f956a7c`) is the reference `BigInt`
(`src/mp.*`), with schoolbook and Karatsuba squares, written for clarity. `make check` runs the
reference Lucas–Lehmer and PRP tests on it only up to p = 10,000 (`REFERENCE_UP_TO`,
`tests/check_reference.cpp:56`), and it is hopeless at the exponents the burn-in and the known primes
need. The established method is Crandall and Fagin's irrational-base discrete weighted transform
(IBDWT): balanced digits of ⌈p/N⌉ or ⌊p/N⌋ bits, irrational weights, and a cyclic convolution of
length N that reduces modulo M_p for free. `doc/weighted-transforms.md` states and proves it
(Theorem 1), with Percival's proven error bound, Crandall and Fagin's conjectured error growth, and
the Crandall–Mayer–Papadopoulos sizing heuristic.

The method is only as good as its rounding. Every convolution output is rounded to an integer and is
right only while its error stays below one half. That depends on the length, the bits per word, the
accuracy of the twiddles and weights, and on the compiler leaving floating-point order alone. An
engine that rounds wrong still returns a residue, just a wrong one, so § Invariants 2 requires every
engine to be tested against the reference arithmetic on the known answers, to measure its round-off
and treat an excess as a fault, to compile without fast-math semantics, and to give the same residue
on every run. Percival's bound does not reach the lengths practice uses, so a length's largest safe
exponent has to come from measurement.

Every later Part II cycle (Lucas–Lehmer, PRP with Gerbicz, `mps bench`) builds on this engine, and
every later engine (the integer transform, the fast CPU kernels, the GPU backends) is checked against
the same answers through the same operations. What exists to build on, all pre-existing:

- `BigInt` is 64-bit limbs, little-endian, with no high zero limb, built and read through
  `BigInt::from_limbs` and `limbs()` (`src/mp.hpp:45`, `src/mp.hpp:47`). `reference-big-integer`
  deferred the balanced variable-base digit form and its round-trip with `BigInt` to this cycle (its
  GOAL, Non-goals; its P5 ledger, 2026-10-16).
- `tests/known/` holds, through `src/known.*`, the 52 Mersenne prime exponents, 11 published final
  residues and 20 partial residues keyed (op, p, start, k), the largest at p = 10,000,019. Partial
  rows are unshifted, so one row answers a run at any length and shift.
- The build already compiles every object with `-fno-fast-math -ffp-contract=off` (`IEEE`,
  `Makefile:8`).
- `doc/weighted-transforms.md`, "What mps uses", is stale: it says `BigInt` keeps decimal digits and
  can only load and print them, which `reference-big-integer` superseded.

## Outcome / vision

A portable FP64 weighted-transform engine, written for correctness rather than speed, squares
residues modulo M_p for every exponent from 3 up to what a length of 2^20 words holds (P_max, about
19 million). It converts residues to and from `BigInt` through the balanced digit form, folds the
Lucas–Lehmer subtraction (at any bit position, so a caller can run shifted) into the squaring,
multiplies two residues (the Gerbicz check's operation), reports the largest round-off distance of
every squaring and product, and refuses to return a residue past its threshold. A committed length table, set by measurement and kept with its data, says which length
each exponent runs at. The engine's operations and their semantics are written down once, as the
contract the later engines implement and are tested through.

The engine is proved against the reference: single squarings and products against `BigInt` at every
supported length, and squaring chains that reproduce the tables' partial and final residues and end at zero
for the Mersenne prime exponents. Nothing on the command line changes: `-t` still runs the stubs,
and no verdict depends on the engine until the Lucas–Lehmer cycle wires it in.

## Acceptance criteria (the contract)

Terms: a **length** is the transform length N; a **supported length** is N = m·2^k ≤ 2^20 with
k ≥ 0 and odd m ≤ 15; **P_max** is the largest exponent R3's table assigns to 2^20 words. The
**round-off distance** of a squaring or a product is the largest, over its N outputs, of |v − round(v)| for the
unweighted floating-point convolution output v. A **shifted chain** stores s·2^k for a shift k, and
its result is rotated back by k before comparison.

- **R1** — WHEN a residue 0 ≤ x < M_p is converted to the engine's balanced digit form at a supported
  length that holds p and back, the engine SHALL return x as a `BigInt`, for every such length and p
  the checks exercise, including x = 0, x = 1, x = M_p − 1 and digits at both ends of their balanced
  range. A digit vector that represents M_p SHALL read back as 0.
- **R2** — The engine SHALL accept every supported length (every odd m in 1, 3, 5, …, 15).
- **R3** — The length table SHALL give each supported length a largest exponent, set by measurement
  so that the largest round-off distance measured at that exponent is at most 0.35. The command that
  produced the measurement and its data SHALL be committed beside the table. For every exponent
  3 ≤ p ≤ P_max the table SHALL name the smallest supported length that holds p, and P_max SHALL be
  at least 10,000,019.
- **R4** — For every supported length N, at the smallest and the largest exponent R3's table assigns
  to N, the engine's squaring of pseudorandom residues, with no subtraction and with a subtraction
  of 2^e at positions that include e = 0, e = 1 and e = p − 1, SHALL return the residue `BigInt` computes (square, subtract, reduce
  modulo M_p).
- **R5** — Squaring chains on the engine, run at the length R3 names for p, SHALL reproduce:
  - every partial-residue row (`ll` and `prp`) in `tests/known/` with p ≤ P_max;
  - every final-residue row with p ≤ 100,069 (a PRP type-1 residue's division by 9 may run in the
    reference arithmetic);
  - a zero Lucas–Lehmer residue s_(p−2) for every Mersenne prime exponent 3 ≤ p ≤ 100,000 in
    `tests/known/`.

  Each row of the first two kinds SHALL also be reproduced at the next larger supported length, and
  by a shifted chain with a nonzero shift.
- **R6** — The engine SHALL report the round-off distance of every squaring it performs.
- **R7** — IF a squaring's round-off distance exceeds the configured threshold (0.4375 unless the
  caller sets another), THEN the engine SHALL raise an error naming p, N, the distance and the
  threshold, and SHALL leave the residue it held before that squaring unchanged.
- **R8** — IF the engine is asked for a length that isn't supported, or for an exponent the requested
  length doesn't hold by R3's table, THEN it SHALL raise an error naming the exponent and the length,
  and SHALL NOT return a residue.
- **R9** — Given the same exponent, length, input residue and subtraction position, the engine SHALL
  return a bit-identical residue and the same round-off distance on every run of one build, and the
  same residue from a clang build and a GCC build.
- **R10** — The engine's sources SHALL compile with the build's `IEEE` flags, and no flag, pragma or
  attribute in the repo SHALL weaken them for any engine source (§ Invariants 2).
- **R11** — The engine SHALL be standard C++20 alone: no SIMD intrinsics, inline assembly, threads, or
  compiler- or platform-specific extensions. Both clang and GCC SHALL build it and pass its checks.
- **R12** — The engine's checks SHALL run in two tiers. With nothing to rebuild, `make check` SHALL
  take no more than 5 s longer than at `f956a7c` on the same machine with clang, and its tier SHALL
  exercise every odd m, the shifted path and the product; the plan records the baseline it measured.
  A separate make target SHALL run every check R1–R9 and R14 name, in no more than 15 minutes on the same machine with
  clang. `bin/mps`'s command-line behaviour SHALL be unchanged: `tests/cli.sh` passes without edits.
- **R13** — In the commit that creates them, the engine's sources, the length table and its
  measurement data SHALL join the high-blast-radius list; `AGENTS.md` § Map SHALL name them and
  § Build and run SHALL name the R12 target; and `doc/weighted-transforms.md` SHALL describe the
  engine as built (its operations, which estimate guided the sizing, and where the measurement
  lives), with every concrete number it adds following `doc/README.md`'s marker rule and every
  algorithm cited where it is used.
- **R14** — WHEN the engine multiplies two residues at a supported length N, it SHALL return the
  residue `BigInt` computes (multiply, reduce modulo M_p), for pseudorandom operand pairs at the
  smallest and the largest exponent R3's table assigns to N, at every supported length. Each product
  SHALL report its round-off distance and refuse past the threshold, leaving its operands unchanged,
  as R6 and R7 require of a squaring, and SHALL be deterministic as R9 requires.

## Non-goals (no-gos)

- Multiplying by a small constant: `issues/pminus1-factoring.md`, its only consumer.
- Lengths above 2^20 and their table rows, the 7.5M words the largest known exponent needs included:
  `issues/cpu-transform-throughput.md`, which needs the portable engine as its reference at those
  lengths. Lengths with odd m above 15.
- What a test driver does with a fault: replaying a squaring from its saved input, telling a hardware
  fault from a reproducible excess, moving to a larger length for a block of iterations, logging the
  band between the sizing limit and the threshold, and retrying from a checkpoint
  (`issues/lucas-lehmer-test.md`, `issues/prp-gerbicz-test.md`).
- Choosing or tracking a shift, checkpoints, the Jacobi check, `mps -t ll` and `mps -t prp`, and any
  verdict (§ Invariants 1).
- Speed: no throughput criterion beyond R12's budgets; `mps bench` (`issues/benchmark-mode.md`);
  cache blocking, fused passes, threads and SIMD (`issues/cpu-transform-throughput.md`,
  `issues/cpu-isa-kernels.md`).
- Sizing by Percival's proven bound, and measuring adversarial worst-case inputs; the note says what
  the measurement covers.
- The integer transform, hybrid FP64+M31 hooks, and any GPU or TPU backend
  (`issues/ntt-transform-engine.md` and the backend seeds).
- Fingerprint rows (they need a second engine) and any edit to an existing row in `tests/known/`.
- Changes to `src/mp.*`, `src/reference.*` and `src/known.*`: the engine builds on their interfaces.
- Moving the R12 target into the default `make check` or the profile's `verify`, and building the
  standing check that runs it: `issues/engine-differential-standing-check.md` (Clarifications).

## Clarifications

- **Q:** Does the size circuit-breaker fire, and what is the pilot? — **A:** Yes: the seed's contract (load, square with the subtraction and shift, multiply by a small constant, multiply two residues, read back, report round-off) plus every length up to 7.5M words, a measured table across that range and the note is several deliverables. The pilot is the digit form, the transform, the squaring and the general multiply of two residues (R14, taken back in at sign-off so the contract is whole before PRP and its Gerbicz check), with the round-off monitor and a table up to 2^20 words; the small-constant multiply, the lengths above 2^20 and the driver's fault policy go to the seeds named in Non-goals. That leaves 14 R-IDs, over the soft limit of about 10 but one deliverable, an engine and its checks, of which § Invariants 2 alone accounts for R6, R7, R9 and R10. The slug stays (confirmed by the foreman, 2026-10-22).
- **Q:** Which lengths? — **A:** Every N = m·2^k ≤ 2^20 with odd m ≤ 15 (R2). The seed's Outline says "factors 3, 5 and 7" while its R5 says "odd m ≤ 15" and cites 11·2^19; its R5 holds, so 11 and 13 are in (confirmed by the foreman, 2026-10-22).
- **Q:** What does "correct" mean for the engine? — **A:** Three layers: the digit round-trip (R1); single squarings and products equal to `BigInt` at every supported length, at the ends of each length's exponent range (R4, R14); and squaring chains that reproduce every partial row up to P_max, every final row up to p = 100,069 (the four `ll` rows and `prp1 28603`), and zero residues for the 27 odd Mersenne prime exponents up to 86,243, with the rows re-run at a second length and shifted (R5). Final rows at 1,000,193 and above are full runs this engine is too slow for in a check (confirmed by the foreman, 2026-10-22).
- **Q:** What round-off policy, and what does this cycle own of it? — **A:** The engine measures every squaring's round-off distance (R6) and refuses past a configurable threshold, 0.4375 by default, leaving its prior residue intact so a caller can replay (R7). The length table sizes to a measured distance of at most 0.35 (R3). Replay, hardware-fault diagnosis, moving to a larger length and retrying from a checkpoint belong to the test drivers, which own the checkpoints; nothing in this cycle produces a verdict (confirmed by the foreman, 2026-10-22).
- **Q:** Portability and speed? — **A:** CPU only, scalar standard C++20, no SIMD, no threads, built by clang and GCC (R11). No speed criterion beyond R12's check budgets; speed is Part V's (confirmed by the foreman, 2026-10-22).
- **Q:** What does determinism cover? — **A:** Bit-identical residues and round-off distances across runs of one build, and identical residues across the clang and GCC builds (R9); the residue after unshifting doesn't depend on the length or the shift (R5). Round-off distances may differ between compilers, since only the rounded residue is exact (confirmed by the foreman, 2026-10-22).
- **Q:** Does the engine take the shift? — **A:** It takes the position of the bit the Lucas–Lehmer step subtracts (R4), which is all a shifted run needs from the squaring; choosing and tracking the shift is the Lucas–Lehmer cycle's. The checks run shifted chains to prove the path (R5) (confirmed by the foreman, 2026-10-22).
- **Q:** How does the engine return a residue and its Res64? — **A:** As a canonical `BigInt`, 0 ≤ r < M_p, with M_p read back as 0 (R1), and the Res64 taken from it as `src/mp.*` already does (confirmed by the foreman, 2026-10-22).
- **Q:** What is the timing budget? — **A:** Two tiers (R12): the default `make check` takes at most 5 s more than at `f956a7c` with nothing to rebuild (about 3.75 s on clang on the Mac the cycles have measured on), and a separate make target runs the full differential set in at most 15 minutes. The heavy target is not in the profile's `verify`; Geoffrey's ruling below sets how it stays enforced (confirmed by the foreman, 2026-10-22).
- **Q:** Does this cycle add rows to `tests/known/`? — **A:** No rows are needed: the tables already reach 10,000,019. The engine alone can't make fingerprints, which need two engines that share no arithmetic (ruling of 2026-10-17). If the plan finds a row missing, it appends it by the existing computed rule and the cycle's gates check the append against `main`, since the standing check now comes after Part III (confirmed by the foreman, 2026-10-22).
- **Q:** Which error estimate does the engine size by, and what does the note record (the seed's item from `math-notes-and-references`)? — **A:** The table is set by measurement (R3); the Crandall–Mayer–Papadopoulos heuristic and Crandall and Fagin's estimate only guide where to measure. The note names that, and points at the committed measurement rather than restating per-length numbers, as `doc/reference-arithmetic.md` does for the Karatsuba threshold; any number it does state takes markers (R13). If the real-input transform follows Sorensen et al. 1987, the plan reads it first and adds a located citation (confirmed by the foreman, 2026-10-22).
- **Q:** How does § Invariants 2's retry clause ("retries from the last good checkpoint at a larger transform length") apply to this cycle? — **A:** The engine measures, refuses at the threshold and preserves its input (R6, R7, R14). The retry at a larger length from the last good checkpoint binds the Lucas–Lehmer and PRP drivers, from the Lucas–Lehmer cycle on, when checkpoints and verdicts exist. No verdict exists before then, so nothing is loosened (Geoffrey, 2026-10-22).
- **Q:** How is the heavy differential target (R12) kept running once the cycle merges? — **A:** It stays out of the default `make check` and out of the profile's `verify`. A standing check enforces it, filed now as `issues/engine-differential-standing-check.md`, and the Lucas–Lehmer cycle's shaping decides its form before any verdict depends on the engine (Geoffrey, 2026-10-22).

## Related materials

- Seed: `issues/ibdwt-transform-engine.md`; `ROADMAP.md`, Part II, "Squaring modulo 2^p − 1 with a
  weighted transform (FP64, portable)".
- Earlier cycles: `spec/reference-big-integer/` (the digit-form deferral; `BigInt`),
  `spec/known-answer-tables/` (the tables, the fingerprint format), `spec/objects-follow-the-compiler/`
  (the build that follows `CXX` and `IEEE`).
- The mathematics: `doc/weighted-transforms.md` (Theorem 1, the error bounds), `doc/lucas-lehmer.md`
  (the shifted run), `doc/reference-arithmetic.md`, `doc/README.md` (the marker rule).
- Downstream seeds that take this cycle's deferrals or build on it: `issues/lucas-lehmer-test.md`,
  `issues/prp-gerbicz-test.md`, `issues/benchmark-mode.md`, `issues/pminus1-factoring.md`,
  `issues/cpu-transform-throughput.md`, `issues/ntt-transform-engine.md`,
  `issues/engine-differential-standing-check.md`.
- References (all in `doc/references.md`): Crandall and Fagin 1994; Percival 2003; Crandall, Mayer and
  Papadopoulos 2003; Sorensen et al. 1987.
