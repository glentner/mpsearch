# Research digest — math-notes-and-references

The lean path: `appetite: small`, `kind: docs`, and trigger 2 doesn't fire (no high-blast-radius
path changes). Trigger 3 fires, because R7 makes computed checks the cycle's gates; its adversarial
review is in PLAN § 4.1. One brief, written by the planner from plan-time probes:
[01 — the citations and the checks](01-citations-and-checks.md). The decisions it backs:

1. **References are checked by machine, against doi.org.** Content negotiation gives CSL JSON for
   every DOI in the starting set, so the gate compares title, authors, year and venue itself. It
   tolerates what the metadata does (online years, missing fields, odd spellings) and makes a builder
   name a second source by URL wherever a field is missing. Two DOIs written from recall at plan time
   resolved to unrelated papers; the gate exists for that.
2. **The bibliography includes RFC 3629 and RFC 8259**, cited by `src/result.cpp`. R6 covers them as
   worded; the GOAL's Problem undercounts the code's citations, and no R-ID bends.
3. **The bounds.** Lucas–Lehmer on every prime $p \le 2500$; the Jacobi sweep on every odd prime
   $p \le 521$, with shifts, plus every odd $p \le 25$ to show the zero case; base-3 PRP on the 23
   published primes to 11213; the factor form on every factor below $2^{20}$ for $p \le 100$; P-1
   on $M_{67}$. Each runs in seconds in pure Python.
4. **The Jacobi derivation is settled** (brief § 3): $J(s_i - 2 \mid M_p)$ is $-1$ or $0$, never
   $+1$, for every odd $p \ge 3$, with $0$ exactly when an earlier $s_k$ shares a factor with $M_p$.
   That is R4's corrected statement, and the sweep checks it both ways.
5. **The gates are proven on prototypes.** They are green on good trees and red on about 100 injected
   defects, the adversarial review's attacks included (PLAN § 4.1).
