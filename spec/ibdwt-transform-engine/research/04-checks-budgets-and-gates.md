# 04 — Checks, the two budgets, and gates that cannot be hollow

> **Revised at the review (2026-10-23; finished 2026-10-31).** Added since: a second sabotage,
> `sabotage-top`, that reads every residue back off by 2^(c_(N−1)) so only bits above the low 64
> change, and the rule that every graded check prints W = K on every tally line under both (chains
> under the first only); every tally prints its line passing or failing; full residues pinned;
> the probe's structured families and full-residue sweep; digraphs spelled out before the scans and
> a disassembly check for fused multiply-adds; data-line counts and the gate's own measurement at
> the maxima; the no-checksum and no-envelope mutants. PLAN § 6 is current; times below are the
> pre-review ones.

**Question.** How do the checks prove R1–R9 and R14 inside R12's budgets, and how do the gates
prove the checks themselves are not hollow (lessons 3–6 of cycles 7–9)? Scope: the tiers' content
and cost, the R12 baseline, the gate's mechanisms and footprint. Inputs: the prototype (built
from this PLAN, in the durable `plan-proto/` beside the worktree's git dir), the closest models'
gates (`known-answer-tables`: its review caught a hollow recomputation gate; `reference-big-integer`:
the wrapper/interposer and trace driver; `objects-follow-the-compiler`: lanes, timing, cluster
evidence, and the deviation that bent R3).

## The R12 baseline (measured)

`make check` with nothing to rebuild at f956a7c, clang, M2 Mac: **least of 10 runs 3.754 s**
(median 3.770 s, load ≈ 4.6, another session active); interleaved with the prototype in 5 pairs,
3.759 s against 4.444 s: **+0.685 s** of the +5 s budget. The method is the gates' (least of N
interleaved, once more before failing), as cycle 7 and 9 measured.

## The default tier (+0.7 s in the prototype)

First draft took +4.6 s; two costs were the builder's to avoid and are now in the PLAN:
`supported_lengths()` must not loop over all n ≤ 2^20 (it reads the table: 144 entries), and a
check's `BigInt` value of a digit vector must place the digits' bits, not shift-and-add each one
(O(p) instead of O(N·p)). Building an engine at every length costs about 0.5 s (13 M roots), so
the default tier builds them only up to 2^14 and leaves the rest to the engine tier.

## The engine tier (7 min 43 s in the prototype, budget 15 min)

Per-check shares (prototype, clang, BOUNDS on, as `bin/check` is built): the `BigInt` products and
squares at p ≈ 20 M (≈ 2.4 s a square, more for a product) dominate the squaring and product
checks; the 100,069 final chain (100,067 squarings × 3 at its length, the next and shifted) and the
Mersenne zeros (≈ 250 k squarings) dominate the chains. Margin ≈ 2×. GCC's run is similar.

## Gate mechanisms, and what each defeats

1. **Pinned definitions, wrapped.** The gate renames ten pinned definitions in a copy
   (`fft::root`, `ibdwt::weights`, `roundoff`, `carry`, `require_held`; from P2 `to_digits`,
   `from_digits`, `square`, `square_subtract`, `multiply`) to `gate_real_*`, declares those, and
   links `interpose.cpp` into `bin/check`. Because the engine's own calls go through the pinned
   functions (pinned too), a wrapper reaches every use.
2. **Mutants** (`GATE_MUTANT`): wrong weights, a perturbed root, a dropped top carry, unbalanced
   digits, the subtraction's sign and place, a silent monitor, a product that squares, the table
   ignored, and a table that overshoots its measurement (a textual edit). Each must fail named
   labels in the default tier — the builder's checks are not hollow on the defects that matter.
   One build, ten runs of a few seconds.
3. **Trace + sabotage** (`GATE_TRACE`, `GATE_MUTANT=sabotage`): the interposed engine tier runs
   with every result of the engine nudged by one in digit 0 (still a valid residue) and every call
   counted by (operation, p, N, bit class). Every graded engine-tier check must fail (proving it
   compares what the engine returned against an independent value), and the counts must cover
   every case R1, R4, R5 and R14 name (proving the engine was called there). One extra engine-tier
   run, beside GCC's.
4. **The gate's own probe** (`probe.cpp`): independent of the builder's checks and helpers, folding
   modulo M_p itself; also the negative control for the monitor (top-of-range digits refused for
   their distance at a sample length; at 2^20 the outputs reach 2^57 and only the magnitude limit
   can refuse them — a deleted guard turned it red before the review; since the review the envelope
   refuses them first, so the magnitude refusal is no longer isolated, PLAN § 5).
5. **Re-measurement**: every boundary probe re-run by the builder's `bin/measure --probe` and
   compared byte for byte: the committed maxima are genuine, the data is the builder's engine's.
6. **Docs**: number words and digits in added lines, the table's values in any form, plus cycle
   6's `notes.py claims`.

## Footprint and run order

At most two jobs at once, each in its own process group, killed with it; deletion only inside the
gate's asserted `mktemp` directory. Full gate from P2: builds (2 compiles) → flags → probe (clang,
GCC) → mutants → clang engine tier timed beside one re-measure worker → GCC engine tier beside the
sabotaged engine tier → timing alone → docs. About 25 minutes (24 in the P3 dry run); quick mode about 2 minutes (clang
only, no engine tier, 16 boundary probes, the default tier sabotaged). After the review (2026-10-31):
47 minutes for P3's full gate, its engine tier 748 s and `make check` +2.4 s (on the `0bc49cb` base); quick about 3 minutes.

## Recommendation

As above; each phase gate is dry-run on a genuine phase tree (the prototype cut back to the
phase), in quick and full mode, and each check seen red once on a defective tree.
