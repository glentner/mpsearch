# PLAN — A reference big integer to check every engine against

> **Status:** Draft for review · **Last updated:** 2026-10-12
> **Authoritative technical design.** The *how*. Vision/contract is [`GOAL.md`](GOAL.md);
> the phased executable roadmap is [`TECH.md`](TECH.md). Backing detail is in
> [`research/`](research/). Every design element traces to a GOAL R-ID.

## 1. Summary

`BigInt` in `src/mp.*` becomes a non-negative integer of 64-bit limbs whose operations are free
functions: comparison, sum, difference and shifts; folding modulo M_p and Res64, with `hex64` moved
here from `src/result.cpp`; hexadecimal conversion; schoolbook and Karatsuba products and squares;
long division, general reduction and modular powers; decimal conversion, divide and conquer. A new
module, `src/reference.*`, holds the reference Lucas–Lehmer test and the base-3 PRP test (type-1
residue) for odd p ≥ 3, run by `make check` and nothing else. One Python generator writes the
vectors under `tests/vectors/`, each file carrying the command that reproduces it byte for byte and a
SHA-256 trailer; `bin/check` reads them, rebuilding operands from SplitMix64 seeds and comparing
large results by digest. `doc/reference-arithmetic.md` grows with each phase. Four build phases, each
a vertical slice with `make check` green on both compilers, then an audit, the deferral ledger and
the cluster runs. Frozen gates recompute every vector in Python, probe the tree's `BigInt` against
Python on cases of their own, trace that every vector record reaches its entry points and that
Karatsuba really recurses, wrap each public function to inject bugs of a known class, and show that
`make check` turns red under each.

## 2. Design

### Files

| Path | Change | Phase | R-IDs |
|---|---|---|---|
| `src/mp.hpp`, `src/mp.cpp` | the decimal stub replaced by the type and its operations below | P1–P3 | R1–R5 |
| `src/result.cpp` | the file-local `hex64` deleted; `#include "mp.hpp"` added; nothing else | P1 | R4 |
| `src/reference.hpp`, `src/reference.cpp` | new: `reference::lucas_lehmer`, `reference::prp3` | P4 | R5–R7 |
| `Makefile` | `MODULES` gains `reference`, exactly `MODULES  := ansi cli io mp math reference result`; nothing else | P4 | R6, R7, R9 |
| `tests/vector_reader.hpp`, `tests/vector_reader.cpp` | new: the vector-file reader, SHA-256 and SplitMix64, test-only | P1 | R8, R11 |
| `tests/check_mp.cpp` | new: the checks of `BigInt` | P1–P3 | R1–R5, R8, R11 |
| `tests/check_reference.cpp` | new: the checks of the reference tests and the prime list | P4 | R5–R8, R11 |
| `tests/vectors/generate.py` and `tests/vectors/<stem>.txt` | new: the generator and the vector families | P1–P4 | R1–R4, R6–R8 |
| `doc/reference-arithmetic.md` | new note, growing with each phase; one line in `doc/README.md`; new entries in `doc/references.md` | P1–P4 | R12 |
| `doc/lucas-lehmer.md`, `doc/prp-and-gerbicz.md` | "What mps uses" names the reference tests | P4 | R12 |
| `AGENTS.md` | § Map lines and the high-blast-radius list, as below | P1, P4 | Invariants 3 |
| `issues/ibdwt-transform-engine.md`, `issues/known-answer-tables.md`, `ROADMAP.md` | the deferral ledger | P5 | — |

Every other file stays as `c9b82ff` has it: `src/main.*`, `src/math.*`, `src/cli.*`, `src/io.*`,
`src/ansi.*`, `src/log.hpp`, `src/exc.hpp`, `src/result.hpp`, `tests/cli.sh`, every existing
`tests/*.cpp` and `tests/*.hpp`, `ci/`, `README.md`. `bin/mps` links `mp.o` and `reference.o`, which
nothing in it calls, and no library (R9).

### The type and its operations (`src/mp.hpp`)

The interface is pinned: the gates and `bin/check` call these names and signatures, and the gates
wrap each free function below by renaming its definition. So each free function is declared in its
header and **defined out of line at file scope in its `.cpp`, its name on the definition's first
line, once** (`BigInt add(const BigInt& a, const BigInt& b) {`), and the code calls these functions
rather than repeating them. `static` helpers in the `.cpp` are free to the builder.

```cpp
class mp_error: public mps_error { … };          // every R5 error; mp.hpp includes exc.hpp

static const std::size_t KARATSUBA_THRESHOLD = 48;           // the builder's measured value, 8 to 256

class BigInt {                                    // little-endian limbs, no high zero limb; empty is 0
public:
	BigInt();                                       // 0
	BigInt(std::uint64_t value);
	static BigInt from_limbs(const std::vector<std::uint64_t>& limbs);   // drops high zero limbs
	const std::vector<std::uint64_t>& limbs() const;
	bool is_zero() const;
	std::size_t bit_length() const;
};

// x = quotient * m + remainder, 0 <= remainder < m
struct Division {
	BigInt quotient;
	BigInt remainder;
};

int compare(const BigInt& a, const BigInt& b);                          // -1, 0, 1      P1
bool operator==(const BigInt& a, const BigInt& b);                      //               P1
BigInt add(const BigInt& a, const BigInt& b);                           //               P1
BigInt subtract(const BigInt& a, const BigInt& b);                      // a < b: error  P1
BigInt shift_left(const BigInt& a, std::size_t bits);                   //               P1
BigInt shift_right(const BigInt& a, std::size_t bits);                  //               P1
BigInt mersenne(long unsigned p);                                       // 2^p - 1       P1
BigInt reduce_mersenne(const BigInt& x, long unsigned p);               // p = 0: error  P1
std::string hex64(std::uint64_t value);                                 //               P1
std::uint64_t res64(const BigInt& x, long unsigned p);                  // p = 0: error  P1
std::string to_hex(const BigInt& x);                                    //               P1
BigInt from_hex(const std::string& text);                               // error         P1
BigInt multiply_schoolbook(const BigInt& a, const BigInt& b);           //               P2
BigInt multiply_karatsuba(const BigInt& a, const BigInt& b,
                          std::size_t cutoff = KARATSUBA_THRESHOLD);    //               P2
BigInt multiply(const BigInt& a, const BigInt& b);                      //               P2
BigInt square_schoolbook(const BigInt& a);                              //               P2
BigInt square_karatsuba(const BigInt& a, std::size_t cutoff = KARATSUBA_THRESHOLD); //   P2
BigInt square(const BigInt& a);                                         //               P2
Division divide(const BigInt& x, const BigInt& m);                      // m = 0: error  P3
BigInt reduce(const BigInt& x, const BigInt& m);                        // m = 0: error  P3
BigInt power_mod(const BigInt& base, const BigInt& exponent, const BigInt& m); // m = 0  P3
std::string to_decimal(const BigInt& x);                                //               P3
BigInt from_decimal(const std::string& text);                           // error         P3
```

`src/reference.hpp`, P4:

```cpp
namespace reference {
	BigInt lucas_lehmer(long unsigned p);   // s_(p-2) mod M_p; p even or p < 3: error
	BigInt prp3(long unsigned p);           // 3^(M_p - 1) mod M_p; p even or p < 3: error
}
```

Defined in `src/reference.cpp` at file scope as `BigInt reference::lucas_lehmer(long unsigned p) {`
and `BigInt reference::prp3(long unsigned p) {`, so the gates can wrap them too.

`unsigned __int128` is spelled once, behind an alias in `mp.cpp`; there are no `__int128` literals
and no reliance on its traits or `numeric_limits` ([03](research/03-time-and-portability.md) § 4).
Normalization and bit lengths use `<bit>` (`std::countl_zero`). Indexing stays `std::vector`'s
`operator[]`: the bounds hardening of `make check` costs at most 16 % here.

### The algorithms ([01](research/01-algorithms-and-sources.md); each comment cites its source)

- **Comparison, sum, difference.** Limb by limb with a carry or a borrow held in a `u128` sum or by
  explicit comparisons; the result is trimmed of high zero limbs. A difference below zero throws
  `mp_error` and returns nothing.
- **Shifts.** By `bits / 64` whole limbs and `bits % 64` bits, never shifting a limb by 64: the
  `bits % 64 == 0` case is its own branch. A right shift past the length gives 0.
- **Schoolbook product** (MCA Algorithm 1.2; Knuth 1997, § 4.3.1), one row per limb with the carry in
  a `u128`. **Schoolbook square** in the symmetric form of MCA § 1.3.6: the cross products
  Σ_{i<j} a_i a_j β^(i+j) once, doubled by a whole-number shift, plus the diagonal Σ a_i² β^(2i).
- **Karatsuba** (Karatsuba and Ofman 1962; MCA § 1.3.2), the additive variant for both, split at k =
  ⌈max(n_a, n_b)/2⌉ limbs, a high part 0 where an operand has at most k limbs: ab = z2 β^(2k) + z1
  β^k + z0 with z0 = a0 b0, z2 = a1 b1, z1 = (a0 + a1)(b0 + b1) − z0 − z2, and z1 = a0 b1 + a1 b0 ≥
  0, so no subtraction goes below zero. `multiply_karatsuba(a, b, cutoff)` recurses into itself and
  falls back to `multiply_schoolbook` when the smaller operand has fewer than max(cutoff, 4) limbs
  (4 keeps the recursion shrinking); `square_karatsuba(a, cutoff)` likewise with
  `square_schoolbook`. `multiply` and `square` dispatch: Karatsuba at `KARATSUBA_THRESHOLD` limbs
  and up in the smaller operand, schoolbook below. The threshold is the builder's to measure on the
  code as written, from 8 to 256 limbs: 48 was best for research's prototype
  ([03](research/03-time-and-portability.md) § 3), and an implementation that allocates more may
  find it higher. The `products` family's sizes follow it, and the gates read it from `mp.hpp`.
- **Folding** (the note's own derivation; GIMPSMath and MCA § 2.4.4 as context): while
  `x.bit_length() > p`, x ← (x mod 2^p) + ⌊x / 2^p⌋; then x = M_p becomes 0. Each step lowers x by
  ⌊x/2^p⌋·M_p > 0, so it ends with x ≤ M_p, never strictly between M_p and 2^p; for x < 2^(2p) there
  are at most two folds. The mask at a non-limb boundary branches on p mod 64 = 0. `p = 0` throws.
- **Res64** is the low limb of `reduce_mersenne(x, p)`, 0 for zero; `hex64` prints it, 16 uppercase
  digits zero-padded, the form `src/result.cpp` prints and now calls.
- **Division** (MCA Algorithm 1.6 BasecaseDivRem; Knuth 1997, § 4.3.1): normalize the divisor by its
  leading zeros (a shift of 0 needs no special case through `shift_left`), estimate each quotient
  digit from the top limbs, correct it, subtract. The correction is either MCA's
  compare-before-subtract or Knuth's two-limb refined estimate with the add-back step; the vectors
  force the clamp case, a two-correction case and an add-back case, so either is checked on its rare
  path. A one-limb divisor takes short division. `reduce(x, m)` is `divide(x, m).remainder`.
- **Powers** (MCA Algorithm 2.12): reduce the base, then left to right over the exponent's bits,
  square and multiply, reducing each time; exponent 0 gives 1 mod m, and m = 1 gives 0 first.
- **Hexadecimal** both ways, linear: 16 digits per limb.
- **Decimal input** (MCA Algorithm 1.23): chunks of 19 digits, x ← x·10^19 + chunk (the first chunk
  shorter).
- **Decimal output, divide and conquer** (MCA Algorithm 1.26): powers P_i = 10^(19·2^i) by squaring
  up to the first whose square exceeds x; split x by `divide(x, P_k)`; the high part unpadded, the low
  part padded to 19·2^k digits; at the bottom, one division by 10^19 and two limbs printed. A 2^20-bit
  operand converts in 0.17 s, against 1.6 s for repeated division by 10^19, which R10's budget cannot
  hold for every vector operand (R3) ([00](research/00-digest.md), item 7).

### The canonical strings (R3)

- **Decimal:** the digits `0`–`9` only, no sign, no leading zero, and `0` for zero.
- **Hexadecimal:** `0`–`9` and `A`–`F`, uppercase like `hex64`, no prefix, no leading zero, and `0`
  for zero.
- **Parsing** accepts a nonempty string of digits of the base, leading zeros and (hexadecimal) either
  case included, and throws `mp_error` for anything else: the empty string, a sign, whitespace
  anywhere, `0x`, `_`, a non-ASCII digit, any other character.

So printing the canonical string of a value gives back that string, and parsing any accepted string
and printing it gives the canonical form.

### The reference tests (`src/reference.cpp`, R5–R7)

- `reference::lucas_lehmer(p)`: for odd p ≥ 3 (`doc/lucas-lehmer.md`, "The hypotheses"), s ← 4, then
  p − 2 times s ← (s² − 2) mod M_p, squaring with `square`, reducing with `reduce_mersenne`, and
  adding M_p before subtracting 2 when the reduced square is below 2. Returns s_(p−2) mod M_p, in
  [0, M_p). Any other p throws.
- `reference::prp3(p)`: for odd p ≥ 3 (`doc/prp-and-gerbicz.md`, Theorem 2), the **type-1 residue**
  3^(M_p − 1) mod M_p: M_p − 1 = 2^p − 2 is p − 1 ones and a zero in binary, so x ← 3, then p − 2
  times x ← 3x² mod M_p, then x ← x² mod M_p. It is 1 for a probable prime; x_p of Theorem 2 is
  9 times it (and at p = 3, x_p reduces to 2, not 9). Any other p throws.
- Neither is wired into `mps -t`, `run_in_order` or `Result`.

### The vectors (`tests/vectors/`, R1–R4, R6–R8; [02](research/02-vectors-and-digests.md))

**One generator**, `tests/vectors/generate.py`, Python 3.8+ standard library only, run from the
repository root, writes one family per call to stdout through `sys.stdout.buffer`:

```
python3 tests/vectors/generate.py <stem> > tests/vectors/<stem>.txt
```

It calls `sys.set_int_max_str_digits(0)` when that exists, never uses `random`, sets or unordered
dicts, uses integer arithmetic only, and gives every case a literal seed in its tables. The
families, each landing with the phase that implements its operations:

| stem | records | phase |
|---|---|---|
| `arith` | `cmp`, `add`, `sub`, `shl`, `shr` | P1 |
| `folding` | `fold`, `res64` | P1 |
| `hex` | `hex` for every operand of every family present, and the edge values | P1, regenerated in P2–P4 |
| `products` | `mul`, `sqr` | P2 |
| `modular` | `mod`, `powmod` | P3 |
| `decimal` | `dec` for every operand of every family present, and the edge values | P3, regenerated in P4 |
| `reference` | `ll`, `prp` for the composite set | P4 |
| `mersenne-exponents` | `mersenne` for the prime list | P4 |

**The file format** (LF line ends, ASCII, single spaces, no tabs, no blank lines):

```
file      = magic command 1*source *note 1*record trailer
magic     = "# mps vectors 1" LF
command   = "# command: python3 tests/vectors/generate.py " stem " > tests/vectors/" stem ".txt" LF
source    = "# source: " text LF
note      = "# " text LF                          ; before the first record only
record    = op *(SP field) LF                     ; fields per the table below
trailer   = "# end " count " records " sha256 LF  ; the last line; sha256, lowercase, of every byte
before it
operand   = "x:" hex                              ; the value in full
          | "u:" lo "-" hi *("," lo "-" hi)       ; the OR of the bit ranges [lo, hi), each nonempty
          | "r:" bits ":" seed                    ; SplitMix64 limbs, exactly `bits` long
          | "c:" bits ":" seed                    ; SplitMix64 carry pattern, exactly `bits` long
value     = "x:" hex                              ; in full, at most 1024 bits
          | "h:" bits ":" low16 ":" sha256        ; above 1024 bits: bit length, low limb, digest
string    = "s:" chars                            ; in full, for a value of at most 1024 bits
          | "g:" length ":" sha256                ; above: its length and the digest of its bytes
hex       = "0" | [1-9a-f] *[0-9a-f]              ; lowercase, no leading zero
low16     = 16 lowercase hexadecimal digits
```

| op | fields | meaning |
|---|---|---|
| `cmp` | A B `lt`\|`eq`\|`gt` | compare(A, B) |
| `add`, `sub` | A B value | A + B; A − B with A ≥ B |
| `shl`, `shr` | A k value | A·2^k; ⌊A/2^k⌋, k decimal |
| `mul` | A B value | A·B, checked by schoolbook, by Karatsuba and by `multiply` |
| `sqr` | A value | A², checked by schoolbook, by Karatsuba and by `square` |
| `fold` | p X value | X mod M_p, p decimal |
| `res64` | p X H | H = the 16 uppercase digits of Res64 |
| `mod` | X M value | X mod M |
| `powmod` | A E M value | A^E mod M |
| `hex`, `dec` | A string | the canonical string of A |
| `ll`, `prp` | p value | the reference residues, value always `x:` |
| `mersenne` | p | p is a Mersenne prime exponent |

**SplitMix64** is the paper's (Steele, Lea and Flood 2014, Fig. 16, the root generator):
`state += 0x9e3779b97f4a7c15; z = state; z = (z ^ z>>33) * 0xff51afd7ed558ccd;
z = (z ^ z>>33) * 0xc4ceb9fe1a85ec53; return z ^ z>>33`, all modulo 2^64, the state starting at the
seed. From seed 0 the first two outputs are `9ca066f1a4ab2eea` and `d30b054265133dd7`.
`r:bits:seed` takes ⌈bits/64⌉ outputs as limbs, least significant first, keeps the low `bits` bits
and sets bit `bits − 1`. `c:bits:seed` builds the limbs in runs: each output v gives a run of
1 + (v >> 32) mod 32 limbs, all zero for v mod 3 = 0, all ones for 1, and the next outputs, one per
limb, for 2; runs are cut at ⌈bits/64⌉ limbs, then trimmed and topped as for `r:`.

**Digests.** SHA-256 (FIPS 180-4), over the value's limbs, least significant first, each as eight
little-endian bytes, with no high zero limb (zero is the empty string), or over a string's ASCII
bytes. A wrong value passes only by a SHA-256 second preimage on a structured input: no bug in the
arithmetic is related to the compression function, so equality is established, not resemblance. The
checks' SHA-256 can only make a check fail, since the expected digests come from `hashlib`.

**The sizes** (the gates check each item):

- `arith`: operands of 64k − 1, 64k and 64k + 1 bits for k = 1, 2, 4, 8 and 48; carry-pattern and
  all-ones operands, sums whose carry runs through at least four limbs and differences whose borrow
  does; comparisons that come out `lt`, `eq` and `gt`, including equal lengths differing only in the
  low limb or only in the top limb; shift counts 0, 1, 63, 64, 65, 128 and 1000 for both shifts, and a
  right shift past the operand's length, so every shift is checked at 0 and at multiples of 64; and
  the two large operands in `cmp`, `add`, `sub`, `shl` and `shr`.
- **The two large operands.** A = `r:1048576:<seed>` and C = `c:1048575:<seed>`, 2^20 and 2^20 − 1
  bits, are the only operands above 2^17 bits in all the families. R1's "up to about 2^20 bits" needs
  them; R3 converts every operand both ways, and R10's budget holds two such conversions, not more.
- `products`: for each s of T − 1, T and T + 1 limbs (T = `KARATSUBA_THRESHOLD`), a product of two
  s-limb operands, an s-limb operand times a longer one and a longer one times an s-limb one, and a
  square of s limbs; products and squares at an odd size from 5 below T and at an odd size above 2T;
  unequal operands, one at most half the other; all-ones operands of at least 128 bits and
  carry-pattern operands; and A·C, A² and C². Schoolbook is checked on every record with operands
  below 2^17 bits and on A·C and A²; Karatsuba at cutoff 4 on every record below 2^17 bits and at
  the default cutoff on every record; `multiply` and `square` on every record.
- `folding`: exponents 1, 2, 3, 5, 7, 31, 61, 63, 64, 65, 89, 127, 128, 129, 521, 1279, 4423 and
  9941; for each, X = 0, M_p, 2^p, 2·M_p, 2^p·M_p, (M_p − 1)² and random X below (M_p − 1)², with
  `fold` and `res64` for each, so Res64 is checked at p < 64 and is `0000000000000000` at x = 0 and
  at x = M_p.
- `modular`: m = 1, a one-limb m, x < m, x = m, x a multiple of m; the clamp case of MCA
  Exercise 1.19, a case needing two corrections of the one-limb estimate, and a case needing Knuth's
  add-back, each found by the generator simulating the algorithm and asserted there; divisors whose
  normalization shift is 0 and 63; x up to about 2^17 bits, by m from one limb to x's length;
  `powmod` with
  exponent 0, with m = 1, with a base above m, with base 0, and with a two-limb exponent.
- `hex` and `decimal`: every distinct operand of every family present (R3's "every vector operand"),
  the edge values 0, 1, 2^64 − 1 and 2^64, and M_p for every exponent of `folding` and `reference`.
- `reference`: `ll` and `prp` with full residues for the composite set, 28 primes p ≤ B with M_p
  composite: 11, 23, 29, 37, 41, 43, 47, 53, 59, 83, 113, 157, 211, 283, 383, 509, 691, 929, 1237,
  1249, 1277, 1669, 2273, 3049, 4111, 5527, 7433, 9973 (every one below 64, then roughly
  log-spaced to the largest prime below B, and 1237 and 1277, whose Lucas–Lehmer Res64 GIMPS
  publishes, as a check on the generator).
- `mersenne-exponents`, **the prime list**: the 22 terms of OEIS A000043 up to B = 10000, 2 to 9941.
  Its source lines cite `OEIS-A000043` as `doc/references.md` names it, and the command, which embeds
  the OEIS terms and exits non-zero unless a Lucas–Lehmer sweep in Python over every odd prime up to
  B finds exactly the odd ones. The known-answer cycle takes the file over (P5's ledger).

Everything is about 200 KB; the generator takes about two minutes, most of it the sweep (the
prototype: 184 KB, and 85 s of its 112 s in the sweep).

### The checks (R11)

`tests/vector_reader.*` loads a family by its stem from `tests/vectors/<stem>.txt` relative to the
working directory (`make check` runs `bin/check` from the repository root, as the cluster job does)
and `require`s, so the check stops: that the file opens; its last byte is LF; the magic line first;
the command line exactly as above, naming its own stem; at least one source line; known ops with
their field counts and well-formed fields; the trailer last, its count equal to the records read and
its digest equal to the SHA-256 of the bytes before it. A check also `require`s at least one record
of each op it covers. It rebuilds operands into limbs with `BigInt::from_limbs`, never through the
parsers under test. A mismatch prints at most three records per check (file, line, op, bit lengths
and low limbs, never a whole large value), then one `expect` that all N records agree. Each check
calls its entry points on every record of its ops, with the record's operands in the record's order
(`compare(A, B)`, `subtract(A, B)`, `multiply_karatsuba(A, B, 4)`, `reduce_mersenne(X, p)`,
`from_hex` of the record's string, …); the gates trace the calls and require it (§ 6).

The checks, by label (the gates name them, so the labels are pinned):

| file | label | phase | R-IDs |
|---|---|---|---|
| `check_mp.cpp` | `SHA-256 matches the FIPS 180-4 examples` | P1 | R8 |
| | `SplitMix64 matches its published outputs` | P1 | R1 |
| | `every vector file carries its command and source` | P1 | R8 |
| | `comparisons match the vectors` | P1 | R1 |
| | `sums match the vectors` | P1 | R1 |
| | `differences match the vectors` | P1 | R1 |
| | `shifts match the vectors` | P1 | R1 |
| | `folding modulo M_p matches the vectors` | P1 | R2 |
| | `Res64 matches the vectors` | P1 | R4 |
| | `hexadecimal matches the vectors and round-trips` | P1 | R3 |
| | `errors are reported, never values` | P1–P3 | R5 |
| | `schoolbook products match the vectors` | P2 | R1 |
| | `Karatsuba products match the vectors` | P2 | R1 |
| | `products match the vectors` | P2 | R1 |
| | `schoolbook squares match the vectors` | P2 | R1 |
| | `Karatsuba squares match the vectors` | P2 | R1 |
| | `squares match the vectors` | P2 | R1 |
| | `general reduction matches the vectors` | P3 | R2 |
| | `modular powers match the vectors` | P3 | R2 |
| | `decimal matches the vectors and round-trips` | P3 | R3 |
| `check_reference.cpp` | `the prime list carries its source` | P4 | R8 |
| | `Lucas-Lehmer ends in zero for every listed prime` | P4 | R6 |
| | `Lucas-Lehmer residues match the vectors` | P4 | R6 |
| | `PRP ends in one for every listed prime` | P4 | R7 |
| | `PRP residues match the vectors` | P4 | R7 |
| | `the reference tests refuse exponents outside their theorems` | P4 | R5 |

What they assert, beyond each record's equality:

- **R1:** each `cmp` both ways round (compare(B, A) is the negation) and `operator==`; each `sub` also
  as A − (A − B) = B; each `shl` and `shr` as the record says; products and squares through the
  three entry points as the sizes say.
- **R2:** every `fold`, `mod` and `powmod` result compared exactly, so a result outside [0, M_p) or
  [0, m) fails; `divide`'s quotient q and remainder r with q·m + r = x and r < m on every `mod`
  record below 2^17 bits.
- **R3:** for each `hex` and `dec` record, the conversion equals the vector's string (or its length
  and digest); parsing that string gives back A; and printing the parse of the string gives back the
  string. Parsing with leading zeros and, for hexadecimal, lowercase gives A too.
- **R4:** each `res64` record through `hex64(res64(X, p))`; `hex64(0)` is `0000000000000000` and
  `hex64(2^64 − 1)` is `FFFFFFFFFFFFFFFF`.
- **R5:** each error case throws `mp_error` and returns nothing (a file-local `expect_mp_error`, which
  records "returned a value" on a normal return and "threw <what>" on another exception):
  `subtract` below zero (one limb and several), `reduce_mersenne` and `res64` with p = 0,
  `from_hex` of `""`, `-1`, `+1`, ` 1`, `1 `, `0x1F`, `G`, `1_0` and a full-width digit, and of
  strings of up to three chunks of 16 digits with one bad character at the first, the last, a chunk
  boundary or an interior position, so a parser that validates only some chunks fails (P1);
  `divide`, `reduce` and `power_mod` with m = 0, `from_decimal` of the same kinds of strings
  (chunks of 19 digits) and `12a` (P3); both reference tests at p = 0, 1, 2, 4 and 10 (P4).
- **R6, R7:** `reference::lucas_lehmer(p)` is 0 and `reference::prp3(p)` is 1 for every exponent of
  the prime list from 3 on, read from the file; each composite exponent's residues equal the
  vector's and are nonzero (Lucas–Lehmer) and not 1 (PRP).
- **R8:** every family's command line and sources, through the reader; the prime list's source line
  names `OEIS-A000043`.

### Where the paths go (`AGENTS.md`, § Invariants 3; GOAL, Notes)

- **P1**, in the commit that creates the vectors: the high-blast-radius list gains exactly
  `` - `tests/vectors/` (the generator and every vector file: expected values) ``. § Map's `src/mp.*`
  line no longer says "a stub" and names `hex64`, the Res64 text the result printer uses; the
  `tests/` bullet names `vector_reader.*` and `vectors/`.
- **P4**, in the commit that creates them: the list gains `` - `src/reference.hpp`,
  `src/reference.cpp` `` and `` - `tests/vectors/mersenne-exponents.txt` (the Mersenne prime
  exponents) ``, and § Map gains a line beginning `` - `src/reference.cpp`, `src/reference.hpp`: ``.

### The note (R12, `doc/reference-arithmetic.md`)

The five sections of the existing notes, in order: The result, Proof, What mps uses, Sources,
Provenance. Each phase adds its part in the commit that implements it:

- **P1:** `### Folding modulo $M_p$` and `### Res64` (results and proofs: the congruence, termination
  at x ≤ M_p, at most two folds below 2^(2p), M_p to 0, the low limb); the canonical strings.
- **P2:** `### Schoolbook` and `### Karatsuba` (the identities for product and square, z1 ≥ 0, the
  split and its termination at 4 limbs, the threshold).
- **P3:** `### Division`, `### Powers` and `### Radix conversion`.
- **P4:** the reference tests, in "What mps uses".

"What mps uses" names, backticked, by phase: `src/mp.cpp`, `reduce_mersenne`, `res64`, `hex64`,
`to_hex`, `from_hex` (P1); `multiply_schoolbook`, `multiply_karatsuba`, `square_schoolbook`,
`square_karatsuba`, `KARATSUBA_THRESHOLD` (P2); `divide`, `reduce`, `power_mod`, `to_decimal`,
`from_decimal` (P3); `src/reference.cpp`, `reference::lucas_lehmer`, `reference::prp3` (P4). Every
number goes in a check span over `claims.py`'s functions (folding written with `%` and `//`) or a
cited span; Lucas–Lehmer facts above p = 4500, beyond `ll_residue`'s reach, are cited or left out.
`doc/README.md` gains one line for the note. `doc/references.md` gains `KaratsubaOfman1962` (P2) and
Knuth's volume 2 (whenever first cited, keyed by the year its metadata gives), each checked against
the work as the bibliography's rule says; Brent–Zimmermann's MCA is already there.

In P4 the "What mps uses" sections change: `doc/lucas-lehmer.md` names `reference::lucas_lehmer`
and still says `test_lucas_lehmer` is a stub that decides nothing; `doc/prp-and-gerbicz.md` names
`reference::prp3`, says it returns the type-1 residue, 1 for a probable prime and 9 times smaller
than x_p, and still says the PRP test of `-t` does not exist yet.

### Requirement → design map

| R-ID | Design element(s) that satisfy it |
|------|-----------------------------------|
| R1 | `compare`, `add`, `subtract`, `shift_*`, the two products and squares and their dispatchers; `arith` and `products`; the R1 checks |
| R2 | `reduce_mersenne` (folding), `divide`/`reduce`, `power_mod`; `folding` and `modular` with their edges and m = 1; exact comparison |
| R3 | `to_hex`/`from_hex` (P1), `to_decimal` (divide and conquer)/`from_decimal` (P3); the canonical forms; `hex` and `decimal` over every operand |
| R4 | `res64`, `hex64` moved from `result.cpp`; `res64` records at p < 64 and at x = 0 and M_p |
| R5 | `mp_error` from `subtract`, `reduce_mersenne`, `res64`, the parsers, `divide`, `reduce`, `power_mod` and the reference tests; `expect_mp_error` |
| R6 | `reference::lucas_lehmer`; the prime list; the composite set's full residues |
| R7 | `reference::prp3`, type-1; the prime list; the composite set; the PRP note says which |
| R8 | The envelope's command and source lines and the trailer; the gates' reproduction under two Pythons |
| R9 | Vectors read at run time, no new tool in `make check`; `tests/cli.sh` untouched; `bin/mps` links two more objects and no library; the gates compare `bin/mps` with `c9b82ff`'s |
| R10 | B = 10,000, the composite set, the sizes and the divide-and-conquer decimal output, within a 3.0 s budget the gates measure |
| R11 | The checks table; the gates' mutants show each turns `make check` red |
| R12 | `doc/reference-arithmetic.md`, the README line, the references, the two "What mps uses" sections, `claims.py` checks |

## 3. Invariant gate (AGENTS.md constitution check)

Checked against `AGENTS.md` § Invariants before research, and again after this design was drafted
and after the adversarial review was folded in.

- **High-blast-radius paths.** The change touches `src/mp.*` and `src/result.cpp` and adds
  `src/reference.*` and `tests/vectors/`; their list lines land in the commits that create them. A
  CONFIRMED review finding in any of them needs Geoffrey's decision before landing (GOAL, Notes).
- **§ 1 Verdicts are earned.** Untouched: the reference tests return residues and never reach
  `Result`, `run_in_order` or `mps -t`; `result.cpp` changes only where `hex64` lives, and its text is
  pinned by the existing checks and the gates' comparison with `c9b82ff`'s `bin/mps`.
- **§ 2 Arithmetic is checked.** This cycle builds the reference the invariant names. It is itself
  checked against an independent method (Python's integers), on vectors the gates recompute and on
  cases of the gates' own. No floating point; the compile flags do not change.
- **§ 3 Known answers.** The vectors and the prime list are expected values: each file carries its
  command (the independent method) and the prime list its published source too; they join the
  high-blast-radius list in their first commit; families are split so no phase rewrites another's
  records, and the generator's literal seeds keep earlier records fixed when cases are appended. The
  `hex` and `decimal` families are regenerated as families are added, within this unlanded cycle.
- **§ 4 Clean room.** The algorithms come from Knuth, Karatsuba–Ofman, Brent–Zimmermann, FIPS 180-4
  and Steele–Lea–Flood, cited where used; no program's source was opened in research, and the builder
  opens none.
- **§ 5 Dependency-free core.** `bin/mps` gains two objects and no library. Python is a test oracle
  used once, by hand, to write files that live with the tests; `make check` never runs it.
- **§ Conventions.** The algorithm note lands in the commit of each algorithm; no R#/P# ids in
  `src/` or `tests/`; `hex64` lives in one place; the code style as `AGENTS.md` gives it; the profile's
  `verify` (`make check`) does not change.

### Deviation justifications

| Deviation | Why needed | Simpler alternative rejected because |
|-----------|-----------|--------------------------------------|
| 1. Decimal output by divide and conquer, not repeated division by 10^19 | R3 converts every vector operand, two of which are about 2^20 bits, within R10's budget | Repeated division costs 1.6 s per 2^20-bit operand; capping decimal vectors at 2^17 bits would bend R3 |
| 2. `result.cpp` loses `hex64` and includes `mp.hpp` (a high-blast-radius file) | GOAL, Notes: one Res64 form; § Conventions: point to a rule, don't restate it | Two copies would be a same-commit obligation in two places |

## 4. Rabbit holes (resolved)

- **The algorithms and their sources** → [01](research/01-algorithms-and-sources.md); decisions in
  [00](research/00-digest.md), items 4–10.
- **Vectors that stay small yet establish equality** → seeds for operands, SHA-256 for large results
  ([02](research/02-vectors-and-digests.md)).
- **R3 against R10** → divide-and-conquer decimal output ([00](research/00-digest.md), item 7).
- **B and the composite set** → 10,000 and 28 log-spaced primes
  ([03](research/03-time-and-portability.md) § 2).
- **Where the reference tests live, one Res64 form, errors** → [04](research/04-integration.md) §§ 1–3.
- **R10 against the first prototype** → a full prototype of § 2, built from this plan to dry-run the
  gates, came in at +3.9 s: six operands of about 2^20 bits (and one of 2^21) each paid a decimal
  conversion both ways. The sizes now hold exactly two large operands, A and C, folding stops at
  p = 9941 and division at about 2^17 bits; the threshold became the builder's measured value
  (96 for the prototype, against 48 for research's). The revised prototype runs +1.9 s with clang++
  and +2.0 s with g++-16. The vectors also caught a prototype `divide` whose add-back step did not
  decrement the quotient digit.
- **Adversarial design review** (trigger 3): a fresh reviewer at xhigh, given the GOAL, this PLAN,
  the gates and the prototype, attacked them with defective variants in copies of the dry-run clone:
  3 HIGH, 4 MEDIUM, 2 LOW, all folded in.
  1. *Karatsuba could be schoolbook* (H): a `multiply_karatsuba` returning `multiply_schoolbook`, and
     the square alike, passed every gate step, R10 included (+2.7 s): the `kara-*` bugs prove the
     checks would see a wrong split, not that the builder's code splits. `trace.cpp` now reads the
     call structure through the wrappers (schoolbook calls no product; Karatsuba recurses into
     itself from its cutoff; the dispatchers choose by the threshold), and § 2 pins it.
  2. *Parsers validating only their first chunk* (H): every malformed string in the probe and in
     R5's list was shorter than one chunk, so a parser checking only 16 or 19 digits passed. The
     probe now sends 2,000 strings of one to three chunks with a bad character at the ends, at a
     chunk boundary and inside; R5's list in § 2 has them; `parse-tail` and `parse-tail-dec` stand
     for the class.
  3. *Checks skipping records* (H): a conversion check skipping A and C, and a prime list stopping
     below 5000, stayed green, since only the last record of each op was tampered. The control run
     now traces every wrapped call, and `vectors.py coverage` requires every record to reach its
     entry points with its own operands.
  4. *A correct note refused* (M): `docs.py`'s heading pattern could not match a heading ending in
     `$`. Fixed.
  5. *R12's checker unpinned* (M): `notes.py` and `claims.py` are imported from the tree under test;
     the math-notes cycle's `verify/` joins the frozen files.
  6. *A later plan commit trusted* (M): a commit re-using the plan's prefix after the build could
     widen the gates. The freeze check now refuses a plan commit that follows any change to code,
     tests, notes or `AGENTS.md`; the foreman may also record the plan commit's tree hash.
  7. *R10 under load* (M, plausible): medians under contention can cross the budget on a correct
     tree. The gate now takes the least of interleaved runs, and ten runs before failing.
  8. LOW: the probe gained `res64` with p = 0 (8); the ledger's IBDWT check looks for a phrase only
     the deferral adds (9).

  It found these held: the wiring refuses header, inline, overloaded and split definitions loudly
  rather than skipping them; the mutant cap with the checks reordered; the tampers and the reader's
  envelope rules; `vectors.py`'s recomputation and coverage of § 2's sizes; the probe's
  independence; the footprint; R9; the cumulative design; every P2 and P4 bug caught.

## 5. Risks & open questions

- **Bibliography.** MathNet.ru timed out three times for Karatsuba–Ofman 1962, and Knuth's algorithm
  letters were not confirmed in a copy. The builder retries and, failing that, cites as the
  bibliography's paywall rule allows (the work's metadata, no locator); the review grades it.
- **Generator run time** is about two minutes, most of it the Lucas–Lehmer sweep behind the prime
  list; the gates run it under two Pythons in parallel from P4 on.
- **R10 on other machines.** The budget is measured on this Mac against `c9b82ff` in the same gate
  run. The cluster nodes are slower (estimates 0.5–0.9× for this code), and their check step includes
  compiling, so their seconds are recorded, not gated.
- **The gates pin the design's surface**: the names and signatures of § 2, each free function
  defined once at file scope with its name on the first line; `KARATSUBA_THRESHOLD` (its line's
  form, the value within 8 to 256); `Division`'s members; `mp_error`; the file names, stems, format
  and op table; the check labels; the canonical strings; the `AGENTS.md` lines; the note's file and
  headings. A builder who needs otherwise stops and escalates; the gates are frozen.
- **This cycle's seed** stays `adopted:` with its ROADMAP entry until `code-roadmap` retires both
  after landing; the entry still promises the transform digit form, which the ledger hands to the
  IBDWT cycle.

## 6. Verification strategy

Each phase's `verify:` is `sh spec/reference-big-integer/verify/pN.sh`. The gates are frozen at the
plan commit (each first requires `verify/` to match the latest commit whose subject starts
`[feature] Plan reference-big-integer`, with nothing untracked, and that commit to come before any
change to `src/`, `tests/`, `doc/`, `ci/`, the Makefile, `AGENTS.md` or `README.md`, so a plan
commit made after the build is refused), run on this Mac (clang++, g++-16,
GNU make 3.81 at `/usr/bin/make`, `python3` and `python3.14`, git, rsync, perl, otool), and are
**cumulative**: P*n*'s gate runs every check of P1 to P*n*, and no gate requires a phase's exact
file set, so a reopened phase reruns its gate after later phases have landed. Each prints
`GATE FAIL: <reason>` and exits 1 at the first failure, or one green line. Each, in order
(`common.sh`, `gate_all`):

1. **builds** a clone-like copy per compiler with `make -B check`: green, no warnings, every check
   label of PLAN § 2 up to the phase run and passed, the same counts on both compilers;
2. **holds the footprint**: the files PLAN § 2 leaves alone equal `c9b82ff`'s, the math-notes
   cycle's `verify/` (whose checker R12's gate imports) among them; `src/result.cpp`
   differs only by `hex64` moving out and `mp.hpp` coming in; the Makefile only by `MODULES` gaining
   `reference` (required from P4); new files only where PLAN § 2 puts them, and each phase's files
   present; no spec ids in `src/` or `tests/`, and nothing there starts a process;
3. **R9**: the tree's `bin/mps` and `c9b82ff`'s print the same bytes and exit alike on 26 runs
   covering every path of `bin/mps` today, each under `NO_COLOR` and `FORCE_COLOR`; `otool -L` lists
   the same libraries; `make check` stays green with `python`, `python3`, every `python3.N` and `uv`
   first on `PATH` as stubs that record a call, and none is called;
4. **R8**: every family's second line is exactly its command, and the command, run from a copy of the
   root, reproduces the file byte for byte under `python3` (3.9) and `python3.14`;
5. **the vectors** (`vectors.py`): each family read against the grammar, the trailer's count and
   digest checked; **every record recomputed** with Python's integers from the gate's own reading of
   the operand forms and SplitMix64, independent of `generate.py`; and the sizes and edges of PLAN § 2
   for the phase, the division paths found by simulating MCA Algorithm 1.6 and Knuth's refined
   estimate, the composite set and the prime list (against the gate's own copy of A000043);
6. **the probe** (`probe.cpp`, `probe.py`): a driver built against the tree's `src/` with libc++'s
   bounds hardening answers about 12,600 cases of the gate's own (structured carry and borrow
   chains, shift counts, every fold edge for 80 exponents, the canonical strings and 25 malformed
   ones, among them 2,000 of one to three chunks with a bad character placed at a chunk boundary,
   the ends and inside, products at every size from 0 to 129 limbs and both sides of the threshold, add-back
   divisions, powers, Lucas–Lehmer and PRP on 37 odd p including composite p, and every error case),
   compared with Python; values cross through the probe's own converters, never `to_hex`;
7. **the bugs** (`mutant.cpp`): a copy whose `mp.cpp` and `reference.cpp` have their public
   definitions renamed, with the gate's wrappers linked in their place, built once; the
   pass-through control run of `bin/check` is green and changes nothing, and it traces every wrapped
   call: **every record reaches its entry points** with its own operands (`vectors.py coverage`:
   each op's entry points as § 2's checks say, schoolbook below 2^17 bits and on A·C and A², Karatsuba
   at cutoff 4 below 2^17 bits and at the threshold on every record, both reference tests on every
   listed prime and every composite), so no check can skip records or a family; and **the products'
   structure holds** (`trace.cpp`, from P2): the schoolbook product and square call no product or
   square, Karatsuba from its cutoff recurses into itself, and `multiply` and `square` call
   Karatsuba from the threshold and schoolbook below it, which no result can show; then, for each
   bug, the
   wrapper computes its answers as a `BigInt` with that one bug would, and `bin/check` must go red
   with the named check failing and at least one answer changed, under a 120 s limit; a bug stands
   down after 2,000 changed answers, so one that compounds (a fold never repeated, inside
   Lucas–Lehmer) cannot run away after its own check has failed;
8. **the tampered files**: with each family removed, or (on three families, the reader being one code
   path) its trailer's digest altered, its command or its sources removed; or the last result of each
   op altered with the trailer recomputed, `bin/check`
   goes red in the named check; and the prime list with 11 added, or without its OEIS source;
9. **R12** (`docs.py`): the note, the index and the bibliography through the landed math-notes
   checker (`notes.py`, imported, never edited): five sections, citations that resolve and are
   listed under Sources, the clean-room rules, every number in a span and every check true; the
   phase's headings, names and citations; the other notes unchanged, and from P4 the two "What mps
   uses" sections; `AGENTS.md`'s lines, the list only widening;
10. **R10**: warm `make check`, the least of five interleaved runs (ten when the first five miss,
    so a busy moment cannot fail a correct tree), for the tree and for `c9b82ff` in the same run,
    with each compiler; the tree's is at most **3.0 s** longer (the baseline measured at planning: 0.88 s
    with clang++ and 3.83 s with g++-16, [03](research/03-time-and-portability.md) § 1).

**The bugs and the classes they stand for** (each must turn `make check` red in the named check):

| Phase | Bug | Class |
|---|---|---|
| P1 | `cmp-top` | a comparison loop that stops after the top limb |
| P1 | `add-carry-out`, `add-carry-in` | a dropped final carry; the carry test `s < x` that misses an all-ones limb meeting a carry |
| P1 | `sub-borrow-in`, `sub-short`, `sub-below` | the borrow test that misses an all-ones limb; a borrow that stops at the shorter operand; a negative difference returned |
| P1 | `shl-word`, `shr-word`, `shr-top` | a limb shifted by 64 at a multiple of 64 (the hardware masks the count); a shift loop one limb short |
| P1 | `fold-once`, `fold-carry`, `fold-mp` | a fold not repeated; the fold's carry out of p bits lost; M_p left for 0 |
| P1 | `res64-raw`, `res64-mp`, `hex64-unpadded` | Res64 of the unreduced value; Res64 at x = M_p; Res64 printed without its leading zeros |
| P1 | `hex-lower`, `hex-lead`, `parse-prefix`, `parse-empty`, `parse-tail` | a non-canonical string; a lenient parser, at the start and past the first chunk |
| P2 | `mul-row-carry`, `sqr-double` | a schoolbook row's last carry dropped; squaring's doubling done limb by limb |
| P2 | `kara-odd`, `kara-sqr-odd`, `kara-sum-carry`, `kara-sqr-sum-carry`, `kara-own-split` | an odd split recombined at the wrong offset; the half-sum's carry limb dropped; unequal operands split at their own middles |
| P3 | `div-add-back`, `div-one-correction`, `mod-to-m`, `mod-zero` | the rare add-back omitted; the second correction omitted; a remainder in [0, m]; a zero modulus answered |
| P3 | `pow-one`, `pow-start`, `pow-reversed`, `dec-pad`, `parse-prefix-dec`, `parse-empty-dec`, `parse-tail-dec` | modulus 1 unreduced; exponent 0 giving the base; the exponent read from the wrong end; decimal groups unpadded; a lenient decimal parser |
| P4 | `ll-extra`, `ll-seed`, `prp-x`, `prp-base2`, `domain` | an off-by-one iteration count; a start value that still proves the primes; the wrong residue type; base 2; p outside the theorem answered |

Hunting mutants never ends, so these are chosen to stand for the bugs a clarity-first big integer
actually has, one or two per class, and no more. They are the gate's own implementations of the
buggy operation over the public interface, so they hold whatever the builder's code looks like.

**Pre-push and P5.** `verify/prepush.sh` is everything P5's checklist runs before the push: the P4
gate on the final tree, the deferral ledger (every `issues/*.md` matched one-to-one with a
`**Seed:**` line; every issue `TECH.md` names present; this cycle's seed still `adopted:`; the
balanced digit form in the IBDWT seed and its ROADMAP entry; the prime list in the known-answer
seed; `evidence/ledger.md` with `## Deferrals`), no uncommitted code, and the site-private scan of
the cycle's files and of `origin/main...HEAD`'s messages and patches. `sh verify/prepush.sh scan`
runs the scan alone. `verify/p5.sh` runs the same and then checks the evidence: three `result.json`
files that pass the job's format check, every step passed, GCC 13 or newer, the target's
architecture, `<cluster>` and `<host>` redacted, distinct job ids matching `jobs.tsv`'s
`COMPLETED 0:0`; one tested commit, on `origin/feature/reference-big-integer`, containing
`origin/main`, whose code equals `HEAD`'s outside `spec/`, `issues/` and `ROADMAP.md`, and whose
`ci/test-commit.sh` is the script each run printed; each check log's summaries equal to this tree's
counts; each check step's seconds recorded under `## Times` in `evidence/ledger.md` against cycle
6's run on the same target (the step compiles too, so R10 is gated on this Mac, not there); the scan
covering the evidence. `cluster.sh` and `redact.sh` are cycle 6's, with this cycle's paths and
ledger.

**Dry runs before commit**: *to be recorded here.*

The freeze check finds the plan commit by its subject, `[feature] Plan reference-big-integer…`; a
revised plan keeps that prefix.

---

*Backing research: [`research/00-digest.md`](research/00-digest.md).*
