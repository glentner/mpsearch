---
status: unshaped
kind: fix
appetite: small
---

# `--` ends the options, for every flag in every position

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

`mps` treats every word after `--` as a positional word, as `cli-and-sieve-fixes` made it (the
`positional_only` branch of `cli::Interface::parse_args` in `src/cli.cpp`). The behaviour is right.
The checks that pin it were added one instance at a time, so `make check` still passes parsers
that break it in ways `bin/mps` can show:

- **Only the first word after `--` is pinned.** `tests/cli.sh`'s loop over 23 flags puts each one
  directly after `--`. If `--` covered only the next word, `mps -- 31 --help` would print the
  help and exit 0, and `-- 31 -j` and `-- 31 -D` would act. Everything stays green. Today all three
  exit 1 with "Unexpected positional argument".
- **A second `--`.** A parser that keeps a second `--` as a positional word but turns options back
  on (`mps 31 -- -- --help` prints the help) stays green.
- **A flag given both before and after `--`.** A parser that keeps the word positional but also
  applies a quiet flag adds a duplicate-use warning to `mps -j 31 -- -j` that today's `mps` never
  prints. Every such mutant has to be written for one flag.

Each of three review cycles in `logger-state-per-translation-unit` found the next sibling of the
same class: `-D` after `--`, then `--detailed` and every long option after `--`, then later
positions. The loop reached its bound with these two left open.

## Why it was deferred

Geoffrey's decision at that cycle's third review, 2026-10-12: "Land now, seed a follow-up". The
behaviour was verified correct against `9a41271`'s bytes; only the checks' reach was missing. See
`spec/logger-state-per-translation-unit/REVIEW.md`, review cycle 3 (FA, FB).

## Outcome / vision

One check in `tests/cli.sh` pins the property, not instances. For every flag and option (short,
long, grouped, valued), in every position after `--` (first, after a positional, after a second
`--`), and also given before `--`, the output of `bin/mps` equals the output with that word as an
ordinary positional: the same exit status and the same stderr, byte for byte.

## Sketch of the acceptance criteria

- **R1** — For every word W that `mps` reads as a flag or option, and every position after `--`,
  `mps` SHALL treat W as a positional word: the same exit status and stderr as `9a41271`.
- **R2** — A second `--` after the first SHALL be a positional word, and words after it SHALL stay
  positional.
- **R3** — A flag given before `--` and repeated after it SHALL act once, with no duplicate-use
  warning from the repeat.
- **R4** — `make check` SHALL pin R1–R3 through `bin/mps`, generated from the parser's flag list,
  so a new option is covered when it is added.

## Notes

- Mutants that must turn `make check` red, from the review: `--` covering only the next word; a
  second `--` turning options back on; a quiet flag applied after `--`.
- Generate the word list from one place (the help, or `flag_by_option` and the short flags), so the
  check grows with the parser.
- Found by: `logger-state-per-translation-unit` review cycle 3, 2026-10-12.
