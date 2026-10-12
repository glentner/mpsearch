---
status: unshaped
kind: feature
appetite: small
---

# Test any commit on a cluster, built fresh from GitHub

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Every target beyond the author's Mac runs on Slurm clusters:

- GCC on Linux x86-64, at the AVX2 and AVX-512 tiers;
- aarch64 Linux;
- NVIDIA GPUs through CUDA, and AMD GPUs through HIP;
- the burn-in itself.

Testing there by hand means interactive shells, builds in home directories, and leftover state from
the last attempt. That is unrepeatable, and a result means nothing unless it names exactly what was
built: which commit, which toolchain, which node. A cycle needs one mechanism it can call for any
target and any phase: push the branch, submit a job with the commit hash, read back a result.

## Why it was deferred

Never started. It follows the test target (`issues/build-and-test-foundation.md`), which gives the
job something to run.

## Outcome / vision

- **The job.** A batch script in the repo takes a commit hash and a target name. It:
  - clones the public repository into a new directory under the user's scratch space, keyed by hash,
    target and job id, and checks out exactly that commit;
  - loads the target's toolchain;
  - builds, then runs `make check` plus whatever the target adds (a GPU smoke test, a benchmark);
  - writes `result.json` (hash, target, node, compiler and runtime versions, each step's outcome and
    duration) and the logs beside it.
- **The targets.** Each target's settings live in one small file: partition, account, GPUs, time
  limit, modules, make variables. Adding a target touches only its file.
- **Submission.** A one-line wrapper submits a hash and a target. The factory drives submission and
  collection through the cluster's MCP server (Slurm submit, queue and accounting calls, and file
  reads), not ad-hoc shell sessions.

## Sketch of the acceptance criteria

- **R1** — WHEN submitted with a commit hash and a target, the job SHALL build that exact commit from
  a fresh clone of the public repository, in a new directory under scratch. IF the hash is not
  reachable from the repository, THEN it SHALL fail with a message saying so.
- **R2** — The job SHALL write a machine-readable result naming the hash, target, node, compiler and
  runtime versions, and each step's outcome and duration. It SHALL exit non-zero if any step fails.
- **R3** — Each target's cluster-specific settings SHALL live in one target file.
- **R4** — The job SHALL write nothing outside its own run directory, and SHALL NOT reuse an earlier
  build.
- **R5** — The job SHALL need only a POSIX shell, git and the target's toolchain.

## Notes

- First targets: GCC on an AVX2 node and on an AVX-512 node, and aarch64 on an NVIDIA GB10 node. CUDA
  targets arrive with `issues/cuda-backend.md`.
- Scratch results are disposable. A cycle copies the summary it relies on into its own records.
- The profile gains a row naming the job script in the commit that adds it (§ Conventions).
- Found by: the author's direction, 2026-10-11.
