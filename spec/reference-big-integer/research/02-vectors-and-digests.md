# Research 02 — The vectors: format, generation, compactness, digests

The prototype is in the session scratchpad, `c7-plan/r02/proto/` (`gen.py`, `reader.cpp`), and is not committed. It
generated four families, which a C++ reader parsed and checked: 0 mismatches over 462 add/mul/sqr records, including
2^20-bit products by digest, and the trailers verified. Measured on an Apple-silicon Mac with Apple clang 21, 2026-10-12.

## 1. Format

The files are line-oriented ASCII with LF line ends: no CR, no tabs, no blank lines, single spaces between fields.
There is one file per family in `tests/vectors/`.

```
file     = magic command 1*source *note 1*record trailer
magic    = "# mps vectors 1" LF
command  = "# command: python3 tests/vectors/generate.py " stem " > tests/vectors/" stem ".txt" LF
source   = "# source: " text LF
note     = "# " text LF                      ; only before the first record
record   = op 1*(SP field) LF                ; op's arity is fixed (table below)
trailer  = "# end " count " records " sha LF ; last line. sha = SHA-256 of every byte before it
operand  = "x:" hex | "u:" lo "-" hi *("," lo "-" hi)       ; literal | OR of bit ranges [lo,hi)
         | "r:" bits ":" seed | "c:" bits ":" seed           ; random | carry-pattern, exactly `bits` long
value    = "x:" hex | "h:" bits ":" low16 ":" sha            ; full | digest of the canonical limb bytes
string   = "s:" chars | "g:" length ":" sha                  ; full | digest of the ASCII string
hex      = "0" | [1-9a-f] *[0-9a-f]          ; lowercase, no leading zeros
```

The ops, with operand fields written A, B, X, M and E: `cmp A B lt|eq|gt`, `add A B value`, `sub A B value`
(A ≥ B), `shl A k value`, `shr A k value`, `mul A B value` (checked by schoolbook and by Karatsuba), `sqr A value`
(both), `fold p X value`, `mod X M value`, `powmod A E M value`, `res64 p X HHHHHHHHHHHHHHHH`, `hex A string`,
`dec A string`, `ll p x:…`, `prp p x:…` and `mersenne p`.

The `u:` form expresses every structured edge with no arithmetic. M_p is `u:0-p`, 2^p is `u:p-(p+1)`,
2^p·M_p is `u:p-2p`, and (M_p−1)^2 = 2^2p − 2^(p+2) + 4 is `u:2-3,(p+2)-2p`. A top limb of 1 is `r:(64n+1):s`, and
2^k − 1 + 1 or 2^k − 1 drive carry and borrow chains across k/64 limbs.

**Rejection.** The reader loads the whole file and fails (`check::require`) on any of these: a missing or empty file,
a last byte that is not LF, a trailer that is not the last line, a wrong count, a SHA-256 mismatch over the
preceding bytes, a malformed header or a header out of order, an unknown op, a wrong arity, or a malformed token.
Each check also requires at least one record of each op it covers, so a family that silently loses its `mul` lines
fails rather than passing vacuously. The trailer hash makes a hand edit of an expected value fail `make check`,
not only the R8 gate (§ Invariants 3). The reader opens the files by relative path, since `make check` runs
`bin/check` from the repo root. No Makefile change is needed: vectors are read at run time, and `tests/vectors/`
holds no `.cpp`.

## 2. Deterministic operands

The generator is SplitMix64, from Steele, Lea and Flood, "Fast splittable pseudorandom number generators", OOPSLA '14
(Proc. 2014 ACM OOPSLA, pp. 453–472, doi:10.1145/2660193.2660195). The citation was checked against Crossref. The
constants were read from Figures 16–17 of the authors' copy (gee.cs.oswego.edu/dl/papers/oopsla14.pdf, p. 464), the
root instance `SplittableRandom(seed)`:

```
seed += 0x9e3779b97f4a7c15   (GOLDEN_GAMMA)
z = seed; z = (z ^ z>>33) * 0xff51afd7ed558ccd; z = (z ^ z>>33) * 0xc4ceb9fe1a85ec53; return z ^ z>>33
```

The paper's `mix64` is the MurmurHash3 finalizer. The widely copied "splitmix64" uses `mix64variant13`
(0xbf58476d1ce4e5b9, 0x94d049bb133111eb, shifts 30/27/31), which the same figure lists as a separate function.
State which one is used; this brief uses the paper's `mix64`. From seed 0 the first output is `9ca066f1a4ab2eea`
in both Python and C++.

`r:bits:s` takes ⌈bits/64⌉ outputs as little-endian limbs, masks the top limb and sets bit bits−1.
`c:bits:s` builds runs of 1–32 limbs, each run all-zero, all-ones or random (v % 3, run 1 + (v>>32) % 32), then
trims the same way. Its long all-ones runs drive carry and borrow chains through add, sub, mul and fold, which
uniform random limbs almost never do. The C++ builds operands into limbs directly through a test-only
`BigInt`-from-limbs entry point, never through the string parser under test. The plan's API needs that entry
point and a limbs accessor.

## 3. Digests

A value field is written in full (`x:`) up to **1024 bits** and as `h:bits:low64:SHA-256` above that. The SHA-256 is
taken over the canonical serialization: the limbs, least significant first, each as 8 little-endian bytes, with no
trailing zero limbs (zero is the empty string). In Python this is `v.to_bytes(8*((v.bit_length()+63)//64),
'little')`. C++ emits the bytes by shifts, with no endianness assumption. String fields digest the ASCII string.
The bit length and low limb are compared exactly, and on a mismatch they are printed for diagnosis.

**Strength.** A wrong `BigInt` passes only if SHA-256(wrong) = SHA-256(right), a second preimage on a structured
input. No generic attack beats about 2^256, and BigInt faults have no algebraic relation to the compression
function. A buggy C++ SHA-256 can only fail checks, never pass them, because the expected digests come from
`hashlib`, an independent implementation. The C++ SHA-256 is checked against the FIPS examples: "abc", "", the
448-bit two-block message, and one million "a". `hashlib` gives the same four digests.

**Alternatives.**
- Residues mod primes q_i miss exactly the errors divisible by Q = ∏q_i. That is the structured class that
  reduction faults produce: a result that is not fully reduced is off by a multiple of M_p or m, and it goes
  unseen whenever some q_i divides M_p (composite M_p have such factors) or m. Ruling that out is an extra proof
  obligation.
- CRC is linear over GF(2) in the difference. It misses every difference divisible by its polynomial and gives
  only 32 or 64 bits. Python has no stdlib CRC-64, so there would be no independent oracle.

**Cost.** C++ SHA-256 hashes the 256 KiB limb bytes of a 2^21-bit product in 0.6 ms at -O3, about 430 MB/s. The
512 KiB hex string of the same product would take about 1.2 ms. Hashing the 158 KB arith file for its trailer takes
under 1.5 ms. Building with `-D_GLIBCXX_ASSERTIONS -D_LIBCPP_HARDENING_MODE=…EXTENSIVE` showed no measurable
difference.

## 4. Size budget (prototype, measured)

| file | records | bytes | generator wall time (3.9.6) |
|---|---|---|---|
| `arith.txt` | 1,804 | 158 KB | 2.2 s |
| `modular.txt` | 364 | 16 KB | 3.1 s |
| `convert.txt` | 177 | 15 KB | 0.3 s |
| `reference.txt` | 56 (28 p × ll, prp) | 43 KB | 12 s |
| `mersenne-exponents.txt` | 22 | < 1 KB | 85 s (LL over all primes ≤ 10⁴) |

The total is about 235 KB.

- **Arith operand sizes.** Limb counts 1–5, 7–9, 15–17, 23–25, 31–33, 47–49, 63–65, 95–97, 127–129 and 255–257,
  each at full top limb, top limb = 1, carry pattern, and n vs n−1 bits. Asymmetric products are 257×1, 257×16,
  129×64, 65×33, 1024×17, 1024×513 and 4096×100 limbs. All-ones, 2^k and 2^k − 1 operands are included at
  k ∈ {64, 128, 4096, 65536, 2^20}. At 2^20 bits there are about 5 pairs (r×r, c×c, (2^20+1)×2^19,
  (2^20−1)×19200, ones×ones).
- **Shifts.** Each operand gets 2 counts: one multiple of 64, and one from {0, 1, 7, 63, 65, 127, 129, 1000}.
  Five operands get the full set {0, 1, 63, 64, 65, 127, 128, 129, 1000, 4096, 2^20}.
- **The 1024-bit threshold.** Raising the full-value cut from 1024 to 4096 bits takes arith from 158 to 360 KB.
  The 4096-bit cut with 7 shift counts per operand gave 773 KB.

**Timing constraints for R10.** A naive schoolbook product costs about 0.25 s at 2^20 × 2^20 bits, and the 10
large products took 2.2 s, so schoolbook should be checked on at most 1–2 products at 2^20 bits and Karatsuba on
all of them. Naive decimal conversion (division by 10^19) costs **1.6 s per 2^20-bit operand**, but about 25 ms at
2^17 bits. Convert vectors should therefore give hex for every size and decimal up to 2^17 bits. R3's "every vector
operand" is a contract reading the plan must settle: either decimal at 2^20 bits is dropped, or `to_decimal` is
subquadratic. Hex costs nothing at any size.

## 5. Reproduction (R8)

The gate runs, for each stem, `python3 tests/vectors/generate.py <stem> > $SCRATCH/<stem>.txt && cmp
$SCRATCH/<stem>.txt tests/vectors/<stem>.txt`, under both `/usr/bin/python3` (3.9.6) and `uv run --python 3.14
python`. The prototype's four families came out byte-identical under 3.9.6, 3.10, 3.12 and 3.14.

- **Decimal conversion limit.** Guard it: `if hasattr(sys, "set_int_max_str_digits"): sys.set_int_max_str_digits(0)`.
  The guard is exercised, because the 2^17-bit decimal has 39,457 digits, past the 4,300-digit default limit.
  Hex conversion and `to_bytes` are exempt from the limit.
- **No `random` module.** Its docs guarantee only `random()` across versions, not `getrandbits` or `randrange`, and
  matching it in C++ would need MT19937 plus Python's `init_by_array`.
- **No hash-order effects.** There is no set or dict iteration over unsorted keys, and the output is written as
  bytes with explicit `"\n"` through `sys.stdout.buffer`.
- **Integer arithmetic only.** Use `math.isqrt` and integer rounding. The prototype's `round(k*(N−1)/K)` should
  become `(2*k*(N−1)+K)//(2*K)`.
- **Append-only seeds.** Give each case a literal seed in the generator's tables rather than a running counter, and
  append new cases at the end of a family. Then adding a case never re-seeds or changes existing records
  (§ Invariants 3).

## 6. The prime list

`tests/vectors/mersenne-exponents.txt` uses the same envelope, with records `mersenne p`:

```
# source: OEIS-A000043 (doc/references.md#oeis-a000043), exponents p <= 10000, read 2026-10-12
# source: recomputed by Lucas-Lehmer in Python ints over every odd prime p <= 10000 (the command above); 2 is from OEIS
```

The generator embeds the 22 OEIS exponents (2 … 9941) and exits non-zero unless its LL sweep finds exactly the odd
ones, so two methods must agree. When `issues/known-answer-tables.md` extends the table to all 52 exponents, rows
above B can no longer be recomputed. That cycle must either keep this file as the bounded, recomputable part or
change the command. The handover point is the file path and the `mersenne p` record.

## 7. The composite set (R6/R7)

The set is every prime 3 ≤ p < 64 with M_p composite: 11, 23, 29, 37, 41, 43, 47, 53 and 59, which also covers R4's
p < 64. To those it adds 17 spread over the range: among the composite-M primes above 64, sorted, indices
⌊(2k(N−1)+K)/(2K)⌋ for k = 0..16, which gives 67, 509, 1033 … 9311, 9973. It also adds 1237 and 1277. That is 28
exponents.

Residues are stored in full (`x:`, at most 2,494 hex digits) for both `ll` (s_{p−2} mod M_p) and `prp`. The PRP
residue in the prototype is type 1, 3^(M_p−1) mod M_p. The plan may pick x_p instead. Full residues cost about
43 KB and give downstream engines full known answers.

The Python LL Res64s match GIMPS-published values (`issues/known-answer-tables.md`): M1237 gives `DF5E6622F83F5BE2`
and M1277 gives `5613A480590E78BA`. This is an outside sanity check of the generator, not a vector.

## Recommendations

1. Put the files in `tests/vectors/`: `arith.txt`, `modular.txt`, `convert.txt`, `reference.txt` and
   `mersenne-exponents.txt`, made by one generator, `tests/vectors/generate.py <stem>`, run from the repo root.
2. Use the envelope from §1: `# mps vectors 1`, then the `# command:` line naming its own stem, one or more
   `# source:` lines, single-space records, and the trailer `# end N records <sha256-of-preceding-bytes>`.
3. Use the operand forms `x:`, `u:lo-hi,…`, `r:bits:seed` and `c:bits:seed`. Rebuild operands into limbs in C++,
   never through the parser under test.
4. Use the paper's SplitMix64 (Steele–Lea–Flood 2014, Fig. 16, MurmurHash3 `mix64`) and pin its seed-0 first
   output in a check.
5. Write values in full up to 1024 bits, and above that as `h:bits:low64:sha256` of the canonical limb bytes.
   Digest strings as `g:len:sha256`.
6. Hand-write SHA-256 in `tests/`, checked against FIPS's abc, empty, 448-bit and million-"a" examples.
7. Keep the budget at about 235 KB in total. Check schoolbook on at most 2 products at 2^20 bits and Karatsuba on
   all. Give decimal vectors only up to 2^17 bits unless `to_decimal` is subquadratic, and settle R3's wording.
8. Give each case a literal seed, use integer arithmetic only, and add the `set_int_max_str_digits` guard. The R8
   gate is `cmp` under 3.9.6 and 3.14.
9. The composite set is the 9 exponents below 64, plus 17 evenly spaced, plus 1237 and 1277: 28 exponents, with
   full LL and type-1 PRP residues.
10. Add `tests/vectors/` (the generator and every `.txt`) to the high-blast-radius list in the commit that creates
    them.
