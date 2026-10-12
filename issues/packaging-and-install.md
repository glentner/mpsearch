---
status: unshaped
kind: feature
appetite: small
---

# Install it on a cluster: `make install`, backends, a container, a release

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

`mps` builds into `bin/` inside the source tree and nowhere else. A burn-in tool has to be installed
once per system image, or per module tree, with the backends that system's hardware needs. It then
has to report exactly which build ran, because a fleet comparison is meaningless across mixed
builds. The repo has no install target and no tags, and the version string is the only build
identity.

## Why it was deferred

Packaging follows the features it packages. Burn-in on real systems (Part III) is where it first
matters.

## Outcome / vision

- `make install PREFIX=…` installs the binary, plus any runtime kernel sources a backend needs.
- `mps --version` names the version, the commit, the compiler, the build flags and the backends
  compiled in, and a result record carries the same build identity.
- A container recipe (Apptainer) builds the CPU and NVIDIA variants reproducibly.
- Releases follow the profile's release recipe and get signed tags.

## Sketch of the acceptance criteria

- **R1** — `make install PREFIX=<dir>` SHALL install `mps` and its runtime files under `<dir>` and
  nothing outside it.
- **R2** — `mps --version` SHALL print the version, the commit, the compiler and the backends
  compiled in.
- **R3** — Every result record SHALL carry the same build identity as `--version`, plus the
  binary's hash.
- **R4** — The container recipe SHALL build `mps` from a tagged release, and the built image SHALL
  pass `make check` (or the installed equivalent).

## Notes

- Shaping decides whether to add a Spack package or modulefile template, or to leave site
  integration to sites.
- The release recipe in `AGENTS.md` § Factory gets its `gate` updated in the same commit
  (§ Conventions).
- Found by: factory adoption review, 2026-10-11.
