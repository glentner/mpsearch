# 02 — Computed rows, recomputation bounds, and what they cost

Scope: R8's computed rows, R9's recomputation, and the gates' run time. Measured on the planning
Mac (Apple M2) on 2026-10-17, CPython 3.9.6 (`python3`) and 3.14.6 (`python3.14`); the two are
within 1 % of each other on every measurement here.

## What a squaring modulo M_p costs in Python's integers

One iteration (square, then fold twice; Karatsuba in CPython):

| p | s per iteration |
|---|---|
| 1,000,003 | 0.080 |
| 3,888,509 | 0.69 |
| 10,000,019 | 2.69 |

The first ⌈log2 p⌉ or so iterations are cheaper: s_k from 4 has about 2^(k+1) bits until it
passes p, so nothing wraps modulo M_p before k ≈ log2 p. A residue after fewer iterations tests no
modular reduction at all. **Rule: a computed partial row takes k ≥ ⌈log2 p⌉ + 8**, so its value has
been reduced modulo M_p at least eight times.

`pow(3, M_p - 1, M_p)` is slow (general division): 57 s at p = 28603. Folding (x ← 3x² mod M_p,
p − 2 times, then x²) gives the same Res64 in 10.5 s. The table command must fold.

## The computed partial rows (R8) and their values

LL: s_k from s_0 = 4. PRP: x_k = 3^(2^k) mod M_p, the x_k of `doc/prp-and-gerbicz.md`, Theorem 2.
Exponents chosen where a published final row exists, so an engine can run the short test and the
full one on the same p. The planner's own folding computation (`plan-proto/scripts/partial.py`):

| p | k | LL Res64 | PRP Res64 | s (both) |
|---|---|---|---|---|
| 1277 | 100 | `EE3E8EF89BAFBDCD` | `C6D7DFE3FCAEDC31` | 0 |
| 1277 | 1000 | `3D43F5D86DECC8F5` | `36F834F2D5842CF5` | 0 |
| 10303 | 100 | `443391E534C210F2` | `088D6D3D863CB0C7` | 0 |
| 10303 | 1000 | `7841BFAB20749B83` | `3C30472597BE7B6C` | 0.1 |
| 10303 | 10000 | `FB3AC33F5A26CC71` | `65CE30B46EF0AA1B` | 1 |
| 100069 | 100 | `03DFC5B4407ED178` | `05DC064E3050BCD0` | 0.4 |
| 100069 | 1000 | `7AA9194B9ED5BBBC` | `D87224FAC22ABDEF` | 4 |
| 1000193 | 100 | `F1234B20ACFD642F` | `44DEBFC2AA1782BA` | 14 |
| 3888509 | 100 | (published, 01) `71E61322CCFB396C` | `79FA4A9B6D077D31` | 111 |
| 10000019 | 32 | `3DC2608FC9C15BE6` | `B6DF55F5DE80E01A` | 53 |

Every row meets the rule (10000019: log2 ≈ 23.3, so k ≥ 32). Decades covered: 10^3, 10^4, 10^5,
10^6 (two exponents), and 10^7 itself, so either reading of R8's "each decade from 10^3 to 10^7"
holds. The LL row at 3888509 is the published one; the command recomputes it (R9).

## Recomputing published final rows (R9)

| row | Python | 
|---|---|
| LL 1237, 1277 | under 0.1 s |
| LL 10303 | about 0.5 s |
| PRP-1 28603 (folding) | 10.5 s |
| LL 100069 | 201 s |
| LL 1000193 | about 22 h (p · 0.08 s) |

**Bound:** the command recomputes every published final row with p < 200,000, which is every row a
Python run finishes in minutes; above it a single row takes most of a day (10^6) to years (10^8), so
those rows rest on their published agreement (R7) alone, and the table's notes say so.

## The cost to the gates

The three table commands run in the background beside the builds: each traced under `python3` from
inside (the same run is the byte-for-byte reproduction) and run as its header says under
`python3.14`, plus a second computation of every recomputed row in four processes (about 200 s,
LL 100069 the longest). The sabotage and literal runs replay the trace's record, so all of them
take under a second. In the dry runs after the design review a full gate took about 10.5 minutes on
this 8-core Mac. The remaining vector families reproduce under `python3` only (they don't change;
the check is that removing a family from `generate.py` leaves the others' output alone).

## R11's baseline

`make check` with nothing to rebuild at `c41950c`, the least of 7 interleaved runs (load 2.8):
**3.52 s with clang++, 6.13 s with g++-16** (medians 3.52 s and 6.16 s). The budget is +0.5 s. The
new checks parse three tables of under a hundred rows and compare two Res64; the reference tests
keep their exponent set (R6), so the expected cost is well under 0.1 s.
