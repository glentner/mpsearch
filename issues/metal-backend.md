---
status: unshaped
kind: feature
appetite: big
---

# The Mac's GPU: an integer engine in Metal

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The first GPU target is the Apple M4 Max on the author's desk: 32 GPU cores and 36 GB of unified
memory, so there is no host-to-device copy. Measured on 2026-10-11:

- **No FP64.** Metal has no `double`, and the compiler rejects it.
- **Arithmetic.** FP32 fused multiply-add sustains 11.0 TFLOP/s. 32-bit integer multiply runs at
  1.60 T/s, `mulhi` at 0.81 T/s and 64-bit multiply at 0.40 T/s. A modular multiply by shift and add
  in GF(2^31 − 1) reaches 0.52 T/s, and a 31-bit Montgomery multiply 0.26 T/s. The budget is set by
  `mulhi`, so moduli that reduce with shifts and adds win. FP32 and integer work issue concurrently,
  so FP32 + integer hybrids are worth a test.
- **Memory.** Copy bandwidth is 324 GB/s, 79% of the part's 410 GB/s. Threadgroup memory is 32 KiB,
  so a 2^23-point transform needs at least two global passes.
- **Power.** About 34 W at an FP32 burn, 27–29 W on integer multiplies.

The toolchain was tested the same day, with Command Line Tools only, no Xcode:

- metal-cpp (Apple's header-only C++ interface to Metal, Apache-2.0) built against the macOS SDK and
  ran.
- A kernel compiled at run time with `newLibraryWithSource` was bit-exact over 2^20 modular
  squarings.
- An eight-kernel library compiled in 14 ms. Kernel source can be embedded with C++ `#embed`.
- The Metal Shading Language rejects C++20 concepts, so headers shared with kernels must stay
  C++17.
- Metal's `fastMathEnabled` defaults to true and must be replaced by `mathMode = .safe`.

The established program on Apple GPUs is PrMers, which runs PRP through OpenCL at about 6.1 ms per
iteration on an M4 Pro at the largest known exponent: 9.6 days per test. PRPLL's integer transform
gained an OpenCL path without FP64 in September 2026 and may also run on macOS's OpenCL 1.2. Native
Metal PRP engines were reported emitting GIMPS-format results in September 2026. There is no Metal
Lucas–Lehmer tester.

## Why it was deferred

It needs the integer engine proven on the CPU first (`issues/ntt-transform-engine.md`), plus the
engine contract and the tests that check it.

## Outcome / vision

`make BACKENDS=metal` builds `mps` with a Metal engine, selected with a device option. The host is C++
on vendored metal-cpp; kernel source is embedded and compiled at start-up, with pipeline states
cached.

The cycle starts with a microbenchmark spike that chooses the engine: GF(M31²)×GF(M61²), or an
FP32 + GF(M31²) hybrid, or both, measured against the roofline. Lucas–Lehmer, PRP and the burn-in
profiles then run on the GPU, matching the CPU engines' residues. `mps bench` reports time per
iteration beside the roofline and beside PrMers, and PRPLL where it runs, on the same Mac. The CPU
build stays free of any Metal dependency.

## Sketch of the acceptance criteria

- **R1** — For every exponent in the known-answer tables up to the shaping bound, the Metal engine
  SHALL produce the same residues as the CPU engines.
- **R2** — `make` without the Metal backend SHALL build no Metal code and link no Apple GPU
  framework.
- **R3** — WHEN built on a machine with only the Command Line Tools, `make BACKENDS=metal` SHALL
  succeed, and `mps` SHALL compile its kernels at start-up with safe math.
- **R4** — `mps bench` on the Metal engine SHALL report time per iteration on the yardstick
  exponents, the fraction of the memory-bandwidth roofline achieved, and the ratio to PrMers on the
  same Mac, and SHALL meet the bar `ROADMAP.md` sets.
- **R5** — Lucas–Lehmer SHALL verify a known Mersenne prime with an exponent in the millions on the
  GPU, end to end, with checkpoints.

## Notes

- Vendoring metal-cpp (Apache-2.0) is allowed only with the author's approval and its licence and
  notices kept (§ Invariants 4). Apache-2.0 code may be included in GPLv3 works. metal-cpp has no
  tagged releases, so newer APIs are gated at run time. Objective-C++ is the fallback.
- MPS and MPSGraph FFTs are FP32/FP16/BF16 only, and cannot square exactly.
- Energy: IOReport gives per-rail energy without root on macOS 26 (entitlement-gated on 27);
  `powermetrics` needs root.
- References: Apple, *Metal Shading Language Specification* and `MTLCompileOptions` (`mathMode`);
  apple/metal-cpp; P. Turner, metal-benchmarks (Apple GPU microarchitecture); PrMers README
  (cherubrock-seb/PrMers).
- Found by: factory adoption review, 2026-10-11.
