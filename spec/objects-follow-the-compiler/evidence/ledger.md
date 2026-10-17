# Deferral ledger for objects-follow-the-compiler

Written at P2, 2026-10-17, from a walk of `TECH.md` P1–P2 (the checklists), the build commit
`55d2fb9` and its body, GOAL's Non-goals and Clarifications, PLAN § 3 (the deviation) and § 5, and
what the P1 build reported. None of them says "do not fix here", "known limitation" or "left as a
follow-up", and the build introduced no deferral; the entries below are the ones GOAL, PLAN and the
P2 checklist name. Each names its home: work a seed already covers goes in that seed's Notes, and
work no seed covers would get a seed of its own and a `ROADMAP.md` entry (none needed one). The P2
audit walked R1–R9 against the `Makefile`, `AGENTS.md` and `.gitignore` and found no gap, so it
committed no fix.

## Deferrals

- **The install target and backends keep the settings record** (GOAL, Non-goals; PLAN § 6).
  `issues/packaging-and-install.md` reshapes the `Makefile` later. Home:
  `issues/packaging-and-install.md`, Notes ("deferred from `objects-follow-the-compiler`"): an
  install target and any backend keep `lib/settings`, the reset before any target and `BUILT` as
  what `make clean` removes, and add their own outputs to `BUILT`; the same bullet notes that a
  compiler version, if the build identity ever probes one, can join the record.

### Accepted behaviour

- **The reset runs whatever the goal** (PLAN § 3, the deviation; § 5, "A dry run removes stale
  outputs"). When this run's settings differ from `lib/settings`, the reset removes `BUILT` while
  make reads the `Makefile`, so `make -n`, `make -q`, `make clean` and `ci/test-commit.sh`'s compiler
  probe remove stale outputs too. Why it stays: the removal has to happen before make stats any
  target, or a switch in the same second as the last build keeps stale objects (make 3.81 here keeps
  whole-second timestamps, PLAN § 4). Guarding it under `-n` or `-q` would make a dry run report
  stale objects as up to date. What it removes is only what the next real build with those settings
  rebuilds anyway, `make clean` removes it regardless, and in a fresh clone (where the CI probe
  runs) there is nothing to remove. No home: this is the design, recorded here and in PLAN § 3.

### Considered, no action

- **A compiler upgraded in place under the same name** (GOAL, Non-goals; PLAN § 5). The record holds
  `CXX` as a name, so an Xcode or Homebrew update, or a `PATH` change, is not seen. `AGENTS.md`
  § Build and run says `make -B` (the pinned edit), and no R-ID covers it. Probing the compiler's
  version on every make would spend part of R4's 0.1 s on each no-op run for a case that `make -B`
  already answers. Nothing is deferred; the packaging seed's bullet names where it could be picked
  up, should a build identity probe the version anyway.
- **`make -j`** (PLAN § 5). Not a supported mode, and no gate runs it. Within one make, the record is
  a single order-only target that make builds once, before any object that names it, and the reset
  happens at parse time, before any job starts, so nothing this cycle added races. Two makes in one
  tree at once were never safe and are not made so. No seed asks for parallel builds; nothing is
  deferred.
- **A settings value containing a newline** (PLAN § 5). It cannot round-trip through `$(shell)`
  and the recipe that writes the record. None is plausible on make's command line, and nothing is
  deferred.
- **The settings compared as text** (PLAN § 5). `CXXFLAGS='-O2  -g'` and `CXXFLAGS='-O2 -g'` differ
  and rebuild. Harmless: a needless rebuild, never a stale link.
- **R5's "no file"** (PLAN § 5). `make clean` leaves the empty `lib/`, `lib/check/` and `bin/`
  directories, as `b3a6c5a`'s did; `.gitignore` and `clean_paths` cover them. Nothing is deferred.
- **The roadmap entry still quotes the old workaround.** `ROADMAP.md`'s entry for this cycle's seed,
  and the seed itself, quote "`make -B check` after changing `CXX`". They are the frozen deferral
  pair, and publishing the cycle retires both; this cycle leaves its seed `adopted:` as it is.

## Times

Each run's check step, from its `result.json`, against cycle 8's run on the same target
(`spec/known-answer-tables/evidence/<target>.json`, which tested `951eacb`). All three runs tested
`14d993a` with GCC 14 under GNU Make 4 and passed 91 checks and 644 smoke-test cases, as cycle 8's
did, with the same compiler and make on each target.

- `x86-64-avx512`: check step 36 s (cycle 8: 37 s), -1 s.
- `x86-64-avx2`: check step 51 s (cycle 8: 51 s), +0 s.
- `aarch64-gb10`: check step 121 s (cycle 8: 120 s), +1 s.

The check step compiles `bin/check` with the standard library's assertions as well as running it,
and this cycle changes no source and no check, so the differences are a single node's noise. Each
check step compiled only the `lib/check/` objects and linked `bin/check`; none recompiled a
`lib/*.o` or relinked `bin/mps` that the build step had built with the same settings (R3 under GNU
Make 4, the record compared rather than timed).

R4 itself is measured on the Mac, warm and with nothing to rebuild, against `b3a6c5a`; the full P2
gate on these runs of `14d993a` (2026-10-17) gave clang++ 3.769 s at `b3a6c5a` and 3.774 s now,
+0.006 s, and g++-16 6.492 s and 6.542 s, +0.050 s, against the 0.1 s budget (make's own share
0.015 s to 0.022 s and 0.014 s to 0.021 s).
