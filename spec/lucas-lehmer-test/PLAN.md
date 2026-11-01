# PLAN — The Lucas–Lehmer test on the engine, with a shift and its error checks

> **Status:** Reviewed (adversarial design review folded in, § 4.1; dry-run, § 6) · **Last updated:** 2026-11-01
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). Backing detail is in
> [`research/`](research/) ([`00-digest.md`](research/00-digest.md)). Every design element traces to
> a GOAL R-ID.

## 1. Summary

A driver module, `src/lucas.*`, runs the Lucas–Lehmer test on the FP64 engine. It holds 2^k·s_i
from a random shift. Every ⌈(p − 2)/32⌉ iterations, and at the end, it checks the residue:
degenerate states first, then the Jacobi symbol J(s − 2 | M_p), then (on a zero final residue)
s_(p−3) = ±2^((p+1)/2). A failed check sends the run back to its last good state, held in memory.
A refused squaring is replayed once, then retried at the next larger length that holds p. Limits
end the run in an error. Results gain a `RunReport` that both views print, and the record's version
becomes 2. The Jacobi symbol is a binary algorithm on `BigInt` (`src/mp.*`), checked against
committed vectors of Python's integers. `run_in_order` stops at an error. `tests/ll.sh` drives
`bin/mps` on the known answers in `make check` and, through `make check-long`, in the long tier.
`ci/engine-gate.sh` is the engine's landing gate the profile names.

The appetite is big; the change touches high-blast-radius paths and two of its deliverables are
gates (R17's command, R16's injection-only checks), so the plan is research-backed, prototyped,
and its gates were attacked by a separate reviewer (§ 4) and dry-run phase by phase (§ 6).

## 2. Design

### 2.1 Files

| Path | What changes | Phase |
|---|---|---|
| `src/mp.hpp`, `src/mp.cpp` | `int jacobi(const BigInt& a, const BigInt& n)`: Shallit and Sorenson's binary algorithm, in place on two limb vectors; `mp_error` for a zero or even modulus | P1 |
| `src/math.cpp` | `run_in_order` stops after an error (R15); `test_lucas_lehmer` runs `lucas::test(p)` | P1, P2 |
| `tests/vectors/generate.py`, `tests/vectors/jacobi.txt`, `tests/vector_reader.*` | the `jacobi` family, field kind `J` | P1 |
| `tests/check_mp.cpp`, `tests/check_result.cpp`, `tests/check_math.cpp` | the vectors' check; the stop rule's stand-ins; the result's views (P2) | P1, P2 |
| `doc/reference-arithmetic.md`, `doc/references.md`, `doc/README.md` | the Jacobi symbol written up and cited (`ShallitSorenson1993`) | P1 |
| `src/lucas.hpp`, `src/lucas.cpp` | the driver (§ 2.3) | P2 |
| `src/result.hpp`, `src/result.cpp`, `src/math.hpp` | `RunReport`, its views, `RECORD_VERSION` 2, an error's record | P2 |
| `tests/check_lucas.cpp`, `tests/ll.sh`, `tests/cli.sh`, `tests/engine_support.*` | the driver's checks; the known-answer runs through `bin/mps`; cli.sh's changed expectations | P2 |
| `Makefile` | `MODULES += lucas`; `check` and `check-engine` run `tests/ll.sh` (P2); `check-long` (P3) | P2, P3 |
| `AGENTS.md` | high-blast-radius line and § Map for `src/lucas.*` (P2); § Build and run and § Map for `check-long` and `ci/engine-gate.sh`, the profile's `landing gate` row (P3) | P2, P3 |
| `doc/lucas-lehmer.md` | "What mps uses" as built (P2); `doc/prp-and-gerbicz.md` and the other two sentences of R18 (P3) | P2, P3 |
| `ci/engine-gate.sh` | the landing gate (§ 2.8) | P3 |

Nothing else changes: the engine (`src/ibdwt.*`, `src/fft.*`), its table and measurement
(`src/lengths.*`, `src/lengths.txt`, `src/measure.cpp`), the numeric flags, `src/reference.*`,
`src/known.*`, `tests/known/`, `src/main.cpp` and the command line (GOAL Non-goals). The gates enforce
the list. `issues/` and `ROADMAP.md` change only in the last phase (the ledger).

### 2.2 The Jacobi symbol (R5, R18)

`jacobi(a, n)`: n odd and positive, a any value; a is reduced below n once, then the loop strips
the factors 2 of a (flipping the sign when n ≡ 3, 5 mod 8 and their count is odd), swaps a and n by
reciprocity when a < n (flipping when both are 3 mod 4), and replaces a by a − n. It ends at a = 0
with n = gcd: the symbol is 0 unless n = 1. Shallit and Sorenson, "A binary algorithm for the Jacobi
symbol", 1993 ([research/01](research/01-jacobi.md)); Brent and Zimmermann 2010, Remark 3, if the
builder cannot read the paper itself. Measured: 0.78 ms at 10,303 bits, 65 ms at 100,069.

The vectors (`tests/vectors/jacobi.txt`, `jacobi <a> <n> <symbol>`, M_p as `u:0-p`) cover every odd
exponent R1 and R2 name in `make check`: the odd Mersenne prime exponents up to 10,000 and 1237,
1277 and 10303, each with symbols −1, 1 and 0 (a ≡ 0 mod M_p: no factor of M_1237, M_1277 or
M_10303 lies below k = 1.5·10⁷), real LL residues in their shifted form, and general odd moduli
(proper factors, squares, the invariant's genuine zeros). Python's reciprocity loop computes every
record, Euler's criterion checks those modulo a prime M_p, a binary form the rest; the file carries
its command (`python3 tests/vectors/generate.py jacobi > tests/vectors/jacobi.txt`).

### 2.3 The test (R1–R4, R6–R11, R13, R14)

`lucas::test(p, options)` (`src/lucas.cpp`), and `test_lucas_lehmer(p)` = `lucas::test(p)`:

1. **Refusals, before any squaring (R14).** p even or below 3: no verdict, "the Lucas-Lehmer test
   applies to odd exponents of 3 or more". p past P_max (`lucas::largest_exponent()`, the largest
   exponent any supported length holds, 20,381,696 today): no verdict naming P_max. p odd and
   composite (only with `-s`): no verdict, "the Lucas-Lehmer test runs on prime exponents, and p is
   composite". When p is both past P_max and composite, the reason names both.
2. **The shift (R3).** `options.shift`, or one drawn uniformly from [0, p) with `std::random_device`.
   r₀ = 4·2^k mod M_p. Each step: `ibdwt::square_subtract(engine, r, (2k + 1) mod p)`, k ← 2k mod p.
   s = r·2^(p−k) mod M_p (`lucas::unshifted`).
3. **The length.** `options.length`, or `ibdwt::length_for(p)`.
4. **Good states (R4, R6, R7).** A good state is (iteration, shift, residue as `BigInt`); the first is
   (0, k₀, r₀), good by construction. After squaring i, when i is a multiple of
   ⌈(p − 2)/`GOOD_STATES`⌉ or i = p − 2, the run reads the residue back and checks it, in this order:
   - `degenerate(s, p, last)`: s ∈ {1, −1, 2, −2} mod M_p, or s = 0 before the last iteration (R7) —
     a failure counts in `degenerate_failures`, and the Jacobi check does not run;
   - otherwise `jacobi_of(r, k, p)` = J(r − 2^((k+1) mod p) | M_p), counted in `jacobi_checks`; −1
     passes, +1 and 0 fail (`jacobi_failures`) (R4);
   - otherwise, at the last iteration with s = 0, `confirms_prime(s_(p−3), p)`: s_(p−3) = ±2^((p+1)/2)
     (`confirmation_failures`) (R8). s_(p−3) is read back before the last squaring.
   A pass makes the state the new good state. A failure logs a warning and sends the run back to the
   last good state; the `CHECK_FAILURES` + 1-th failure of the run is an error naming the check (R6).
5. **Refusals of the engine (R9, R10).** A `roundoff_error` is replayed once at the same length from
   the residue the engine kept: accepted, the run goes on and counts a transient fault. Refused
   again: back to the last good state at the next larger supported length that holds p (a new
   `ibdwt::Engine`; the good state converted by `to_digits`), counted in `length_retries` and added to
   `lengths`; with no such length, or after `LENGTH_RETRIES` moves, an error naming p, the length and
   the distance. Any other `engine_error` is an error result, never a retry.
6. **Distances (R11).** An accepted squaring past `ibdwt::SIZING_DISTANCE` (0.35) is counted
   (`large_distances`); the first `DISTANCES_LOGGED` are logged one by one, the rest folded into one
   line at the end. `largest_distance` is the run's largest.
7. **Progress (R13).** A line after a squaring once `PROGRESS_SECONDS` have passed since the last:
   the iteration, the time per iteration (from squarings since the last line, so a recovery cannot
   wrap it), the time to go and the largest distance since the last line.
8. **The result.** s = 0 at the end: `Result::prime_by_residue` (Res64 0); else
   `composite_by_residue` with `res64(s, p)`. Both carry the run.

Constants (`src/lucas.hpp`): `GOOD_STATES` 32, `CHECK_FAILURES` 3, `LENGTH_RETRIES` 3,
`PROGRESS_SECONDS` 30, `DISTANCES_LOGGED` 8. A good state every 1/32 of the run costs the in-place
symbol about 8 % at 10,303 and 5 % at 100,069; 30 s keeps every run of `tests/cli.sh` and of the long
tier to its start line.

**The injections (R16).** `lucas::Options` carries three test-only hooks that `bin/mps` never sets:
`refuse(step)` before each squaring, which may throw `roundoff_error` as the engine does (the residue
untouched); `corrupt(step, r)` after each accepted squaring, which may return a residue to hold in its
place; `distance(step, d)`, which may replace the reported distance. `Step` gives the iteration, how
many times it ran before in this run, the length and the shift of the residue it makes. No option,
environment variable or input of `bin/mps` reaches them: `test_lucas_lehmer` passes the defaults, and
nothing under `src/` but the header sets a hook (the gates scan for it).

### 2.4 Pinned for the gates

The gates build against these, wrap them and trace them; anything else is the builder's
(`verify/ll_files.py` holds the exact text):

- `int jacobi(const BigInt& a, const BigInt& n);` and its definition's first line.
- In `src/lucas.hpp`: the five constants by name (their values are the builder's to tune, the note
  must state them, § 2.7), `struct Step` and `struct Options` with the members above, and
  `largest_exponent`, `unshifted`, `jacobi_of`, `degenerate`, `confirms_prime`, `test`, each defined
  once in `src/lucas.cpp` on a pinned first line; `lucas::test` calls the four helpers and
  `ibdwt::square_subtract`.
- In `src/result.hpp`: `struct RunReport` and its ten members, the three run-carrying named
  constructors, `run()`, `RECORD_VERSION = 2`.
- `run_in_order`'s and `test_lucas_lehmer`'s first lines as at `bcb6fdf`; `ibdwt::square_subtract`'s.
- The check labels (§ 2.6), the known-answer check's tally of answers named "known answers…", and
  the formats (§ 2.5).
- `lucas::test`'s first line (the gates wrap it as well as the helpers).
- `src/lucas.cpp` includes neither `src/reference.hpp` nor `src/known.hpp` (§ Invariants 3).

### 2.5 The views (R12, R13, R11)

Verdict line, a run's fields after the outcome, the lengths space-separated when two or more:

```text
M1237 ll: composite, res64 DF5E6622F83F5BE2, length 56, shift 437, jacobi 32 checked 0 failed, degenerate 0, confirmation 0, transient 0, retries 0, large distances 0, largest distance 0.0469 (mps 0.1.0)
M31 ll: prime, res64 0000000000000000, length 2, shift 27, jacobi 29 checked 0 failed, ... (mps 0.1.0)
M127 ll: error, <reason>, lengths 4 5 6 7, shift 3, ... (mps 0.1.0)
```

Record (keys in this order, spaced as `json.dumps` writes them; `largest_distance` the shortest
decimal that reads back, as Python's `repr` writes a float, with `.0` on a whole number):

```text
{"exponent": 1237, "worktype": "ll", "status": "composite", "res64": "DF5E6622F83F5BE2", "lengths": [56], "shift": 437, "jacobi_checks": 32, "jacobi_failures": 0, "degenerate_failures": 0, "confirmation_failures": 0, "transient_faults": 0, "length_retries": 0, "large_distances": 0, "largest_distance": 0.046875, "program": {"name": "mps", "version": "0.1.0"}, "record_version": 2}
```

`"status": "error"` with `"reason"` for an error. Under `-j` every error, the exponent check's
included, prints its record on stdout as it arrives; its line still goes to stderr (GOAL
Clarification, superseding `verdicts-and-exit-statuses`). The no-verdict lines of tf and p1, and the
LL test's own no verdicts (R14), carry no run. Log lines (pinned for the probe):
`ll M<p>: iteration <i> at length <N> has round-off distance <d.dddd>, past 0.35`,
`ll M<p>: <n> more squarings past the sizing distance, counted and not logged`,
`ll M<p>: iteration <i> of <p−2>, <x> ms per iteration, about <h>h<mm>m<ss>s to go, largest distance <d.dddd> since the last line`.

### 2.6 The checks

| File | Label | Phase | R-IDs |
|---|---|---|---|
| `tests/check_mp.cpp` | the Jacobi symbol matches the vectors | P1 | R5 |
| `tests/check_result.cpp` | run_in_order stops at the first verdict or error | P1 | R15 |
| `tests/check_lucas.cpp` | the test reports the known answers at two given shifts | P2 | R1, R2, R3 |
| | the test chooses a fresh shift each run | P2 | R3 |
| | the test checks the Jacobi symbol at intervals and on the final residue | P2 | R4 |
| | the test catches degenerate states | P2 | R7 |
| | the test confirms a zero residue before it reports prime | P2 | R8 |
| | the test refuses exponents it cannot run | P2 | R14 |
| | the result of the test names its run in both views | P2 | R12 |
| | the test recovers from a failed check, up to its limit | P2 | R6 |
| | the test replays a refused squaring once | P2 | R9 |
| | the test moves to a larger length when a refusal reproduces | P2 | R10 |
| | the test counts and logs large round-off distances | P2 | R11 |
| | the test logs its progress | P2 | R13 |

The known-answer check runs every exponent R1 and R2 cover in `make check` at shift 0 and at
p/3 + 1 and grades each with an `engine_support::Tally` whose `what` begins "known answers" (its line printed
passing or failing; under the engine's sabotage every case of it must disagree).
`tests/ll.sh BINARY [long]` runs `BINARY -t ll p` on the odd Mersenne prime exponents up to 10,000
(100,000 with `long`) and the `ll` rows up to 20,000 (200,000), prints each verdict line and expects
its start and exit status; `make check` and `make check-engine` run it after `tests/cli.sh`,
`make check-long` runs it with `long`. `tests/cli.sh` changes only as R16 now reads.

### 2.7 AGENTS.md and the notes (R17, R18)

- **AGENTS.md** (append-only to the high-blast list and to the four lines `tests/cli.sh` reads; the
  `verify` row unchanged): P2 adds `` - `src/lucas.hpp`, `src/lucas.cpp` (…) `` to the list and a § Map
  bullet, refreshes the stale § Map wording (`src/math.*`, `src/result.*`, `src/mp.*`, `src/ibdwt.*`,
  the intro) and names `tests/check_lucas.cpp`, `tests/ll.sh` and the vectors; P3 adds the
  `make check-long` and `ci/engine-gate.sh` lines to § Build and run and § Map, and the profile row
  `| landing gate | … |` naming R17's paths, `make check-engine` and `ci/engine-gate.sh`
  ([research/04](research/04-docs-and-landing-gate.md) has the wording).
- **`doc/reference-arithmetic.md`** (P1): the Jacobi symbol, its algorithm, invariant and termination,
  cited; `doc/references.md` gains `ShallitSorenson1993`.
- **`doc/lucas-lehmer.md`** (P2): "What mps uses" as built — `lucas::test`, `src/lucas.cpp`,
  `test_lucas_lehmer`, the shift, the schedule, the order of the checks, recovery, replay, the
  larger length, 0.35 and the largest distance, progress, the refusals (P_max, prime exponents) — each
  limit as "`NAME` (value)" equal to the header's; the 0-symbol text rewritten (a failed check, no
  factor path); no stale sentence. Limits are format constants under `doc/README.md`'s rule.
- **The three sentences** (P3) no longer say `make check` runs the reference tests "on the published
  Mersenne prime exponents".

### 2.8 The landing gate (R17)

`ci/engine-gate.sh [REV]` archives REV (default HEAD) into two fresh directories, runs
`make check-engine` with `MPS_CLANG` (default clang++) and then with `MPS_GCC` (a real GCC:
`-dM -E` defines `__GNUC__` and not `__clang__`; else exit 2), one after the other, and compares their
`   residues <label>: <sha256>` lines through `ci/engine-gate.sh compare LOG_A LOG_B`, which requires a
green `make check-engine` in each log (two `check:` summaries and a `cli:` line, 0 failed), well-formed
digest lines, and exactly the five labels it holds in a constant. It exits 1 naming each label that
differs and both digests, 2 on usage or a missing compiler, and ends a green run with
`engine-gate: green on <full hash>: …`. Two engine tiers, about 20 minutes on an M2 Mac. The profile
row names it; this cycle passes it before landing (P4 dispatch A runs it on the tested commit, the
ledger quotes its green line, P4's gate checks that commit's code is the tested one).

### 2.9 Requirement → design map

| R-ID | Design element(s) | Graded by |
|------|-------------------|-----------|
| R1 | § 2.3; `tests/ll.sh` (`make check`, `make check-long`) | known-answer check; ll.sh in both make logs; gate drives `bin/mps` itself on both compilers; sabotaged engine reaches no known answer |
| R2 | as R1, the `ll` rows | as R1 |
| R3 | § 2.3.2; `Options::shift` | known-answer check at two given shifts; probe at 2–4 shifts with the shift sequence traced; six `bin/mps` runs not all equal |
| R4 | § 2.3.4, `jacobi_of` | Jacobi check; probe (+1 at the last and at a checked iteration); `jacobi-pass` mutant |
| R5 | § 2.2 | vectors check; the file regenerated byte for byte; the gate's own recomputation and coverage; probe against its own symbol; `jacobi-flip`, `jacobi-nonzero` mutants |
| R6 | § 2.3.4 | recovery check; probe at `CHECK_FAILURES` and every attempt; `jacobi-pass` |
| R7 | `degenerate` | degenerate check; probe (1, −1, 2, −2, 0 at and between checks); `no-degenerate` |
| R8 | `confirms_prime` | confirmation check; probe (0 at the last iteration of M1237); `no-confirmation` |
| R9, R10 | § 2.3.5 | replay and retry checks; probe (once; at one length; every length; no larger length) |
| R11 | § 2.3.6 | distance check; probe (counts, largest, the log lines, the fold) |
| R12 | § 2.5, `RunReport` | views check; probe builds both views itself for every known answer and an error; gate parses `-j` records |
| R13 | § 2.3.7 | progress check; probe (one line per iteration at interval 0, in order, in § 2.5's form) |
| R14 | § 2.3.1 | refusal check; probe (no squaring); gate drives `bin/mps` |
| R15 | `run_in_order` | stop-rule check; probe's stand-ins; `old-stop-rule` mutant |
| R16 | § 2.3 injections; § 2.6; Makefile | gate: hook and environment scan; cli.sh against `bcb6fdf`'s (§ 6); interleaved timing; `make check-long` timed; both compilers |
| R17 | § 2.8 | gate: compare mode on six fixtures, a clang-as-GCC refusal, the run mode on the tree (P3 full) and on the tested commit (P4); the profile row |
| R18 | § 2.7 | gate: `ll_files.py agents`, `ll_docs.py`, `notes.py claims`; P4: the same-commit history |

## 3. Invariant gate (AGENTS.md constitution check)

Walked before research (from the GOAL) and again on this design. Sections touched: the high-blast
list, § 1, § 2, § 3, § 4, § 5, § Conventions.

- **High-blast list** — `src/math.*`, `src/mp.*`, `src/result.*` and `tests/vectors/` are on it
  (trigger 2 fired); `src/lucas.*` joins it in the commit that adds it (R18).
- **§ 1 Verdicts are earned** — prime only from a complete run ending in 0 whose every check passed,
  with the confirmation; composite only from a nonzero final residue of such a run; a run that cannot
  recover reports an error and exits 20; every result names the test, exponent, lengths, shift, Res64,
  counts and version (R12). The verdict never comes from `reference::` or the tables (pinned).
- **§ 2 Arithmetic is checked** — the engine refuses, the driver replays and retries from the last
  good state at a larger length (the 2026-10-22 ruling binds it from this cycle); R17's landing gate
  keeps the engine's full differential suite and both compilers' digests in force; no flag changes;
  a given shift gives the same residue on every run.
- **§ 3 Known answers** — no row added or edited; the checks read them through `src/known.*`.
- **§ 4 Clean room** — the Jacobi algorithm from Shallit and Sorenson 1993 (and Brent and Zimmermann
  2010), the test from Lehmer and the project's notes; no other program's source opened.
- **§ 5 Dependency-free** — standard C++20 and POSIX sh; the vectors' oracle is Python, with the tests.
- **§ Conventions** — the record's version bumped in the commit that changes it; the notes updated
  in the commits that change their algorithms; no spec ids in source; the profile's `verify` stays
  `make check` (its target gains a step, the string does not change).

### Deviation justifications

Re-read against every R-ID's literal wording (2026-11-01, after the foreman's three rulings and
again after the review's fold): no row changes what an R-ID requires.

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| Test-only hooks in `lucas::Options` (added complexity) | R6–R11 SHALL be shown by injection, and R16 keeps injections out of `bin/mps`'s reach | A compile-time switch would test a binary other than the one that ships; an environment variable is what R16 forbids |
| `make check` and `make check-engine` gain a step, `tests/ll.sh` (added complexity) | R1 and R2 are worded as `mps -t ll p` runs, so they run `bin/mps`; `check-engine` stays a superset so the landing gate runs no less than `verify` | Through `bin/check` alone the exit status and the line would be a function's, not `mps`'s; inside `tests/cli.sh` they would widen R16's clause |
| The degenerate check runs before the Jacobi check, and a degenerate failure is not counted as a Jacobi check | 0, 1 and −2 pass the Jacobi check, so the order decides which counter a failure lands in; R12 names both | Running both on every state would count one fault twice |

## 4. Rabbit holes (resolved)

- **Which Jacobi algorithm, at what cost?** → binary, in place, 65 ms at 100,069 bits
  ([research/01](research/01-jacobi.md)).
- **Symbol 0 at an odd composite p** → the foreman's ruling: R14 refuses odd composite p before any
  squaring.
- **Which residue does each check read, under a shift?** → § 2.3 ([research/02](research/02-driver.md));
  checked in Python for every shift of 14 exponents.
- **An error's record vs `verdicts-and-exit-statuses`** → R12 supersedes it (ruling); printed under `-j`.
- **R16's cli.sh clause, mechanically** → `bcb6fdf`'s script against stand-ins of the test (§ 6)
  ([research/03](research/03-results-and-cli.md)).
- **The landing gate's form and its hollow passes** → § 2.8 ([research/04](research/04-docs-and-landing-gate.md)).
- **Budgets** → measured on the prototype (§ 6).
- **The planner's prototype** found one driver bug the checks now pin (the progress rate wrapped
  after a recovery) and one format question (record floats: `%.17g` against `json.dumps`).
- **Adversarial design review** — see § 4.1.

### 4.1 Adversarial design review

Run 2026-11-01 by a fresh reviewer, a child of the planning step, against the GOAL, this PLAN's
drafted design, the drafted gates and the planner's prototype (`49c5f8b`), with executed attacks in a
scratch clone. Its headline: one tree carrying a wrong recovery, a wrong length move, a discarded
distance, a Jacobi symbol that returns −1 above 11,000 bits, seven hollowed checks and two weakened
cli.sh lines gave the same gate result as the prototype. No finding needs the GOAL changed. Every
finding and what it changed:

1. **MEDIUM — the sabotaged `bin/mps` never saw `GATE_MUTANT`** (`ll_runs.py` cleared the
   environment): always red. → `GATE_MUTANT` passes through.
2. **MEDIUM — the hook scan matched `stats.distance =` in the engine**: always red. → It reads only
   files that include `src/lucas.hpp`, and `src/lucas.cpp`.
3. **MEDIUM — the all-disagree rule failed a correct check** (its tally of agreeing shifts agrees
   under sabotage). → The rule applies to the tally named "known answers…" (pinned, § 2.6).
4. **HIGH — recovery to s₀ instead of the last good state passed.** → The probe traces the steps
   through the refuse hook: after a failure seen at a checked iteration, the next step is the last
   good state's next iteration, its second run, at that state's doubled shift (every injection).
5. **HIGH — a length move carrying the current residue passed; the replay was not traced.** → The
   probe traces both: the replay is iteration 100, attempt 1, same length and shift; after a move,
   the first step at the next length is the last good state's next iteration.
6. **HIGH — a driver discarding the engine's distance passed.** → The probe records the distances
   the engine reports (through the hook, as identity) and requires the reported largest to equal
   their largest and be above 0; `ll_runs.py` requires a largest distance above 0 from 521 up.
7. **HIGH — checks hollow above ~11,000 bits passed** (a symbol returning −1 there; checks skipped
   above it). → The probe checks `jacobi` and the four helpers at 21,701 and 44,497 bits;
   `ll_runs.py` reads every verdict line in `make check` and `make check-long` for a clean run of its
   schedule (Jacobi count equal to the schedule's, every failure count 0).
8. **HIGH — seven builder checks could be hollow.** → `lucas::test`'s pinned first line is wrapped
   too, with six more mutants: `refuse-off` (replay, length move), `distance-off`, `progress-off`,
   `shift-fixed` (fresh shift), `refusal-off` (refusals), `run-dropped` (views); each must turn its
   check red.
9. **HIGH — cli.sh's gate matched expectations by name only** (a hollowed expectation, or a helper
   made to return 0, kept its name; a script could print its own PASS lines). → (d): bcb6fdf's helpers
   verbatim in the tree's script, and every `expect` line of bcb6fdf's script that is gone must name an
   expectation (b) found the test decides; the PASS lines carry a nonce the gate draws per run. The
   prototype's own cli.sh broke (d) once (a line changed for `-s 33`'s old warnings); fixed in the
   prototype, which shows the check bites.
10. **MEDIUM — multi-length views were not graded by the gate.** → The probe compares both views
    with PLAN § 2.5's in the length-move and every-length cases.
11. **MEDIUM — R17's fixtures and its GCC test were weak.** → A flip fixture per label, each in both
    orders; an extra-label fixture; the refusal of this host's `g++` when it is clang; the run mode
    under the compiler shim, requiring that clang++ and g++-16 each compiled the engine.
12. **MEDIUM — P4's same-commit greps failed a natural § Map bullet and passed a missing high-blast
    line.** → `ll_files.py added COMMIT PATH` reads AGENTS.md at the commit and its parent by section
    (the high-blast list; § Map); `agents` checks the high-blast line inside its section.
13. **MEDIUM — the caller's environment could steer the gate** (`GATE_NO_ENGINE_RUN`,
    `CHANGES_PHASE`). → `common.sh` clears both (and `GATE_LL`); `prepush.sh` and `p4.sh` set theirs
    after sourcing it.
14. **MEDIUM — injections by aggregate initialisation or from a file.** → No file under `src/` but
    `src/lucas.*` names `lucas::Options`; `src/lucas.cpp` names none of `getenv`, `environ`,
    `fstream`, `fopen`, `argv`, `std::cin`.
15. **LOW** — every "`NAME` (v)" in the note must equal the header (not only the first); the three
    sentences must name `tests/known/mersenne-exponents.txt` and `tests/vectors/reference.txt`; the
    `check` and `check-engine` recipes may not hold `|| true`; P4's `RECORD_VERSION` grep allows an
    indent. Left with the builder's own check: R13's rate after a recovery (the prototype's check pins
    it).

## 5. Risks & open questions

- **The budget.** The prototype's `make check` is +4.05 to +4.11 s over `bcb6fdf` in every full dry
  run (budget +5 s): about 0.9 s of margin, most of the cost the known-answer check at two shifts in
  the bounds-checked `bin/check`, and 32 Jacobi checks a run (it was +3.3 s at 8). The levers, in
  order: `GOOD_STATES` (its value is not pinned; the note states whatever the header says), the
  in-place symbol's speed, the hook checks' exponents. Never coverage: R3 needs every exponent in
  `make check` at two shifts, and R1 and R2 every one through `bin/mps`.
- **An undetected error becomes a good state.** A corrupted residue passes one Jacobi check with
  probability about ½; then the run ends wrong. The schedule sets how fast errors are caught, not
  whether; a second shifted run is `issues/known-primes-campaign.md`'s.
- **Large exponents.** The quadratic symbol at P_max costs about an hour per check, 32 per run, and a
  failure redoes 1/32 of a run in memory; the last phase files both as deferrals
  (`issues/large-gcd-and-jacobi.md`, `issues/checkpoint-and-resume.md`).
- **The landing gate's GCC.** On this Mac `g++` is clang; the script refuses it and the gates pass
  `MPS_GCC=g++-16`.

## 6. Verification strategy

**Gates** live in `verify/` from the plan commit and are frozen there: `p1.sh`, `p2.sh`, `p3.sh`
(each `full` by default, `quick` for the cheap partial mode), `prepush.sh` (P4 dispatch A; `scan`
mode), `p4.sh` (P4 dispatch B; `audit` mode and full), `redact.sh`; helpers `common.sh`, `gate.sh`,
`cluster.sh`, `probe.cpp`, `ll_files.py`, `ll_vectors.py`, `ll_runs.py`, `ll_cli.py`, `ll_docs.py`,
`ll_checklog.py` (no helper named after a Python module; every one runs with `python3 -I`). A gate
copies the tree as a clone holds it and runs, in order:

1. **Files** (`ll_files.py`): only § 2.1's paths changed for the phase (`issues/` and `ROADMAP.md` only
   in P4, whose gates grade the file list as of P4 and everything else as of P3); the pins of § 2.4;
   the labels; AGENTS.md (the list only widens, the four lines `tests/cli.sh` reads and the `verify`
   row kept, the new names in their sections); the Makefile (flags as at `bcb6fdf`, the recipes); the
   sources (SPDX headers, no spec ids, no reference or tables in the driver, the hooks and
   `lucas::Options` out of `bin/mps`'s reach).
2. **Builds** with clang++ and g++-16 behind the logging shim: `make check` green on both, the same
   counts, no warnings; from P2 `tests/ll.sh`'s lines in the log read for every known answer, each a
   clean run of its schedule.
3. **Vectors**: `tests/vectors/jacobi.txt` equal byte for byte to what its command writes; every
   record recomputed by the gate's own Python (its own operand forms, the reciprocity loop, Euler's
   criterion for prime M_p); every exponent covered with each symbol.
4. **Probe** (`probe.cpp`, on both builds' objects): P1, `jacobi` against the probe's own symbol to
   44,497 bits and on general odd moduli, its errors, and the stop rule on stand-ins; P2, the helpers
   against their definitions, every known answer at two to four given shifts with the schedule traced,
   the random shift, the refusals, every injection of R4 and R6–R11 with its counts, outcomes, traced
   recovery and replay, the views of § 2.5, the error record under `-j`, the distances the engine gave,
   and R13's lines.
5. **Mutants** (the pinned definitions renamed and wrapped in one build, `bin/check` and `bin/mps`):

   | Mutant | Must fail | From |
   |---|---|---|
   | old-stop-rule | the stop rule | P1 |
   | jacobi-nonzero | the vectors | P1 |
   | jacobi-flip | the vectors (P2: and the known answers) | P1 |
   | jacobi-pass | the Jacobi check; recovery | P2 |
   | no-degenerate | degenerate states | P2 |
   | no-confirmation | the confirmation | P2 |
   | unshift-off, subtract-bit | the known answers | P2 |
   | engine-sabotage | the known answers, every case of "known answers…" disagreeing; and the sabotaged `bin/mps` reaches no known answer | P2 |
   | refuse-off | the replay; the length move | P2 |
   | distance-off, progress-off, shift-fixed, refusal-off, run-dropped | distances; progress; fresh shift; refusals; views | P2 |

6. **`bin/mps`** (P2): driven by `ll_runs.py` on both builds — every known answer's line, exit status
   and record, R14's refusals, six M1279 runs not all at one shift — and `ll_cli.py`, R16's cli.sh
   clause: (a) `bcb6fdf`'s script, its record version alone made 2, green on a stand-in whose test is
   `bcb6fdf`'s stub; (b) every expectation it fails on the real binary also fails on some other
   stand-in (another no verdict, a prime, an error); (c) every one it passes is in the tree's script as
   often, passing; (d) `bcb6fdf`'s helpers verbatim and every vanished `expect` line one (b) decided.
7. **Landing gate** (P3): `compare` mode on twenty fixtures (green on equal digests; a flip of each
   label in each order, a dropped label, none, a red summary, a malformed line, an extra label: each
   exit 1, naming the label and digest where one differs); refusal of clang as GCC; in full mode the
   run mode on a commit of the tree, under the shim.
8. **Time** (full mode): `make check` with nothing to rebuild, `bcb6fdf`'s copy against the tree's,
   interleaved, least of 5 each, once more before failing, ≤ +5 s; from P3 `make check-long` on clang,
   alone, ≤ 900 s, then on GCC, both logs read for every long-tier answer.
9. **Notes** (`ll_docs.py`, `notes.py claims` on every note this cycle changed).

**Footprint.** At most two jobs at once (the two builds); every heavy run alone; each command in its
own process group, killed with the group on exit; programmatic deletion only of the gate's own
`mktemp` directory, asserted. **Run one gate at a time.**

**R16 baseline** (measured 2026-11-01 on an M2 Mac, clang, `make check` with nothing to rebuild,
`bcb6fdf`): least of 5 runs interleaved with the prototype, 4.967 s (load 5.6). Every full dry run
re-measured it beside the tree: 4.929–4.980 s.

**Planner's dry runs** (self-test mode, on the prototype in the detached worktree
`.worktrees/lucas-lehmer-test-prototype`, each phase's gate on its own phase's tree — P1 `62117a1`,
P2 `8eec537`, P3 `783d932`, P4 `ddf5f41` — one at a time; times on an M2 Mac under a load of about 5):

| Gate | Tree | Mode | Time | Result |
|---|---|---|---|---|
| `p1.sh` | P1 | quick | 2 m 26 s | green: 104 checks, 644 cli; probe 380 cases; 3 mutants |
| `p1.sh` | P1 | full | 4 m 22 s | green: both compilers; make check +0.062 s over `bcb6fdf` |
| `p2.sh` | P2 | quick | 4 m 44 s | green: 116 checks, 645 cli, 48 ll; probe 1,278 cases; 16 mutants; cli.sh: 75 expectations changed, each one the test decides, 569 kept |
| `p2.sh` | P2 | full | 7 m 9 s | green: both compilers; probe 1,374 cases; make check +4.110 s |
| `p3.sh` | P3 | quick | 4 m 48 s | green: and the landing gate's compare mode red on its 20 fixtures |
| `p3.sh` | P3 | full | 26 m 20 s | green: make check +4.106 s; `make check-long` 57 s on clang (both compilers green); `ci/engine-gate.sh` green on a commit of the tree (both builds compiled the engine under the shim; 5 digests equal) |
| `prepush.sh` | P3 tree with the P4 Notes bullets in the working tree | — | 26 m 41 s | green: its two lines as § 2.8 and TECH P4 quote them (make check +4.106 s; check-long 58 s; `engine-gate: green on 783d932…`) |
| `p4.sh audit` | P4 fixture (bullets, ledger and redacted evidence of three runs of the P3 commit) | — | 49 s | green |
| `p4.sh` | P4 fixture | full | 9 m 5 s | green: the cumulative gate as of P3, its file list (with `issues/`) as of P4 |

The first P1 quick run (before the fold) showed the self-test grading a prototype against `bcb6fdf`
saw the shaping's own `issues/` edits; the self-test now takes the build's starting commit
(`GATE_FROM`), the real gate the plan commit. The first P2 runs found the review's findings 1–3
independently. The P4 runs are the remedy for `ibdwt-transform-engine`'s frozen gates refusing their
own ledger: dispatch A's pre-push and dispatch B's gate were each run on a tree carrying the ledger's
`issues/` edits, and the tested commit, the landing gate's commit and `PUSH_REF` are read from one place
each (`common.sh`, the ledger).

**Red defects** (each applied to its phase's prototype tree, the phase gate in quick mode):

31 defects, 30 red. P1: an edited vector symbol (trailer recomputed), the stop rule reverted, the
vectors check hollowed — red; the initial reduction of a ≥ n removed from `jacobi` — green, and not a
defect: the binary loop reduces by subtraction, and the probe's a ≥ n cases agree. P2: the Jacobi
argument s in place of s − 2, −2 missing from the degenerate states, no return to the good state, no
length move, a shift that is a function of p, an environment variable reaching a hook, a tf line
edited in both `bin/mps` and cli.sh, the error record not printed, odd composite p run, distances
uncounted, the confirmation skipped, the final residue taken from `reference::lucas_lehmer`, a limit
misstated in the note, the high-blast line missing — each red, most several times over (the builder's
checks, the probe, the mutants' clean run, the file scan). P3: `check-long` missing, `ci/engine-gate.sh`
blind to one label, the landing gate row missing `src/measure.cpp`, a stale sentence restored — red.
P4 (`p4.sh audit` on the fixture): a Notes bullet missing, `## Times` missing a line, the landing gate
quoted on other code, a cluster count off by one, the tested commit not on the pushed branch — red.
And the review's own attacks, rerun on the folded gates (P2 quick): recovery that rewinds to s₀ with
its counts kept (red: probe, the builder's checks), a length move that carries the current residue
(red: probe), a Jacobi symbol that returns −1 above 11,000 bits (red: probe), and a cli.sh expectation
weakened under its old name (red: `ll_cli.py` (d)). The seven hollow builder checks are the six new
mutants' targets and are red by construction on any check that grades nothing.

---

*Backing research: [`research/00-digest.md`](research/00-digest.md).*
