---
status: unshaped
kind: feature
appetite: small
---

# `mps bench`: time per iteration that compares across machines

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Two of the roadmap's goals are numbers.

- **Performance:** `mps`'s throughput on the yardstick exponents against the established programs on
  the same machine (`ROADMAP.md`).
- **Burn-in walltime:** a time per iteration, scaled.

Nothing in `mps` measures either, and a one-off timing is worthless for both. It has to be
repeatable: fixed binary, code path, exponent, transform, iterations and seeded shift, a warm-up
discarded, and the median of several steady windows, because turbo clocks aren't sustainable. It has
to run with production settings on (Gerbicz check enabled), as the comparison with other programs
assumes. And it has to check its own answer, or a fast wrong engine looks like a win.

Whole-device throughput also depends on layout. mprime peaks at one worker per socket, Mlucas at one
worker per core. A benchmark that tries one layout measures the layout, not the engine.

The established programs' own histories show why the context must be recorded. gpuowl lost 55% to a
ROCm upgrade on one GPU, and 9–14% on NVIDIA across about 520 commits.

## Why it was deferred

Never started. It needs the transform engine and is needed by every performance cycle after it, so it
lands at the end of Part II.

## Outcome / vision

`mps bench` runs a fixed workload on the selected engine and device for each requested exponent or
transform length. It searches worker and thread layouts when asked, and reports the median time per
iteration and its spread over at least three steady windows after warm-up. It also reports
whole-device throughput, the largest round-off seen, and the context: CPU governor, boost and SMT
state, and driver and runtime versions. Each workload starts from a known state and ends on a known
residue, so a benchmark is also a correctness test. Results print as a table, and as one JSON object
per workload with `--json`. The default set is the two yardstick exponents in `ROADMAP.md`.

## Sketch of the acceptance criteria

- **R1** — For each workload, `mps bench` SHALL report the median time per iteration and its spread
  over at least three steady windows, excluding the warm-up.
- **R2** — Each workload SHALL end by comparing its residue with the known answer. IF they differ,
  THEN it SHALL report the mismatch and exit non-zero.
- **R3** — WHERE `--json` is given, each result SHALL be one JSON object carrying the engine, device,
  layout, length, exponent, iterations, median, spread, the context listed above, and the program
  version.
- **R4** — WHERE a layout search is requested, `mps bench` SHALL report the throughput of each layout
  tried and name the best.
- **R5** — Timings SHALL be taken with the production error checks on.

## Notes

- Energy per iteration joins in `issues/fleet-runs-and-reports.md`, where the telemetry readers live.
- The yardstick runs, the established programs beside `mps bench` on the same machine, belong to each
  performance cycle's review, not to this one.
- A performance regression test pinned to recorded driver versions is a natural follow-on, once CI
  hosts with the hardware exist.
- Found by: factory adoption review, 2026-10-11.
