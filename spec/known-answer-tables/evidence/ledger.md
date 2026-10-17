# Deferral ledger for known-answer-tables

Written at P4, 2026-10-17, from a walk of `TECH.md` P1–P4 (the checklists and their two
amendments), the build commits `e87d7bf`, `7470136` and `20daa6e` and their bodies, GOAL's
Non-goals and Notes, PLAN § 5, and what the P1–P3 builds reported. None of them says "do not fix
here", "known limitation" or "left as a follow-up", and no build introduced a deferral; the
deferrals below are the ones GOAL, PLAN and the P4 checklist name. Each names its home: work a seed
already covers goes in that seed's Notes, and work no seed covers gets a seed of its own and a
`ROADMAP.md` entry. The P4 audit found no gap in R1–R12, so it committed no fix.

## Deferrals

- **The factor table** (GOAL, Non-goals and the first Clarification). The known factors and the
  P-1 boundary cases, split out at shaping, become a fourth stem of this format, command and
  loader. Home: `issues/known-factor-tables.md`, Notes ("deferred from `known-answer-tables`"): what
  a new stem touches, and that two factors of one M_p need the factor in the key, or the conflict
  check refuses the second.
- **How an installed `mps` finds the tables** (GOAL, Non-goals). `known::load` takes a path and
  `known::TABLES` names the tables from the repository root, so the loader works from a checkout
  only. Home: `issues/burn-in-mode.md`, Notes ("deferred from `known-answer-tables`"), with
  `issues/packaging-and-install.md` named there for the install side.
- **The exponent list's new home, for the doc-claims check** (GOAL, Non-goals: the copies of the
  list in landed `spec/*/verify/` records stay). Home: `issues/doc-claims-standing-check.md`, Notes
  ("deferred from `known-answer-tables`"): the list now lives in
  `tests/known/mersenne-exponents.txt`, which the check can read when it moves the evaluator out.
- **A standing append-only check** (R4; design review F11). This cycle's gates check once that each
  table on `origin/main` is a prefix of the new one, and no later cycle's gate does unless it asks;
  `make check` reads no history. Home: a new seed, `issues/known-answer-standing-check.md`, and its
  `ROADMAP.md` entry in Part I, after the known answers, "A standing check that the known answers
  only grow". The seed's Notes also carry PLAN § 5's two recorded risks it would meet first: an
  exponent below 136,279,841 listed later (ascending against append-only, a GOAL amendment then),
  and an exponent row's lack of a correction form (a new format version then).

### Open for Geoffrey

- **Fingerprint independence** (GOAL, Notes; recorded in PLAN § 2's correction rule and § 5).
  Whether two `mps` engines that share no arithmetic satisfy § Invariants 3's "command that
  recomputes it with an independent method". No fingerprint row lands this cycle, so it doesn't
  block; until the ruling, a fingerprint supersedes nothing (design review F3). Home: GOAL Notes and
  PLAN § 5, where the first cycle to add a fingerprint (the integer transform, per GOAL's Non-goals)
  meets it; `issues/burn-in-mode.md`'s new bullet points at it too.

### Considered, no action

- **The bibliography rests on the planner's saved fetches.** `GIMPSPrimes`, `GIMPSReportLL`,
  `GIMPSReportPRP` and `MlucasREADME` were checked against the pages as fetched on 2026-10-17, the
  date every published source line in the tables gives, and each Verified line says when and what
  was read. That meets R2: the entries are checked against the work itself, as the existing entries
  are ("its page fetched and read"), and the saved copy is the version the rows cite, which a later
  fetch of a living report would not be. The audit spot-checked the live pages the same day: the
  list's 52 linked exponents equal the table's; `report_LL` for 1237 gives `DF5E6622F83F5BE2` twice
  (shifts 0 and 116) beside a 16-bit `5BE2`; `report_PRP` for 28603 gives `F899E5F1E2B40323` twice
  (shifts 12731 and 16129, type 1, base 3); the README's self-test reads "Res64: 71E61322CCFB396C",
  last revised 12 Jul 2022. No entry changes.
- **No PRP row in the decades 10^3 and 10^5** (R7, PLAN § 2). The reports held no accepted type-1
  result there on 2026-10-17, and R7 asks for one wherever they do. A later retrieval that finds one
  appends it under a new source line; nothing is deferred.
- **The recompute bound in two places** (PLAN, deviation 3). `RECOMPUTED_BELOW` is 200,000 in
  `src/known.cpp` and in `tests/known/generate.py`, and each says beside it that a change to one
  changes the other; that is the durable record of the same-commit obligation, since `AGENTS.md`
  takes only the pinned lines this cycle.

## Times

Each run's check step, from its `result.json`, against cycle 7's run on the same target
(`spec/reference-big-integer/evidence/<target>.json`, which tested `6e8c20d`). All three runs tested
`951eacb` with GCC 14 and passed 91 checks and 644 smoke-test cases, against cycle 7's 81 and 644.

- `x86-64-avx512`: check step 37 s (cycle 7: 32 s), +5 s.
- `x86-64-avx2`: check step 51 s (cycle 7: 45 s), +6 s.
- `aarch64-gb10`: check step 120 s (cycle 7: 106 s), +14 s.

The check step compiles `bin/check` with the standard library's assertions as well as running it,
so the difference holds this cycle's new sources (`src/known.cpp`, about 480 lines, and
`tests/check_known.cpp`, about 460) compiled at `-O3`, plus the eleven new checks less the one
removed: the loader's refusals on its own samples, the three tables read and held to their sources
at run time, and each final residue that `tests/vectors/reference.txt` covers compared with the
low 64 bits of its full residue, at a single node's speed.

R11 itself is measured on the Mac, warm and with nothing to rebuild, against `c41950c`; the last
gate run (the full P4 gate on these runs of `951eacb`, 2026-10-17) gave clang++ 3.56 s at
`c41950c` and 3.57 s now, +0.01 s, and g++-16 6.20 s and 6.25 s, +0.05 s, against the 0.5 s
budget.
