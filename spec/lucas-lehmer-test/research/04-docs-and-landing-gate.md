# Research 04: the landing gate (R17) and AGENTS.md and the notes (R18)

## 1. R17: the landing gate's command

### What exists

- `make check-engine` (`Makefile:74-76`) is one recipe line: `bin/check`, then `sh tests/cli.sh bin/mps`,
  then `bin/check engine`, each always run, exit 1 if any failed. So its log holds two
  `check: N passed, M failed` summaries (`tests/check.cpp:9-10`, `:204`) and one `cli: N passed, M failed`
  (`tests/cli.sh`, last lines).
- The digest lines are printed by `engine_support::Digest::print` (`tests/engine_support.cpp:369-371`)
  as exactly `   residues <label>: <sha256>`: three spaces, `residues`, a space, the label, `: `, 64 lower-case
  hex digits. Five labels, all from the engine tier (`tests/check_engine.cpp`): `squarings` (:93), `products`
  (:100), `partial chains` (:133), `final chains` (:174), `zero chains` (:188). The default tier builds
  `Digest`s (`tests/check_ibdwt.cpp:372`, `:458`) but never prints them. A digest is printed at the end of its
  check's body, after `expect` calls that don't throw. **A red check can still print a digest, and two builds
  that compute the same wrong residue print equal digests.** The comparison alone is not a pass.
- The previous cycle compared them in `spec/ibdwt-transform-engine/verify/gate.sh:344-349`:
  `grep '^   residues '` on each log, then `cmp -s`. It only refused an empty clang list from phase 2 on (:347).
  It never checked the label set against a known list, so one label missing from both logs would pass. It
  checked green summaries only for clang (:308). Trees were working-tree copies (`common.sh:157-162`, tracked
  plus untracked files, no `lib/` or `bin/`), and it used `/usr/bin/make` refused unless 3.81 (`common.sh:110`).
- Times: `make check-engine` took 464 s on clang (`spec/ibdwt-transform-engine/evidence/ledger.md:78`) and
  459 s alone at P4 (:65). Both were measured with nothing to rebuild. The ledger has no GCC time. A fresh tree
  adds the compile.
- `ci/` holds `test-commit.sh` (POSIX `sh`, SPDX block, a header that is the manual, `mps-test:` prefixed
  status lines) and `targets/`. A local gate script fits beside it.

### Recommendation: `ci/engine-gate.sh`

```
ci/engine-gate.sh [REV]              # default REV = HEAD
ci/engine-gate.sh compare LOG_A LOG_B
```

**Tree: the committed tree, `git archive REV`, not the working tree.** What lands is a commit: under
`squash-local`, it is the squash onto `origin/main`. A commit can be cited by hash in the ledger and checked
later (tree equality). The working-tree copy of the old gate can't be cited, and it picks up untracked files.
`git archive` writes no `lib/` or `bin/`, and the repo has no `.gitattributes`, so no `export-ignore` drops a
file. If `git status --porcelain` is non-empty, the script prints one warning line ("testing REV; the working
tree has uncommitted changes") but does not refuse.

**Run mode, step by step.**

1. `root=$(git rev-parse --show-toplevel)`. `rev=$(git rev-parse --verify "$REV^{commit}")`, so the full
   hash is printed and recorded.
2. `work=$(mktemp -d "${TMPDIR:-/tmp}/mps-engine-gate.XXXXXX")`, with `clang/` and `gcc/` under it. Each gets
   `git -C "$root" archive "$rev" | tar -xf - -C …`. A `trap` removes the trees on exit. Logs are kept, and
   their directory is printed when the run fails. Use `/bin/rm -rf`, as the Makefile does (`Makefile:69`, `:84`).
3. Compilers. `MPS_CLANG` (default `clang++`) must define `__clang__`. `MPS_GCC` must be a real GCC. When it
   is unset, take the first of `g++-16 g++-15 g++-14 g++-13 g++-12 g++` on `PATH` that passes. **Detection,
   portable to Apple clang, Homebrew GCC and Linux GCC:** run `"$cxx" -dM -E -x c++ /dev/null`. Require
   `#define __GNUC__` and no `#define __clang__` (and, to be strict, no `__INTEL_LLVM_COMPILER`). On this
   Mac, `/usr/bin/g++` prints `#define __clang__ 1` and `g++-16` (Homebrew GCC 16.2.0) prints none. Don't use
   `--version` text: Apple's `g++ --version` says "Apple clang", but distribution strings vary. With no GCC,
   exit 2: "engine-gate: no GCC found (g++ is clang); set MPS_GCC". Print the first line of each
   `--version` in the result.
4. Make. Use `${MAKE:-make}`. Unset `MAKEFLAGS MFLAGS MAKELEVEL GNUMAKEFLAGS`, as `ci/test-commit.sh`
   clears its inherited variables (header :33-34). Then run `"$MAKE" CXX="$cxx" check-engine >LOG 2>&1` in
   each tree, **one at a time**, clang first. The Makefile assigns `CXXFLAGS`, `CPPFLAGS` and `IEEE` with `:=`,
   so the environment can't change them, and `CXX` on the command line beats `ifeq ($(origin CXX),default)`
   (`Makefile:4-6`). On macOS, `make` is `/usr/bin/make` 3.81. On Linux it is 4.x. The script adds nothing to
   the Makefile, so the 3.81 rule (`AGENTS.md:29-30`) holds trivially. Record each status. Don't stop after a
   red clang run: running GCC shows whether a failure is shared.
5. Then `compare` on the two logs. The exit status is 1 if either make was red, even if the digests agree.

**Compare: one function, used by both modes, with these guards against a hollow match:**

- Each log has exactly two `^check: [0-9]+ passed, 0 failed$` lines and one `^cli: [0-9]+ passed, 0 failed$`.
  Otherwise: "engine-gate: LOG is not a green make check-engine". This also makes `compare` on a red log fail.
- Every line that begins with `   residues ` must match `^   residues ([a-z][a-z ]*): ([0-9a-f]{64})$`. Any
  other such line fails as malformed.
- No label appears twice in a log.
- The label set equals the script's `LABELS` constant (the five above, one per line) in both logs. A missing
  label gives "engine-gate: LOG prints no digest for 'final chains'", and an unknown one fails too. A later
  cycle that adds a digest edits `LABELS` in the same commit. This is the guard against "no lines at all" and
  against a check that silently stops printing.
- For each label, in `LABELS` order: if the digests differ, print
  `engine-gate: residues <label> differ: clang++ <hexA>, g++-16 <hexB>`. Report every label that differs,
  then exit 1.
- Green: one line per label (`engine-gate: residues <label>: <hex>`), then a last line
  `engine-gate: green on <full hash>: make check-engine under <clang version> and <gcc version>, 5 residue digests equal`.
  In `compare` mode the last line names the two logs instead of the hash.
- Exit statuses: 0 green, 1 a red build or a difference, 2 usage, environment or a missing compiler.

Write it in POSIX `sh` with `grep`, `sed` and `awk` only (no `perl` and no Python). A missing `sha256` tool
isn't an issue, since the digests come from `bin/check`.

**Run time.** Two fresh builds, plus two `make check-engine` runs in series: about 8 minutes each (464 s on
clang with nothing to rebuild, GCC unmeasured), so about 16–20 minutes in all. Running them in parallel would
halve the time but break the "one heavy process" norm and skew both runs. Say so in the header, and have the
plan record the GCC time on its first run.

**Self-test.** `compare` mode lets the phase gate check the script in seconds, on fixture logs it writes
itself:

- equal digests: exit 0;
- one digest flipped: non-zero, and the message names the label and both digests;
- one label dropped from both logs;
- no `residues` lines at all;
- a `1 failed` summary with equal digests;
- a malformed digest line.

Only the landing run does the 16-minute run mode.

**Landing.** Run the gate on the squash commit (or on a branch head whose tree equals it) before the push.
Quote the green last line, with its hash, in `spec/lucas-lehmer-test/evidence/ledger.md`. The pre-push check
then confirms `git rev-parse HASH^{tree}` equals the pushed commit's tree. The risk: `origin/main` moves after
the gate ran. The tree check catches that.

### The profile row (`AGENTS.md` § Factory, after `cluster test`, `:116`)

```
| landing gate | before a change to the engine's sources (`src/ibdwt.*`, `src/fft.*`), its length table and measurement (`src/lengths.*`, `src/lengths.txt`, `src/measure.cpp`) or the numeric build flags (`IEEE` in the `Makefile`) lands: `ci/engine-gate.sh` green on the commit that lands, which runs `make check-engine` under clang and under GCC in fresh copies of it and compares their residue digests; its header is the manual |
```

Keep `verify` byte-for-byte as it is (the 2026-10-22 ruling; the previous cycle's `files.py` already pins
it). Add `ci/engine-gate.sh` to § Map's `ci/` bullet (§ 2 below).

## 2. R18: AGENTS.md

**`tests/cli.sh` reads AGENTS.md** (`tests/cli.sh:592-614`). It requires four exact lines:
`` - `src/result.hpp`, `src/result.cpp` ``, `` - `src/main.cpp` ``, the "A cycle that adds…" paragraph and the
format-version convention. If the build adds a description to either list line, `make check` goes red.
Leave every existing high-blast line untouched. The list only widens (`AGENTS.md:183`).

**High-blast lines, appended after `:201` in the list's style:**

```
- `src/lucas.hpp`, `src/lucas.cpp` (the Lucas–Lehmer test driver: the shift, the error checks, the recovery and the length retry)
```

If the Jacobi symbol lives in `src/mp.*`, it is already covered (`:186`). Jacobi vectors under
`tests/vectors/` are covered by `:190`. If the build puts the Jacobi function in a module of its own (say
`src/jacobi.*`), it gets a line too, and the AGENTS.md paragraph at `:203` requires that line in the same
commit as the file. Optional, not required by R18: `ci/engine-gate.sh`. I recommend against adding it,
because the profile row that names it already lives in a human-gated file.

**§ Map:**

- New bullet after `src/math.*`: ``- `src/lucas.cpp`, `src/lucas.hpp`: the Lucas–Lehmer test on the FP64
  engine (`<entry point>`). It runs from s_0 = 4 at a given shift, at the length the table names. It checks the
  Jacobi symbol at intervals and on the final residue, checks for degenerate states, and confirms a zero
  residue. On a failed check it returns to the last good state, held in memory. It replays a refused squaring,
  and retries at the next larger length when the refusal reproduces. It logs progress, and it fills the counts
  the result reports. Its fault injections are reachable only from the checks.``
- `src/math.*` (`:57-59`): replace "the three tests (stubs); and `run_in_order`, which runs them and stops at
  the first decisive result" with "the three tests: trial factoring and P-1 (stubs), and `test_lucas_lehmer`,
  which runs `src/lucas.*` at a fresh random shift and refuses p = 2 and p past the table; and `run_in_order`,
  which runs them, stops at the first decisive result and ends the run at the first error".
- `src/result.*` (`:60-61`): "the result type, the verdict line, the JSON record (`RECORD_VERSION`) and the
  exit status; a Lucas–Lehmer result carries each length it ran at, the shift and the error-check counts".
- `src/mp.*` (`:47-49`): add "`jacobi`, the Jacobi symbol of a `BigInt` modulo an odd `BigInt` (the
  Lucas–Lehmer test's check; `doc/reference-arithmetic.md`)", or name the module that holds it.
- `src/io.*` (`:46`): stays "format sketch only". The checkpoint file is a non-goal. Change it only if the
  plan touches io.
- `src/ibdwt.*` (`:67-69`): **stale.** "Nothing in `bin/mps` calls it yet" becomes "`bin/mps` calls it through
  the Lucas–Lehmer driver (`src/lucas.*`); `make check` and `make check-engine` check it against `BigInt`."
- `src/reference.*` and `src/known.*` ("nothing in `bin/mps` calls it") stay true as long as the driver never
  loads the tables or calls the reference (§ Invariants 3 forbids taking answers from the run).
- The intro (`:3-4`), "All three are stubs today": **stale.** It becomes "Trial factoring and P-1 are stubs
  today, the Lucas–Lehmer test runs on the FP64 engine, and PRP is planned".
- `tests/` (`:78-94`): add `check_lucas.cpp` (the driver's checks: the known answers at two shifts, the Jacobi
  vectors, and the injections of R6–R10). Add the known-answer script (whatever the plan names it, e.g.
  `tests/ll.sh`, run by `make check` up to its bound and by the long tier above it) and the Jacobi vector file
  that `vectors/generate.py` writes. Replace "`make check` runs both" with "`make check` runs them; `make
  check-long` runs the long tier".
- `ci/` (`:95-98`): add "`ci/engine-gate.sh`: the engine's landing gate (§ Factory). It runs `make
  check-engine` under clang and under a GCC in two fresh copies of a commit and compares their residue
  digests; `compare LOG_A LOG_B` only compares. Its header is the manual."

**§ Build and run (`:13-27`)**, two lines in the block's style:

```
make check-long           # make check's steps, then the long tier: bin/mps -t ll on every Mersenne prime
                          # exponent up to 100,000 and the final residue at 100,069 (minutes; not in make check)
ci/engine-gate.sh         # the engine's landing gate: make check-engine under clang and under GCC in fresh
                          # copies of HEAD, residue digests compared (two engine tiers, about 20 minutes)
```

**Name:** `check-long` is the right name. It names the tier the GOAL names and sits beside `check-engine`.
`check-known` would mislead, since `make check` reads `tests/known/` too. Mirror `check-engine`'s shape and
always run every step. If `make check` gains a step (a known-answer script for R1/R2 up to 10,000), then
`check-engine` (`Makefile:74-76`) and its comment ("make check's two steps") must gain it too, along with the
`make check` line here ("run it, then the smoke test"). Otherwise R17's gate runs a weaker suite than
`verify`. Add `check-long` to `.PHONY` (`Makefile:88`). Change `verify` only if `check`'s target changes,
per the Conventions rule.

**README.md** has no usage section and no mention of the stub (19 lines). Nothing changes there. The profile's
"README.md usage" (`AGENTS.md:118`) refers to text that doesn't exist. Note it and don't fix it here.

## 3. R18: the notes

`python3 -I -B spec/math-notes-and-references/verify/notes.py claims doc/lucas-lehmer.md` passes today (rc 0,
11 spans), as do `prp-and-gerbicz.md` and `reference-arithmetic.md`. It enforces three things
(`notes.py:407-443`):

- each check expression evaluates to True;
- the visible integers of a span equal the expression's literals or values;
- no integer of 100 or more, and no 16-character hex string, stands outside a marker pair.

**Inline code is scanned too** (`visible()`, `:400-404`). So `` `src/math.cpp:88` ``, `RECORD_VERSION 2` and a
Res64 in backticks are fine only below 100. Tokens split at commas and points (`NUMBER`, `:368`): "10,000"
passes the tool but still breaks the prose rule, while "100,069", "20,381,696" and "0.4375" fail it. Available
functions (`claims.py:481-490`) include `jacobi`, `lucas_seq` (p, i ≤ 5000), `ll_residue` (≤ 4500), `pow`,
`mersenne` (≤ 100000), `jacobi_sweep` and `res64`. Nothing reads a code constant or a table of `tests/known/`.

**Stale sentences:**

- `doc/lucas-lehmer.md:112`: the whole paragraph, "Today the test is a stub … through `run_in_order`".
- `:114`: "A reference test sits beside the stub"; "`make check` runs it on the published Mersenne prime
  exponents"; "It is the known answer the planned test will be graded against" (R1/R2 grade against
  `tests/known/`); "It is not wired into `-t`, and `test_lucas_lehmer` is still a stub that decides nothing."
- `:116`: the whole "The planned test will…" paragraph. That includes "and $M_2 = 3$ by inspection", which is
  wrong now that `-t ll 2` is no verdict (R14, Clarification on p = 2). The theorem's remark at `:19` is
  mathematics and stays. It also includes "0 as an error unless the GCD gives a factor that passes the exact
  re-check".
- `:103`: "**0 is an error, or a common factor.** … The run reports a factor only if $g$ passes the exact
  re-check … Otherwise it treats the 0 as an error." This says what the run does, and that is now wrong.
- `:104`, last sentence: comparing with a second shifted run is not built. Add "`mps` does not make that run
  itself (`issues/known-primes-campaign.md`)".
- `:133`: "The planned test will be built from these" becomes "The test is built from these".
- `doc/prp-and-gerbicz.md:118` and `doc/reference-arithmetic.md:75`: "on the published Mersenne prime
  exponents".
- Elsewhere: `doc/README.md:6` stays true, and could add "the degenerate states and the prime confirmation".
  `doc/reference-arithmetic.md`'s README line (`:8`) should name the Jacobi symbol if `src/mp.*` gains it.
  `AGENTS.md:3-4` is covered in § 2.

**What `make check` runs since `known-answer-tables`** (`tests/check_reference.cpp:6-9`, `:53-71`; that cycle's
R5/R6, `spec/known-answer-tables/GOAL.md:73-80`):

- `reference::lucas_lehmer` and `reference::prp3` on every exponent of `tests/known/mersenne-exponents.txt`
  from 3 up to `REFERENCE_UP_TO` (`10'000`, `:56`; today that is 21 exponents, the last 9941);
- both tests on the 28 composite-M_p records per test of `tests/vectors/reference.txt` (p from 11 to 9973),
  against full residues computed with Python's integers.

The final rows of `tests/known/final-residues.txt` are checked against `reference.txt` by `check_known.cpp`,
not by running the reference. **Recommended text** (no numbers, so no markers, and it points at the data per
"point to a rule"):

> `make check` runs it on every exponent of `tests/known/mersenne-exponents.txt` from 3 up to the bound
> `tests/check_reference.cpp` sets, where it must end in 0, and on the prime exponents with $M_p$ composite in
> `tests/vectors/reference.txt`, against the full residues Python's integers computed.

Use the same text with "end in 1" in the PRP note, and "end in 0 and 1" for both in `reference-arithmetic.md`.
Stating "21" or "9941" would need a check marker, and no claims function reaches the table.

**Limits under the marker rule.** The driver's limits are mps's own design constants: the Jacobi interval,
the failures allowed per run, the length retries allowed per run, the progress interval. The previous cycle's
precedent treats such constants as "definitions and format constants": `docs.py:28-34` exempted the sizing
distance 0.35 and the default threshold 7/16. They take no marker. Recommendation:

- State each limit with the constant that holds it, e.g. "at most `lucas::MAX_FAILURES` (3) failed checks a
  run".
- Write values below 100 or as powers of two ($2^{16}$ iterations). notes.py refuses unmarked integers of 100
  or more.
- List them in PLAN § 2 as the note's exempt constants.
- Have the gate check that the note's values equal the header's (§ 4).

Instance numbers (a worked example, a count of known exponents) still need check markers.

**Outline of the rewritten "What mps uses"** (and one new derivation subsection):

0. *New subsection under Proof*: "### Degenerate states and the prime confirmation (the project's own
   derivation)". It covers:
   - x ↦ x² − 2 fixes 2 and −1, sends 1 to −1 and −2 to 2, and sends 0 to −2. A stuck sequence never reaches
     0, and a prime M_p has s_i ∉ {0, ±1, ±2} for i < p − 2 (the "consequence" at `:56` covers 0; the others
     would end in 2 or −1).
   - Which of them the Jacobi check misses, with M ≡ 7 mod 8, J(−1|M) = −1 and J(2|M) = 1:
     - 0 gives J(−2|M) = −1 and passes;
     - 1 gives J(−1|M) = −1 and passes;
     - −2 gives J(−4|M) = −1 and passes;
     - 2 gives J(0|M) = 0 and is caught;
     - −1 gives J(−3|M) = +1 and is caught.

     So the degenerate check is needed for 0, 1 and −2.
   - The confirmation: s_(p−2) ≡ 0 means s_(p−3)² ≡ 2 ≡ (2^((p+1)/2))², and a zero residue means M_p is prime
     (sufficiency), so s_(p−3) ≡ ±2^((p+1)/2).
   - Shifted: r ≡ ±2^((k + (p+1)/2) mod p).
   - A worked check: `lucas_seq(7, 4) == mersenne(7) - 2**4` (111 = 127 − 16). Verified in Python: true for
     9941. For 1237, 1277 and 10303 the confirmation fails, as expected for composites.
1. The entry. `-t ll` calls `test_lucas_lehmer`, which calls the driver. It is odd p only (p = 2: no verdict,
   exit 42). Past P_max it is no verdict, decided before any squaring, and the text points at the length
   table rather than stating P_max. The length is the one the table names.
2. The shift. A fresh k₀ per run, from the source the plan picks. It stores 2^k·s_i, uses `square_subtract`
   at bit (2k+1) mod p, sets k ← 2k mod p, and undoes the shift before any comparison or Res64. The reported
   shift is k₀. The checks pin shifts through the interface. There is no option (`issues/burn-in-mode.md`).
3. The Jacobi schedule. J(r − 2^(k+1) | M_p) via the `jacobi` function, every `<interval>` iterations and on
   the final residue before any verdict. −1 passes, and +1 and 0 fail. Its cost: the algorithm is quadratic,
   so the interval bounds its share. The subquadratic version is deferred to `issues/large-gcd-and-jacobi.md`.
4. The degenerate check: at every good state kept and on the final residue.
5. The prime confirmation: keep the residue before the last squaring.
6. Good states and recovery. A good state is a residue, iteration, shift and length that passed every check,
   held in memory (no file: `issues/checkpoint-and-resume.md`). On a failed check the run returns to it and
   redoes the iterations. Past the limit it ends in an error naming the check, exit 20, and no verdict.
7. Refusals. A refused squaring is replayed once at the same length; if that is accepted, one transient fault
   is counted. If refused again, the run returns to the last good state at the next larger supported length
   that holds p. With no such length, or past the retry limit, it ends in an error naming p, the length and
   the distance, exit 20. Accepted distances past 0.35 are counted and logged, and the largest is reported.
8. The result. The verdict line and JSON fields, `RECORD_VERSION` bumped, exit statuses 0/40/20/42. An error
   in `-t all` ends the run.
9. Progress lines.
10. What checks it: `make check` (bounds pointed at, not stated), `make check-long`, the Jacobi vectors, the
    injections.

**The 0 rewrite (`:103`, per the GOAL's Clarification):**

> **0 is a failed check.** For a prime $M_p$ it cannot occur at $i \le p - 2$. For a composite $M_p$ it can
> be genuine only when some $s_k$ with $k \le i - 2$ shares a prime factor with $M_p$, which the sweep above
> never finds for $p \le 521$. `mps` treats 0 as a failed error check, like $+1$, and takes no factor from it:
> a factor would need $\gcd(r - 2^{k+1}, M_p)$ and its exact re-check, which `issues/large-gcd-and-jacobi.md`
> holds. A genuine 0 would therefore end a run in an error, never in a wrong verdict.

Don't write "never occurs at these exponents". It is proved only up to 521 in the note. I checked in Python
that J(s_(p−2) − 2 | M_p) = −1 for 1237, 1277 and 10303 (and for 9941). The symbol stays at 0 once it reaches
0, so the final value covers every iteration: R2's `make check` rows can't hit a genuine 0, and none of the
four has a degenerate value before the last iteration. 100069 was not checked (about 18 minutes in pure
Python); the long tier will show it.

**The Jacobi write-up** goes in the note of the module that holds it (R18). For `src/mp.*` that is
`doc/reference-arithmetic.md`: a "### The Jacobi symbol" subsection in its algorithms, plus a sentence in
"What mps uses" (`:73`). The binary algorithm follows from the identities in BrentZimmermann2010 § 1 (read,
per `doc/references.md:21`). CrandallPomerance2005 was verified only against its table of contents
(`references.md:84`), so cite it by section (§ 2.3.1), never by an algorithm number nobody has read. Its
check: `jacobi(…)` worked examples in markers, e.g. a 0 case `jacobi(14, mersenne(9)) == 0` ($M_9 = 511 = 7 \cdot 73$ shares 7 with 14) and a −1 case `jacobi(12, mersenne(7)) == -1` (that is J(12|M) = −1).

## 4. What a gate can check of R18 (modeled on `spec/ibdwt-transform-engine/verify/files.py:259-283`, `docs.py:87-130`, `p4.sh:33-46`)

- **High-blast list:** every base bullet survives verbatim, and one bullet names `` `src/lucas.hpp` `` and
  `` `src/lucas.cpp` `` (plus any new Jacobi module). `make check` keeps cli.sh's four exact lines.
- **Same commit:** for each new path, the first commit on the branch that adds it (`git log --reverse
  --diff-filter=A origin/main..HEAD`) adds a `+- ` line naming it in AGENTS.md and touches
  `doc/lucas-lehmer.md`. The commit that adds `check-long:` to the Makefile adds `+make check-long` to
  § Build and run (`p4.sh:41-43` pattern).
- **§ Map** names `src/lucas.cpp`, `src/lucas.hpp`, `tests/check_lucas.cpp`, the known-answer script,
  `ci/engine-gate.sh` and `make check-long`. **§ Build and run** names `make check-long` and
  `ci/engine-gate.sh`. **Profile:** a `| landing gate |` row containing `src/ibdwt.*`, `src/fft.*`,
  `src/lengths.*`, `src/lengths.txt`, `src/measure.cpp`, `IEEE`, `make check-engine` and `ci/engine-gate.sh`.
  The `verify` row equals the base's.
- **Stale phrases gone:**
  - AGENTS.md: "the three tests (stubs)", "All three are stubs today", "Nothing in `bin/mps` calls it yet;
    `make check` and `make".
  - `doc/lucas-lehmer.md`: "Today the test is a stub", "beside the stub", "still a stub", "The planned test",
    "unless the GCD gives a factor", "reports a factor only if".
  - All three notes: "on the published Mersenne prime exponents".
- **Required in "What mps uses"** (MUST list):
  - names: `square_subtract`, `jacobi`, `roundoff_error`, "shift", "degenerate", `2^{(p+1)/2}`, "0.35";
  - links: `issues/checkpoint-and-resume.md`, `issues/large-gcd-and-jacobi.md`;
  - files and targets: `make check-long`, `src/lucas.cpp`.
  - Each limit constant parsed from `src/lucas.hpp` (`NAME = value`) appears with its value.
  - The three rewritten sentences each name `tests/known/mersenne-exponents.txt` and
    `tests/vectors/reference.txt`.
- **Numbers:** `notes.py claims` is green on the three notes. Added lines are scanned for unmarked numbers and
  number words with an EXEMPT list (the limits, 0.35, the exit statuses, `2^{…}`), as in `docs.py:59-75`.
- **Citations:** the Jacobi subsection links `references.md#brentzimmermann2010`. Each title cited in
  `src/mp.cpp` and `src/lucas.cpp` comments is in `doc/references.md` (`docs.py:149-153` pattern). No spec ids
  in source.
- **R17:** the `compare`-mode self-test on fixtures (§ 1). The ledger quotes `engine-gate: green on <hash>`,
  and that commit's tree equals the pushed tree.

## Risks

1. Editing an existing high-blast or convention line in AGENTS.md turns `make check` red through `tests/cli.sh`.
2. A `make check` that gains a step while `check-engine` doesn't makes the landing gate weaker than `verify`.
3. A `g++` that is clang on macOS gives a hollow "two compiler" pass unless the script detects `__clang__`.
4. Matching digests from a red run look like a pass unless the script requires the green summaries.
5. The GCC engine-tier time is unmeasured, so the 16–20 minute estimate is a guess until the first run.
6. A limit stated in the note drifts from `src/lucas.hpp` unless the gate ties the two.
7. A genuine Jacobi 0 or degenerate value at 100069 is unverified. If one exists, the long tier ends in an
   error (exit 20), not in R2's verdict.
