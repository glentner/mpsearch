# PLAN — Test any commit on a cluster, built fresh from GitHub

> **Status:** Draft for review · **Last updated:** 2026-10-12
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). Backing detail is in
> [`research/`](research/). Every design element traces to a GOAL R-ID.

## 1. Summary

One POSIX shell script, `ci/test-commit.sh HASH TARGET`, is the whole job: it validates its two
arguments, clears inherited variables that could redirect git or make, makes a fresh run directory
under scratch, clones the public repository, checks out exactly the hash, reads the target's
`key=value` file from that commit, loads its modules, and runs `make` and `make check`, timing and
logging each step and always leaving `result.json`. Three target files, named by architecture
(`x86-64-avx512`, `x86-64-avx2`, `aarch64-gb10`), carry only public settings; which site cluster serves
which target, and the account and QoS, stay outside the repository. Local gates prove every path on the
Mac against a fixture repository with a fake login profile; the last phase proves it on the clusters
through the MCP server. Research found that `main` did not compile with GCC on Linux; the foreman fixed
`main` directly (D1, § 5).

## 2. Design

### Files

| Path | What it is |
|---|---|
| `ci/test-commit.sh` | the batch job: `#!/bin/sh`, POSIX only, SPDX header, a header comment that is its manual (R12) |
| `ci/targets/x86-64-avx512` | GCC on x86-64 with AVX-512 (AMD EPYC 9654, Zen 4), partition `cpu`, `gcc/14.1.0` |
| `ci/targets/x86-64-avx2` | GCC on x86-64 with AVX2 (AMD EPYC 7763, Zen 3), partition `cpu`, `gcc/14.4.0` |
| `ci/targets/aarch64-gb10` | GCC on aarch64 NVIDIA GB10, partition `spark-batch`, one GPU requested, `gcc/14.2.0` |
| `AGENTS.md` | the Map gains `ci/`; the Profile gains a `cluster test` row pointing at the script's header |

No change to `Makefile`, `src/` or `tests/`. No submit wrapper (brief 01, "Not adopted").

### The job, in order

The script runs with `LC_ALL=C`, `set -u` and `set -f` (no globbing, so make variables split on spaces
only), and keeps the original stderr on fd 3 so a step can print to the scheduler output while its own
output goes to its log. Every message that carries an outside value is written with `printf '%s'`,
never `echo`, because dash's `echo` interprets backslashes (`\c` ends the output).

0. **Clean environment.** Before anything else, and again after the login profile, the job unsets
   the inherited variables that can subvert it: `CDPATH`, `ENV`, `BASH_ENV`; git's `GIT_DIR`,
   `GIT_WORK_TREE`, `GIT_INDEX_FILE`, `GIT_OBJECT_DIRECTORY`, `GIT_ALTERNATE_OBJECT_DIRECTORIES`,
   `GIT_NAMESPACE`, `GIT_CEILING_DIRECTORIES`, `GIT_CONFIG`, `GIT_CONFIG_PARAMETERS`,
   `GIT_CONFIG_COUNT`, `GIT_CONFIG_GLOBAL`, `GIT_CONFIG_SYSTEM`, `GIT_TEMPLATE_DIR`; make's
   `MAKEFLAGS`, `MAKELEVEL`, `MFLAGS`, `MAKEFILES`, `GNUMAKEFLAGS`, `MAKEOVERRIDES`; and the build's
   `CXX`, `CC`, `CXXFLAGS`, `CPPFLAGS`, `LDFLAGS`, `LDLIBS` (the target's make variables and modules
   set what the build uses).
1. **Arguments** (R4). Exactly two, or `usage: test-commit.sh HASH TARGET` and exit 2. `HASH` must be
   7–40 lower-case hex digits and `TARGET` must match `[A-Za-z0-9][A-Za-z0-9._-]*`, checked with
   `case` patterns and `${#}` (no `grep`, so an embedded newline cannot slip through); otherwise a
   message naming the value ("… is not a commit hash", "… is not a target name") and exit 2. Both
   values become path components, so this is also what keeps the run directory under scratch.
2. **Login environment.** When `module` is not a command or neither `MPS_SCRATCH` nor `SCRATCH` is
   set, the job sources `/etc/profile` (with `set +u` around it), with `HOME` pointed at a path that
   does not exist so the profile and Lmod cannot write to the real home (brief 02, findings 2–3).
3. **Scratch and the run directory** (R5). `scratch` is `MPS_SCRATCH`, else `SCRATCH`; it must be an
   absolute path to an existing directory, or a message and exit 3. The run directory is
   `$scratch/mpsearch/runs/$HASH/$TARGET/$jobid`, `jobid` being `SLURM_JOB_ID` (else `local-$$`).
   Its ancestors are created with `mkdir -p`, and the run directory itself with a plain `mkdir`, which
   fails if it exists: a message and exit 3, race-free and without touching it. A requeued job (same
   job id) therefore refuses to run rather than reuse anything.
4. **Containment** (R5). The job `cd`s into the run directory and points `HOME` and `TMPDIR` at
   `home/` and `tmp/` inside it, unsets the `XDG_*` base directories, and sets
   `GIT_TERMINAL_PROMPT=0`. Git, Lmod and the compiler then write only there. It prints
   `mps-test: script <git hash-object "$0">`, so a run is tied to the exact script it executed.
5. **Steps** (R6). `run_step NAME FUNCTION` runs each step in the current shell (so `module load`
   persists), with stdout and stderr in `NAME.log`, timed with `date +%s`, printing `mps-test: NAME`
   on stdout before it starts. When a step fails, the job prints `mps-test: NAME failed` and the last
   20 lines of its log to stderr, so the scheduler output alone explains the failure; the remaining
   steps are recorded `not_run` and get no log. Statuses and seconds live in fixed per-step
   variables set through a `case`, no `eval`.
   - **clone**: `git clone --quiet --no-checkout "$url" src`, `url` being `MPS_REPO_URL`, else
     `https://github.com/glentner/mpsearch.git` (R2).
   - **checkout**: `git -C src rev-parse --verify --quiet "$HASH^{commit}"` must succeed and the full
     hash must start with `HASH` (so a branch or tag named like hex cannot stand in for a commit);
     otherwise `mps-test: HASH is not a commit in URL` goes to the log and to fd 3 (R3). Then
     `git checkout --quiet --detach FULL`, and `git rev-parse HEAD` must equal `FULL` (R2).
   - **toolchain**: read `src/ci/targets/$TARGET` from the checked-out commit (foreman decision: the
     job is self-contained in the commit it tests). A missing file prints `mps-test: no target file
     ci/targets/TARGET in commit FULL` (R4). The file is parsed line by line, never sourced (below).
     When `SLURM_JOB_PARTITION` is set and differs from the target's `partition`, the step fails
     naming both, so a run can never be filed under the wrong target. Then `module purge` and
     `module load` of each module in order (any failure fails the step), then the compiler `make`
     will use, asked of `make` itself
     (`printf 'mps-print-cxx:\n\t@echo $(CXX)\n' | make -s -f Makefile -f - mps-print-cxx $make_vars`,
     which works in make 3.81 and 4.x), its first `--version` line, and `make --version`'s first line.
   - **build**: `make $make_vars` in `src/`.
   - **check**: `make $make_vars check` in `src/`.
6. **`result.json`** (R7), written to `result.json.tmp` and renamed, with the shape in
   [brief 03](research/03-posix-json-and-timing.md): `format` 1, `requested`, `commit` (full hash or
   `null`), `target`, `cluster` (`SLURM_CLUSTER_NAME` or `null`), `host` (`uname -n`), `arch`
   (`uname -m`), `job_id`, `start`, `end` (UTC ISO 8601), `compiler`, `make`, `steps` (an ordered
   array of `{name, status, seconds}`), `status`. No paths. A `TERM`, `INT` or `HUP` trap marks the
   running step failed, prints `mps-test: interrupted (SIG) during STEP` to fd 3, writes the file and
   exits non-zero, so a run killed at its time limit still leaves a result.
7. **Exit status** (R7): 0 when every step passed, 1 when a step failed (or the job was interrupted),
   2 for argument errors, 3 for scratch or run-directory errors. The last line names the run
   directory and the overall status.

The job uses only the shell's built-ins and `date`, `mkdir`, `uname`, `tr`, `sed`, `head`, `tail`,
`cat`, `mv`, plus `git`, `make`, `module` and the target's toolchain (R10).

### Target files (R8)

```sh
# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# GCC on x86-64 AVX-512 (AMD EPYC 9654, Zen 4)
#
description='GCC on x86-64 AVX-512 (AMD EPYC 9654, Zen 4)'
partition=cpu
gpus=
cpus=4
mem=4G
time=00:30:00
modules='gcc/14.1.0'
make_vars='CXX=g++'
```

- **Grammar.** Each line is empty, a comment starting with `#`, or `key=value` with a key from the
  fixed set `description partition gpus cpus mem time modules make_vars`, and a value that is either
  unquoted from `[A-Za-z0-9_./:,+=@%-]` or single-quoted with no `'` inside. Every such file is a
  valid sequence of POSIX shell assignments, and a line parser reads it with two parameter expansions
  and a quote strip. Duplicate keys, unknown keys (including `account`, `qos` and `cluster`) and any
  other line are errors naming the line number. Required: everything but `description` and `gpus`.
- **The job never executes a target file.** It reads it with `while IFS= read -r`, validates each
  line with `case` patterns, and assigns through a `case` on the key. A line holding `$( )`, a
  backquote, a double quote, `${…}` or a `;` is rejected before anything could run it (P3 proves it).
- **Submission mapping**, stated once in the job's header and nowhere else: `partition` →
  `--partition`; `gpus` → `--gpus` when non-empty; `cpus` → `--cpus-per-task` with `--ntasks=1`;
  `mem` → `--mem`; `time` → `--time`; account and QoS from `MPS_SLURM_ACCOUNT` and `MPS_SLURM_QOS`
  (R9); always `--output` to a file in scratch, since the MCP server submits from the home directory
  (brief 01). Which cluster to submit a target to is the submitter's site knowledge (its site file),
  not the repository's.
- **Adding a target** is a new file; the job names no target or partition (P3 checks).

### Submission through the MCP server (R1, R11)

The amended R1: the job script, taken unchanged from the tested commit, is submitted through Slurm
with the hash and target as its arguments, with no prior clone or build on the login node. The
factory's recipe, written up generically in the job's header (brief 01):

1. `write_file` the job, from the commit under test, to `<scratch>/mpsearch/jobs/<name>.sh`
   (`create_dirs`), which also creates the directory for the scheduler output.
2. `sbatch` with `script_path="<that file> <hash> <target>"`, the target's settings mapped as above,
   `account`/`qos` from the environment, `job_name="mps-<target>"` and
   `output="<scratch>/mpsearch/jobs/%j.out"`.

`script_content` is never used: it cannot pass arguments and it writes into the home directory.

### Site-private material (R9)

Nothing site-specific is committed: no account, QoS, user, cluster name or site path. The literal
values live in a site file outside the repository, `${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}`
(`MPS_SITE_ACCOUNTS`, `MPS_SITE_QOS`, `MPS_SITE_USER`, `MPS_SITE_CLUSTERS`,
`MPS_SITE_SCRATCH_PREFIX`, `MPS_SITE_PATH_PREFIXES`). The gates' scan reads it and fails when it is
missing or a value is empty; it matches words case-insensitively, QoS names only after `-q`, `--qos`
or `qos=`, and path prefixes as substrings, over every committed and untracked file the cycle touches
and over the messages and patches of `origin/main...HEAD`. The public repository URL is the one
allowed appearance of its owner's name. The builder runs the scan before every push. Cluster
evidence is redacted by `verify/redact.sh` (cluster and host become `<cluster>` and `<host>`, site
paths and names become placeholders) before it is committed.

### Requirement → design map

| R-ID | Design element(s) that satisfy it |
|------|-----------------------------------|
| R1   | One self-contained `ci/test-commit.sh` taking `HASH TARGET` positionally, independent of `$0` and the working directory, printing its own `git hash-object`; the MCP recipe (`write_file` the script from the tested commit, then `sbatch` with `script_path="<file> <hash> <target>"`). Local proof in P1 (a copy run from an unrelated directory); real proof in P4 (the printed script hash equals the tested commit's `ci/test-commit.sh`) |
| R2   | `MPS_REPO_URL` or the public URL; clone without checkout; `rev-parse --verify HASH^{commit}` plus the prefix check; detached checkout; `HEAD` = `commit` in `result.json`; inherited git variables cleared |
| R3   | The checkout step's "not a commit" message on fd 3 and in `checkout.log`; checkout `failed`, later steps `not_run`, `commit: null`, status `failed`, exit 1 |
| R4   | Two-argument check (exit 2); missing target file detected in the toolchain step from the tested commit, with a message naming it and no build; malformed hash or target name rejected up front (exit 2) |
| R5   | `MPS_SCRATCH` › `SCRATCH`; exit 3 when unset, relative or missing; plain `mkdir` of the run directory (exit 3 if it exists); `HOME`/`TMPDIR` inside it; the profile loaded with a non-existent `HOME`; a fresh clone every run |
| R6   | `run_step` in the order clone, checkout, toolchain, build, check; `NAME.log` per step that ran; `not_run` after a failure; `make $make_vars` and `make $make_vars check` with the loaded toolchain; inherited make variables cleared |
| R7   | `result.json` (brief 03), always written once the run directory exists, also from the signal trap; exit 0 only when every step passed |
| R8   | `ci/targets/<name>` grammar, line parser without `eval` or `.`, required keys, the three files; adding a target touches only its file |
| R9   | No `account`/`qos`/`cluster` keys; generic target names; the header's mapping reads `MPS_SLURM_ACCOUNT`/`MPS_SLURM_QOS`; no paths in `result.json`; evidence redacted; the literal-value site scan over files and history |
| R10  | The utility list above; checked statically, by shadow stubs, and by a run with GNU coreutils first on `PATH` |
| R11  | The three target files (GCC 14.1, 14.4 and 14.2 modules); P4's real runs and negative control, with redacted evidence and `sacct` states in `spec/cluster-test-jobs/evidence/` |
| R12  | The job's header (usage, arguments, environment, `/etc/profile`, run-directory layout, exit statuses, submission mapping, result format); `AGENTS.md` Map entry for `ci/` and the Profile's `cluster test` row |

## 3. Invariant gate (AGENTS.md constitution check)

Checked against `AGENTS.md` § Invariants before research and again after this design was drafted and
after the adversarial review was folded in.

- **High-blast-radius paths.** The design touches none of them (`src/math.*`, `src/mp.*`,
  `src/io.*`). The D1 fix to `src/mp.hpp` landed on `main` outside this cycle, by the foreman's
  decision. The list's widening rule names "a checkpoint or result format"; `result.json` reports
  build and test steps, not a verdict of `mps` (§ 1's "every result names the test, the
  exponent…"), so the job is not added to the list. Adding it would only widen future gates.
- **§ 1–3 (verdicts, arithmetic, known answers).** Untouched: no verdict, engine or expected value
  changes. The job runs `make check` as it stands.
- **§ 4 Clean room.** The job and gates are our own shell; nothing is taken from another program.
- **§ 5 Dependency-free core.** Untouched: the CPU build still needs only a C++20 compiler and GNU
  make, and nothing new links into `bin/mps`. The job's own needs (R10) are a POSIX shell, git, the
  module command and the toolchain.
- **§ Conventions.** The Profile row lands in the commit that adds the job (same-commit rule). No
  R-IDs or phase ids in `ci/` (P1 checks). The submission mapping lives once, in the header, and
  `AGENTS.md` points to it rather than restating it. `result.json` carries `format: 1`, and the header
  says a change to it bumps the number. The job and target files open with the SPDX block; the job is
  indented with tabs.
- **§ Factory, Profile.** `ci` stays `none` (no hosted CI). `verify` stays `make check`; the cluster
  job is a separate, explicit act.

### Deviation justifications

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| R4's unknown target is caught after clone and checkout, in the toolchain step, with a `result.json` | The target files live in the commit under test (foreman decision), so the job cannot know them earlier | Reading targets from the submitter's tree would make a run depend on files outside the tested commit |
| A malformed hash or target name exits 2 with no run directory and no `result.json` | Both become path components; a value like `../x` would escape scratch | Treating every bad hash under R3 would need a run directory named after an unsafe string |

The plan matches the amended R1 (a script file from the tested commit, submitted by path with
arguments), so R1 needs no row.

## 4. Rabbit holes (resolved)

- **How the MCP `sbatch` tool passes arguments** → it has no argument parameter; `script_path` is
  appended unquoted, so `"<file> <hash> <target>"` reaches the job as `$1 $2` (verified on all three
  clusters); `script_content` cannot carry arguments and writes into the home directory, so it is not
  used, and R1 was amended to match ([01](research/01-mcp-submission.md)).
- **Network from compute nodes** → GitHub is reachable over HTTPS from all three; clone takes about
  a second ([02](research/02-cluster-environments.md)).
- **Modules in a batch shell** → not defined in a job submitted through the MCP server; sourcing
  `/etc/profile` restores them under bash and dash; GCC modules are `gcc/14.1.0`, `gcc/14.4.0`,
  `gcc/14.2.0`; `module purge` is safe; a bad `module load` exits 1 ([02](research/02-cluster-environments.md)).
- **Scratch inside a job** → `SCRATCH` is set wherever a scratch variable is, and is missing on the
  GB10 cluster until the profile is loaded ([02](research/02-cluster-environments.md)).
- **Home-directory writes** → the profile writes into `HOME`; a non-existent `HOME` during the profile
  and a run-local `HOME` afterwards leave the real home untouched ([02](research/02-cluster-environments.md)).
- **JSON and timing without an interpreter** → `tr -cd '\040-\176'` then two `sed` escapes, atomic
  rename, `date +%s` ([03](research/03-posix-json-and-timing.md)).
- **Gate design** → a fixture repository over `file://` with a newer `HEAD`, a fake login profile
  whose `module` puts a wrapper compiler on `PATH`, shadowed interpreters, polluted and GNU-tools
  runs, a containment check, under dash and sh; real-cluster evidence checked in P4
  ([04](research/04-gate-design.md)).
- **Adversarial design review** (trigger 3): run by the foreman on the first draft. All twelve
  findings were folded in:
  1. the site-private scan now reads literal values from a site file outside the repository and
     covers history (§ 2, "Site-private material");
  2. the gates load a fake `/etc/profile` with no `module` on `PATH`, and assert the profile ran,
     `HOME` was absent meanwhile, and the loaded toolchain reached build and check (the fake also
     sets `MAKEFLAGS`, `CXX` and `CDPATH`, which the job must clear again);
  3. `make_vars='CXX=gate-c++ CXXFLAGS=-O1'` with a wrapper compiler; compile lines and the
     recorded compiler are checked in build and check;
  4. the fixture is cloned over `file://` and its `HEAD` is a newer commit than any tested one;
  5. the interpreter ban no longer exempts `/` or `-` prefixes; the utility list now names `tail`;
  6. P4 evidence: target and redacted cluster on all five records, a scheduler output per run, the
     printed script hash against the committed script, `sacct` state and exit code per job; review
     checks `verify/` is unchanged (§ 6);
  7. step 0 clears inherited variables; a polluted happy run and a polluted red build;
  8. R1 amended by the foreman; no deviation remains;
  9. the JSON checker rejects duplicate keys and `NaN`; the partition-mismatch case uses a cluster
     name holding `"`, `\` and a tab;
  10. the signal case requires build ≥ 2 s; P1 requires start and end inside the gate's own run;
  11. P4's freshness diff excludes `issues/` and `ROADMAP.md`, its scan requires a non-empty list
      covering the job, and files are listed from `origin/main...HEAD`;
  12. the clone must have no `objects/info/alternates`; the first run is `chmod 000` during the
      second; one run puts Homebrew's coreutils `gnubin` first on `PATH` (installed here).
  Folding them in surfaced one more fact: dash's and macOS sh's `echo` interpret backslashes, so the
  job and the gates write outside values with `printf`, and P2's partition-mismatch value holds `\c`
  to catch an `echo`.

## 5. Risks & open questions

- **D1 — `main` did not build with GCC on Linux (resolved).** `src/mp.hpp` used `uint16_t` without
  `<cstdint>`; every GCC 14 build on all three clusters failed, and the include made `make check`
  pass with no warnings on all three (brief 02). The foreman added the include to `main` as a direct
  commit, `97962ce` (`[mp] Include <cstdint> for the fixed-width integer types`). P4 first submits
  this branch's head before the fix as a *real red build*, then merges `origin/main` into this branch
  and pushes before the green runs, so the tested commit contains the fix.
- **The MCP server's string building.** The argument mechanism relies on `script_path` being appended
  unquoted. A future server that quotes it would break submission (the job would see no arguments and
  exit 2, loudly). The fallback is `run_command` with `sbatch … <file> <hash> <target>`.
- **Evidence authenticity.** P4's gate checks the evidence for consistency, not provenance; the
  orchestrator grades R11 from the job ids, `sacct` and the session transcript.
- **Redacted evidence.** The committed `result.json` copies carry `<cluster>` and `<host>`; the
  unredacted files stay in scratch (disposable) and in the session transcript.
- **The site file is a gate dependency.** P3 and P4 fail without it, by design; a reviewer without
  it cannot run the scan.
- **Requeue.** A preempted job may be requeued under the same job id; the job then finds its run
  directory and exits 3 rather than reuse it. The factory resubmits on any failure that is not the
  expected one.
- **Queue waits.** Two of the three targets run in a QoS that waits for idle nodes, and the GB10
  cluster allows one job at a time; P4 has no time-critical step.
- **Portability beyond these sites.** `/etc/profile` and `SCRATCH` are conventions, not standards; a
  site without them sets `MPS_SCRATCH` and submits from a login environment.
- **`date +%s`** is not POSIX `date`, but every target, macOS, GNU, BSD and busybox have it (brief 03).

## 6. Verification strategy

- **P1–P3 run on the Mac**, each as `sh spec/cluster-test-jobs/verify/pN.sh`, with the harness of
  [brief 04](research/04-gate-design.md): the working tree's job, copied outside the repo with its
  `/etc/profile` pointed at a fake, run under `env -i` from an unrelated directory against a bare
  fixture repository over `file://`, with shadowed interpreters and a sandboxed scratch, home, temp
  and working directory. `result.json` is checked field by field against independently computed
  values (hash, host, architecture, the wrapper's compiler line, make line, times inside the gate's
  run), and every failure path in brief 04's table must go red in exactly the way R3–R6 describe.
  Every gate fails with a `GATE FAIL:` line naming the R-ID.
- **P4 runs on the clusters** through the MCP server and records redacted `result.json` files,
  scheduler outputs and `sacct` states under `spec/cluster-test-jobs/evidence/`; its gate checks that
  evidence and re-runs the site-private scan over files and history.
- **The gates are fixed at plan time.** Review checks that `spec/cluster-test-jobs/verify/` is
  unchanged since the commit that adds this plan (`[feature] Plan cluster-test-jobs: design + phased
  roadmap`): `git diff --quiet <that commit> HEAD -- spec/cluster-test-jobs/verify`. A gate edited
  during build is a finding unless the phase body records why.
- **The gates were dry-run before commit**, against a throwaway prototype of the job kept outside the
  repository. On the first draft, P1–P3 went green, P4 went green on synthetic evidence and red when
  code changed after the tested commit, and ten deliberately broken prototypes each turned the
  intended gate red (a stray write, no `module purge`, a Python call, a missing field, swapped steps,
  no prefix check, no `TERM` trap, no failure tail, `mkdir -p` on the run directory, sourcing the
  target before validating it). After the review was folded in, P1–P3 are green on the updated
  prototype (P1 about 25 s, P2 about 16 s, P3 about 5 s, its scan covering these drafts and the
  history), the scan fails on a missing or incomplete site file and on a planted cluster name, QoS
  and site path, and P4 is green on synthetic evidence put through `redact.sh` and red on a wrong
  `sacct` state, a wrong script hash and a code change after the tested commit. Fourteen broken
  prototypes in all turned the intended gate red, the four new ones being no second `unset` after the
  profile, no script-hash line, an `echo` for messages, and a `HOME` left in place for the profile.
- The profile's `verify` (`make check`) still passes untouched, and each gate runs it in the fixture
  as part of the job's own check step.

---

*Backing research: [`research/00-digest.md`](research/00-digest.md).*
