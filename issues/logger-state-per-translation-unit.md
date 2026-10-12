---
status: unshaped
kind: fix
appetite: small
---

# One logger state for the whole program

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The logger keeps its state in a header. `style`, `level`, `hostname` and `task_id` (and `name`) are
declared `static` in `src/log.hpp:93-98`, so every translation unit that includes the header gets its
own copy. `logging::init()` and the `-D`/`-d` handling run only in `src/main.cpp:21-31`, so only
`main.cpp`'s copy is ever set:

- `-D` (detailed style) and `-d` (debug level) reach only `main.cpp`'s messages.
- Every other unit (`src/cli.cpp`, `src/math.cpp`, and each module to come) logs at the
  zero-initialised level, `DEBUG`, in the default style, with no hostname or `TASK_ID` stamp.

Evidence, reproduced on the `logging` rename and identical on the code before it:

- `TASK_ID=abc mps -D -t ll 31` prints `    info [mps] Running Lucas-Lehmer test`, the default style
  with no timestamp, hostname or task stamp. The message comes from `src/math.cpp`.
- `mps -D abc` prints `… <host> [abc] critical [mps] …`, the detailed style, because that message
  comes from `src/main.cpp`.
- With the state made `inline`, the first command prints `TS HOST     info [mps] …` instead.

The fleet relies on the detailed stamp to tell nodes and tasks apart. Every arithmetic module will log
from its own translation unit, so as the program grows, less and less of its output carries the stamp
or honours the level.

## Why it was deferred

Pre-existing on `main` since 2024. `build-and-test-foundation` renamed the namespace from `log` to
`logging` and locked the logger's output (`spec/build-and-test-foundation/GOAL.md` R9): levels,
styles, colours and the stamps had to stay byte-identical to the code before the rename. Making the
state shared changes that output, so it needs its own cycle.

## Outcome / vision

The logger has one state for the whole program. `-D` and `-d` apply to every message, whichever
source file logs it, and the detailed style stamps the time, hostname and `TASK_ID` on all of them.

## Sketch of the acceptance criteria

- **R1** — WHEN `-D` is given, `mps` SHALL print every log message in the detailed style, whichever
  translation unit emits it.
- **R2** — WHEN `-d` is given, `mps` SHALL emit debug messages from every translation unit, and
  WHEN it is absent, no translation unit SHALL emit debug messages.
- **R3** — WHERE `TASK_ID` is set and the detailed style is on, every log message SHALL carry the
  hostname and the task id.
- **R4** — The logger SHALL keep exactly one copy of its style, level, hostname and task id per
  program.

## Notes

- Related: `spec/build-and-test-foundation/` (the rename and R9), `issues/fleet-runs-and-reports.md`
  (fleet runs read the stamps).
- The likely fix is `inline` variables in `src/log.hpp`. `tests/cli.sh` pins the default-style stderr
  line for `-t ll 31`; shaping decides which pinned output the fix may change.
- Found by: build-and-test-foundation (research and P2).
