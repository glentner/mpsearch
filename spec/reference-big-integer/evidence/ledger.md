# Deferral ledger for reference-big-integer

Written at P5, 2026-10-16, from a walk of `TECH.md` P1–P5 (the checklists and their amendments),
the build commits `dad84ee`, `bd67c78`, `8694143`, `a583c49` and the audit's `b8863f0`, their
bodies, GOAL's Non-goals and Notes, and what the P1–P4 builds reported. None of them says "do not
fix here", "known limitation" or "left as a follow-up"; the deferrals below are the ones GOAL and
the checklist name, and the ones the build ran into. Each names its home: work a seed already
covers goes in that seed's Notes, and work no seed covers gets a seed of its own and a `ROADMAP.md`
entry.

## Deferrals

- **The balanced digit form** (GOAL, Non-goals). The transform's balanced variable-base digit form
  and its round-trip with `BigInt` are the IBDWT cycle's. Home: `issues/ibdwt-transform-engine.md`,
  Notes ("deferred from `reference-big-integer`"), building them on `BigInt::from_limbs` and
  `limbs()`; its `ROADMAP.md` entry, "Squaring modulo 2^p − 1 with a weighted transform", now names
  the balanced digit form.
- **The prime list** (GOAL, Notes). The exponents up to 10000 live in
  `tests/vectors/mersenne-exponents.txt` with their source and command. Home:
  `issues/known-answer-tables.md`, Notes: that cycle takes the file over as its single table, keeps
  the old path on the high-blast-radius list if it moves it, and gives rows above 10000 a published
  source, since the file's command cannot recompute them.
- **The published residues of M1237 and M1277** (the audit; the foreman's item 4). The generator's
  comment said GIMPS publishes them, and nothing in `doc/references.md` backs it. The audit
  (`b8863f0`) made the comment point to `issues/known-answer-tables.md`, which records the values
  and the GIMPS reports they come from; citing them belongs to that cycle (GOAL, Non-goals: the
  published GIMPS residues are the known-answer cycle's). Home: `issues/known-answer-tables.md`,
  Notes: when that cycle lands the residues with their sources, it repoints the comment at its
  table.
- **Objects that don't follow the compiler** (the P1–P4 builds; the foreman's item 1). `lib/` and
  `lib/check/` objects don't depend on `CXX`, so a plain `make` after a build with the other
  compiler links stale objects and fails; the profile's `verify` documents `make -B check` as the
  workaround. Pre-existing on `main`, outside this cycle's Makefile diff (one `MODULES` line), and
  no existing seed fits: `issues/packaging-and-install.md` sits in Part III, too late for a defect
  every builder meets now. Home: a new seed, `issues/objects-follow-the-compiler.md`, and its
  `ROADMAP.md` entry at the end of Part I.
- **`.gitignore` lacks `__pycache__/`** (the P1–P4 builds; the foreman's item 2). Importing
  `tests/vectors/generate.py` as a module leaves `tests/vectors/__pycache__/`; the files' own
  commands run it as a script, and it imports only the standard library, so they leave nothing.
  Too small for a seed of its own. Home: `issues/objects-follow-the-compiler.md`, Notes, as a
  one-line rider in the same area.

### Considered, no action

- **`expect_mp_error` in two files** (the foreman's item 3). `tests/check_mp.cpp` and
  `tests/check_reference.cpp` each hold an identical file-local copy, as PLAN § 2 pins it. It is a
  test helper, not a rule, so § Conventions' "point to a rule" does not bind it, and two copies of
  eighteen lines cost less than widening the runner's interface (`tests/check.hpp`) in a cycle whose
  footprint the gates fix. When a third check file needs it, it belongs in `check.hpp`; whoever adds
  that file moves it.
- **The decimal round trip above 2^17 bits** (P3 amendment). For the two largest operands the
  decimal check stops once the parse gives back A, saving 0.6 s of R10's budget. GOAL R3 holds in
  full (conversion equals the vector, the parse gives back A); the extra print of the parse is
  PLAN's, kept below 2^17 bits, and the gates' probe round-trips its own cases. Nothing is deferred.
- **KaratsubaOfman1962 without a locator** (P2 amendment). Math-Net.Ru refused connections on every
  try, so the entry is checked against MCA's bibliography and its section 1.3.2, which the code and
  the note follow, and its Verified line says so, as the bibliography's rule allows. No seed builds
  from the 1962 paper itself; a cycle that changes Karatsuba rereads the record then.
- **The division's correction** (P3 amendment). MCA Algorithm 1.6's add-back loop, not either
  option PLAN named; a design choice within the builder's latitude, covered by the vectors and the
  gates on all three paths. Nothing is deferred.

## Times

Each run's check step, from its `result.json`, against cycle 6's run on the same target
(`spec/logger-state-per-translation-unit/evidence/<target>.json`). All three runs tested `6e8c20d`
with GCC 14 and passed 81 checks and 644 smoke-test cases.

- `x86-64-avx512`: check step 32 s (cycle 6: 27 s), +5 s.
- `x86-64-avx2`: check step 45 s (cycle 6: 34 s), +11 s.
- `aarch64-gb10`: check step 106 s (cycle 6: 76 s), +30 s.

The check step compiles `bin/check` with the standard library's assertions as well as running it,
so the difference holds this cycle's new sources (`src/mp.cpp` grew by about 650 lines,
`src/reference.cpp`, `tests/check_mp.cpp`, `tests/check_reference.cpp` and
`tests/vector_reader.cpp`) compiled at `-O3`, plus the 26 new checks, the reference tests up to
p = 9973 and the 2^20-bit conversions among them, at a single node's speed. R10 itself is
measured on the Mac, warm and with nothing to rebuild; the last gate run (the P5 gate on these
runs of `6e8c20d`, 2026-10-16) gave clang++ 1.14 s at `c9b82ff` and 3.55 s now, +2.42 s, and
g++-16 3.92 s and 6.22 s, +2.30 s, against the 3.0 s budget.
