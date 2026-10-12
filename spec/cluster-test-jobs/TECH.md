---
slug: cluster-test-jobs
title: Test any commit on a cluster, built fresh from GitHub
kind: feature
appetite: small
status: done
branch: feature/cluster-test-jobs
base: main
current_phase: done
last_updated: '2026-10-12'
phases:
- id: P1
  name: The job, happy path end to end, with its header and the AGENTS.md rows
  status: done
  satisfies:
  - R1
  - R2
  - R5
  - R6
  - R7
  - R10
  - R12
  depends_on: []
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/cluster-test-jobs/verify/p1.sh
- id: P2
  name: Every failure path red, recorded and contained
  status: done
  satisfies:
  - R3
  - R4
  - R5
  - R6
  - R7
  depends_on:
  - P1
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/cluster-test-jobs/verify/p2.sh && sh spec/cluster-test-jobs/remediation/f2.sh
- id: P3
  name: The three GCC targets, the target grammar, nothing site-private
  status: done
  satisfies:
  - R8
  - R9
  - R11
  depends_on:
  - P2
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/cluster-test-jobs/verify/p3.sh
- id: P4
  name: Real runs on the three clusters; negative control; red build; deferral ledger
  status: done
  satisfies:
  - R1
  - R3
  - R7
  - R11
  depends_on:
  - P3
  parallel: false
  hammerable: false
  hill: downhill
  verify: sh spec/cluster-test-jobs/verify/p4.sh
review:
  last_reviewed_commit: 19d0a24276854c91cd298d6eb8bcc97e60eddf4e
  verdict: approved
  blocked_reason: ''
  cycle: 2
---
# TECH.md — Test any commit on a cluster, built fresh from GitHub

The **context engine and finite-state machine** for building this feature. The YAML
frontmatter above is the resume ground-truth (read it from the cycle worktree with
`uv run --with pyyaml python "$KB/bin/factory/next_phase.py" spec/cluster-test-jobs/TECH.md`); the
per-phase checklists below are the work. `code-build` executes the next actionable phase, runs its
`verify:` command, updates state via
`uv run --with pyyaml python "$KB/bin/factory/set_phase.py" …`, and makes one atomic code+state commit.

- **Vision / requirements (locked):** [`GOAL.md`](GOAL.md) — R-IDs are the contract (R1 as amended
  by the foreman on 2026-10-11).
- **Authoritative design:** [`PLAN.md`](PLAN.md).
- **Backing research:** [`research/00-digest.md`](research/00-digest.md) + briefs.

## Frontmatter field reference

- `status` (top): `planned | in_progress | blocked | in_review | done`.
- `appetite`: `small` — caps phase count and build-iteration budget (circuit breaker).
- `branch`, `base`: the cycle's branch and the profile's `base`.
- phase `status`: `pending | in_progress | done | blocked`
- `satisfies`: GOAL R-IDs this phase delivers (traceability anchor for `code-review`).
- `depends_on`: phase ids that must be `done` first.
- `parallel`: every phase edits `ci/test-commit.sh` or depends on it, so none is parallel.
- `hammerable`: `false` throughout: each phase is a correctness gate for a gate.
- `hill`: `uphill` → `crest` → `downhill`.
- `attempts`: durable failed-verify counter, bumped by `set_phase.py --phase P<n> --record-attempt`.
- `verify`: the exact command that proves the phase.
- `review.cycle`: completed review passes.

## Conventions (apply to every phase)

- `AGENTS.md` is the constitution; walk the § Invariants sections PLAN § 3 lists.
- One phase per `code-build` invocation; one atomic commit with the code and the `TECH.md` state,
  subject `[feature] Build cluster-test-jobs P<n>: …`, no trailers.
- Same-commit rules: the Profile's `cluster test` row and the Map entry land in the commit that adds
  the job (P1); a change to `result.json`'s shape bumps its `format` in the same commit.
- **The gates are fixed.** `spec/cluster-test-jobs/verify/` is reviewed as of the plan commit; review
  checks it is unchanged (PLAN § 6). If a gate is wrong, stop and escalate rather than edit it.
- The gates run on macOS and need `dash`, `clang++`, GNU make 3.81 at `/usr/bin/make`, `git`,
  `rsync`, `pgrep` and `python3` (the gates' JSON checker and scan only; the job itself must not use
  Python). Each prints `GATE FAIL: <reason> (R-ID)` and exits 1 on the first failure, or ends with a
  `Pn gate: green (…)` line.
- **Site-private scan.** P3 and P4 read literal values from
  `${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}`, outside the repository, and fail without it.
  Run P3's scan (or P3 itself) before every push. Nothing committed names an account, QoS, user,
  cluster or site path, in files or in commit messages: write `<scratch>`, `<account>`, `<qos>`,
  `<cluster>`; name clusters by the node type they provide; the public repository URL is the one
  allowed appearance of its owner's name.
- In the job, write any outside value with `printf '%s'`, never `echo` (dash interprets backslashes).

---

## Phase P1 — The job, happy path end to end
**Satisfies:** R1, R2, R5, R6, R7, R10, R12 · **Depends on:** —
**Goal:** `ci/test-commit.sh HASH TARGET` runs every step on a good commit and writes a correct
`result.json`, from a copy outside any repository, under dash and sh, loading the login profile when
`module` is missing, without writing outside its run directory; its header and `AGENTS.md` document
it.

- [x] Write `ci/test-commit.sh` as PLAN § 2 "The job, in order" describes: `#!/bin/sh`, the SPDX block,
      `LC_ALL=C`, `set -u`, `set -f`, fd 3 for messages; step 0's `unset` of the inherited variables
      (as one `unset` command, continuation lines allowed), repeated after the profile; argument
      checks (exit 2); `/etc/profile` loaded only when `module` or a scratch variable is missing,
      with `HOME` at a non-existent path; scratch from `MPS_SCRATCH`, else `SCRATCH` (exit 3); the run
      directory made with a plain `mkdir` (exit 3 if it exists); `HOME`, `TMPDIR` inside it;
      `mps-test: script <git hash-object "$0">`; `run_step` for clone, checkout, toolchain, build,
      check, announcing `mps-test: NAME` on stdout and, on failure, `mps-test: NAME failed` plus the
      log's last 20 lines on stderr; the target-file line parser (no `.`, no `eval`); the partition
      check; `module purge` then each module; the compiler asked of `make`; `result.json` via
      `result.json.tmp` and `mv`, with the shape in research/03; the `TERM`/`INT`/`HUP` trap; exit 0
      or 1.
- [x] The header is the manual (R12): usage and both arguments, every environment variable
      (`MPS_REPO_URL` with the public URL as default, `MPS_SCRATCH`, `SCRATCH`, the `SLURM_*` ones it
      reads, and `MPS_SLURM_ACCOUNT`/`MPS_SLURM_QOS` for the submitter), when it loads `/etc/profile`,
      the run-directory layout `<scratch>/mpsearch/runs/<hash>/<target>/<jobid>/` with `src/`,
      `home/`, `tmp/`, each step's `.log` and `result.json`, the exit statuses, the target-file keys
      and their `sbatch` options (`--partition`, `--gpus`, `--cpus-per-task` with one task, `--mem`,
      `--time`, and always `--output` in scratch), the MCP submission recipe in generic terms (copy
      the script from the tested commit into scratch, then `sbatch` it by path with the hash and
      target as arguments; never inline content), and that `result.json` carries `format` and a
      change to it bumps the number.
- [x] No R-IDs, phase ids, target names or partition names in the job; tabs for indentation.
- [x] `AGENTS.md`: a Map bullet for `ci/test-commit.sh` and `ci/targets/`, and a Profile row
      `| cluster test | … ci/test-commit.sh <hash> <target> … |` that points at the header rather than
      restating it.
- **Verify:** `sh spec/cluster-test-jobs/verify/p1.sh` — static checks (shebang, SPDX, `dash -n`,
  `sh -n`, no interpreter name even after `/` or `-`, no bashism, no `/tmp`, no spec ids, default URL,
  every inherited variable unset); the header and `AGENTS.md` greps; then four real runs on a
  `file://` fixture whose `HEAD` is newer than the tested commit: (1) dash, full hash, no `module` and
  no scratch variable, so the fake profile must load with `HOME` absent and its `module load` must put
  `gate-c++` on `PATH`; (2) sh, a 12-digit prefix, `module` already a command (no profile), the first
  run `chmod 000` meanwhile, a full rebuild, the first run unchanged after; (3) a polluted
  environment; (4) Homebrew coreutils first on `PATH`. Each is checked field by field (the wrapper's
  compiler line, times inside the gate's run), with step order, purge-then-load, `gate-c++ … -O1` and
  no `-O3` on every compile line of build and check, green summaries, the printed script hash, no
  alternates, containment and no interpreter run.
- **Touches:** `ci/test-commit.sh`, `AGENTS.md`.

## Phase P2 — Every failure path red, recorded and contained
**Satisfies:** R3, R4, R5, R6, R7 · **Depends on:** P1
**Goal:** each way a run can go wrong produces the exit status, message and `result.json` the GOAL
names, and nothing else.

- [x] Walk research/04's failure table against the job and fix what P1 left: argument counts and
      malformed hashes (including an embedded newline) and target names exit 2 and create nothing;
      no scratch even after the profile, a relative or a missing one exits 3 and creates nothing;
      `SCRATCH` alone is used and `MPS_SCRATCH` wins; an existing run directory exits 3 untouched; an
      absent hash (full or short) and a hex-named tag fail checkout with "HASH is not a commit in URL"
      on stderr and in `checkout.log`; a failed clone; a missing target file named in the message; a
      module that does not load; a partition that differs from the target's, naming it, with a
      cluster name holding `"`, `\` and a tab still giving valid JSON; a failed build, clean and under
      a polluted environment; a failed check; `TERM` mid-build still writes `result.json` (build
      failed after ≥ 2 s) and prints `interrupted`.
- Amendment (review cycle 1, F2 remediation, 2026-10-12): P2 reopened. The class is *a HASH that
  does not name exactly one commit gets a message that misstates why*. Evaluated per case: absent
  (R3's message, true); malformed or too short (exit 2, "not a commit hash", true); a prefix two
  or more commits share (was "not a commit": false); a prefix one commit shares with a blob or tree
  (was checked out: right); a prefix one commit shares with an annotated tag object (was "not a
  commit": false, since git's commit-ish lookup counts the tag); a blob's or tag's own hash (not a
  commit: true); a ref named like hex (never read as a commit: closed). PLAN § 2's checkout
  (`rev-parse --verify HASH^{commit}` plus the prefix check) is superseded by the item below; the
  record in PLAN is left as written.
- [x] The checkout step lists every object whose name begins with HASH
      (`git rev-parse --disambiguate`) and asks each its type (`git cat-file -t`), so only object
      names count and no git message is parsed. No commit: "HASH is not a commit in URL", with the
      other objects' types in parentheses when some exist. One commit: check it out. More than one:
      "HASH is ambiguous in URL: N commits begin with it" and each full hash, on stderr and in
      `checkout.log`; checkout fails, later steps `not_run`, exit 1.
- [x] Sweep the live sites that describe HASH: the job's header (the HASH argument and the checkout
      messages). The `AGENTS.md` `cluster test` row and the usage line only name `<hash>`/`HASH`
      and point at the header, so they stay.
- **Verify:** `sh spec/cluster-test-jobs/verify/p2.sh && sh spec/cluster-test-jobs/remediation/f2.sh`.
  `p2.sh` (frozen) — 16 argument cases, 6 scratch and run-directory cases, then 11 failing runs,
  each with the exact exit status, the failed step and `not_run` after it in a `result.json` free of
  duplicate keys and non-JSON numbers, times inside the gate's run, logs only for the steps that
  ran, and the sandbox containment check. `remediation/f2.sh` (added for F2) — on the same fixture
  plus precomputed colliding objects, each prefix first checked to name exactly the objects
  assumed: a prefix two commits share is ambiguous, naming both commits and never "not a commit",
  under dash, sh and with a branch named like the prefix; an eight-digit prefix, a commit beside a
  blob and a commit beside a tag check out that commit; a blob's and a tag's full hash, an absent
  short hash and two malformed hashes say "not a commit".
- **Touches:** `ci/test-commit.sh`, `spec/cluster-test-jobs/remediation/f2.sh`.

## Phase P3 — The three GCC targets, the grammar, nothing site-private
**Satisfies:** R8, R9, R11 (the three targets) · **Depends on:** P2
**Goal:** the three target files exist, follow the grammar, read the same sourced or parsed, are
accepted by the job's own parser, and nothing this cycle adds or says is site-private.

- [x] `ci/targets/x86-64-avx512`, `ci/targets/x86-64-avx2`, `ci/targets/aarch64-gb10` as in PLAN § 2
      and research/02: SPDX block; `description` naming the hardware (x86-64 AVX-512, x86-64 AVX2,
      aarch64 GB10) and no site; `partition` (`cpu`, `cpu`, `spark-batch`); `gpus` (empty, empty,
      `1`: the GB10 partition refuses CPU-only jobs); `cpus=4`; `mem=4G` (the AVX-512 cluster caps
      memory per CPU); `time=00:30:00`; `modules` `gcc/14.1.0`, `gcc/14.4.0`, `gcc/14.2.0`;
      `make_vars='CXX=g++'`. No `cluster`, `account` or `qos` key.
- [x] The job refuses a malformed line by number before anything could execute it, and a missing
      required key by name.
- **Verify:** `sh spec/cluster-test-jobs/verify/p3.sh` — `ci/` holds exactly the job and three
  targets; the job names none of them; each target matches the grammar, sources identically under sh
  and dash and through a line parser, has GCC ≥ 13 and `CXX=g++`, the expected partition, GPUs and
  description, and passes the job's parser up to the (fake-refused) GCC module load; nine malformed
  lines are refused by line number and none executes; a missing key is named; the site-private scan
  over every file this cycle touches and over `origin/main...HEAD`'s messages and patches is clean
  (it needs the site file).
- **Touches:** `ci/targets/*`, possibly `ci/test-commit.sh`.

## Phase P4 — Real runs on the clusters
**Satisfies:** R1, R11 (and R3, R7 on a cluster) · **Depends on:** P3
**Goal:** the cycle's pushed head passes on all three targets through the MCP server, a hash that is
not in the repository fails as R3 says, and an earlier commit of this branch that GCC cannot compile
comes back red, all recorded as redacted evidence.

**Graded by the orchestrator.** The evidence lives in `spec/cluster-test-jobs/evidence/`, which the
blind reviewer cannot see; the orchestrator grades R11 from it, from the job ids, `sacct` and the
session's MCP transcript.

**Limits for every job:** ≤ 30 min, ≤ 8 cores, ≤ 16 GB, at most 2 queued per cluster (the GB10
cluster: one at a time), output only under `<scratch>/mpsearch/`, account and QoS from
`MPS_SLURM_ACCOUNT` and `MPS_SLURM_QOS` (approved per cluster by the foreman), nothing written in
the home directory. Which cluster serves which target is in the site file, not the repository.

- [x] Push the branch at P3's head (authorized by the GOAL), then submit that head on `x86-64-avx2`
      through the MCP recipe (PLAN § 2: `write_file` the job from that commit to
      `<scratch>/mpsearch/jobs/`, then `sbatch` with `script_path="<file> <hash> x86-64-avx2"` and
      `output` in that directory). Without D1's fix it must come back `build: failed` with a GCC 13+
      compiler: the real red build.
- [x] D1: merge `origin/main` (which carries `97962ce`, the `<cstdint>` fix) into this branch, run
      `make check`, the P1–P3 gates and the scan locally, push.
- [x] Submit the pushed head on `x86-64-avx512`, `x86-64-avx2` and `aarch64-gb10`, each with its
      target file's settings. Each must report overall success and a GCC 13+ compiler.
- [x] Negative control: submit a well-formed 40-digit hash that is not in the repository on
      `x86-64-avx512`, with the job script taken from the pushed head.
- [x] If a run fails for a reason that is not the expected one (preemption and requeue, a node
      fault), resubmit; record only runs whose outcome is the one under test.
- [x] Evidence, each piece through `sh spec/cluster-test-jobs/verify/redact.sh IN OUT` (it redacts
      cluster, host, site names and paths, then scans): `evidence/<target>.json` and
      `evidence/<target>.out` for the three green runs, `evidence/not-a-commit.json` and `.out`,
      `evidence/red-build.json` and `.out`. Each `.out` is the run's scheduler output, which holds the
      `mps-test: script …` line, the step lines and any failure tail.
- [x] `evidence/jobs.tsv`: a header line, then one tab-separated line per job, `label`, `job_id`,
      `sacct` `State`, `sacct` `ExitCode`, with labels `x86-64-avx512`, `x86-64-avx2`,
      `aarch64-gb10`, `not-a-commit`, `red-build`. Record submission times and options in the phase
      commit's body, with no account, QoS or cluster names.
- [x] Deferral ledger: walk P1–P4 of this file for "do not fix here", "known limitation", "left as a
      follow-up" and any deferral the build introduced; each needs an `issues/<slug>.md` from the
      ISSUE template and a `ROADMAP.md` entry. None is planned. Filed one, found at P4: nothing in
      `make check` exercises the job or the target files, so once these gates are records a
      regression would go unnoticed; this cycle's GOAL excluded changes to `make check`
      (`issues/ci-job-standing-checks.md`, with its `ROADMAP.md` entry after this cycle's).
- Amendment (P4 build): the MCP server's `write_file` appends a newline to the content it is
  given, which changes the script's blob hash. The submitter sends the script without its final
  newline, then checks `git hash-object` of the written file against the tested commit's
  `ci/test-commit.sh` before `sbatch`. The job's header is unchanged (its recipe is generic, and
  a change to the job after the tested commit would void the runs).
- Amendment (review cycle 1, F2 remediation, 2026-10-12): P4 reopened as `pending`. P2's fix
  changes `ci/test-commit.sh`, so the runs above no longer cover `HEAD` (`p4.sh`: "code changed
  after the tested commit"). A separate run redoes them on the new pushed head: push, the three
  green runs and the negative control, with fresh evidence and `jobs.tsv`. The red build of
  `c785e51` stays valid, since it ran that commit's own script.
- Runs, history (P4 build, 2026-10-12): tested head `66d70db`; `16963706` (x86-64-avx512),
  `44160374` (x86-64-avx2), `474548` (aarch64-gb10) `COMPLETED 0:0`; negative control `16963707`
  `FAILED 1:0`; red build of `c785e51`, `44160320`, `FAILED 1:0`. All ran script blob `62723964…`.
  Superseded by the reruns below, except the red build.
- Reruns (F2 remediation, 2026-10-12): pushed head `ee2c115`, script blob `8fb69ffc…`, each copy's
  `git hash-object` checked on the cluster before `sbatch`. `16964306` (x86-64-avx512, GCC 14.1.0),
  `44161743` (x86-64-avx2, GCC 14.4.0), `474553` (aarch64-gb10, GCC 14.2.0, one GPU)
  `COMPLETED 0:0`, every step passed; negative control `16964312` on x86-64-avx512 with
  `04e6a97d…`, a fresh hash that is no object in the repository, `FAILED 1:0`, steps `PFNNN`,
  "not a commit". No resubmissions. Evidence and `jobs.tsv` replaced for these four; the red
  build's record kept.
- **Verify:** `sh spec/cluster-test-jobs/verify/p4.sh` — the three green results (all steps passed,
  GCC 13+, the target's architecture, `sacct` `COMPLETED 0:0`, one commit that contains
  `origin/main`, is on the pushed branch, and whose tree outside `spec/`, `issues/` and `ROADMAP.md`
  equals `HEAD`'s, and each printed script hash equal to that commit's `ci/test-commit.sh`); the
  negative control (`PFNNN`, `commit` null, a well-formed hash absent from the repository, the message
  in its output, `FAILED 1:0`); the red build (`PPPFN`, an earlier commit of this branch with `ci/`, a
  compiler error in its output, `FAILED 1:0`); on all five, a target among the three, `<cluster>` and
  `<host>`, and distinct job ids matching `jobs.tsv`; the site-private scan over files and history;
  the deferral ledger.
- **Touches:** `spec/cluster-test-jobs/evidence/*`; the merge of `origin/main`.

---

## How `code-build` drives this

1. `next_phase.py` prints the next actionable phase.
2. Pre-flight: the cycle worktree is clean, on `branch`, and `base` is reachable.
3. Execute every `[ ]` in the phase (consult `PLAN.md` / `research/` for detail).
4. Run the phase's `verify:` command — never advance on a checkbox alone.
5. Amend this file freely if reality diverges (regenerate frontmatter with `set_phase.py`; note the
   amendment in the commit body). STOP and escalate only on a **`GOAL.md` contradiction**.
6. Mark the phase `done`, advance `current_phase`, `--touch`; one `[feature]` commit; stop and report.
