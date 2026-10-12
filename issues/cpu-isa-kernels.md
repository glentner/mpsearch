---
status: unshaped
kind: feature
appetite: big
---

# SIMD kernels for each CPU family, chosen at run time

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Throughput at cache-resident lengths, and the arithmetic share of every pass, depends on using the
vector units each CPU has. The targets reduce to three kernel sets:

- **NEON, written once for every aarch64 target.** Apple's M4 has no SVE outside streaming mode.
  Grace, GB10 and Graviton4 implement SVE2 at 128 bits with NEON's FMA rate, so SVE adds predication,
  not width. Use it for loop tails or gathers, detected through `HWCAP_SVE`.
- **x86-64-v3** (AVX2 + FMA) for Zen 3 EPYC Milan: 16 FP64 FLOPs per cycle per core.
- **x86-64-v4** (AVX-512) for Sapphire Rapids and Skylake-SP at 32 FLOPs per cycle, and for Zen 4
  Genoa. Zen 4 double-pumps 512-bit operations, so peak FLOPs equal its 256-bit rate: AVX-512 buys
  registers and masking there, not throughput, and 256 against 512 is worth benchmarking.

One binary has to run correctly on every node of a fleet with mixed CPU generations, and pick its
kernels at start-up. A binary built with `-march=native` on a login node is exactly wrong for a
heterogeneous cluster.

Untested kernels are dangerous. In September 2026 Mlucas reported AVX-512 builds silently computing
wrong results because one leading radix had not been ported.

The M4's SME2 unit was measured on 2026-10-11 and found not worth a kernel yet. FP64 outer products
reach about 1.0 TFLOP/s for the chip, against 0.51 TFLOP/s for NEON on all 14 cores. But a DFT
written as a matrix product costs 3–6× the FLOPs of an FFT, and streaming-SVE vector code runs at
only 15 GFLOP/s. Two toolchain traps turned up as well. `-march=armv9.2-a+sme2` let the vectorizer
emit SVE into ordinary code, which crashed with SIGILL; build with `-mcpu=apple-m4`. And Homebrew GCC
16 cannot build SME on macOS.

## Why it was deferred

It follows `issues/cpu-transform-throughput.md`, which fixes the pass structure the kernels implement.

## Outcome / vision

Each pass has a portable kernel plus a NEON kernel set and two x86 tiers, each compiled as its own
translation unit with its own flags (`-march=x86-64-v3`, `-march=x86-64-v4`, `-mcpu=…`). One `mps`
binary per OS and architecture picks the tier at start-up from CPUID or the hardware capabilities,
logs its choice, and records it in every result. Each tier produces the same residues as the portable
kernels, at every radix and length it supports.

## Sketch of the acceptance criteria

- **R1** — One `mps` binary per OS and architecture SHALL run on every CPU generation in the target
  list and select the widest supported kernel tier at start-up.
- **R2** — The selected tier SHALL appear in the log and in every result record.
- **R3** — For every exponent in the known-answer tables up to the shaping bound, each tier SHALL
  produce the same residues as the portable kernels, at every radix and length the tier supports.
- **R4** — IF a tier lacks a tested kernel for some radix or length, THEN `mps` SHALL use the portable
  kernel there and say so, never an untested path.
- **R5** — An option SHALL force a given tier, or the portable one, for testing and comparison.

## Notes

- Shaping decides between compiler intrinsics and portable vector types. Function multiversioning
  (`target_clones`) suits small leaf kernels, not whole FFT passes.
- SME stays parked: a later spike could try radix-4 outer-product blocks or exact int16→int64 limb
  products.
- Each tier is tested on a host that has it, through the cluster test jobs
  (`issues/cluster-test-jobs.md`), whose first targets are AVX2, AVX-512 and aarch64 GB10 nodes.
  There is no hosted CI matrix.
- Found by: factory adoption review, 2026-10-11.
