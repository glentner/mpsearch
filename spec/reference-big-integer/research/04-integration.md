# Research 04 — How the change fits the repo

Read at `f0a5158` (base `c9b82ff`). Read-only; nothing built in the worktree.

## 1. Where the reference tests live

- `src/math.hpp:24-33` is the test entry-point surface (`TestFunction` returns `Result`), which is coupled core
  (AGENTS.md:75). Putting residue-returning functions there widens coupled core and invites a `test_`
  name that reads like a verdict test. Rejected.
- `tests/` can't serve the later cycles: `issues/lucas-lehmer-test.md:48` runs "first on the reference
  big integer", and `issues/known-answer-tables.md:57` takes "computed rows" from it. Both need it
  callable from `src/`. AGENTS.md:46-48 also reserves `tests/` for checks.
- Free functions in `src/mp.*` would work, but they mix theorem-domain logic (R5's odd p ≥ 3) into the type.
- **Recommend a new pair, `src/reference.hpp`/`.cpp`.** It follows the code style "one .hpp/.cpp pair per
  module" (AGENTS.md:211), and its checks go in `tests/check_reference.cpp` per the `check_<module>.cpp`
  rule (AGENTS.md:47). Add `reference` to `MODULES` (Makefile:17). That also links it into `bin/mps`
  as dead code, with no new library and no behaviour change (R9). Use `namespace reference` and keep
  the exponent type of math.hpp: `BigInt reference::lucas_lehmer(long unsigned p)` (s_{p−2} mod M_p)
  and `BigInt reference::prp3(long unsigned p)` (type-1).
- **AGENTS.md edits, same commit:**
  - § Map line 38: replace "(a stub)" with what `BigInt` now provides, including "`hex64`, the Res64
    text the result printer uses".
  - Add a Map bullet: "`src/reference.cpp`, `src/reference.hpp`: the reference Lucas–Lehmer and base-3
    PRP tests on `BigInt`, run by `make check`, not by `-t`".
  - `tests/` bullet (46-51): add "`vectors/`: the expected values Python computed, each file with the
    command that reproduces it, and the Mersenne prime exponents with their source. `bin/check` reads
    them relative to the repository root."
  - High-blast-radius list (142-146), append: `src/reference.hpp`, `src/reference.cpp`; `tests/vectors/`
    (every file in it), naming the prime-list file explicitly, since the known-answer cycle will take it
    over and the list "only widens" (AGENTS.md:140).

## 2. One Res64 form

- `hex64` is `static` at `src/result.cpp:98-109`, called at :264 and :300. `result.cpp` includes
  `main.hpp`, `log.hpp`, `result.hpp` (:21-23), not `mp.hpp`. `mp.hpp` is included by io.hpp:19,
  math.hpp:16, main.cpp:18 (and math.cpp).
- **Recommend moving it to `mp.*`.** Declare `std::string hex64(std::uint64_t)` in `mp.hpp` and delete
  the static. Then `result.cpp` adds `#include "mp.hpp"`; its call sites are unchanged. The direction is
  arithmetic ← printer, the right way round. Declaring it in `result.hpp` instead would make `mp` depend
  on the verdict module. Keeping two copies is a same-commit obligation in two places (§ Conventions).
  The static has to go in the same commit anyway, since a `static` definition after an extern
  declaration won't compile. `BigInt` also gives a numeric `uint64_t res64(p)`, which feeds
  `Result::composite_by_residue(…, std::uint64_t)` (result.hpp:61) directly.
- **Guards that already pin the text** (tests/check_result.cpp):
  - "each outcome has its line" (:394-403) and "each outcome has its record" (:406-416), via
    `each_outcome` :342-344 (`0123456789ABCDEF`);
  - "residues are sixteen uppercase digits" (:428-437: `0x1` → `0000000000000001`, uppercase
    `ABCDEF0123456789`);
  - "the printer shows each outcome" (:689-);
  - "every record is valid JSON" (:440-, structural only).
- Neither 0 nor 2^64−1 is pinned. `check_mp.cpp` should add `hex64(0)` = `0000000000000000` (R4) and
  `FFFFFFFFFFFFFFFF`.

## 3. Errors (R5)

- **What exists:**
  - `mps_error : std::exception` (exc.hpp:15-23; its title line wrongly says "Command-line Interface");
  - `cli::show_info` and `cli::argument_error : mps_error`, owned by their module header (cli.hpp:25-35);
  - `std::logic_error("No test to run")` at math.cpp:116;
  - nothing in result or io.

  main.cpp:81-95 catches show_info, argument_error, `std::runtime_error`, then `std::exception`.
- **Recommend one module-owned type, `class mp_error: public mps_error`, in `mp.hpp`**, following the cli
  precedent and naming. `mp.hpp` includes `exc.hpp`. Throw it for:
  - a negative difference;
  - a zero modulus (in mod and powmod);
  - a bad digit, an empty string or an unknown base in a parse;
  - p outside odd p ≥ 3 in `reference::*`, with p ≥ 3 for folding/Res64 as the plan's domain.

  The standard types (`domain_error`, `invalid_argument`, `underflow_error`; underflow is a floating-point
  notion) would scatter the catch and fit poorly. Messages should name the operation.
- **Check helper:** follow the try/catch idiom of `check_cli.cpp:43-56` and `:60-76`, as a file-local
  template in `check_mp.cpp`, `expect_mp_error(call, what, location)`:
  - pass only on a caught `mp_error`;
  - a normal return records "returned a value", which is R5's "SHALL NOT return";
  - any other `std::exception` records "threw <what> instead".

  `check::record` (check.hpp:64) is public. Uninstantiated templates don't trip `-Werror=unused-function`
  (Makefile:36).

## 4. Runner, vector files, build

- **Counting:** `record` counts every expectation and prints one line per failure (check.cpp:58-74). A
  check fails on any failure, on an uncaught exception, or on zero expectations (:116-138). The summary
  counts checks, not expectations (:186). `expect_equal` prints both operands with `<<` (check.hpp:76-91).
  On a 2^20-bit `BigInt` that is about 256 KB per line.
- **Flood cap:** never `expect_equal` a large `BigInt`. Loop over the vectors and record the first 3
  mismatches with their file:line, bit lengths and Res64. Then make one
  `expect(mismatches == 0, "all N vectors of <file> agree")`. The runner stays unchanged.
- **Working directory:** `check:` runs `$(CHECK)` from make's directory (Makefile:44-45). The cluster job
  runs `cd "$run/src" && make … check` (ci/test-commit.sh:444-446). The gates use `cd "$d" && make -B check`
  (logger common.sh:92). All three run from the repo root, so open `tests/vectors/<f>` relative to it.
- **Missing files must fail:** `check::require(file.is_open(), "tests/vectors/<f> opens (run from the
  repository root)")`. Each file should declare its record count in the header, and the check should
  require the parsed count to equal it and be > 0. That catches a truncated or emptied file, which
  "no expectations ran" alone wouldn't.
- **Build:**
  - `CHECKS` is `$(wildcard tests/*.cpp)` (Makefile:20). It doesn't descend, so `tests/vectors/*.py|*.txt`
    are never compiled.
  - The `tests` directory prerequisite (:27) relinks only when `tests/` itself gains an entry (once, for
    `vectors/`). Edits inside `vectors/` don't relink, which is correct for runtime data.
  - New check files `tests/check_mp.cpp` and `tests/check_reference.cpp` sort before `log_peer.cpp`,
    which stays last.
  - `.gitignore` is only `bin/*`, `lib/*`, so the generator must run as `python3 -I -B` (no `__pycache__`).
  - `tests/cli.sh` needs nothing: it only drives `bin/mps` (cli.sh:7-28).
- **Warnings:** no `-pedantic` (Makefile:7), so `unsigned __int128` is warning-free. The gates fail on any
  `warning:` (logger common.sh:95).

## 5. R12: doc note and claim markers

- **Recommend a new `doc/reference-arithmetic.md`** with exactly five `##` sections, in order: The result /
  Proof / What mps uses / Sources / Provenance (math-notes PLAN § 2, lines 55-57). `###` subsections
  cover folding, schoolbook and Karatsuba (product and square), general reduction, and Res64.
- **Index:** add one line to `doc/README.md`.
- **Edits to existing notes:**
  - `lucas-lehmer.md:110-114`: name `reference::lucas_lehmer` while keeping "stub" and "decides
    nothing" true of `test_lucas_lehmer`.
  - `prp-and-gerbicz.md:114-118`: keep "does not exist yet" true of the `-t` test.
  - That note says a run "ends in 9" (:116). If type-1 is chosen, add that the reference returns
    3^(M_p−1), 1 for a probable prime (R7).
- **Citations:** the bibliography already holds BrentZimmermannMCA2010 and CrandallPomerance2005 (both
  treat Karatsuba and Mersenne reduction). A Karatsuba–Ofman entry would be new. Citation form is
  `[Key, locator](references.md#key)`.
- **Command:** `python3 -I -B spec/math-notes-and-references/verify/notes.py claims doc/<note>.md`
  (notes.py:812-823). Exit 0 on today's notes: lucas-lehmer 8.6 s, prp-and-gerbicz 5.3 s,
  mersenne-numbers 0.09 s.
- **Evaluator limits:**
  - `FUNCS` (claims.py:481-489) has `pow` (builtin, 3-arg), `mersenne` (p ≤ 100000), `res64` (n mod 2^64),
    `ll_residue` (odd prime p ≤ 4500), `lucas_seq` (≤ 5000), `ll_exponents` (B ∈ [2500, 4500]), `prp3`
    (bool, p ≤ 30000), `prp3_known` (B ≥ 11213), `primes`, `isprime`, `gcd`.
  - Operators are only `+ - * // % **`, so there's no `&` or `>>` (:495-498). Folding is written
    `x % 2**p + x // 2**p`.
  - `**` is capped at bit_length·exponent ≤ 4·10^6 (:560-562).
- **Token rules:** any number ≥ 100, and any 16-character hex token, outside a span fails (notes.py:381-385,
  :441-443). So "n^1.585" is unwritable (585 can't be checked), and the exponent is written
  `\log_2 3`. A Res64 like `00000000000006C8` is fine inside a check that computes `res64(ll_residue(11))`
  = 1736.
- **Coverage:** everything R12 needs is expressible. LL facts above 4500 (e.g. B = 10000) can't be
  checked, so cite OEIS ("the data section") for them or leave them out of the note. claims.py stays
  untouched.

## 6. Downstream contracts

- **known-answer-tables:**
  - R3 wants one exponent table that "every test reads from" (:70-71), under `test/` (sic; GOAL fixes
    `tests/`), "loaded by one C++ module" (:50).
  - So keep the prime list a plain-text file with a source line, at its own path
    (e.g. `tests/vectors/mersenne-exponents.txt`), and read it by one helper.
  - Hand off at the ledger: append to that seed that the list exists here, with its path, and that the
    cycle may move it, keeping the old path on the list.
- **ibdwt-transform-engine:**
  - It is graded "at every compared iteration" against the reference residue (:55-56).
  - Expose per-iteration access, e.g. `reference::lucas_lehmer(p, iterations)`, or let it chain
    `BigInt` squaring and folding itself.
  - The engine contract returns "the canonical residue and its Res64" (:44). For that, give read-only
    limb access (`limbs()`), construction from limbs, and `bit_length()`, so the balanced digit form can
    be built outside `mp.*` without reopening the human gate.
  - Ledger: add a note to `issues/ibdwt-transform-engine.md` and its ROADMAP entry (ROADMAP:188-197)
    that the balanced digit form and its round-trip with `BigInt` are that cycle's (GOAL Non-goals).
- **This cycle's own entry:** ROADMAP:118-127 and the seed's R4 (:56-57) still promise "the transform
  digit form". The landing retires both, as `c9b82ff` did for math-notes.
- **lucas-lehmer-test:** M1277 → `5613A480590E78BA` (:68-69) is reachable by the reference, but that's
  the known-answer cycle's to table.
- **prp-gerbicz-test:** reports type-1, Res64 `0000000000000001` for a prime (:47-48). The type-1 choice
  makes these line up directly.

## 7. The pushing phase's machinery

- **Copy verbatim from `spec/logger-state-per-translation-unit/verify/`:**
  - `cluster.sh`'s `result_check` (:16-118);
  - `scan.py`, `scan_selftest`, `redact`, `cycle_files`, `scan_cycle`, `ledger` (:297-316),
    `code_committed` (:320-324);
  - `redact.sh`;
  - p3.sh's evidence block (:30-102).
- **Adapt:**
  - `slug`, `BASE=c9b82ff` (common.sh:10,13);
  - the plan grep `^\[fix\] Plan` → `^\[feature\] Plan reference-big-integer`, following
    verdicts/common.sh:38;
  - `origin/fix/$slug` → `origin/feature/$slug` (p3.sh:100);
  - `FROZEN` (common.sh:130) and the footprint, which list `src/mp.*`, Makefile and `tests/check.*` as
    untouchable. Rewrite them to this cycle's allowed set;
  - drop the logger-specific `drive`/`detailed`/`R6_RUNS`. Keep `same_as_base` for R9 against
    `c9b82ff`'s `bin/mps`.
- **Mutants:** `mutant()` (common.sh:427-466) has no time limit, and no harness in `spec/*/verify` has one.
  A fold mutant that never terminates hangs the gate. Wrap the `make -B check` in a timeout.
  `/opt/homebrew/bin/timeout` exists here but not on stock macOS, so add `need timeout`, or use perl
  `alarm`.
- **Scan:** it reads every cycle file and `git log -p` (cluster.sh:271-295), so multi-MB vectors are
  scanned; the cost is fine. Word matches are whole-token, so hex can't collide unless an entire token
  equals a site word.
- **R10:** the check step's seconds are whole seconds from `date +%s` (test-commit.sh:247-251), and
  include compiling `lib/check/*.o`, since the build step makes only `bin/mps`. So result.json can't show
  R10 ("nothing to rebuild"). Record it locally (`c9b82ff` vs head, second `make check`, best of N).
  Keep the cluster seconds as information only.

## 8. Lessons from the last three reviews

- **math-notes REVIEW:125:** P2's gate had to be retuned to `p4.sh` "because `p2.sh` checks P2's exact
  file set". Gate each phase on the allowed set and its own files present, cumulatively, so a later
  phase landing doesn't break it.
- **verdicts REVIEW G1 (:170-178):** the checks pinned `check_exponent_by`, not what `bin/mps` runs.
  Here, check the public product and square, not only `schoolbook`/`karatsuba` separately. Check Res64
  through `hex64` as `result.cpp` calls it.
- **logger REVIEW F1 (:50-58):** an unpinned edge survived a mutant. Plan mutants for:
  - carry and borrow across limbs;
  - the Karatsuba threshold ±1;
  - shifts by 0 and by 64k;
  - the fold's final x = M_p → 0;
  - the last limb's normalization.
- **Gate drift:** every cycle froze `verify/` at the plan commit (logger common.sh:47-52). A gate that is
  wrong can only be fixed by a remediation gate.

## Recommendations

- Put the reference tests in a new module `src/reference.*` (`namespace reference`), added to `MODULES`, with `tests/check_reference.cpp`.
- Move `hex64` into `mp.*`, and have `result.cpp` include `mp.hpp`. Five existing checks guard the text. Add the 0 and 2^64−1 cases.
- Add one `mp_error : mps_error` in `mp.hpp`, and assert it with a file-local `expect_mp_error` helper.
- Read vectors from the repo root with `require` on the open and on a declared record count. Cap printed mismatches at 3, never printing a large `BigInt`.
- AGENTS.md: update the `mp` Map line, add a `reference` line and the `tests/vectors/` description. Add `src/reference.*` and `tests/vectors/` (the prime-list file by name) to the high-blast-radius list.
- Write a new `doc/reference-arithmetic.md` (five sections) and a README line. Update the "What mps uses" sections of the LL and PRP notes, and the PRP note's type-1 statement. claims.py suffices if folding is written with `%` and `//`, there are no decimal exponents, and LL facts above 4500 are cited.
- Downstream: give per-iteration LL access, limb access and construction, and `bit_length`. Ledger the digit form to the IBDWT cycle and the prime list to known-answer-tables. Retire this cycle's ROADMAP entry.
- Pushing phase: copy logger's `cluster.sh`/`redact.sh`/p3 evidence block. Adapt slug, BASE `c9b82ff`, `[feature] Plan`, `origin/feature/`, the footprint. Add a mutant timeout.
- Measure R10 locally. Cluster check seconds include the compile and are whole seconds.
- Write gates on allowed sets, cumulatively, never exact per-phase file sets.
