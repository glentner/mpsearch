---
status: unshaped
kind: feature
appetite: big
---

# AMD GPUs: the GPU engines through HIP

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

AMD's Instinct GPUs have long been the strongest FP64 parts in HPC, and gpuowl was developed on AMD
hardware first.

| GPU | FP64 vector / matrix (TF) | Memory |
|---|---|---|
| MI210 | 22.6 / 45.3 | 64 GB HBM2e, 1.6 TB/s |
| MI250X | 47.9 / 95.7 | 3.2 TB/s |
| MI300X | 81.7 / 163.4 | 5.3 TB/s |
| MI355X | 78.6 / 78.6 | 8 TB/s; CDNA4 halved the matrix rate but kept the vector rate, so a vector-FMA FFT is as fast as on MI300X |

The next generation splits by market: MI430X is the FP64 part, and MI455X is an AI part with about
5 TF of FP64.

HIP is close enough to CUDA that one set of `.cu` sources can serve both, compiled by
`amdclang++ -x hip` through a thin header of macros. Conditions:

- **No fixed wave width.** Instinct parts through MI355X run 64-wide wavefronts, but RDNA and the
  MI400 generation run 32-wide. `warpSize` has not been a compile-time constant since ROCm 7, and lane
  masks are 64-bit.
- **No HIP-on-NVIDIA.** AMD no longer documents that path. CUDA builds use `nvcc`.
- **Contraction off.** HIP's default contraction (`fast-honor-pragmas`) must be set explicitly.
- **Measured multiply rates.** CDNA's integer multiply rate is undocumented: LLVM's model calls it
  full rate and warns it may be wrong. Measure it before choosing integer-engine moduli.
- **Pinned ROCm versions.** One ROCm upgrade raised gpuowl's time per iteration by 55%, so ROCm
  versions are recorded and pinned. Check each ROCm release's OS support against the target system.

## Why it was deferred

It follows `issues/cuda-backend.md`, whose kernels it reuses, and needs an AMD Instinct GPU to measure
on.

## Outcome / vision

`make BACKENDS=hip` builds `mps` with the GPU engines from the same sources as CUDA. On Instinct
parts the FP64 engine is the default. Residues match the CPU engines, and time per iteration is
measured beside PRPLL on the same AMD GPU, with the ROCm version recorded.

## Sketch of the acceptance criteria

- **R1** — For every exponent in the known-answer tables up to the shaping bound, the HIP engines
  SHALL produce the same residues as the CPU engines.
- **R2** — The CUDA and HIP builds SHALL compile the same kernel sources. Differences live only in
  the portability header.
- **R3** — No kernel SHALL assume a fixed warp or wavefront width.
- **R4** — On an Instinct MI210-class GPU, `mps bench` time per iteration on the yardstick exponents
  SHALL meet the bar `ROADMAP.md` sets against PRPLL on the same GPU.
- **R5** — Every result record SHALL carry the ROCm and driver versions.

## Notes

- Shaping names the AMD GPU to measure on and how to reach it.
- rocFFT may serve as a test oracle, as cuFFT may for CUDA. Its output is bitwise reproducible per
  GPU model, but no error bounds are published.
- Found by: factory adoption review, 2026-10-11.
