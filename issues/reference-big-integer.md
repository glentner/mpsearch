---
status: adopted:reference-big-integer
kind: feature
appetite: big
---

# A reference big integer to check every engine against

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

`mps` has no arithmetic on numbers wider than 64 bits. `BigInt` (`src/mp.hpp`, `src/mp.cpp`) keeps
decimal digits in a `std::vector<uint16_t>` and can only load and print them. Every test needs
arithmetic modulo 2^p − 1 on numbers of p bits: Lucas–Lehmer, PRP, the P-1 stages, and checking a
trial-factoring hit.

The fast engines that come later can't be trusted on their own. The transform engines are
floating-point or modular, with weights and carries, and they fail in ways that still produce
plausible residues. § Invariants 2 requires each engine to be tested against a reference, so the
reference has to exist first, and it has to be simple enough to be obviously right.

The author's uncommitted 2024 Lucas–Lehmer test reached for GMP (`mpz_class`, from a Rosetta Code
snippet). GMP conflicts with § Invariants 4 and 5, and it isn't included or linked, so the code
doesn't compile. The snippet couldn't land in any case. Its comment cites CC BY-SA 4.0, but Rosetta
Code's content is under the GNU Free Documentation License, which is incompatible with the GPL.

## Why it was deferred

Never started. It precedes every engine and test cycle: those cycles grade themselves against it.

## Outcome / vision

A binary-limb `BigInt` written for clarity, not speed. It provides:

- comparison, addition, subtraction and shifts;
- schoolbook and Karatsuba multiplication and squaring;
- reduction modulo 2^p − 1 by folding (x mod M_p = (x & M_p) + (x >> p), repeated);
- modular exponentiation, used to check a trial-factoring hit and to run small PRP tests;
- conversion to and from decimal, hex, and the balanced variable-base digit form the transforms
  use;
- the 64-bit residue (Res64).

Lucas–Lehmer and PRP run on it directly for small exponents, so the earliest tests have a known
answer before any transform exists.

## Sketch of the acceptance criteria

- **R1** — For every known Mersenne prime exponent up to a bound set at shaping (around 10,000),
  the reference Lucas–Lehmer test SHALL end in a zero residue.
- **R2** — For composite exponents in the same range, the reference test SHALL reproduce the
  residues in the known-answer tables (`issues/known-answer-tables.md`).
- **R3** — For random operands up to about 2^20 bits, products, squares and reductions SHALL equal
  vectors computed by an independent method and committed with the command that produced them.
- **R4** — The reference type SHALL round-trip exactly between its binary form, decimal and hex
  strings, and the transform digit form, for every exponent and transform length the tests use.
- **R5** — Res64 SHALL be the low 64 bits of the fully reduced residue in [0, M_p), printed as
  16 hexadecimal digits.

## Notes

- The independent method in R3 runs only when vectors are regenerated: Python's arbitrary-precision
  integers or GMP, under `test/`, and never linked into `bin/mps` (§ Invariants 5). `make check`
  reads the committed vectors.
- Shaping decides the limb width (32 or 64 bits). `unsigned __int128` is available on GCC and clang
  for every target in view.
- The exponent check names its factor symbolically, as `2^a-1` (`verdicts-and-exit-statuses`,
  2026-10-12); printing a large factor in decimal was left for this type's decimal conversion.
- Shaping decides whether to keep the class name `BigInt`. The decimal-digit stub has no callers.
- High-blast-radius: `src/mp.*` (§ Invariants).
- Found by: factory adoption review, 2026-10-11.
