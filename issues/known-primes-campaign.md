---
status: unshaped
kind: release
appetite: big
---

# Done: every known Mersenne prime, on every platform class, with the numbers

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The definition of done in `ROADMAP.md` is an outcome, not a feature: `mps` verifies the known
Mersenne primes at a speed and efficiency that rival the established programs. No single cycle
produces that, and nothing records when it is reached.

The largest exponents cost the most. M136279841 needs 136,279,841 squarings. With current programs
that takes about 6 to 16 hours on a datacenter GPU: 182 µs per iteration on an H100 and 331.5 µs on
an A100 with PRPLL-NTT. It takes days on one CPU socket. The dozen exponents below it cost
proportionally less, and the first 37 cost seconds to hours. So the campaign has to be planned. It
runs as cluster jobs, split across devices, with checkpoints, and doubles as long burn-in soaks.
Its results need one table: exponent, platform, engine, transform, time per iteration, total time,
energy, and the established program's numbers on the same hardware.

## Why it was deferred

It is the last entry before 1.0. It runs once Parts II–VIII have landed on each platform class.

## Outcome / vision

A results table in `doc/` and the raw records behind it. Every known Mersenne prime exponent is
verified prime by Lucas–Lehmer on at least one engine per platform class:

- the Apple CPU;
- the Apple GPU;
- x86-64 CPUs;
- Arm Linux CPUs;
- NVIDIA GPUs;
- AMD GPUs.

The largest exponents run as cluster campaigns. Each class has its "rivals" comparison on the two
yardstick exponents, measured side by side with production settings on, and energy per iteration
where measurable. The release that reaches this is `1.0.0`.

## Sketch of the acceptance criteria

- **R1** — For every known Mersenne prime exponent, a Lucas–Lehmer run of `mps` SHALL end in a zero
  residue on at least one engine per platform class. The run's record SHALL be kept.
- **R2** — For each platform class with an established program, the table SHALL show `mps`'s
  throughput and energy per iteration on the yardstick exponents against that program's on the same
  machine, and SHALL meet the bars in `ROADMAP.md`.
- **R3** — For each class without a competitive established program, the table SHALL show the
  fraction of the memory-bandwidth roofline achieved.
- **R4** — Every run in the table SHALL carry the build identity, the device identity and the
  residue.

## Notes

- `kind: release`: this is a checkpoint, not a feature. Its build phases are runs and tables, and the
  code changes are the fixes the runs turn up.
- Shaping sets which exponents run on which class. Each class need not repeat every exponent, but
  each class runs at least the largest one it can reach in a campaign.
- Found by: factory adoption review, 2026-10-11.
