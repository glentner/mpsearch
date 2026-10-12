# 04 — Gates: what can be proved on the Mac, and what only a cluster can prove

**Question.** The deliverable is test infrastructure, so a design error fails in the green direction
(code-plan trigger 3). Which properties can a local gate prove, how, and what is left to the real runs
of R11? This brief includes the twelve findings of the foreman's adversarial review of the first
draft.

## The local harness (`verify/common.sh`)

Everything runs in a temporary directory, and the job under test is the working tree's
`ci/test-commit.sh`, so a gate grades what the phase is about to commit.

- **Fixture repository.** The gate copies the tree as a clone would hold it (tracked plus untracked,
  unignored files: cycle 1's `copy_tree`) into a scratch git repository and commits it with a fixed
  identity. Further fixture commits branch from it: one with a compile error, one whose `make check`
  fails, one whose build sleeps (for the signal test). A last commit on the default branch makes the
  fixture's `HEAD` newer than every tested commit, so a job that built `HEAD` would be caught. A bare
  clone of the fixture, addressed as `file://…`, is the job's `MPS_REPO_URL`: git then copies objects
  rather than borrowing them, and the gate asserts the clone has no `objects/info/alternates`.
- **Local targets.** The fixture's first commit adds gate-only target files beside the real ones:
  `local-stub` (module `stub/1.0`, `make_vars='CXX=gate-c++ CXXFLAGS=-O1'`), one with a module that
  does not exist, and the malformed ones of P3. Adding them touches no other file, which is R8's
  "only a new target file".
- **A realistic login environment.** The gate's copy of the job has its `/etc/profile` path rewritten
  to `$work/etc/profile`, and `module` is not on `PATH`, as in a job submitted through the MCP server.
  The fake profile defines a `module()` shell function (`purge`; `load stub/*` prepends a toolchain
  directory to `PATH`; any other `load` fails, as Lmod does), sets `SCRATCH`, sets `MAKEFLAGS`,
  `CXX` and `CDPATH` to harmful values (site profiles set build variables too, so the job must clear
  them again after the profile), and records whether `$HOME` existed while it ran. The toolchain directory holds `gate-c++`, a wrapper around `clang++`
  with its own `--version` line. So the gate proves three things together: the job sources the
  profile when `module` is missing, `HOME` did not exist meanwhile, and the loaded toolchain persists
  into build and check (every compile line starts with `gate-c++`, carries `-O1` and not `-O3`, and
  `result.json`'s compiler is the wrapper's line).
- **The other branch.** One run has an executable `module` on `PATH` and the toolchain already on
  `PATH`, and a scratch variable set: the job must not source the profile.
- **Shadowed interpreters.** `python`, `python3`, `jq`, `perl`, `ruby` and `node` on `PATH` are
  stubs that drop a marker file in the gate's own directory and exit 127; any marker fails R10.
- **A sandbox and a clean environment.** The job runs under `env -i` with only the variables the gate
  lists, from `<sandbox>/cwd`, as a copy outside any repository (R1's "no prior clone").
- **Polluted environment.** One happy run and one red-build run add `CDPATH`, `GIT_DIR`,
  `GIT_WORK_TREE`, `GIT_CONFIG_PARAMETERS`, `MAKEFLAGS=n`, `MAKELEVEL`, `MFLAGS`, `CXX` and `CXXFLAGS`
  pointing somewhere harmful; results must be identical to the clean runs (a `MAKEFLAGS=n` that
  survived would turn the red build green).
- **GNU tools.** One run puts Homebrew's coreutils `gnubin` first on `PATH` when it is installed (GNU
  `date`, `head`, `tail`, `tr`, `mkdir`, `mv`, `uname`); without it the gate says so, and P4's real
  runs on Linux cover GNU tools.
- **Containment (R5).** After a run, the sandbox may hold only the run directory, its contents and its
  ancestors; `home/`, `tmp/` and `cwd/` must be empty. During the second run the first run's directory
  is `chmod 000`, so a second run that read or reused it fails; afterwards it must be byte-identical.
- **`result.json` checker.** A short Python script (gate-only; R10 binds the job, not its gates)
  rejects duplicate keys and `NaN`/`Infinity`, and validates the exact key set, types, the step order,
  the pass/fail/not-run pattern, `null` for absent values, ISO-8601 UTC times with end ≥ start, start
  and end inside the gate's own run, and every expected value the gate passes in.
- **Script identity.** The job prints `mps-test: script <git hash-object of $0>`; P1 checks it against
  the copy it ran, and P4 against the tested commit's `ci/test-commit.sh`.

## What each failure-path test pins

| Path | Fixture or input | Expected |
|---|---|---|
| Argument count (R4) | 0, 1 and 3 arguments | exit 2, a usage message, nothing created under the sandbox |
| Malformed hash or target | `xyz`, upper-case hex, 6 and 41 hex digits, empty, an embedded newline, `../../x`; `../x`, `.x`, `-x`, `a/b`, empty, a space | exit 2, a message naming the value, nothing created |
| No scratch (R5) | no scratch variable even after the profile; a relative `MPS_SCRATCH`; a missing directory | exit 3, a message about scratch, nothing created |
| Scratch precedence | `SCRATCH` only; both set | the run lands under `SCRATCH`, then under `MPS_SCRATCH` |
| Run directory exists (R5) | pre-created with a sentinel | exit 3, a message, the directory unchanged |
| Not a commit (R3) | a well-formed absent hash, full and short; a hex *tag name* in the fixture | "not a commit" and the hash printed; clone passed, checkout failed, rest not_run; `commit` null |
| Clone fails | `MPS_REPO_URL` at a missing path | clone failed, rest not_run |
| No target file (R4) | an unknown target name | a message naming it; toolchain failed; no build log |
| Bad module | a target loading a module the fake refuses | toolchain failed; no build log |
| Wrong partition | `SLURM_JOB_PARTITION` is `else\cwhere`; `SLURM_CLUSTER_NAME` holds `"`, `\` and a tab | toolchain failed naming the partition verbatim (an `echo` would stop at `\c`); `result.json` still valid JSON with the escaped cluster |
| Build fails (R6) | the compile-error commit, clean and polluted | build failed, check not_run |
| Check fails (R6) | the failing-check commit | build passed, check failed |
| Killed at the time limit | `TERM` to the job shell, then each process that was its descendant mid-build | `result.json` still written, build failed after ≥ 2 s, the "interrupted" message printed, none of those processes left alive |
| Target executed? (R8) | target lines with `$( )`, backquotes, double quotes, `;`, `${…}`, an unknown key, a duplicate, a leading space | toolchain failed naming the line; the marker those lines would create never appears |

## Site-private scan (R9)

The scan reads literal values from a site file outside the repository,
`${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}` (accounts, QoS names, the cluster user, cluster
names, the scratch prefix, other site path prefixes), and fails if the file is missing or any value is
empty. Words match case-insensitively; QoS names, being common words, match only after `-q`,
`--qos` or `qos=`; path prefixes match as substrings. The public repository URL is the one allowed
appearance of its owner's name. It covers every committed and untracked file the cycle touches and
the messages and patches of `origin/main...HEAD`; the builder runs it before every push.

## What only the cluster can prove (R11, and R1 for real)

`/etc/profile` itself, real Lmod, GCC on Linux, dash as `/bin/sh`, the network, Slurm's own variables
and the MCP submission path exist only there. P4 pushes the branch, submits through the MCP server,
and copies each `result.json` and scheduler output into `spec/cluster-test-jobs/evidence/` after a
redaction helper (`verify/redact.sh`) replaces the site literals, the cluster name and the host with
placeholders. Its gate checks the evidence for consistency: overall success and GCC ≥ 13 on each
target, all three runs on one pushed commit whose code equals `HEAD`'s, the architecture each target
promises, each run's printed script hash equal to that commit's `ci/test-commit.sh`, `sacct`'s state
and exit code for every job, the negative control's shape, and a real red build. A gate cannot prove
the evidence came from a real run; the job ids, `sacct` and the orchestrator's grading carry that.

## Not adopted

- Running the job under a `PATH` of whitelisted utilities only: on macOS, `git`, `make` and `clang++`
  are `xcrun` shims with helpers of their own; the shadow stubs, the static interpreter ban and the
  `gnubin` run cover the same ground with less machinery.
- A fake `sbatch` exercising a submit wrapper: there is no wrapper (brief 01).
