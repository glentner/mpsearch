---
status: unshaped
kind: feature
appetite: big
---

# `mps burnin`: known answers and expected walltimes for node acceptance

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Big-number squaring is one of the most demanding workloads a node can run. It keeps the FP64 (or
integer) units, the caches, the memory system and the power delivery saturated for hours, and every
bit of the result depends on every operation before it. The same arithmetic powers Prime95's torture
test. That test compares each run with a known-correct result, and its FFT sizes are chosen to
stress the L1/L2 caches, then L3, then the memory controller and DRAM.

Fleet studies show why this matters at acceptance time:

- About 1 CPU in 1,000 causes silent data corruption (Meta; Google), or 3.61 per 10,000 in
  Alibaba's 32-month study of over a million CPUs. Floating-point and vector units were the most
  implicated.
- Alibaba's largest catch came from testing at system re-installation, before production.
- Defects are often data-, frequency- and temperature-dependent. One fired only above 59 °C, and
  Google usually finds one bad core per CPU.
- One short burn-in is not enough. A few days of post-install testing caught only 12% of Google's
  defective machines.

`mps` can give a node two signals:

- **A known answer.** A run on a known exponent, or a fixed number of iterations at a fixed transform
  length, ends on a residue known in advance, and the shift randomises the data without changing the
  answer.
- **An expected walltime.** The same workload on the same node class should take the same time.

It has neither mode. There are no profiles, no expected answers wired to them, no walltime baselines,
and no record a fleet operator could compare.

## Why it was deferred

Never started. It needs PRP with the Gerbicz check, Lucas–Lehmer, `mps bench` and the known-answer
tables (Parts I–II). It comes straight after them because it is the first use the operators need.

## Outcome / vision

`mps burnin --profile NAME` runs a named set of workloads on the selected devices. Each workload is
an exponent, test, transform length and iteration count with an expected residue from the
known-answer tables, and a fresh random shift per run. Proposed profiles, calibrated at shaping:

- **quick** (~5 min): one single-threaded instance per core, all at once, alternating
  cache-resident sizes; every GPU on mid-size transforms. Run after a reboot or maintenance.
- **standard** (~1 h): a heat soak, then a per-core ladder from L1-resident to DRAM-filling sizes, a
  multithreaded run over at least half of RAM, and GPU runs that fill HBM. Run at return to service,
  and monthly.
- **soak** (~24 h): the standard cycle repeated on CPUs and GPUs together, with random exponents and
  shifts. Run at acceptance, after an RMA, and on suspect nodes.
- **diag** profiles: a single-core sweep at boost clock (a different frequency and voltage point),
  and a load/idle pulse for power-delivery triage.

On a burn-in, recovery is the wrong behaviour. Every internal check trip counts as a failure even if
the run would have recovered: a Gerbicz mismatch, round-off above threshold, a Jacobi failure. A run
writes one JSON record per workload plus a node summary, in a versioned schema:

- identity: build, git SHA and binary hash;
- scheduler context: Slurm job, HyperShell `TASK_*` variables;
- hardware: CPU model and microcode, BIOS, kernel, GPU model, VBIOS, driver;
- configuration: device, threads, test, residue type, exponent, transform, iterations, shift;
- outcome: expected and observed Res64, check counts, verdict;
- timing: median, p05, p95.

## Sketch of the acceptance criteria

- **R1** — WHEN every workload in a profile ends on its expected residue with no check trips,
  `mps burnin` SHALL exit with the pass status.
- **R2** — IF any workload ends on a different residue, or trips any internal check (Gerbicz,
  round-off, Jacobi), THEN `mps burnin` SHALL exit with the fault status and name the workload, the
  check and the residues, even if the run recovered.
- **R3** — WHERE a walltime baseline for the node class is supplied, IF a workload's time falls
  outside the baseline's band, THEN the run SHALL be flagged slow with its own exit status, even
  though its answer was right.
- **R4** — Every run SHALL write its records in the versioned schema, one file per node per run.
- **R5** — Every expected residue a profile uses SHALL come from the known-answer tables
  (§ Invariants 3), and every run SHALL use a fresh random shift.
- **R6** — WHERE the platform allows pinning, per-core workloads SHALL run pinned, and the record
  SHALL name the core, so a fault is attributed to it.
- **R7** — A test-only fault injection SHALL show that a corrupted residue fails the run with the
  fault status.

## Notes

- Scope, said plainly in the docs: `mps` complements, and doesn't replace, the vendors' GPU
  diagnostics (DCGM, ROCm RVS), STREAM, HPL, memory testers and network tests. It doesn't exercise
  tensor or low-precision units, interconnects, coherency paths, or ECC-corrected faults, which only
  counters reveal.
- macOS has no thread pinning (QoS classes only), so per-core attribution is Linux-only.
- Profiles grow per device as backends land (Parts IV, VI and VII); each backend cycle adds its
  profile.
- Fleet orchestration, telemetry and the outlier report are `issues/fleet-runs-and-reports.md`.
- References: H. D. Dixit et al., "Silent data corruptions at scale", arXiv:2102.11245, 2021; P. H.
  Hochschild et al., "Cores that don't count", HotOS 2021, doi:10.1145/3458336.3465297; S. Wang et
  al., SOSP 2023, doi:10.1145/3600006.3613149; S. Mitra et al., arXiv:2508.01786, 2025; Prime95
  `stress.txt`.
- Found by: factory adoption review, 2026-10-11.
