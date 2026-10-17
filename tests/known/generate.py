# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# Known-answer tables
#
# Writes one table of known answers to stdout. Run from the repository root, with Python 3.8 or
# later and the standard library only:
#
#     python3 tests/known/generate.py <stem> > tests/known/<stem>.txt
#
# Each table is written whole, in the format src/known.cpp reads: the magic line, this command,
# notes, then source lines, each the source of the rows below it, and a trailer counting the rows.
# Published answers are embedded here with their sources and the dates they were retrieved, and
# nothing is fetched. Every answer Python's integers reach in minutes is recomputed here, by a
# method independent of every mps engine, and the command exits 1, naming the row, on a mismatch.
# bin/check reads the tables through src/known.* and never runs this script.

import sys


# --- Arithmetic modulo M_p = 2^p - 1, with Python's integers ------------------------------------
#
# Every recomputation goes through the three functions below.

def fold(x, p, m):
	# x mod M_p for x >= 0: 2^p = 1 modulo M_p, so the bits above the low p fold onto them
	while x >> p:
		x = (x & m) + (x >> p)
	return 0 if x == m else x


def lucas_lehmer(p, start, k):
	# s_k mod M_p, from s_0 = start and s_(i+1) = s_i^2 - 2, kept non-negative by adding M_p
	m = (1 << p) - 1
	s = start % m
	for _ in range(k):
		s = fold(s * s + m - 2, p, m)
	return s


def squarings(p, start, k):
	# start^(2^k) mod M_p, the x_k of doc/prp-and-gerbicz.md, Theorem 2
	m = (1 << p) - 1
	x = start % m
	for _ in range(k):
		x = fold(x * x, p, m)
	return x


def prp_type1(p):
	# 3^(M_p - 1) mod M_p, the type-1 residue: M_p - 1 is p - 1 ones then a zero in binary, so
	# square and multiply by 3 p - 2 times after the leading one, then square once
	m = (1 << p) - 1
	x = 3
	for _ in range(p - 2):
		x = fold(3 * x * x, p, m)
	return fold(x * x, p, m)


def is_prime(n):
	# trial division
	if n < 2:
		return False
	d = 2
	while d * d <= n:
		if n % d == 0:
			return False
		d += 1
	return True


def res64(x):
	# the low 64 bits of a residue, as 16 uppercase hexadecimal digits
	return "%016X" % (x & 0xFFFFFFFFFFFFFFFF)


def fail(message):
	sys.stderr.write("generate.py: %s\n" % message)
	sys.exit(1)


class Table:

	def __init__(self, stem, notes):
		self.lines = ["# mps known 1", "# command: python3 tests/known/generate.py %s > tests/known/%s.txt" % (stem, stem)]
		self.lines += ["# " + note for note in notes]
		self.rows = 0

	def source(self, text):
		self.lines.append("# from " + text)

	def row(self, *fields):
		self.lines.append(" ".join(fields))
		self.rows += 1

	def text(self):
		return "".join(line + "\n" for line in self.lines + ["# end %d rows" % self.rows]).encode("ascii")


# --- mersenne-exponents: the Mersenne prime exponents -------------------------------------------

# Up to this bound the command recomputes the exponents; above it, it writes the list's
SWEEP = 10000

# GIMPS, "List of Known Mersenne Prime Numbers", https://www.mersenne.org/primes/ (doc/references.md,
# GIMPSPrimes): every exponent it listed on the date it was retrieved, ascending
PRIMES_RETRIEVED = "2026-10-17"
GIMPS_PRIMES = [
	2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281, 3217, 4253, 4423, 9689, 9941,
	11213, 19937, 21701, 23209, 44497, 86243, 110503, 132049, 216091, 756839, 859433, 1257787, 1398269,
	2976221, 3021377, 6972593, 13466917, 20996011, 24036583, 25964951, 30402457, 32582657, 37156667,
	42643801, 43112609, 57885161, 74207281, 77232917, 82589933, 136279841,
]


def mersenne_exponents():
	# Up to SWEEP, the exponents p with M_p prime: M_2 = 3 by trial division, and every odd prime p
	# by the Lucas-Lehmer test (Lehmer 1930), s_(p-2) = 0 from s_0 = 4. They must be exactly the
	# list's up to SWEEP. Above it the list is written as it stands, each p checked prime and the
	# order ascending, and nothing more is claimed.
	found = [2] if is_prime((1 << 2) - 1) else []
	found += [p for p in range(3, SWEEP + 1, 2) if is_prime(p) and lucas_lehmer(p, 4, p - 2) == 0]
	listed = [p for p in GIMPS_PRIMES if p <= SWEEP]
	for p in sorted(set(found) - set(listed)):
		fail("mersenne %d: the sweep finds M_%d prime, and the list does not give %d" % (p, p, p))
	for p in sorted(set(listed) - set(found)):
		fail("mersenne %d: the list gives %d, and the sweep does not find M_%d prime" % (p, p, p))
	for a, b in zip(GIMPS_PRIMES, GIMPS_PRIMES[1:]):
		if not (is_prime(b) and a < b):
			fail("mersenne %d: the list's exponent is not a prime above %d" % (b, a))
	t = Table("mersenne-exponents", [
		"mersenne <p>: M_p = 2^p - 1 is prime. Every exponent the GIMPS list gives, ascending.",
		"Up to %d this command recomputes them; above it, it writes the list's and recomputes none." % SWEEP,
	])
	t.source("computed: M_2 = 3 prime by trial division, and a Lucas-Lehmer sweep in Python over every odd prime "
	         "up to %d, in this command, find exactly these exponents; the command exits 1 otherwise" % SWEEP)
	for p in listed:
		t.row("mersenne", str(p))
	t.source("published GIMPSPrimes retrieved %s: the exponents above %d the list gives" % (PRIMES_RETRIEVED, SWEEP))
	for p in GIMPS_PRIMES:
		if p > SWEEP:
			t.row("mersenne", str(p))
	return t


# --- final-residues: the final residues of composite exponents ----------------------------------

# Below this bound the command recomputes each row; above it, a row rests on the reports' agreement.
# src/known.cpp holds the same bound, and a change to one is a change to both.
RECOMPUTED_BELOW = 200000

# GIMPS, the reports of Lucas-Lehmer and PRP results, https://www.mersenne.org/report_LL/ and
# https://www.mersenne.org/report_PRP/ (doc/references.md, GIMPSReportLL and GIMPSReportPRP), in
# their plaintext form as retrieved: one accepted result or more per decade of exponent, from 10^3
# to 10^8, wherever the reports hold one. Accepted: every 64-bit residue the report gives for the
# exponent agrees, two or more of them come from different shifts or programs, no result for it
# carries an error code or is listed as bad, and for PRP every result is type 1, base 3. Each entry
# is (p, Res64, how many 64-bit residues agreed).
REPORTS_RETRIEVED = "2026-10-17"
REPORT_LL = [
	(1237, "DF5E6622F83F5BE2", 2),
	(1277, "5613A480590E78BA", 3),
	(10303, "3DD943A77EC9238B", 3),
	(100069, "1ECC98522D52594C", 5),
	(1000193, "8F5354551BC18373", 5),
	(10000139, "76BB6A7CE0BCA62B", 6),
	(100000007, "F9042256B193FAA0", 12),
]
REPORT_PRP = [
	(28603, "F899E5F1E2B40323", 2),
	(1000193, "CA0B0B10C609A027", 4),
	(10000019, "9DD181D73794E731", 4),
	(100005533, "ADF278E588B2CDCE", 2),
]


def final_residue(op, p):
	# ll: s_(p-2) from s_0 = 4 (Lehmer 1930); prp1: the type-1 residue 3^(M_p - 1)
	return res64(lucas_lehmer(p, 4, p - 2) if op == "ll" else prp_type1(p))


def final_residues():
	# The reports' rows, each below RECOMPUTED_BELOW recomputed first; a mismatch exits 1 naming it
	t = Table("final-residues", [
		"ll <p> <Res64>: the low 64 bits of s_(p-2) mod M_p, s_0 = 4. prp1 <p> <Res64>: of 3^(M_p - 1) mod M_p, "
		"the type-1 residue. Every p is a prime with M_p composite.",
		"agreed=<n>: how many 64-bit residues the report gave that agree. This command recomputes every row "
		"with p below %d; the others rest on the reports' agreement." % RECOMPUTED_BELOW,
	])
	reports = [
		("ll", "GIMPSReportLL", "Lucas-Lehmer results,", REPORT_LL),
		("prp1", "GIMPSReportPRP", "PRP results of type 1, base 3,", REPORT_PRP),
	]
	for op, key, kind, rows in reports:
		t.source("published %s retrieved %s: accepted %s two or more agreeing from different shifts or programs, "
		         "none with an error code or listed as bad" % (key, REPORTS_RETRIEVED, kind))
		for p, value, agreed in rows:
			if p < RECOMPUTED_BELOW:
				computed = final_residue(op, p)
				if computed != value:
					fail("%s %d: the report gives %s, and Python's integers %s" % (op, p, value, computed))
			t.row(op, str(p), value, "agreed=%d" % agreed)
	return t


# --- partial-residues: the residues after a stated number of iterations --------------------------

# E. W. Mayer, the Mlucas README, https://www.mersenneforum.org/mayer/README.html (doc/references.md,
# MlucasREADME), its basic build self-test as retrieved: the Res64 after 100 iterations of M3888509.
# The README does not name the test; recomputed, it is s_100 from s_0 = 4. (op, p, start, k, Res64)
README_RETRIEVED = "2026-10-17"
README_SELF_TEST = ("ll", 3888509, 4, 100, "71E61322CCFB396C")

# LL and PRP rows at one exponent or more in each decade from 10^3 to 10^7, each exponent but
# 3888509 one with a published final residue in final-residues.txt, so an engine can run the short
# and the full test on one p, and 3888509 the README's. Each k is at least ceil(log2 p) + 8, so the
# residue has been reduced modulo M_p eight times or more. (op, p, start, k, Res64)
COMPUTED_PARTIAL = [
	("ll", 1277, 4, 100, "EE3E8EF89BAFBDCD"),
	("prp", 1277, 3, 100, "C6D7DFE3FCAEDC31"),
	("ll", 1277, 4, 1000, "3D43F5D86DECC8F5"),
	("prp", 1277, 3, 1000, "36F834F2D5842CF5"),
	("ll", 10303, 4, 100, "443391E534C210F2"),
	("prp", 10303, 3, 100, "088D6D3D863CB0C7"),
	("ll", 10303, 4, 1000, "7841BFAB20749B83"),
	("prp", 10303, 3, 1000, "3C30472597BE7B6C"),
	("ll", 10303, 4, 10000, "FB3AC33F5A26CC71"),
	("prp", 10303, 3, 10000, "65CE30B46EF0AA1B"),
	("ll", 100069, 4, 100, "03DFC5B4407ED178"),
	("prp", 100069, 3, 100, "05DC064E3050BCD0"),
	("ll", 100069, 4, 1000, "7AA9194B9ED5BBBC"),
	("prp", 100069, 3, 1000, "D87224FAC22ABDEF"),
	("ll", 1000193, 4, 100, "F1234B20ACFD642F"),
	("prp", 1000193, 3, 100, "44DEBFC2AA1782BA"),
	("prp", 3888509, 3, 100, "79FA4A9B6D077D31"),
	("ll", 10000019, 4, 32, "3DC2608FC9C15BE6"),
	("prp", 10000019, 3, 32, "B6DF55F5DE80E01A"),
]


def partial_residue(op, p, start, k):
	# ll: s_k from s_0 = start, s_(i+1) = s_i^2 - 2; prp: start^(2^k)
	return res64(lucas_lehmer(p, start, k) if op == "ll" else squarings(p, start, k))


def partial_residues():
	# The README's row, then the computed ones, each recomputed first; a mismatch exits 1 naming it
	t = Table("partial-residues", [
		"ll <p> <start> <k> <Res64>: the low 64 bits of s_k mod M_p, s_0 = start, s_(i+1) = s_i^2 - 2. "
		"prp <p> <start> <k> <Res64>: of start^(2^k) mod M_p.",
		"Unshifted: op, p, start and k determine the value, so one row answers a run at any transform length and "
		"shift. The final Lucas-Lehmer residue, ll <p> 4 <p-2>, belongs in final-residues.txt and is refused here.",
		"This command recomputes every row under a published or computed source.",
		"A fingerprint is a row under a '# from runs: <text>' line: the key and Res64 as above, then "
		"run=<engine>:<length>:<shift> for each agreeing run, two or more of them from two or more engines. "
		"It supersedes no row, and one that disagrees with a row of its key is refused.",
	])
	groups = [
		("published MlucasREADME retrieved %s: the README's basic build self-test, the Res64 after 100 iterations "
		 "of M3888509, which this command recomputes as s_100 from s_0 = 4" % README_RETRIEVED, [README_SELF_TEST]),
		("computed: s_k and start^(2^k) modulo M_p by folding, with Python's integers, in this command",
		 COMPUTED_PARTIAL),
	]
	for source, rows in groups:
		t.source(source)
		for op, p, start, k, value in rows:
			computed = partial_residue(op, p, start, k)
			if computed != value:
				fail("%s %d %d %d: the table gives %s, and Python's integers %s" % (op, p, start, k, value, computed))
			t.row(op, str(p), str(start), str(k), value)
	return t


TABLES = {
	"mersenne-exponents": mersenne_exponents,
	"final-residues": final_residues,
	"partial-residues": partial_residues,
}


def main(argv):
	if len(argv) != 2 or argv[1] not in TABLES:
		sys.stderr.write("usage: python3 tests/known/generate.py {%s}\n" % ",".join(TABLES))
		return 2
	sys.stdout.buffer.write(TABLES[argv[1]]().text())
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
