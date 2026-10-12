# Research 02: the test runner and the license footing

Topic A was tested on a throwaway prototype (scratchpad `research02/repo/`):

- `make -B check` passes with Apple clang 21 and with g++-16 under make 3.81.
- `cli.sh` passes under sh, dash and ksh.
- Each runner and script guard below was broken on purpose, and the run failed every time.
- With the header prerequisites, a version bump rebuilds `cli.o`, and both version checks still pass
  without being edited.

## A. Test runner

**Name.** In this codebase "test" already means a primality test (`test_lucas_lehmer`,
`cli::TestCase`, `-t`), so the unit is a *check*.

**Files.**

- `tests/check.hpp` and `tests/check.cpp`: the runner, including `main`.
- `tests/check_<module>.cpp`: the checks for one module.
- `tests/cli.sh`: the smoke script.

The build writes `lib/check*.o` and `bin/check`, so R6's `make clean` covers them. `bin/check` links
`$(OBJECTS)`, which is every module except `main`, so the checks call the real `prime_sieve`.

**Registration.** Checks are `static` functions. Each file's table registers itself:

```cpp
static check::Suite
check_by_name {
	{"sieve matches trial division",     sieve_matches_trial_division},
	{"sieve marks squares at the bound", sieve_marks_squares_at_the_bound},
};
```

- `Suite(initializer_list, std::source_location = current())` appends to a vector held in a
  function-local static, so static-initialization order doesn't matter.
- `check::expect(cond, what)` records a result. `require` also throws `check::failure` to end the
  check. `expect_equal` prints both values.
- The defaulted `std::source_location` gives `file:line` without macros.

**Runner.** It prints `check <label>` with `std::endl` before each body, and catches
`check::failure`, `std::exception` and `...`. A check that runs no expectation fails. Each failure
prints ` FAIL <label>` at once and again in the summary. The runner exits 1 on any failure or on an
empty registry. It reports through plain iostreams rather than the logger, which is itself under test
(R9).

**Makefile.** This works under make 3.81. `HEADERS` is `$(wildcard src/*.hpp)`, and the test
sources are `$(sort $(wildcard tests/*.cpp))`:

```make
lib/%.o: src/%.cpp $(HEADERS) Makefile | lib
lib/%.o: tests/%.cpp tests/check.hpp $(HEADERS) Makefile | lib
	$(CXX) $(<) -c -o $(@) $(CXXFLAGS) -Werror=unused-function

check: $(EXE) $(CHECK)
	@status=0; \
	$(CHECK) || status=1; \
	sh tests/cli.sh $(EXE) || status=1; \
	exit $$status
```

Both halves always run. If either fails, `make` exits 2. Keep `$(EXE)` as the first target. The
checks build with the same `CXXFLAGS` as `bin/mps` (R5).

**Smoke script.** It uses shell built-ins only, with no `sed`, `grep` or `mktemp`, so it meets R4
under a strict reading:

```sh
run() { out=$("$mps" "$@" 2>/dev/null); status=$?; }    # never a pipe
run -p 30
expect "-p 30 exits 0" [ "$status" -eq 0 ]
case $out in 2"$nl"*) out=${out#2"$nl"} ;; esac          # accept output with or without the 2
expect "-p 30 prints the odd primes below 30" [ "$out" = "$odd_primes" ]
```

`expect NAME CMD…` passes only when `CMD` succeeds. The script reads the expected version from
`src/main.hpp` with `read` and `case`, because the version lives only at `version.source`. It exits 1
unless `failed` is 0 and `passed` equals `planned`.

**Ways the gate could pass while broken**

| Threat | Guard |
|---|---|
| Zero checks | An empty registry exits 1. Objects come from a wildcard and are linked directly, never from an archive, which drops unreferenced members. |
| Check never registered | `-Werror=unused-function`, which errors in both compilers. A separate `static std::map` would raise no warning. |
| Duplicate name | The runner fails it. A `std::map` would silently drop one. |
| Hollow check | Reported as "no expectations ran". |
| Crash | The name is flushed before the body runs. The shell reports the signal and `make` exits 2. |
| `exit(0)` mid-run | An `atexit` guard calls `_Exit(1)`. |
| Stale objects | Measured: an `APP_VERSION` bump left `lib/cli.o` stale, and `--version` still printed 0.1.0. The Makefile now has header and Makefile prerequisites. make 3.81 compares whole seconds, so proofs, and any change of `CXX`, use `make -B check`. |
| A pipe, `set -e` or `local` hides a status | Capture output before filtering it. No `set -e`, which is off inside `if` and `&&`. No `local`, which isn't POSIX and masks the status. |
| A shell check is skipped | `set -u` stops a misspelled variable. `planned` catches a misspelled `expect`. |
| The caller's environment leaks in | Set `LC_ALL=C` and `NO_COLOR=1`, unset `FORCE_COLOR` and `TASK_ID`, and always redirect stderr. `--help` colors stdout when stderr is a TTY. |

**Proof for the review.** Under `make -B check`, each of these breaks must exit non-zero and name
the check: a `check::expect(false, …)`, a deleted table row, an empty body, a null dereference, and a
call to `std::exit(0)`. `sh tests/cli.sh` must also exit non-zero when given `/usr/bin/true` or
`/usr/bin/false` as the binary. An optional permanent canary would make `CHECK_CANARY=1 bin/check`
append a failing check, so the run must exit 1.

**Known-wrong today; don't lock it in** (all verified on this tree):

- **Sieve.** `prime_sieve(n)[1]` is true. For n < 3 the sieve writes `is_prime[2]` out of range,
  and `mps -p 0` exits 30 with `bad_alloc`. Never call it with n < 3. No issue covers either bug yet,
so record both in `issues/`.
- **Primes.** `-p N` omits 2.
- **Exit statuses.** `mps abc` exits 30, where 1 is due. `mps 31` exits 0 with no verdict. Bare `mps`
  exits 0.
- **Options.** `-t tf`, `-t p1`, `-t=…`, `-f=…` and `--file` misparse. Use only `ll` for `-t`.
  Never pass `-f`, because it creates the file.
- **Logger.** Its state is `static`, one copy per translation unit, so `-D` and `-d` reach only
  `main.cpp`. R9 freezes this, so the rename keeps `static`. `NO_COLOR` never resets within a
  process, so colour cases belong in `cli.sh`.

**Safe to lock in:**

- `--version` and `-v` print `mps v<APP_VERSION>` and exit 0.
- `--help` exits 0, and its output starts with `Usage:`.
- An unknown option, an unknown `-t` name, two PVALs, or a 10-digit PVAL exits 1.
- `-p 30` prints the odd primes 3 to 29.
- `parse_args` accepts `-t ll 31`.

## B. License footing

**FSF template (GPLv3).** Verbatim from <https://www.gnu.org/licenses/gpl-faq.html#GPLIncompatibleLibs>.
The FAQ's instructions are to replace the bracketed text and, "If not everybody can distribute
source for the libraries you intend to link with", to remove the braced text:

> Copyright (C) [years] [name of copyright holder]
>
> This program is free software; you can redistribute it and/or modify it under the terms of the
> GNU General Public License as published by the Free Software Foundation; either version 3 of the
> License, or (at your option) any later version.
>
> This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
> even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
> General Public License for more details.
>
> You should have received a copy of the GNU General Public License along with this program; if
> not, see <https://www.gnu.org/licenses>.
>
> Additional permission under GNU GPL version 3 section 7
>
> If you modify this Program, or any covered work, by linking or combining it with [name of
> library] (or a modified version of that library), containing parts covered by the terms of [name
> of library's license], the licensors of this Program grant you additional permission to convey
> the resulting work. {Corresponding Source for a non-source form of such a combination shall
> include the source code for the parts of [name of library] used as well as that of the covered
> work.}

SPDX License List 3.29.0 (2026-09-16) lists this permission, without the braced sentence, as
**`GPL-3.0-linking-exception`**. The library name and the license name are template variables
(`.+`).

**`LICENSE-EXCEPTION`.** Fill only the brackets and add nothing else, so the file still matches the
SPDX template:

> Additional permission under GNU GPL version 3 section 7
>
> If you modify this Program, or any covered work, by linking or combining it with the NVIDIA CUDA
> Toolkit runtime and libraries, the CUDA libraries of the NVIDIA driver, the AMD ROCm libraries,
> Apple's Metal, Foundation and QuartzCore frameworks, or Google's libtpu TPU runtime and its PJRT
> plugin (or a modified version of that library), containing parts covered by the terms of that
> library's own license, the licensors of this Program grant you additional permission to convey the
> resulting work.

- **Braced sentence:** dropped, because only the vendors can ship that source.
- **Licenses:** named generically, because vendors rename them. On PyPI, libtpu's license is "Google
  Cloud Platform Terms of Service".
- **No exception needed:** metal-cpp (`apple/metal-cpp`) and the OpenXLA PJRT C API header are
  Apache-2.0, which is compatible with GPLv3. They still enter only under § Invariants 4.

**README.**

```
License
-------

Copyright (C) 2024-2026 Geoffrey Lentner

MPS is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License, version 3 only, as published by the Free
Software Foundation (LICENSE). It is distributed WITHOUT ANY WARRANTY.

As an additional permission under section 7 of the GPL (LICENSE-EXCEPTION), you
may convey MPS linked with the NVIDIA CUDA Toolkit and driver CUDA libraries, the
AMD ROCm libraries, Apple's Metal, Foundation and QuartzCore frameworks, and
Google's libtpu. The CPU build links none of them.
```

Drop the template's "or (at your option) any later version", or the README contradicts the GOAL's
`-only`.

**SPDX.** SPDX 2.3 allows `license-id WITH license-exception-id` only with an exception from the
SPDX list, so `WITH LicenseRef-…` is invalid. SPDX 3.0.1 adds `AdditionRef-…` for custom exceptions,
but 2.x tools reject it. REUSE declined custom exceptions in 2021; whether it supports them now is
unverified. **Recommended header:**

```
// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
```

- **Why:** this header is valid in SPDX 2.3 and 3.0 and states the permission in every header.
- **Other files:** the Makefile and `cli.sh` carry it in `#` comments, `cli.sh` after its shebang.
- **Years:** existing files keep 2024; new files take 2026.
- **R7:** the foreman should record that R7's identifier is the `GPL-3.0-only` inside this
  expression.
- **Fallback:** if counsel rewords the permission beyond the template, use `GPL-3.0-only` plus a
  comment line pointing to `LICENSE-EXCEPTION`.

**Caveats from the FAQ.**

- **Who can grant it:** only copyright holders can, "assuming your employer or school does not
  claim the copyright". Put that to counsel.
- **Contributors:** they needn't grant the permission, and outside GPL code can't carry it, so
  § Invariants 4 should require contributions under the same expression.
- **Recipients:** section 7 lets them remove the permission.
