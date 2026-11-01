# Research digest — lucas-lehmer-test

Four briefs, consolidated; each contradiction resolved with one recommendation. 

## Decisions

- **The Jacobi symbol** ([01](01-jacobi.md)): Shallit and Sorenson's binary algorithm
  (subtract, shift, compare; no division) as `int jacobi(const BigInt& a, const BigInt& n)` in
  `src/mp.*`, in place on two limb vectors. Measured on an M2 Mac: 0.78 ms at 10,303 bits, 65 ms
  at 100,069. `mp_error` for a zero or even modulus. Citation: Shallit and Sorenson 1993 (new
  `doc/references.md` entry, DOI checked), or Brent and Zimmermann 2010, Remark 3, already in the
  bibliography, if the builder cannot read the paper; Crandall and Pomerance § 2.3.1's algorithm
  number is to be verified in the book. Written up in `doc/reference-arithmetic.md`.
- **The vectors** ([01](01-jacobi.md)): `tests/vectors/jacobi.txt`, `jacobi <a> <n> <symbol>`, a new
  field kind `J`, M_p as `u:0-p`; every exponent R1 and R2 cover in `make check`, both signs
  (by theorem and by literal searched seeds), symbol 0 (a ≡ 0 mod M_p: no factor of M_1237, M_1277
  or M_10303 exists below k = 1.5·10⁷), real LL residues s_i − 2 in their shifted form, and general
  odd moduli (proper factors of M_11…M_37, the invariant's genuine zeros at p = 9, 15, 21, 25).
  Python's classical loop on every record, Euler's criterion for prime M_p, Brent–Zimmermann's
  cubic binary form for the rest. About 470 records, 45 KB.
- **The schedule** (01 against 02's 10 and the prototype's 8): a good state, checked, every
  ⌈(p − 2)/32⌉ iterations and at the end (`GOOD_STATES = 32`): about 8 % at 10,303 and 5 % at
  100,069 with the in-place symbol. Iteration-based, so the reported Jacobi count is a function of
  p alone.
- **The shift** ([02](02-driver.md)): r₀ = 2^((k₀+2) mod p); each step subtracts
  2^((2k+1) mod p) and doubles k; the Jacobi argument is r − 2^((k+1) mod p); degenerate states are
  r ∈ {±2^k, ±2^((k+1) mod p)} (checked unshifted); R8 holds when s_(p−3) = ±2^((p+1)/2). Length 1
  holds p = 3 and 5. Degenerate check first, then the Jacobi check, then (on a zero final residue)
  the confirmation: the Jacobi check alone passes s = 0, 1 and −2.
- **Recovery** (02): one replay per refusal (R9); a refused replay goes back to the last good state
  at the next larger supported length that holds p (R10); `CHECK_FAILURES = 3`, `LENGTH_RETRIES = 3`.
  Both error branches are reachable in checks (p = 3 runs out of lengths; a start at the largest
  length holding 31 does too). Any other `engine_error` is an error result, never a retry.
- **Injections** (02, the prototype): `lucas::Options` with a shift, a first length, a progress
  interval and three test-only hooks (`refuse` before a squaring, throwing a real `roundoff_error`;
  `corrupt` after an accepted squaring; `distance` replacing the reported distance) that `bin/mps`
  never sets. A sure Jacobi failure: at a check iteration, r = 2^((k+1) mod p) + 1, symbol J(1) = +1.
  Known-answer runs never pass 0.35 (largest seen 0.25 at 44,497), so R11 is shown through the hook.
- **Progress** (02 against the prototype): time-based for `bin/mps`; 30 s, so the long tier's runs
  of up to ~25 s and every `tests/cli.sh` run log only their start line; the checks set 0.
- **The result** ([03](03-results-and-cli.md)): a `RunReport` struct (not `Run`: the name is taken
  by locals in `src/main.cpp`, the printer and `tests/check_result.cpp`), carried by
  `prime_by_residue`, a run-carrying `composite_by_residue` and an `error` overload; Res64 0 is never
  read as prime. Verdict line and record in the brief's formats, `RECORD_VERSION` 2.
- **The stop rule** (03): `or outcome == OUTCOME_ERROR` at `src/math.cpp:111`; the only check of the
  old rule is `tests/check_result.cpp:908-942`.
- **Known answers through `bin/mps`** (03): `tests/ll.sh BINARY [long]`, out of `tests/cli.sh`;
  `make check` runs it after `tests/cli.sh`, `make check-engine` too (a superset, per 04), and
  `make check-long` runs it with `long`. Measured: 100,069 takes 23 s on the prototype `bin/mps`;
  the long tier is about a minute per pass.
- **The cli.sh gate** (03): compare the real binary's failures on bcb6fdf's `tests/cli.sh` (with
  only the record version patched) to an oracle stand-in returning the expected outcome per exponent;
  a stub stand-in must pass that script whole. The union of generic stand-ins is too loose.
- **The landing gate** ([04](04-docs-and-landing-gate.md)): `ci/engine-gate.sh [REV]` (git archive
  of the commit, two fresh copies, `make check-engine` with clang++ and a real GCC, detected by
  `__GNUC__` without `__clang__`, one after the other, about 16–20 minutes) and
  `ci/engine-gate.sh compare LOG_A LOG_B`; the five digest labels held in a constant, two green
  `check:` summaries and a green `cli:` line required, exit 1 naming the label and both digests.
  Profile key `landing gate`.
- **Notes and AGENTS.md** (04): append-only to the four AGENTS.md lines `tests/cli.sh:592-614`
  reads; the stale sentences listed with lines; limits stated by their constants' names (format
  constants, no markers), values under 100.

## Measured on the prototype

- R16's baseline: `make check` with nothing to rebuild, clang, bcb6fdf: **4.967 s** (least of 5,
  interleaved with the prototype, load 5.6, an M2 Mac, 2026-11-01). The prototype (32-check schedule
  not yet applied; 8 good states): 8.292 s, **+3.325 s** of the +5 s budget. Of that, the known-answer
  check in `tests/check_lucas.cpp` is about 2.2 s and `tests/ll.sh` about 0.9 s.
- The long tier (`make check-long`, clang): 43.8 s for 27 odd primes up to 100,000 and the rows at
  1237, 1277, 10303 and 100069 (budget 900 s). 100069 alone: 23 s, the row's Res64, no genuine zero
  symbol.
- `make check` green on clang and g++-16: 116 checks, 645 cli expectations, 48 ll expectations.
- A prototype bug the checks now pin: the progress rate after a recovery wrapped (unsigned
  `i - line_iteration`); counted by squarings since the last line instead.
- Record floats: `%.17g` writes 0.36 as 0.35999999999999999; the record's rule is Python's
  `json.dumps`, so the plan pins a shortest round-trip form (`std::to_chars`).

## Questions the digest raised, resolved

The foreman's rulings at plan, committed to the GOAL as dated Clarifications (2026-11-01):

1. `-s` on an odd composite exponent: no verdict, exit 42, before any squaring (R14 extended).
2. An error's JSON record: under `-j` every error prints its record on stdout with status `error`;
   R12 supersedes `verdicts-and-exit-statuses`.
3. R16's cli.sh clause reworded to the expectations the Lucas–Lehmer test's outcomes decide plus the
   record version, graded mechanically against `bcb6fdf`'s script.
