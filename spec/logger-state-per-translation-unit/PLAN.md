# PLAN — One logger state for the whole program

> **Status:** Draft for review · **Last updated:** 2026-10-12
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). There is no `research/`: the lean path
> applies (§ 4). Every design element traces to a GOAL R-ID.

## 1. Summary

The logger's five state variables in `src/log.hpp` become `inline` variables, so the program holds
one of each. The parser then applies `-D` and `-d` at the moment it reads them, in
`cli::Interface::check_flag`. Every message raised after that point uses them, whichever file logs
it; every message raised before it keeps the default style. These are two edits in two files, and
neither is on the high-blast-radius list: `src/main.cpp`, `src/math.*` and `src/result.*` don't
change. Main's post-parse application of the flags becomes a harmless repeat (§ 3).

The checks:

- a second test translation unit, `tests/log_peer.*`, for the cross-unit checks of the address, the
  level and the style;
- a check of the result printer's error lines;
- a check of the parser;
- `tests/cli.sh` driving `bin/mps` for every message the GOAL names.

The frozen gates do three things:

- build `9a41271` beside the tree and compare every default-style run byte for byte;
- match each detailed line's shape against this machine's host;
- inject 16 defects, each of which must turn `make check` red.

The last phase pushes the branch and runs the three cluster targets. Its scan also rejects any
committed detailed stamp that names a host.

## 2. Design

### Files

| Path | Change | R-IDs |
|---|---|---|
| `src/log.hpp` | the five state declarations `static` → `inline`; nothing else but comment lines | R1, R3–R6 |
| `src/cli.cpp` | `check_flag` applies `-d` and `-D` as it reads them: one statement each | R1, R2, R4 |
| `tests/log_peer.hpp`, `tests/log_peer.cpp` | new: a second unit for the logger's checks, the state and capture guards, and the check that the state is restored | R1, R3–R5, R7 |
| `tests/check_log.cpp` | three checks; the stale comment at lines 22–24 rewritten | R1, R3–R5, R7 |
| `tests/check_result.cpp` | one check: the printer's error lines in the shared style | R1, R3 |
| `tests/check_cli.cpp` | one check: the flags apply as read; the state guard wherever a check parses `-d` or `-D` | R1, R2, R4 |
| `tests/cli.sh` | `bin/mps` in the detailed style, the task stamp, the left-to-right rule, and the default style's bytes, colour included | R1–R3, R6 |
| `AGENTS.md` | § Map's `tests/` line names `log_peer.*` | — |

No other file changes. The gates hold every high-blast-radius path, `src/main.hpp`, `src/cli.hpp`,
`src/ansi.*`, `src/exc.hpp`, `Makefile`, `README.md`, `ci/`, `tests/check.cpp` and
`tests/check.hpp` equal to `9a41271`. No option, usage or help text changes, so the coupled core's
same-commit rule for options has nothing to move. `README.md` documents no options.

### One state (`src/log.hpp`; R5)

Lines 93–98 become exactly:

```cpp
	inline Style style;
	inline Level level;

	inline std::string hostname;
	inline std::string task_id;
	inline std::string name;
```

- An `inline` variable at namespace scope has external linkage and one definition in the whole
  program (C++17, [basic.def.odr]; [dcl.inline]). Every unit that includes the header reads and
  writes the same object.
- There are no initialisers. The state is zero-initialised as today (`DEFAULT`, `DEBUG`, empty), so
  `bin/check`, which never calls `logging::init()`, starts where it does today.
- `std::string`'s default constructor is `constexpr` in C++20, so the strings are
  constant-initialised and no static-initialisation order arises.
- Nothing else in `log.hpp` changes but comment lines. That holds `print`, the formats in
  `log_default` and `log_detailed`, `timestamp`, the setters, `init` and the four tables
  byte-identical. It is R6's lock on the format, and the gates check it textually.
- The tables (`style_by_name`, `level_by_name`, `name_by_level`, `color_by_level`) stay `static`, as
  § Conventions' code style writes lookup tables. They are constant and are not the state the GOAL
  names (§ 5).

Rejected alternatives:

- `extern` declarations with definitions in a new `src/log.cpp`: a new module, a `MODULES` change
  and a `.hpp`/`.cpp` pair, all for the same effect.
- Accessor functions holding function-local statics: they change every reader's syntax, including
  `check_log.cpp`'s direct reads of `logging::level` and `logging::style`.

### `-D` and `-d` as the parser reads them (`src/cli.cpp`; R1, R2, R4)

`Interface::check_flag` gains one statement in each of two cases:

```cpp
			case 'd':
				if (debug_mode)
					logging::warn("Duplicate use of -d/--debug");
				debug_mode = true;
				logging::set_level(logging::DEBUG);
				break;

			case 'D':
				if (detailed_mode)
					logging::warn("Duplicate use of -D/--detailed");
				detailed_mode = true;
				logging::set_style(logging::DETAILED);
				break;
```

Each statement appears exactly once in `cli.cpp`, spelled as above, because the gates find them by
their text. `check_flag` is the one place a flag takes effect, whether it is short, grouped or long,
so the left-to-right rule follows from the parser's own order:

- **A message raised after the flag is read** uses it (R1, R4). That covers a duplicate-use warning
  for a later word, an argument error thrown at a later word, and PVAL's error, because `set_pval`
  runs after the loop. `main.cpp`'s `catch` logs the error with the state already set.
- **A message raised before the flag is read** keeps the default style (R2): `-x -D 31` throws at
  `-x`, and `-p -p -D 3` warns at the second `-p`.
- **A word that never reaches `check_flag` as a flag** changes nothing:
  - `-t -D` and `-f -D` take `-D` as a value;
  - `-- -D` makes it PVAL;
  - `--detailed=1` is rejected in `check_option_long`;
  - `-x` in `-xD` throws before `D`.
- **A group applies its flags one character at a time**, so `-Dpp 3` warns in the detailed style
  and `-ppD 3` doesn't.

There is no pre-scan (GOAL non-goal). `cli.hpp` doesn't change. `debug_mode` and `detailed_mode`
stay, and `main.cpp` still reads them.

### `src/main.cpp` stays as it is

`logging::init()` runs before `parse_args`: level `INFO`, style `DEFAULT`, the host and `TASK_ID`.
The parser then applies the flags as it reads them. Main's lines 30–34 set the same values again
once `parse_args` returns, which is idempotent. Deleting them would put a high-blast-radius path,
and its human gate, into a cycle that changes no behaviour there. § 3 records this as accepted
complexity, and no follow-up is planned.

What the GOAL's examples print once this lands, with `TASK_ID=abc NO_COLOR=1` (`<stamp>` is
`<date> <time> <host> [abc]`):

| Command | stderr |
|---|---|
| `mps -D -t ll 31` | `<stamp>     info [mps] Running Lucas-Lehmer test` |
| `mps -D -p -p 3` | `<stamp>  warning [mps] Duplicate use of -p/--primes` |
| `mps -D -x 31` | `<stamp> critical [mps] Unrecognized option: -x` |
| `mps -D abc` | `<stamp> critical [mps] Invalid PVAL 'abc': …` |
| `mps -x -D 31`, `mps -p -p -D 3`, `mps -f -D 31` | exactly what `9a41271` prints for them, which is the default style |

### The checks (R7)

**`tests/log_peer.hpp`, `tests/log_peer.cpp`** (new). This is a helper unit, namespace
`log_peer`, with the SPDX header and the code style of `src/`. Every function below, `SavedLogger`'s
and `CaptureCerr`'s included, is declared in the header and defined in `log_peer.cpp`. Defined in
the header, `addresses()` would compare the calling file with itself.

- `struct Addresses { const void* style; const void* level; const void* hostname; const void* task_id; const void* name; };`
  and `Addresses addresses();`, which gives the five as this unit sees them.
- `void set_level(logging::Level level);` and `void set_style(logging::Style style);`, which assign
  the variables directly in this unit.
- `std::string debug(const std::string& text);` and `std::string info(const std::string& text);`,
  which log through a type local to `log_peer.cpp`, with `std::cerr` captured, and return what
  reached it with the escape sequences removed.
- `std::string plain(const std::string& text);`, which removes the escape sequences.
  `bin/check` never calls `init_tty()`, so its log is always coloured.
- `bool stamped(const std::string& line, const std::string& rest);`: `line` is a date, a space, a
  time, a space, then exactly `rest`. Every check of a detailed line goes through it.
- `class SavedLogger`, which saves the five on construction and restores them on destruction.
- `class CaptureCerr`, which points `std::cerr` at its own buffer on construction, gives what it
  caught through `text()` (escapes removed), and restores `std::cerr` on destruction, so a capture
  survives a throw from `parse_args`. Every capture in a check that logs goes through it; a capture
  left pointing at a destroyed buffer is undefined behaviour and could swallow a leaked line.
- One check of its own, `the logger's state is as the run began`: style `DEFAULT`, level `DEBUG`
  and the three strings empty, as zero-initialisation leaves them. `log_peer.cpp` sorts last among
  `tests/*.cpp`, so on both linkers it registers, and runs, last. Run there it catches a state a
  check failed to restore; run anywhere it passes on a correct tree.

**Three rules bind every check that touches the logger.** The gates enforce the third.

1. **Set the state by assignment**, or through `log_peer`. Never call `logging::init()` or
   `set_hostname()` in `bin/check`.
   - `init()` runs `init_tty()`, which would turn colour off for the rest of the run.
   - The host is the fixed fake `check-host`, never the machine's name.
2. **Log only through a type local to the file**, as `check_log.cpp`'s `Message` does.
   - A template instantiated with the same arguments in two units may be merged by the linker.
   - A check could then read the state through the other unit's code and pass under the very
     defect it should catch. (The review checked today's tree: no two files share an instance.)
3. **Capture `std::cerr` around anything logged in the detailed style (`CaptureCerr`), and restore
   the state before the check returns** (`SavedLogger`).
   - `check_cli.cpp`'s parses of `-d` and `-D` now change the shared state, so every check there
     that parses either flag, groups included, holds a `SavedLogger`.
   - In the prototype (§ 4), check_cli's `-dD` parses left the style detailed, and a later
     uncaptured `-j -j` warning printed a detailed line into `make check`'s output.
   - That output is the cluster job's `check.log`, so `make check`'s output must hold no detailed
     line and never the machine's host name.

**The checks, with their labels.** The gates read these labels; spell each exactly as given.

| File | Label | What it pins |
|---|---|---|
| `check_log.cpp` | `the logger's state is one copy` | R5: the addresses of the five here equal `log_peer::addresses()`. |
| `check_log.cpp` | `the level holds across units` | R4: with the level `INFO` set here, the peer's debug prints nothing; with `DEBUG`, it prints. With the level set in the peer, a debug logged here honours it, both ways. |
| `check_log.cpp` | `the detailed style holds across units` | R1, R3: with `DETAILED`, `check-host` and `abc` set here, the peer's info line is `stamped` with exactly `check-host [abc]     info [mps] <text>`. With the task id empty there is no stamp. With `DEFAULT` the line is exactly `    info [mps] <text>`. |
| `check_result.cpp` | `the printer's errors log in the shared style` | R1, R3: with the same state set here, an error through `report_result` and a contradiction through `report_run` log `critical` lines `stamped` with exactly `check-host [abc] critical [mps] …`. In the default style they log today's lines exactly, escapes removed. |
| `check_cli.cpp` | `-D and -d apply as the parser reads them` | R1, R2, R4 (P2), through `cli::parse_args`, each parse starting from level `INFO`, style `DEFAULT`, host `check-host` and task id `abc`. See the bullets below. |
| `log_peer.cpp` | `the logger's state is as the run began` | Rule 3: no check leaves the state changed. |

What `-D and -d apply as the parser reads them` covers:

- `-D 31` and `--detailed 31` leave the style detailed and the level as it was.
- `-d 31` and `--debug 31` leave the level at debug and the style as it was; after either, the
  peer's debug prints.
- `31`, `-t -D 31` (which throws), `-f -D 31` and `--detailed=1 31` (which throws) leave both
  unchanged.
- The captured warnings of `-D -p -p 3` and `--detailed -p -p 3` are `stamped` with exactly
  `check-host [abc]  warning [mps] Duplicate use of -p/--primes`; that of `-p -p -D 3` is exactly
  ` warning [mps] Duplicate use of -p/--primes`.

`check_log.cpp`'s comment at lines 22–24 becomes, word for word:

```cpp
// Messages go through a type local to this file, so the linker never merges this file's
// instances of the logger's templates with another unit's, and a check reads the state
// through this file's code.
```

Nothing under `tests/` describes a per-file copy (R7). The gate rejects the phrases "copy per
translation unit", "copy per file", "this file's copy", "per-file copy" and "per file copy".

**`tests/cli.sh`, through `bin/mps`.** The GOAL names `bin/mps` behaviour, so it is checked there
(the previous cycle's G1: a check one call below the path `bin/mps` runs leaves that path unpinned).

- **The shape.** It is matched with built-ins only: a date `[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]`,
  a space, a time `[0-9][0-9]:[0-9][0-9]:[0-9][0-9]`, a space, a non-empty host with no space, a
  space, `[abc] ` only where `TASK_ID=abc`, then exactly the default line, for every line of stderr.
- **Setting `TASK_ID` for one run.** Write `TASK_ID=abc "$mps" …`, a simple command. POSIX leaves it
  unspecified whether an assignment before a shell function call persists after it.
- **Literal bytes.** Every expected line is written out as `9a41271` prints it, never produced by
  running `bin/mps` again: `mps -f 31` and `mps -t 31` print something else than `-f -D 31` and
  `-t -D 31` minus their `-D`.
- **The expectations the gates name** (spell each name exactly; the gates' defects must fail them):

  | Name | Run | Phase |
  |---|---|---|
  | `-D -t ll 31 stamps its start line with TASK_ID` | `TASK_ID=abc`, one stamped line | P1 |
  | `-D 31 without TASK_ID has no task stamp` | `TASK_ID` unset, three stamped lines with no `[` stamp | P1 |
  | `-t ll 31 under FORCE_COLOR logs its exact bytes` | the info line's bytes, colour included | P1 |
  | `-d 31 logs as without -d` | exactly the three default start lines | P1 |
  | `-D -p -p 3 stamps its warning` | `TASK_ID=abc` | P2 |
  | `-D -x 31 stamps its error` | `TASK_ID=abc` | P2 |
  | `-x -D 31 errs in the default style` | `TASK_ID=abc`, exactly `critical [mps] Unrecognized option: -x` | P2 |

- **Besides those, P1:** `-D -t ll 31` and `-D 31` under `TASK_ID` unset and empty; `-D 31` under
  `TASK_ID=abc`; `-d -t ll 31` exactly; the exact `FORCE_COLOR=1` bytes of `-p -p 3`'s warning and
  `-x 31`'s critical line (R1, R3, R4, R6).
- **Besides those, P2:** `-Dpp 3` and `-D abc` stamped under `TASK_ID=abc`; `-p -p -D 3`,
  `-f -D 31` and `-t -D 31` exactly as `9a41271` prints them (R1, R2, R3).
- **`got`.** A failed expectation's `got` may echo a detailed line, host included. Only passed runs
  become evidence, and `redact.sh` and the scan cover stamps anyway (§ 5).

### Requirement → design map

| R-ID | Design element(s) that satisfy it |
|------|-----------------------------------|
| R1   | One state (`inline`), so the start lines, the result printer and main's messages log with the style the parser set. `check_flag` sets `DETAILED` as it reads `-D`, so the warnings and errors after it follow. Checks: `the detailed style holds across units`, `the printer's errors log in the shared style`, `-D and -d apply as the parser reads them`, and cli.sh's detailed runs. |
| R2   | The style changes only in `check_flag`, when `-D` is read; there is no pre-scan. Checks: cli.sh's four runs pinned to `9a41271`'s bytes; the parser check's `-p -p -D 3`. |
| R3   | One `task_id`, set by `init()` from `TASK_ID`; `log_detailed`'s existing empty test, unchanged. Checks: `the detailed style holds across units`, the printer check, and cli.sh with `TASK_ID` set, unset and empty. |
| R4   | One `level`. `check_flag` sets `DEBUG` as it reads `-d`, and `init()` sets `INFO` before parsing. Checks: `the level holds across units` (set in one unit, logged from `log_peer`) and the parser check. That `init()` still sets `INFO` is pinned by the gates' textual lock on `log.hpp`, not by `make check`: `bin/check` never calls `init()`, and no message in `src/` is at debug (§ 5). |
| R5   | `inline` state, one definition program-wide. Check: `the logger's state is one copy` (addresses from two units). |
| R6   | Nothing that formats a line changes (`log.hpp` beyond the five declarations is byte-identical). The level and style start where `init()` puts them, as today. No message outside `main.cpp` is below `info`. Checks: cli.sh's default bytes, colour included; the printer check's default lines. |
| R7   | The checks above, a label each, and the restore check. The comment rewritten. `make check` green on both compilers, with no detailed line and no host name in its output. |

## 3. Invariant gate (AGENTS.md constitution check)

Walked against the GOAL's intent before the design (gate #1) and against this design after it
(gate #2). The sections touched:

- **High-blast-radius paths:** none touched. The design keeps `src/main.cpp`, `src/math.*` and
  `src/result.*` as they are, and the gates assert it, so research trigger 2 doesn't fire and no
  human gate arises.
- **§ 1 Verdicts are earned:** not touched. The logger writes only to stderr, and the verdict lines,
  the records and the statuses are unchanged. The result printer's `critical` lines gain the stamp
  under `-D` only.
- **§ 2 Arithmetic is checked:** not touched. There is no numeric code, and the `Makefile` and its
  flags are unchanged.
- **§ 4 Clean room:** our own code. `inline` variables are a C++17 language feature.
- **§ 5 Dependency-free core:** no new dependency. `log_peer.*` lives with the tests and never links
  into `bin/mps`.
- **§ Conventions:**
  - **Code style:** tabs, the SPDX block, `snake_case` functions, a `PascalCase` class
    (`SavedLogger`), doc blocks at the top of function bodies, `not`/`and`/`or`. The lookup tables
    stay `static std::map`.
  - **Same-commit rules:** no option change, so no help or README move. The § Map line for
    `log_peer.*` lands with the file.
  - **IDs:** no R#/P# ids in `src/` or `tests/`.

### Deviation justifications

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| `src/main.cpp` keeps applying `-D` and `-d` after `parse_args` returns, a repeat of what the parser now does as it reads them | It avoids touching a high-blast-radius path in a cycle that changes no behaviour there. The repeat is idempotent: the same two setters with the same values. | Deleting lines 30–34 is simpler code, but it puts `main.cpp` and its human gate into this cycle for no observable change. It is accepted as is, not deferred; nothing records it as a follow-up. |

## 4. Rabbit holes (resolved)

Lean path: `appetite: small`, `kind: fix`, and research trigger 2 doesn't fire (§ 3), so there is no
fan-out and no `research/`. Four unknowns were settled by targeted reads and a scratch prototype
(the two edits applied to a copy of `def1adc`, never committed).

- **Which mechanism gives one state:** `inline` variables (§ 2). The prototype prints the GOAL's
  outputs for every example in § 2's table, and today's `make check` stays green on it (49 and 530
  passed).
- **Does the default style move:** no. In 88 runs without `-D` (22 argument lists × 4
  environments: `NO_COLOR` or `FORCE_COLOR`, each with `TASK_ID` unset and set), stdout, stderr and
  status were identical to `9a41271`. That includes the R2 cases `-x -D 31`, `-p -p -D 3`,
  `-pp -D 3`, `-f -D 31`, `-t -D 31`, `-- -D`, `--detailed=1 31` and `-xD 31`.
- **The checks leak state:** confirmed in the prototype. A `-j -j` warning in `make check`'s output
  came out in the detailed style after `check_cli.cpp`'s `-dD` parses. That gave rule 3 and the gate
  that `make check`'s output holds no detailed line. It is also the root of the evidence risk in § 5.
- **Linker merges can hide a defect from a check:** the per-file copy was never reliably visible to
  a check, because merged template instances read whichever unit's copy the linker kept (the reason
  for today's `Message` type). This gave rule 2. The gates also run every injected defect at the
  Makefile's own flags, so each one is seen red as `make check` builds it.
- **Are the gates right?** They were run on a scratch prototype of the whole design (never
  committed), behind a scratch plan commit. `p1.sh` was green on the P1 state in 3 min 35 s, and
  `p2.sh` on the P2 state in about 6 min 10 s (both on this Mac), with all 16 defects caught for the
  named reason. `p1.sh` fails on the untouched tree at the footprint check. Without the
  `SavedLogger`s in `check_cli.cpp`, `make check` printed a detailed line, which the first gate step
  rejects. `prepush.sh scan` passed with its self-test, and the footprint check rejects a block
  comment around `set_level(INFO);`.
- **The adversarial design review (trigger 3) ran:** see § 4.1.

### 4.1 Adversarial design review

A fresh reviewer (effort xhigh) had GOAL.md and the drafted design, built the design and its
defects in its own scratch copy on Apple clang and `g++-16`, and looked for inputs the checks pass
while the behaviour is broken, or fail while it is good. It confirmed the core: the GOAL's examples
print as § 2 says, `nm` shows one symbol for each of the five, no two files share a template
instance that could hide defects 6, 7 or 12, and defects 1–5 each fail exactly their label. It
found no bent R-ID. What it changed:

- **Defect 13 caught for the wrong reason.** Dropping `main.cpp`'s include-level task id moves
  `init()`'s write away from the shared state, so every other file lost the stamp instead.
  Defect 13 now clears the task id in `main.cpp`'s argument-error handler, and only `-D -x 31` and
  `-D abc` lose it.
- **"Red in cli" accepted any failure.** Every defect now names the check or the `cli.sh`
  expectation it must fail (§ 2's two name tables; § 6). Other checks may fail as well: a named
  half is a minimum, never an exact set.
- **A block comment could hide a line from the footprint check.** `/*` and `*/` around
  `set_level(INFO);` in `init()` read as comment lines, and `bin/check` never calls `init()`. The
  footprint check now rejects any added line holding `/*` or `*/` or ending in `\`. That `init()`
  sets `INFO` before parsing is R4's other half, and only this textual lock pins it (§ 2 map).
- **The parser check could pass vacuously.** It now sets `check-host` and `abc` and matches the
  whole stamped line through `log_peer::stamped`, so defect 12 fails it too, and it covers the long
  forms `--detailed` and `--debug`.
- **"As without `-D`" was wrong for two R2 runs.** `mps -f 31` and `mps -t 31` print other errors
  than `-f -D 31` and `-t -D 31`; cli.sh pins `9a41271`'s literal bytes instead.
- **Rule 3 leaned on `make check`'s output alone.** `CaptureCerr` restores `std::cerr` through a
  throw, and the last-registered check `the logger's state is as the run began` catches a leaked
  state.
- **The host name.** The gates accept either name `hostname` gives before and after a run (macOS
  renames a host on a network conflict); every gate message and log tail shows `<host>` in place of
  it; the scan also fails on this machine's own name anywhere in the cycle's files or history. The
  stamp's time must be the local time within two minutes, which tells local time from UTC.
- **Smaller:** the `log_peer` functions, guards included, are defined in `log_peer.cpp`; the
  replacement comment for `check_log.cpp:22–24` is given word for word.

## 5. Risks & open questions

- **Node names in committed evidence.** This is the new risk. More messages now carry the host and
  `TASK_ID`, and a cluster node's host name is a site detail. Four defences, in the order they
  act:
  1. `make check`'s output holds no detailed line. Every gate enforces this on both compilers, and
     checks use the fake host `check-host`.
  2. P3's gate requires that each committed `check.log` hold no detailed stamp at all.
  3. `redact.sh` rewrites the host after any detailed stamp, plain or coloured, to `<host>`.
  4. The scan flags any detailed stamp whose host isn't `<host>`, in every cycle file and in the
     history. It proves itself on planted plain, coloured and `got`-echoed lines, built at run time,
     before each scan.
  5. The gates never print this machine's host name, and the scan fails on it anywhere in the
     cycle's files or history.
- **The tables stay per unit.** The `static` tables are referenced from `inline` templates. That is
  an ODR violation that has been harmless since 2024, because every copy is identical and none
  changes after static initialisation. It is outside the GOAL's state and its non-goals, and it is
  noted here rather than deferred.
- **R4's "not debug until `-d`" rests on the text.** `init()` setting `INFO` is unobservable today:
  `bin/check` never calls `init()`, and no message in `src/` is at debug (a GOAL non-goal forbids
  adding one). The footprint check holds `init()` byte-identical, block comments and line splices
  included.
- **The gates are textual.** They find the two `cli.cpp` statements, the five declarations and the
  `#include "log.hpp"` lines by their text, and they hold the files listed in § 2 equal to
  `9a41271`. A builder who needs another shape stops and escalates; the builder doesn't edit a gate.
- **Out of scope, as before:**
  - `gethostname` truncation, a pre-existing behaviour;
  - thread safety, a GOAL non-goal;
  - the time's bytes, which are matched by shape only.

## 6. Verification strategy

There is no throwaway-environment helper: `mps` keeps no state outside its working directory. The
gates drive `bin/mps` in empty directories with `env -i` (`PATH`, `HOME`, `LC_ALL=C`, plus the
run's own variables) and run the profile's `make check`.

**The gates** (`verify/`, frozen at the plan commit; each refuses to run if `verify/` differs from
it). Each phase gate is cumulative, prints `GATE FAIL: <reason>` and exits 1 on the first failure, or
ends with a `Pn gate: green (…)` line. Requirements: macOS, `clang++`, `g++-16`, GNU make 3.81 at
`/usr/bin/make`, `git`, `rsync`, `tar`, `python3` and `hostname`; P3 adds the site file.

1. **`make -B check`** with `clang++` and `g++-16` on a copy of the tree as a clone would hold it:
   - green, with no warnings, and the same counts on both compilers;
   - no detailed stamp in the output, escapes removed;
   - not this machine's host name.
2. **The files** (R6's format lock; the design's footprint):
   - every path § 2 freezes equals `9a41271`, and no file is added under `src/` or `ci/`;
   - `src/log.hpp` differs from `9a41271` only in the five declarations, `static` to `inline`
     exactly as § 2 gives them, plus `//` comment and blank lines; no added line holds `/*` or `*/`
     or ends in `\`;
   - `src/cli.cpp` equals `9a41271` in P1. From P2 it differs only by the two statements, once each,
     plus comment and blank lines.
   - No file under `tests/` describes a per-file copy, and there are no R#/P# ids in `src/` or
     `tests/`.
3. **R6 and R2: the same as `9a41271`.** The gate builds `9a41271`'s `bin/mps` beside the tree. It
   runs both on 41 argument lists without `-D` and 8 with a `-D` the parser reads too late or never,
   each under five environments:
   - `NO_COLOR=1` and `FORCE_COLOR=1`, each with and without `TASK_ID=abc`;
   - `NO_COLOR=1` with `TASK_ID` empty.

   Stdout, stderr and status must match byte for byte. The lists cover:
   - every test start line;
   - each duplicate-use warning;
   - each kind of argument error;
   - the exponent check;
   - `-j`, `-p`, `-d`, `-h` and `-v`.
4. **R1 and R3: the detailed style through `bin/mps`.** For each pair of argument lists
   (`-D -t ll 31` against `-t ll 31`, and so on), under `TASK_ID=abc`, unset and empty, the tree's
   stderr is `9a41271`'s for the second list line for line, each line behind a stamp.
   - **The stamp:** a date and time that parse and lie within two minutes of the local time, then
     the name `hostname` gives just before or just after the run, then `[abc] ` only under
     `TASK_ID=abc`.
   - **Stdout and status** must match too.
   - **P1** pairs the messages raised after parsing: the start lines, including `--detailed`, `-Dj`
     and `31 -D`. **P2** adds those raised while parsing: each duplicate warning, `-D -x 31`,
     `-D abc`, `-Dpp 3`, `-p -D -p 3`, a missing value, a second PVAL, and `-D -- -1`.
5. **`make check` pins it: defects.** The gate injects each defect into a copy of the tree. `make -B
   check` at the Makefile's own flags (`clang++`) must then build, go red, and fail at least the
   named `bin/check` check and the named `cli.sh` expectation (§ 2's tables). Other checks may fail
   as well.

   | # | Defect | Must fail in `bin/check` | Must fail in `cli.sh` |
   |---|---|---|---|
   | 1 | the state back to per-file `static`, all five | `the logger's state is one copy` | `-D -t ll 31 stamps its start line with TASK_ID` |
   | 2 | the level not shared (`static Level level;`) | `the level holds across units` | — |
   | 3 | the style not shared | `the detailed style holds across units` | `-D -t ll 31 stamps its start line with TASK_ID` |
   | 4 | the host name not shared | `the detailed style holds across units` | `-D -t ll 31 stamps its start line with TASK_ID` |
   | 5 | the name not shared | `the logger's state is one copy` | — |
   | 6 | the task stamp dropped from `src/math.cpp`'s messages (`#define task_id task_id_lost` around its `#include "log.hpp"`) | — | `-D -t ll 31 stamps its start line with TASK_ID` |
   | 7 | the task stamp dropped from `src/result.cpp`'s messages, likewise | `the printer's errors log in the shared style` | — |
   | 8 | the default style loses its bold, so the colour changes | — | `-t ll 31 under FORCE_COLOR logs its exact bytes` |
   | 9 | a task stamp printed for an empty `TASK_ID` | — | `-D 31 without TASK_ID has no task stamp` |
   | 10 (P2) | `-D` applied only after parsing again (the `set_style` statement removed) | `-D and -d apply as the parser reads them` | `-D -p -p 3 stamps its warning` |
   | 11 (P2) | `-d` applied only after parsing again | `-D and -d apply as the parser reads them` | — |
   | 12 (P2) | the task stamp dropped from `src/cli.cpp`'s messages, likewise | `-D and -d apply as the parser reads them` | `-D -p -p 3 stamps its warning` |
   | 13 (P2) | the task stamp dropped from `src/main.cpp`'s messages (`logging::task_id.clear();` at the top of its argument-error handler) | — | `-D -x 31 stamps its error` |
   | 14 (P2) | a pre-scan: `main.cpp` sets the detailed style before parsing if any word is `-D` | — | `-x -D 31 errs in the default style` |
   | 15 (P2) | `-d` also turns on the detailed style | — | `-d 31 logs as without -d` |
   | 16 (P2) | `-D` also sets the level to debug | `-D and -d apply as the parser reads them` | — |

**The pushing phase (P3)** follows the previous cycle's P4, with three helpers copied from
`spec/verdicts-and-exit-statuses/verify/` and adapted:

- `cluster.sh`: the `result.json` checker unchanged; the scan with the stamp rule and its self-test;
  the file list; the ledger.
- `redact.sh`: plus the stamp's host.
- `prepush.sh`: the P2 gate on the final tree, the ledger, the code committed and the scan.
  `prepush.sh scan` runs the scan and its self-test alone.

The P3 gate adds the evidence checks:

- three passed runs of one pushed commit whose code equals `HEAD`'s outside `spec/`, `issues/` and
  `ROADMAP.md`, each with GCC 13 or newer and that commit's job script;
- `sacct` agreeing;
- each `check.log`'s counts equal to this tree's, with no detailed stamp in it;
- the scan covering the evidence.

There is no negative control, because `ci/` doesn't change.
