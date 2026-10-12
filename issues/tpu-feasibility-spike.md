---
status: unshaped
kind: spike
appetite: big
---

# TPU v7: can a matrix machine square modulo 2^p − 1?

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Google's TPU v7 (Ironwood, generally available since 2026-03-31) is built for low-precision matrix
work:

- **Compute.** Two TensorCores per chip, 256×256 matrix units with BF16 (2,307 TF) and FP8
  (4,614 TF) peaks, and 192 GiB of HBM at 7.38 TB/s.
- **No FP64.** XLA's TPU FFT is complex64 only, and complex128 is rejected.
- **A 32-bit vector unit.** The hardware "generally only supports elementwise computation using
  32-bit types", and Pallas documents no `mulhi`.
- **Unpublished integer rates.** Earlier TPUs had int8 matrix modes. Ironwood's new low-precision
  path is FP8, and Google publishes no INT8 rate for it.

The one prior attempt, TensorPrime (JAX, bfloat16, 2021–22), stopped at p = 4,423 for lack of 64-bit
types.

Three routes are conceivable:

1. **The integer transform on the vector unit.** GF(M31²) reduces with shifts and adds, and its
   32-bit multiplies need a high-word product, which may have to be emulated.
2. **Matrix-unit NTT blocks with exact int8 limb splitting.** The CROSS line of work does this for
   FHE. On TPU v6e it beats an A100 library at N = 2^12 but loses from 2^13. A rough estimate for our
   case is about 5·10^11 multiply-accumulates per squaring, at least 17 hours for the largest known
   exponent before any epilogue: slower than an RTX 5090.
3. **Emulated higher precision on the matrix unit (the Ozaki scheme).** It works for GEMM, but FFTs
   built this way have been bound by their integer epilogue.

None is known to be competitive, and a backend can't be planned until one is shown to be correct.

The C++ constraint holds. The route from C++ is `dlopen("libtpu.so")` → `GetPjrtApi()`, feeding the
PJRT C API StableHLO programs emitted by C++. libtpu is a proprietary, x86-64-only wheel whose ABI is
pinned to the PJRT version.

## Why it was deferred

The author's decision (2026-10-11): an exploration after the other targets reach their goals. It
also needs the integer engine and the engine contract, and access to TPU v7: `tpu7x` machine types on
GKE or Compute Engine.

## Outcome / vision

A bounded spike, from a C++ host on a TPU v7 VM. It first measures 32-bit multiply and high-product
throughput on the vector unit, and the int8 matrix path if one exists. It then takes the most
promising route far enough to:

- square modulo 2^p − 1 correctly at small lengths;
- reproduce known Lucas–Lehmer answers for small exponents;
- measure time per iteration at one or two large lengths.

It ends with a written go or no-go. A go comes with a seed for a TPU backend, its expected
performance, and the reasons. A no-go is recorded under *Settled questions* with what would reopen
it.

## Sketch of the acceptance criteria

- **R1** — The spike SHALL produce a C++ program that runs on a TPU v7 VM, squares modulo 2^p − 1
  through one route, and matches the CPU engines' residues for small known exponents.
- **R2** — It SHALL report the measured vector-unit integer rates, and the matrix-unit integer rate
  if available.
- **R3** — It SHALL measure time per iteration at the lengths set at shaping, and compare them with
  the CPU and GPU engines and with the machine's bandwidth roofline.
- **R4** — It SHALL end with a written decision, its evidence, and either a backend seed or a
  settled-question record.

## Notes

- `kind: spike`: the deliverable is evidence and a decision. The code may be throwaway.
- Shaping confirms how to reach TPU v7, and the budget.
- Licensing: linking proprietary libtpu raises the same GPLv3 question as the CUDA runtime
  (`issues/cuda-backend.md`).
- References: Google Cloud, "TPU7x (Ironwood)" documentation; OpenXLA, PJRT C API
  (`pjrt_c_api.h`); T. Lu et al., "Large-scale discrete Fourier transform on TPUs", *IEEE Access* 9,
  2021, doi:10.1109/ACCESS.2021.3092312; K. Ozaki, Y. Uchino and T. Imamura, "Ozaki Scheme II",
  arXiv:2504.08009, 2025; TensorPrime (TPU-Mersenne-Prime-Search/TensorPrime).
- Found by: factory adoption review, 2026-10-11.
