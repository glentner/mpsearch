# GOAL — One logger state for the whole program

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** logger-state-per-translation-unit
- **kind:** fix
- **appetite:** small

## Problem

`-D` (detailed style) and `-d` (debug level) don't reach most of what `mps` logs. The logger's
style, level, hostname, task id and name are declared `static` in a header (`src/log.hpp:93-98`), so
every source file that includes it holds its own copy. `logging::init()` and the `-D`/`-d` handling
run only in `src/main.cpp:24-34`, so only `main.cpp`'s copy is ever set. Every other file logs with
its zero-initialised copy: the default style, no hostname, no `TASK_ID`.

Pre-existing on `main` (`9a41271`): the declarations date from `1dd03d1e` (2024-03-25), before the
factory. Observed on `9a41271` with `TASK_ID=abc NO_COLOR=1`:

- `mps -D -t ll 31` prints `    info [mps] Running Lucas-Lehmer test`: the default style, from
  `src/math.cpp:94`. The same holds for the other two start lines (`src/math.cpp:72,83`).
- `mps -D -p -p 3` prints ` warning [mps] Duplicate use of -p/--primes`, from `src/cli.cpp:149`. The
  duplicate-use warnings (`src/cli.cpp:149-193`) have a second gap besides the copy: they are
  emitted while the command line is being parsed, before `main.cpp` applies `-D`.
- `mps -D abc` and `mps -D -x 31` print `critical [mps] …` in the default style, although that
  message comes from `src/main.cpp:86`. The option errors are raised inside the parser's loop
  (`src/cli.cpp:50`) and PVAL's check just after it (`src/cli.cpp:77`), both inside `parse_args`,
  and `main.cpp` applies `-D` only after `parse_args` returns. The seed's evidence that `mps -D abc`
  prints the detailed style is stale: it prints the default style on `9a41271`.

So the defect has two parts: the per-file copies, and the parse-time messages (the duplicate-use
warnings, the option errors and PVAL's error), which `-D` and `-d` reach in no file today.
- The error and contradiction lines from `src/result.cpp:384,408`, added by
  `verdicts-and-exit-statuses`, log through the result module's own copy and can't carry the stamp
  either.

The fleet relies on the detailed stamp to tell nodes and tasks apart (`issues/fleet-runs-and-reports.md`),
and every arithmetic module to come will log from its own source file. As the program grows, less
and less of its output carries the stamp or honours the level.

The default style is unaffected in what it prints. Every message outside `main.cpp` is at `info` or
above, so none of them printed only because its file's level was zero-initialised to `debug`: under
one shared state at the default `info` level each still prints, byte for byte as today. No file
outside `main.cpp` emits a debug message today, so `bin/mps` can't show the `-d` gap; the checks have
to.

## Outcome / vision

The logger has one state for the whole program. `-D` and `-d` apply to every message raised once
the parser has read them, whichever source file logs it, and the detailed style stamps the time, the hostname and `TASK_ID` on each of
them. Without `-D` and `-d`, nothing anyone sees on stderr changes.

## Acceptance criteria (the contract)

A detailed message has the shape `YYYY-MM-DD HH:MM:SS <host> [<TASK_ID>] <level> [mps] <text>` once
colour is off: the local time, the machine's host name, the task stamp only where `TASK_ID` is set,
then the level right-aligned in 8 columns. A check matches that shape, not the time's bytes.

- **R1** — WHEN `-D` is given, `mps` SHALL print each log message raised after the parser has read
  `-D` in the detailed style, whichever source file emits it: the test start lines, the duplicate-use
  warnings, the result printer's error and contradiction lines, and `main.cpp`'s own messages.
  `mps -D -t ll 31`, `mps -D -p -p 3`, `mps -D -x 31` and `mps -D abc` (PVAL is checked after the
  whole command line is read) SHALL print in the detailed style.
- **R2** — WHEN a message is raised before the parser reaches `-D`, `mps` SHALL print it in the
  default style: `mps -x -D 31` and `mps -p -p -D 3` print as they do without `-D`.
- **R3** — WHERE `TASK_ID` is set and non-empty, every message printed in the detailed style SHALL
  carry `[<TASK_ID>] ` after the host name, whichever source file emits it. WHERE `TASK_ID` is unset or
  empty, no message SHALL carry a task stamp.
- **R4** — WHEN the level is set to debug (by `-d` in `bin/mps`, from the point the parser has read
  it, by the same left-to-right rule as R1–R2), a debug message SHALL be emitted whichever source
  file logs it; while it isn't, no source file SHALL emit one. `make check` shows
  this by setting the level in one translation unit and logging from another, which may be a helper
  file of the checks.
- **R5** — The logger SHALL hold exactly one copy of its style, level, hostname, task id and name in
  the program. `make check` shows this by comparing the state's address as seen from two translation
  units, or an equivalent the plan picks.
- **R6** — Without `-D`, every message `mps` logs SHALL stay byte-identical to `9a41271`, colour
  included, and print under the same conditions: `build-and-test-foundation` R9's lock still holds
  for the default style. Without `-d`, no message that prints today SHALL disappear, and none that
  doesn't SHALL appear.
- **R7** — `make check` SHALL pass and carry a check for each of R1–R6, and no file under `tests/`
  SHALL describe the per-file copy as known (today `tests/check_log.cpp:22-24` does).

## Non-goals (no-gos)

- New log formats, levels or colours.
- A quiet flag.
- Structured or JSON logging.
- Thread safety: `mps` is single-threaded today; Part V's multithreaded transforms revisit it.
- Logging to files.
- Configuration files.
- New debug messages in `src/` added only to make R4 visible from `bin/mps`.
- A pre-scan of the command line: `-D` and `-d` don't reach messages raised before the parser reads
  them.

## Clarifications

Geoffrey delegated every gate in this effort to the foreman on 2026-10-11; the answers below are the
foreman's decisions under that delegation.

- **Q:** How many logger states? — **A:** One per program. Style, level, hostname, task id and name
  each exist once, and every translation unit reads and writes the same state. The plan chooses the
  mechanism; `inline` variables are the seed's likely fix. (resolved 2026-10-12)
- **Q:** What must `-D` reach? — **A:** Every log message, whichever translation unit emits it: the
  start lines from `src/math.cpp`, `src/cli.cpp`'s duplicate-use warnings, `src/result.cpp`'s error
  and contradiction lines, and `src/main.cpp`'s messages. Where `TASK_ID` is set, every detailed
  message carries it. (resolved 2026-10-12)
- **Q:** How is `-d` shown, when nothing outside `main.cpp` logs at debug? — **A:** By the checks: one
  sets the level in one translation unit and logs from another, which may be a test helper file. No
  debug messages are added to `src/` for this. Without `-d`, no translation unit emits a debug
  message. (resolved 2026-10-12)
- **Q:** Does the default style change? — **A:** No. Without `-D`, every message's text stays
  byte-identical; `build-and-test-foundation` R9's lock holds for the default style. Confirmed
  against `9a41271`: no message outside `main.cpp` printed by default only because of the
  zero-initialised `debug` level (all are `info` or above), so no message's visibility changes.
  (resolved 2026-10-12)
- **Q:** How is "one copy" shown? — **A:** By a check comparing the state's address from two
  translation units, or an equivalent the plan picks. (resolved 2026-10-12)
- **Q:** What is out of scope? — **A:** New formats, levels or colours; a quiet flag; structured or
  JSON logging; thread safety; logging to files; configuration files. (resolved 2026-10-12)
- **Q:** Must `-D` (and `-d`) apply to messages raised while parsing words that come before it on the
  command line? — **A:** No. The command line is read left to right, and a message raised after the
  parser has read `-D` honours it: a duplicate-use warning for a later word, an argument error raised
  at a later word, and PVAL's error, since PVAL is checked after the whole line is read. A message
  raised before the parser reaches `-D` stays in the default style. There is no pre-scan of the
  command line. The plan chooses the mechanism (the parser applying the setting as it reads the
  flag, or the error carrying the flags read so far, for example), as long as the parser and its
  error handling stay in their files and the coupled core's same-commit rule holds. `-d` follows the
  same rule. (resolved 2026-10-12)

## Related materials

- Seed: `issues/logger-state-per-translation-unit.md`; `ROADMAP.md` Part I, "One logger state for the
  whole program".
- `spec/build-and-test-foundation/GOAL.md` R9: the logger's output lock.
- `spec/verdicts-and-exit-statuses/GOAL.md`: `src/result.cpp`'s error and contradiction lines.
- `issues/fleet-runs-and-reports.md`: fleet runs read the stamps.
- Source: `src/log.hpp`; `src/main.cpp:24-34,86-94`; `src/math.cpp:72,83,94`;
  `src/cli.cpp:50-77,149-193`; `src/result.cpp:384,408`; `tests/check_log.cpp`; `tests/cli.sh:167-169`.
