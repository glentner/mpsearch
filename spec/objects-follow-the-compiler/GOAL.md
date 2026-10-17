# GOAL — A compiler or flag change rebuilds what it links

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** objects-follow-the-compiler
- **kind:** fix
- **appetite:** small

## Problem

The repo must build with both Apple clang and GCC (`AGENTS.md` § Build and run), and every phase
gate builds with both. But after a build with one compiler, a plain `make` or `make check` with the
other (`make CXX=g++-16 check` after `make check` with the default `clang++`, or the reverse) does
not rebuild: it links the objects the first compiler left in `lib/` and `lib/check/`, and the link
fails with `ld: symbol(s) not found`. Only `make -B` recovers. A mix that happened to link would be
worse: a `bin/check` that neither compiler built alone, passing or failing for reasons no one chose.
The same holds for a change of compile flags on the command line (`make CXXFLAGS=-O0`, then plain
`make`), where the flags that compile numeric code belong to § Invariants 2.

The defect is **pre-existing** on `main`. The object rules (`Makefile`, `lib/check/%.o: src/%.cpp`,
`lib/check/%.o: tests/%.cpp`, `lib/%.o: src/%.cpp`) have named no prerequisite that changes with the
compiler since `655c75d` added the clang and GCC build. The profile's `verify` row documents the
workaround ("`make -B check` after changing `CXX`"), and every builder of `reference-big-integer`
(P1–P4, 2026-10-16) hit it after a phase gate. Whatever this cycle settles for the object layout,
`make clean` and the release recipe's `clean_paths` must still cover it.

A rider in the same area: `.gitignore` lists only `bin/*` and `lib/*`. Importing
`tests/vectors/generate.py` or `tests/known/generate.py` as a Python module (their own commands run
them as scripts and leave nothing) leaves a `__pycache__/` beside it, which `git status` then
reports as untracked and a later cycle's clean-tree and footprint gates reject.

## Outcome / vision

Switching compilers, or compile flags, needs no extra flag and no `make clean`: a plain `make` or
`make check` notices and builds a correct binary with what it was given, while an unchanged setup
stays exactly as fast and incremental as today. The profile's `verify` row loses its parenthesis,
and importing a generator leaves the tree clean.

## Acceptance criteria (the contract)

Here "the compile settings" are `CXX`, whether given on make's command line or in the environment,
and `CXXFLAGS`, `CPPFLAGS`, `IEEE` and `BOUNDS` given on make's command line: everything that
selects the compiler or its flags in the build's compile and link commands.

- **R1** — WHEN `make` or `make check` runs with a `CXX` other than the one that built the objects
  already in the tree (`g++-16` after `clang++`, and `clang++` after `g++-16`), the build SHALL
  succeed without `-B` or `make clean`, `make check` SHALL pass, and every object linked into
  `bin/mps` and `bin/check` SHALL have been compiled by that run's `CXX`. (Today the link fails with
  `ld: symbol(s) not found`.)
- **R2** — WHEN `make` or `make check` runs with `CXXFLAGS`, `CPPFLAGS`, `IEEE` or `BOUNDS` other than
  those that built the objects already in the tree (including a return to the defaults), every
  object linked into `bin/mps` and `bin/check` SHALL have been compiled with that run's settings.
- **R3** — WHILE the compile settings are unchanged since the last build, `make` and `make check`
  SHALL recompile only the objects whose sources or headers changed, as at `b3a6c5a`; with nothing
  changed, they SHALL run no compile or link command.
- **R4** — `make check`, run with nothing to rebuild, SHALL take no more than 0.1 s longer than at
  `b3a6c5a` on the same machine. The plan records the baseline it measures and how.
- **R5** — The build, with either compiler and any compile settings, SHALL write files only under
  `lib/` and `bin/`, so `.gitignore` and the release recipe's `clean_paths` still cover it; and
  `make clean` SHALL then leave no file under either.
- **R6** — The `Makefile` SHALL keep working with GNU make 3.81 (macOS `/usr/bin/make`) and need
  nothing beyond a C++20 compiler and a POSIX shell with its standard utilities (§ Invariants 5);
  R1–R5 SHALL hold under make 3.81 with both compilers.
- **R7** — `ci/test-commit.sh` SHALL need no change: its compiler probe
  (`make -s -f Makefile -f - mps-print-cxx CXX=g++`) SHALL still print exactly the compiler name,
  and `make` and `make check` from a fresh clone SHALL behave as at `b3a6c5a`.
- **R8** — In the same commit as the `Makefile` change, the profile's `verify` row in `AGENTS.md`
  SHALL drop "`make -B check` after changing `CXX`" (§ Conventions: the profile's `verify` changes
  with its target), and `AGENTS.md` § Build and run SHALL still describe correctly where the objects
  go and what `make clean` removes.
- **R9** — WHEN `tests/vectors/generate.py` or `tests/known/generate.py` has been imported as a
  Python module from its own directory, `git status --porcelain` SHALL report no untracked file.
  (Today it reports `__pycache__/`.)

## Non-goals (no-gos)

- A compiler upgraded in place under the same name (an Xcode or Homebrew update of `clang++` or
  `g++-16`), or a `PATH` change that resolves the same `CXX` name to another binary. `make -B`
  stays the answer there; no R-ID covers it.
- Keeping both compilers' builds warm. Switching back MAY rebuild; whether it does is the plan's
  choice of layout (the seed's Notes weigh a per-compiler directory against a settings stamp).
- Link-only variables (`LDFLAGS`, `LDLIBS`), which the `Makefile` does not use today.
- Any change to `src/`, `tests/` (both high-blast-radius paths hold expected values), `ci/`,
  `bin/mps`'s behaviour, or the set of checks `make check` runs: `tests/cli.sh` passes unedited.
- An install target, backends or a new build system: `issues/packaging-and-install.md` reshapes
  the `Makefile` later and follows whatever object layout this cycle sets.
- Making a rebuild after a switch faster than today's `make -B`: R4 bounds only the no-op run.

## Clarifications

- **Q:** Do compile-flag changes count, or only `CXX`? — **A:** Both, as R2: `CXXFLAGS`,
  `CPPFLAGS`, `IEEE` and `BOUNDS` on the command line, since a stale mix of flags is the same
  defect and § Invariants 2 owns the numeric flags. `CXX` from the environment counts too, because
  the `Makefile` honours it. In-place compiler upgrades and link-only variables are out (Non-goals)
  (confirmed by the foreman, 2026-10-17).
- **Q:** Must switching back to the first compiler be free? — **A:** No. R1–R3 require correctness
  and an unchanged incremental build; the layout, and so whether both builds stay warm, is the
  plan's (confirmed by the foreman, 2026-10-17).
- **Q:** How is the timing budget handled? — **A:** 0.1 s over `b3a6c5a` for a no-op `make check`
  (R4). `reference-big-integer` allowed +3.0 s over `c9b82ff` and used +2.4 s, and
  `known-answer-tables` took a 0.5 s budget on top, so about 0.1 s of that headroom is left; a
  dependency check that costs more than that is a design problem, not a budget question (confirmed
  by the foreman, 2026-10-17).
- **Q:** Does `AGENTS.md`'s `make -B` note change? — **A:** Yes, in the same commit as the
  `Makefile` (R8), and § Build and run follows any move of the objects. No replacement note is
  required for the in-place-upgrade case (confirmed by the foreman, 2026-10-17).
- **Q:** Is the `__pycache__` rider in this cycle, and for which generators? — **A:** Yes (R9), for
  both `tests/vectors/generate.py` and `tests/known/generate.py`; the second postdates the seed. The
  rider changes no file under `tests/` (confirmed by the foreman, 2026-10-17).
- **Q:** Does the cluster job matter? — **A:** Its builds start from a fresh clone, so the defect
  never reaches it. But it runs the `Makefile` with an injected rule and reads the output, so R7
  holds it working unchanged (confirmed by the foreman, 2026-10-17).

## Related materials

- Seed: `issues/objects-follow-the-compiler.md` (status `unshaped` at promotion); `ROADMAP.md`
  Part I, "A compiler change rebuilds every object".
- Source: `Makefile` (the object and link rules, `clean`), `.gitignore`, `AGENTS.md` § Build and
  run, § Factory (profile `verify`, release recipe `clean_paths` and `gate`), § Invariants 2 and 5,
  § Conventions; `ci/test-commit.sh` (the compiler probe and the build steps); `ci/targets/`
  (`make_vars='CXX=g++'`).
- Origin of the defect: `655c75d` (`spec/build-and-test-foundation/`).
- Found by: `spec/reference-big-integer/` P1–P4, recorded in its P5 ledger (2026-10-16).
- Related: `issues/packaging-and-install.md`.
