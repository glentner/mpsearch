# 03 — `result.json` from POSIX sh, and timing the steps

**Question.** How does a job limited to a POSIX shell and its standard utilities (R10: no Python, no
jq) write valid JSON and time each step?

## Strings

Only a handful of values are free text: the requested hash, the full hash, the target, the cluster,
the host, the compiler's first version line and the make version line. The job runs under
`LC_ALL=C`, so tool output is ASCII. One function makes any value a safe JSON string:

```sh
json_str() { # VALUE: a JSON string literal; anything but printable ASCII is dropped
	printf '"%s"' "$(printf '%s' "$1" | tr -cd '\040-\176' | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g')"
}
```

`tr -cd '\040-\176'` keeps printable ASCII only, so control characters, tabs, newlines and stray
high bytes can never produce invalid JSON; then backslashes and double quotes are escaped, in that
order. Tested under dash and macOS `/bin/sh` with a value holding `"`, `\` and a tab; Python's
`json.load` accepted the output. An absent value (no full hash after a failed checkout, no compiler
before the toolchain step) is written as `null`, not as an empty string.

Numbers (durations) are produced by shell arithmetic, so they are always plain integers.

Messages that carry an outside value (a hash, a target, a partition, a cluster name) are written with
`printf '%s'`, never `echo`: dash's `echo`, and macOS `sh`'s in POSIX mode, interpret backslashes, and
`\c` silently ends the output. The gates found this with a cluster name holding a backslash.

## Atomic write

The job writes `result.json.tmp` and renames it to `result.json` (`mv` within one directory is a
rename), so a reader polling the run directory never sees half a file. `result.json` is written once,
at the end, on every path that reaches the steps, including a `TERM` from the scheduler at the time
limit (the trap marks the running step failed and writes the file before exiting).

## Times

- Start and end: `date -u +%Y-%m-%dT%H:%M:%SZ` (POSIX).
- Durations: `date +%s` before and after each step. `%s` is not in POSIX's `date`, but GNU, BSD/macOS,
  busybox and all three cluster nodes support it (brief 02). The strictly POSIX alternative,
  `awk 'BEGIN { srand(); print srand() }'`, relies on `srand()`'s time-of-day seed and is obscure; it
  is not worth the readability cost. Durations are whole seconds, which R7 asks for.
- A step that did not run has `"seconds": null` and `"status": "not_run"`.

## Shape

```json
{
  "format": 1,
  "requested": "<hash as given>",
  "commit": "<full 40-hex hash, or null>",
  "target": "<target name>",
  "cluster": "<SLURM_CLUSTER_NAME, or null outside Slurm>",
  "host": "<uname -n>",
  "arch": "<uname -m>",
  "job_id": "<SLURM_JOB_ID, or the local stand-in>",
  "start": "2026-10-11T22:53:08Z",
  "end": "2026-10-11T22:53:40Z",
  "compiler": "<first line of $CXX --version, or null>",
  "make": "<first line of make --version, or null>",
  "steps": [
    {"name": "clone", "status": "passed", "seconds": 1},
    {"name": "checkout", "status": "passed", "seconds": 0},
    {"name": "toolchain", "status": "passed", "seconds": 2},
    {"name": "build", "status": "passed", "seconds": 9},
    {"name": "check", "status": "passed", "seconds": 4}
  ],
  "status": "passed"
}
```

- `steps` is an array because R6's order is part of the contract; an object's keys are unordered.
- `status` values: `passed`, `failed`, `not_run` for steps; `passed` or `failed` overall.
- `format: 1` follows § Conventions ("a change to the checkpoint or result format bumps that format's
  version in the same commit"); the job's header says so.
- No path appears in the file. The run directory's path contains the user's scratch, and a cycle
  copies `result.json` into its records (R9). The host name and the cluster name do carry site
  names, so the copy in the records is redacted to `<host>` and `<cluster>` (`verify/redact.sh`).
