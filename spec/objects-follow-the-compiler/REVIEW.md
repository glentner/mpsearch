# REVIEW — A compiler or flag change rebuilds every object

> Adversarial QA by `code-review`, run in an isolated context (a fresh subagent at xhigh). The
> correctness pass grades the branch diff against [`GOAL.md`](GOAL.md) and the repo's `AGENTS.md`
> § Invariants **only**. It did not see `PLAN.md` or `TECH.md`. Every finding cites an **executed**
> command.

- **Reviewed commit:** `9e3bd51` · **Base:** `b3a6c5a` (`origin/main`) · **Date:** 2026-10-17
- **Verdict:** changes-requested
- **Cycle:** 1 of ≤3
- **Contract drift check:** `GOAL.md` has two commits after the shape commit, `1ce0338` and
  `355c236`. Both are the foreman's sign-off of the proposed Clarifications (marker text only), made
  before the build; no R-ID changed.

## Verification run

All builds ran in scratch clones of head and base with `/usr/bin/make` (GNU Make 3.81).

- **R1:**
  - At base, a switch after a touch fails to link, and with nothing touched it silently runs the
    other compiler's binaries.
  - At head, every switch in both directions compiles 27/27 objects with the run's compiler,
    including `CXX` from the environment and `make -j8`. Proof per object comes from the C++ ABI in
    `nm` (libc++ `St3__1` against libstdc++ `__cxx11`), and `otool -L` shows the matching runtime.
- **R2:** a logging wrapper compiler shows that, for each `CXXFLAGS`, `CPPFLAGS`, `IEEE` and `BOUNDS`
  change and for the return to defaults, every object is compiled in that run with the new flag and
  none with the old.
- **R3:**
  - no-op runs execute nothing, and the inode and nanosecond mtime of all 30 built files are
    unchanged;
  - touches rebuild the same sets as base;
  - explicitly equal settings run nothing;
  - a same-second switch (outputs dated ahead) rebuilds 27/27, and quoted settings round-trip.
- **R4:** a no-op build costs +6.9 ms (30 runs); a no-op `make check` costs +1.1 ms (12 runs).
- **R5:** nothing is written outside `lib/` and `bin/`; `make clean` leaves no files.
- **R7:** the CI probe's output is byte-identical to base in fresh clones; fresh-clone `make` and
  `make check` match base.
- **R8:** `Makefile` and `AGENTS.md` changed together in `55d2fb9`.
- **R9:** imports under both Pythons leave `git status` clean.

## Requirement → evidence matrix

| R-ID | Status |
|------|--------|
| R1 | ✅ |
| R2 | ✅ |
| R3 | ❌ F1 (✅ on every building path) |
| R4 | ✅ |
| R5 | ✅ |
| R6 | ✅ |
| R7 | ✅ (O2 noted) |
| R8 | ✅ (the new `AGENTS.md` paragraph overstates nothing except as F1 describes) |
| R9 | ✅ |

## Findings

### [MEDIUM/CONFIRMED] F1: a make run that builds nothing deletes the build (R3)
- **Where:** `Makefile:34`, the parse-time `RESET := $(shell … || /bin/rm -f $(BUILT))`, which runs
  whenever make reads the file.
- **Failure scenario:**
  1. Build with clang.
  2. Run `make -n CXX=g++-16 check` (a preview), `make -q CXXFLAGS=-O0`, `make -npq .DEFAULT` (the
     parse-only form editors and shell completion use), or a mistyped goal (`make CXX=g++-16 chek`).
  3. `lib/` and `bin/` are emptied.
  4. A plain `make check`, with settings unchanged since the last build, now recompiles all 27
     objects. R3 requires no compile or link command.
- **Evidence:** head 57 files before, 0 after; base keeps all 56. The follow-up `make check` runs 27
  compiles at head and 0 at base.
- **Foreman's note:** PLAN § 3 recorded this as an accepted deviation, and the foreman accepted it at
  plan sign-off. That was a mistake: code-plan's deviation table never bends an R-ID, and this bends
  R3.
- **Class to fix:** no make invocation that builds nothing changes or deletes any build output.
  That covers `-n`, `-q`, `-p`, a failed or unknown goal, the CI probe inside a developer tree, and
  `-f <repo>/Makefile` from another directory (O1, folded into the class because it is the same
  mechanism). Building runs must still meet R1–R3, including the same-second case.
- **Gate:** the frozen gates passed this, so the fix's gate must evaluate the class: every
  non-building invocation leaves the build byte- and inode-identical, and the next plain build runs
  nothing.

### Out of Scope
- **O1** (`-f` from another directory deletes that directory's files): folded into F1's class above.
- **O2** (the CI probe in a developer tree with other settings deletes that build; its output is
  still correct, so R7 holds): also covered by F1's class.

## Human-gate triggers

- None. The `Makefile` is not on § Invariants' high-blast-radius list or in the coupled core.
