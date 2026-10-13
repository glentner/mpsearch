# REVIEW — The mathematics, written down: notes and references

> Adversarial QA by `code-review`, run in an isolated/clean context (a subagent; where the runtime
> has none, `code-review`'s sequential fallback). The correctness pass grades the branch diff against
> [`GOAL.md`](GOAL.md) + the repo's `AGENTS.md` § Invariants **only** — it does not see
> `PLAN.md`/`TECH.md` (avoids grading-its-own-homework / plan-sycophancy). Every finding cites an
> **executed** command, not an assertion.

- **Reviewed commit:** 7c0b806  ·  **Base:** main (`origin/main` 61f5ce7)  ·  **Date:** 2026-10-12
- **Verdict:** changes-requested
- **Cycle:** 1 of ≤3 — mirrors `review.cycle` in `TECH.md` (escalate to human on non-convergence)

**Mode: one fresh reviewer at xhigh.** The reviewer was blind to everything under `spec/` but GOAL.md. As the GOAL directs, it graded the notes' mathematics and citations against the cited works themselves, fetched over the network. The diff touches no code and no coupled core.

## Verification run

- **The mathematics, re-derived:**
  - the Lucas–Lehmer proof in both directions, with Euler's criterion and the second supplement via $\zeta_8$;
  - the Jacobi invariant: the product $s_i - 2 = 12\prod s_k^2$, $J(12 \mid M) = -1$ from $M \equiv 7 \pmod{24}$, the zero case and the shifted symbol;
  - P-1's stages 1 and 2;
  - IBDWT Theorem 1 and its output bound;
  - NTT Theorems 1–3, Lemma 1 and the CRT corollary.

  All correct, except F1 and F3.
- **Recomputed independently:**
  - Lucas–Lehmer to 2500: 17 primes among 367 exponents, and every composite also fails base 3;
  - base-3 PRP on the 22 exponents, and $3^{2046} \equiv 1013 \pmod{2047}$;
  - the factor forms, $M_{67}$ and $M_{29}$, and the 960 of 4620 classes;
  - P-1's gcds, (1, 193707721) and (1, 1);
  - the Gerbicz and DWT examples, and an exact IBDWT in GF($M_{61}^2$);
  - the NTT orders and bounds, and C&F (8.7) for $k \le 127$;
  - the Jacobi sweep over 97 odd primes $p \le 521$, with and without shifts, which found no value other than −1.
- **Citations:**
  - All 26 DOIs resolve to the stated works, and Crossref and DataCite metadata match.
  - These were read in full or in the relevant part, and say what the notes attribute to them:
    - Percival 2003;
    - Crandall–Fagin 1994, by OCR of the authors' scan;
    - Crandall–Mayer–Papadopoulos 2003;
    - Harvey 2014;
    - Li–Gallot 2022;
    - Pietrzak 2019;
    - Brent–Zimmermann 2010 (both works);
    - Cole 1903;
    - Lehmer 1930;
    - OEIS A000043;
    - the GIMPS math page, except F2.
  - Seven more were checked against their abstracts.
  - Not verifiable online: Montgomery–Kruppa 2008, whose abstract was blocked, and the four works cited without a locator under the paywall rule.
- **Clean room:** no program names or source found, and Harvey's pseudocode is not reproduced.
- **"What mps uses"** matches `src/math.cpp`, `src/main.cpp`, `src/cli.hpp`, `src/mp.hpp` and `src/result.hpp`.
- **Orchestrator:**
  - P1–P5's gates were re-run green, and `verify/` is unchanged since the plan commit 45ba45e.
  - No code changed, so there were no cluster runs (PLAN § 6).
  - The ledger and records (`evidence/`) were read: every deferral has its home, and `issues/*.md` match the `**Seed:**` lines one to one.

## Requirement → evidence matrix

| R-ID | Verified how | Status |
|------|--------------|--------|
| R1 | 9 files; the index and the clean-room sentence; GitHub math syntax only | ✅ |
| R2 | five sections in every note (grep); proofs re-derived | ❌ F1, F3 |
| R3 | the Mersenne note against `src/math.cpp`'s exponent check | ✅ |
| R4 | the Jacobi derivation re-derived and swept | ✅ |
| R5 | 26 DOIs resolve; metadata matches | ✅ |
| R6 | 28 entries, all cited; every key resolves; the `src/` citations included | ✅ |
| R7 | the recomputations above | ❌ F4, and F2 as a misstatement of a source |
| R8 | no other program's source | ✅ |
| R9 | the § Map and § Conventions lines | ✅ |

## Findings

Severity: **CRITICAL** (a violation of any numbered section of § Invariants is auto-CRITICAL,
**including lettered subsections**; a § Conventions violation is **HIGH**) · **HIGH** · **MEDIUM** ·
**LOW**. Verdict: **CONFIRMED** (reproduced) vs **PLAUSIBLE** (suspected, needs human triage). Only
CONFIRMED findings auto-loop to `code-build`.

### [MEDIUM/CONFIRMED] F1 — the Gerbicz check misses a residue that stops being invertible, once checks are spaced
- **Where:** `doc/prp-and-gerbicz.md:27`, `:58`, and the lists at `:28–32` and `:64`.
- **The note's claim:** "A value that is not invertible fails the first check after it", and "the first check at or after block j fails".
- **What actually holds:** only the comparison of $D_j$ with $D_{j-1}$ catches it. A later check compares $D_{t+1}$ with $x_0 D_t^{2^L}$. For $t \ge j$ both sides are non-invertible, and modulo a prime both are 0, so the check passes.
- **Why it matters:** the note's own plan runs the check at intervals. The list of what the check cannot detect, and the list of extra checks, both omit this blind spot.
- **Evidence:** the reviewer took $M_{127}$ with $L = 10$ and 12 blocks, checked at $t = 3, 7, 11$, and zeroed the residue at squaring 45. Every check passed, and the final residue was not 9, so a prime $M_{127}$ would be reported composite. Checked at every block, only $t = 4$ fails.
- **Touches:** R2, and the planned PRP test's design (§ Invariants 1: verdicts are earned).

### [LOW/CONFIRMED] F2 — the note says its sources give no error probability, but one does
- **Where:** `doc/prp-and-gerbicz.md:34`.
- **Failure:** "The sources cited here give no probability for an undetected error." GIMPSMath, cited in the next sentence, says results are "99.999+% reliable", gives "the previous 1 or 2% chance of missing a new Mersenne prime", and gives a 1.5% error rate for LL tests.
- **Evidence:** https://www.mersenne.org/various/math.php, fetched 2026-10-12.

### [LOW/CONFIRMED] F3 — the Pietrzak sketch handles only an even $T$, then applies it to $T = p$, which is odd
- **Where:** `doc/prp-and-gerbicz.md:38`.
- **Failure:** "until $T = 1$" terminates only when $T$ is a power of 2. Pietrzak2019 § 3.1, step 4, passes on $(N, x', (T+1)/2, y'^2)$ for an odd $T/2$, and its Lemma 2 proof covers odd $T$.

### [LOW/CONFIRMED] F4 — the NTT note repeats an unchecked search result
- **Where:** `doc/number-theoretic-transforms.md:30`.
- **Failure:** the claim that no prime below $2^{32}$ gives a power-of-two weight length longer than $2^{16}$ is the seed's. The note says it doesn't check it, and a seed is not a published source (R7).
- **Evidence:** the reviewer reran the search over every prime $k \cdot 2^{17} + 1 < 2^{32}$. None has $2^{(q-1)/2^{17}} = 1$, and the largest length is $2^{16}$, at 3539599361. The claim is true but unchecked.

**Dropped after checking:**
- Lehmer 1935 described as a "shorter proof": its content can't be read online.
- A hedged cost estimate, off by about a factor of 2.
- Pietrzak's soundness conditions: correct.

## Human-gate triggers

- **Not triggered.** The findings are in `doc/`, which is off the high-blast-radius list, and no `[human gate]` invariant is violated.
- F1 bears on § 1 for the future PRP test. Its routing therefore carries the blind spot into `issues/prp-gerbicz-test.md`, where the test's own cycle, which is human-gated, will design the guard.

## Remediation (orchestrator's routing)

- **F1, F2, F3 → P2 reopened** (`doc/prp-and-gerbicz.md`). These are classes, not instances:
  - **F1:** state exactly what the Gerbicz check catches for each placement of checks, including the non-invertible residue, which only the next check catches. Add it to the blind spots. Name the guard a test needs: a residue that is zero or not invertible is an error at every check, or checks run every block. Tie the demonstration to a check span where the span grammar allows.
  - **F2:** every statement of what a source says or omits must agree with the source. Give GIMPSMath's figures, with their source.
  - **F3:** the Pietrzak sketch covers odd $T$ as § 3.1 does, and states every hypothesis of the recursion.
- **F4 → P4 reopened** (`doc/number-theoretic-transforms.md`): check the search result in a check span if the grammar can express it; otherwise state it as the seed's unchecked claim with no number the note asserts, or drop it.
- **P5 reopened:** rerun the audit over the final tree, and add F1's guard to `issues/prp-gerbicz-test.md`'s notes in the ledger.
- **Gates:** the frozen gates still hold structure, citations and spans. The scoped review cycle 2 grades the truth of the fixes.

## Optional completeness sub-pass (separate reviewer; may see TECH.md)

- Not run.

## Review cycle 2 — changes-requested (2026-10-12)

- **Reviewed commit:** a35da3c. The remediation is `37404dd` (F1–F3), `6f704d4` (F4) and `a35da3c` (the audit and the PRP seed's guard). P2's gate was retuned to `p4.sh` in `f519d23`, because `p2.sh` checks P2's exact file set.
- **Mode:** scoped to F1–F4 and nearby regressions. One fresh reviewer ran at xhigh, blind to everything under `spec/` but GOAL.md, and graded against the cited works and its own computation.

### Verification

- **F1 — verified.**
  - Theorem 5's parts 1–4 and the every-block proof were re-derived.
  - Exhaustive fault injection: every value at every squaring on $M_7$, $M_{11}$ ($L = 1, 2, 3$) and $M_{13}$, with up to 200 check placements each.
  - Sampled runs on $M_{13}$, $M_{23}$, $M_{29}$ and $M_{37}$, plus 4,000 random multi-fault runs each on $M_{127}$, $M_{29}$ and $M_{23}$.
  - None violated the theorem. Every undetected single fault was one of the two stated kinds.
  - The $M_7$ example reproduces, and the guard is stated correctly for prime and composite $N$.
- **F2 — verified** against the GIMPS math page ("Last updated: Oct 16, 2024"): all five figures, their attribution and their locators.
- **F3 — verified** against Pietrzak 2019 § 3.1, Theorem 1 and footnote 4.
  - The steps, the hypotheses and the identity, including the $T/2 + 1$ case, hold, and termination holds for every even $T < 5000$.
  - The note's reading of the printed $(T+1)/2$ is literally right, and its repair is valid. The paper's other statements give $\lceil T/2 \rceil$, which the note could also mention; not filed.
- **F4 — verified.** The new sentence follows from Lemma 1, the note asserts no unchecked number, and Lemma 1 holds by brute force for primes below 3000.
- **Nearby regressions:** the new citations resolve, the renumbered theorem left no stale reference, and every new number is checked or cited.

### Findings

- **[LOW/CONFIRMED] G1 — the PRP seed overstates the blind spot** (`issues/prp-gerbicz-test.md:79–81`).
  - "Not invertible modulo M_p … every later check passes" holds for a zero residue, or for one still correct modulo the cofactor, but not in general.
  - On $M_{11}$, $M_{23}$ and $M_{29}$, a residue zeroed modulo one factor and wrong modulo the cofactor failed every later check. Every later check passed when the residue was still correct modulo the cofactor.
  - The note itself (`doc/prp-and-gerbicz.md:40,46`) states it correctly, and the seed's remedy stands.
  - Seeds are candidates, not contracts (ISSUE template), but the seed's statement should be true.
- **[LOW/PLAUSIBLE → triaged FIX] G2 — the $\pm y$ caveat stops at squaring** (`doc/prp-and-gerbicz.md:70,74`).
  - In the residues prime to $M_p$, $-1$ has order 2. So Pietrzak's low-order assumption is not merely unproved, as line 74 says: it is false there, and the sign also breaks Step 4's halving.
  - A prover who sends $-\mu$ carries the false claim $y = -3^{2^p}$ through each halving when $r$ is odd. In the note's own variant the verifier accepted the false claim 377/400 times for $p = 31$, 380/400 for $p = 61$ and 396/400 for $p = 127$, matching $1 - 2^{-\text{halvings}}$. With an honest $\mu$ it accepted 0/100.
  - No sentence in the note is false, and the note never claims soundness for $M_p$.
  - **The orchestrator's triage, under the author's delegation: fix.** The note is where the PRP-proof cycle will start. It must say the assumption fails in this group, show what that lets a prover do, and say the remedy is that cycle's to design. `issues/prp-proofs.md` gets the same point.

## Remediation (review cycle 2's routing)

- **G1 → P5 reopened:** correct the PRP seed's bullet so it states the blind spot exactly as the note's Theorem 5 does, a zero residue or one correct modulo the cofactor, keeping the remedy.
- **G2 → P2 reopened, gate `p4.sh`:**
  - Line 74 says the low-order assumption is false in the residues prime to $M_p$, because $-1$ has order 2.
  - The caveat states what that lets a prover do in Step 4's halving, and that the remedy is the PRP-proof cycle's: for example, a group where squaring is one-to-one, such as Pietrzak's $QR^+_N$.
  - Every number is tied to a check or a citation.
  - A matching note goes to `issues/prp-proofs.md` and the ledger at P5.
- **The next review, cycle 3, is the last the loop allows.**

## Review cycle 3 — approved (2026-10-12)

- **Reviewed commit:** 8711034. The remediation is `3fb59ca` (G2) and `8711034` (G1, and the PRP-proof seed's note).
- **Mode:** scoped to G1 and G2 and anything their fixes touched. One fresh reviewer ran at xhigh, blind to everything under `spec/` but GOAL.md, and graded against Pietrzak 2019 and its own computation.
- **Verdict:** approved. G1 and G2 are verified, and the one remaining point (H1) lies in a seed, which code-review doesn't grade. The orchestrator corrected it in this commit.

### Verification

- **G2 — verified.**
  - The sign-flip identity $y' = (-1)^{r+1} x'^{2^{T/2}}$ was re-derived, and held as an assertion at every halving.
  - The note's exact protocol was simulated: the odd start, halving with $-\mu$, the exact rewrite at an odd $t > 1$, the stop at $T = 1$, and membership by gcd. It ran on 17 exponents from 3 to 127, prime and composite $M_p$.
    - Exhaustively, with a 1-bit challenge, acceptance is exactly $1 - 2^{-h}$ in every case.
    - With random 64-bit challenges it matches: $p = 127$ gives 0.984 against $63/64$.
    - An honest prover is always accepted.
  - $h = \lfloor \log_2(p-1) \rfloor$ for every $p$ tested, and the new check span evaluates True.
  - "The assumption is false modulo $M_p$" holds.
  - The locators were checked against the Dagstuhl PDF: § 2.1 for $QR^+_N$, § 2.3 for "no sub-group of small order", and § 3 for the protocol. The new Li–Gallot § 4 locator was outside the reviewer's inputs, so it was not re-checked.
- **G1 — verified** by fault injection: $L = 3$, 8 blocks, on $M_7$ and $M_{13}$ (prime) and $M_{11}$, $M_{23}$, $M_{29}$ and $M_{37}$ (composite), at every fault block and every coprime split.

  | Fault | Cases | Result |
  |---|---|---|
  | zero residue | 48 | the block's own check fails, every later check passes |
  | zero mod a, correct mod b | 96 | the block's own check fails, every later check passes |
  | zero mod a, wrong mod b | 1,900 | the own check and every later check fail |
  | invertible wrong residue | 952 | the own check and every later check fail |

  The seed agrees with Theorem 5.

### Finding

- **[LOW/PLAUSIBLE → corrected by the orchestrator] H1 — the PRP-proof seed's figure is the note's recursion, not Pietrzak's literal Step 4** (`issues/prp-proofs.md`).
  - The seed attributed $1 - 2^{-h}$ to "Pietrzak's halving computed modulo $M_p$".
  - Under § 3.1, Step 4, read literally (an odd half becomes $(x', (T+1)/2, y'^2)$), the squaring removes the sign at the first odd half. The $-\mu$ prover is then accepted with probability 1, for every $p$ in $\{7, 11, 13, 19, 23, 29, 31, 37, 41, 43, 61, 89, 107, 127\}$.
  - The seed understated the attack, so its conclusion and remedy stand. The note itself scopes the claim correctly.
  - **Triage, under the author's delegation:** seeds are candidates that code-review doesn't grade (ISSUE template). The orchestrator corrected the seed's sentence in this commit to give both readings. No further review cycle is needed.

### Human-gate triggers

- **Not triggered** in any cycle. The findings were in `doc/` and `issues/`, which are off the high-blast-radius list.
