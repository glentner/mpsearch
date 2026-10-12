# 00 — Research digest: cluster-test-jobs

Research ran because the deliverable is test infrastructure (code-plan trigger 3), sequentially, by
the planner. Four briefs; the decisions below are the ones `PLAN.md` builds on. The briefs do not
contradict each other. Clusters are named by the node type they provide; the mapping to site
clusters lives outside the repository.

| # | Brief | Decision |
|---|---|---|
| 01 | [MCP submission](01-mcp-submission.md) | The job takes `HASH TARGET` as positional arguments. The factory uploads the job (from the tested commit) to scratch with `write_file` and submits it with `sbatch`'s `script_path="<file> <hash> <target>"`; `script_content` is never used (no arguments, writes into home). R1 was amended to say so. `output` is always set to scratch. No submit wrapper in the repo. |
| 02 | [Cluster environments](02-cluster-environments.md) | GitHub is reachable from every compute node. The job sources `/etc/profile` when `module` or a scratch variable is missing, with `HOME` at a non-existent path, then moves `HOME` and `TMPDIR` into the run directory. Scratch is `MPS_SCRATCH`, else `SCRATCH`. Toolchains: `gcc/14.1.0` (AVX-512, partition `cpu`), `gcc/14.4.0` (AVX2, partition `cpu`), `gcc/14.2.0` (GB10, aarch64, partition `spark-batch`, one GPU requested). `module purge` before loading; `CXX` always passed. `/bin/sh` is dash on the GB10 cluster. Host names carry site names, so evidence is redacted. |
| 02 | Blocker (resolved) | `main` did not compile with GCC 14 on Linux: `src/mp.hpp` lacked `<cstdint>`. The foreman fixed `main` directly (`97962ce`); P4 merges `origin/main` before the green runs. |
| 03 | [JSON and timing](03-posix-json-and-timing.md) | Strings through `tr -cd '\040-\176'` and two `sed` escapes; `null` for absent values; `steps` as an ordered array; `format: 1`; written via a temporary file and `mv`; `date +%s` for durations; a signal trap still writes the file. |
| 04 | [Gate design](04-gate-design.md) | Local gates on a `file://` fixture with a newer `HEAD`, a fake login profile and wrapper compiler, shadowed interpreters, polluted and GNU-tools runs, and a sandbox containment check, under dash and sh; a literal-value site scan over files and history; real-cluster evidence redacted, checked in P4 and graded by the orchestrator. The foreman's adversarial review (twelve findings) is folded in. |

## Probe jobs (2026-10-11, US Eastern)

All submitted through the MCP server with the approved account and QoS taken from the environment,
1 task, 4 CPUs, a 15-minute limit, output under `<scratch>/mpsearch/research/`; all ended
`COMPLETED 0:0` in `sacct`.

| Cluster | Job | Submitted | Memory | Outcome |
|---|---|---|---|---|
| AVX-512 | 16962512 | 22:52:05 | 4G | arguments and exported variable received; network, clone and checkout fine; `make check` of `main` failed (`uint16_t`) |
| AVX2 | 44156336 | 22:52:07 | 4G | same |
| GB10 | (rejected) | | | CPU-only job refused at submit: the partition requires a GPU request |
| GB10 | 474502 | 22:52:17 | 8G, 1 GPU | same as AVX-512, under dash, on aarch64 |
| AVX-512 | 16962573 | 22:57:17 | 4G | `make -k`: all errors from the missing include; with it added, `make check` green (6 + 14, no warnings) |
| AVX2 | 44156509 | 22:57:19 | 4G | same |
| GB10 | 474503 | 22:57:21 | 8G, 1 GPU | same |
