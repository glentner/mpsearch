# Gate probe driver: a cycle record, never part of mps. Python 3.8+, standard library only.
#
#     python3 -I -B probe.py PHASE PROBE_BINARY
#
# Builds the cases for every phase up to PHASE from its own seeded generator (independent of the
# committed vectors and of their generator), answers each with Python's integers, runs the probe
# binary on them and compares line by line. Prints `probe: N cases agree` or the first mismatches
# and exits 1.

import random
import subprocess
import sys

PHASE = int(sys.argv[1])
BINARY = sys.argv[2]
R = random.Random(20261012)
W = 64
MASK = (1 << W) - 1
cases = []  # (line, expected)


def h(x):
	return format(x, "x")


def sbytes(s):
	return s.encode("utf-8").hex() or "-"


def case(line, expected):
	cases.append((line, expected))


def ones(k):
	return (1 << (W * k)) - 1


def rand_limbs(n):
	# n limbs, the top one nonzero
	if n == 0:
		return 0
	x = R.getrandbits(W * n)
	return x | (1 << (W * n - 1 - R.randrange(W)))


def structured(n):
	# values that make long carry and borrow chains, and sparse ones
	if n == 0:
		return [0] * 6
	return [ones(n), 1 << (W * n - 1), (1 << (W * (n - 1))) if n > 1 else 1, ones(n) ^ 1,
	        ones(n) - (1 << (W * (n - 1))) + 1, rand_limbs(n)]


def values(max_limbs):
	out = [0, 1, 2, MASK, MASK + 1, (1 << 128) - 1]
	for n in range(1, max_limbs + 1):
		out += structured(n)
	return out


def mersenne(p):
	return (1 << p) - 1


# --- P1 ------------------------------------------------------------------------------------------

vals = values(9)
for a in vals:
	for b in R.sample(vals, 8) + [a, a + 1, max(a - 1, 0)]:
		case("cmp %s %s" % (h(a), h(b)), str((a > b) - (a < b)))
		case("eq %s %s" % (h(a), h(b)), "1" if a == b else "0")
		case("add %s %s" % (h(a), h(b)), h(a + b))
		case("sub %s %s" % (h(a), h(b)), h(a - b) if a >= b else "ERR")
	case("bits %s" % h(a), str(a.bit_length()))
for n in range(1, 12):
	for m in range(1, 12):
		a, b = ones(n), ones(m)
		case("add %s %s" % (h(a), h(b)), h(a + b))
		case("add %s 1" % h(a), h(a + 1))
		big, small = (a + 1) << 3, b
		case("sub %s %s" % (h(big), h(small)), h(big - small) if big >= small else "ERR")
		case("sub %s 1" % h(1 << (W * n)), h((1 << (W * n)) - 1))
for _ in range(300):
	a, b = rand_limbs(R.randrange(0, 40)), rand_limbs(R.randrange(0, 40))
	case("add %s %s" % (h(a), h(b)), h(a + b))
	case("sub %s %s" % (h(a), h(b)), h(a - b) if a >= b else "ERR")
	case("cmp %s %s" % (h(a), h(b)), str((a > b) - (a < b)))
counts = [0, 1, 2, 31, 63, 64, 65, 127, 128, 129, 191, 192, 640, 1000, 4096] + [R.randrange(0, 3000) for _ in range(10)]
for a in values(5) + [rand_limbs(R.randrange(1, 30)) for _ in range(20)]:
	for s in counts:
		case("shl %s %d" % (h(a), s), h(a << s))
		case("shr %s %d" % (h(a), s), h(a >> s))

exps = sorted(set(list(range(1, 70)) + [61, 89, 107, 127, 128, 129, 191, 192, 193, 521, 607, 1279, 2203]
                  + [R.randrange(2, 5000) for _ in range(15)]))
for p in exps:
	m = mersenne(p)
	edges = [0, 1, m, m + 1, 2 * m, 1 << p, (1 << p) * m, (m - 1) ** 2, m * m, (1 << (2 * p)) - 1,
	         R.randrange(0, (m - 1) ** 2 + 1), R.randrange(0, 1 << (3 * p))]
	for x in edges:
		case("fold %s %d" % (h(x), p), h(x % m))
		case("res64 %s %d" % (h(x), p), format((x % m) & MASK, "016X"))
	case("mersenne %d" % p, h(m))
case("fold 5 0", "ERR")
case("mersenne 0", "0")
for v in [0, 1, 9, 10, MASK, 0x123456789ABCDEF, 0xABCDEF0123456789]:
	case("hex64 %s" % h(v), format(v, "016X"))

conv = [0, 1, 9, 10, 99, MASK, MASK + 1, 10 ** 19, 10 ** 19 - 1, 10 ** 19 + 1, 10 ** 38, 10 ** 57 + 5,
        mersenne(61), mersenne(64), mersenne(127), mersenne(521), mersenne(1279)]
conv += [rand_limbs(R.randrange(1, 70)) for _ in range(40)] + [10 ** R.randrange(20, 400) * R.randrange(1, 9) for _ in range(10)]
for x in conv:
	x16 = format(x, "X")
	case("hex %s" % h(x), x16)
	case("phex %s" % sbytes(x16), h(x))
	case("phex %s" % sbytes(x16.lower()), h(x))
	case("phex %s" % sbytes("00" + x16), h(x))
for bad in ["-1", "+1", " 1", "1 ", "0x1F", "G", "1g", "12_3", "１", "1.0", "f\n"]:
	case("phex %s" % sbytes(bad), "ERR")
case("phex -", "ERR")
case("res64 5 0", "ERR")


def long_bad(digits, chunk):
	# Strings of one to three chunks with one bad character at the first, the last, a chunk boundary
	# and an interior position, so a parser that validates only some of its chunks is seen
	out = []
	for n in (chunk - 1, chunk, chunk + 1, 2 * chunk, 2 * chunk + 3, 3 * chunk):
		base = "".join(R.choice(digits) for _ in range(n))
		for i in sorted({0, n - 1, chunk - 1, min(chunk, n - 1), n // 2, max(n - chunk, 0)}):
			for bad in ("G" if digits != "0123456789" else "a", " ", "-", "_"):
				out.append(base[:i] + bad + base[i + 1:])
	return out


for bad in long_bad("0123456789ABCDEFabcdef", 16):
	case("phex %s" % sbytes(bad), "ERR")
for x in (1, 0xFFFF, (1 << 200) - 3):
	case("phex %s" % sbytes("0" * 37 + format(x, "X")), h(x))


# --- P2 ------------------------------------------------------------------------------------------

if PHASE >= 2:
	sizes = sorted(set(list(range(0, 12)) + [15, 16, 17, 31, 32, 33, 34, 63, 64, 65, 66, 67, 97, 129]))
	for n in sizes:
		for m in R.sample(sizes, 5) + [n]:
			for a, b in [(rand_limbs(n), rand_limbs(m)), (structured(n)[0], structured(m)[0]),
			             (structured(n)[1], rand_limbs(m))]:
				for op in ("mul_s", "mul_k", "mul"):
					case("%s %s %s" % (op, h(a), h(b)), h(a * b))
		for a in structured(n) + [rand_limbs(n)]:
			for op in ("sqr_s", "sqr_k", "sqr"):
				case("%s %s" % (op, h(a)), h(a * a))
	for n in (300, 1001):
		a, b = rand_limbs(n), rand_limbs(n - 7)
		for op in ("mul_s", "mul_k", "mul"):
			case("%s %s %s" % (op, h(a), h(b)), h(a * b))
		for op in ("sqr_s", "sqr_k", "sqr"):
			case("%s %s" % (op, h(a)), h(a * a))


# --- P3 ------------------------------------------------------------------------------------------

def add_back_cases(count):
	# Inputs on which Knuth's Algorithm D needs its add-back step, found by running the algorithm's
	# quotient estimate in Python over structured candidates
	out = []
	tries = 0
	while len(out) < count and tries < 200000:
		tries += 1
		n = R.randrange(2, 6)
		v = [R.choice([0, 1, MASK, MASK - 1, 1 << 63, (1 << 63) - 1, R.getrandbits(64)]) for _ in range(n)]
		v[-1] |= 1 << 63
		k = R.randrange(1, 4)
		u = [R.choice([0, 1, MASK, MASK - 1, 1 << 63, (1 << 63) - 1, R.getrandbits(64)]) for _ in range(n + k)]
		V = sum(x << (W * i) for i, x in enumerate(v))
		U = sum(x << (W * i) for i, x in enumerate(u))
		if needs_add_back(U, V):
			s = R.randrange(0, 64)
			out.append((U >> s, V >> s))
	return out


def needs_add_back(U, V):
	# With V's top bit set (no normalization needed), does some step of Algorithm D estimate a
	# quotient digit one too large, so that the subtraction goes below zero?
	n = (V.bit_length() + W - 1) // W
	vt, vs = V >> (W * (n - 1)), (V >> (W * (n - 2))) & MASK
	r = U
	for j in range((U.bit_length() + W - 1) // W + 1 - n - 1, -1, -1):
		window = r >> (W * j)
		qh, rh = divmod(window >> (W * (n - 1)), vt)
		u2 = (window >> (W * (n - 2))) & MASK
		while qh > MASK or qh * vs > ((rh << W) | u2):
			qh -= 1
			rh += vt
			if rh > MASK:
				break
		if qh * V > window:
			return True
		r -= (qh * V) << (W * j)
	return False


if PHASE >= 3:
	moduli = [1, 2, 3, MASK, MASK + 1, mersenne(61), mersenne(127), 10 ** 19, ones(2), 1 << 127, (1 << 127) + 1]
	moduli += [rand_limbs(R.randrange(1, 12)) for _ in range(30)]
	for m in moduli:
		for x in [0, 1, m - 1, m, m + 1, 2 * m, m * m, m * m - 1, rand_limbs(R.randrange(0, 30)),
		          rand_limbs(R.randrange(0, 30)) * m]:
			case("mod %s %s" % (h(x), h(m)), h(x % m))
		for b, e in [(0, 0), (0, 5), (2, 0), (3, 1), (R.randrange(0, 1 << 200), R.randrange(0, 1 << 100)),
		             (m + 2, R.randrange(1, 1000)), (rand_limbs(4), rand_limbs(2))]:
			case("pow %s %s %s" % (h(b), h(e), h(m)), h(pow(b, e, m)))
	for x, m in add_back_cases(12):
		case("mod %s %s" % (h(x), h(m)), h(x % m))
	for x in conv:
		d = str(x)
		case("dec %s" % h(x), d)
		case("pdec %s" % sbytes(d), h(x))
		case("pdec %s" % sbytes("000" + d), h(x))
	for bad in ["-1", "+1", " 1", "1 ", "1_000", "1,000", "0x1F", "1.0", "1e3", "\uff11", "12a", "\t7", "1\n", "A", "\u0661"]:
		case("pdec %s" % sbytes(bad), "ERR")
	case("pdec -", "ERR")
	for bad in long_bad("0123456789", 19):
		case("pdec %s" % sbytes(bad), "ERR")
	for x in (1, 10 ** 19, 10 ** 60 + 7):
		case("pdec %s" % sbytes("0" * 41 + str(x)), h(x))
	for x, m in [(R.getrandbits(R.randrange(1, 3000)), rand_limbs(R.randrange(1, 20))) for _ in range(60)] + add_back_cases(12):
		case("div %s %s" % (h(x), h(m)), "%s %s" % (h(x // m), h(x % m)))
	case("div 5 0", "ERR")
	case("mod 5 0", "ERR")
	case("mod 0 0", "ERR")
	case("pow 2 3 0", "ERR")
	for p in (31, 61, 89, 127):
		m = mersenne(p)
		case("pow 3 %s %s" % (h(m - 1), h(m)), h(pow(3, m - 1, m)))


# --- P4 ------------------------------------------------------------------------------------------

def ll(p):
	m, s = mersenne(p), 4
	for _ in range(p - 2):
		s = (s * s - 2) % m
	return s


if PHASE >= 4:
	odd = [3, 5, 7, 9, 11, 15, 21, 23, 25, 29, 33, 35, 37, 41, 43, 45, 49]
	odd += sorted(R.sample([p for p in range(51, 1600, 2)], 20))
	for p in odd:
		case("ll %d" % p, h(ll(p)))
		case("prp %d" % p, h(pow(3, mersenne(p) - 1, mersenne(p))))
	for p in (0, 1, 2, 4, 6, 10, 64, 100, 1000):
		case("ll %d" % p, "ERR")
		case("prp %d" % p, "ERR")


text = "".join(line + "\n" for line, _ in cases)
run = subprocess.run([BINARY], input=text, capture_output=True, text=True)
if run.returncode != 0:
	print("probe: the probe exited %d: %s" % (run.returncode, run.stderr[-500:]), file=sys.stderr)
	sys.exit(1)
got = run.stdout.split("\n")[:-1]
if len(got) != len(cases):
	print("probe: %d answers for %d cases" % (len(got), len(cases)), file=sys.stderr)
	sys.exit(1)
bad = [(line, want, g) for (line, want), g in zip(cases, got) if g != want]
for line, want, g in bad[:5]:
	print("probe: %s\n   want %s\n   got  %s" % (line[:200], want[:200], g[:200]), file=sys.stderr)
if bad:
	print("probe: %d of %d cases disagree with Python" % (len(bad), len(cases)), file=sys.stderr)
	sys.exit(1)
print("probe: %d cases agree" % len(cases))
