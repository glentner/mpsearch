# 02 — The three target clusters, inside a batch job

The three clusters are named here by the node type each one provides: the **AVX-512 cluster**
(partition `cpu`), the **AVX2 cluster** (partition `cpu`) and the **GB10 cluster** (partition
`spark-batch`). Which site cluster is which lives in the factory's site file, outside the repository.

**Question.** On each: which GCC 13+ modules exist, are `git` and `make` on compute nodes, which
scratch variable is set inside a job, does `module` work in a non-login batch shell, and can compute
nodes reach GitHub?

**Method.** Login-node reads through the MCP server's `run_command`, then two rounds of probe jobs
submitted exactly as the factory will submit (`write_file` to scratch, then `sbatch` with
`script_path="<file> <arg1> <arg2>"`, the approved account and QoS from the environment, 4 CPUs,
4–8 GB, a 15-minute limit, output under the user's scratch). Probe scripts wrote only under
`<scratch>/mpsearch/research/`; `HOME` was pointed away from the real home directory throughout.

| Round | AVX-512 | AVX2 | GB10 | What it ran |
|---|---|---|---|---|
| 1 | 16962512 | 44156336 | 474502 | environment, modules, tools, network, clone and checkout of `main`, `make check` with the newest GCC |
| 2 | 16962573 | 44156509 | 474503 | `make -k check` on `main` to size the failure, then `make check` with one include added |

A first GB10 submission without a GPU request was rejected at submit time (no job id).

## Answers

| | AVX-512 (`cpu`) | AVX2 (`cpu`) | GB10 (`spark-batch`) |
|---|---|---|---|
| Node | x86-64, AMD EPYC 9654 (Zen 4): `avx2` and `avx512f` | x86-64, AMD EPYC 7763 (Zen 3): `avx2`, no `avx512f` | aarch64, NVIDIA GB10 (Cortex-X925/A725): `asimd`, `sve` |
| GCC ≥ 13 module | `gcc/14.1.0` (also the default) | `gcc/14.4.0` (default is `gcc/12.2.0`) | `gcc/14.2.0` (default; also `gcc/12.5.0`) |
| Compiler's first version line | `g++ (Spack GCC) 14.1.0` | `g++ (Spack GCC) 14.4.0` | `g++ (Spack GCC) 14.2.0` |
| `git`, `make` on the node | git 2.47.3, GNU Make 4.3 | git 2.43.7, GNU Make 4.2.1 | git 2.43.0, GNU Make 4.3 |
| `/bin/sh` | bash 5.1 | bash 4.4 | **dash** |
| `module` defined in the job as submitted | no | no | no |
| Scratch variable in the job as submitted | `SCRATCH` and the site's own name set | same | **neither set** |
| After `. /etc/profile` | `module` works; both variables set | same | same (also under dash) |
| GitHub over HTTPS (`git ls-remote`, `git clone`) | yes, ≤ 1 s | yes, ≤ 1 s | yes, ≤ 1 s |
| Submit-time requirement | none | none | **a GPU request** (`--gpus=1`): the partition rejects CPU-only jobs |
| Memory limit to respect | about 2 GB per CPU at most, so `mem` ≤ 2 GB × `cpus` | none relevant | default is the whole node's memory: set `mem` explicitly |
| `SLURM_CLUSTER_NAME` | set (the site's cluster name) | set | set |

## Findings that shape the design

1. **Network is not a blocker.** All three compute nodes clone from GitHub in about a second, so the
   job clones the public URL directly (R2).
2. **The job must load the login environment itself.** The MCP session is a non-login shell, and
   `sbatch` exports it to the job: no `module` function on any cluster, and on the GB10 cluster no
   scratch variable either. Sourcing `/etc/profile` restores both, under bash and under dash. The job
   does so only when `module` is missing or no scratch variable is set.
3. **The login environment writes to `HOME`.** With `HOME` redirected, sourcing the profile left a
   `.cache/` directory in the redirected home on the AVX-512 cluster. With `HOME` pointed at a path
   that does not exist, the profile, `module purge` and `module load` all succeed and nothing is
   written. The job therefore loads the profile with `HOME` at a non-existent path and, once the run
   directory exists, points `HOME` and `TMPDIR` into it (R5).
4. **`SCRATCH` is enough.** Wherever a scratch variable is set, `SCRATCH` is set beside the site's own
   name, and `SCRATCH` is the conventional name at other sites too. The job reads `MPS_SCRATCH`, then
   `SCRATCH`; it names no site-specific variable.
5. **Pass `CXX` explicitly.** On two clusters the GCC module exports `CXX` (the Makefile then honours
   it, origin `environment`); on the third it does not, and the Makefile falls back to `clang++`. A
   target's make variables therefore always set `CXX`, the job clears an inherited `CXX` before
   loading modules, and it asks `make` which compiler it will use rather than guessing.
6. **`module purge` is safe.** It leaves only the site's sticky modules, and the GCC modules stay
   loadable afterwards, so a target's toolchain is loaded onto a known baseline regardless of what the
   submitter had loaded. `module load` of a missing module exits 1, so a bad target fails its step.
7. **Compiler identification is `(Spack GCC)`, not `(GCC)`.** A "GCC 13 or newer" check matches
   `GCC) <major>.` with major ≥ 13, not the literal `(GCC)`.
8. **`/bin/sh` is dash on the GB10 cluster.** The job is POSIX sh in earnest; the local gate runs it
   under dash.
9. **Dates.** `date +%s` and `date -u +%Y-%m-%dT%H:%M:%SZ` work on all three nodes and on macOS.
10. **Host names carry site names.** `uname -n` on a compute node is a fully qualified name that
    includes the cluster's and the site's names, so evidence copied into the repository is redacted.

## The blocker: `main` did not build with GCC on Linux (resolved)

`make check` of `main` at `283a034` failed on all three clusters, in the same place:

```
src/mp.hpp:20:21: error: 'uint16_t' was not declared in this scope
note: 'uint16_t' is defined in header '<cstdint>'; this is probably fixable by adding '#include <cstdint>'
```

`make -k check` showed that every error (32 in `src/mp.hpp`, 12 in `src/mp.cpp`) follows from that one
missing include: `src/mp.hpp` uses `uint16_t` and included only `<iostream>`, `<string>` and
`<vector>`. Apple clang and Homebrew GCC 16 on macOS get `<cstdint>` transitively; GCC 14's libstdc++
on Linux does not. With `#include <cstdint>` added (in the probe's own clone), `make check` passed on
all three with zero warnings: `check: 6 passed, 0 failed` and `cli: 14 passed, 0 failed`, in 13 s,
19 s and 42 s.

This answered cycle 1's open "GCC 13 or later" verification: the claim was false on Linux. The
foreman settled it (decision D1, PLAN § 5): `main` gained the include as a direct commit, `97962ce`,
and P4 merges `origin/main` into this branch before the cluster runs.
