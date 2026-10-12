---
status: unshaped
kind: feature
appetite: big
---

# Fast on CPUs: cache-blocked, fused, multithreaded transforms

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The portable engine from Part II is built to be right, not fast. At the lengths that matter, about 4
to 8 million words, one squaring costs about a GFLOP. A naive transform streams the whole 64 MiB
array through memory once per radix pass. The established CPU programs do something else:

- a four-step or six-step decomposition sized to the caches;
- the weights, the pointwise squaring and the carry fused into as few sweeps as possible;
- all of it run across every core.

Two DRAM round trips per iteration is the design target: forward DIF, inverse DIT, no bit reversal.

Measured on the M4 Max (14 cores: 10 performance, 4 efficiency):

- FP64 fused multiply-add reaches 56 GFLOP/s per performance core and 469 GFLOP/s on ten threads;
- a STREAM-style triad reaches 263–275 GB/s of the part's 410 GB/s.

The compute and bandwidth floors for a 7.5M-word squaring both land near 2 ms per iteration, so the
engine has to be good at both.

On x86 the bar is mprime. One published comparison, on a two-socket 64-core AVX-512 server at
6144K, has mprime at 2,318 iterations per second (one worker per socket) and Mlucas at 1,117 (48%).
On Arm the only established program is Mlucas, which its own README places at 50–90% of Prime95's
speed. `ROADMAP.md` sets the bars against these programs.

## Why it was deferred

Correctness comes first (Part II). This entry also depends on `issues/benchmark-mode.md`, which
measures it.

## Outcome / vision

The CPU engine approaches its memory-bandwidth roofline at large lengths and stays compute-efficient
at cache-resident ones. It does so with:

- a cache-aware decomposition, with lines of 2^11–2^12 points worked on-chip;
- two fused sweeps per iteration;
- a thread pool with NUMA-aware first-touch allocation and pinning on Linux, and QoS classes on
  macOS, which offers no pinning.

Worker and thread layouts are searched, not assumed. Every optimisation is checked against the
portable engine's residues (§ Invariants 2), and the round-off monitor stays on.

## Sketch of the acceptance criteria

- **R1** — On the M4 Max, `mps bench` whole-chip throughput on the yardstick exponents SHALL reach
  the bar `ROADMAP.md` sets against Mlucas on the same machine.
- **R2** — On an AMD EPYC node, once `issues/cpu-isa-kernels.md` provides the x86 kernels, throughput
  SHALL reach the bar `ROADMAP.md` sets against mprime, each program in its best layout.
- **R3** — For every exponent in the known-answer tables up to the shaping bound, the optimised engine
  SHALL produce the same residues as the portable engine.
- **R4** — The run SHALL report bytes moved per iteration, measured or modelled, so a reviewer can
  compare the time with the memory-bandwidth roofline.

## Notes

- Expected research:
  - the pass structure for unified memory (M4 Max) and for multi-socket DDR5 (EPYC Genoa, 12 channels
    per socket);
  - per-CCD placement on V-Cache parts, where one CCD's 96 MB L3 holds the whole array;
  - per-socket placement on Sapphire Rapids, whose 105 MB L3 does too.
- The established programs' published timings set the yardsticks. Their code is out of bounds
  (§ Invariants 4).
- The logger's thread safety is this seed's to decide: its one shared state was built for a
  single-threaded `mps` (`spec/logger-state-per-translation-unit/GOAL.md`, Non-goals).
- Found by: factory adoption review, 2026-10-11.
