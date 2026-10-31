# 05 — Integration: build, tiers, AGENTS.md, the note, the bibliography, the ledger

**Question.** What in the repository must change around the engine, and under which same-commit
rules? Scope: Makefile, the check runner, AGENTS.md (R13), `doc/weighted-transforms.md` and its
markers (R13), `doc/references.md`, the known-answer tables, the deferral ledger.

## Sources read

`Makefile` (f956a7c, after cycle 9's settings record), `tests/check.{hpp,cpp}`, `tests/cli.sh`,
`AGENTS.md` (§ Build and run, § Map, § Factory, § Invariants, § Conventions), `doc/README.md`
(marker rule, line 18) and decision `2026-10-16-mpsearch-doc-markers` as the repo card records it,
`spec/math-notes-and-references/verify/notes.py` (its `claims` mode), `doc/references.md`,
`tests/known/*` and `generate.py`'s header, `issues/large-gcd-and-jacobi.md`,
`issues/engine-differential-standing-check.md`, `ROADMAP.md` Part VIII, and the closest models'
PLAN/TECH/verify (`known-answer-tables`, `reference-big-integer`, `objects-follow-the-compiler`).

## Findings

1. **Build.** `MODULES` gains `fft ibdwt lengths`; every object already compiles with `$(IEEE)`
   (R10 needs no flag change, and the gate forbids any). `bin/measure` links `lib/measure.o` with
   the modules, never into `bin/mps`; it joins `BUILT` (so `make clean` and a settings change
   remove it) and `DEPS`. `make lengths` rewrites `src/lengths.txt` through a `.new` file and prints
   the table's rows. All within GNU make 3.81.
2. **Two tiers in one binary.** A `check::Tier` (DEFAULT, ENGINE) on each registered case; `bin/check`
   runs the default tier, `bin/check engine` the engine tier. `make check`'s recipe is unchanged;
   `make check-engine` runs make check's two steps and then `$(CHECK) engine`, each always, the
   status failing if any did. The engine tier's sources sit in `tests/*.cpp` with the rest, so the
   Makefile's wildcard and the settings record cover them with no new pattern rule. Unknown runner
   arguments exit 2. The profile's `verify` stays `make check` (Geoffrey's ruling; GOAL Non-goals).
3. **AGENTS.md (R13).** In the commit that creates them: high-blast-radius lines for `src/fft.*`,
   `src/ibdwt.*`, `src/lengths.*`, and `src/lengths.txt` with `src/measure.cpp`; § Map bullets for
   each, and for `tests/check_engine.cpp` and `tests/engine_support.*`; § Build and run lines for
   `make check-engine` and `make lengths`. P1 creates every one of those paths and the target, so
   all of R13's AGENTS.md lines land in P1's commit; the list only widens.
4. **The note and its markers (R13; doc/README.md line 18 with the 2026-10-16 ruling).**
   Definitions, format constants and theorem values take no markers; instance values and
   restatements do. `notes.py claims` (cycle 6's tool, the README names it) additionally refuses
   any integer of 100 or more outside a marker pair and reads `0.4375` as the integer 4375, so the
   note writes the default threshold as 7/16. The gate reads digits *and number words*
   ("two", "twice", "million", ...) in every added line, allows unmarked only a pinned list of
   definitions (15, 2^20's 20, 0.35, 7/16, 2^50/2^51/2^52's exponents, 53, 64, radices 2 and 4,
   formula constants 0–2), and refuses any value of the table or the measurement in any form,
   marked or not: the note points at `src/lengths.txt` and `src/lengths.cpp`, as cycle 7's fix and
   `doc/reference-arithmetic.md`'s Karatsuba threshold do. The stale "What mps uses" sentences
   ("does not exist yet", "keeps decimal digits") go.
5. **Bibliography.** New comments cite Cooley–Tukey 1965 (`src/fft.*`) and Steele, Lea and Flood
   2014 (SplitMix64, `src/measure.cpp`); `doc/references.md` lists every work a `src/` comment
   cites, so both need entries (keys `CooleyTukey1965`, `SteeleLeaFlood2014`), each with a Verified
   line from the builder's own check of the DOI metadata.
6. **Known answers.** The tables reach 10,000,019 already; R5 needs no row. The plan appends none,
   and the gate forbids any change under `tests/known/` (the standing append check is after Part
   III, so a change here would go unguarded).
7. **The one known gap for the ledger.** `issues/large-gcd-and-jacobi.md` needs plain,
   zero-padded, unweighted multiplication of operands of *varying* sizes (its Notes say so); R14's
   product is modulo M_p at the engine's lengths. ROADMAP Part VIII's "Depends on: the transform
   engine's general multiply" for the GCD entry is half true: the engine gives multiplication mod
   M_p (enough for P-1's stage products), not the general product the half-GCD needs. P4's ledger
   adds a Notes bullet to that seed saying so ("deferred from `ibdwt-transform-engine`").
8. **Commit and site hygiene.** Subjects ≤ 72 characters; no cluster or site names in commits or
   files; the pre-push scan reads `~/.config/mpsearch/site.env` (cycle 9's `cluster.sh`, reused).

## Recommendation

As above; the PLAN pins the AGENTS.md paths, the Makefile targets and recipe shape, the runner's
tier API, the two reference keys, the note's required contents and its exempt numbers.
