# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# Vector generator
#
# Writes one family of the reference arithmetic's expected values to stdout, computed with
# Python's arbitrary-precision integers, the independent method. Run from the repository root,
# with Python 3.8 or later and the standard library only:
#
#     python3 tests/vectors/generate.py <stem> > tests/vectors/<stem>.txt
#
# Every operand is written in full (x:), as bit ranges (u:), or as a SplitMix64 seed (r:, c:;
# Steele, Lea and Flood, "Fast splittable pseudorandom number generators", 2014, Fig. 16), each
# seed a literal in the tables below, so a case appended to a table never changes an earlier
# record. Values above 1024 bits are written as their bit length, low limb and SHA-256 (FIPS 180-4)
# of their limb bytes. A file ends in a trailer holding its record count and the SHA-256 of every
# byte before it. bin/check reads the files and never runs this script.

import hashlib
import re
import sys

if hasattr(sys, "set_int_max_str_digits"):
	sys.set_int_max_str_digits(0)


MASK = (1 << 64) - 1
BETA = 1 << 64
FULL_BITS = 1024
BIG = 1 << 20

ORACLE = "Python's arbitrary-precision integers, the independent method: every expected value is CPython's int"
ENCODING = ("operands r: and c: per SplitMix64 of Steele, Lea and Flood 2014, Fig. 16; digests SHA-256 of "
            "FIPS 180-4 (hashlib)")


# --- Operands -----------------------------------------------------------------------------------

class SplitMix64:
	# Steele, Lea and Flood 2014, Fig. 16: the state advances by the golden gamma and is mixed

	def __init__(self, seed):
		self.state = seed & MASK

	def next(self):
		self.state = (self.state + 0x9E3779B97F4A7C15) & MASK
		z = self.state
		z = ((z ^ (z >> 33)) * 0xFF51AFD7ED558CCD) & MASK
		z = ((z ^ (z >> 33)) * 0xC4CEB9FE1A85EC53) & MASK
		return z ^ (z >> 33)


def topped(limbs, bits):
	# the low `bits` bits of the limbs, least significant first, with bit `bits - 1` set
	value = 0
	for i, limb in enumerate(limbs[:(bits + 63) // 64]):
		value |= limb << (64 * i)
	value &= (1 << bits) - 1
	return value | (1 << (bits - 1))


def random_operand(bits, seed):
	g = SplitMix64(seed)
	return topped([g.next() for _ in range((bits + 63) // 64)], bits)


def carry_operand(bits, seed):
	# runs of 1 + (v >> 32) mod 32 limbs: all zero (v mod 3 = 0), all ones (1), or the next outputs (2)
	g = SplitMix64(seed)
	n = (bits + 63) // 64
	limbs = []
	while len(limbs) < n:
		v = g.next()
		for _ in range(1 + (v >> 32) % 32):
			limbs.append(0 if v % 3 == 0 else MASK if v % 3 == 1 else g.next())
	return topped(limbs[:n], bits)


def operand(spec):
	kind, _, rest = spec.partition(":")
	if kind == "x":
		return int(rest, 16)
	if kind == "u":
		value = 0
		for part in rest.split(","):
			lo, hi = (int(t) for t in part.split("-"))
			assert lo < hi, spec
			value |= ((1 << (hi - lo)) - 1) << lo
		return value
	bits, seed = (int(t) for t in rest.split(":"))
	assert bits >= 1, spec
	return random_operand(bits, seed) if kind == "r" else carry_operand(bits, seed)


def x(v):
	return "x:%x" % v


def u(*ranges):
	return "u:" + ",".join("%d-%d" % r for r in ranges)


def r(bits, seed):
	return "r:%d:%d" % (bits, seed)


def c(bits, seed):
	return "c:%d:%d" % (bits, seed)


# The two large operands, 2^20 and 2^20 - 1 bits, and the only ones above 2^17 bits in any family
A = r(BIG, 101)
C = c(BIG - 1, 102)


# --- Values, strings and files ------------------------------------------------------------------

def limb_bytes(v):
	return v.to_bytes(8 * ((v.bit_length() + 63) // 64), "little")


def value(v):
	# in full up to 1024 bits; above, the bit length, the low limb and the digest of the limbs
	assert v >= 0
	if v.bit_length() <= FULL_BITS:
		return "x:%x" % v
	return "h:%d:%016x:%s" % (v.bit_length(), v & MASK, hashlib.sha256(limb_bytes(v)).hexdigest())


def string(v, text):
	# in full for a value up to 1024 bits; above, the length and the digest of the bytes
	if v.bit_length() <= FULL_BITS:
		return "s:" + text
	return "g:%d:%s" % (len(text), hashlib.sha256(text.encode("ascii")).hexdigest())


class Family:

	def __init__(self, stem, sources):
		self.stem = stem
		self.sources = list(sources)
		self.records = []

	def add(self, *fields):
		self.records.append(fields)

	def text(self):
		lines = ["# mps vectors 1",
		         "# command: python3 tests/vectors/generate.py %s > tests/vectors/%s.txt" % (self.stem, self.stem)]
		lines += ["# source: " + s for s in self.sources]
		lines += [" ".join(f) for f in self.records]
		body = "".join(line + "\n" for line in lines).encode("ascii")
		trailer = "# end %d records %s\n" % (len(self.records), hashlib.sha256(body).hexdigest())
		return body + trailer.encode("ascii")


# --- arith: cmp, add, sub, shl, shr -------------------------------------------------------------

# Pairs (A, B): operands of 64k - 1, 64k and 64k + 1 bits for k = 1, 2, 4, 8 and 48; unequal
# lengths; carry patterns; all ones, sums carrying and differences borrowing through four or more
# limbs; equal lengths differing only in the low or only in the top limb; zero; and A and C.
ARITH_PAIRS = [
	(r(63, 1), r(63, 2)), (r(64, 3), r(64, 4)), (r(65, 5), r(65, 6)),
	(r(127, 7), r(127, 8)), (r(128, 9), r(128, 10)), (r(129, 11), r(129, 12)),
	(r(255, 13), r(255, 14)), (r(256, 15), r(256, 16)), (r(257, 17), r(257, 18)),
	(r(511, 19), r(511, 20)), (r(512, 21), r(512, 22)), (r(513, 23), r(513, 24)),
	(r(3071, 25), r(3071, 26)), (r(3072, 27), r(3072, 28)), (r(3073, 29), r(3073, 30)),
	(r(3073, 31), r(65, 32)), (r(512, 33), r(3071, 34)), (r(1000, 35), r(64, 36)),
	(c(3072, 41), c(3072, 42)), (c(1024, 43), c(960, 44)), (c(6000, 45), c(6001, 46)),
	(u((0, 320)), x(1)), (u((0, 3072)), u((0, 3072))), (u((0, 3072)), u((0, 64))),
	(u((320, 321)), x(1)), (u((3072, 3073)), u((0, 64))), (u((256, 257)), u((0, 1), (128, 129))),
	(u((0, 256)), u((1, 256))), (u((0, 256)), u((0, 192), (193, 256))),
	(u((0, 3072)), u((0, 3008), (3009, 3072))), (u((1, 3072)), u((0, 3072))),
	(x(0), x(0)), (x(0), r(64, 47)), (r(300, 48), r(300, 48)),
	(A, C),
]

# Every shift count at 0 and at multiples of 64, around them, and far; and one past each operand
SHIFTS = (0, 1, 63, 64, 65, 128, 1000)


def unique(specs):
	seen = set()
	return [s for s in specs if not (s in seen or seen.add(s))]


def arith():
	f = Family("arith", [ORACLE, ENCODING])
	for sa, sb in ARITH_PAIRS:
		a, b = operand(sa), operand(sb)
		f.add("cmp", sa, sb, "lt" if a < b else "eq" if a == b else "gt")
		f.add("cmp", sa, sa, "eq")
		f.add("add", sa, sb, value(a + b))
		if a >= b:
			f.add("sub", sa, sb, value(a - b))
		else:
			f.add("sub", sb, sa, value(b - a))
	for s in unique([sa for sa, _ in ARITH_PAIRS] + [C]):
		a = operand(s)
		for k in SHIFTS + (a.bit_length() + 1,):
			f.add("shl", s, str(k), value(a << k))
			f.add("shr", s, str(k), value(a >> k))
	return f


# --- folding: fold, res64 -----------------------------------------------------------------------

FOLD_EXPONENTS = [1, 2, 3, 5, 7, 31, 61, 63, 64, 65, 89, 127, 128, 129, 521, 1279, 4423, 9941]


def fold_inputs(p, seed):
	# 0, M_p, 2^p, 2 M_p, 2^p M_p, (M_p - 1)^2, and a random X below (M_p - 1)^2, each once
	inputs = [x(0), u((0, p)), u((p, p + 1)), u((1, p + 1)), u((p, 2 * p))]
	if p == 2:
		inputs += [u((2, 3)), r(2, seed)]
	elif p >= 3:
		# (M_p - 1)^2 = 2^(2p) - 2^(p+2) + 4
		inputs += [u((2, 3), (p + 2, 2 * p)), r(2 * p - 1, seed)]
	m = (1 << p) - 1
	assert [operand(s) for s in inputs[:5]] == [0, m, 1 << p, 2 * m, m << p]
	if p >= 2:
		assert operand(inputs[5]) == (m - 1) ** 2 and operand(inputs[6]) < (m - 1) ** 2
	return unique(inputs)


def folding():
	f = Family("folding", [ORACLE, ENCODING])
	for i, p in enumerate(FOLD_EXPONENTS):
		m = (1 << p) - 1
		for s in fold_inputs(p, 200 + i):
			v = operand(s) % m
			f.add("fold", str(p), s, value(v))
			f.add("res64", str(p), s, "%016X" % (v & MASK))
	return f


# --- products: mul, sqr ------------------------------------------------------------------------

def karatsuba_threshold():
	# The threshold the products' sizes straddle, read from src/mp.hpp so that it is stated once
	with open("src/mp.hpp", encoding="ascii") as f:
		found = re.findall(r"^static const std::size_t KARATSUBA_THRESHOLD = ([0-9]+);", f.read(), re.M)
	assert len(found) == 1, "src/mp.hpp states KARATSUBA_THRESHOLD once"
	return int(found[0])


def limbs(n):
	return 64 * n


def product_cases():
	# (pairs, squares): T - 1, T and T + 1 limbs against each other, against a longer operand both
	# ways round, and squared; odd sizes from 5 limbs to below T and above 2 T, where the default
	# cutoff splits twice; unequal operands, one at most half the other, across the threshold too;
	# zero and one; all ones and carry patterns; and A * C, A^2 and C^2. The full-size product and
	# square the schoolbook check takes are the first of each: A * C, and A^2 before C^2.
	t = karatsuba_threshold()
	pairs, squares = [], []
	for i, s in enumerate((t - 1, t, t + 1)):
		seed = 300 + 10 * i
		pairs += [(r(limbs(s), seed), r(limbs(s) - 1, seed + 1)),
		          (r(limbs(s), seed + 2), r(limbs(300), seed + 3)),
		          (r(limbs(300) - 5, seed + 4), r(limbs(s), seed + 5))]
		squares += [r(limbs(s), seed + 6)]
	for i, n in enumerate((5, 7, 13, 33, t - 1 if t % 2 == 0 else t - 2, 2 * t + 1, 4 * t + 3)):
		seed = 340 + 3 * i
		pairs += [(r(limbs(n) - 3, seed), r(limbs(n), seed + 1))]
		squares += [r(limbs(n) + 1 - 64, seed + 2)]
	pairs += [
		(r(limbs(200), 370), r(limbs(7), 371)), (r(limbs(7) + 1, 372), r(limbs(200), 373)),
		(r(limbs(250), 374), r(limbs(120), 375)), (r(limbs(4 * t), 376), r(limbs(t) + 1, 377)),
		(x(0), r(limbs(150), 378)), (r(limbs(150), 379), x(1)), (x(1), x(1)),
		(u((0, 3072)), u((0, 3072))), (u((0, limbs(200))), u((0, limbs(t + 1)))),
		(c(limbs(150), 380), c(limbs(130), 381)), (c(12800, 382), c(6400, 383)),
		(A, C),
	]
	squares += [x(0), x(1), u((0, limbs(150))), u((0, 128)), c(limbs(200), 384), c(6400, 385), A, C]
	return pairs, squares


def products():
	f = Family("products", [ORACLE, ENCODING])
	pairs, squares = product_cases()
	for sa, sb in pairs:
		f.add("mul", sa, sb, value(operand(sa) * operand(sb)))
	for s in squares:
		f.add("sqr", s, value(operand(s) ** 2))
	return f


# --- modular: mod, powmod -----------------------------------------------------------------------

def limb_count(v):
	return (v.bit_length() + 63) // 64


def division_steps(x, m, refined):
	# Long division of x by a divisor m of two limbs or more, normalized so its top limb has its top
	# bit set: (clamps, the most corrections of one quotient digit). Unrefined, each digit is
	# estimated from the remainder's top two limbs over the divisor's top limb, clamped below 2^64,
	# and lowered while it is too large (MCA, Algorithm 1.6). Refined, the estimate is first lowered
	# while its product with the divisor's second limb exceeds what the remainder's next limb allows,
	# a more accurate estimate from the divisor's top two limbs (MCA, 1.4.1 and Exercise 1.20), after
	# which a correction, the add-back, is rare.
	s = (-m.bit_length()) % 64
	v, rest = m << s, x << s
	n = limb_count(v)
	top, second = v >> (64 * (n - 1)), (v >> (64 * (n - 2))) & MASK
	clamps = most = 0
	for j in range(limb_count(x) - n, -1, -1):
		window = rest >> (64 * j)
		q, r = divmod(window >> (64 * (n - 1)), top)
		if q > MASK:
			clamps += 1
		if refined:
			while q > MASK or q * second > (r << 64) + ((window >> (64 * (n - 2))) & MASK):
				q -= 1
				r += top
				if r > MASK:
					break
		q = min(q, MASK)
		corrections = 0
		while q * v > window:
			q -= 1
			corrections += 1
		most = max(most, corrections)
		rest -= (q * v) << (64 * j)
	assert rest >> s == x % m and rest % (1 << s) == 0
	return clamps, most


def limbs_from_top(*limbs):
	v = 0
	for limb in limbs:
		v = (v << 64) | limb
	return v


# The rare paths of long division, each asserted by simulating it. The clamp (MCA, Exercise 1.19):
# the remainder's top limb equals the divisor's, so the estimate reaches 2^64. Two corrections of
# the unrefined estimate: a seeded pair, found by searching seeds and fixed here. The add-back of
# the refined estimate: m = (h1 B + h0) B + (B - 1) with B = 2^64, and x = q (h1 B + h0) B, whose
# two-limb quotient q the refinement keeps, one too large for the whole divisor.
CLAMP = (limbs_from_top(1 << 63, 0, MASK), limbs_from_top(1 << 63, 1))
TWO_CORRECTIONS = (r(640, 700), r(192, 701))
ADD_BACK_HIGH = limbs_from_top(0x8000000123456789, 0xFEDCBA9876543210)
ADD_BACK = (0x8765432112345678 * ADD_BACK_HIGH << 64, (ADD_BACK_HIGH << 64) | MASK)


def rare_divisions():
	clamp_x, clamp_m = CLAMP
	assert division_steps(clamp_x, clamp_m, False)[0] > 0, "the clamp case clamps"
	two_x, two_m = operand(TWO_CORRECTIONS[0]), operand(TWO_CORRECTIONS[1])
	assert division_steps(two_x, two_m, False)[1] == 2, "the two-correction case corrects twice"
	back_x, back_m = ADD_BACK
	assert division_steps(back_x, back_m, True)[1] == 1, "the add-back case adds back"
	return [(x(clamp_x), x(clamp_m)), TWO_CORRECTIONS, (x(back_x), x(back_m))]


# Pairs (X, M): m = 1; a one-limb m; x < m, x = m, x a multiple of m; the rare paths; divisors whose
# normalization shift is 0 and 63; and x of about 2^17 bits by m from one limb to x's length
def modular_pairs():
	multiple = operand(r(640, 612)) * operand(r(300, 613))
	pairs = [
		(r(200, 601), x(1)), (u((0, 3000)), x(1)), (x(0), x(1)),
		(r(3000, 602), r(64, 603)), (r(640, 604), r(33, 605)), (u((0, 640)), x(3)),
		(r(64, 606), r(64, 607)), (r(500, 608), r(640, 609)), (x(0), r(640, 611)),
		(r(640, 610), r(640, 610)), (x(multiple), r(640, 612)),
		(x(operand(r(128, 614)) * operand(r(700, 615))), r(128, 614)),
	]
	pairs += rare_divisions()
	pairs += [
		(r(3000, 622), r(1024, 620)), (r(3000, 623), r(961, 621)), (r(2000, 624), r(700, 625)),
		(c(5000, 626), c(1500, 627)), (r(6400, 628), r(3200, 629)), (u((0, 4096)), u((0, 2048))),
		(r(131000, 630), r(64, 631)), (r(131000, 632), r(640, 633)), (r(131000, 634), r(65500, 635)),
		(r(131000, 636), r(130000, 637)), (r(131000, 638), r(131000, 639)),
	]
	assert (-operand(r(1024, 620)).bit_length()) % 64 == 0 and (-operand(r(961, 621)).bit_length()) % 64 == 63
	return pairs


# Triples (A, E, M): exponent 0, alone and with m = 1; m = 1; a base above m with a two-limb
# exponent; base 0; Fermat's 3^(M_127 - 1) mod M_127 = 1; moduli whose squares take Karatsuba; and
# exponent 1 with a base above m, a multiple of m and m itself, so that only the base's own reduction
# brings it into [0, m). Those four come last, and their operands are earlier ones, so no record moves.
POWERS = [
	(r(300, 640), x(0), r(200, 641)), (x(5), x(0), x(1)), (r(300, 642), r(100, 643), x(1)),
	(r(900, 644), r(128, 645), r(500, 646)), (x(0), r(64, 647), r(300, 648)),
	(x(3), u((1, 127)), u((0, 127))), (r(500, 649), r(128, 650), r(512, 651)),
	(r(1000, 652), r(1000, 653), r(1024, 654)), (r(64, 655), r(64, 656), r(64, 657)),
	(r(12800, 658), r(128, 659), r(6400, 660)),
	(r(300, 640), x(1), r(200, 641)), (x(operand(r(640, 612)) * operand(r(300, 613))), x(1), r(640, 612)),
	(r(640, 610), x(1), r(640, 610)), (u((0, 640)), x(1), x(3)),
]


def modular():
	f = Family("modular", [ORACLE, ENCODING,
	                       "the clamp, two-correction and add-back cases of long division are found by simulating "
	                       "it, with the one-limb estimate of Brent and Zimmermann, Modern Computer Arithmetic, "
	                       "2010, Algorithm 1.6, and with the refined estimate of its Exercise 1.20, and asserted there"])
	for sx, sm in modular_pairs():
		f.add("mod", sx, sm, value(operand(sx) % operand(sm)))
	for sa, se, sm in POWERS:
		f.add("powmod", sa, se, sm, value(pow(operand(sa), operand(se), operand(sm))))
	return f


# --- hex, decimal: every operand of every family, the edges, and M_p ----------------------------

OPERAND_FIELDS = {"cmp": (1, 2), "add": (1, 2), "sub": (1, 2), "shl": (1,), "shr": (1,), "fold": (2,), "res64": (2,),
                  "mul": (1, 2), "sqr": (1,), "mod": (1, 2), "powmod": (1, 2, 3)}
EDGES = [x(0), x(1), x(MASK), x(BETA)]


def every_operand():
	# The edges and M_p of the folding exponents, every operand of every family, then M_p of the
	# reference exponents: appended last, so that earlier records keep their lines
	specs = list(EDGES) + [u((0, p)) for p in FOLD_EXPONENTS]
	for build in (arith, folding, products, modular):
		for fields in build().records:
			specs += [fields[i] for i in OPERAND_FIELDS[fields[0]]]
	specs += [u((0, p)) for p in COMPOSITE]
	return unique(specs)


def hexadecimal():
	f = Family("hex", [ORACLE, ENCODING])
	for s in every_operand():
		v = operand(s)
		f.add("hex", s, string(v, "%X" % v))
	return f


def decimal():
	f = Family("decimal", [ORACLE, ENCODING])
	for s in every_operand():
		v = operand(s)
		f.add("dec", s, string(v, str(v)))
	return f


# --- reference: ll, prp ------------------------------------------------------------------------

# Prime p up to B with M_p composite: every one below 64, then roughly log-spaced up to 9973, the
# largest prime below B, with 1237 and 1277, whose Lucas-Lehmer Res64 issues/known-answer-tables.md
# records as published, with its sources
B = 10000
COMPOSITE = [11, 23, 29, 37, 41, 43, 47, 53, 59, 83, 113, 157, 211, 283, 383, 509, 691, 929, 1237, 1249,
             1277, 1669, 2273, 3049, 4111, 5527, 7433, 9973]


def lucas_lehmer(p):
	# s_(p-2) mod M_p from s_0 = 4, folding s^2 - 2 + M_p, which stays positive, modulo M_p
	m = (1 << p) - 1
	s = 4
	for _ in range(p - 2):
		s = s * s + m - 2
		while s >> p:
			s = (s & m) + (s >> p)
		if s == m:
			s = 0
	return s


def lucas_lehmer_by_remainder(p):
	# the same residue through Python's own remainder, a second reading of the recurrence
	m = (1 << p) - 1
	s = 4
	for _ in range(p - 2):
		s = (s * s - 2) % m
	return s


def reference():
	f = Family("reference", [ORACLE, "ll: s_(p-2) mod M_p from s_0 = 4, by folding and again by Python's remainder; "
	                                 "prp: the type-1 residue pow(3, M_p - 1, M_p)"])
	for p in COMPOSITE:
		s = lucas_lehmer(p)
		assert s == lucas_lehmer_by_remainder(p) and s != 0, p
		f.add("ll", str(p), x(s))
	for p in COMPOSITE:
		m = (1 << p) - 1
		t = pow(3, m - 1, m)
		assert t != 1, p
		f.add("prp", str(p), x(t))
	return f


# --- mersenne-exponents: the prime list ---------------------------------------------------------

# OEIS A000043, "Mersenne exponents: primes p such that 2^p - 1 is prime", its terms up to B
OEIS_A000043 = [2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281, 3217, 4253, 4423,
                9689, 9941]


def odd_primes(bound):
	# the sieve of Eratosthenes
	sieve = bytearray([1]) * (bound + 1)
	sieve[0] = sieve[1] = 0
	for i in range(2, bound + 1):
		if i * i > bound:
			break
		if sieve[i]:
			sieve[i * i::i] = bytes(len(range(i * i, bound + 1, i)))
	return [p for p in range(3, bound + 1) if sieve[p]]


def mersenne_exponents():
	# The OEIS terms, written only if a Lucas-Lehmer sweep over every odd prime up to B finds
	# exactly the odd ones
	found = [p for p in odd_primes(B) if lucas_lehmer(p) == 0]
	if found != [p for p in OEIS_A000043 if p % 2]:
		sys.stderr.write("generate.py: the Lucas-Lehmer sweep finds %r; OEIS A000043 lists %r\n" % (found, OEIS_A000043))
		sys.exit(1)
	f = Family("mersenne-exponents", [
		"OEIS-A000043 (https://oeis.org/A000043), Mersenne exponents: its terms up to %d, embedded in this command" % B,
		"a Lucas-Lehmer sweep in Python over every odd prime up to %d, in this command, finds exactly the odd terms; "
		"the command exits non-zero otherwise" % B,
	])
	for p in OEIS_A000043:
		f.add("mersenne", str(p))
	return f


FAMILIES = {
	"arith": arith,
	"folding": folding,
	"products": products,
	"modular": modular,
	"hex": hexadecimal,
	"decimal": decimal,
	"reference": reference,
	"mersenne-exponents": mersenne_exponents,
}


def main(argv):
	if len(argv) != 2 or argv[1] not in FAMILIES:
		sys.stderr.write("usage: python3 tests/vectors/generate.py {%s}\n" % ",".join(FAMILIES))
		return 2
	sys.stdout.buffer.write(FAMILIES[argv[1]]().text())
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
