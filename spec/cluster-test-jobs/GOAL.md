# GOAL — Test any commit on a cluster, built fresh from GitHub

> **Origin spec.** The *what* and *why* — the locked contract `code-review` grades against.
> The *how* lives in [`PLAN.md`](PLAN.md) and [`TECH.md`](TECH.md) (written by `code-plan`).

- **slug:** cluster-test-jobs
- **kind:** feature
- **appetite:** small

## Problem

Every target beyond the author's Mac runs on Slurm clusters: GCC on Linux x86-64 at the AVX2 and
AVX-512 tiers, aarch64 Linux, and later NVIDIA GPUs through CUDA, AMD GPUs through HIP, and the
burn-in itself. Testing there by hand means interactive shells, builds in home directories, and
leftover state from the last attempt. That is unrepeatable, and a result means nothing unless it
names exactly what was built: which commit, which toolchain, which node.

The gap is already open. Cycle 1 (`spec/build-and-test-foundation/`) promised that `make` succeeds
"with Apple clang or with GCC 13 or later", but verified it only at GCC 16 on macOS; its review marks
GCC 13–15 and glibc as coming "with the cluster jobs" (`spec/build-and-test-foundation/REVIEW.md`,
R1 row; `PLAN.md`, "GCC versions"). The Makefile picks `clang++` unless `CXX` is set (`Makefile`,
`ifeq ($(origin CXX),default)`), so a GCC build on a cluster has to say so. `make check` now exists on
`main` (cleared 2026-10-11), so a job has something to run.

A cycle needs one mechanism it can call for any target and any phase: push the branch, submit a job
with the commit hash, read back a result.

## Outcome / vision

A batch job script under `ci/` takes a commit hash and a target name. Submitted inline through Slurm,
with no checkout on the login node, it clones the public repository into a fresh run directory in the
user's scratch, checks out exactly that commit, loads the target's toolchain, runs `make` and
`make check`, and leaves the logs and a machine-readable `result.json` in the run directory. Each
target is one small `key=value` file under `ci/targets/`; adding a target touches only that file.
Nothing site-private (accounts, QoS, user names, internal paths) is committed. The factory submits and
collects through the cluster's MCP server, and this cycle ends with real, passing runs on three GCC
targets and a failing negative control.

## Acceptance criteria (the contract)

Here *the job* is the batch job script under `ci/`, and *a target file* is one file under
`ci/targets/`. Exact names are the plan's call.

- **R1** — WHEN the job script, taken unchanged from the tested commit, is submitted through Slurm
  with the commit hash and target name as its arguments, it SHALL build and test that commit with no
  prior clone or build on the login node.
- **R2** — WHEN the job runs, it SHALL clone the repository from
  `https://github.com/glentner/mpsearch.git`, or from the URL in an override environment variable
  when that is set, and check out exactly the given commit, so that `git rev-parse HEAD` in the build
  tree equals the full hash recorded in `result.json`.
- **R3** — IF the given hash is not a commit in the cloned repository, THEN the job SHALL print a
  message containing "not a commit" and naming the hash, write `result.json` with an overall failed
  status and a failed checkout step, and exit non-zero.
- **R4** — IF the job is given the wrong number of arguments, or a target name with no target file,
  THEN it SHALL print a message saying which, run no build, and exit non-zero.
- **R5** — The job SHALL run in a new directory `<scratch>/mpsearch/runs/<hash>/<target>/<jobid>/`,
  where `<scratch>` is an override environment variable when set and otherwise the site's scratch
  variable. IF neither is set, or the run directory already exists, THEN the job SHALL print a
  message saying so and exit non-zero without building. Apart from the scheduler's own output file,
  it SHALL write nothing outside the run directory, and SHALL never reuse an earlier clone or build.
- **R6** — The job SHALL run these steps in order, timing each and keeping each step's log in the run
  directory: clone, checkout, toolchain load (the target's modules), build (`make` with the target's
  make variables), check (`make check` with the same variables). A step after a failed step SHALL NOT
  run, and `result.json` SHALL record it as not run.
- **R7** — Whether every step passes or one fails, the job SHALL leave a `result.json` in the run
  directory that names the full commit hash, the target, the cluster name and node hostname, the
  start and end times, the compiler identification (the first line of its version output), the make
  version, each step's status and duration in seconds, and the overall status. The job SHALL exit 0
  when every step passes and non-zero when any step fails.
- **R8** — A target file SHALL consist of plain `key=value` assignments that a POSIX shell can
  source and a line-oriented parser can read without executing them. It SHALL carry the target's
  public settings: partition, GPUs or gres, CPUs, memory, time limit, modules to load, and make
  variables. Adding a target SHALL require only a new target file, with no change to the job script.
- **R9** — Slurm account and QoS SHALL be taken from the environment at submission
  (`MPS_SLURM_ACCOUNT`, `MPS_SLURM_QOS`) and SHALL NOT appear in any committed file. Nothing committed
  in this cycle SHALL name an account, QoS, user name, or site-internal path beyond generic
  placeholders; partitions and module names, which the site documents publicly, are allowed.
- **R10** — The job SHALL need only a POSIX shell and its standard utilities, git, the site's module
  command, and the target's toolchain: no Python, jq, or other interpreter.
- **R11** — The repo SHALL define three targets: GCC on an x86-64 AVX-512 node, GCC on an x86-64 AVX2
  node, and GCC on an aarch64 NVIDIA GB10 node, each loading a GCC 13 or newer module. WHEN the
  cycle's pushed head is submitted on each of the three through the cluster's MCP server, its
  `result.json` SHALL report overall success and a GCC 13 or newer compiler identification. WHEN a
  hash that is not in the repository is submitted on one target, the run SHALL behave as R3 states.
- **R12** — `AGENTS.md` SHALL name the job script and the target directory (its Map, and a profile
  row saying how a cycle tests a commit on a cluster), and the job script's header SHALL state its
  arguments, its environment variables, and the run-directory layout.

## Non-goals (no-gos)

- Hosted CI; the profile's `ci` stays `none`.
- GPU targets (CUDA, HIP): they arrive with their backend cycles (`issues/cuda-backend.md`,
  `issues/hip-backend.md`).
- Burn-in runs, benchmarks, or any performance numbers.
- Any change to product source (`src/`) or to `make check` itself.
- Scheduling policy beyond the target file: no retries, queue selection logic, job arrays, or
  dependency chains.
- The cluster MCP tooling itself; the repo only has to be submittable through it (R1).
- Copying results out of scratch; scratch results are disposable, and a cycle copies the summary it
  relies on into its own records.

## Clarifications

The author delegated every gate for this effort to the foreman; these are the foreman's decisions.

- **Q:** Where do the files live, and what shape is a target? — **A:** Under `ci/`: a batch job
  script taking a commit hash and a target name, and one small file per target under `ci/targets/`
  of plain `key=value` shell assignments (partition, gres or GPUs, CPUs, memory, time, modules, make
  variables) that a POSIX shell can source and a simple parser can read. Exact names are the plan's
  call (resolved 2026-10-11).
- **Q:** Does submission need a clone on the login node? — **A:** No. The job script is
  self-contained and submittable as inline content through `sbatch` with the hash and target as
  arguments. It clones the public repository itself (default
  `https://github.com/glentner/mpsearch.git`, overridable by an environment variable), checks out
  exactly the hash, and fails with a clear message if the hash is not a commit in that repository
  (resolved 2026-10-11).
- **Q:** Where do Slurm account and QoS come from? — **A:** They are user- and site-specific and
  never committed. They come from the environment at submission (e.g. `MPS_SLURM_ACCOUNT`,
  `MPS_SLURM_QOS`). Target files name public settings only; partitions and modules are public in the
  site's documentation (resolved 2026-10-11).
- **Q:** Where does a run live? — **A:** In a fresh directory
  `<scratch>/mpsearch/runs/<hash>/<target>/<jobid>/`, scratch from an override variable or the site's
  scratch variable. The job writes nothing outside its run directory and never reuses an earlier
  build. The run directory is kept (logs and result) for the factory to read; scratch results are
  disposable (resolved 2026-10-11).
- **Q:** Which steps, and what does the result hold? — **A:** Clone, checkout, toolchain load, build
  (`make`), check (`make check`), each timed. `result.json` names the hash, target, cluster/host,
  start and end times, compiler identification (first line of the compiler's version), make version,
  each step's status and seconds, and the overall status. The job exits non-zero if any step fails,
  and still writes `result.json` (resolved 2026-10-11).
- **Q:** Which targets come first? — **A:** GCC on an x86-64 AVX-512 node, GCC on an x86-64 AVX2
  node, and GCC on an aarch64 NVIDIA GB10 node, each pinning a GCC 13 or newer module, which closes
  cycle 1's open "GCC 13 or later" verification on Linux. CUDA/HIP targets come with their backend
  cycles (resolved 2026-10-11).
- **Q:** How is this cycle verified? — **A:** For real: the cycle's pushed head is submitted on all
  three targets and each must report success, plus one negative control with a hash that is not in
  the repository, which must fail with the "not a commit" message and a failed `result.json`. The
  factory pushes the cycle branch for this; that push is authorized. The factory submits through the
  cluster's MCP server, not ad-hoc shells, so the job must be submittable that way (inline content
  plus arguments); the MCP tooling itself is not part of the repo (resolved 2026-10-11).
- **Q:** What is out of scope? — **A:** Hosted CI; GPU targets; burn-in runs; performance numbers;
  any change to product source (`src/`) or to `make check`; scheduling policy beyond the target file
  (resolved 2026-10-11).
- **Q:** What may the public repo carry? — **A:** No internal account names, user names, or paths
  beyond generic placeholders in anything committed (resolved 2026-10-11).
- **Q:** Inline content or a script file? — **A:** A script file. The cluster MCP's inline
  submission writes into the home directory and can't pass arguments (research). The factory copies
  the job script from the tested commit into scratch and submits it by path with its arguments.
  Amended by the foreman (2026-10-11).

## Related materials

- Seed: [`issues/cluster-test-jobs.md`](../../issues/cluster-test-jobs.md); `ROADMAP.md`, "Test any
  commit on a cluster, built fresh from GitHub"
- Cycle 1's open GCC verification: `spec/build-and-test-foundation/REVIEW.md` (R1 row),
  `spec/build-and-test-foundation/PLAN.md` ("GCC versions")
- `Makefile` (`CXX` defaults to `clang++`; `check` runs `bin/check` and `tests/cli.sh`)
- `AGENTS.md` § Factory (Profile), § Invariants (5, dependency-free core), § Conventions
- Later GPU targets: `issues/cuda-backend.md`, `issues/hip-backend.md`
