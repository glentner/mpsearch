---
status: adopted:math-notes-and-references
kind: docs
appetite: small
---

# The mathematics, written down: notes and references

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The author left a TODO in the uncommitted 2024 `src/math.cpp`: "Add comprehensive listing of notes
and references: 1) Math proofs 2) Algorithms and methods." No such listing exists. The clean-room
rule (§ Invariants 4) makes it load-bearing. Every algorithm in `mps` must come from the mathematics
and the published papers, cited where it is used. That needs one place where the citations and
derivations live, so a builder in a later cycle starts from the paper, not from another program.

## Why it was deferred

Never started. It belongs early: the transform-engine and test cycles cite it, and it is cheap.

## Outcome / vision

A `doc/` folder holds short notes, one per algorithm family:

- Mersenne numbers, and why p must be prime;
- the Lucas–Lehmer theorem;
- Fermat PRP tests and the Gerbicz check;
- the form of factors (q = 2kp + 1, q ≡ ±1 mod 8) and trial factoring;
- Pollard's P-1;
- weighted transforms (the IBDWT) and their error bounds;
- number-theoretic transforms.

Each note states the result, sketches or cites the proof, and names the papers. `doc/references.md`
lists every cited work with its DOI or URL. Later cycles extend the note for the algorithm they
implement.

Some results have no citable published statement and must be derived in the notes. One is the
Lucas–Lehmer Jacobi invariant: J(s_i − 2 | M_p) = −1 for every i ≥ 1, unchanged by shifts.

## Sketch of the acceptance criteria

- **R1** — `doc/references.md` SHALL list each work the code or notes cite: author, title, venue,
  year, and DOI or URL.
- **R2** — `doc/` SHALL hold one note per algorithm family above. Each SHALL state the result, give
  or cite its proof, and cite its sources.
- **R3** — No note SHALL quote or paraphrase another Mersenne program's source code
  (§ Invariants 4).
- **R4** — Each note SHALL record its provenance: the papers and documentation its algorithm was
  built from.

## Notes

- Shaping decides whether the notes are Markdown with LaTeX-style math (renders on GitHub) or plain
  text.
- A convention may follow from this: a cycle that implements an algorithm updates its note, and
  that note's provenance, in the same commit. Shaping decides whether to add it to § Conventions.
- Seed list for `doc/references.md`:
  - Crandall and Fagin 1994; Percival 2003; Crandall, Mayer and Papadopoulos 2003;
  - Lehmer 1930 and 1935; Bruce 1993;
  - Pietrzak 2019; Li and Gallot 2022;
  - Pollard 1974; Montgomery 1987; Montgomery and Silverman 1990; Montgomery and Kruppa 2008;
  - Möller 2008; Brent and Zimmermann 2010;
  - Reed and Truong 1975; Harvey 2014;
  - Montgomery 1985; Barrett 1987.
  The seeds in `issues/` give full citations.
- Found by: factory adoption review, 2026-10-11 (the author's 2024 TODO).
