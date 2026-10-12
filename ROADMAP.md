# MPSearch Roadmap

`mps` tests Mersenne numbers 2^p − 1 for primality. This document records the work between today's
scaffold and the finished program. That program runs the same mathematics on CPUs and GPUs, with TPUs
an exploration after 1.0. It verifies every known Mersenne prime at a speed and efficiency that rival
the established programs on the same hardware. And it serves as a burn-in and regression test for HPC nodes: a known exponent
has a known answer, so a wrong residue flags faulty hardware, and an expected walltime, so a slow
node stands out.

This is a **forward-looking index, not an implementation plan.** Each entry states a problem and the
intention behind solving it; the *how* belongs to `code mpsearch plan`. As-built architecture and
the load-bearing invariants live in [`AGENTS.md`](AGENTS.md), which stays ground truth.

Each entry's **Seed** points at an [`issues/{slug}.md`](issues/) file. The seed carries the evidence
in enough detail that a later session doesn't have to re-derive it. Seeds are **candidates, not
contracts**. `code mpsearch feature` promotes one into `spec/{slug}/GOAL.md`, and that promotion is
where a human negotiates appetite, non-goals and the R-IDs the review will grade. The `status:` field
on each seed is the guard:

- `unshaped`: a raw deferral;
- `shaped`: negotiated, not yet accepted into a cycle;
- `adopted:{slug}`: promoted. `code mpsearch roadmap` retires it from this index once its cycle lands
  on `main`.

Two further values, `declined` and `accepted-behaviour`, close a question without shipping. Those
are listed under *Settled questions* at the end.


**Order is deliberate.** Each Part is a milestone, and the Parts run in order:

- correctness before speed;
- the operators' burn-in on CPUs as soon as there is something correct to burn in with;
- NVIDIA GPUs next, Blackwell first, because a large Blackwell deployment is the next thing to burn
  in;
- then CPU speed, the Mac's GPU, AMD GPUs, and the rest of the trio;
- the TPU after 1.0, as an exploration.

Within a Part, the stated dependencies are the hard constraints, and the order is a recommendation.

**Done** means all three of these:

1. **Verified.** Every known Mersenne prime exponent ends in a zero Lucas–Lehmer residue on at least
   one `mps` engine per platform class: Apple CPU, Apple GPU, x86-64 CPU, Arm Linux CPU, NVIDIA GPU,
   AMD GPU. The largest exponents run as cluster campaigns.
2. **Rivals the incumbents.** On every platform class, `mps` is measured side by side with the
   best established program on the same machine:

   | Platform class | Established program | Bar for `mps` |
   |---|---|---|
   | x86-64 CPUs | Prime95/mprime | at least 80% of its whole-device throughput |
   | Arm CPUs | Mlucas | at least its throughput (stretch: 1.25×). It is the only Arm program, and it trails mprime by about half on x86 |
   | NVIDIA and AMD GPUs | PRPLL-NTT | at least 80% of its whole-device throughput |
   | Apple GPU | PrMers, through OpenCL | at least 2× its throughput. It manages about 6.1 ms per iteration at the largest exponent on an M4 Pro. The share of the memory-bandwidth roofline is reported too |

   The comparisons use the yardstick exponents 77,936,867 and 136,279,841. Each program runs its own
   best transform and layout, with production error checks on, and must produce the same 64-bit
   residues. Energy per iteration is reported beside time wherever the platform exposes it, and should
   come within 1.25× of the established program's, read on the same meter.
3. **Burns in.** Every device class has burn-in profiles with known answers and expected walltimes.

The trio is complete as well: trial factoring, P-1 and the primality tests, each on every engine it
suits.

**Not on this roadmap.** Two things are left out until decided otherwise:

- Searching untested exponents and reporting to GIMPS's PrimeNet. Its terms bind participants
  whatever software they run.
- ECM, which is useful only for fully factoring small Mersenne numbers; GIMPS's published factors
  already supply the factoring test cases.

## Delivered to date

The 2024 scaffold. It has:

- a C++20 command-line program with a hand-rolled option parser (stacked short options,
  `--opt=value`, colourized help);
- a header-only logger whose detailed style stamps the time, the hostname and `TASK_ID` for fleet
  runs;
- exit statuses by class;
- a sieve of Eratosthenes (`mps -p N`).

The three tests are stubs, and there is no multiple-precision arithmetic yet.

---

# Part I — Foundation

The groundwork every later cycle grades itself against:

- a way to test any commit on a cluster;
- a result type that says what was found;
- a reference big integer;
- the known answers with their sources;
- the mathematics written down.

## Test any commit on a cluster, built fresh from GitHub

Every target beyond the author's Mac lives on Slurm clusters. A batch script takes a commit hash and a
target. It clones the public repository into a fresh directory in scratch, builds that exact commit
with the target's toolchain, runs the tests, and writes a machine-readable result. A cycle tests a
phase on a target by pushing its branch and submitting the job with the hash.

*Horizon: near-term · Depends on: the test target (cleared 2026-10-11) · Refs: —*
**Seed:** [`issues/cluster-test-jobs.md`](issues/cluster-test-jobs.md)

## A standing check for the cluster test job

`make check` never runs the cluster test job or reads its target files, and the cycle that added
them proved them with phase gates that become records once it merges. A small check in `make check`,
with a local fixture repository and a stand-in module command and compiler, runs the job's happy
path and its main failure paths and validates every target file, so a regression shows up on the
Mac before any cluster time is spent.

*Horizon: near-term · Depends on: test any commit on a cluster · Refs: cluster-test-jobs P4, 2026-10-12*
**Seed:** [`issues/ci-job-standing-checks.md`](issues/ci-job-standing-checks.md)

## Say what was found: verdicts, exit statuses, and the options that don't parse

A run prints no verdict. The three tests return `bool` with mixed meanings, and `main` discards
them. Four option-parsing bugs reproduce on `main`, including `--file` setting the test name and help
offering test names the parser rejects. A three-way result type, one verdict line, exit statuses that
tell prime from composite from error, and a `--json` record give every later test something to
return and the fleet something to read.

*Horizon: near-term · Depends on: — · Refs: factory adoption review, 2026-10-11*
**Seed:** [`issues/results-and-cli-fixes.md`](issues/results-and-cli-fixes.md)

## One logger state for the whole program

The logger's style, level, hostname and task id are `static` in a header, so each source file has
its own copy. `-D` and `-d` reach only `main.cpp`'s messages; every other file logs at the
zero-initialised level, `DEBUG`, in the default style, without the hostname and `TASK_ID` stamp the
fleet reads. One shared state makes the options and the stamp apply to every message.

*Horizon: near-term · Depends on: — · Refs: build-and-test-foundation research, 2026-10-11*
**Seed:** [`issues/logger-state-per-translation-unit.md`](issues/logger-state-per-translation-unit.md)

## The mathematics, written down: notes and references

The author's 2024 TODO, "a comprehensive listing of notes and references", becomes `doc/`: short
notes on each algorithm family and a reference list with DOIs. The clean-room rule makes it
load-bearing. Every algorithm here comes from the mathematics and the papers, and builders start from
the note, never from another program.

*Horizon: near-term · Depends on: — · Refs: —*
**Seed:** [`issues/math-notes-and-references.md`](issues/math-notes-and-references.md)

## A reference big integer to check every engine against

`BigInt` holds decimal digits and can only print them. Every fast engine to come fails in ways that
still produce plausible residues, so each must be tested against an arithmetic simple enough to be
obviously right. A binary-limb integer becomes that oracle: schoolbook and Karatsuba products,
folding reduction modulo 2^p − 1, and the transform digit form. It also runs Lucas–Lehmer and PRP for
small exponents before any transform exists.

*Horizon: near-term · Depends on: the test target (cleared 2026-10-11) · Refs: —*
**Seed:** [`issues/reference-big-integer.md`](issues/reference-big-integer.md)

## The known answers: exponents, residues and factors, each with its source

The fixed points every verdict test and burn-in run compares against:

- the known Mersenne prime exponents;
- published residues of composite exponents;
- known factors;
- burn-in fingerprints: residues after a fixed number of iterations at a fixed length, agreed by two
  independent engines.

Every row carries its source, and the tables are append-only (§ Invariants 3).

*Horizon: near-term · Depends on: the reference big integer, for computed rows · Refs: —*
**Seed:** [`issues/known-answer-tables.md`](issues/known-answer-tables.md)

---

# Part II — Correct on the CPU

The primality tests on a portable FP64 weighted-transform engine, written for correctness before
speed. At the end of this Part, `mps` verifies every known Mersenne prime up to a few million on any
CPU. It has checkpoints, the Jacobi and Gerbicz error checks, and a benchmark that checks its own
answer.

## Squaring modulo 2^p − 1 with a weighted transform (FP64, portable)

The core of everything after it: Crandall and Fagin's irrational-base discrete weighted transform.
Reduction modulo 2^p − 1 comes free, with roughly 17–20 bits per word, so the largest known exponent
fits about 7.5 million words. This entry defines the engine contract every backend implements, and
builds the first, portable implementation with a round-off monitor and a measured table of lengths
and maximum exponents.

*Horizon: near-term · Depends on: the reference big integer; the known answers · Refs: —*
**Seed:** [`issues/ibdwt-transform-engine.md`](issues/ibdwt-transform-engine.md)

## Lucas–Lehmer with error checks, shifts and checkpoints

The test the definition of done is stated in. It runs on the engine with:

- a random shift, so reruns are independent;
- a periodic Jacobi-symbol check;
- checks for the degenerate states that fake a prime;
- versioned, checksummed, atomically replaced checkpoints that it resumes from without changing the
  answer.

*Horizon: near-term · Depends on: the transform engine · Refs: —*
**Seed:** [`issues/lucas-lehmer-test.md`](issues/lucas-lehmer-test.md)

## PRP with the Gerbicz error check

Base-3 PRP with Gerbicz's check catches nearly every arithmetic error within the block where it
happens, then recovers from the last verified state. That makes it the strongest in-flight
hardware-fault detector for this arithmetic, and the default for every burn-in profile.

*Horizon: near-term · Depends on: the transform engine; the checkpoint format from Lucas–Lehmer ·
Refs: —*
**Seed:** [`issues/prp-gerbicz-test.md`](issues/prp-gerbicz-test.md)

## `mps bench`: time per iteration that compares across machines

A repeatable timing at a fixed exponent, length, iteration count and layout, with warm-up and
repeats. It ends on a known residue, so a fast wrong engine can't look like a win. Every performance
claim on this roadmap and every burn-in walltime is measured with it.

*Horizon: near-term · Depends on: the transform engine · Refs: —*
**Seed:** [`issues/benchmark-mode.md`](issues/benchmark-mode.md)

---

# Part III — Burn-in on CPUs

The use the operators need first. It runs on the CPU engine as soon as Part II lands, and each later
backend adds its device's profile.

## `mps burnin`: known answers and expected walltimes for node acceptance

Profiles of workloads with expected residues from the known-answer tables and a fresh random shift.
They run from a few minutes to a 24-hour soak, from cache-resident to memory-bound, and per core
where cores can be pinned. A wrong residue fails the node, and so does any internal check trip, even
when the run recovers. A walltime outside its node class's band flags the node slow, even when the
answer is right. Fleet studies put silent-corruption defects at about 1 CPU in 1,000. They found the
most at the re-installation step before production, which is this test's moment.

*Horizon: near-term · Depends on: PRP, Lucas–Lehmer, `mps bench`, the known answers · Refs: —*
**Seed:** [`issues/burn-in-mode.md`](issues/burn-in-mode.md)

## Fleet runs: telemetry, batch-scheduler recipes, and the outlier report

Energy, temperatures, clocks, throttle reasons and memory error counters go into each record, read
without root and without linking vendor libraries. Linux CPU energy comes from the scheduler, since
RAPL is root-only. Batch templates run one exclusive burn-in per node. `mps report` flags outliers
against their hardware class (median and MAD) and their own history, and never changes scheduler
state.

*Horizon: near-term · Depends on: `mps burnin` · Refs: —*
**Seed:** [`issues/fleet-runs-and-reports.md`](issues/fleet-runs-and-reports.md)

## Install it on a cluster: `make install`, backends, a container, a release

An install target, a build identity in `--version` and in every record, a container recipe, and
signed releases. A fleet comparison is meaningless across mixed builds.

*Horizon: near-term · Depends on: `mps burnin` · Refs: —*
**Seed:** [`issues/packaging-and-install.md`](issues/packaging-and-install.md)

---

# Part IV — NVIDIA GPUs, Blackwell first

The next burn-in is a large deployment of NVIDIA RTX PRO 6000 Blackwell GPUs. These GPUs issue 2
FP64 results per SM per clock against 128 FP32, so their fast path is an exact integer transform, not
FP64. The integer engine is proven on the CPU first, where it also becomes the second, independent
engine the burn-in fingerprints need. CUDA then carries it, the FP64 engine, and their hybrid to every
NVIDIA class, and each class gets its own burn-in profile.

## Exact integer transforms for hardware without fast FP64

A number-theoretic transform over GF(M31²)×GF(M61²). In fields over Mersenne primes, the weighted
transform's roots of 2 are powers of 2, so its weights are bit rotations. The pair holds about
35–39 bits per word, so the largest known exponent fits 4M words. PRPLL added this transform in 2026
and runs it about 5× faster than FP64 on FP64-poor GPUs. Ordinary 32-bit NTT primes can't carry the
weights past 2^16 words, and 2^64 − 2^32 + 1 has no lengths with a factor of 3. It is built exact on
the CPU against the same contract and answers, with hybrids (FP64+M31, FP32+M31) designed in.

*Horizon: near-term · Depends on: the engine contract; the known answers · Refs: —*
**Seed:** [`issues/ntt-transform-engine.md`](issues/ntt-transform-engine.md)

## NVIDIA GPUs: CUDA engines, FP64 where it's fast and integer where it isn't

Three engines: FP64, FP64+M31 and the integer transform, chosen per GPU by its FP64:INT32 ratio.
Fused kernels keep the residue on the device between checkpoints, with one run per GPU in a node. The
bar is PRPLL-NTT, measured side by side. At the largest exponent it runs 156 µs per iteration on an
RTX PRO 6000 Blackwell, 182 µs on an H100 and 331.5 µs on an A100.

*Horizon: near-term · Depends on: the integer engine; the burn-in profiles · Refs: —*
**Seed:** [`issues/cuda-backend.md`](issues/cuda-backend.md)

---

# Part V — Fast on CPUs, the Mac first

From correct to competitive. At the lengths that matter, a squaring is limited by memory traffic, so
the CPU engine needs a cache-aware structure, fused passes, threads, and per-ISA kernels. The Mac
comes first, then the x86 and Arm Linux CPUs.

## Fast on CPUs: cache-blocked, fused, multithreaded transforms

A four- or six-step decomposition sized to the caches, with weights, squaring and carry folded into
the passes, run across every performance core. It is measured against the best established Arm
program on the M4 Max, then the best x86 program on an EPYC node.

*Horizon: mid-term · Depends on: `mps bench`; the portable engine as the correctness reference ·
Refs: —*
**Seed:** [`issues/cpu-transform-throughput.md`](issues/cpu-transform-throughput.md)

## Targets that check their node

A target names the node class it was written for, but the cluster test job checks only the
partition. An AVX-512 target that lands on an AVX2-only node therefore passes under its name. Before
the SIMD kernels ship, a target names the machine and the CPU features its node must have. The job
refuses a node without them, and the result names the CPU that ran it, so each tier's kernels are
known to have run on that tier.

*Horizon: mid-term, before the SIMD kernels · Depends on: test any commit on a cluster · Refs:
cluster-test-jobs review cycle 1 (F3), 2026-10-12*
**Seed:** [`issues/target-node-check.md`](issues/target-node-check.md)

## SIMD kernels for each CPU family, chosen at run time

NEON written once for every Arm target, plus x86-64-v3 (AVX2) and x86-64-v4 (AVX-512) tiers. One
binary picks its tier at start-up and runs on every node of a mixed fleet, and no untested kernel ever
runs. The M4's SME2 unit was measured and parked: its FP64 outer products don't pay for an FFT.

*Horizon: mid-term · Depends on: the throughput pass structure · Refs: —*
**Seed:** [`issues/cpu-isa-kernels.md`](issues/cpu-isa-kernels.md)

---

# Part VI — The Mac's GPU

## The Mac's GPU: an integer engine in Metal

Metal kernels compiled at run time from a C++ host on metal-cpp. On 2026-10-11 this was shown to work
with the Command Line Tools alone, bit-exact over 2^20 modular squarings. The GPU's high-word multiply
is slow, so a microbenchmark spike chooses between the Mersenne-prime-field transform and an FP32
hybrid. The bar is PrMers on the same Mac, and the roofline.

*Horizon: mid-term · Depends on: the integer engine · Refs: —*
**Seed:** [`issues/metal-backend.md`](issues/metal-backend.md)

---

# Part VII — AMD GPUs

## AMD GPUs: the GPU engines through HIP

The CUDA kernel sources compiled by ROCm through a thin portability header, with no assumption of warp
width, measured against PRPLL on an Instinct GPU.

*Horizon: mid-term · Depends on: the CUDA backend; access to an AMD Instinct GPU · Refs: —*
**Seed:** [`issues/hip-backend.md`](issues/hip-backend.md)

---

# Part VIII — The trio, complete

Trial factoring and P-1, on every engine they suit, and the proofs that finish PRP.

## Trial factoring to a chosen bit depth

The cheapest test. The 2024 work-in-progress has the right idea (q = 2kp + 1, q ≡ ±1 mod 8), but its
arithmetic overflows above 2^32 and it stops at a ten-million-entry sieve. Class sieving modulo 4620
(960 surviving classes) and exact multi-word modular arithmetic take it to the 2^70–2^80 depths that
matter, with every hit re-checked.

*Horizon: mid-term · Depends on: the result type; the reference big integer · Refs: —*
**Seed:** [`issues/trial-factoring.md`](issues/trial-factoring.md)

## Trial factoring on GPUs

Integer work that every GPU in view does far faster than a CPU, the Apple GPU included. It is also a
second burn-in stress profile.

*Horizon: mid-term · Depends on: CPU trial factoring; the GPU backends · Refs: —*
**Seed:** [`issues/gpu-trial-factoring.md`](issues/gpu-trial-factoring.md)

## P-1 factoring: stage 1 and stage 2

The middle test of the trio. Every factor has the form 2kp + 1, so P-1 needs only k to be smooth, and
it finds factors far beyond trial-factoring depth at a few percent of a primality test's cost. Stage 1
and a prime-pairing stage 2 run on the engine with checkpoints. Bounds are chosen by estimated success
probability. The GCD is quadratic until the large-exponent GCD below lands.

*Horizon: mid-term · Depends on: the transform engine's general multiply · Refs: —*
**Seed:** [`issues/pminus1-factoring.md`](issues/pminus1-factoring.md)

## A GCD and Jacobi symbol that scale to the largest exponents

P-1's GCD and the Lucas–Lehmer Jacobi check are quadratic with textbook algorithms, which means tens
of minutes per call at the top of the exponent range. A half-GCD and a subquadratic Jacobi symbol,
built on `mps`'s own multiplication, bring them down to seconds or minutes. The established programs
use GMP for this, which § Invariants rules out here.

*Horizon: mid-term · Depends on: the transform engine's general multiply · Refs: —*
**Seed:** [`issues/large-gcd-and-jacobi.md`](issues/large-gcd-and-jacobi.md)

## PRP proofs: a result anyone can verify cheaply

Pietrzak proofs saved during a PRP test let anyone verify the result with a small fraction of the
work. This is the last piece of a modern PRP implementation, and it lets a burn-in on one node be
checked cheaply on another.

*Horizon: long-term · Depends on: PRP · Refs: —*
**Seed:** [`issues/prp-proofs.md`](issues/prp-proofs.md)

---

# Part IX — Done

## Done: every known Mersenne prime, on every platform class, with the numbers

The campaign that meets the definition of done above, with its results table, and the `1.0.0`
release.

*Horizon: long-term · Depends on: Parts II–VIII on each platform class · Refs: —*
**Seed:** [`issues/known-primes-campaign.md`](issues/known-primes-campaign.md)

---

# Beyond 1.0 — exploration

## TPU v7: can a matrix machine square modulo 2^p − 1?

An exploration after the other targets are done. It is a bounded spike from a C++ host (PJRT C API)
on a TPU v7 VM. Ironwood has no FP64, has a 32-bit vector unit and publishes no integer matrix rate,
and the one earlier TPU attempt stopped at p = 4,423. The spike measures the vector unit's integer
throughput, tries the integer transform there, and ends with a measured go or no-go.

*Horizon: after 1.0 · Depends on: the integer engine; TPU access · Refs: —*
**Seed:** [`issues/tpu-feasibility-spike.md`](issues/tpu-feasibility-spike.md)

---

# Settled questions

None yet.
