# 00 — Digest

Six briefs (the sixth after the adversarial design review), written sequentially by the planner (no subagents in this delegated run; the skill's
portability path). Triggers: appetite big (full fan-out), trigger 2 (the change adds an engine,
whose paths join the high-blast-radius list), trigger 3 (the deliverable is checked by gates that
must not be hollow; the adversarial review runs at Step 5).

| Brief | Topic | Decision |
|---|---|---|
| [01](01-ibdwt-mathematics.md) | digits, weights, carry, subtraction | pieces as pinned free functions; exact fractions for weights; int64 carry with a bounded wrap; `from_digits` takes any digits (M_p is no balanced value) |
| [02](02-transform.md) | the transform | complex mixed-radix Cooley–Tukey for every m·2^k ≤ 2^20; roots alone, octant-reduced; explicit two-double arithmetic; no real-signal transform this cycle |
| [03](03-roundoff-and-the-table.md) | monitor and table | refuse at |v| ≥ 2^51 as well as past 7/16; size at ≤ 0.35 and < 2^50; grid of 1/16 bit to the first failure; data, table and command in `src/`; run once in P1 (42 min serial after 06's larger probes) |
| [04](04-checks-budgets-and-gates.md) | checks, budgets, gates | baseline 3.754 s; default +0.7 s; engine tier 7 min 43 s; wrapped pins, mutants, trace + sabotage, own probe, byte-for-byte re-measurement |
| [06](06-review-guards.md) | after the review | exact O(N) character checksums of the rounded outputs and an rms envelope from the measurement close the aliasing (0 wrong in both sweeps, no refusal on 899,830 real squarings); 64 squarings a probe and a 2-grid-point margin close the tail; P_max 20,316,160 |
| [05](05-repo-integration.md) | integration | `check::Tier` in one binary; `make check-engine`, `make lengths`; R13's lines all in P1; 7/16 in the note; two new references; the GCD gap for the ledger |

## Cross-brief contradictions, resolved

- **Where the measurement may run beyond the table (03) vs R8 (05).** Resolved by `Transform`
  (no residues) for the measurement and `Engine` (table-checked) for everything that returns a
  residue. Recorded in PLAN's deviation table and risks as a possible GOAL question.
- **The threshold's spelling.** The GOAL says 0.4375; `notes.py claims` refuses the integer 4375
  outside markers. Code keeps `THRESHOLD = 0.4375`; the note writes 7/16 (a definition, unmarked).
- **Default-tier budget vs "every supported length" (R2).** The default tier checks support for all
  144 lengths and builds engines up to 2^14; the engine tier builds and squares at all 144.
- **Speed (02) vs simplicity.** The complex transform costs 2× a real-signal one; the engine tier
  still fits with a 2× margin, so simplicity wins and the speedup is deferred to
  `issues/cpu-transform-throughput.md` (P4 ledger bullet).

## Sources read (clean room record)

Crandall and Fagin 1994 (pp. 309, 310, 315, 316 of the published scan); Cooley and Tukey 1965
(abstract and opening, via scite; AMS 403); Steele, Lea and Flood 2014 (metadata and abstract, via
scite); Sorensen et al. 1987 (metadata and abstract only; not used); Percival 2003 and
Crandall–Mayer–Papadopoulos 2003 through `doc/weighted-transforms.md`'s located statements (checked
against the papers in cycle 6); the repo's notes, sources and earlier specs. No source code of
Prime95/mprime, Mlucas, gpuOwl/PRPLL, GMP, FFTW or any other big-number or FFT program was opened.
