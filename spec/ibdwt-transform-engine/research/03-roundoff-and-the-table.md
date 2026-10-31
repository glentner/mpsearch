# 03 — The round-off monitor and the length table: what is measurable, how to measure, the cost

> **Revised at the review (2026-10-23).** The monitor also refuses on exact checksums and an rms
> envelope, and the protocol now takes 64 squarings a probe and a 2-grid-point margin below the last
> held probe; the evidence and the new figures (P_max 20,316,160; 2,530 s CPU) are in
> [06](06-review-guards.md). The figures below are the pre-review ones.

**Question.** How do R3 (a table set by measurement, ≤ 0.35 at each maximum), R6/R7 (report every
distance, refuse past 0.4375) and § Invariants 2 ("treats an excess as a fault") turn into code
that cannot quietly lie? Where and how does the measurement run, what does it cost, and what is
committed? Scope: the monitor, the measurement protocol, the table, the measured cost.

## Sources read

- `doc/weighted-transforms.md` (f956a7c): Percival's bound does not reach practice's lengths; the
  Crandall–Mayer–Papadopoulos heuristic, eq. (8), with B_mant = 53 and C = 2 (≈ 19 bits a word at
  2^20); CF eq. (4.5)/(4.6) conjectures; Percival § 7, worst inputs far worse than random ones.
- `doc/reference-arithmetic.md`, "The threshold": the precedent for a design parameter set by
  measurement and pointed at, not restated.
- The prototype's own measurement (below). No program's source.

## Findings

1. **The distance is blind above 2^52 and coarse above 2^48.** \|v − round(v)\| is exact, but a double
   at 2^k has spacing 2^(k−52): at 2^50 the distance takes only the values 0, 1/4, 1/2; at 2^52 and
   above every double is an integer and the distance is always 0 — a wrong output reads as perfect.
   An input with every digit at the top of its range makes the outputs about N·2^(2β), 2^57 at the
   largest length: the monitor reports 0 and the engine would return a wrong residue. So the engine
   also refuses when any \|v\| ≥ 2^51 (`MAGNITUDE_LIMIT`, where spacing reaches 1/2), with the same
   `roundoff_error` and the residue untouched. This adds a refusal; it changes no R-ID's
   requirement (R7 still raises past the threshold; R4, R5 and R14's inputs never reach it), and it
   is what Invariant 2's "treats an excess as a fault" needs once the distance can't see the excess.
2. **The sizing also bounds the magnitude.** A probe holds only if its distance ≤ 0.35 *and* its
   largest \|v\| < 2^50 (`SIZING_MAGNITUDE`), one bit inside the refusal limit, so pseudorandom
   residues at a table exponent never trip the guard. In the planning measurement 142 of 144
   lengths failed by distance and 2 by magnitude; at the table maxima the magnitudes run from
   2^48.2 to just under 2^50 and the distances from 0 to 0.344.
3. **The protocol** (`src/measure.cpp`, `make lengths`): for each supported length, exponents
   ceil(N·i/16) on a grid of 1/16 bit a word, from 1 bit below the CMP eq. (8) estimate upward to
   the first probe that fails; if the first probe fails the start moves down a bit and the length is
   measured again. A probe squares at least 16 vectors of SplitMix64 pseudorandom balanced digits
   (seeded by N and p), fresh for each squaring, enough for ≥ 2^22 outputs. It never carries, so it
   forms no residue past the table (the first draft chained LL steps through `carry`, which put a
   residue past the table within reach of `Transform`; changed before the review). The table gives each length its last held probe. The data file carries the command, the
   protocol and, per probe, length, exponent, distance and magnitude printed with `%.17g`, so a
   re-run of any probe (`bin/measure --probe N p`) reproduces its line byte for byte (R9).
   The heuristic only picks the start (GOAL Clarifications).
4. **Result (prototype, M2 Mac, clang):** 2,909 probes, 2 to 30 per length; monotone maxima, so
   every length is assigned exponents; P_max = 20,512,768 at 2^20 (19.56 bits a word, above CMP's
   ≈ 19 because the sizing distance is 0.35, not 1/4). 1 word: 26 bits; 15 words: 23.3;
   1,024: 22.25; 65,536: 20.6. P_max ≥ 10,000,019 holds with room (R3).
   *These numbers are the planner's; the builder's engine makes its own data, which may differ.*
5. **Cost.** 1,038 s of CPU for all 144 lengths, 6 min 8 s wall on 3 workers (one length per
   process, deterministic, so a split run concatenated in length order is byte-identical to a
   serial `make lengths`). The large lengths dominate (2^20 alone: 50 s). Serial `make lengths`:
   about 18 minutes on an M2 Mac. It is a one-off, run by the builder in P1, never by `make check`.
   *Superseded at the review ([06](06-review-guards.md) § 5): with 64 squarings a probe, 2,530 s of
   CPU, about 42 minutes serial or 16 on three workers.*
6. **What the gate re-measures.** Every length's two boundary probes (the last held, the first
   failing): 288 probes, about 5 minutes on one worker, beside the timed clang engine tier. That
   proves each committed maximum held (R3's ≤ 0.35) and the next grid point failed (it is the
   largest), with the builder's binary. Quick mode re-measures 8 lengths' boundaries (16 probes).
7. **Tail risk.** A probe sees 2^22 outputs; a long chain at a table maximum sees far more, and its
   largest distance can creep past 0.35 toward 0.4375 (CF eq. (4.6) grows with N log N, and maxima
   of more samples grow). The 7/16 threshold is the margin. The prototype's R5 chains (100,067
   iterations at 100,069's length; 28,603 at 28,603's) all passed under 0.4375. If the builder's
   engine trips R7 in a chain, that is a finding about the sizing (more outputs per probe), not a
   reason to loosen the threshold.

## Recommendation

`src/lengths.{hpp,cpp}` (the table as `static std::map<std::uint64_t, std::uint64_t>
largest_exponent_by_length`, `SIZING_DISTANCE`, `SIZING_MAGNITUDE`), `src/lengths.txt` (the data),
`src/measure.cpp` (the command; `--table`, `--probe`). The engine's `MAGNITUDE_LIMIT` = 2^51
refusal goes in the PLAN's deviation table as added complexity that bends no R-ID. The note
describes the protocol and points at the files, stating none of their numbers.
