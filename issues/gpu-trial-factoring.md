---
status: unshaped
kind: feature
appetite: big
---

# Trial factoring on GPUs

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Trial factoring is integer work: sieve the candidates, then compute 2^p mod q for q of 64 to 96 bits.
GPUs do it far faster than CPUs, which is why GIMPS's trial factoring moved to GPUs long ago. Today
that means mfaktc 0.24 on CUDA and mfakto on OpenCL (AMD, Intel, NVIDIA and macOS's OpenCL), both
GPL-3.0, both sieving on the GPU with Barrett-reduction kernels for 64–92-bit factors. GPU factoring
runs about four bits deeper than Prime95's CPU defaults.

It needs no FP64, so it runs well on every GPU in view, the Apple GPU included. PrimePath does it in
Metal on Apple silicon (MIT, 2026), though its candidates-per-second figures aren't comparable with
mfaktc's. It is also a second burn-in stress profile: integer units and on-chip memory instead of
FP64 and DRAM.

## Why it was deferred

It follows the CPU trial factoring (`issues/trial-factoring.md`), which fixes the class and sieve
structure and the answers to test against. It also needs the GPU backends for the device plumbing.

## Outcome / vision

`mps -t tf` runs on the selected GPU in Metal, CUDA and HIP. It finds the same factors as the CPU
path, re-checked on the CPU, and reports candidates per second. A GPU trial-factoring burn-in profile
exists, with known factors as its expected answers.

## Sketch of the acceptance criteria

- **R1** — For every factor case in the known-answer tables, GPU trial factoring SHALL report the
  same factors as the CPU path, re-checked on the CPU.
- **R2** — Each surviving class SHALL be tested exactly once, as on the CPU path.
- **R3** — On an NVIDIA GPU, `mps bench` SHALL report candidates per second, and that rate SHALL be
  compared with mfaktc on the same GPU at the same bit level.

## Notes

- GIMPS's trial-factoring results now carry a CRC32 checksum. It matters only if results are ever
  submitted.
- References: mfaktc README (primesearch/mfaktc); mfakto README (primesearch/mfakto); P. Barrett,
  CRYPTO '86, LNCS 263, 1987.
- Found by: factory adoption review, 2026-10-11.
