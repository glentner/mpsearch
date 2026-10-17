# PLAN — A compiler or flag change rebuilds what it links

> **Status:** Draft for review · **Last updated:** 2026-10-17 · **Amended:** 2026-10-17, review F1
> (§ 2 the reset, § 3 the deviation removed, § 5, § 6 the class checker; see § 7)
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). No `research/`: a `kind: fix`,
> `appetite: small` cycle on the lean path, with its few unknowns settled by measurement (§ 4).
> Every design element traces to a GOAL R-ID.

## 1. Summary

The `Makefile` records the compile settings that built `lib/` and `bin/` in one file,
`lib/settings`. While make parses the `Makefile`, before it looks at any target, one shell call
compares this run's settings with that record. When they differ, or there is no record, every
object takes a phony prerequisite and the binaries are phony, so all of them are out of date
whatever their timestamps; the record's rule, an order-only prerequisite of every object, runs
before the first compile, removes everything the build writes (the same list `make clean` removes)
and writes the new record. Only that recipe removes anything, so a run that builds nothing
(`make -n`, `make -q`, a goal with no rule, the CI probe) changes nothing, and `-n` and `-q` still
report the rebuild (amended for review F1, § 7). Nothing compares timestamps: GNU make 3.81 on macOS keeps
whole seconds (measured, § 4), so the usual idiom, a stamp file that every object depends on, misses
a switch made in the same second as the last build, which is exactly what a gate's scripted builds
do. An unchanged run removes nothing and costs one `sh` and one `cat`. The rider adds
`__pycache__/` to `.gitignore`. Two phases: the fix, its documentation and the rider; then the
audit, the ledger, the push and the cluster runs.

## 2. Design

### Files

| Path | Change | Phase | R-IDs |
|---|---|---|---|
| `Makefile` | the settings record, the reset, the record's rule, `BUILT` shared with `clean` (below) | P1 | R1–R7 |
| `AGENTS.md` | the three edits below, verbatim, in the same commit as the `Makefile` | P1 | R8 |
| `.gitignore` | one line, `__pycache__/`, appended | P1 | R9 |
| `issues/packaging-and-install.md`, `ROADMAP.md` | the deferral ledger (§ 6) | P2 | — |

Everything else stays as `b3a6c5a` has it: `src/`, `tests/` (with `tests/cli.sh`), `ci/`, `doc/`,
`README.md`. The gates hold every other path to `b3a6c5a`.

### The `Makefile`

What is pinned: the record's path, `lib/settings`; the five variables it records, in the effective
values the `Makefile` computes; the comparison while make reads the `Makefile`, before any target
is examined; the removal only in a recipe, so a run that builds nothing changes nothing (review F1,
§ 7); the record's rule as an order-only prerequisite of every object; one list, `BUILT`, of
everything the build writes, used by the reset and by `clean`; no output from any of it. The compile and link recipes, `MODULES`, `DEPS`,
the `check` recipe, `.DELETE_ON_ERROR` and `-include $(DEPS)` do not change (R7: a fresh clone
prints the same commands as `b3a6c5a`, and the gates compare them). Names other than `lib/settings`
and `BUILT`, and the comment's wording, are the builder's.

The design as amended for review F1 (§ 7; the P1 prototype, which removed `BUILT` from a
parse-time `RESET := $(shell …)`, passed the gate's self-test, § 6) adds this after `DEPS`, comments
aside:

```make
BUILT    := lib/*.o lib/*.d lib/check/*.o lib/check/*.d $(EXE) $(CHECK) lib/settings
SETTINGS := $(CXX) | $(CPPFLAGS) | $(CXXFLAGS) | $(IEEE) | $(BOUNDS)
QUOTED   := '$(subst ','\'',$(SETTINGS))'
CHANGED  := $(shell test "`cat lib/settings 2>/dev/null`" = $(QUOTED) || echo FORCE)

ifneq ($(CHANGED),)
.PHONY: $(EXE) $(CHECK) lib/settings
endif
```

and, beside the directory rule:

```make
lib/settings: | lib
	@/bin/rm -f $(BUILT) && printf '%s\n' $(QUOTED) >$(@)
```

Each object rule gains `$(CHANGED)` as a prerequisite and `lib/settings` as an order-only one
(`src/%.cpp Makefile $(CHANGED) | lib lib/settings`, and `| lib/check lib/settings`), `FORCE:` is
a phony rule with no recipe, and `clean` becomes `/bin/rm -f $(BUILT)`.

Why each piece:

- **The effective values.** `SETTINGS` expands after the `Makefile`'s own assignments, so it holds
  whatever reached the compile and link commands: `CXX` from the command line, from the
  environment (R1's second route, which the `Makefile` honours through `$(origin CXX)`) or the
  `clang++` default, and `CXXFLAGS`, `CPPFLAGS`, `IEEE` and `BOUNDS` from the command line or the
  defaults. A return to the defaults is a change like any other (R2).
- **Compared at parse time, removed by a recipe.** `CHANGED` is a `:=` assignment, so the
  comparison runs while make reads the `Makefile`, before it has stat'ed a single target. On a
  difference, `FORCE` (phony) is a prerequisite of every object and the binaries and the record are
  phony, so make rebuilds all of them with this run's settings whatever the clock says (R1, R2): a
  phony prerequisite is always newer. The binaries are made phony rather than given `FORCE`
  because `bin/mps`'s link names its prerequisites (`$(^)`), which `FORCE` would join. `FORCE` goes
  into the object *pattern* rules, not an explicit `$(OBJECTS): FORCE`, so an object with no source
  still has no rule: an explicit rule would let `make -f <repo>/Makefile` in another directory
  "make" the missing objects and run a link there. The record's recipe, which every object waits
  for, removes `BUILT` before the first compile, so the objects and binaries this goal does not
  build (`lib/check/` under `make`) don't survive under the new record either.
- **Only a recipe removes anything (review F1).** A run that builds nothing runs no recipe: `make -n`
  prints the removal, the compiles and the links without running them, `make -q` exits 1, and a goal
  with no rule here (a typo, `.DEFAULT`, the CI probe's `mps-print-cxx` from stdin) runs nothing, so
  each leaves every file as it was and the next build with the recorded settings does nothing.
  `make -f <repo>/Makefile` from another directory finds no `src/` there, so no object rule applies
  and no recipe runs. Nothing reads `MAKEFLAGS` or `MAKECMDGOALS`, so nothing depends on how a make
  version spells its flags at parse time (GNU Make 4.4 puts long options there too). An interrupted
  switch leaves the new record and only objects built under it, as before.
- **Order-only record.** The record needs only to exist before the first compile writes an object
  under the new settings; as an order-only prerequisite it is never compared by time, so it can
  neither force a rebuild of an unchanged tree nor hide a stale object (R3).
- **One record for `lib/` and `lib/check/`.** `BOUNDS` compiles only `lib/check/`, so changing it
  also rebuilds `lib/`. A switch's speed is a non-goal, and one record is one comparison.
- **One set of objects.** Clarification 2 lets the plan choose. A directory per settings
  (`lib/<hash>/`) keeps both builds warm but still has to relink `bin/mps` and `bin/check` on a
  switch back to objects older than the binaries: the same timestamp problem, moved to the link.
- **`subst`, not `$(call)`.** `$(call)` splits its argument at commas, so a flag such as
  `-Wl,-x` would be cut from the record and a change after the comma missed. `subst`'s third
  argument takes the rest of the line. The quoting escapes `'` for the shell.
- **Silent and fresh-clone neutral.** In a fresh clone there is no record and nothing to remove,
  and the comparison writes nothing, so `ci/test-commit.sh`'s compiler probe
  (`make -s -f Makefile -f - mps-print-cxx CXX=g++`) prints exactly `g++` and leaves no `lib/`
  behind; the build then prints `b3a6c5a`'s commands line for line (R7). The record's recipe is
  silent.
- **Within the limits.** `:=`, `$(shell)`, `$(subst)` and order-only prerequisites are all in
  GNU make 3.81; the shell call uses `test`, `cat` and `rm`, and the rule `printf`, all POSIX
  (R6, § Invariants 5).

How it behaves:

| Situation | What happens |
|---|---|
| fresh clone, CI probe, `make`, `make check` | nothing removed, nothing written by the probe; `b3a6c5a`'s commands; the record appears with `lib/` |
| nothing changed | no removal, no compile, no link (R3) |
| a source or header changed | the objects that depend on it, as at `b3a6c5a` (R3) |
| `CXX`, `CXXFLAGS`, `CPPFLAGS`, `IEEE` or `BOUNDS` changed, or back to the defaults | the record's recipe removes everything under `BUILT` before the first compile and writes the new record; a full build with the new settings, whatever the timestamps (R1, R2) |
| settings changed, but `make -n`, `make -q`, `make -npq .DEFAULT`, a goal with no rule, the CI probe, or `make -f` from another directory | nothing removed or written; `-n` prints the removal and the full rebuild, `-q` exits 1; the next build with the recorded settings does nothing (R3, review F1) |
| a tree built before this cycle (objects, no record) | treated as changed: one full rebuild |
| `make clean` | `BUILT` removed, record included; no file left under `lib/` or `bin/` (R5) |
| a compiler upgraded in place under the same name | not seen (GOAL non-goal); `make -B`, as `AGENTS.md` says |

### `AGENTS.md` (pinned, verbatim)

The gates require exactly these three edits and no other change to the file. The profile's
`verify` row:

```
| verify | `make check`: the checks and the smoke test, both always run |
```

§ Build and run, the `make clean` line becomes two:

```
make clean                # remove what the build wrote: the objects in lib/ and the record of
                          # the settings that built them (lib/settings), the binaries in bin/
```

and the paragraph under the block gains four lines after its second:

```
A build whose compiler or compile flags (`CXX`, `CXXFLAGS`, `CPPFLAGS`, `IEEE`, `BOUNDS`) differ
from those recorded in `lib/settings` first removes everything the last build made, so a switch
needs neither `make -B` nor `make clean`. A compiler upgraded in place under the same name still
needs `make -B`.
```

The release recipe's `clean_paths: [lib/, bin/]` and `gate: [make clean, make check]` stay as they
are: the record lives in `lib/` and `make clean` removes it (R5).

### `.gitignore`

`b3a6c5a`'s two lines and a third, `__pycache__/`, which matches the directory at any depth, so
importing either generator from its own directory leaves `git status` clean (R9). No file under
`tests/` changes.

### Requirement → design map

| R-ID | Design element(s) that satisfy it |
|------|-----------------------------------|
| R1 | `SETTINGS` holds the effective `CXX` (argument, environment or default); on a difference every object and binary is out of date by a phony prerequisite or as phony, and the record's recipe removes `BUILT` before the first compile, so every linked object is compiled by the run's `CXX` |
| R2 | the same, for `CXXFLAGS`, `CPPFLAGS`, `IEEE`, `BOUNDS`; a return to the defaults differs from the record like any change |
| R3 | an unchanged run matches the record, so `CHANGED` is empty and the rules are `b3a6c5a`'s plus the order-only record, which never forces a rebuild; `-MMD` dependencies are `b3a6c5a`'s; a run that builds nothing runs no recipe, so it removes nothing (review F1) |
| R4 | the cost of an unchanged run is one `sh` and one `cat` at parse time; measured in § 6 |
| R5 | the record lives in `lib/`; `BUILT` is everything the build writes and `clean` removes all of it; `clean_paths` and `.gitignore` unchanged |
| R6 | GNU make 3.81 constructs and POSIX utilities only; every gate build uses `/usr/bin/make` 3.81 with both compilers, and the cluster runs use GNU Make 4 |
| R7 | a silent comparison that writes nothing; the probe's goal runs no recipe of this file; recipes unchanged; `ci/` untouched |
| R8 | the three `AGENTS.md` edits above, in the P1 commit that changes the `Makefile` |
| R9 | `__pycache__/` in `.gitignore` |

## 3. Invariant gate (AGENTS.md constitution check)

Checked against `AGENTS.md` § Invariants before the design (gate #1) and again against the drafted
design and the prototype (gate #2). No path on the high-blast-radius list is touched: the change is
the `Makefile`, `AGENTS.md` and `.gitignore`. So triggers 1 and 2 do not fire, and neither does
trigger 3 (§ 4).

- **§ 2, the numeric flags** ("the flags that compile numeric code belong to this invariant"). Their
  values do not change: `IEEE` keeps `-std=c++20 -fno-fast-math -ffp-contract=off`, and the gates
  compare the tree's fresh build byte for byte with `b3a6c5a`'s. The fix strengthens the invariant:
  an object compiled with other flags can no longer be linked under the defaults.
- **§ 5, dependency-free core.** The build still needs only a C++20 compiler and GNU make; the
  reset adds `test`, `cat` and `rm` in the shell make already uses, and the rule `printf`.
- **§ Conventions.** The profile's `verify` row changes in the same commit as the `Makefile`
  (R8, checked by the P2 gate on the branch's commits). "Point to a rule": `BUILT` is written once
  and used by the reset and by `clean`, and `AGENTS.md` describes the behaviour without restating
  the list. No spec ids in the `Makefile`. The `Makefile` keeps its SPDX header.
- **§ Factory, release recipe.** `clean_paths` and `gate` unchanged and still true (R5).
- **§ 1, § 3, § 4.** Not touched: no verdict, expected value or outside code is involved.

### Deviation justifications

None. The table held one row, a parse-time removal that ran whatever the goal (`make -n`,
`make -q`, an unknown goal, the CI probe), accepted at plan sign-off. It bent R3: after such a run
the next build with unchanged settings recompiled everything. A deviation never bends an R-ID, so
the review's F1 removed it, and the row with it (§ 7).

## 4. Rabbit holes (resolved)

No research fan-out (lean path). Each unknown was settled by a measurement on this Mac, in the
planning prototype directory (`$(git rev-parse --absolute-git-dir)/plan-proto/`, not committed).

- **Timestamp resolution of `/usr/bin/make` 3.81** → whole seconds. A target dated
  1700000000.1 s and its prerequisite dated 1700000000.5 s: make says the target is up to date. So
  no design may rest on the record's time against an object's.
- **What a switch does at `b3a6c5a`** → after `make CXX=clang++ check`, `make CXX=g++-16 check`
  calls no compiler at all and exits 0, running clang's `bin/check` again. The `ld` failure GOAL
  describes appears only once some objects have been rebuilt (after a touch or a gate). So a link
  that succeeds proves nothing, and the gates judge by bytes.
- **Are the builds reproducible?** → yes, for both compilers: two fresh `make check` builds of
  `b3a6c5a` in different directories at different times give byte-identical objects, `bin/mps` and
  `bin/check` with clang++ and with g++-16. One exception, found by the gate's first self-test:
  Apple's linker gives a clang++ `-O0` link a new `LC_UUID` every time (identical objects, binaries
  differing only in the UUID and the ad-hoc signature that hashes its page). So the gates hash a
  Mach-O file with those two zeroed and every other byte counted, and compare every output with a
  fresh `make -B` reference built with the same settings.
- **`MAKEFLAGS` at parse time in 3.81** → set (`n`, `sn`, `q`, `kB`), so a dry-run guard was
  possible. The amended design (§ 7) needs no guard: it removes nothing at parse time.
- **`$(call)` and commas** → `$(call)` splits at commas; the prototype quotes with `subst`.
- **Trigger 3** (a gate-shaped deliverable) → not fired. The deliverable is the build's dependency
  tracking, not a check: the set of checks `make check` runs and their code do not change (GOAL
  non-goals), and the gates judge the `Makefile` by comparing its outputs with independent
  `make -B` references of `b3a6c5a`, not by anything the `Makefile` reports. The closest call: a
  stale `bin/check` is a check passing for the wrong binary, which is why the gates compare bytes.
  In place of an adversarial review, the drafted gates were run against five defective trees
  (the old `Makefile` among them) and seen to fail on each check (§ 6).

## 5. Risks & open questions

- **A dry run with other settings** prints the record's removal and the full rebuild and removes
  nothing; `make -q` exits 1 (review F1, § 7). `make -t` touches the objects and leaves the record
  as it was, so the next build with those settings rebuilds; `-t` lies about the build by design.
- **A new output rule** (the packaging seed's install target or a backend) has to take `$(CHANGED)`
  in its prerequisites, or be phony on a change when its recipe names its prerequisites, and add
  its outputs to `BUILT`; the seed's Notes say so.
- **The settings are compared as text.** `CXXFLAGS='-O2  -g'` and `CXXFLAGS='-O2 -g'` differ and
  rebuild. Harmless. A value with a newline cannot round-trip through `$(shell)`, and none is
  plausible on a command line.
- **In-place compiler upgrades** stay a non-goal; `AGENTS.md` says `make -B`.
- **Two makes in one tree at once** were never safe and are not made so.
- **`make -j`.** Not a supported mode and not gated; the record is one order-only target, made
  once before any object.
- **R4's headroom.** The budget is 0.1 s; the prototype costs about 7 ms (§ 6).
- **R9 on this Mac.** Apple's `python3` keeps its bytecode outside the tree, so the rider matters
  for `python3.14` and other Pythons; the gate imports with both.
- **R5's "no file".** Read as no non-directory: `make clean` leaves the empty `lib/`, `lib/check/`
  and `bin/` directories, as `b3a6c5a`'s does.
- **GOAL notes for the foreman** (no R-ID is affected): Clarification 3 still reads "foreman to
  confirm" after the confirming commit `1ce0338`; and the Problem's "the link fails" is what happens
  after a partial rebuild, while a plain switch with every object current reruns the first
  compiler's binaries silently (§ 4). R1's wording covers both.

## 6. Verification strategy

### The phase gates

`spec/objects-follow-the-compiler/verify/`, frozen at the plan commit: `common.sh` (the lanes and
the checks), `outputs.py` (manifests, byte comparisons, dates, the compiler calls), `files.py` (the
pinned edits), `p1.sh`, `prepush.sh` (with a `scan` mode), `p2.sh` (with an `audit` mode),
`cluster.sh` and `redact.sh` (copied from `known-answer-tables`, the ledger rewritten). Each gate
copies the tree as a clone would hold it, runs only `/usr/bin/make` (GNU make 3.81), with clang++
and g++-16, and puts a shim first on `PATH` that logs every compiler call with its arguments.

1. **Files.** Against `b3a6c5a`: only `Makefile`, `AGENTS.md`, `.gitignore` and this spec change in
   P1 (plus `issues/*.md` and `ROADMAP.md` in P2), and all three of the first change; `AGENTS.md` is
   `b3a6c5a`'s with exactly the three edits of § 2; `.gitignore` gains exactly `__pycache__/`; the
   `Makefile` uses nothing newer than make 3.81 (`$(file`, `!=`, `::=`, `undefine`, `.ONESHELL`, …),
   which 3.81 would expand to nothing without a word (R6, R8, R9, non-goals).
2. **Four lanes at once**, each a chain of plain serial makes. For each compiler X (Y the other):
   - **Lane A** exports `b3a6c5a`, runs the CI probe verbatim, a fresh `make` and `make check`
     (the default reference), then three touches (`src/ansi.cpp`, `src/mp.hpp`,
     `tests/check_cli.cpp`, each dated after the outputs) with the objects each recompiles; then,
     in a second export, the references: a fresh `make -B bin/mps bin/check` with each of
     `CXXFLAGS`, `CPPFLAGS`, `IEEE` and `BOUNDS` set to its default plus `-include` of a gate header
     that adds one used string (so every object the variable compiles changes), and a
     `make -B CXXFLAGS=-O0 bin/mps`.
   - **Lane B** walks a git-initialized copy of the tree, never with `-B` or `make clean`: the
     probe; a fresh `make` and `make check`; `make check` and `make` again with nothing changed; the
     same three touches; `make CXX=Y`, then `make CXX=Y check`, then that again; the probe again (in
     a tree Y built); every output dated 2 s ahead (a build in the same second, as make 3.81 sees
     it) and `CXX=X make check` from the environment; then for each variable, outputs dated ahead,
     `make check` with it changed, and the same again; outputs dated ahead and `make check` at the
     defaults; `make CXXFLAGS=-O0`, then plain `make`, each after dating ahead; a last `make check`
     and `git status --porcelain --ignored --untracked-files=all`.
3. **The checks, on what the lanes recorded:**
   - R7: the probe exits 0 and prints exactly `g++` and nothing on stderr, in a fresh copy and in a
     tree another compiler built, and leaves no `lib/` or `bin/` in a fresh copy; a fresh `make` and
     `make check` print `b3a6c5a`'s commands line for line, the same check summaries, and outputs
     byte-identical to `b3a6c5a`'s.
   - R3: every no-op run called no compiler (the shim's log is empty), printed no compile or link
     command, and left every file under `lib/` and `bin/` with its inode, time, size and hash; each
     touch recompiled exactly the objects it recompiled at `b3a6c5a`, a non-empty set smaller than a
     full build.
   - R1: after `make CXX=Y`, `lib/*.o` and `bin/mps` are byte for byte the fresh Y reference's, and
     every compiler call in the run was Y's, a link of `bin/mps` among them; after `make CXX=Y check`
     every object and both binaries are, and the checks pass; after `CXX=X make check` (environment,
     outputs dated ahead) every output is X's reference again.
   - R2: each changed-variable reference differs from the previous one in its binaries (so the
     comparison can tell them apart), and after each change every output equals its reference; so do
     the return to the defaults, `CXXFLAGS=-O0` through plain `make`, and the plain `make` after it.
   - R5: after every setting, `git status` shows nothing outside `lib/` and `bin/`; after the
     timing, `make clean` leaves no non-directory under either.
   - R4: alone, after the lanes: `make check` with nothing to rebuild in `b3a6c5a`'s copy and the
     tree's, interleaved, five each, the least of each, the difference within 0.1 s, once more
     before failing; the two copies' binaries must be byte-identical, so the only difference timed is
     the `Makefile`. Reported beside it: make's own share, a no-op `make bin/mps bin/check`, the
     least of twenty each.
   - R9: in a git-initialized copy, `import generate` from `tests/vectors/` and `tests/known/` under
     python3 and python3.14; `__pycache__/` must appear, and `git status` must show nothing.
   - R6 is every step above running under make 3.81 with both compilers, plus the cluster's
     GNU Make 4 in P2.
4. **Footprint.** At most four makes at once (each one compiler process or one check binary), then
   the timing alone. About 11 minutes. Run one gate at a time. Every make, check and import runs
   under `limit`, in a process group of its own recorded under the gate's temporary directory; on
   exit the gate kills every recorded group and its own lanes, then removes the directory by
   Python's `shutil` after checking it is this gate's own `mktemp` directory. Nothing else is
   deleted.

### R4's baseline

Measured on mac-m2 with the gate's method (`make check` with nothing to rebuild, `b3a6c5a` against
the prototype, interleaved, the least of each), on 2026-10-17:

| Compiler | `b3a6c5a` | prototype | difference | make's own share (`b3a6c5a` → prototype) |
|---|---|---|---|---|
| clang++ | 3.740 s | 3.741 s | +0.001 s | 0.014 s → 0.021 s |
| g++-16 | 6.463 s | 6.492 s | +0.029 s | 0.014 s → 0.020 s |

Five interleaved pairs per compiler (the gate retries once with five more before failing); make's
own share is the least of twenty no-op `make bin/mps bin/check` runs. The load average was about 10
when this ran, straight after the lanes; across the five other self-test runs `b3a6c5a`'s clang++
figure ranged 3.741–3.761 s, and a base-against-base run of the method differed by 0.006 s. So the
reset costs about 7 ms, and 0.1 s stands clear of the noise.

### The gates' self-test

Before the plan commit, the drafted gates ran (`GATE_SELFTEST`, which runs only the named
compilers' lane B, skips the frozen-gate check, counts every failure and never prints green)
against the prototype and against defective `Makefile`s, each meant to break a known set of checks:

| Tree (clang++ lane unless noted) | Defect | Checks that went red |
|---|---|---|
| prototype, both compilers | none | none (exit 3, 663 s) |
| M0 | `b3a6c5a`'s `Makefile`, `AGENTS.md` and `.gitignore`; `tests/cli.sh` edited | files: the edited path, all three files unchanged, `AGENTS.md`, `.gitignore`; R1: `make CXX=g++-16` and `make CXX=g++-16 check` reuse clang's outputs, the switch back links no `bin/mps`; R2: all four variables and `-O0`; R9: `git status` shows `__pycache__/`. R3, R4, R5 and R7 green, as `b3a6c5a` is their reference |
| M1 | the record outside `lib/` (`.settings`), written with a trailing space so it never matches | R7: a fresh `make check` recompiles; R3: every no-op run (compiler calls, printed commands, changed files) and all three touches; R5: `git status` shows `.settings`; R4: +23.4 s |
| M2 | git's idiom: a `FORCE` rule rewriting `lib/settings` only on change, every object depending on it; and a 0.2 s `$(shell sleep)` | R1: the switch back with outputs dated ahead; R2: all four variables, the defaults, `-O0` and plain `make`; R4: the binaries are not `b3a6c5a`'s, and +2.98 s. Its natural switch passed: the idiom works only when the clock helps |
| M3 | the record holds `CXX` only; `clean` leaves the record; `check` skips `tests/cli.sh` | R7: the summaries (twice); R1: `make CXX=g++-16 check` not green; R2: all four variables and `-O0`; R5: `make clean` leaves `lib/settings` |
| M6 | the record written at parse time (creating `lib/`), announced with `$(info)` and `$(warning)`; `-O2` by default | R7: the probe's stdout and stderr (fresh and stale), the probe leaves `lib/`, a fresh `make` and `make check` print other commands and make other bytes; R1: all three; R2: three variables, the defaults, plain `make`; R4: the binaries |

Unit-level, on the gates' own helpers: `outputs.py calls` refuses a call by the other compiler, and
`by` a run with no `bin/mps` link (M0 above, too); `differs` refuses identical references;
`files.py makefile` refuses `!=` and `::=`; P2's same-commit check (R8) is red on a branch that
commits the `Makefile` and `AGENTS.md` apart and green on one that commits them together; P2's
`build.log`/`check.log` patterns pass `known-answer-tables`' cluster logs and catch one doctored
with a `-o lib/ansi.o` line, and its make-version pattern refuses `GNU Make 3.81`; `p2.sh audit`
in a tree with no ledger and no evidence is red on the ledger, R8 and every evidence check. Not seen
red: the probe's exit-status check (a `$(error)` `Makefile` makes the probe exit 2, so the check
has something to catch), and the guards that stop the gate (`die`) when a check would prove nothing
(a touch that rebuilds nothing or everything, `git status` showing nothing under `lib/`, no
`__pycache__/` after the imports). The last matters here: Apple's `python3` (3.9.6) writes its
bytecode under `~/Library/Caches/com.apple.python` (a built-in `pycache_prefix`), so only
`python3.14` creates `__pycache__/` beside the generators and gives R9's check its bite.

The cleanup was exercised too: two self-test runs stopped with `TERM` left no make, compiler or
check process behind, and both temporary directories were gone.

### P2: the cluster runs

P2 runs as two dispatches (TECH). The pushed head runs `ci/test-commit.sh` on the three targets;
the P2 gate holds the redacted evidence to: three passed runs of one pushed commit equal to `HEAD`
outside `spec/`, `issues/` and `ROADMAP.md`; GCC 13 or newer and GNU Make 4 or newer (R6 beyond
3.81); the check step's summaries equal to this tree's; the build step linking `bin/mps`; and the
check step compiling only `lib/check/` objects and `bin/check`, never `lib/*.o` or `bin/mps`, which
is R3 under GNU Make 4 on a fresh clone (the build and check steps share their settings). The
evidence is `<target>.json`, `.out`, `.build.log` and `.check.log` per target, and `jobs.tsv`, each
through `redact.sh`. The scan reads `~/.config/mpsearch/site.env`.

### The deferral ledger

`spec/objects-follow-the-compiler/evidence/ledger.md` with `## Deferrals`. The one known deferral:
`issues/packaging-and-install.md` gains a Notes bullet, "deferred from
`objects-follow-the-compiler`": an install target and any backend keep the settings record, the
comparison before any target with the removal only in the record's recipe, and `BUILT` as what
`make clean` removes, and add their own outputs to `BUILT` (amended for review F1). Anything the build defers goes the same way (`AGENTS.md` § Factory).

### Review F1: the class checker

The frozen gates passed F1 (a make run that builds nothing deleted the build), so P2's `verify:` adds
a class checker outside `verify/`: `spec/objects-follow-the-compiler/remediation/f1-quiet-runs.sh`.
In a copy of the tree built once with clang++ and dated 2 s ahead, each case in its own copy of that
build, with settings other than the build's: `make -n CXX=g++-16 check`, `make -q CXXFLAGS=-O0`,
`make -npq CXX=g++-16 .DEFAULT`, `make CXX=g++-16 chek`, the CI probe verbatim, and
`make -f <copy>/Makefile CXX=g++-16 [check]` from a directory holding `lib/` and `bin/` files of
its own. After each, every file under `lib/` and `bin/` (and the other directory's) keeps its inode,
size, nanosecond mtime and bytes, no compiler ran, and the next plain `make check` compiles and links
nothing and passes. `-n` must print g++-16's compiles and links and `-q` exit 1; with the build's
own settings `-n` prints no compile and `-q` exits 0. Process groups and the checker's own
`mktemp` directory are cleaned up as the gates do. Seen red on `9e3bd51`'s `Makefile` (16 failures:
every case changed the build, every follow-up recompiled, and the other directory lost its files)
and on a mutation that keeps the recipe-only removal but drops `$(CHANGED)` and the phony binaries
(`-n` hid the rebuild, `-q` exited 0); green on the amended `Makefile`.

## 7. Amendments

- **2026-10-17, review F1 (R3).** The reset no longer removes anything while make reads the
  `Makefile`. The comparison still runs there; on a difference it makes every object (a phony
  prerequisite in the pattern rules) and both binaries and the record (phony) out of date, and the
  record's recipe removes `BUILT` before the first compile. § 1, § 2 (the pin, the code, the
  reasons, the behaviour table, the R-map), § 3 (the deviation row removed), § 4, § 5 and § 6 (the
  ledger bullet, the class checker) follow. The `AGENTS.md` paragraph ("A build whose compiler or
  compile flags … differ … first removes everything the last build made") stays true as pinned: a
  run that builds nothing is not a build, so the pin and R8 are unchanged. The candidate the
  foreman named (anchor `BUILT` to the `Makefile`'s directory, skip the parse-time removal under
  `n`/`q`/`p` in `MAKEFLAGS` or an unknown goal, and a phony prerequisite in those modes) was not
  needed: with the removal in a recipe, no mode test and no anchoring is involved, and `make -p`
  alone, which builds, behaves as a build.
