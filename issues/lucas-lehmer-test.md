---
status: unshaped
kind: feature
appetite: big
---

# Lucas–Lehmer with error checks, shifts and checkpoints

> **Candidate, not a contract.** `code mpsearch feature` promotes this into `spec/{slug}/GOAL.md`,
> where appetite, non-goals and the R-IDs get negotiated. Do not copy it verbatim.

## Problem

The Lucas–Lehmer test is the only test here that proves a Mersenne number prime, and the definition
of done in `ROADMAP.md` is stated in its terms: every known Mersenne prime exponent ends in a zero
residue. GIMPS still uses LL for this. A probable prime from PRP is confirmed by LL, and the largest
known prime was confirmed with four programs. At `257acb7` the test is a stub that returns `true`
(`src/math.cpp`).

A trustworthy LL test needs four things, and the stub has none of them:

- **A Jacobi check.** With s_0 = 4 and s_{i+1} = s_i² − 2, the identity
  s_{i+1} − 2 = (s_i − 2)·s_{i−1}² makes the Jacobi symbol J(s_i − 2 | M_p) the same for every
  i ≥ 1. It equals J(12 | M_p) = J(3 | M_p) = −1, since M_p ≡ 7 (mod 12). J(2 | M_p) = +1, so a
  residue shift leaves it unchanged. A random error flips the symbol with probability ½: a simulation
  caught 49% of single-bit errors injected into M1277 runs. Prime95 runs the check every 12 hours, at
  about 30 s each, and keeps two save files that passed it. No citable published statement of the
  invariant turned up, so the derivation belongs in `doc/`.
- **A shift.** Store s·2^k. Each squaring doubles k modulo p, and the "−2" becomes a subtraction of
  2^((2k+1) mod p), a single bit fused into the carry pass. Two runs with different shifts push
  different data through the transform and must agree. GIMPS shifts every test for this reason.
- **Degenerate-state checks.** x ↦ x² − 2 fixes 2 and −1, and sends 0 to −2 and then to 2. A buffer
  zeroed near the end therefore fakes a "prime". The residue has to be checked for 0, ±1 and ±2 at
  every checkpoint. For a prime, s_{p−3} ≡ ±2^((p+1)/2), which gives a free sanity check on any
  "prime" verdict.
- **Checkpoints.** The largest known exponent needs 136 million squarings: days on one device. A run
  has to survive interruption without changing its answer. `src/io.hpp` sketches a plain-text `.mps`
  format, but nothing reads or writes it.

Historically, unprotected LL tests were wrong about 1.5% of the time when no error was reported, and
about half the time when one was. That is why GIMPS moved first-time tests to PRP
(`issues/prp-gerbicz-test.md`).

## Why it was deferred

Never started. It depends on the transform engine (`issues/ibdwt-transform-engine.md`). For the
smallest exponents it can run first on the reference big integer
(`issues/reference-big-integer.md`).

## Outcome / vision

`mps -t ll p` runs the Lucas–Lehmer test on the selected engine, from seed 4 with a per-run shift. It:

- logs progress with iteration, time per iteration, ETA and the largest round-off seen;
- checks the Jacobi symbol on a schedule and at the end, and checks for degenerate states at every
  checkpoint;
- writes a versioned, checksummed checkpoint atomically on a timer, and resumes from it;
- ends with a verdict, the 64-bit residue and the shift.

The checkpoint format is the one `.mps` file format, shared with PRP and P-1.

## Sketch of the acceptance criteria

- **R1** — For every known Mersenne prime exponent up to a bound for `make check` (a few seconds)
  and a larger bound for a long check, the test SHALL end in a zero residue and report the exponent
  prime.
- **R2** — For composite exponents in the known-answer tables, the final Res64 SHALL match the
  table. M1277 → `5613A480590E78BA`, from published runs at three shifts.
- **R3** — WHEN a run is killed and restarted with the same checkpoint file, it SHALL resume from the
  last checkpoint and finish with the same residue as an uninterrupted run.
- **R4** — WHEN two runs use different shifts, both SHALL report the same Res64.
- **R5** — IF a Jacobi check fails, THEN the run SHALL return to the last checkpoint that passed. IF
  failures repeat past a limit, THEN it SHALL report an error, exit non-zero, and print no verdict.
- **R6** — IF the residue reaches 0, ±1 or ±2 before the last iteration, THEN the run SHALL treat it
  as an error, not a result.
- **R7** — The checkpoint SHALL record the format version, test, exponent, iteration, shift,
  transform length and engine, with a checksum over the residue. A file that fails its checksum
  SHALL NOT be resumed.
- **R8** — The checkpoint SHALL be written to a temporary name and renamed into place, so an
  interruption never leaves a partial file under the real name.

## Notes

- Error injection for tests: a test-only way to corrupt the residue at iteration k, so R5 and R6 are
  shown to work.
- The Jacobi symbol of a p-bit number costs about one GCD. A quadratic algorithm is fine for small p.
  For the largest exponents it needs `issues/large-gcd-and-jacobi.md`, or the check runs rarely.
- Shaping question: a Gerbicz-protected LL, run as ω^(2^i) in Z[√3]/(M_p) with ω = 2 + √3, costs
  about twice a plain LL. Is it worth it for confirming primes, against two independent shifted runs?
  No published analysis was found.
- High-blast-radius: the verdict path and `src/io.*` (§ Invariants 1 and 2).
- References: D. H. Lehmer, "An extended theory of Lucas' functions", *Annals of Mathematics* 31(3),
  1930, doi:10.2307/1968235; D. H. Lehmer, "On Lucas's test for the primality of Mersenne's numbers",
  *J. London Math. Soc.* s1-10, 1935, doi:10.1112/jlms/s1-10.2.162; J. W. Bruce, "A really trivial
  proof of the Lucas–Lehmer test", *Amer. Math. Monthly* 100(4), 1993, doi:10.2307/2324959; GIMPS,
  "The Math", https://www.mersenne.org/various/math.php.
- **Stop `-t all` at the first error** (deferred from `verdicts-and-exit-statuses`, review cycle 1,
  2026-10-12). `run_in_order` (`src/math.cpp`) runs the next test after one returns an error, and
  the run exits 20 with the later tests' no-verdict lines printed. The contract allows that, since
  an error is not decisive and no decisive verdict prints. With stubs it costs nothing. Once
  Lucas-Lehmer is real, a run that has already failed should not spend days on the tests after it.
  This cycle decides whether an error ends the run, and changes the stop rule and its checks
  together.
- **Pin the verdict path's guards in `make check`** (deferred from `verdicts-and-exit-statuses`,
  review cycle 2, by Geoffrey's decision, 2026-10-12). The behaviour is right and was verified
  exhaustively. What's missing is regression protection, and this cycle, the first with real
  verdicts, reworks that path anyway:
  - **G1.** The exponent check's exact re-check is pinned in `check_exponent_by` only.
    Rewriting `check_exponent`, or building the composite in `src/main.cpp`, stays green.
    Pinning that needs a fault on the production path, a search that lies inside `bin/mps`.
  - **G2.** `utf8_length`'s table bounds in `src/result.cpp` are under-pinned. These pass
    `make check` and produce invalid records:
    - E0 or F0 taking an overlong second byte;
    - leads F5–F7;
    - unchecked third or fourth bytes.
    Check every lead and continuation boundary against an independent validator.
  - **G3.** `verdict_line` writes a factor as it is, and so does the contradiction line on
    stderr. Put factors, and every string on those lines, through `line_text`, as reasons
    are.
- Found by: factory adoption review, 2026-10-11.
