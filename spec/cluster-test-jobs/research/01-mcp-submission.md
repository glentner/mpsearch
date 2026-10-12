# 01 — Submitting through the cluster MCP server: parameters and how arguments arrive

**Question.** What exactly does the cluster MCP server's `sbatch` tool accept, and how does a job
submitted through it receive the commit hash and the target name?

**Sources.** The tool's input schema, listed through an MCP client, and the server's own source for
that tool (`tools/slurm.py` in the site's open-source MCP server, commit `c285704`), read 2026-10-11.
Six probe jobs on 2026-10-11 confirmed the behaviour (see
[`02-cluster-environments.md`](02-cluster-environments.md)).

## The tool's parameters

`sbatch(script_path | script_content, job_name, account, partition, qos, time, nodes, ntasks,
cpus_per_task, mem, gpus, constraint, output, error, extra_args)`, all optional strings or integers.
Exactly one of `script_path` and `script_content` is required. The server builds one shell string and
runs it over SSH in the user's login shell, in the home directory:

```
sbatch -J <job_name> -A <account> -p <partition> -q <qos> -t <time> -N <nodes> -n <ntasks> \
       -c <cpus_per_task> --mem=<mem> --gpus=<gpus> -C <constraint> -o <output> -e <error> \
       <extra_args> <script_path>
```

Each option appears only when its parameter is set. Nothing is quoted.

## How the job receives its two arguments

There is **no parameter for script arguments**, and `extra_args` lands *before* the script path, so
it cannot carry them. Three mechanisms remain:

| Mechanism | What happens | Verdict |
|---|---|---|
| `script_content` | The server writes the text to `~/jobs/<job_name>_<timestamp>.sh` in the **home directory**, then submits that file with no arguments | Rejected: it cannot pass arguments, and it writes into the home directory on every submission |
| `script_path` = `"<file> <hash> <target>"` | The path string is appended unquoted, so the login shell splits it and `sbatch` receives `<file> <hash> <target>`: the script's `$1` and `$2` | **Chosen.** Probe jobs on all three clusters printed `args: 2 [deadbeefcafe probe-target]` |
| `extra_args` = `"--export=ALL,MPS_X=…"` | Passes environment variables to the job | Works (the probes printed the exported value), but it does not pass arguments; not used |

So the factory's submission is two MCP calls, with no clone on the cluster, which is what the amended
R1 asks for (the script taken unchanged from the tested commit, submitted by path with arguments):

1. `write_file` the job script, taken from the commit under test (`git show <hash>:ci/test-commit.sh`),
   to a file under the user's scratch, for example `<scratch>/mpsearch/jobs/<name>.sh`.
2. `sbatch` with `script_path="<that file> <hash> <target>"`, the target file's settings mapped onto
   the tool's parameters, `account`/`qos` from `MPS_SLURM_ACCOUNT`/`MPS_SLURM_QOS`, and `output` set
   to a file in that same scratch directory.

The file in step 1 is the submission copy of the script, written by the submitter, not by the job;
R5 constrains what the *job* writes. The job prints `git hash-object` of the script it runs, so a run
can be tied to the committed script byte for byte.

## Consequences for the design

- **Two positional arguments stay the contract** (R1, R4). The job never reads the hash or the target
  from the environment, so a stray exported variable cannot change what is tested.
- **`output` must always be set.** The server runs `sbatch` from the home directory, so Slurm's default
  `slurm-<jobid>.out` would land there. The output directory must exist before submission (Slurm does
  not create it, and a job whose output file cannot be opened fails without a trace); step 1 creates
  it.
- **The job's environment is the MCP session's environment.** `sbatch` exports the submitting
  environment by default, and the server's SSH session is a non-login shell without the site's
  profile: no `module` function, and on one cluster no scratch variable either. The job must load the
  login environment itself when it is missing (brief 02), and must clear inherited variables that can
  redirect git or make.
- **Unquoted string building.** The hash and target are validated by the job, but they are also
  interpolated into the submitter's shell string. A hash is hex and a target name is
  `[A-Za-z0-9._-]`, so neither can carry shell syntax; the submitter must still pass only values it
  has validated the same way.
- **Mapping target keys to tool parameters**: `partition` → `partition`; `gpus` → `gpus` (omitted
  when empty); `cpus` → `cpus_per_task` with `ntasks=1`; `mem` → `mem`; `time` → `time`. Which
  cluster serves which target, and the account and QoS, are site knowledge: they live in the
  submitter's environment and site file, outside the repository.

## Not adopted

- A submit wrapper in the repo (`ci/submit.sh`). It would need a clone and a local `sbatch`, the
  opposite of R1's no-clone path, and it would be a second copy of the key → option mapping. The
  mapping lives once, in the job script's header.
