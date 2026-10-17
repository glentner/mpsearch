---
status: unshaped
kind: fix
appetite: small
---

# A compiler change rebuilds every object

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The Makefile keeps one set of objects whatever the compiler. `lib/%.o` and `lib/check/%.o` depend
on their source, the `Makefile` and (through `-MMD`) the headers, but not on `CXX` or the flags. So
after a build with one compiler, a plain `make` or `make check` with the other (`make CXX=g++-16`
after Apple clang, or the reverse) finds every object up to date and links the first compiler's
objects with the second. With Apple clang's libc++ objects and GCC's libstdc++ the link fails; a
mix that happens to link would test a binary neither compiler built alone.

Evidence:

- `Makefile`: the object rules name no file that changes with `CXX`; `bin/mps` and `bin/check` are
  linked from whatever objects are there.
- `AGENTS.md` § Factory, the profile's `verify` row, works around it: "`make -B check` after
  changing `CXX`". Every builder of `reference-big-integer` (P1 to P4, 2026-10-16) hit it after a
  phase gate, which builds with both compilers, and had to rebuild with `make -B` before a plain
  build.
- Pre-existing on `main`: no object rule has depended on the compiler since `655c75d` added the
  clang and GCC build.

## Why it was deferred

Found during `reference-big-integer`, whose plan fixed the Makefile's diff to one line (`MODULES`
gaining `reference`), and the gates hold it there. The workaround is documented, so nothing is
wrong once it is followed.

## Outcome / vision

Switching compilers needs no flag: the build notices and rebuilds what the other compiler made,
and the profile's `verify` row loses its parenthesis.

## Sketch of the acceptance criteria

- **R1** — WHEN `make` or `make check` runs with a `CXX` (or compile flags) other than the last
  build's, the build SHALL recompile every object it links rather than reuse the other build's.
- **R2** — The Makefile SHALL stay within GNU make 3.81 and need nothing beyond a POSIX shell
  (§ Invariants 5).
- **R3** — The profile's `verify` row in `AGENTS.md` SHALL drop "`make -B check` after changing
  `CXX`" in the same commit (§ Conventions, the profile's `verify` changes with its target).

## Notes

- Two shapes to weigh at shaping: objects under a directory keyed by the compiler (`lib/<cc>/`),
  which keeps both builds warm, or a stamp file holding `CXX` and the flags that every object
  depends on, rewritten only when it changes. `make clean`'s paths and the release recipe's
  `clean_paths` follow whichever is chosen.
- Rider, same area: `.gitignore` lists `bin/*` and `lib/*` only. A session that imports
  `tests/vectors/generate.py` as a module (the files' own commands run it as a script and leave
  nothing) leaves `tests/vectors/__pycache__/`, which a clean-tree check then reports. Adding
  `__pycache__/` to `.gitignore` costs one line.
- Related: `issues/packaging-and-install.md` (an install target and backends reshape the Makefile
  too; whichever lands first sets the object layout).
- Found by: `reference-big-integer` P1–P4, recorded in its P5 ledger (2026-10-16).
