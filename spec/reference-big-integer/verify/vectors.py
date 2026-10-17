# The vector files' gate: a cycle record, never part of mps. Python 3.8+, standard library only.
#
#     python3 -I -B vectors.py PHASE ROOT THRESHOLD      check every family present by PHASE
#     python3 -I -B vectors.py tamper HOW FILE           edit one family for a tamper run (common.sh)
#     python3 -I -B vectors.py coverage PHASE ROOT THRESHOLD TRACE
#                                                        every record reaches its entry points
#
# The check reads each family under ROOT/tests/vectors/ against PLAN § 2's grammar, recomputes every
# record with Python's integers from its own reading of the operand forms (independent of
# generate.py), and checks the sizes and edges PLAN § 2 lists. It prints `GATE FAIL: <reason>` and
# exits 1 at the first failure, or ends with one summary line.

import hashlib
import os
import re
import sys

if hasattr(sys, "set_int_max_str_digits"):
	sys.set_int_max_str_digits(0)

W = 64
MASK = (1 << W) - 1
B = 10000
OEIS_A000043 = [2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281, 3217, 4253, 4423,
                9689, 9941, 11213, 19937, 21701, 23209]
COMPOSITE_SET = [11, 23, 29, 37, 41, 43, 47, 53, 59, 83, 113, 157, 211, 283, 383, 509, 691, 929, 1237, 1249,
                 1277, 1669, 2273, 3049, 4111, 5527, 7433, 9973]
FOLD_EXPONENTS = [1, 2, 3, 5, 7, 31, 61, 63, 64, 65, 89, 127, 128, 129, 521, 1279, 4423, 9941]
FAMILIES = [("arith", 1), ("folding", 1), ("hex", 1), ("products", 2), ("modular", 3), ("decimal", 3),
            ("reference", 4), ("mersenne-exponents", 4)]
OPS = {  # op: (family, operand fields, then the kind of the result field or None)
	"cmp": ("arith", "AA", "cmp"), "add": ("arith", "AA", "value"), "sub": ("arith", "AA", "value"),
	"shl": ("arith", "Ak", "value"), "shr": ("arith", "Ak", "value"),
	"fold": ("folding", "pA", "value"), "res64": ("folding", "pA", "res64"),
	"hex": ("hex", "A", "string"), "dec": ("decimal", "A", "string"),
	"mul": ("products", "AA", "value"), "sqr": ("products", "A", "value"),
	"mod": ("modular", "AA", "value"), "powmod": ("modular", "AAA", "value"),
	"ll": ("reference", "p", "full"), "prp": ("reference", "p", "full"),
	"mersenne": ("mersenne-exponents", "p", None),
}
HEX = r"(0|[1-9a-f][0-9a-f]*)"
DEC = r"(0|[1-9][0-9]*)"
SHA = r"[0-9a-f]{64}"
OPERAND = re.compile(r"x:%s|u:%s-%s(,%s-%s)*|[rc]:[1-9][0-9]*:%s" % (HEX, DEC, DEC, DEC, DEC, DEC))
VALUE = re.compile(r"x:%s|h:[1-9][0-9]*:[0-9a-f]{16}:%s" % (HEX, SHA))
STRING = re.compile(r"s:[0-9A-F]+|s:[0-9]+|g:[1-9][0-9]*:%s" % SHA)


class Fail(Exception):
	pass


def fail(msg):
	raise Fail(msg)


# --- operands ------------------------------------------------------------------------------------

class SplitMix64:
	# Steele, Lea and Flood 2014, Fig. 16, as PLAN § 2 gives it
	def __init__(self, seed):
		self.state = seed & MASK

	def next(self):
		self.state = (self.state + 0x9E3779B97F4A7C15) & MASK
		z = self.state
		z = ((z ^ (z >> 33)) * 0xFF51AFD7ED558CCD) & MASK
		z = ((z ^ (z >> 33)) * 0xC4CEB9FE1A85EC53) & MASK
		return z ^ (z >> 33)


def topped(limbs, bits):
	v = 0
	for i, l in enumerate(limbs):
		v |= l << (W * i)
	v &= (1 << bits) - 1
	return v | (1 << (bits - 1))


def operand(spec):
	kind, rest = spec[0], spec[2:]
	if kind == "x":
		return int(rest, 16)
	if kind == "u":
		v = 0
		for part in rest.split(","):
			lo, hi = (int(t) for t in part.split("-"))
			if not lo < hi:
				fail("bit range %s is empty" % part)
			v |= ((1 << (hi - lo)) - 1) << lo
		return v
	bits, seed = (int(t) for t in rest.split(":"))
	if seed > MASK:
		fail("seed %d is above 2^64 - 1" % seed)
	g = SplitMix64(seed)
	n = (bits + W - 1) // W
	if kind == "r":
		return topped([g.next() for _ in range(n)], bits)
	limbs = []
	while len(limbs) < n:
		v = g.next()
		kind3, run = v % 3, 1 + (v >> 32) % 32
		for _ in range(run):
			limbs.append(0 if kind3 == 0 else MASK if kind3 == 1 else g.next())
	return topped(limbs[:n], bits)


def limb_bytes(v):
	return v.to_bytes(8 * ((v.bit_length() + W - 1) // W), "little")


def value_field(v):
	if v.bit_length() <= 1024:
		return "x:" + format(v, "x")
	return "h:%d:%016x:%s" % (v.bit_length(), v & MASK, hashlib.sha256(limb_bytes(v)).hexdigest())


def string_field(s, v):
	if v.bit_length() <= 1024:
		return "s:" + s
	return "g:%d:%s" % (len(s), hashlib.sha256(s.encode("ascii")).hexdigest())


# --- the file ------------------------------------------------------------------------------------

def read_family(root, stem):
	path = os.path.join(root, "tests", "vectors", stem + ".txt")
	where = "tests/vectors/%s.txt" % stem
	try:
		data = open(path, "rb").read()
	except OSError:
		fail("%s is missing" % where)
	if not data.endswith(b"\n") or b"\r" in data or b"\t" in data:
		fail("%s: LF line ends only, no CR, no tab, and a final LF" % where)
	try:
		text = data.decode("ascii")
	except UnicodeDecodeError:
		fail("%s: not ASCII" % where)
	lines = text.split("\n")[:-1]
	if len(lines) < 5:
		fail("%s: too short to hold the envelope" % where)
	if lines[0] != "# mps vectors 1":
		fail("%s:1: the magic line '# mps vectors 1'" % where)
	if lines[1] != "# command: python3 tests/vectors/generate.py %s > tests/vectors/%s.txt" % (stem, stem):
		fail("%s:2: the command line naming its own stem (R8)" % where)
	m = re.fullmatch(r"# end ([1-9][0-9]*) records (%s)" % SHA, lines[-1])
	if not m:
		fail("%s:%d: the trailer '# end N records <sha256>'" % (where, len(lines)))
	head = text[:len(text) - len(lines[-1]) - 1]
	if hashlib.sha256(head.encode("ascii")).hexdigest() != m.group(2):
		fail("%s: the trailer's digest is not the SHA-256 of the bytes before it" % where)
	sources, records, k = [], [], 2
	while k < len(lines) - 1 and lines[k].startswith("# source: "):
		if not lines[k][len("# source: "):].strip():
			fail("%s:%d: an empty source" % (where, k + 1))
		sources.append(lines[k])
		k += 1
	if not sources:
		fail("%s:3: at least one '# source: ' line (R8)" % where)
	while k < len(lines) - 1 and lines[k].startswith("# "):
		k += 1
	for n in range(k, len(lines) - 1):
		line = lines[n]
		if line == "" or line.startswith("#") or "  " in line or line != line.strip():
			fail("%s:%d: a record is one op and its fields, single-spaced; notes come before the records" % (where, n + 1))
		f = line.split(" ")
		op = f[0]
		if op not in OPS or OPS[op][0] != stem:
			fail("%s:%d: op %r does not belong in this family" % (where, n + 1, op))
		_, args, res = OPS[op]
		want = len(args) + (1 if res else 0)
		if len(f) - 1 != want:
			fail("%s:%d: %s takes %d fields" % (where, n + 1, op, want))
		for a, t in zip(args, f[1:]):
			ok = OPERAND.fullmatch(t) if a == "A" else re.fullmatch(DEC, t)
			if not ok:
				fail("%s:%d: malformed field %r" % (where, n + 1, t[:80]))
		if res:
			t = f[-1]
			ok = {"cmp": re.fullmatch(r"lt|eq|gt", t), "value": VALUE.fullmatch(t),
			      "res64": re.fullmatch(r"[0-9A-F]{16}", t), "string": STRING.fullmatch(t),
			      "full": re.fullmatch("x:" + HEX, t)}[res]
			if not ok:
				fail("%s:%d: malformed result %r" % (where, n + 1, t[:80]))
		records.append((n + 1, f))
	if int(m.group(1)) != len(records):
		fail("%s: the trailer counts %s records; the file holds %d" % (where, m.group(1), len(records)))
	return where, sources, records


# --- recomputation -------------------------------------------------------------------------------

def mersenne(p):
	return (1 << p) - 1


def ll(p):
	m, s = mersenne(p), 4
	for _ in range(p - 2):
		s = s * s - 2
		s = (s & m) + (s >> p)
		s = (s & m) + (s >> p)
		if s >= m:
			s -= m
	return s


def isprime(n):
	if n < 2:
		return False
	d = 2
	while d * d <= n:
		if n % d == 0:
			return False
		d += 1
	return True


def recompute(where, n, f):
	op = f[0]
	_, args, res = OPS[op]
	vals = [operand(t) if a == "A" else int(t) for a, t in zip(args, f[1:])]
	got = f[-1] if res else None
	if op == "cmp":
		a, b = vals
		want = "lt" if a < b else "eq" if a == b else "gt"
	elif op in ("add", "sub", "mul", "sqr", "shl", "shr", "fold", "mod", "powmod"):
		if op == "add":
			r = vals[0] + vals[1]
		elif op == "sub":
			if vals[0] < vals[1]:
				fail("%s:%d: sub with A < B" % (where, n))
			r = vals[0] - vals[1]
		elif op == "mul":
			r = vals[0] * vals[1]
		elif op == "sqr":
			r = vals[0] * vals[0]
		elif op == "shl":
			r = vals[0] << vals[1]
		elif op == "shr":
			r = vals[0] >> vals[1]
		elif op == "fold":
			if vals[0] < 1:
				fail("%s:%d: fold with p = 0" % (where, n))
			r = vals[1] % mersenne(vals[0])
		elif op == "mod":
			if vals[1] == 0:
				fail("%s:%d: mod by 0" % (where, n))
			r = vals[0] % vals[1]
		else:
			if vals[2] == 0:
				fail("%s:%d: powmod by 0" % (where, n))
			r = pow(vals[0], vals[1], vals[2])
		want = value_field(r)
	elif op == "res64":
		want = format((vals[1] % mersenne(vals[0])) & MASK, "016X")
	elif op == "hex":
		want = string_field(format(vals[0], "X"), vals[0])
	elif op == "dec":
		want = string_field(str(vals[0]), vals[0])
	elif op == "ll":
		want = "x:" + format(ll(vals[0]), "x")
	elif op == "prp":
		m = mersenne(vals[0])
		want = "x:" + format(pow(3, m - 1, m), "x")
	else:
		return vals
	if got != want:
		fail("%s:%d: %s's result is not what Python computes (want %s)" % (where, n, op, want[:90]))
	return vals


# --- division simulations (MCA Algorithm 1.6; Knuth's refined estimate) ---------------------------

def division_paths(x, m):
	# The paths a long division of x by m takes: the clamp (q* >= beta), two corrections of the one-limb
	# estimate, and an add-back after Knuth's two-limb refinement
	paths = set()
	if m.bit_length() <= W or x < m:
		return paths
	s = (W - m.bit_length() % W) % W
	v, u = m << s, x << s
	n = (v.bit_length() + W - 1) // W
	vt, vs = v >> (W * (n - 1)), (v >> (W * (n - 2))) & MASK
	ul = (x.bit_length() + W - 1) // W + 1
	r = u
	for j in range(ul - n - 1, -1, -1):
		window = r >> (W * j)
		qstar = (window >> (W * (n - 1))) // vt
		if qstar > MASK:
			paths.add("clamp")
		q, corrections = min(qstar, MASK), 0
		while q * v > window:
			q -= 1
			corrections += 1
		if corrections >= 2:
			paths.add("two-corrections")
		qh, rh = divmod(window >> (W * (n - 1)), vt)
		u2 = (window >> (W * (n - 2))) & MASK
		while qh > MASK or qh * vs > ((rh << W) | u2):
			qh -= 1
			rh += vt
			if rh > MASK:
				break
		if qh * v > window:
			paths.add("add-back")
		r -= (q * v) << (W * j)
	if s == 0:
		paths.add("shift-0")
	if s == W - 1:
		paths.add("shift-63")
	return paths


# --- the families ---------------------------------------------------------------------------------

def limbs(v):
	return (v.bit_length() + W - 1) // W


def carry_run(a, b):
	best = run = c = 0
	for i in range(max(limbs(a), limbs(b))):
		s = ((a >> (W * i)) & MASK) + ((b >> (W * i)) & MASK) + c
		c = s >> W
		run = run + 1 if c else 0
		best = max(best, run)
	return best


def borrow_run(a, b):
	best = run = br = 0
	for i in range(limbs(a)):
		d = ((a >> (W * i)) & MASK) - ((b >> (W * i)) & MASK) - br
		br = 1 if d < 0 else 0
		run = run + 1 if br else 0
		best = max(best, run)
	return best


def need(cond, msg):
	if not cond:
		fail(msg)


def check(phase, root, threshold):
	T = threshold
	big = (1 << 20) - 64
	data, operands, counts = {}, set(), 0
	for stem, ph in FAMILIES:
		if ph > phase:
			continue
		where, sources, records = read_family(root, stem)
		rows = []
		for n, f in records:
			vals = recompute(where, n, f)
			rows.append((n, f, vals))
			_, args, _ = OPS[f[0]]
			for a, t, v in zip(args, f[1:], vals):
				if a == "A" and f[0] not in ("hex", "dec"):
					operands.add(v)
		data[stem] = (where, sources, rows)
		counts += len(records)

	def ops(stem, op):
		return [(f, v) for _, f, v in data[stem][2] if f[0] == op]

	# arith (R1)
	for op in ("cmp", "add", "sub", "shl", "shr"):
		need(ops("arith", op), "arith: no %s records" % op)
	res = {f[-1] for f, _ in ops("arith", "cmp")}
	need(res == {"lt", "eq", "gt"}, "arith: cmp must come out lt, eq and gt")
	need(any(limbs(a) == limbs(b) >= 2 and a != b and a >> W == b >> W for _, (a, b) in ops("arith", "cmp")),
	     "arith: a cmp of equal lengths differing only in the low limb")
	need(any(limbs(a) == limbs(b) >= 2 and a != b and a % (1 << (W * (limbs(a) - 1))) == b % (1 << (W * (limbs(a) - 1)))
	         for _, (a, b) in ops("arith", "cmp")), "arith: a cmp of equal lengths differing only in the top limb")
	lens = set()
	for op in ("cmp", "add", "sub", "shl", "shr"):
		for _, v in ops("arith", op):
			lens.update(x.bit_length() for x, a in zip(v, OPS[op][1]) if a == "A")
	for k in (1, 2, 4, 8, 48):
		for L in (64 * k - 1, 64 * k, 64 * k + 1):
			need(L in lens, "arith: an operand of %d bits (64k - 1, 64k and 64k + 1 for k = 1, 2, 4, 8, 48)" % L)
	need(any(carry_run(a, b) >= 4 for _, (a, b) in ops("arith", "add")), "arith: a sum whose carry runs through four limbs")
	need(any(borrow_run(a, b) >= 4 for _, (a, b) in ops("arith", "sub")), "arith: a difference whose borrow runs through four limbs")
	for op in ("shl", "shr"):
		ks = {v[1] for _, v in ops("arith", op)}
		for k in (0, 1, 63, 64, 65, 128, 1000):
			need(k in ks, "arith: %s by %d" % (op, k))
	need(any(k >= a.bit_length() and a > 0 for _, (a, k) in ops("arith", "shr")), "arith: a right shift past the operand's length")
	bigs = {x for x in operands if x.bit_length() >= big}
	for op in ("cmp", "add", "sub", "shl", "shr"):
		need(any(v[0].bit_length() >= big for _, v in ops("arith", op)), "arith: %s with an operand of about 2^20 bits" % op)
	# The two large operands: A = r:1048576:s and C = c:1048575:s, and no other operand above 2^17 bits
	large = {}
	for stem in data:
		for _, f, vals in data[stem][2]:
			for a, t, x in zip(OPS[f[0]][1], f[1:], vals):
				if a == "A" and x.bit_length() > 1 << 17:
					large[x] = t
	kinds = sorted((x.bit_length(), t.split(":")[0]) for x, t in large.items())
	need(kinds == [((1 << 20) - 1, "c"), (1 << 20, "r")],
	     "the operands above 2^17 bits must be exactly A = r:1048576:<seed> and C = c:1048575:<seed>; found %s" % kinds)

	# folding (R2, R4)
	fold = {}
	for f, (p, x) in ops("folding", "fold"):
		fold.setdefault(p, set()).add(x)
	r64 = {}
	for f, (p, x) in ops("folding", "res64"):
		r64.setdefault(p, set()).add(x)
	for p in FOLD_EXPONENTS:
		m = mersenne(p)
		edges = {0, m, 1 << p, 2 * m, (1 << p) * m, (m - 1) ** 2}
		for name, have in (("fold", fold), ("res64", r64)):
			need(p in have and edges <= have[p], "folding: %s at p = %d on 0, M_p, 2^p, 2 M_p, 2^p M_p and (M_p - 1)^2" % (name, p))
			need(any(x < (m - 1) ** 2 and x not in edges for x in have[p]) or p <= 2,
			     "folding: %s at p = %d on a random input below (M_p - 1)^2" % (name, p))

	# hex (R3)
	def covered(stem):
		have = {v[0] for f, v in ops(stem, stem[:3] if stem == "hex" else "dec")}
		want = set(operands) | {0, 1, MASK, MASK + 1}
		for st in ("folding", "reference"):
			if st in data:
				want |= {mersenne(v[0]) for _, f, v in data[st][2] if f[0] in ("fold", "res64", "ll")}
		missing = want - have
		need(not missing, "%s: no record for %d operands, e.g. one of %d bits (R3: every vector operand)"
		     % (stem, len(missing), max(x.bit_length() for x in missing) if missing else 0))
	covered("hex")

	# products (R1)
	if phase >= 2:
		muls = [v for _, v in ops("products", "mul")]
		sqrs = [v[0] for _, v in ops("products", "sqr")]
		need(muls and sqrs, "products: mul and sqr records")
		for s in (T - 1, T, T + 1):
			need(any(limbs(a) == s and limbs(b) == s for a, b in muls), "products: a mul of two %d-limb operands" % s)
			need(any(limbs(a) == s and limbs(b) > T + 1 for a, b in muls), "products: a mul of %d limbs by a larger operand" % s)
			need(any(limbs(b) == s and limbs(a) > T + 1 for a, b in muls), "products: a mul of a larger operand by %d limbs" % s)
			need(any(limbs(a) == s for a in sqrs), "products: a sqr of %d limbs" % s)
		for name, sizes in (("mul", [max(limbs(a), limbs(b)) for a, b in muls]), ("sqr", [limbs(a) for a in sqrs])):
			need(any(n % 2 and 5 <= n < T for n in sizes), "products: a %s at an odd size from 5 below the threshold" % name)
			need(any(n % 2 and n > 2 * T for n in sizes), "products: a %s at an odd size above twice the threshold" % name)
		need(any(min(limbs(a), limbs(b)) * 2 <= max(limbs(a), limbs(b)) and min(a, b) > 0 for a, b in muls),
		     "products: a mul of unequal operands, one at most half the other")
		ones = lambda x: x.bit_length() >= 128 and x & (x + 1) == 0
		need(any(ones(a) or ones(b) for a, b in muls) and any(ones(a) for a in sqrs), "products: all-ones operands in mul and sqr")
		need(any(a.bit_length() >= big and b.bit_length() >= big for a, b in muls), "products: a mul of the two operands of about 2^20 bits")
		need(len({a for a in sqrs if a.bit_length() >= big}) >= 2, "products: a sqr of each operand of about 2^20 bits")
		cs = [f for f, _ in ops("products", "mul") + ops("products", "sqr") if any(t.startswith("c:") for t in f[1:-1])]
		need(cs, "products: carry-pattern operands")

	# modular (R2)
	if phase >= 3:
		mods = [v for _, v in ops("modular", "mod")]
		pows = [v for _, v in ops("modular", "powmod")]
		need(mods and pows, "modular: mod and powmod records")
		need(any(m == 1 for x, m in mods), "modular: mod by m = 1")
		need(any(limbs(m) == 1 and m > 1 and x >= m for x, m in mods), "modular: mod by a one-limb m")
		need(any(x < m for x, m in mods), "modular: mod with x < m")
		need(any(x == m for x, m in mods), "modular: mod with x = m")
		need(any(x > m > 1 and x % m == 0 for x, m in mods), "modular: mod of a multiple of m")
		paths = set()
		for x, m in mods:
			paths |= division_paths(x, m)
		for p in ("clamp", "two-corrections", "add-back", "shift-0", "shift-63"):
			need(p in paths, "modular: a division that takes the %s path" % p)
		need(any(e == 0 and m > 1 for b, e, m in pows), "modular: powmod with exponent 0")
		need(any(m == 1 for b, e, m in pows), "modular: powmod modulo 1")
		need(any(b >= m > 1 for b, e, m in pows), "modular: powmod with a base above m")
		need(any(b == 0 for b, e, m in pows), "modular: powmod of base 0")
		need(any(limbs(e) >= 2 for b, e, m in pows), "modular: powmod with a two-limb exponent")
		covered("decimal")

	# reference (R6, R7) and the prime list (R8)
	if phase >= 4:
		for op in ("ll", "prp"):
			ps = [v[0] for _, v in ops("reference", op)]
			need(sorted(ps) == COMPOSITE_SET, "reference: %s for exactly the composite set of PLAN § 2" % op)
		for f, (p,) in ops("reference", "ll"):
			need(ll(p) != 0, "reference: M_%d is prime, not composite" % p)
		where, sources, rows = data["mersenne-exponents"]
		listed = [v[0] for _, f, v in rows]
		need(listed == [p for p in OEIS_A000043 if p <= B], "the prime list is not OEIS A000043 up to %d, in order" % B)
		need(any("OEIS-A000043" in s for s in sources), "the prime list's sources name OEIS-A000043 (R8)")
		for p in listed:
			need(isprime(p), "the prime list holds %d, which is not prime" % p)

	print("vectors: %d families, %d records recomputed, %d distinct operands" % (len(data), counts, len(operands)))


# --- coverage ------------------------------------------------------------------------------------

def fpv(x):
	if x == 0:
		return "0.0.0.0"
	mix, y = 0, x
	while y:
		mix ^= y & MASK
		y >>= W
	return "%x.%x.%x.%x" % (x.bit_length(), x & MASK, x >> (W * (limbs(x) - 1)), mix)


def fps(s):
	h = 0xCBF29CE484222325
	for c in s.encode("ascii"):
		h = ((h ^ c) * 0x100000001B3) & MASK
	return "s%x.%x" % (len(s), h)


def coverage(phase, root, threshold, path):
	# Every record reaches the entry points PLAN § 2 names, with its operands in the record's order,
	# in the pass-through control run of bin/check (mutant.cpp's trace)
	calls = set(open(path).read().split("\n"))
	T, small, big = threshold, 1 << 17, (1 << 20) - 64
	missing = []
	n = 0
	for stem, ph in FAMILIES:
		if ph > phase:
			continue
		where, sources, records = read_family(root, stem)
		for line, f in records:
			op = f[0]
			_, args, _ = OPS[op]
			vals = [operand(t) if a == "A" else int(t) for a, t in zip(args, f[1:])]
			want = []
			if op == "cmp":
				want = [["compare %s %s" % (fpv(vals[0]), fpv(vals[1]))]]
			elif op in ("add", "sub"):
				want = [["%s %s %s" % ("add" if op == "add" else "subtract", fpv(vals[0]), fpv(vals[1]))]]
			elif op in ("shl", "shr"):
				want = [["%s %s %d" % ("shift_left" if op == "shl" else "shift_right", fpv(vals[0]), vals[1])]]
			elif op in ("fold", "res64"):
				want = [["%s %s %d" % ("reduce_mersenne" if op == "fold" else "res64", fpv(vals[1]), vals[0])]]
			elif op in ("hex", "dec"):
				s = format(vals[0], "X") if op == "hex" else str(vals[0])
				to, frm = ("to_hex", "from_hex") if op == "hex" else ("to_decimal", "from_decimal")
				want = [["%s %s" % (to, fpv(vals[0]))], ["%s %s" % (frm, fps(s))]]
			elif op == "mul":
				a, b = vals
				fa, fb = fpv(a), fpv(b)
				want = [["multiply %s %s" % (fa, fb)], ["multiply_karatsuba %s %s %d" % (fa, fb, T)]]
				if max(a.bit_length(), b.bit_length()) < small:
					want += [["multiply_schoolbook %s %s" % (fa, fb)], ["multiply_karatsuba %s %s 4" % (fa, fb)]]
				elif a.bit_length() >= big and b.bit_length() >= big:
					want += [["multiply_schoolbook %s %s" % (fa, fb)]]
			elif op == "sqr":
				a = vals[0]
				fa = fpv(a)
				want = [["square %s" % fa], ["square_karatsuba %s %d" % (fa, T)]]
				if a.bit_length() < small:
					want += [["square_schoolbook %s" % fa], ["square_karatsuba %s 4" % fa]]
				elif a.bit_length() == 1 << 20:
					want += [["square_schoolbook %s" % fa]]
			elif op == "mod":
				want = [["reduce %s %s" % (fpv(vals[0]), fpv(vals[1])), "divide %s %s" % (fpv(vals[0]), fpv(vals[1]))]]
			elif op == "powmod":
				want = [["power_mod %s %s %s" % tuple(fpv(x) for x in vals)]]
			elif op in ("ll", "prp", "mersenne"):
				p = vals[0]
				if op == "mersenne" and p < 3:
					continue
				want = [["lucas_lehmer %d" % p]] if op == "ll" else [["prp3 %d" % p]] if op == "prp" \
				       else [["lucas_lehmer %d" % p], ["prp3 %d" % p]]
			for alternatives in want:
				n += 1
				if not any(w in calls for w in alternatives):
					missing.append("%s:%d (%s) never reaches %s" % (where, line, op, " or ".join(w.split(" ")[0] for w in alternatives)))
	if missing:
		fail("%d of %d record calls never happen in make check, e.g. %s" % (len(missing), n, "; ".join(missing[:3])))
	print("coverage: every record reaches its %d entry-point calls" % n)


# --- tampering ------------------------------------------------------------------------------------

def rewrite(path, lines, keep_trailer=False):
	if not keep_trailer:
		body = lines[:-1]
		count = sum(1 for l in body if not l.startswith("#"))
		head = "".join(l + "\n" for l in body)
		lines = body + ["# end %d records %s" % (count, hashlib.sha256(head.encode("ascii")).hexdigest())]
	open(path, "w").write("".join(l + "\n" for l in lines))


def flip(field):
	# one character of a result changed, the field still well-formed
	if field in ("lt", "eq", "gt"):
		return {"lt": "gt", "eq": "lt", "gt": "lt"}[field]
	last = field[-1]
	alt = {"0": "1"}.get(last, "0")
	if field.startswith("h:"):
		kind, bits, low, sha = field.split(":")
		return "h:%s:%s%s:%s" % (bits, low[:-1], "1" if low[-1] == "0" else "0", sha)
	if field.startswith("x:") and field == "x:0":
		return "x:1"
	return field[:-1] + alt


def tamper(how, path):
	lines = open(path).read().split("\n")[:-1]
	if how == "trailer":
		t = lines[-1]
		lines[-1] = t[:-1] + ("0" if t[-1] != "0" else "1")
		return rewrite(path, lines, keep_trailer=True)
	if how == "command":
		return rewrite(path, [l for l in lines if not l.startswith("# command: ")])
	if how == "source":
		return rewrite(path, [l for l in lines if not l.startswith("# source: ")])
	if how == "drop-oeis":
		return rewrite(path, [l for l in lines if not (l.startswith("# source: ") and "OEIS-A000043" in l)])
	if how == "add-composite":
		k = next(i for i, l in enumerate(lines) if l.startswith("mersenne ") and int(l.split()[1]) > 11)
		return rewrite(path, lines[:k] + ["mersenne 11"] + lines[k:])
	if how.startswith("value-"):
		op = how[len("value-"):]
		k = max(i for i, l in enumerate(lines) if l.split(" ")[0] == op)
		f = lines[k].split(" ")
		f[-1] = flip(f[-1])
		lines[k] = " ".join(f)
		return rewrite(path, lines)
	fail("unknown tamper %s" % how)


def main(argv):
	try:
		if len(argv) == 6 and argv[1] == "coverage":
			coverage(int(argv[2]), argv[3], int(argv[4]), argv[5])
			return 0
		if len(argv) == 4 and argv[1] == "tamper":
			tamper(argv[2], argv[3])
			return 0
		if len(argv) == 4:
			check(int(argv[1]), argv[2], int(argv[3]))
			return 0
	except Fail as e:
		print("GATE FAIL: %s" % e, file=sys.stderr)
		return 1
	print("usage: vectors.py PHASE ROOT THRESHOLD | tamper HOW FILE", file=sys.stderr)
	return 2


if __name__ == "__main__":
	sys.exit(main(sys.argv))
