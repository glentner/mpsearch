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

## Review cycle 2 — changes-requested (2026-10-17)

- **Reviewed commit:** `59f6b4b` · **Mode:** a fresh full blind pass at xhigh (`GOAL.md` only from
  `spec/`), told to grade every make invocation, not only builds.
- **R1–R9: met by execution.**
  - Shims log every compile and the shas of each output and input; `prov.py` traces each binary to
    its objects' compiles, and `nm` ABI checks agree (27/27 after each switch).
  - Every flag change and return to defaults rebuilt all 27.
  - A 14-step incremental sequence gave output byte-identical to base on both compilers.
  - R4: +6.7–8.6 ms.
  - `make clean` leaves 0 files, the CI probe matches base, and `__pycache__` stays clean.
- **Cycle 1's F1 is fixed.** None of these changed anything in `lib/` or `bin/`: `-n`, `-n check`,
  `--dry-run`, `-q`, `-npq`, `-p -n`, the bash and zsh completion commands, a mistyped goal (with
  and without `-k`), `-n -B`, `-C . -n`, the CI probe, `-nsp .PHONY`.
  - After a switch, `-q` exits 1 and `-n` prints the rebuild.
  - `-t`, `-f` from another directory and `-C` behave correctly.
  - In six interrupted mid-switch trials (SIGINT at 0.02–6 s into `-j4` switches), every leftover
    file matched the recorded compiler.

### [LOW/PLAUSIBLE] F1 (cycle 2): phony binaries lose `.DELETE_ON_ERROR`
- **Where:** `Makefile:40-42`, `.PHONY: $(EXE) $(CHECK) lib/settings` when `CHANGED` is set (every
  switch, and every fresh clone).
- **Failure scenario:**
  1. A linker that writes its output in place is interrupted, or fails partway.
  2. Base deletes the half-written `bin/mps` (`.DELETE_ON_ERROR`, and make's interrupt cleanup);
     head keeps it, because phony targets are exempt.
  3. `lib/settings` is already rewritten, so the next run with the same settings calls the partial
     binary up to date. Only `make -B` or `make clean` recovers.
- **Evidence:** a constructed in-place linker (`fake/inplace-ld`). Head keeps `PARTIAL` and the next
  `make` says "`bin/mps' is up to date"; base prints "Deleting file `bin/mps'" and relinks.
  - Apple ld never left a partial file in 120 interrupted links, so this host's real toolchain
    doesn't show it. GNU ld on Linux plausibly does.
- **Triage (foreman, under the 2026-10-11 delegation): fix.** It is a regression introduced by this
  cycle, in a safety property the Makefile declares on purpose (`.DELETE_ON_ERROR`), and it
  reproduces the defect class this cycle removes: a stale output treated as current. **Class to
  fix:** every output the Makefile builds keeps `.DELETE_ON_ERROR` and interrupt cleanup, while
  switches still force a rebuild and non-building runs still touch nothing.

### Reviewer disclosure
- The reviewer's own driver script ran `/bin/rm -rf lib bin` in its scratch clones (build output
  only), bypassing the shell's `del` rule. It switched to `make clean` once it noticed.

### Checks not run
- GNU Make 4.x and GNU ld (no `gmake` or Linux ld on this host); the cluster runs cover the building
  path under Make 4.

## Review cycle 3 — approved (2026-10-17)

- **Reviewed commit:** `017b50e` · **Mode:** a fresh full blind pass at xhigh (`GOAL.md` only from
  `spec/`), grading every make invocation.
- **Findings:** none (no CONFIRMED, no PLAUSIBLE). Cycle 2's F1 is fixed.
- **Verification run:**
  - **R1–R2:** a logging wrapper on `PATH` records each compile and link. Each object on a link line
    was compiled in that run by the expected compiler with the required flags, and is sha-identical;
    `nm` ABI and `otool -L` agree.
    - Switches in both directions, from the environment, under plain `make`, under `-j8`, and in the
      same second all pass.
    - Each flag variable and the return to defaults pass.
  - **R3:** rebuild sets match base for no-ops and six touches, on both compilers.
  - **R4:** +5.8–7.9 ms.
  - **R5:** clean paths; `make clean` empty.
  - **R6:** Make 3.81 only.
  - **R7:** the CI probe gives byte-identical output, and fresh-clone echoed output matches base.
  - **R8:** `55d2fb9` changes `Makefile` and `AGENTS.md` together, and the text is accurate.
  - **R9:** python3.14 leaves `git status` clean.
- **Non-building invocations,** all leaving the tree fingerprint-identical with a switch pending:
  `-n`, `-q` (rc 1 on a switch, rc 0 otherwise), the bash and zsh completion commands, unknown goals
  (with and without `-k`), `-C`, and `-f` from another directory.
- **`-t`:** the next real build still rebuilds.
- **Failed and interrupted compiles and links**, with a constructed in-place compiler, SIGINT and
  SIGTERM: every partial file is deleted, and the rerun builds exactly what is missing.
- **Settings quoting:** awkward values round-trip.
- **Parity with base:** `.DELETE_ON_ERROR` is kept, `.PHONY` adds only `FORCE`, the echoed commands
  match, and `make clean` adds only `lib/settings`.

### Out of Scope (declined, no action; each is identical at base or requires deliberate misuse)
- `make -j clean check` races: base does the same, and serial `make clean check` passes.
- `make -o lib/settings CXX=<other>` bypasses the record: this needs a deliberate `-o` on an
  internal file.
- SIGKILL during a compile can leave a partial object: no make can clean up after SIGKILL, and base
  has the same exposure.

## Human-gate triggers

- None in any cycle. The `Makefile` is not on the high-blast-radius list or in the coupled core.
