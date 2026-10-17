# REVIEW — The known answers: exponents, residues and fingerprints, each with its source

> Adversarial QA by `code-review`, run in an isolated context (a fresh subagent at xhigh). The
> correctness pass grades the branch diff against [`GOAL.md`](GOAL.md) and the repo's `AGENTS.md`
> § Invariants **only**. It did not see `PLAN.md` or `TECH.md`. Every finding cites an **executed**
> command.

- **Reviewed commit:** `f108547` (blind pass on `68de546`; `f108547` adds only the out-of-Scope
  home below, in `issues/`) · **Base:** `c41950c` (`origin/main`) · **Date:** 2026-10-17
- **Verdict:** approved
- **Cycle:** 1 of ≤3
- **Contract drift check:** `GOAL.md` has one commit after the shape commit, `df5e5ab`. That is the
  foreman's GOAL sign-off, made before the plan existed. It changed the 13 Clarification markers from
  "proposed" to "confirmed" and named the follow-up seed `issues/known-factor-tables.md`, with no
  R-ID changed. The contract did not move mid-build.

## Verification run

- **`make check`:** clean builds, no warnings, on Apple clang 21 and g++-16: 91 checks and 644 smoke
  cases each. Every build commit passes `make check` on its own.
- **23 negative controls on the real tables, in a scratch clone.** Each failed `make check` (exit 2),
  naming `path:line`:
  - lowercase Res64, trailer miscount, a row above every source line, a deleted source line;
  - conflicts in the final and partial tables;
  - fingerprints with one engine, with two runs of one engine, conflicting with a computed row, and
    superseding;
  - `agreed=` missing, a CRLF line, exponents out of order;
  - an unknown bibliography key, a renamed entry;
  - 1237 and 1277 off by one;
  - an exponent under the computed source near the bound.
  - Green controls: a correction row, an agreeing fingerprint, a fingerprint on a new key.
- **Fuzzing:** 120,000 mutated tables under ASan and UBSan; each one either loaded or raised
  `known_error`.
- **Published sources re-read on 2026-10-17, about 6.5 h after the author's retrieval:**
  - mersenne.org/primes lists the same 52 exponents in the same order;
  - every final row matches `report_LL` / `report_PRP`, with `agreed=` equal to the number of full
    64-bit residues at different shifts or programs, and no error code or bad result;
  - the two missing PRP decades (10^3, 10^5) hold no accepted row;
  - the Mlucas README (lines 403–404) gives s_100 of M3888509 = 71E61322CCFB396C;
  - all four bibliography entries' Verified claims hold.
- **Independent recomputation:** `pow()`/`%` agreed on all 14 partial rows with p ≤ 100069 and on the
  final rows 1237, 1277, 10303 and 28603.
- **Reproduction:** all three tables reproduce byte for byte with `generate.py` under Python 3.9.6;
  final and partial also under 3.14. The generator's controls each exit 1, naming the row.
- **R11:** seven interleaved no-rebuild runs each: base median 3.52 s, head median 3.51 s.
  `tests/cli.sh` is unchanged and passes 644, and `bin/mps` behaves identically on 14 invocations.

## Requirement → evidence matrix

Every R-ID was graded by the blind reviewer. The orchestrator graded P4's ledger and cluster runs
(`evidence/`) from the job ids and `sacct`. R3's "the plan's gates" clause and R11's "the plan
records the baseline" clause live in `spec/`; the orchestrator re-ran every phase gate green
(P1–P4).

| R-ID | Implemented by | Verified how | Status |
|------|----------------|--------------|--------|
| R1 | `tests/known/` (three tables), `src/known.*` in `MODULES` | grep for other parsers; vector reader's stem and op removed | ✅ |
| R2 | one `# from` line per row; four new `doc/references.md` entries | controls C12, C13; entries re-read | ✅ |
| R3 | the loader's refusals | 13 controls, each naming `path:line`; fuzz | ✅ |
| R4 | the 22 records first, in order; the supersedes form | `cmp` of the prefix; C11; vector families regenerate identically | ✅ |
| R5 | `mersenne-exponents.txt`, 52 rows | the live primes list; byte-for-byte regeneration; drop-9941 and non-prime controls | ✅ |
| R6 | `REFERENCE_UP_TO = 10'000` | C16 (all 22 at 9949), C17 (10007 untested) | ✅ |
| R7 | `final-residues.txt`, 11 rows | every row against the live reports; decade coverage | ✅ |
| R8 | `partial-residues.txt`, 20 rows | the README value; independent recomputation | ✅ |
| R9 | the R9 check; the command's recomputation | C14, C15; generator controls | ✅ |
| R10 | the `run=` form | C7–C10b | ✅ |
| R11 | — | median timings; cli.sh; `bin/mps` identical | ✅ |
| R12 | high-blast-radius and § Map lines in `e87d7bf`, `7470136`, `20daa6e`; the `COMPOSITE` comment | `git log` per commit | ✅ |

Unmapped changes: the deferral records (ROADMAP entries, the seeds `known-answer-standing-check` and
`known-factor-tables`, notes in the burn-in, doc-claims and LL seeds, the seed's status), and loader
rules beyond the GOAL that support R3 and R4. All are sanctioned by `AGENTS.md` § Factory, and their
prose matches the code.

## Findings

None in Scope: no CONFIRMED and no PLAUSIBLE finding.

### Out of Scope
- **O1. Stale wording in three doc notes.**
  - `doc/lucas-lehmer.md:114`, `doc/prp-and-gerbicz.md:118` and `doc/reference-arithmetic.md:75`
    still say `make check` runs the reference tests "on the published Mersenne prime exponents". They
    now run on the exponents from 3 to 9941, sourced `computed`.
  - `doc/reference-arithmetic.md:77` names only `tests/vectors/` as what they are checked against.
  - Out of Scope because the rubric's drift item covers `AGENTS.md`, and the doc-update convention
    applies to algorithm changes, which this diff doesn't make.
  - **Home:** a Notes bullet in `issues/lucas-lehmer-test.md`, the cycle that rewrites those
    sections (`f108547`).

## Reviewer disclosures

- One unfiltered grep printed three lines of `spec/` text: one from this cycle's research 03, two
  from `spec/math-notes-and-references/PLAN.md`, all naming which code used `OEIS_A000043`.
- A `git show --stat` listed `spec/` file names.
- Neither bore on a grade.

## Checks not run

- The cluster targets (GCC 14) were not run by the reviewer. P4's runs passed on all three
  (`evidence/`).
- No independent recomputation for p ≥ 10^6. The final rows there rest on the reports' agreement,
  as the GOAL allows. The partial rows rest on the generator's recomputation, which the gates check
  by tracing and by a second method.

## Human-gate triggers

- None. No CONFIRMED finding in any cycle touched a path on § Invariants' high-blast-radius list.

## Optional completeness sub-pass

- Not run.
