# PLAN — Squaring modulo 2^p − 1 with a weighted transform (FP64, portable)

> **Status:** Reviewed (adversarial design review folded in, § 4) · **Last updated:** 2026-10-31
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). Backing detail is in
> [`research/`](research/) ([`00-digest.md`](research/00-digest.md)). Every design element traces to
> a GOAL R-ID.

## 1. Summary

A portable FP64 irrational-base discrete weighted transform, built in small cited pieces that are
each checked against their definition: balanced digits of widths ⌈pj/N⌉ − ⌈p(j−1)/N⌉, weights
2^(f_j) computed from exact fractions, a complex mixed-radix Cooley–Tukey transform of every length
m·2^k ≤ 2^20 (odd m ≤ 15) with every root computed on its own, the pointwise square or product, a
carry that wraps the top word into word 0 and subtracts 2^b at any bit, and a monitor that refuses a
squaring or product on four grounds: an output too large (or not a number) for a distance to be
seen, the round-off distance past 7/16, rounded outputs that disagree with exact O(N) checksums of
the inputs, and outputs whose root mean square leaves the length's measured envelope. The last two
were added at the review, which showed the distance alone silently accepts wrong squarings of
structured inputs. A length table set by a committed, reproducible measurement, with a margin below
the first failure, says which length holds which exponent (P_max 20,316,160 in the planner's
prototype). Two check tiers: the default `make check` (+2.4 s over the base in the prototype; budget +5 s) and
`make check-engine` (748 s in the prototype; budget 15 min). The appetite is big and the change is
high-blast-radius and gate-shaped, so the plan is research-backed and its gates are built to fail
red on the defects that matter.

## 2. Design

### 2.1 Modules and files

| Path | What it holds | Phase |
|---|---|---|
| `src/fft.hpp`, `src/fft.cpp` | `fft::Complex` (two doubles), `fft::root`, `fft::is_supported`, `fft::Plan` (forward, unnormalized inverse), `fft_error` | P1 |
| `src/ibdwt.hpp`, `src/ibdwt.cpp` | public: `positions`, `weights`, `inverse_weights`, `Transform`, `Engine` (with `rms_limit`), `Residue`, `require_residue`, `roundoff`, `Statistics`/`sample` (P1); `to_digits`, `from_digits`, `square`, `square_subtract`, `multiply` (P2); `engine_error`, `roundoff_error`, `THRESHOLD`, `MAGNITUDE_LIMIT`, `MAX_WIDTH`, `RMS_MARGIN`. Internal (external linkage, declared only in the `.cpp` and where checked): `convolve`, `convolve_square`, `carry`, `rms` (P1), `checksums_agree` (P2) | P1, P2 |
| `src/lengths.hpp`, `src/lengths.cpp` | `LARGEST_LENGTH`, `SIZING_DISTANCE`, `SIZING_MAGNITUDE`, the protocol constants `PROBE_SQUARINGS`, `PROBE_OUTPUTS`, `GRID_STEPS`, `START_BELOW`, `BACKOFF`; `is_supported`, `supported_lengths`, `largest_exponent`, `holds`, `require_held`, `length_for`, `rms_envelope`; the table as two maps, `largest_exponent_by_length` and `rms_envelope_by_length` | P1 |
| `src/lengths.txt` | the measurement: header (magic, command, protocol), one probe per line, trailer | P1 |
| `src/measure.cpp` | the measurement command: `bin/measure [LENGTH ...]`, `--table FILE`, `--probe LENGTH EXPONENT` | P1 |
| `tests/check.hpp`, `tests/check.cpp` | the runner gains `check::Tier` and `bin/check engine` | P1 |
| `tests/check_ibdwt.cpp` | the default tier's engine checks | P1–P3 |
| `tests/check_engine.cpp` | the engine tier | P1–P3 |
| `tests/engine_support.hpp`, `tests/engine_support.cpp` | what both tiers share; every expected value on `BigInt` | P1–P3 |
| `Makefile` | `MODULES` += `fft ibdwt lengths`; `bin/measure`; `check-engine`; `lengths`; `BUILT`/`DEPS` | P1 |
| `AGENTS.md` | high-blast-radius lines, § Map, § Build and run (R13) | P1 |
| `doc/weighted-transforms.md`, `doc/references.md` | the note's "What mps uses", Sources, Provenance; `CooleyTukey1965`, `SteeleLeaFlood2014` | P1, P2 |

Nothing else changes: `tests/cli.sh`, `src/mp.*`, `src/reference.*`, `src/known.*`, `tests/known/`,
the command line and the profile's `verify` stay the base's (GOAL Non-goals; R12). The gate enforces
the list.

**The base is `0bc49cb`**, the relicense to Apache-2.0 that landed on `main` after the GOAL's
sign-off. It changes only licence text (every SPDX line, `LICENSE`, `LICENSE-EXCEPTION` removed,
AGENTS.md § Invariants 4, the README); its code, tests and `tests/cli.sh` behaviour are
`f956a7c`'s. So R12's "no more than 5 s longer than at `f956a7c`" is measured against `0bc49cb`,
whose code is the same, and `tests/cli.sh` "without edits" means as `0bc49cb` holds it (its SPDX
line is the relicense's). Every new file opens with `// SPDX-FileCopyrightText: 2026 Geoffrey
Lentner` and `// SPDX-License-Identifier: Apache-2.0`.

### 2.2 The pieces

Each is a free function or small class, cited where it is used, and checked against its definition
in the default tier ([research/01](research/01-ibdwt-mathematics.md), [02](research/02-transform.md)).

1. **Digit positions and widths** — `positions(p, N)`: c_j = ⌈pj/N⌉ = (pj + N − 1)/N in integers,
   0 ≤ j ≤ N (Crandall–Fagin eq. (6.5), (6.6)). Widths s_j = c_(j+1) − c_j ∈ {⌊p/N⌋, ⌈p/N⌉}.
2. **Weights** — `weights`, `inverse_weights`: a_j = 2^(f_j), f_j = (N c_j − pj)/N, the integer
   numerator formed exactly and each of a_j and a_j⁻¹ computed on its own by `std::exp2`
   (CF eq. (6.7); Percival's δ). `Transform`'s constructor obtains them only by calling these two.
3. **The transform** — `fft::Plan(N)`: decimation in time, Cooley–Tukey 1965: n = r·m, r
   sub-transforms of the samples r apart, then length-r transforms of the twiddled values written
   back in place. Radices: the odd factor once (by the definition), then 4s, then a 2. ω₄ exact.
   `fft::root(k, n)` reduces 2πk/n to the first octant in integers and calls `cos`/`sin` on at most
   π/4 (Percival: the error is bounded by each root's error; never a recurrence). Complex values
   as two doubles with the products written out (not `std::complex`). The weighted real signal
   enters as complex values with zero imaginary parts; the real-signal transforms CF p. 310
   recommends are speed work (`issues/cpu-transform-throughput.md`).
4. **The pointwise square or product** — in `convolve_square` (one forward transform) and
   `convolve` (two): v_k = a_k⁻¹ · Re(inverse(…))_k / N, the unrounded outputs of Theorem 1, part 3.
5. **The carry** — `carry(engine, outputs, subtract, bit)`: round each v_k; if `subtract`, take
   2^(bit − c_j) from z_j for the word with c_j ≤ bit < c_(j+1); then t = z_j + carry, the digit
   t mod 2^(s_j) in [−2^(s_j−1), 2^(s_j−1)), carry (t − digit)/2^(s_j); the top carry into word 0
   and on until it is 0, bounded by a "does not settle" error. int64 throughout (no `__int128`).
   It takes an `Engine`, not a `Transform`, so no residue is ever formed at an exponent the table
   does not let its length hold (R8).
6. **The monitor** — `roundoff(outputs)` = max |v − round(v)|, NaN if any output is not finite
   (`std::max` would drop a NaN). The operations refuse with `roundoff_error`, residue and operands
   untouched, in this order: (a) any output not finite or |v| ≥ `MAGNITUDE_LIMIT` = 2^51, where
   neighbouring doubles are half an integer apart; (b) the distance past the threshold (R7);
   (c) `checksums_agree` false; (d) `rms(outputs)` > `Engine::rms_limit()` = `RMS_MARGIN` (2^(1/2))
   × `rms_envelope(N)`. Why (c) and (d): above about 2^48 an error every output shares (structured
   inputs: constant, alternating, periodic, tones) passes one half on every output at once and
   aliases to a reported distance of 0.25–0.4375; the review found 99 such silent wrong squarings
   at table maxima ([research/06](research/06-review-guards.md)).
   - **(c) Exact checksums.** By Theorem 1, z_k = Σ_{i+j≡k} x_i y_j 2^(e_ij) with e_ij = 1 exactly
     when f_i + f_j ≥ 1, i.e. n_i + n_j ≥ N for the integer numerators n_j = N c_j − pj. For a root
     of unity u of order d | N, u^((i+j) mod N) = u^i u^j, so
     Σ_k u^k z_k = (Σ u^i x_i)(Σ u^j y_j) + Σ_i u^i x_i S(N − n_i), S(t) = Σ_{n_j ≥ t} u^j y_j,
     O(N) by bucketing on n_j. Both sides in Z[u] mod 2^64 for d = 1, and 2, 3, 4 where they divide
     N (u = −1, a cube root ω with ω² = −1 − ω, and i). Exact: a correct squaring always agrees, so
     it never refuses a right answer. It sees any error pattern with a nonzero component at those
     characters: a shared error, alternation, period 3 or 4.
   - **(d) The envelope.** By Parseval the outputs' root mean square is the pointwise product's
     spectrum size over N, and it sets the scale of the errors the inverse transform spreads.
     Pseudorandom residues at a length's largest exponent stay within the measured envelope;
     structured inputs that err have more energy. On the planner's sweep (9 families × 20 fractions
     × every length ≤ 4096 at its maximum) every wrong squarings' rms was at least 2^0.84 times the
     random envelope, and every wrong one the checksums missed (period-8 sawtooths) at least 2^2.02.
   Neither is a proof for every input; together they leave no wrong squaring in either sweep (§ 4).
7. **What the measurement sees** — `sample(transform, x)` returns `Statistics {distance, magnitude,
   rms}` of squaring x; never the outputs, never a residue. The pieces that do square or carry
   (`convolve`, `convolve_square`, `carry`) are internal, so every squaring a caller can ask for goes
   through the monitor; the checks and the gates declare them themselves.
8. **The digit form** (P2) — `to_digits(engine, x)`: x of at most p bits (M_p included) cut into
   words and balanced by the same carry; `from_digits(engine, x)`: any digits under 2^62 in size
   carried into plain words with the wrap, packed into limbs, all ones read as 0, returned through
   `BigInt::from_limbs` as 0 ≤ r < M_p.
9. **The length table** — see 2.4.

### 2.3 The operations and the contract later engines implement

```text
Transform(p, N)                   any supported N, 3 <= p, N <= p, widths <= MAX_WIDTH (50); no table
Engine(p, N, threshold = 7/16)    + require_held(N, p) (R8) and 0 < threshold < 0.5
to_digits(engine, x) -> Residue   R1
from_digits(engine, r) -> BigInt  R1; M_p -> 0
square(engine, r) -> distance                 r <- r^2 mod M_p             R4, R6, R7, R9
square_subtract(engine, r, b) -> distance     r <- r^2 - 2^b mod M_p, b<p  R4 (and the shifted path)
multiply(engine, r, s) -> distance            r <- r s mod M_p             R14
```

Every operation validates its residue (N balanced digits), computes the outputs, then runs the
monitor (2.2.6), and only then assigns the result: on `roundoff_error` (carrying exponent,
length, distance, threshold; its message names all four) the residue and the operands are as they
were (R7, R14). `engine_error` names the exponent and the length for every refusal (R8). The
engine chooses no shift and holds no state between calls; residues leave it only as `BigInt`
through `from_digits`, and Res64 comes from `src/mp.*`'s `res64`. `Transform` exists so the
measurement can probe exponents beyond the table; what takes a `Transform` returns statistics only
(`sample`), and everything that forms a residue (`carry`, `to_digits`, the operations) takes a
table-checked `Engine` (R8; the foreman ruled this reading sound at the review).

### 2.4 The length table and its measurement (R3)

- **Protocol** (pinned in `src/lengths.hpp`): a probe squares `PROBE_SQUARINGS` (64) or more
  vectors of SplitMix64 pseudorandom balanced digits (seeded by N and p), fresh for each squaring,
  enough for `PROBE_OUTPUTS` (2^22) outputs, through `sample`; the first squaring past a sizing
  limit ends it. It holds when its largest distance ≤ `SIZING_DISTANCE` (0.35) and its largest
  |v| < `SIZING_MAGNITUDE` (2^50). For each length the probes are the exponents ⌈N·i/16⌉
  (`GRID_STEPS`), consecutive, from `START_BELOW` (1) bit a word below the Crandall–Mayer–
  Papadopoulos eq. (8) estimate (C = 2) up to the first failing probe; with fewer than `BACKOFF` + 1
  held probes before it the start moves down a bit and the length is measured again.
- **Table rule:** a length's largest exponent is the probe `BACKOFF` (2) grid points below the last
  held one; its envelope (`rms_envelope_by_length`) is that probe's largest rms. The margin and the
  larger probes answer the review's tail finding: at the old maxima 1.6 % (2^20) and 4.4 % (7·2^17)
  of squarings passed 0.35; at the new ones none of 500 (each of 2^20, 7·2^17) or of 10,000
  (65,536) did, and no off-grid exponent below a maximum did either ([research/06](research/06-review-guards.md)).
- **Files:** `src/lengths.txt` (`# mps lengths 2`, `# command: make lengths (...)`, the protocol
  in words, `<N> <p> <squarings> <outputs> <distance> <magnitude> <rms>` with `%.17g`,
  `# end <n> probes`); `src/lengths.cpp` (the rows `bin/measure --table src/lengths.txt` prints for
  both maps); `src/measure.cpp`. All three in `src/` (the foreman confirmed the placement).
- **Run** (P1, by the builder, on an M2 Mac with clang): `make lengths`, about 42 minutes serial (2,530
  s CPU in the prototype), or `bin/measure <lengths…>` in at most three processes concatenated in
  length order (byte-identical: each length is deterministic; 16 minutes on three), with the header
  and trailer `make lengths` writes. Remeasure whenever the convolution, the weights, the transform,
  `roundoff`, `rms` or the protocol changes.
- **Checks:** the default tier reads `src/lengths.txt` at run time (`the length table holds to its
  measurement`): 144 lengths, ascending probes ending at the first failure, every held probe with
  the protocol's squarings and N outputs each, the table and the envelope from the probe `BACKOFF`
  below the last held, every length assigned an exponent, `length_for` the smallest holder,
  P_max ≥ 10,000,019. The gate re-derives all of it independently (`lengths.py`), reproduces the
  boundary probes byte for byte with the builder's binary, and measures each sample length's maximum
  itself with its own generator and twice the squarings (`probe measure`): a maximum fails when any
  squaring passes 7/16 or more than a sixteenth pass 0.35. That last check is what catches a
  protocol weakened in the code while the data still prints the pinned counts.
- **Planner's prototype result** (not committed; the builder measures its own engine): 2,954 probes,
  monotone, P_max 20,316,160 at 2^20 (was 20,512,768 before the margin).

### 2.5 The two tiers (R12)

`check::Tier { DEFAULT, ENGINE }` on every registered case; `check::Suite(Tier, {…})` registers an
engine-tier table; `bin/check` runs DEFAULT, `bin/check engine` runs ENGINE, any other argument
exits 2 with a usage line; duplicate labels are still caught across both tiers. `make check`'s
recipe is unchanged. `make check-engine` (pinned shape):

```make
check-engine: $(EXE) $(CHECK)
	@status=0; $(CHECK) || status=1; sh tests/cli.sh $(EXE) || status=1; $(CHECK) engine || status=1; exit $$status
```

So `make check-engine` runs every check R1–R9 and R14 name, both tiers. The default tier exercises
every odd m, the subtraction at 0, 1, p − 1 and inside, the shifted path and the product.

### 2.6 The checks

Labels are pinned verbatim (the gates read them). Every expected value comes from `BigInt` or the
tables, never from the engine. "Assigned" = the exponents whose smallest holding length is N.

| Tier | Label | Phase | R-IDs | What |
|---|---|---|---|---|
| default | the weighted transform's positions and weights are Crandall and Fagin's | P1 | R1 | CF's p = 37, N = 4 example; widths, counts, weight range, a·a⁻¹ = 1, a_i a_j = a_k 2^(e_ij) |
| default | the transform matches its definition at every odd factor | P1 | R2 | forward vs the term-by-term DFT; inverse(forward) = N x; m = 1…15 × 2^0…3, powers of 2 to 64 |
| default | the convolution rounds to Theorem 1's integers and the carry balances them | P1 | R1, R4, R6 | exact z_k by Theorem 1 for N ≤ 64; `roundoff` = the outputs' own distance; `carry` balanced and = Σ z_k 2^(c_k) − 2^b on `BigInt` for b = 0, 1, p − 1, p/2 |
| default | every supported length is accepted | P1 | R2 | exactly the 144 lengths supported; engines built up to 2^14 |
| default | the length table holds to its measurement | P1 | R3 | 2.4 |
| default | the engine refuses lengths and exponents it does not hold | P1 (P2 adds) | R8 | lengths 0, 17, 34, 2^21, 19·1024; p over the maximum, below N, below 3 (messages name both); P2: more than p bits, a bit at p, threshold 0.5 |
| default | residues round-trip through the balanced digits | P2 | R1 | sample lengths, both assigned ends: 0, 1, M_p − 1, M_p, pseudorandom; vectors at both ends of the range; all-ones → 0; digits checked balanced and valued on `BigInt` |
| default | squarings and products equal BigInt's at every odd factor | P2 | R4, R14 | each odd m at k = 0, 1, 2, 10: square, less 2^0, 2^1, 2^(p−1), a random bit; a product |
| default | the round-off monitor reports and refuses past the threshold | P2 | R6, R7, R14 | reported = the outputs' own distance (computed in the check); threshold half of it → `roundoff_error` with p, N, distance, threshold, nothing changed, for square, subtract and product; top-of-range digits refused for their distance somewhere |
| default | the engine repeats itself bit for bit | P2 | R9 | fresh engines, identical digits and distances |
| default | structured inputs are refused or squared right | P2 | R6, R7, R4 | the checksum piece (exact outputs agree; one output off, ±1 alternating, period-3 and period-4 patterns each disagree), `roundoff`/`rms` NaN on a NaN; 9 families × 4 fractions at the sample lengths: refused with nothing changed, or equal `BigInt` in full, and refused whenever the rms the check computes passes the engine's limit |
| default | shifted chains reproduce the partial residues | P3 | R5 (sample) | partial rows p ≤ 10,303, k ≤ 1,000: at the table's length, the next length, shifted by p/3 + 1 |
| engine | every supported length is accepted at both its assigned exponents | P1 | R2 | engines and a squaring's outputs within the limits at all 144 lengths |
| engine | residues round-trip at every supported length | P2 | R1 | as the default check, all 144 lengths |
| engine | squarings equal BigInt's at every supported length | P2 | R2, R4 | 2 residues × 5 variants × both assigned ends × 144 lengths; prints a residue digest |
| engine | products equal BigInt's at every supported length | P2 | R14 | 2 pairs × both ends × 144; digest |
| engine | the round-off monitor reports and refuses at every supported length | P2 | R6, R7, R14 | all 144 lengths; exact-output lengths (N = 1, 2) excepted by count |
| engine | the engine repeats itself at every supported length | P2 | R9 | all 144 |
| engine | structured inputs are refused or squared right at every supported length | P2 | R6, R7, R4 | 9 families × 10 fractions up to 2^14 words, constant and alternating × 2 fractions above |
| engine | squaring chains reproduce the known partial residues | P3 | R5 | every partial row p ≤ P_max; base, next length, shifted; digest |
| engine | squaring chains reproduce the known final residues | P3 | R5 | every final row p ≤ 100,069 (ll start 4, p − 2 steps; prp1 = 3^(2^p) / 9 on `BigInt`); base, next, shifted; digest |
| engine | Lucas-Lehmer chains end in zero for the Mersenne prime exponents | P3 | R5 | every listed 3 ≤ p ≤ 100,000 at the table's length |

Every differential check compares the **full** residue against `BigInt` (`==` on `BigInt`); only
the chains compare Res64, because the tables hold only Res64. R9's cross-compiler half: the engine
tier prints `   residues <label>: <sha256>`; the gate compares the clang and GCC lines. R10, R11 and
R13 are graded by the gates (§ 6).

### 2.7 AGENTS.md, the note, the bibliography (R13)

- **AGENTS.md, in P1's commit:** high-blast-radius lines naming `` `src/fft.hpp`, `src/fft.cpp` ``,
  `` `src/ibdwt.hpp`, `src/ibdwt.cpp` ``, `` `src/lengths.hpp`, `src/lengths.cpp` `` and
  `` `src/lengths.txt`, `src/measure.cpp` `` (one line each, with a parenthesis saying what it is);
  § Map bullets for the three modules, the data and the command, and `tests/check_engine.cpp` and
  `tests/engine_support.*` in the `tests/` bullet; § Build and run lines for `make check-engine`
  and `make lengths`. No existing line removed; the profile's `verify` row unchanged.
- **The note, `doc/weighted-transforms.md`:** "What mps uses" rewritten as built — the pieces by
  name (P1: `ibdwt::positions`, `weights`, `carry`, the transform with `CooleyTukey1965` and
  Percival's root error; P2 adds `ibdwt::to_digits`, `from_digits`, `square`, `square_subtract`,
  `multiply`, the threshold 7/16, the four refusals, the checksums' identity and the envelope, and
  that neither is a proof for every input), the sizing (the Crandall–Mayer–Papadopoulos link as
  what chose the grid's start; measurement, not a bound; the margin below the last held probe; it
  covers pseudorandom residues, what LL and PRP runs produce, and says nothing about the worst
  inputs, against which the engine has only its refusals), and where it lives (`src/lengths.txt`, `src/lengths.cpp`,
  `make lengths`, `make check-engine`). **It states no measured value** and no table value; the
  stale "does not exist yet" and "keeps decimal digits" sentences go. Unmarked numbers are allowed
  only as these definitions and format constants, in their defining form: "odd m up to 15" (or
  m ≤ 15), 2^{20}, 2^{50}, 2^{51}, 2^{52}, 2^{64}, 2^{1/2}, 0.35, 7/16, "53 bits", "64-bit",
  "radix-2", "radix-4", "word 0", "Theorem 1", and 0, 1, 2 inside a formula; any other number or
  number word in an added line — inside code spans, in exponent or hexadecimal form, or the same
  digits in another place ("about 20 bits") — is marked per `doc/README.md` (with Geoffrey's
  2026-10-16 ruling) or dropped. No value of the table appears in any form, marked or not;
  citations' locators are not read. `notes.py claims`
  must pass (it refuses unmarked integers ≥ 100, hence 7/16 for 0.4375). Sources gains
  `CooleyTukey1965`; Provenance says the engine is built from the papers and the table is the
  project's own measurement.
- **`doc/references.md`:** entries `CooleyTukey1965` (Math. Comp. 19(90), 297–301,
  10.1090/S0025-5718-1965-0178586-1) and `SteeleLeaFlood2014` (OOPSLA '14 / SIGPLAN Notices
  49(10), 453–472, 10.1145/2660193.2660195), in key order, each Verified by the builder against
  the DOI's metadata; every "Title", year citation in the engine's comments resolves to an entry.

### 2.8 Pinned for the gates

The gates build against these, wrap them and trace them; anything else is the builder's.

- **Declarations** (exact text, whitespace aside), as `verify/files.py` `DECLS` lists them per
  phase: every signature in 2.1–2.3, the constants `THRESHOLD = 0.4375`, `MAGNITUDE_LIMIT =
  2251799813685248.0`, `MAX_WIDTH = 50`, `LARGEST_LENGTH = 1'048'576`, `SIZING_DISTANCE = 0.35`,
  `SIZING_MAGNITUDE = 1125899906842624.0`, `LARGEST_ODD_FACTOR = 15`, the members
  `exponent, length, distance, threshold` of `roundoff_error`, `struct Residue {
  std::vector<std::int64_t> digits; }`, `struct Complex { double re = 0; double im = 0; }`.
- **Definitions**, each defined once, unindented, its whole first line exactly (`verify/files.py`
  `DEFS`): `fft::root`, `ibdwt::weights`, `ibdwt::roundoff`, `ibdwt::carry`, `ibdwt::convolve`,
  `ibdwt::convolve_square`, `ibdwt::require_held` (P1); `ibdwt::to_digits`, `ibdwt::from_digits`,
  `ibdwt::square`, `ibdwt::square_subtract`, `ibdwt::multiply`, `ibdwt::checksums_agree`,
  `ibdwt::rms` (P2). The gates rename and wrap all but the convolutions, which the checks and the
  probe declare by these signatures. The engine reaches the weights, the carry, the distance, the
  checksums, the rms and the table only through these functions. `convolve`, `convolve_square`,
  `carry`, `checksums_agree` and `rms` appear in no header under `src/`.
- **Full residues.** Every differential check grades the full `BigInt` residue (chains: Res64, the
  tables' only form).
- **The tally line.** A check's verdict over its cases is a tally (`engine_support::Tally`): its
  `expect_none` first prints `   tally <what>: <K> cases, <W> disagree`, passing or failing, then
  makes one `check::expect` whose text is `all <K> <what> to agree (<W> did not)`. Under a sabotage
  every graded check must print W = K > 0 on every one of its tally lines: a check that calls a case
  but grades only part of it (the review's res64-only attack) leaves W < K, and a tally that grades
  nothing the sabotage changes prints W = 0 instead of passing unseen beside a failing one.
- **Labels** (2.6), each in its tier's table: `static check::Suite` / `check_by_name { … };` in
  `tests/check_ibdwt.cpp`, `engine_check_by_name {check::Tier::ENGINE, { … }};` in
  `tests/check_engine.cpp`.
- **Every check runs every case**, as the repo's checks do (print the first few that disagree, then
  expect none): a row's three chains all run whatever the first gives. The gates take the engine
  tier's trace from the sabotaged run, where every case disagrees, so a check that stops at its
  first failure leaves cases untraced and fails the coverage.
- **The measurement's format, the protocol constants, and `bin/measure --probe N p`**, printing one
  line exactly as the data holds it.
- **The Makefile's `check-engine` recipe** (2.5), the `lengths` target on `$(MEASURE)`, `$(MEASURE)`
  in `BUILT`, both targets `.PHONY`, `IEEE`/`CXXFLAGS`/`CPPFLAGS`/`BOUNDS` and the `check` recipe
  unchanged.
- **The note's required names and exempt numbers** (2.7) and the two reference keys.

### 2.9 Requirement → design map

| R-ID | Design element(s) | Graded by |
|------|-------------------|-----------|
| R1 | `to_digits`/`from_digits` (2.2.7); balanced vectors at both ends; all-ones → 0 | default + engine round-trip checks; probe; sabotage |
| R2 | `fft::Plan` for every m·2^k ≤ 2^20 (2.2.3); `is_supported`; engines at all 144 | transform check; both "accepted" checks; engine squarings at every length; trace |
| R3 | 2.4: protocol, margin, files, `make lengths`, P_max ≥ 10,000,019 | table check; gate `lengths.py` (counts, rule, envelope); boundary probes re-measured; the gate's own measurement at the maxima; overshoot mutant |
| R4 | `square`, `square_subtract` (2.3); carry's subtraction (2.2.5) | default + engine squaring checks; probe; mutants; trace (bits 0, 1, p − 1); sabotage |
| R5 | engine-tier chains: partial ≤ P_max, final ≤ 100,069, zeros ≤ 100,000; next length; shifted | P3 checks; trace counts; sabotage |
| R6 | every operation returns its distance (2.2.6) | monitor checks compute the distance themselves; probe; silent-monitor mutant |
| R7 | threshold refusal, residue untouched, error names p, N, distance, threshold; the magnitude, checksum and envelope refusals | monitor and structured checks; top-of-range negative control; probe's structured families; no-checksum, no-envelope mutants |
| R8 | `Engine` → `require_held`; `engine_error` naming both | refusal check; probe; no-table mutant |
| R9 | deterministic code, explicit arithmetic (2.2.3); residue digests | repeat checks; probe; clang vs GCC digests (engine tier and probe); byte-identical re-measurement |
| R10 | no flag change; IEEE on every object | gate: compile lines, flag/pragma scan with digraphs normalized, no fused multiply-add in any engine object (disassembly, both compilers) |
| R11 | standard C++20 only; no `__int128`, intrinsics, threads | gate: scan, `-pedantic-errors -Werror` on both compilers, both builds pass both tiers |
| R12 | two tiers (2.5); budgets | gate: interleaved timing vs the base (`0bc49cb`, `f956a7c`'s code); timed `make check-engine`; `tests/cli.sh` untouched |
| R13 | 2.7 | gate: `files.py agents`, `docs.py`, `notes.py claims`; P4 audit: the same-commit history |
| R14 | `multiply` (2.3) | product checks; probe; product-square mutant; trace; sabotage |

## 3. Invariant gate (AGENTS.md constitution check)

Walked before research (from the GOAL) and again on this design. Sections touched: § High-blast
list, § 2 (and § 2's flags), § 3, § 4, § 5, § Conventions. § 1 is not touched: nothing on the
command line, no verdict, `-t` still runs the stubs (R12's `tests/cli.sh` unchanged).

- **High-blast-radius list** — the change adds an arithmetic engine, so its sources, the table and
  its data join the list in the commit that creates them (P1), and trigger 2 fired (full research).
  A CONFIRMED review finding in them goes to Geoffrey.
- **§ 2 Arithmetic is checked, not trusted** — tested against `BigInt` on the known answers at every
  supported length (R4, R5, R14), with differential checks arriving with the engine; the monitor
  measures every squaring and product and treats an excess as a fault (R6, R7), plus the magnitude
  refusal where an excess would be invisible; the retry at a larger length binds the drivers, not
  this engine (Geoffrey, 2026-10-22); IEEE flags unchanged and enforced (R10); deterministic (R9).
- **§ 3 Known answers** — expected values come from `tests/known/` through `src/known.*`; no row is
  added or edited (the gate forbids any change there); the measurement is not a known answer (it is
  the engine's sizing data, reproducible by its command).
- **§ 4 Clean room** — papers and the repo's notes only, recorded in `research/` with what each
  backs; no program's source opened; comments cite papers (author, title, year).
- **§ 5 Dependency-free core** — standard C++20 and GNU make only; `bin/measure` and the tests use
  nothing outside the repo.
- **§ Conventions** — module pairs, code style, no spec ids in source, the note updated in the same
  commit as the algorithm, the profile's `verify` unchanged, point-to-a-rule (the sizing constants
  live once, in `src/lengths.hpp`; the note points at the data).

### Deviation justifications

Re-read against every R-ID's literal wording (2026-10-22, and again 2026-10-31 after the review's
fold): no row changes what an R-ID requires. Row 1 adds refusals R7 does not forbid, and R4, R5 and
R14 still demand the right residue at both assigned ends and in every chain, so a false refusal
there fails their checks rather than bending them; row 2 keeps R3's measured distance ≤ 0.35 at
each maximum (the maximum is itself a held probe); row 3 returns no residue past the table (R8).

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| Three refusals beyond R7's: an output not finite or ≥ 2^51; rounded outputs disagreeing with exact checksums of the inputs; the outputs' rms past `RMS_MARGIN` × the length's measured envelope. Each raises `roundoff_error` with the residue untouched (added complexity; R7 unchanged — it still raises past the threshold, first in order after the magnitude) | The distance is blind above 2^52 and aliases whenever an error every output shares passes one half; the review found 99 silent wrong squarings of structured inputs at table maxima, the planner's prototype 47 even after the sizing margin. Invariant 2 needs an excess treated as a fault, and these excesses cannot be seen as a distance | The threshold alone returns wrong residues; sizing every length by the worst input (N·2^(2β) < 2^51, Percival) would cut P_max far below 10,000,019 and is a GOAL non-goal; the spectral peak alone (the reviewer's suggestion) separated less well than rms with checksums on the sweep (min 2^1.03) |
| The sizing rule is stricter than R3's ≤ 0.35, which it keeps: magnitude < 2^50, at least 64 squarings and 2^22 outputs a probe, and the maximum `BACKOFF` grid points below the last held probe | Keeps pseudorandom residues inside the 2^51 refusal, and long runs at a maximum away from the threshold (the review's tail finding) | Distance alone on a 16-squaring probe at the last held point: 4.4 % of squarings past 0.35 at 7·2^17's maximum, and a probe passing by luck about half the time |
| A `Transform` layer under `Engine` that takes exponents beyond the table, with only `sample` on it (statistics; the convolutions and the carry are internal, the carry, the conversions and the operations take an `Engine`) — added complexity; R8 unchanged | R3's measurement must probe past every maximum to find it | A measurement switch inside `Engine` would let an engine return residues past the table, against R8 |
| A complex transform of the real signal (twice the work of a real-signal FFT) | One code path for every length, odd N ≤ 15 included; Percival's model applies as is; reviewable | Real-signal or right-angle transforms add a second path and untangling twiddles for speed the GOAL does not ask for (Non-goals: speed) |

## 4. Rabbit holes (resolved)

- **Which FFT for m·2^k, portable and deterministic?** → mixed-radix Cooley–Tukey decimation in
  time, odd factor by the definition, roots computed alone, explicit two-double arithmetic; 0.13 s a
  squaring at 2^20 ([research/02](research/02-transform.md)).
- **Can the monitor be fooled?** → yes, in two ways: above 2^52 it is blind (the 2^51 refusal and the
  2^50 sizing bound close that), and an error every output shares aliases to a small distance (the
  review; the checksums and the envelope close what the sweeps found) ([research/03](research/03-roundoff-and-the-table.md),
  [research/06](research/06-review-guards.md)).
- **What does M_p "read back as 0" mean for balanced digits?** → M_p is not a balanced vector's
  value; `from_digits` takes any digits, and all-ones reads 0
  ([research/01](research/01-ibdwt-mathematics.md) § 3).
- **How is the table measured, at what cost, and how is it checked?** → the grid-and-first-failure
  rule, about 42 min serial on an M2 Mac after the review's larger probes (2,530 s CPU; 16 min on
  three workers), run once in P1; the gate re-runs every boundary probe
  ([research/03](research/03-roundoff-and-the-table.md) §§ 3–6).
- **Does the heavy tier fit 15 minutes, and the default tier 5 s?** → 7 min 43 s and +0.7 s in the
  prototype before the review; 748 s and +2.4 s after it, with the structured checks in both tiers
  (P3 dry run on the `0bc49cb` base, load 6.6) ([research/04](research/04-checks-budgets-and-gates.md)).
- **How do gates prove the engine computed each result?** → wrapped pinned definitions: a trace of
  every call against what R1, R4, R5 and R14 name, a sabotage of every result that every graded
  check must fail, and the gate's own probe computing on `BigInt`
  ([research/04](research/04-checks-budgets-and-gates.md)).
- **Where do the table's data and command live?** → `src/`, beside the table, all three on the
  high-blast list ([research/05](research/05-repo-integration.md)).
- **The planner's dry runs** (before the review) changed the design and the gates: `carry` now takes
  an `Engine` and the measurement squares fresh digits instead of chaining LL steps, so nothing on a
  `Transform` forms a residue past the table (R8; remeasured, same P_max); the first full run found
  the interposer's trace written after its table was destroyed (fixed: the table is built before
  `atexit` registers the dump); and the P1 dry run on its genuine tree found two gate checks that
  assumed the operations exist (the pinned-call check for `carry`/`roundoff` and the default-tier
  sabotage now start at P2) and the probe's missing `<algorithm>` under GCC; the P3 dry run found
  the prototype's chain check skipping the next-length and shifted chains once the first failed,
  which left them untraced under the sabotage (fixed in the prototype; now a pin).
- **Adversarial design review** (trigger 3), run by the foreman on 2026-10-22 against the GOAL,
  this PLAN, the drafted gates and the prototype; its tools and logs are the foreman's. Findings and
  what each changed ([research/06](research/06-review-guards.md) has the evidence):
  1. **HIGH, the monitor aliases** (99 silent wrong squarings of constant and alternating inputs at
     table maxima up to 3,328 words). → The exact checksums and the rms envelope (2.2.6); the
     probe's structured families graded "refused or equal to `BigInt`"; the structured checks in
     both tiers; § 3 row 1 and the note corrected. The review's sweep rerun on the revised engine:
     0 wrong of 6,400 (47 with the two guards switched off). Real LL and PRP chains at every
     length's maximum: 899,830 squarings, no refusal.
  2. **HIGH, res64-only grading passed a wrong engine** (the full P3 gate, exit 3). → Full residues
     pinned; a second sabotage, `sabotage-top`: every residue read back off by 2^(c_(N−1)), the top
     word's place, so the engine computes right and only bits above the low 64 change (a first
     version that bumped the top digit of every result was dropped: the error reached the next
     squaring's low bits, so a res64-only check still saw every case disagree and passed); every
     graded check must print W = K on every tally line under both (the chains, which grade Res64 by
     design, under the digit-0 sabotage only); every tally prints its line passing or failing, so a
     second, hollow tally beside a failing one shows; the probe compares full residues at every
     length in full mode. Rerun (2026-10-31): the review's attack (`hollow-full`) and the same with
     the large lengths graded in a second tally (`hollow-split`) are each red twice, on the probe's
     full residue at 229,376 words and on the readback sabotage (`tally squarings: 2880 cases, 70
     disagree`; `tally large squarings: 640 cases, 0 disagree`).
  3. **MEDIUM, a digraph `%:pragma` bypassed R10.** → Digraphs normalized before the scans; a
     disassembly check refuses fused multiply-adds in every engine object on both compilers. Rerun:
     red twice (the scan and the disassembly).
  4. **MEDIUM, the protocol was not enforced** (fewer squarings raised 122 maxima, unseen). → Counts
     in every data line, the constants pinned, the gate's own measurement at the maxima. Rerun of an
     honest and a dishonest weakening: both red.
  5. **MEDIUM, the tail at large lengths.** → 64 squarings a probe, the `BACKOFF` margin; P_max
     20,316,160. Rerun of the reviewer's distribution and off-grid checks: none past 0.35.
  6–7. **LOW, docs.py** (code spans, `2e7`, exemptions, citations, table values only). → Fixed;
     the reviewer's slip rerun: red on all four numbers.
  8. **LOW, API paths around the monitor; NaN.** → `convolve`, `convolve_square`, `carry`,
     `checksums_agree`, `rms` internal; NaN and infinities count as an excess.
  The foreman's rulings on the five open questions: `Transform` past the table is sound for R8;
  data and command stay in `src/`; the `#pragma once` exemption stays; 7/16 in the note is fine;
  cluster runs cover the default tier only.
  **2026-10-31, the fold finished** (a resumed plan step): the readback form of `sabotage-top`,
  every tally line printed passing or failing (a hollow second tally beside a failing one was the
  last way past the W = K rule), the review's attacks and the planner's red set rerun on the final
  gates, and each phase gate dry-run quick and full on its genuine tree (§ 6). The guard, the
  protocol and the sizing did not change, so the table was not remeasured: P_max stays 20,316,160,
  and every full dry run reproduced all 288 boundary probes byte for byte.

## 5. Risks & open questions

- **The guards are evidence, not proof.** The checksums see error patterns with a component at
  characters of order 1–4; the envelope bounds energy, not error. Both sweeps (9 families, and the
  reviewer's constant and alternating digits) leave no wrong squaring, and real chains are never
  refused; an adversarial input could still be built. The drivers' Jacobi and Gerbicz checks
  (`issues/lucas-lehmer-test.md`, `issues/prp-gerbicz-test.md`) remain the backstop.
- **False refusals.** The envelope could refuse an ordinary residue whose rms exceeds 2^(1/2) times
  the probe's largest; none did in 899,830 real squarings. A refusal is safe (the driver moves to a
  larger length), never wrong.
- **Long chains near a maximum.** With the margin, none of the planner's long runs at a maximum
  passed 0.375. If the builder's engine trips R7 in a chain, the finding is the sizing (raise the
  probe size or `BACKOFF`, remeasure), never the threshold.
- **An empty assignment.** If the builder's maxima were not monotone, a length could be assigned no
  exponent and R4's "smallest and largest exponent R3's table assigns to N" would be empty. The
  prototype's are monotone; the table check fails a tree where any length is assigned nothing, and
  that would be a GOAL question.
- **Gate time.** A full gate is about 36 (P2) to 47 (P3) minutes, 8 in P1 (two engine tiers, two
  sabotaged ones, the boundary re-measurement and the gate's own measurement); quick mode is about
  3 minutes and is what to iterate on.
- **The magnitude refusal is not isolated by any gate.** Before the review, deleting it turned the
  probe red (top-of-range digits at 2^20, whose outputs pass 2^52); now the envelope refuses those
  inputs first, and an output of 2^51 or more that passed the monitor would still stop at the carry's
  `engine_error`. No input the gates build reaches the limit without another refusal firing, so the
  `no-guard` defect passes. The refusal stays as defence in depth (it keeps the reported distance
  meaningful), pinned by `MAGNITUDE_LIMIT` and the monitor's order; a block input that reaches 2^51
  inside the envelope would isolate it, at the cost of a probe family built for one guard.
- **The engine tier's margin.** 748 s of the 900 s budget in the P3 dry run, timed beside one table
  worker at load 6.6 (457 s before the review added the structured checks). A slower builder engine
  could reach the budget; trimming the engine tier's structured fractions above 2^14 words is the
  first lever, never dropping a length R2, R4 or R14 names.
- **`#pragma once`.** Required by the house style; R11's "extensions" is read as excluding it (every
  header in `src/` uses it). The gate's scan exempts exactly that line.
- **Measurement on another machine.** The data's bytes come from this Mac's clang build; distances
  may differ under another libm. The table check reads the file, not the machine, so `make check`
  stays green elsewhere; only the gate's byte comparison assumes this Mac.

## 6. Verification strategy

**Gates** live in `verify/` from the plan commit and are frozen there: `p1.sh`, `p2.sh`, `p3.sh`
(each `full` by default, `quick` for the cheap partial mode), `p4.sh` (`audit` mode and full),
`prepush.sh` (`scan` mode), `redact.sh`; helpers `common.sh`, `gate.sh`, `cluster.sh`, `files.py`,
`lengths.py`, `trace.py`, `docs.py`, `probe.cpp`, `interpose.cpp`. A gate copies the tree as a clone
holds it and runs, in order:

1. **Files** (`files.py`): only 2.1's paths changed; every pinned declaration, definition and label
   present for the phase; the internal pieces in no header; AGENTS.md; the Makefile; R10's and R11's
   scans, digraphs (`%:`, `<%`, …) spelled out first; no spec ids; the Apache-2.0 SPDX header on every new
   source; `tests/cli.sh` as `0bc49cb`'s.
2. **Builds** with clang++ and g++-16 behind a logging shim; `make check` green on both, same count.
3. **Flags** (R10, R11): every compile of an engine source carries `-fno-fast-math` and
   `-ffp-contract=off` and nothing weakening; no engine object (product and check builds, both
   compilers) holds a fused multiply-add (`otool -tv`: arm64 `fmadd`/`fmsub`/`fnmadd`/`fnmsub`/
   `fmla`/`fmls`, x86 `vfmadd*`/`vfmsub*`/`vfnmadd*`/`vfnmsub*`); every engine source compiles under
   `-std=c++20 -pedantic-errors -Wall -Wextra -Werror` on both compilers.
4. **Probe** (`probe.cpp`, the gate's own driver, `BigInt` only): R8 refusals and the table's
   assignment (P1); from P2 at sample lengths (each m at k = 0, 1, 5, 10; full mode adds each m's
   largest length): round trips, squares and subtractions at 0, 1, p − 1, products, reported
   distances = the outputs' own, refusal leaving operands, repeats, top-of-range inputs refused or
   squared right (refused for their distance somewhere; at 2^20 and 15·2^16 refused); 9 structured
   families × 10 fractions at the sample lengths (full mode: every length ≤ 4,096, and constant and
   alternating at each m's largest), each refused or equal to `BigInt`, and refused past the rms
   limit; in full mode a squaring at every one of the 144 lengths compared with `BigInt` in full.
   Clang and GCC digests equal.
5. **Mutants** (`interpose.cpp`, wrapped pinned definitions, one build): no mutant → default tier
   green; each mutant must fail the labels named here:

   | Mutant | Must fail (P1) | and from P2 |
   |---|---|---|
   | weights | convolution | squarings and products |
   | twiddle | transform | — |
   | carry-drop | convolution | squarings and products |
   | unbalanced | convolution | round trip |
   | subtract-sign | convolution | squarings and products |
   | subtract-place | convolution | squarings and products |
   | silent-monitor | convolution | monitor |
   | no-table | refusals | — |
   | product-square | — | squarings and products |
   | no-checksum | — | structured inputs |
   | no-envelope | — | structured inputs |
   | sabotage (digit 0 of every result, one step inside its range) | — | round trip, squarings and products (P3: + shifted chains), each with W = K on every tally line |
   | sabotage-top (every residue read back off by 2^(c_(N−1))) | — | round trip, squarings and products, each with W = K on every tally line |
   | table overshoot (2^20's maximum set to its first failing probe) | table | — (and `lengths.py` must fail it) |

6. **Engine tier** (full mode): `make check-engine` on clang with nothing to rebuild, timed beside
   one table worker (≤ 900 s, R12); then on GCC beside the interposed copy's engine tier under the
   sabotage (traced) and then the top sabotage; GCC must be green, the clang and GCC residue
   digests equal (R9), the trace must cover every assigned end of every length with conversions,
   squares, subtractions at 0, 1, p − 1 and products (P2) and every chain row at its lengths and
   shifted (P3), and every graded engine-tier check (round trips, squarings, products) must fail
   under both sabotages with W = K on every tally line, and P3's three chain checks under the
   digit-0 sabotage.
7. **Table** (`lengths.py`, then `probe measure`): the measurement's format, grid, counts, scan shape
   and 144 lengths; the table and envelope from the probe `BACKOFF` below the last held; monotone;
   P_max; the boundary probes (all 288 in full mode, 16 in quick) re-measured by the tree's
   `bin/measure --probe` and compared byte for byte; then the gate's own measurement at the maxima
   (all 144 in full mode, 8 in quick).
8. **Time** (full mode, R12): `make check` with nothing to rebuild, `0bc49cb`'s copy against the
   tree's, interleaved, least of 5 each (once more before failing), ≤ +5 s.
9. **Note** (`docs.py`, `notes.py claims`): 2.7.

**Footprint.** At most two jobs at once (two compiles; the clang engine tier beside one table
worker; the GCC engine tier beside the sabotaged runs, one after the other), each in its own process group, killed with
the group on exit; programmatic deletion only of the gate's own `mktemp` directory, asserted.
**Run one gate at a time.**

**R12 baseline** (measured 2026-10-22 on an M2 Mac, clang, `make check` with nothing to rebuild,
f956a7c): least of 10 runs 3.754 s (median 3.770 s, load ≈ 4.6). The revised prototype, from the
P3 full dry run's interleaved timing on the `0bc49cb` base: `make check 3.737 s at 0bc49cb, 6.111 s
now, +2.374 s (budget +5 s; least of 5 interleaved runs each; load 6.6)`; its `make check-engine`:
748 s on clang beside one table worker (budget 900 s).

**Planner's dry runs** (self-test mode, on the prototype in `<git dir>/plan-proto/`, never
committed), each phase's gate on its genuine phase tree (the prototype cut back to what the phase
delivers), one at a time. Before the review: see § 4. After it, on the final gates (2026-10-31),
run twice: on `f956a7c`, P1 quick 1 m 38 s (97 checks, 9 mutants), P2 quick 2 m 46 s (102, 14), P3
quick 2 m 39 s (103, 14), P1 full 8 m (engine tier 7 s, +0.04 s), P2 full 36 m (532 s, +2.07 s), P3
full 47 m (739 s, +2.38 s); then with the prototype rebased onto `0bc49cb` and the gates' base moved
there, P1 quick 1 m 38 s, P2 quick 2 m 41 s, P3 quick 2 m 51 s, P1 full 8 m (7 s, +0.039 s), P2
full 36 m (523 s, +2.104 s), P3 full 47 m (748 s, +2.374 s); all 288 boundary probes reproduced in
each full run, no check failed. The defects the base move could touch (cli, agents, verify-row,
known-row, label, pin, no-guards, docs-stale, docs-slip) rerun red on the `0bc49cb` gates. Every finding's
attack rerun red (§ 4); the earlier defects rerun against the final gates, 29 of 30 red (weights, carry-drop, unbalanced,
subtract-sign, silent-monitor, no-table, table-overshoot, data-tampered, no-guards, digraph, pragma,
the three docs slips, fast-math, int128, thread, cli, agents, verify-row, known-row, label, pin,
references, hollow-default, the honest and dishonest weakenings, hollow-engine, short-sweep). The
one that stays green is `no-guard`, the magnitude refusal alone deleted: see § 5.

---

*Backing research: [`research/00-digest.md`](research/00-digest.md).*
