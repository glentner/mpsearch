---
status: unshaped
kind: feature
appetite: small
---

# Targets that check their node

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

A target file names the node class it was written for: x86-64 with AVX-512, x86-64 with AVX2, or
aarch64 with an NVIDIA GB10. `ci/test-commit.sh` checks only the partition. The two x86 targets
differ only in their GCC module and description, and both ask for partition `cpu`. One site can give
that name to AVX-512 nodes on one cluster and to AVX2-only nodes on another. A run on the wrong class
passes under the target's name. Its `result.json` records only the host and `uname -m`, so the record
cannot show the tier.

Evidence:

- `spec/cluster-test-jobs/REVIEW.md`, review cycle 1, F3: target `x86-64-avx512` passed when run on
  an arm64 Mac, and its `result.json` says `"arch": "arm64"`.
- `ci/test-commit.sh`, the toolchain step: the partition guard is the job's only check of the node.
- The target grammar is a closed key set (`description partition gpus cpus mem time modules
  make_vars`). The job refuses an unknown key, so a target has no way to state its CPU.

Today nothing builds for a tier, so a mislabeled pass is still a valid GCC build. That changes with
the SIMD kernels (`ROADMAP.md` Part V). One binary picks its tier at start-up, so an AVX-512 target
that lands on an AVX2 node would pass without running a single AVX-512 kernel.

## Why it was deferred

The review of `cluster-test-jobs` found this in its first cycle. That cycle had no R-ID asking the
job to check the node's CPU: its R8 lists the target's settings, and none of them is an ISA. Its real
runs did land on the classes their targets name. The foreman deferred the check rather than widen the
contract in remediation.

## Outcome / vision

A target can name the machine and the CPU features its node must have. The job checks them before it
builds and refuses a node that lacks one, so a target's name becomes a claim the job enforces. Its
`result.json` names the CPU that ran it.

## Sketch of the acceptance criteria

- **R1** — A target file MAY name its node's machine (as `uname -m` prints it) and a list of CPU
  features the node must report. The grammar SHALL stay plain `key=value`, read alike by a sourcing
  shell and by the job's line parser.
- **R2** — IF the node's machine differs from the target's, or the node lacks a named feature, THEN
  the job SHALL fail the toolchain step with a message naming what is missing, and run no later step.
- **R3** — `result.json` SHALL name the CPU model the run used. This is a format change, so `format`
  goes up.
- **R4** — The x86 targets SHALL name their tier's features (AVX2; AVX-512 Foundation), and the
  aarch64 target its machine.
- **R5** — WHEN the AVX-512 target is submitted to an AVX2-only node, the run SHALL fail at the
  toolchain step, as a cluster negative control.

## Notes

- Reading `/proc/cpuinfo` and `uname` keeps the job within cluster-test-jobs R10: a POSIX shell and
  its standard utilities. A GPU model check (`nvidia-smi`, `rocm-smi`) would not, so the GPU targets
  that arrive with the CUDA and HIP cycles need another answer.
- Related: `ROADMAP.md`, "SIMD kernels for each CPU family, chosen at run time"
  (`issues/cpu-isa-kernels.md`).
- Found by: `cluster-test-jobs` review cycle 1 (F3), 2026-10-12.
