# 00 — Digest

Three briefs, written in sequence by the planner (no subagents in this delegated run). Triggers:
2 (high blast radius: `tests/vectors/`, `tests/vectors/mersenne-exponents.txt`, and the new tables)
and 3 (the deliverable is checks and their data). Appetite small, so three topics.

## Decisions

1. **Directory and files** ([03](03-format-loader-integration.md) §1–2): `tests/known/` with
   `mersenne-exponents.txt`, `final-residues.txt`, `partial-residues.txt` and the command
   `tests/known/generate.py`. The exponent file moves there with its 22 record lines byte for byte.
2. **Format** ([03](03-format-loader-integration.md) §3–4, §6): `# mps known 1`, the command line,
   notes, then `# from …` source groups and rows, then `# end <n> rows`. Corrections by
   `supersedes=<Res64> because <reason>`. Fingerprints are partial-table rows under `# from runs`.
3. **Loader** ([03](03-format-loader-integration.md) §5, §7): `src/known.*`, `known::parse` and
   `known::load`, `known_error` naming `<path>:<line>`; `make check` tests it on its own samples.
4. **Published rows** ([01](01-published-sources.md)): 52 exponents from the GIMPS list; eleven
   final residues by the seed's acceptance rule, one LL and one PRP type-1 per decade where the
   reports hold one (no PRP row at 10^3 or 10^5: none accepted); s_100 of M3888509 from the Mlucas
   README, confirmed as the LL residue from 4. All retrieved 2026-10-17.
5. **Computed rows** ([02](02-computed-rows-and-cost.md)): twenty partial rows, LL and PRP at
   1277, 10303, 100069, 1000193, 3888509 (PRP only) and 10000019, with k ≥ ⌈log2 p⌉ + 8.
6. **Recomputation bound** ([02](02-computed-rows-and-cost.md)): the command recomputes every
   published final row with p < 200,000 (1237, 1277, 10303, 28603, 100069) and the published
   partial row; the larger rows rest on published agreement.
7. **R11**: baseline at `c41950c` 3.52 s (clang++) and 6.13 s (g++-16), least of 7 interleaved runs.

## Contradictions resolved

- R8's decade range: 03 had four decades; 02 adds 10000019 so the reading that counts 10^7 itself
  holds too, at about 53 s of command time.
- The GOAL's `txt=1` versus `txt=2`: the same report, HTML-wrapped or plain (01). The command embeds
  the values either way and never fetches.
- 1000003's LL result set carries an error code, so 1000193 serves both tests at 10^6 (01).
