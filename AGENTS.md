# MPSearch: agent notes

`mps` tests Mersenne numbers 2^p − 1 for primality with trial factoring, P-1 and the Lucas–Lehmer
test. All three are stubs today, and PRP is planned (see `ROADMAP.md`). It is dependency-free C++20,
built with a plain Makefile. The goal is a
feature-complete tester that runs the same mathematics on CPUs, GPUs and TPUs. It must verify the
known Mersenne primes at a speed and efficiency that rival the established programs, and it doubles
as a burn-in and regression test for HPC nodes: a known exponent has a known answer and an expected
walltime. Read `README.md` and `ROADMAP.md` first.

## Build and run

```sh
make                      # bin/mps from src/*.cpp (objects in lib/)
make check                # bin/check from tests/*.cpp and the modules, built with the standard
                          # libraries' bounds assertions (objects in lib/check/); run it, then
                          # the smoke test tests/cli.sh
make clean                # remove the objects in lib/ and the binaries in bin/
bin/mps --help
bin/mps -p 100            # primes below 100
bin/mps -t ll 31          # one test on one exponent
```

The build needs only a C++20 compiler and GNU make, and `make check` adds only a POSIX shell. Apple
clang and GCC must both build it: macOS ships GNU make 3.81, so keep the Makefile within it.

## Map

- `src/main.cpp`, `src/main.hpp`: `main()`, the metadata, the usage and help text, and the exit
  statuses (`enum Status`).
- `src/cli.cpp`, `src/cli.hpp`: the option parser (`cli::Interface`) and the test names
  (`test_by_name`).
- `src/log.hpp`: the header-only logger. Its detailed style stamps the time, the hostname and
  `TASK_ID`.
- `src/ansi.cpp`, `src/ansi.hpp`: terminal colour, honouring `NO_COLOR` and `FORCE_COLOR`.
- `src/exc.hpp`: `mps_error`, the base of the project's exceptions.
- `src/io.cpp`, `src/io.hpp`: the `.mps` checkpoint file (format sketch only).
- `src/mp.cpp`, `src/mp.hpp`: `BigInt`, the multiple-precision type (a stub).
- `src/math.cpp`, `src/math.hpp`: the prime sieve; the exponent check (`check_exponent`), which
  reports a factor only through its re-check (`composite_by_exponent`); the three tests (stubs);
  and `run_in_order`, which runs them and stops at the first decisive result.
- `src/result.cpp`, `src/result.hpp`: the result type, the verdict line, the JSON record and the
  exit status.
- `doc/`: the mathematics, one note per algorithm family, with `README.md` (the index) and
  `references.md` (the bibliography, each entry checked against the work).
- `tests/`: the checks. `check.hpp` and `check.cpp` are the runner (a check is a `static void`
  function listed in its file's `check_by_name` table), `check_<module>.cpp` hold one module's
  checks, and `cli.sh` is the smoke test of `bin/mps`, in shell built-ins only. `make check` runs
  both. `log_peer.hpp` and `log_peer.cpp` are a second unit the logger's checks log from, so a
  state that isn't shared shows; they hold the checks' guards (`SavedLogger` restores the logger's
  state, `CaptureCerr` captures `std::cerr`) and, run last, the check that the state is restored.
- `ci/test-commit.sh`: the cluster test job. It builds and checks one commit on one target in a
  fresh run directory in scratch and leaves the logs and `result.json` there; its header is the
  manual. `ci/targets/`: one `key=value` file per target, naming its partition, resources, modules
  and make variables.

## Factory

This repo is built by the spec-driven factory: GOAL → PLAN/TECH → build → blind review → publish. The method and the skills live in the operator's KB (`method/agentic-engineering.md`, `skill/code-*.md`). The cycle's records live here: `spec/{slug}/` for work in flight, `issues/` and `ROADMAP.md` for deferred work. Inside this repo, its own rules win.

### Profile

| Key | Value |
|---|---|
| base | `main`, the branch every cycle starts from and lands on |
| branches | `feature/{slug}`, `fix/{slug}`: the prefix per GOAL `kind`; any other kind takes `feature/`; the set is open |
| commit categories | `[cli]` `[log]` `[io]` `[math]` `[mp]` `[burnin]` `[cpu]` `[metal]` `[cuda]` `[hip]` `[tests]` `[ci]` `[build]` `[docs]`: the area a change lands in; the set is open. Branch commits take the GOAL's `kind` verbatim |
| branch commits | `[{kind}] Build {slug} P<n>`; no trailers |
| landing | `squash-local`: squash onto `origin/main` in the cycle worktree, one `[area] What changed` subject, then push direct to `main` |
| ci | none |
| setup | `make` |
| verify | `make check`: the checks and the smoke test, both always run (`make -B check` after changing `CXX`) |
| cluster test | `ci/test-commit.sh <hash> <target>`, copied from the tested commit into scratch and submitted to Slurm by path with its two arguments; the options, environment, run directory and `result.json` are in its header |
| throwaway environment | none: `mps` keeps no state outside its own working directory yet |
| coupled core | the command-line surface (`src/main.hpp` usage and help, `src/cli.*`, `README.md` usage); the test entry points (`src/math.hpp` and the dispatch in `src/main.cpp`) |
| same-commit rules | see § Conventions |
| high-blast-radius paths | see § Invariants |
| security lane | off: a local calculator with no network access, no credentials, and no input beyond its arguments and its own checkpoint files |
| release | see Release recipe (`release.trigger`) |

### Release recipe

```yaml
release:
  trigger: on-demand
  version:
    source: src/main.hpp APP_VERSION
    scheme: semver
    tag_format: "{v}"
    prerelease_syntax: '^\d+\.\d+\.\d+-(alpha|beta|rc)\.\d+$'
    propose_next: by-kind
  bump:
    lock: none
    lockstep: []
    files: [src/main.hpp]
    diff_rule: only the APP_VERSION string changes
    subject: "[build] Version {v}"
  gate: [make clean, make check]
  tag:
    message: "Release {v}"
    signed: true
  notes: git-log-by-category
  modes:
    release:
      base: main
      prerelease: forbidden
      final: true
      inputs: []
      work_branch: in-place
      branch_ops: []
      merge_into: none
      tag_on: bump
      back_merge_into: none
      push: [HEAD:main, "{v}"]
      publish: none
  post_verify: []
  caveats: {}
  clean_paths: [lib/, bin/]
```

### Where deferred work goes

A pass that decides not to fix something records it, and the destination is fixed:

| Home | Holds | Written by |
|---|---|---|
| `issues/{slug}.md` | deferred code work, from the `ISSUE.md` template, `status: unshaped` | whoever defers it |
| `ROADMAP.md` | the ordered index: one entry per issue, its `**Seed:**` pointing at the `issues/` file | whoever defers it |
| `spec/{slug}/` | work in flight | the lifecycle skills |
| the KB inbox | harness friction ("was this the skill's fault?"), as `friction` lines; never code follow-ups | the lifecycle skills |

An `issues/` seed is a candidate, not a contract. The feature step promotes it into `spec/{slug}/GOAL.md`, and that is where appetite, non-goals and R-IDs get negotiated. A seed is never copied into a GOAL verbatim.

## Invariants

The gate for every plan and the footgun list for every review. A plan walks only the sections its change touches. A review grades every violation of a numbered section, lettered subsections included, as CRITICAL; a violation of § Conventions is HIGH.

### High-blast-radius paths (human gate)

A CONFIRMED finding in any of these files needs a human decision before the cycle lands; a plan touching one always runs research. The list only widens: when code moves, add the new home and never remove the old entry. Skills and templates cite this list and never copy it.

- `src/math.hpp`, `src/math.cpp`
- `src/mp.hpp`, `src/mp.cpp`
- `src/io.hpp`, `src/io.cpp`
- `src/result.hpp`, `src/result.cpp`
- `src/main.cpp`

A cycle that adds an arithmetic engine, a test driver that prints a verdict, a checkpoint or result format of `mps`, or a table of known answers adds its paths here in the same commit.

### 1. Verdicts are earned (`src/math.*` and every test driver) [human gate]

- `mps` reports a Mersenne number prime only after a complete Lucas–Lehmer test ends in a zero residue, and "probable prime" only after a complete PRP test ends in the prime residue. It never reports either from a partial run.
- It reports a composite only with a factor q, re-checked with exact integer arithmetic (2^p ≡ 1 mod q) before it prints, or with a nonzero final residue from a run whose every error check passed.
- A run whose error checks fail, and which cannot recover from a good checkpoint, reports an error and exits non-zero. It never prints a verdict.
- Every result names the test, the exponent, the transform length and shift where they apply, the 64-bit residue, the error-check counts and the program version.

### 2. Arithmetic is checked, not trusted (`src/mp.*` and every engine) [human gate]

- Every arithmetic engine is tested against the reference multiple-precision arithmetic on the known answers before it may produce a verdict. A new engine, transform length or backend comes in with those differential tests.
- A floating-point engine measures its round-off error and treats an excess as a fault. It retries from the last good checkpoint at a larger transform length and never rounds through the excess.
- Numeric code compiles without fast-math semantics (no reassociation, no flush-to-zero assumptions), because the carry and rounding steps depend on exact IEEE behaviour. The flags that compile numeric code belong to this invariant.
- The same exponent, test, transform length and shift produce the same residue on every run and every engine. Timing never changes a result.

### 3. Known answers are fixed points [human gate]

- Expected values (the Mersenne prime exponents, 64-bit residues, factors) are append-only. Each carries its source: a published result, or the command that recomputes it with an independent method.
- An expected value is never edited to make a test pass. A mismatch is a finding.
- A burn-in run takes its expected answers from these tables, never from the run under test.

### 4. Clean room [human gate]

- No code from Prime95/mprime, Mlucas, gpuowl/PRPLL, CUDALucas, mfaktc/mfakto, GMP/GMP-ECM, or any other Mersenne or big-number program enters this repo. That covers copying, translating, and paraphrasing from their source. The algorithms come from the mathematics and the published papers, cited where they are used. Reading those programs' documentation, release notes and published timings is fine.
- An agent working here never opens those programs' source: no repository file views, commits, pull-request diffs, or source archives. Pull-request pages mix prose with code, so read them only when their prose is needed, and never their file tabs.
- Snippets from Rosetta Code and similar sites stay out. Rosetta Code's content is under the GNU FDL, which is incompatible with the GPL.
- Third-party code enters only with the author's approval and a GPLv3-compatible licence, recorded where it lives. Prime95's licence adds an award-assignment term, so its code can't enter on licence grounds either.
- Author our own code wherever possible. `mps` is GPL-3.0-only. Linking a proprietary vendor runtime (NVIDIA CUDA, AMD ROCm, Apple Metal, Google libtpu) is allowed only under the additional permission (GPLv3 section 7) that `LICENSE-EXCEPTION` grants beside the GPLv3 text in `LICENSE`, and no other proprietary code is linked.
- Contributions come under the same expression as every header, `GPL-3.0-only WITH GPL-3.0-linking-exception`, because outside GPL code can't carry the additional permission.

### 5. Dependency-free core

- The CPU build needs only a C++20 compiler and GNU make. No third-party library links into `bin/mps` for the CPU build.
- A GPU or TPU backend depends only on its own platform's toolchain and runtime, and builds only when asked for. The CPU build never needs a GPU toolchain.
- Test oracles (an independent big-integer library, a script) may use outside tools, live only with the tests, and never link into `bin/mps`.

### Conventions (same-commit rules)

- Version lives only at the release recipe's `version.source`; the release recipe's `gate` mirrors CI.
- An option change moves the parser (`src/cli.*`), the usage and help text (`src/main.hpp`) and the usage in `README.md` together.
- A change to a checkpoint or result format of `mps` bumps that format's version in the same commit.
- The profile's `verify` changes in the same commit as the make target it names.
- No spec R#/P# ids in source code.
- Point to a rule; don't restate it. A rule restated in N places is a same-commit obligation in all N.
- Comments cite the paper or theorem an algorithm comes from (author, title, year), never another program's source.
- A cycle that implements or changes an algorithm updates that algorithm's note in `doc/` (its
  result, what `mps` uses, its provenance) in the same commit.
- Prose and commit voice follow this section; voice is hygiene, not a review finding.

#### Code style

New code reads as if the author wrote it alongside `src/`:

- Every file opens with the SPDX block (`// SPDX-FileCopyrightText: <year> Geoffrey Lentner`, `// SPDX-License-Identifier: …`), a `//` line, the module title, and a `//` line, with optional description lines below. The Makefile carries the same header in `#` comments.
- Headers use `#pragma once`. Includes come in two groups under `// Standard libs` and `// Internal libs`. A system header may carry a trailing comment naming what it is for (`#include <unistd.h>    // gethostname`).
- Indent with tabs. Leave two blank lines between top-level definitions. A namespace body is indented one level.
- Functions and variables are `snake_case`. Classes are `PascalCase`, except exception classes, which are lower `snake_case` like the standard library's (`mps_error`, `argument_error`). Enum types are `PascalCase` with `UPPER_SNAKE` enumerators. Constants are `UPPER_SNAKE`.
- Lookup tables are `static std::map<…>` named `<value>_by_<key>` (`test_by_name`, `color_by_level`), with the type on the line above the name and the initializers in aligned columns.
- Logical operators are `not`, `and` and `or`.
- Write `if (…)`, `for (…)` and `while (…)` with a space before the parenthesis. A single-statement body may drop its braces. Chains are `} else if (…) {`. A long function opens with a blank line after its brace.
- A function's documentation is a `//` block at the top of its body: a title line, a blank `//`, then the description (see `src/mp.cpp`).
- Log through `logging::info(…)` and its siblings with variadic arguments. Multi-line text goes in raw strings with named delimiters (`R"HELP(…)HELP"`). Digit separators are welcome (`10'000'000`).
- Each module is one `.hpp`/`.cpp` pair under `src/`. A backend gets its own directory under `src/`, paired the same way.
- Commit subjects are `[area] What changed`: imperative, capitalized, at most 72 characters, no trailing period, no trailers.
