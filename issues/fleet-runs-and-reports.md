---
status: unshaped
kind: feature
appetite: big
---

# Fleet runs: telemetry, batch-scheduler recipes, and the outlier report

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

A burn-in record from one node answers "is this node right?". An operator returning hundreds of nodes
to service also needs to know which nodes are slower than their peers, and why. That takes three
things `mps` doesn't have.

**Telemetry in each record.** Most of it is readable without root:

- NVIDIA's management library reports energy in millijoules since driver load, plus ECC counts and
  remapped rows; `amd-smi` covers AMD GPUs.
- Linux hwmon gives temperatures and EDAC gives memory error counts. The microcode revision is
  world-readable.
- On macOS, per-process energy (`proc_pid_rusage`) and the IOReport energy counters read without
  root on macOS 26. macOS 27 adds an entitlement gate, and `powermetrics` needs root.

The gap is Linux CPU energy. RAPL counters have been root-only since CVE-2020-8694, so CPU energy
comes from the scheduler's accounting (Slurm's `ConsumedEnergy`, valid for exclusive jobs) or not at
all.

**A standard way to launch one run per node.** In Slurm that is one `sbatch -N1 -w <node>
--exclusive` per node, inside a maintenance reservation. A job array or a single multi-node `srun`
doesn't guarantee per-node coverage. Draining a node is an administrator's act, through a root epilog
or an admin's aggregation step, never `mps`'s. Slurm's health-check hook is killed after 60 s, so it
can read `mps`'s last verdict but can't host a run. Under HyperShell, the `TASK_*` variables go into
each record. A pull queue doesn't guarantee one task per node, so coverage is checked per host, and a
failure followed by a pass still counts as a failure.

**A report.** Identical CPU nodes repeat within about 1% and spread up to about 5%. Identical GPUs
vary about 8% on average (22% at most), and turbo alone can produce 16%. So a verdict must be relative
to the node's class and to the node's own history. Use the modified z-score M = 0.6745·(x −
median)/MAD within a class, with a fixed reference band for classes smaller than about eight nodes.

## Why it was deferred

It builds on `issues/burn-in-mode.md` and its record format.

## Outcome / vision

- Each burn-in record gains telemetry deltas from the sources the platform offers, each tagged with
  its source. Vendor libraries are loaded at run time, never linked.
- The repo carries batch-script templates: one exclusive run per node, records written as
  `<share>/<cluster>/<date>/<host>-<run_id>.jsonl`, never one shared file appended over NFS.
- `mps report` reads a directory of records and groups nodes by a class key: model, microcode, BIOS,
  driver, kernel, `mps` build and test. It prints pass/fail, time per iteration, the robust z-score,
  the change from the node's own baseline, and energy per iteration where measured. It exits non-zero
  when any node failed or is flagged.

Starting thresholds, to calibrate with a baseline campaign of three runs per node:

| Verdict | Trigger |
|---|---|
| fail | a residue mismatch, a check trip, or a new uncorrectable ECC error |
| suspect | M ≤ −3.5, or more than 3% slower than the node's own baseline; rerun it |
| performance failure | more than 10% (CPU) or 20% (GPU) below the class median on two runs |

## Sketch of the acceptance criteria

- **R1** — WHERE a telemetry source is readable, the record SHALL include its values with the
  source named. IF one is not readable, THEN the record SHALL say which and why, and the run SHALL NOT
  fail for it.
- **R2** — No telemetry reader SHALL require linking a vendor library at build time
  (§ Invariants 5).
- **R3** — `mps report` SHALL group records by class key and flag a node whose time per iteration has
  a modified z-score at or below the configured threshold, or that falls outside its own baseline
  band.
- **R4** — `mps report` SHALL exit non-zero WHEN any record failed or any node is flagged, and SHALL
  list each one with its reason.
- **R5** — The batch templates SHALL run exactly one burn-in per node, exclusively, write each record
  to its own file, and report any node in the requested set that produced no record.
- **R6** — Neither `mps burnin` nor `mps report` SHALL change scheduler state; draining is left to
  the operator.

## Notes

- Shaping decides whether `mps report` is a subcommand in C++ (dependency-free, the house style) or a
  script beside the templates.
- Energy per iteration also feeds the efficiency comparison in the definition of done
  (`ROADMAP.md`).
- References: NIST/SEMATECH e-Handbook, "Detection of outliers" (modified z-score); Slurm
  documentation (`sbatch`, `scontrol`, prolog/epilog, `slurm.conf` `HealthCheckProgram`); HyperShell
  documentation (task environment); Linux kernel documentation (powercap, hwmon, EDAC); NVML and
  AMD SMI references.
- Found by: factory adoption review, 2026-10-11.
