---
status: unshaped
kind: feature
appetite: small
---

# A standing check for the cluster test job

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

Nothing in `make check`, the profile's standing gate, exercises `ci/test-commit.sh` or the target
files under `ci/targets/`. A change to the job, or a malformed target file, passes every check the
repository runs and is found only when a cluster run fails or, worse, reports the wrong thing.

Evidence:

- `Makefile:34-35`: `check` runs `bin/check` (the unit checks built from `tests/*.cpp`) and
  `sh tests/cli.sh bin/mps`, and nothing else.
- Nothing under `tests/` or in the `Makefile` names `ci/` (`grep -rn 'ci/' tests/ Makefile` finds
  nothing).
- The job's behaviour was proved by `spec/cluster-test-jobs/verify/p1.sh` to `p4.sh`: the happy path
  on a `file://` fixture with a fake login profile and module command, every failure path in
  `spec/cluster-test-jobs/research/04-gate-design.md`'s table, the target-file grammar, and the real
  cluster runs. Those are phase gates. Once the cycle merges they are records of the cycle, not
  standing gates, and they need tools the standing gate does not (`dash`, `rsync`, `python3`, a site file
  outside the repository), so nothing re-runs them.
- The `AGENTS.md` Profile row `cluster test` points at the job's header as its manual; nothing keeps
  the header and the behaviour in step either.

## Why it was deferred

Introduced by `cluster-test-jobs`, which added the job. That cycle's GOAL excluded "any change to
product source (`src/`) or to `make check` itself" (`spec/cluster-test-jobs/GOAL.md`, Non-goals), so
a standing check could not land with it.

## Outcome / vision

`make check` runs a small, dependency-light check of the job: a happy run and the main failure paths
against a local fixture repository, with a stand-in module command and compiler, needing only what
the job itself needs. A regression in the job or a malformed target file turns `make check` red on
the author's Mac, before any cluster time is spent.

## Sketch of the acceptance criteria

- **R1** — WHEN `make check` runs, it SHALL run a check of `ci/test-commit.sh` that needs only a POSIX
  shell and its standard utilities, git and make: no cluster, network, site file or interpreter.
- **R2** — WHEN the check runs the job on a commit of a local fixture repository, with a stand-in
  module command and compiler, it SHALL fail unless the job exits 0, prints every step in order, and
  writes a `result.json` naming that commit with every step passed.
- **R3** — WHEN the check gives the job a hash that is not in the fixture, a wrong number of
  arguments, or a commit that does not compile, it SHALL fail unless the job exits with the status its
  header documents and, where a run directory exists, `result.json` records the failed step and
  `not_run` after it.
- **R4** — WHEN any committed file under `ci/targets/` does not follow the grammar in the job's header,
  the check SHALL fail naming the file and the line.
- **R5** — The check SHALL write only inside a temporary directory that it removes, and SHALL leave the
  working tree as it found it.

## Notes

- Related: `spec/cluster-test-jobs/` (the job, its gates and research 04, which the check can borrow
  from: the fixture repository, the fake profile and the wrapper compiler); `AGENTS.md` Profile row
  `cluster test`.
- Reading `result.json` without an interpreter: the job writes one field per line, so fixed-field
  `grep`/`sed` reads are enough for the statuses the check asserts.
- Found by: `cluster-test-jobs` P4 (the foreman's deferral review).
