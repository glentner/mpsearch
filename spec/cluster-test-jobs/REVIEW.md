# REVIEW — Test any commit on a cluster, built fresh from GitHub

> Adversarial QA by `code-review`, run in an isolated/clean context (a subagent; where the runtime
> has none, `code-review`'s sequential fallback). The correctness pass grades the branch diff against
> [`GOAL.md`](GOAL.md) + the repo's `AGENTS.md` § Invariants **only** — it does not see
> `PLAN.md`/`TECH.md` (avoids grading-its-own-homework / plan-sycophancy). Every finding cites an
> **executed** command, not an assertion.

## Review cycle 1 — changes-requested (2026-10-12)

- **Reviewed commit:** ed7c79060819fa1c504a72ad026d465f3d8c97be, re-pinned to
  284cb153c1dfedf0c773e7eea105e00840865a70 after the F3 deferral commit · **Base:** main ·
  **Date:** 2026-10-12
- **Verdict:** changes-requested
- **Cycle:** 1 of ≤3 (mirrors `review.cycle` in `TECH.md`)
- **Mode:** a fresh blind pass over the full spec-excluded diff, by one subagent at extra-high
  reasoning effort. No debate, because the diff doesn't touch the profile's `coupled core`. No sequential
  fallback.

### Pre-flight

- The worktree was clean on `feature/cluster-test-jobs`, with head `ed7c790` equal to
  `origin/feature/cluster-test-jobs`. `TECH.md` showed `in_review` and 4 of 4 phases done.
- **Contract drift.** `GOAL.md` changed once after the shape commit, in the plan commit `dcda63c`,
  before any build. That change added the "Inline content or a script file?" clarification and the
  matching R1 wording: the job is copied from the tested commit and submitted by path with its two
  arguments. The foreman made it under the author's delegation of every gate in this effort
  (2026-10-11) and confirms it here. The review grades the amended contract.
- `verify/` has not changed since the plan commit: `git diff --stat dcda63c HEAD --
  spec/cluster-test-jobs/verify/` is empty.

### Verification run

**The reviewer** saw only `GOAL.md`, the spec-excluded diff, the rubric and `AGENTS.md`:

- `make check` passed: check 6/0, cli 14/0.
- **The job as committed:** `git show ed7c790:ci/test-commit.sh` was copied into a scratch directory
  with no repository and run on macOS under dash and `/bin/sh`, with the scratch and URL overrides, a
  stub `module` and fake job ids.
  - **Happy path:** it cloned the public GitHub URL, and `git rev-parse HEAD` in `src/` equals
    `result.json`'s `commit`, including when given the short hash.
  - **Bad hashes:** an absent hash, a hex-named tag and a tag shadowing a short hash were all refused
    as "not a commit".
  - **Bad arguments:** wrong argument counts and the names `../x`, `a/b`, `..`, `.hidden`, `-rf` and
    `a b` exit 2 and create nothing.
  - **Scratch and run directory:** scratch unset, relative or missing, or an existing run directory,
    exits 3. Of two concurrent runs with one job id, one is refused.
  - **Containment:** with spaces in every path and decoy HOME and TMPDIR, a `find` diff shows nothing
    written outside the run directory.
  - **Failing steps:** a failing build, a failing check, a missing `module`, a failing `module load`
    and a partition mismatch each fail their step, and the later steps are `not_run`.
  - **Interruption:** SIGTERM during the build gives `failed` and valid JSON.
  - **Hostile input:** with a hostile cluster name and hostile compiler output, `result.json` still
    parses.
  - **Target grammar:** 21 malformed or hostile target fixtures were refused by line, or read
    literally. No canary file appeared.
  - **Dependencies:** a grep found no python, jq, perl or awk in the job.
- The tree was clean on hand-back, and the build state untouched: `make -q bin/mps bin/check` is 0.

**The orchestrator** graded the halves the blind reviewer cannot reach:

- `sh spec/cluster-test-jobs/verify/p4.sh` passed, on `ed7c790` and again on `284cb15`.
- **The runs, read through the cluster MCP server** (`sacct`, plus each run directory's
  `result.json`):
  - `16963706` (x86-64-avx512), `44160374` (x86-64-avx2) and `474548` (aarch64-gb10) on `66d70db`:
    COMPLETED 0:0, every step passed. The compilers were GCC 14.1.0, 14.4.0 and 14.2.0.
  - `16963707`, the negative control: FAILED 1:0, steps PFNNN, "… is not a commit in
    https://github.com/glentner/mpsearch.git".
  - `44160320`, the red build of `c785e51`: FAILED 1:0, steps PPPFN, `uint16_t` undeclared.
  - The committed evidence matches these records, redacted.
- **The nodes, from `scontrol show node`:**
  - The AVX-512 target ran on x86_64 with 96 cores per socket (EPYC 9654, Zen 4, which has
    AVX-512).
  - The AVX2 target ran on x86_64 with 64 cores per socket (EPYC Milan, AVX2 only).
  - The GB10 target ran on aarch64 with GRES `gpu:nvidia_gb10`.
- **Script provenance:** `git rev-parse <c>:ci/test-commit.sh` is `62723964…` at `c785e51`,
  `66d70db` and `ed7c790`. Every run's output prints `mps-test: script 62723964…`.
- **Site-private scan:** `git log -p origin/main..HEAD`, every patch and commit message, was grepped
  for account, QoS, user, cluster, site and path terms, with the public repository URL excepted. It
  found only generic placeholders and the scan's own descriptions.

### Requirement → evidence matrix

| R-ID | Implemented by | Verified how (by whom) | Status |
|------|----------------|------------------------|--------|
| R1 | `ci/test-commit.sh` | Local copy from the commit, run with no repository (reviewer). Real Slurm submission by path, with the printed script blob matched to the commit (orchestrator). | ✅ |
| R2 | `ci/test-commit.sh` | Public clone, `rev-parse HEAD` = `result.json` `commit`; the URL override (reviewer). | ✅ |
| R3 | `ci/test-commit.sh` | Absent hash and hex-named tags (reviewer). Cluster negative control `16963707` (orchestrator). | ⚠️ F2 |
| R4 | `ci/test-commit.sh` | Argument counts, a missing target file, hostile names (reviewer). | ✅ |
| R5 | `ci/test-commit.sh` | Scratch cases, an existing run directory, concurrency, the containment `find` diff (reviewer). | ✅ |
| R6 | `ci/test-commit.sh` | Each step failing in turn, with later steps `not_run` (reviewer). | ✅ |
| R7 | `ci/test-commit.sh` | Fields and JSON validity on every path, exit statuses (reviewer). Real records (orchestrator). | ✅ |
| R8 | `ci/targets/*`, the job's parser | 21 fixtures, value equivalence across shells (reviewer). | ✅ |
| R9 | the whole cycle | `+` lines of the spec-excluded diff (reviewer). `spec/`, history and commit messages (orchestrator). | ✅ |
| R10 | `ci/test-commit.sh` | grep for interpreters, a full run under dash (reviewer). | ✅ |
| R11 | `ci/targets/*`, the runs | Three definitions with GCC 13 or newer modules (reviewer). Three passing runs on one pushed head, the negative control, and the node classes (orchestrator). | ✅ |
| R12 | `AGENTS.md` Map and the `cluster test` row; the job's header | Read against the behaviour (reviewer). | ✅ |

Unmapped changes:

- The `ROADMAP.md` entries and `issues/` seeds, which are deferral bookkeeping (`AGENTS.md` § Factory).
- The cluster-test-jobs seed's status change.
- Supporting detail within R5 and R7: the partition guard, the script-hash line, the SIGTERM record,
  and the `requested`, `arch`, `job_id` and `format` fields.

None is scope creep.

### Findings

#### [LOW/CONFIRMED] F2 — An ambiguous short hash is reported as "not a commit"

- **Where:** `ci/test-commit.sh:269-274` (`do_checkout`)
- **Failure scenario:** given a short hash that two commits share, the job prints
  "mps-test: HASH is not a commit in URL" and exits 1. It fails closed, so nothing wrong is checked
  out, but the diagnostic is false: the hash names two commits.
- **Evidence:** the reviewer made two fixture commits sharing the prefix `0c01901`, by birthday
  search. `0c01901` gave "0c01901 is not a commit in file://…/fx" and EXIT=1. `0c01901b` passed with
  commit `0c01901bd473…`.
- **Touches:** R3 (what "not a commit" means) and R2.

#### [PLAUSIBLE → dropped] F1 — Is `result.json` a "result format" under § Invariants?

- **Where:** `AGENTS.md` § Invariants, the same-commit sentence under the high-blast-radius list,
  against `ci/test-commit.sh:53-58`.
- **Question:** the header versions `result.json` ("Any change to its shape bumps format"), as
  § Conventions asks of a result format. Should the job's path therefore join the high-blast-radius
  list?
- **Triage:** dropped by the foreman, under the author's delegation of every gate in this effort
  (2026-10-11).
  - The sentence's company scopes it to the formats that carry `mps`'s own verdicts and checkpoints.
    That company is an arithmetic engine, a test driver that prints a verdict and a table of known
    answers, and the list itself is `src/math.*`, `src/mp.*` and `src/io.*`.
  - `result.json` is the factory's record of a build-and-check run. The factory reads it; `mps` never
    does. Versioning it is ordinary hygiene.
  - The cycle that adds `mps`'s own result format makes the wording explicit.

#### [LOW/CONFIRMED → out of scope, deferred] F3 — Nothing ties a target to its CPU tier

- **Where:** `ci/targets/x86-64-avx2` and `ci/targets/x86-64-avx512`. The only node check is the
  partition guard at `ci/test-commit.sh:373`.
- **Failure scenario:** the AVX-512 target, submitted where partition `cpu` holds AVX2-only nodes,
  passes, and the record's target name claims a tier the node lacks.
- **Evidence:** the reviewer ran `x86-64-avx512` on an arm64 Mac. It passed, and `result.json` says
  `"arch": "arm64"`.
- **Orchestrator:** this behaviour is outside the rubric's Scope.
  - No R-ID asks the job to check the node's CPU. R8 lists the target's settings, and none is an ISA.
  - Nothing builds for a tier yet.
  - The record names the host and machine truthfully, and the cycle's runs landed on the classes
    their targets name (see Verification run).

  So this is hardening for the tier-specific builds to come, not a defect against the contract, and
  it does not block. It is deferred to `issues/target-node-check.md` and its `ROADMAP.md` entry
  (Part V, before the SIMD kernels), in commit `284cb15`.

### Human-gate triggers

- None. No CONFIRMED finding touches a path on § Invariants' high-blast-radius list or violates a
  section marked *[human gate]*.

### Route

F2 blocks, so the cycle returns to `code mpsearch build` (remediation). Fixing it changes the job
script, so the real runs must be earned again on the new pushed head: three green runs and the
negative control. The red build of `c785e51` remains valid evidence, since it ran that commit's own
script.

## Review cycle 2 — approved (2026-10-12)

- **Reviewed commit:** 19d0a24276854c91cd298d6eb8bcc97e60eddf4e · **Base:** main · **Date:** 2026-10-12
- **Verdict:** approved
- **Cycle:** 2 of ≤3 (mirrors `review.cycle` in `TECH.md`)
- **Mode:** scoped to the remediation of F2, plus a regression check of the changed file. The
  foreman chose the scope under the author's delegation of every gate in this effort (2026-10-11).
  One fresh blind subagent ran at extra-high reasoning effort. The diff since cycle 1
  (`git diff 284cb15 HEAD -- . ':(exclude)spec/'`) touches only `ci/test-commit.sh`. No debate and no
  sequential fallback.

### Pre-flight

- The worktree was clean on `feature/cluster-test-jobs`, and head `19d0a24` equals
  `origin/feature/cluster-test-jobs`. `TECH.md` shows `in_review` with every phase done.
- No contract drift since cycle 1. `GOAL.md`'s commits are still the shape commit and the plan
  commit `dcda63c`. `verify/` is unchanged since `dcda63c`.

### Verification run

**The reviewer** saw only `GOAL.md`, the spec-excluded diffs, the rubric and `AGENTS.md`.

- **Fixture.** A bare clone of `19d0a24` was extended with colliding objects found by birthday
  search and written with `git hash-object -w`. Each object is reachable, so a `file://` clone
  carries it. The job was run byte-identical to HEAD (blob `8fb69ff`) under `/bin/dash` with
  `env -i`, a minimal PATH, a stub `module` and fake job ids.
- **Control.** The script from `284cb15`, run on the same fixture, still printed "… is not a commit"
  for the shared prefixes, so the fixture reproduces F2.
- **The class, case by case.**
  - Prefixes shared by two and by three commits: "HASH is ambiguous in URL: N commits begin with it",
    with every full hash listed.
  - A longer prefix that names one of those commits checks that commit out.
  - A commit sharing its prefix with a blob, a tree or an annotated tag: the commit is checked out.
  - The prefix or full hash of a blob, a tree or a tag: "is not a commit in URL (it names a blob)",
    or "a tree", or "a tag".
  - Absent full and short hashes: "is not a commit in URL".
  - Branches and tags named like hex, including one named after a real commit's prefix and one named
    after another commit's full hash: never read as commits.
  - Uppercase, malformed, too short, too long, empty and `HEAD~1`: exit 2, with "not a commit hash:
    want 7 to 40 lower-case hex digits" and no run directory.

  Every checkout failure exits 1, leaving a valid `result.json` with checkout `failed` and the later
  steps `not_run`. The same cases under `/bin/sh` match.
- **Regressions.**
  - The happy path with a full hash from public GitHub and with a short hash through the URL override:
    exit 0, all five steps passed, check 6/0, cli 14/0.
  - R2: `rev-parse HEAD` equals `result.json`'s `commit` in every run.
  - R3 and R4: messages and exit statuses as documented. R5: an existing run directory exits 3.
  - R10: the new code uses shell builtins, `git rev-parse --disambiguate` and `git cat-file -t`, and
    nothing else.
  - The header's argument and checkout-message text matches the behaviour, and no spec R#/P# ids
    appear in `ci/`.
- `make check` passed: check 6/0, cli 14/0. The tree was clean on hand-back, and the build state was
  untouched.

**The orchestrator** graded the real runs, re-earned on the remediated head `ee2c115`, whose script
blob is `8fb69ffc…`.

- `sh spec/cluster-test-jobs/verify/p4.sh` is green on `19d0a24`, and P1, P2 (with
  `remediation/f2.sh`) and P3 are green on `ee2c115`.
- **The runs,** read through the cluster MCP server (`sacct` and each run directory's
  `result.json`):
  - `16964306` (x86-64-avx512), `44161743` (x86-64-avx2) and `474553` (aarch64-gb10) on `ee2c115`:
    COMPLETED 0:0, every step passed, GCC 14.1.0, 14.4.0 and 14.2.0.
  - The negative control `16964312`: FAILED 1:0, steps PFNNN, "04e6a97… is not a commit in
    https://github.com/glentner/mpsearch.git".
  - The red build `44160320` of `c785e51` stands from cycle 1. It ran that commit's own script.
- **The nodes, from `scontrol show node`:**
  - AVX-512: x86_64, 96 cores per socket (EPYC 9654).
  - AVX2: x86_64, 64 cores per socket (EPYC Milan).
  - GB10: aarch64 with GRES `gpu:nvidia_gb10`.

### Requirement → evidence matrix

The cycle-1 matrix stands for every R-ID. Cycle 2 changes one row:

| R-ID | Implemented by | Verified how (by whom) | Status |
|------|----------------|------------------------|--------|
| R3 | `ci/test-commit.sh` (`do_checkout`) | The F2 class table above (reviewer). Cluster negative control `16964312` (orchestrator). | ✅ |

R1, R7 and R11's real runs were re-earned on `ee2c115` (orchestrator, above).

Unmapped changes: none new.

### Findings

- None. F2 is fixed as a class. F1 (dropped) and F3 (deferred to `issues/target-node-check.md`)
  stand as triaged in cycle 1.

### Human-gate triggers

- None.
