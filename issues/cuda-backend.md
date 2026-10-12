---
status: unshaped
kind: feature
appetite: big
---

# NVIDIA GPUs: CUDA engines, FP64 where it's fast and integer where it isn't

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

NVIDIA's FP64 rate per SM per clock decides the engine:

- **FP64-rich: 32 or 64 per SM per clock.** A30, A100, H100, B200/GB200, Rubin. Suits the FP64
  weighted transform. These parts are bandwidth-bound at 2^22–2^23 words.
- **FP64-poor: 2 per SM per clock.** A10, L40S, GB10, the RTX and RTX PRO cards, and B300, whose FP64
  fell to about 1.25 TFLOPS. These have roughly 30× more integer multiply-add than FP64 throughput,
  and suit the integer transform. Gallot's rule of thumb: FP64 wins where FP64:INT32 is at least
  1:8, and the NTT wins at 1:16 or worse.

The bar is PRPLL-NTT, Woltman's 2026 line of gpuowl (GPL-3.0, CUDA and OpenCL), which mersenne.org
lists as the GPU primality test. At the largest known exponent it reports:

| GPU | Best path, µs/iter | Other paths, µs/iter |
|---|---|---|
| RTX PRO 6000 Blackwell | NTT 156.1 | FP64 849.6 |
| H100 | FP64+M31 182.0 | FP64 192.6, NTT 250.1 |
| A100-40GB | FP64+M31 331.5 | FP64 381.4, NTT 464.2 |

A two-round-trip model puts H100's FP64 bandwidth floor near 75 µs, so PRPLL's FP64 path there runs
at about 39% of it. Beating the incumbent on HBM parts looks plausible.

Practical constraints:

- CUDA 13 dropped every architecture below sm_75. `mul.hi` and 64-bit multiply rates are
  unpublished and need microbenchmarks.
- On compute capability 10.3 and 12.x, float→int conversion runs at only 2 per SM per clock, so
  round with the magic-constant add.
- `nvcc` contracts multiply-adds by default (`--fmad=true`), and `--use_fast_math` also flushes
  denormals and approximates division and square root.

## Why it was deferred

It follows the CPU engines (Parts II and IV), which fix the engine contract and the pass structure,
and the integer engine (Part V), which the FP64-poor parts need.

## Outcome / vision

`make BACKENDS=cuda` builds `mps` with three CUDA engines: FP64, FP64 + GF(M31²), and
GF(M31²)×GF(M61²). The engine is chosen per device by measurement or by option. Each squaring runs
in two fused kernels where the transform allows, and the residue stays on the device between
checkpoints. Lucas–Lehmer, PRP and the burn-in profiles run on every GPU in a node, one run per
device. The kernel sources are shared with HIP through a thin header (`issues/hip-backend.md`).

## Sketch of the acceptance criteria

- **R1** — For every exponent in the known-answer tables up to the shaping bound, each CUDA engine
  SHALL produce the same residues as the CPU engines.
- **R2** — On an A100 and an H100, `mps bench` time per iteration at the largest known exponent SHALL
  meet the bar `ROADMAP.md` sets against PRPLL-NTT on the same GPU, measured side by side. At 80%,
  that is at most 414 µs on A100 and 228 µs on H100, against the published figures above.
- **R3** — `make` without the CUDA backend SHALL need no CUDA toolkit.
- **R4** — WHEN a node has several GPUs, a burn-in SHALL run one workload per device and record each
  device's identity (UUID, VBIOS, driver).
- **R5** — Lucas–Lehmer SHALL verify a known Mersenne prime with an exponent in the tens of millions
  on an FP64-rich GPU, end to end.
- **R6** — Numeric kernels SHALL compile without `--use_fast_math`, with contraction controlled
  explicitly in the rounding and double-word code.

## Notes

- Shaping decides whether cuFFT may serve as a correctness oracle in tests. The fused kernels are
  `mps`'s own either way. cuFFTDx's licence bars open-source licensing of the combined work, so it is
  out.
- **Licensing.** GPLv3's system-library exception may not cover the proprietary CUDA runtime. The
  author chose to grant an additional permission under GPLv3 section 7 for it (2026-10-11), and
  `LICENSE-EXCEPTION` grants it. Kernels and host code are `mps`'s own.
- Reference: wfgarnett3, "PRPLL NTT now supports both CUDA and OpenCL", NVIDIA Developer Forums,
  2026, https://forums.developer.nvidia.com/t/prpll-ntt-now-supports-both-cuda-and-opencl/367918.
- **A cluster target.** The cluster test job (`ci/test-commit.sh`) shipped with CPU targets only;
  GPU targets were left to the backend cycles. This cycle adds a CUDA target under `ci/targets/`,
  with the GPU resources and the toolkit module it needs, so each phase is tested on an NVIDIA node.
- Found by: factory adoption review, 2026-10-11.
