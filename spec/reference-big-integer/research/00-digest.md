# Research digest — reference-big-integer

Four briefs, fanned out in parallel because `src/mp.*` is on the high-blast-radius list (trigger 2)
and the appetite is big:

- [01 — algorithms and their sources](01-algorithms-and-sources.md)
- [02 — the vectors: format, generation, digests](02-vectors-and-digests.md)
- [03 — time and portability](03-time-and-portability.md)
- [04 — how the change fits the repo](04-integration.md)

The decisions below are what [`PLAN.md`](../PLAN.md) builds on. Where briefs disagree, the line says
which recommendation wins and why.

## Decisions

1. **The type and its home.** `BigInt` stays in `src/mp.*`: little-endian 64-bit limbs in a
   `std::vector<std::uint64_t>`, no high zero limb, `unsigned __int128` spelled once behind an alias,
   `<bit>` (`std::countl_zero`) rather than builtins (03 § 4). Its operations are free functions,
   each defined out of line at file scope, so the gates can wrap them (04 § 7). One error type,
   `mp_error : mps_error`, in `mp.hpp` (04 § 3).
2. **The reference tests** live in a new module, `src/reference.*`, `namespace reference`, joining
   `MODULES` and the high-blast-radius list in the commit that creates them (04 § 1). `math.hpp`, the
   coupled core, is not touched.
3. **One Res64 form.** `hex64` moves from `src/result.cpp` to `src/mp.*`, and `result.cpp` includes
   `mp.hpp`. The five checks in `tests/check_result.cpp` that pin its text stay as they are (04 § 2).
4. **Multiplication.** Schoolbook by MCA Algorithm 1.2; squaring by the symmetric form of MCA § 1.3.6
   (cross products once, doubled, plus the diagonal), so the squaring check tests an algorithm of its
   own (01 § 1). Karatsuba is the additive variant for both product and square, split at half the
   larger operand rounded up, with a `cutoff` argument clamped to at least 4; the checks run it at
   cutoff 4 on small and middle sizes, so every level splits (01 § 2). The dispatcher's threshold is
   48 limbs, measured flat from 32 to 128 (03 § 3).
5. **Folding** loops while the value is longer than p bits and then maps M_p to 0; for x < 2^(2p) it
   folds at most twice, proved in the note as the project's own derivation (01 § 3). No shift by 64:
   branch on p mod 64 = 0.
6. **Division** is MCA Algorithm 1.6 (Knuth's classical algorithm, TAOCP vol. 2 § 4.3.1), with
   normalization and a one-limb short division. The builder may correct the quotient digit by
   compare-before-subtract or by Knuth's refined estimate and add-back; the vectors force the clamp
   case, a two-correction case and an add-back case either way (01 § 4). `divide` returns both
   quotient and remainder; `reduce` is its remainder.
7. **Decimal output is divide-and-conquer** (MCA Algorithm 1.26, through `divide`). Briefs 02 and 03
   proposed capping decimal vectors near 2^17 bits because the quadratic conversion costs 1.6 s per
   2^20-bit operand. That would bend R3 ("every vector operand"), so it is rejected; the
   divide-and-conquer prototype converts a 2^20-bit operand in 0.17 s. Decimal input is MCA
   Algorithm 1.23 in chunks of 19 digits. Both land with division, in P3; hexadecimal (linear) lands
   in P1.
8. **Canonical forms.** Decimal: digits only, no leading zero, `0` for zero. Hexadecimal: uppercase,
   no prefix, no leading zero, `0`. Parsing accepts leading zeros and both hexadecimal cases and
   rejects everything else, the empty string included (01 § 6).
9. **Powers** by MCA Algorithm 2.12, left to right, with exponent 0 and modulus 1 handled first.
10. **PRP residue: type 1**, 3^(M_p − 1) mod M_p, which is 1 for a probable prime: p − 1 squarings
    from 3, each of the first p − 2 followed by a multiplication by 3. GIMPS publishes it, and it has
    no edge like x_p at p = 3, where 9 reduces to 2 (01 § 7).
11. **B = 10,000.** The 21 prime-list exponents 3 … 9941 cost 0.35–0.55 s for both tests; 20,000
    would cost five times as much (03 § 2).
12. **The composite set** is 28 primes: the nine below 64 (11, 23, 29, 37, 41, 43, 47, 53, 59, which
    also give R4 its p < 64), sixteen roughly log-spaced above (83, 113, 157, 211, 283, 383, 509, 691,
    929, 1249, 1669, 2273, 3049, 4111, 5527, 7433, 9973), and 1237 and 1277, whose Lucas–Lehmer Res64
    GIMPS publishes as a cross-check of the generator. Brief 02's evenly spaced set costs four to
    eight times as much (03 § 2), so the log-spaced one wins. Residues are stored in full.
13. **Vectors** (02): one generator, `tests/vectors/generate.py <stem>`, writing one family per file;
    the envelope `# mps vectors 1`, `# command:`, `# source:`, records, and a trailer
    `# end N records <sha256 of the preceding bytes>`; operands written literally, as bit ranges, or
    as SplitMix64 seeds (the paper's variant, Steele–Lea–Flood 2014, Fig. 16); values in full up to
    1024 bits and above that as bit length, low limb and SHA-256 of the limb bytes. SHA-256 is written
    in `tests/` from FIPS 180-4 and checked against its examples. Families split by phase, so no
    phase edits another's records, except that the hexadecimal and decimal families cover the
    operands of every family present.
14. **Sizes.** Two operands at about 2^20 bits (one SplitMix64, one carry pattern); schoolbook checked
    on one full-size product and one full-size square, Karatsuba on every product and square; middle
    sizes up to 2^16 bits are cheap. Total under about 400 KB (02 § 4, 03 § 3).
15. **R10.** The budget is 3.0 s over `c9b82ff`'s warm `make check` on the same Mac, measured by the
    gates for clang++ (baseline 0.88 s) and for g++-16 (3.83 s, mostly `tests/cli.sh` process start).
    The cluster job's check step includes compiling, so its seconds are recorded for information
    only (03 § 1, § 5; 04 § 7).
16. **The doc note.** A new `doc/reference-arithmetic.md` in the five-section shape of the existing
    notes, growing with each phase; the Lucas–Lehmer and PRP notes' "What mps uses" sections name the
    reference tests in P4. `claims.py` suffices if folding is written with `%` and `//` and facts above
    p = 4500 are cited or left out (04 § 5). New bibliography entries: Knuth 1997 and Karatsuba–Ofman
    1962, each checked against the work; MathNet.ru timed out for 01, so the builder retries it.
17. **The gates** wrap each public function through a renamed definition (cycle 4's interposer),
    cap every mutant run with a time limit, check allowed file sets cumulatively, never a phase's exact
    set, and copy cycle 6's cluster, scan and redaction machinery with the `feature/` prefix (04 § 7,
    § 8).
