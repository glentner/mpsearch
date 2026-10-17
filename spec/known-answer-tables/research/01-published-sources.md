# 01 — The published sources: what they hold, and the rows they back

Scope: R2, R5, R7, R8's published row. Every fetch below was made by the planner on 2026-10-17
(UTC 04:28–04:50) with `curl`, one request every two seconds, and saved outside the tree. User
names in the reports are left out here; nothing in this cycle needs them.

## The primes list (R5)

- `https://www.mersenne.org/primes/`, title "List of known Mersenne prime numbers - PrimeNet",
  copyright Mersenne Research, Inc. Its table has one row per prime, ranked 1 to 52, each exponent
  in a link `/M<p>`. Row 52 is starred: "Provisional ranking, not all candidates between
  M82,589,933 and M136,279,841 have been eliminated".
- On 2026-10-17 it lists 52 exponents, ascending:
  2 3 5 7 13 17 19 31 61 89 107 127 521 607 1279 2203 2281 3217 4253 4423 9689 9941 11213 19937
  21701 23209 44497 86243 110503 132049 216091 756839 859433 1257787 1398269 2976221 3021377
  6972593 13466917 20996011 24036583 25964951 30402457 32582657 37156667 42643801 43112609
  57885161 74207281 77232917 82589933 136279841.
- The 22 up to 9941 equal `tests/vectors/mersenne-exponents.txt`'s records, in order. The 30 above
  10,000 are the rows R5 makes cite this list.

## The reports (R7)

- `https://www.mersenne.org/report_LL/?exp_lo=A&exp_hi=B&txt=2` returns `text/plain` CSV:
  sections headed `<n> Verified LL test results`, `Unverified`, `Bad`, `Later Factored`; columns
  `Exponent,User name,Computer name,Residue,Shift count,Error code,LL,PRP,Software,Date found`.
  `txt=1` wraps the same text in HTML `<pre>`; the GOAL's "txt=1" and `txt=2` are the same report.
- `report_prp/` answers HTTP 302 to `/report_PRP/`; follow it. Columns add `ResidueType` and
  `ResidueBase`. A PRP result "verified" by a proof certificate is listed alone: it is one result,
  not two, so R7 does not accept it.
- Early results carry a short residue, not a Res64, and are not counted. Their widths, read against
  the Res64s they sit beside: 12 bits from the Cray program (`373` beside `…8373`, `E4D` beside
  `…8E4D`), 15 bits from the results the report credits to "Unknown" (`3188` beside `…B188`, the
  low 15 bits; `5BE2`, `78BA`, `238B` and `594C` fit 15 bits too), 16 bits from the DWT program
  (`8E4D`). The planner's rule compared them as 16 bits, so it rejected 10061 wrongly: its 15-bit
  residue agrees. No row changes (10303 serves the decade), and this note corrects the reading
  (design review F12).

**The acceptance rule as applied** (the seed's, R7): every 64-bit residue for the exponent agrees;
two or more of them come from different shifts or different programs; no result for the exponent
carries an error code or sits in a Bad section; PRP results are all type 1, base 3. `agreed` is the
number of agreeing 64-bit residues. 1000003's LL is out (an error code `03003F00` on a matching
result and a Bad result); its PRP is clean but 1000193 is used for both tests.

| test | decade | p | Res64 | agreed | the results (shift, program) |
|---|---|---|---|---|---|
| LL | 10^3 | 1237 | `DF5E6622F83F5BE2` | 2 | 0 Prime95; 116 Prime95 (and `5BE2`, a short residue) |
| LL | 10^3 | 1277 | `5613A480590E78BA` | 3 | 0, 762, 132, Prime95 (and `78BA`) |
| LL | 10^4 | 10303 | `3DD943A77EC9238B` | 3 | 0 Prime95; 0 John Sweeney's Mac program; 3567 Prime95 (and `238B`) |
| LL | 10^5 | 100069 | `1ECC98522D52594C` | 5 | 0, 0, 84821, 33165, 13171, Prime95 (and `594C`) |
| LL | 10^6 | 1000193 | `8F5354551BC18373` | 5 | 0 Prime95; 0 Mlucas; 768486, 586970, 135093 Prime95 (and `373`) |
| LL | 10^7 | 10000139 | `76BB6A7CE0BCA62B` | 6 | six shifts, Prime95 |
| LL | 10^8 | 100000007 | `F9042256B193FAA0` | 12 | eleven shifts Prime95; 50012226 CUDALucas |
| PRP-1 | 10^4 | 28603 | `F899E5F1E2B40323` | 2 | 12731, 16129, Prime95 (one machine, two minutes apart) |
| PRP-1 | 10^6 | 1000193 | `CA0B0B10C609A027` | 4 | 0 gpuowl (two versions); 197579 Prime95; 0 prpll |
| PRP-1 | 10^7 | 10000019 | `9DD181D73794E731` | 4 | 1377971 Mlucas; 0 gpuowl, GpuOwl, prpll |
| PRP-1 | 10^8 | 100005533 | `ADF278E588B2CDCE` | 2 | 13318258 Prime95; 0 prpll |

28603's two results come from one user's one computer, two shifts two minutes apart: they meet the
rule, but they are not independent hardware. Its independent check is Python's integers: the table's
command recomputes it (R9), and so does the gates' second computation (`pow`, design review F12).

**Decades without an accepted PRP type-1 result** (R7's "wherever the reports hold one"):

- 10^3: `report_PRP` for 1000–9999 lists no result at all.
- 10^5: 100000–199999 lists none; 200000–999999 lists six factored exponents, each with one base-3
  type-1 result (and one base-5), so none has two agreeing.

Other decades have many accepted rows; one per test per decade is the minimum, and the engine
cycles append more. 10^8 here means 10^8 up to 136,279,841.

## The Mlucas README (R8)

- `https://www.mersenneforum.org/mayer/README.html`, title "Mlucas README", "Copyright (C) 2021
  Ernst W. Mayer", footer "README.html − Last Revised: 12 Jul 2022"; the page now carries a note
  that the author died on 10 September 2023 and that it is kept for posterity. It is documentation,
  so § Invariants 4 allows reading it; its source tree was not opened.
- Its section "BASIC BUILD SELF-TEST" gives, for `-fft 192 -iters 100`: "100 iterations of M3888509
  with FFT length 196608 = 192 K, final residue shift count = 744463 / Res64: 71E61322CCFB396C",
  and "Res mod 2^35 - 1 = 29259839105", "Res mod 2^36 - 1 = 50741070790". It does not say which
  test the self-test runs.
- Python's integers, s_0 = 4 and s_(k+1) = s_k² − 2 mod M3888509, give s_100 with exactly that
  Res64 and both residues (and s_99, s_101 differ), so the row is the LL residue s_100, start 4,
  k = 100, unshifted. 57 s under Python 3.9 here.

## Bibliography entries this needs (R2)

Keys, sorted into `doc/references.md`: `GIMPSPrimes` (the list), `GIMPSReportLL`, `GIMPSReportPRP`,
`MlucasREADME`. Each is checked against the page as the bibliography's rule says: fetched and read,
title, owner, date. Cite only what the Verified line records: the list's table, the reports'
plaintext output and columns, the README's self-test section. No locator beyond a section heading
that was read. The bibliography carries no claims, so `doc/README.md`'s marker rule does not reach
it (`spec/reference-big-integer/TECH.md`, P5, F1).
