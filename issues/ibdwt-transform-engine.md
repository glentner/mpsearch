---
status: unshaped
kind: feature
appetite: big
---

# Squaring modulo 2^p − 1 with a weighted transform (FP64, portable)

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Every test spends its time in one operation: squaring a p-bit residue modulo 2^p − 1. Schoolbook
squaring is O(p²) and hopeless at p in the hundreds of millions. The established method is
Crandall and Fagin's irrational-base discrete weighted transform (IBDWT, 1994). Split the residue
into N words of ⌈p/N⌉ or ⌊p/N⌋ bits, and multiply word j by a weight 2^(⌈jp/N⌉ − jp/N). A cyclic
convolution of length N then performs the reduction modulo 2^p − 1 for free. With FP64 FFTs and
balanced digits it carries about 18–19 bits per word at a million words, losing roughly a quarter
bit per doubling. The largest known exponent, 136,279,841, fits 7.5M words (15·2^19) at 17.3 bits
per word. The Crandall–Mayer–Papadopoulos sizing heuristic lands within 0.3–0.6 bits per word of the
limits the established programs document. Rigorous worst-case bounds (Percival 2003) would cost
1.65–2× the transform length, so practice is heuristic sizing plus monitoring.

The method is only as good as its rounding. Each output is rounded back to an integer, and every
convolution output must land within 0.5 of an integer. That depends on the transform length, the
bits per word, the accuracy of the twiddle factors and weights, and the compiler leaving the
floating-point order alone (§ Invariants 2). An engine that rounds wrong still produces a residue,
just a wrong one.

Nothing of this exists yet, and no engine contract exists for the CPU, GPU and TPU backends to share.

## Why it was deferred

Never started. It is the core of Part II and needs `issues/reference-big-integer.md` to test against.

## Outcome / vision

The deliverable is an engine contract plus its first implementation, written for correctness and
portability rather than speed.

- **The contract** covers: load a residue; square it, with the Lucas–Lehmer "− 2" and the shift
  folded in; multiply by a small constant; multiply two residues (P-1 and the Gerbicz check need
  it); return the canonical residue and its Res64; report the round-off error.
- **The implementation** is scalar C++ FP64, for power-of-two lengths and for lengths with factors
  3, 5 and 7.
- **A length table** maps each transform length to its largest safe exponent. It is set by
  measurement, with a margin, and kept with the data that justify it.

Faster CPU kernels (Part V), the integer transform (Part IV) and the GPU backends implement the same
contract and are tested against the same answers.

## Sketch of the acceptance criteria

- **R1** — For every exponent in the known-answer tables up to the bound set at shaping, a squaring
  chain on the engine SHALL match the reference big integer's residue at every compared iteration.
- **R2** — The engine SHALL report the largest round-off distance it saw. IF that distance exceeds
  the configured threshold, THEN it SHALL raise an error instead of returning a residue.
- **R3** — Each supported transform length SHALL have a maximum exponent set by measurement, and the
  measurement data SHALL be committed beside the table.
- **R4** — Given the same input, length and shift, the engine SHALL produce bit-identical output on
  every run.
- **R5** — The engine SHALL accept every length m·2^k with odd m ≤ 15 between the bounds set at
  shaping. The upper bound SHALL cover the largest known Mersenne prime exponent. Intermediate
  lengths pay: 7.5M instead of 8M saves 6% for that exponent, and 5.5M instead of 8M saves 31% near
  p = 100M.
- **R6** — Numeric sources SHALL compile without fast-math semantics (§ Invariants 2).

## Notes

- Expected rabbit holes for research: the rounding-error bound (Percival 2003) against measured
  error; twiddle and weight accuracy; right-angle convolution (a real-input transform via a
  half-length complex one); carry propagation; non-power-of-two radices.
- Shaping decides the round-off policy. Documented practice: Prime95 keeps errors below 0.4 and
  repeats an iteration above it; Mlucas treats 0.4375 as dangerous and moves to a larger length.
  One proposal: normal below 0.35; log from 0.35 to 0.4375; at 0.4375 replay the iteration from its
  saved input, and treat a replay that differs as a hardware fault; if the excess reproduces, use the
  next length for a block of iterations.
- Twiddles and weights are generated in double-double from exact rational angles, never by
  recurrence. Published stability analyses of twiddle recurrences have been wrong.
- High-blast-radius: the engine's paths join § Invariants in the commit that creates them.
- References: R. Crandall and B. Fagin, "Discrete weighted transforms and large-integer arithmetic",
  *Mathematics of Computation* 62(205), 1994, doi:10.1090/S0025-5718-1994-1185244-1; C. Percival,
  "Rapid multiplication modulo the sum and difference of highly composite numbers", *Mathematics of
  Computation* 72(241), 2003, doi:10.1090/S0025-5718-02-01419-9; R. Crandall, E. Mayer and
  J. Papadopoulos, "The twenty-fourth Fermat number is composite", *Mathematics of Computation*
  72(243), 2003, doi:10.1090/S0025-5718-02-01479-5 (the sizing heuristic); H. V. Sorensen et al.,
  "Real-valued fast Fourier transform algorithms", *IEEE TASSP* 35(6), 1987.
- Open from `math-notes-and-references` (its P5 ledger, 2026-10-12). `doc/weighted-transforms.md`
  states Percival's bounds as theorems, Crandall and Fagin's error growth (their eq. (4.5) and (4.6))
  as their conjecture, and the Crandall–Mayer–Papadopoulos sizing as a heuristic. Only Percival's
  bound is proved, and it does not reach the lengths practice uses. Which estimate the engine sizes by,
  and its measured largest rounding error at each length against them, are this cycle's to settle and
  to record in the note. Sorensen et al. 1987 was read only in its abstract (no open copy) and is cited
  without a locator: if the engine builds its real-input transform from it, read it first and add a
  located citation.
- The balanced digit form, deferred from `reference-big-integer` (its GOAL, Non-goals; its P5
  ledger, 2026-10-16). The balanced variable-base digit form the transform works in (N words of
  ⌈p/N⌉ or ⌊p/N⌋ bits, as Problem describes, each digit in a balanced range), and its round-trip with
  `BigInt` in both directions, are this cycle's. That cycle built `BigInt` as 64-bit limbs,
  little-endian, with no high zero limb (`src/mp.hpp`), so the conversions build on
  `BigInt::from_limbs` and `limbs()`, and R1's comparison against the reference goes through them.
- Found by: factory adoption review, 2026-10-11.
