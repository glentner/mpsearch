# Claim library and evaluator for the math-notes-and-references gates (a cycle record, not a
# standing gate). Python 3.8+, standard library only; it never links into bin/mps (AGENTS.md
# § Invariants 5). notes.py imports it.
#
# A note's numeric claim carries its check as an HTML comment:
#
#     <!-- check: EXPR -->visible text<!-- /check -->
#
# EXPR is a restricted Python expression over integers and the functions in FUNCS. It must
# evaluate to True. The integers the visible text shows and the integers EXPR writes are tied:
# every integer token in the text is a literal of EXPR or the value of one of its
# subexpressions, and every integer literal of EXPR appears as a token in the text (PLAN § 2).
#
# Published values the checks compare against carry their source here, beside the value.

import ast
import re
import operator
from math import gcd, isqrt


class ClaimError(Exception):
	pass


# --- Published values ------------------------------------------------------------------------

# The Mersenne prime exponents up to 23209, the first 26 terms of OEIS A000043 ("Mersenne
# exponents: primes p such that 2^p - 1 is prime"), https://oeis.org/A000043, read from the
# OEIS JSON record on 2026-10-12 by the planner. Every check below compares against these terms
# and never extends past the last one.
PUBLISHED_EXPONENTS = (
	2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281, 3217, 4253, 4423,
	9689, 9941, 11213, 19937, 21701, 23209,
)
PUBLISHED_LIMIT = 23209

# The least bounds R7's required checks may use (PLAN § 2); the largest the gate will run.
MIN_LL_BOUND, MAX_LL_BOUND = 2500, 4500
MIN_JACOBI_BOUND, MAX_JACOBI_BOUND = 521, 1000
MIN_FIRST_ZERO_BOUND = 25
MIN_PRP_BOUND = 11213
MIN_FORM_EXPONENTS, MIN_FORM_FACTORS = 100, 1 << 20
MAX_FORM_EXPONENTS, MAX_FORM_FACTORS = 200, 1 << 22


# --- Integer helpers ----------------------------------------------------------------------------

_SMALL_PRIMES = (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41)


def _int(x, name="argument"):
	if isinstance(x, bool) or not isinstance(x, int):
		raise ClaimError("%s must be an integer, got %r" % (name, x))
	return x


def _strong_witness(a, n):
	# True when a proves n composite (the Miller-Rabin test)
	d, s = n - 1, 0
	while d % 2 == 0:
		d //= 2
		s += 1
	x = pow(a, d, n)
	if x in (1, n - 1):
		return False
	for _ in range(s - 1):
		x = x * x % n
		if x == n - 1:
			return False
	return True


def isprime(n):
	# Decided, never guessed. Below 10^12, by trial division. Above, a Miller-Rabin witness proves
	# n composite; otherwise n is proved prime by Lucas's converse of Fermat (n - 1 fully factored
	# and some a of order n - 1), or the call fails and the claim must be cited instead.
	n = _int(n)
	if n < 2:
		return False
	for q in _SMALL_PRIMES:
		if n % q == 0:
			return n == q
	if n < 10 ** 12:
		d = 43
		while d * d <= n:
			if n % d == 0:
				return False
			d += 2
		return True
	if any(_strong_witness(a, n) for a in _SMALL_PRIMES):
		return False
	qs = set(factor(n - 1))
	for a in range(2, 200):
		if pow(a, n - 1, n) == 1 and all(pow(a, (n - 1) // q, n) != 1 for q in qs):
			return True
	raise ClaimError("isprime(%d): no proof found here; cite a published statement" % n)


_prime_cache = {}


def primes(n):
	n = _int(n)
	if n > 1 << 22:
		raise ClaimError("primes(%d): bound above 2^22" % n)
	if n not in _prime_cache:
		if n < 2:
			_prime_cache[n] = ()
		else:
			s = bytearray([1]) * (n + 1)
			s[0:2] = b"\0\0"
			for i in range(2, isqrt(n) + 1):
				if s[i]:
					s[i * i::i] = bytearray(len(s[i * i::i]))
			_prime_cache[n] = tuple(i for i in range(n + 1) if s[i])
	return _prime_cache[n]


def factor(n):
	n = _int(n)
	if n < 1:
		raise ClaimError("factor(%d): need n >= 1" % n)
	out = []
	d = 2
	while d * d <= n and d <= 1000000:
		while n % d == 0:
			out.append(d)
			n //= d
		d += 1 if d == 2 else 2
	if n > 1:
		if not isprime(n):
			raise ClaimError("factor: cofactor %d has no factor up to 10^6 and is composite" % n)
		out.append(n)
	return tuple(out)


def jacobi(a, n):
	a, n = _int(a), _int(n)
	if n <= 0 or n % 2 == 0:
		raise ClaimError("jacobi(%d, %d): n must be odd and positive" % (a, n))
	a %= n
	t = 1
	while a:
		while a % 2 == 0:
			a //= 2
			if n % 8 in (3, 5):
				t = -t
		a, n = n, a
		if a % 4 == 3 and n % 4 == 3:
			t = -t
		a %= n
	return t if n == 1 else 0


def order(a, n):
	# the multiplicative order of a modulo n, for gcd(a, n) = 1
	a, n = _int(a), _int(n)
	if n < 2 or gcd(a, n) != 1:
		raise ClaimError("order(%d, %d): need n >= 2 and gcd(a, n) = 1" % (a, n))
	phi = 1
	for q in set(factor(n)):
		e = factor(n).count(q)
		phi *= (q - 1) * q ** (e - 1)
	m = phi
	for q in set(factor(phi)):
		while m % q == 0 and pow(a, m // q, n) == 1:
			m //= q
	return m


def mersenne(p):
	p = _int(p)
	if p < 1 or p > 100000:
		raise ClaimError("mersenne(%d): exponent out of range" % p)
	return (1 << p) - 1


def res64(n):
	return _int(n) % (1 << 64)


# --- Lucas-Lehmer ------------------------------------------------------------------------------

def _odd_prime_exponent(p, limit):
	p = _int(p)
	if p < 3 or p > limit or not isprime(p):
		raise ClaimError("exponent %d: need an odd prime p, 3 <= p <= %d" % (p, limit))
	return p


def lucas_seq(p, i):
	# s_i mod M_p, with s_0 = 4 and s_{k+1} = s_k^2 - 2
	p, i = _int(p), _int(i)
	if p < 2 or p > 5000 or i < 0 or i > 5000:
		raise ClaimError("lucas_seq(%d, %d): out of range" % (p, i))
	M = (1 << p) - 1
	s = 4 % M
	for _ in range(i):
		s = (s * s - 2) % M
	return s


def ll_residue(p):
	# the final Lucas-Lehmer residue s_{p-2} mod M_p, for an odd prime p
	p = _odd_prime_exponent(p, MAX_LL_BOUND)
	return lucas_seq(p, p - 2)


_ll_cache = {}


def ll_exponents(B):
	# The primes p <= B with M_p prime, found by the Lucas-Lehmer test for odd p and by M_2 = 3
	# for p = 2. Each odd prime p whose residue is nonzero is shown composite a second way, by a
	# failed base-3 Fermat test; the list must equal the published exponents up to B.
	B = _int(B)
	if B < MIN_LL_BOUND or B > MAX_LL_BOUND:
		raise ClaimError("ll_exponents(%d): bound must lie in [%d, %d]" % (B, MIN_LL_BOUND, MAX_LL_BOUND))
	if B in _ll_cache:
		return _ll_cache[B][0]
	found = [2] if isprime(3) else []
	composite = 0
	for p in primes(B):
		if p == 2:
			continue
		M = (1 << p) - 1
		if ll_residue(p) == 0:
			found.append(p)
		else:
			if pow(3, M - 1, M) == 1:
				raise ClaimError("ll_exponents: M_%d has a nonzero residue but passes base-3 Fermat" % p)
			composite += 1
	published = [q for q in PUBLISHED_EXPONENTS if q <= B]
	if found != published:
		raise ClaimError("ll_exponents(%d): found %r, published (OEIS A000043) %r" % (B, found, published))
	_ll_cache[B] = (tuple(found), composite)
	return tuple(found)


def _sweep(exponents):
	# For each p, every iteration 1 <= i <= p - 2: the Jacobi symbol J(s_i - 2 | M_p) unshifted,
	# and as three shifted runs compute it. A shifted run stores r = s * 2^k mod M_p, squares it
	# to s^2 * 2^(2k) and subtracts 2^(2k+1), so k doubles mod p; its symbol is
	# J(r - 2^(k+1) | M_p). Every case where the symbol is not -1 is returned as (p, i), after
	# checking: it is 0; every shifted symbol equals the unshifted one; and the hypothesis
	# excludes it, namely gcd(s_j, M_p) > 1 for some 0 <= j <= i - 2. A case the hypothesis
	# excludes with symbol -1 is an error too, since the derivation says 0 exactly there.
	cases = []
	for p in exponents:
		M = (1 << p) - 1
		shifts = sorted({1, (p - 1) // 2, p - 1})
		s = 4
		r = {k0: (4 << k0) % M for k0 in shifts}
		k = dict((k0, k0) for k0 in shifts)
		history = [4]
		shares = False
		for i in range(1, p - 1):
			s = (s * s - 2) % M
			history.append(s)
			for k0 in shifts:
				r[k0] = (r[k0] * r[k0] - (1 << ((2 * k[k0] + 1) % p))) % M
				k[k0] = (2 * k[k0]) % p
				if r[k0] != (s << k[k0]) % M:
					raise ClaimError("sweep: shifted run (p=%d, k0=%d) diverged at i=%d" % (p, k0, i))
			if i >= 2 and gcd(history[i - 2], M) > 1:
				shares = True
			j = jacobi(s - 2, M)
			for k0 in shifts:
				js = jacobi(r[k0] - (1 << ((k[k0] + 1) % p)), M)
				if js != j:
					raise ClaimError("sweep: p=%d i=%d shift %d gives %d, unshifted %d" % (p, i, k0, js, j))
			if j == -1 and shares:
				raise ClaimError("sweep: p=%d i=%d symbol -1 although an earlier s_j shares a factor" % (p, i))
			if j != -1:
				if j != 0 or not shares:
					raise ClaimError("sweep: p=%d i=%d symbol %d not excluded by the hypothesis" % (p, i, j))
				cases.append((p, i))
	return cases


_sweep_cache = {}


def jacobi_sweep(B):
	# R7's Jacobi check over every odd prime p <= B: the cases (p, i) whose symbol is not -1.
	B = _int(B)
	if B < MIN_JACOBI_BOUND or B > MAX_JACOBI_BOUND:
		raise ClaimError("jacobi_sweep(%d): bound must lie in [%d, %d]" % (B, MIN_JACOBI_BOUND, MAX_JACOBI_BOUND))
	if B not in _sweep_cache:
		_sweep_cache[B] = tuple(_sweep([p for p in primes(B) if p > 2]))
	return _sweep_cache[B]


def jacobi_first_zero(B):
	# The same sweep over every odd p, 3 <= p <= B, prime or not: for each p with a case, the
	# pair (p, first i whose symbol is 0).
	B = _int(B)
	if B < MIN_FIRST_ZERO_BOUND or B > 200:
		raise ClaimError("jacobi_first_zero(%d): bound must lie in [%d, 200]" % (B, MIN_FIRST_ZERO_BOUND))
	first = {}
	for p, i in _sweep(list(range(3, B + 1, 2))):
		first.setdefault(p, i)
	return tuple(sorted(first.items()))


# --- PRP and the Gerbicz check -----------------------------------------------------------------

def prp3(p):
	# 3^(M_p - 1) == 1 mod M_p
	p = _int(p)
	if p < 2 or p > 30000:
		raise ClaimError("prp3(%d): exponent out of range" % p)
	M = (1 << p) - 1
	return pow(3, M - 1, M) == 1


def prp3_known(B):
	# The base-3 Fermat PRP property on every published Mersenne prime exponent p <= B other
	# than 2 (OEIS A000043, above); returns how many were checked.
	B = _int(B)
	if B < MIN_PRP_BOUND or B > PUBLISHED_LIMIT:
		raise ClaimError("prp3_known(%d): bound must lie in [%d, %d]" % (B, MIN_PRP_BOUND, PUBLISHED_LIMIT))
	n = 0
	for p in PUBLISHED_EXPONENTS:
		if p > B:
			break
		if p == 2:
			continue    # M_2 = 3 is the base itself: 3^2 = 0 mod 3, so the test says nothing
		if not prp3(p):
			raise ClaimError("prp3_known: M_%d fails base-3 Fermat" % p)
		n += 1
	return n


def _gerbicz(p, L, blocks, fault):
	# Square x = 3 mod M_p L * blocks times, checking each block of L squarings with the
	# Gerbicz product d: d_0 = x_0, d_{t+1} = d_t * x_{(t+1)L}, and the check
	# d_{t+1} == x_0 * d_t^(2^L). A fault adds 1 to x after the given squaring.
	M = (1 << p) - 1
	x = 3
	d = x
	x0 = x
	count = 0
	for _ in range(blocks):
		d_prev = d
		for _ in range(L):
			x = x * x % M
			count += 1
			if count == fault:
				x = (x + 1) % M
		d = d * x % M
		if d != x0 * pow(d_prev, 1 << L, M) % M:
			return False
	return True


def gerbicz_ok(p, L, blocks):
	p, L, blocks = _int(p), _int(L), _int(blocks)
	if p < 3 or p > 5000 or L < 1 or L > 1000 or blocks < 1 or blocks > 1000:
		raise ClaimError("gerbicz_ok: argument out of range")
	return _gerbicz(p, L, blocks, 0)


def gerbicz_detects(p, L, blocks, k):
	# True when a fault after squaring k (1 <= k <= L * blocks) fails a block check
	p, L, blocks, k = _int(p), _int(L), _int(blocks), _int(k)
	if p < 3 or p > 5000 or L < 1 or L > 1000 or blocks < 1 or blocks > 1000 or not 1 <= k <= L * blocks:
		raise ClaimError("gerbicz_detects: argument out of range")
	return not _gerbicz(p, L, blocks, k)


# --- Factors and trial factoring ---------------------------------------------------------------

def divides_mersenne(q, p):
	q, p = _int(q), _int(p)
	if q < 2 or p < 1:
		raise ClaimError("divides_mersenne: out of range")
	return pow(2, p, q) == 1


def has_factor_form(q, p):
	# q = 2kp + 1 for some k >= 1, and q = +-1 mod 8
	q, p = _int(q), _int(p)
	if q < 3 or p < 3:
		raise ClaimError("has_factor_form: out of range")
	return q % (2 * p) == 1 and q % 8 in (1, 7)


def factor_form(B, Q):
	# Every prime q <= Q that divides M_p, for every odd prime p <= B, found by trying every prime
	# q (not only those of the form), must be 2kp + 1 and +-1 mod 8. Returns how many pairs.
	B, Q = _int(B), _int(Q)
	if not (MIN_FORM_EXPONENTS <= B <= MAX_FORM_EXPONENTS and MIN_FORM_FACTORS <= Q <= MAX_FORM_FACTORS):
		raise ClaimError("factor_form(%d, %d): bounds out of range" % (B, Q))
	exps = [p for p in primes(B) if p > 2]
	n = 0
	for q in primes(Q):
		if q == 2:
			continue
		for p in exps:
			if pow(2, p, q) == 1:
				if not has_factor_form(q, p):
					raise ClaimError("factor_form: q=%d divides M_%d without the form" % (q, p))
				n += 1
	if n == 0:
		raise ClaimError("factor_form: no factor found, the check is vacuous")
	return n


# --- P-1 ---------------------------------------------------------------------------------------

def pm1(p, B1, B2):
	# Pollard's P-1 on M_p with base 3: stage 1 raises x = 3 to E = 2p * prod(r^e), r^e <= B1, and
	# takes g1 = gcd(x - 1, M_p); stage 2 multiplies (x^r - 1) over the primes B1 < r <= B2 and
	# takes g2 = gcd of that product with M_p. Returns (g1, g2).
	p, B1, B2 = _int(p), _int(B1), _int(B2)
	if p < 3 or p > 2000 or B1 < 2 or B1 > 100000 or B2 < B1 or B2 > 1000000:
		raise ClaimError("pm1(%d, %d, %d): out of range" % (p, B1, B2))
	M = (1 << p) - 1
	E = 2 * p
	for r in primes(B1):
		e = r
		while e * r <= B1:
			e *= r
		E *= e
	x = pow(3, E, M)
	g1 = gcd(x - 1, M)
	acc = 1
	for r in primes(B2):
		if r > B1:
			acc = acc * (pow(x, r, M) - 1) % M
	g2 = gcd(acc, M)
	return (g1, g2)


# --- Weighted transforms -----------------------------------------------------------------------

def dwt_widths(p, N):
	# the digit widths ceil(p(j+1)/N) - ceil(pj/N), j = 0 .. N-1, of the irrational-base
	# representation modulo 2^p - 1
	p, N = _int(p), _int(N)
	if p < 2 or N < 1 or N > p or p > 10**9:
		raise ClaimError("dwt_widths: out of range")
	c = lambda j: -(-p * j // N)
	return tuple(c(j + 1) - c(j) for j in range(N))


def dwt_product_ok(p, N, trials):
	# The weighted-convolution identity behind the IBDWT, checked exactly in integers: with
	# digits x_j of widths dwt_widths(p, N) at positions c(j) = ceil(pj/N), the product mod M_p
	# is sum_k z_k 2^c(k), where z_k = sum over i + j = k (mod N) of x_i y_j 2^e and
	# e = c(i) + c(j) - c(k) - (p if i + j >= N else 0) lies in {0, 1}. Deterministic inputs.
	p, N, trials = _int(p), _int(N), _int(trials)
	if p < 2 or N < 1 or N > p or p > 5000 or trials < 1 or trials > 100:
		raise ClaimError("dwt_product_ok: out of range")
	M = (1 << p) - 1
	c = [-(-p * j // N) for j in range(N + 1)]
	seed = 12345
	for _ in range(trials):
		xs, ys = [], []
		for v in (xs, ys):
			for j in range(N):
				seed = (seed * 6364136223846793005 + 1442695040888963407) % (1 << 64)
				v.append(seed % (1 << (c[j + 1] - c[j])))
		X = sum(xs[j] << c[j] for j in range(N)) % M
		Y = sum(ys[j] << c[j] for j in range(N)) % M
		z = [0] * N
		for i in range(N):
			for j in range(N):
				k = (i + j) % N
				e = c[i] + c[j] - c[k] - (p if i + j >= N else 0)
				if e not in (0, 1):
					return False
				z[k] += xs[i] * ys[j] << e
		if sum(z[k] << c[k] for k in range(N)) % M != X * Y % M:
			return False
	return True


FUNCS = {
	"gcd": gcd, "pow": pow, "len": len, "isprime": isprime, "primes": primes, "factor": factor,
	"jacobi": jacobi, "order": order, "mersenne": mersenne, "res64": res64,
	"lucas_seq": lucas_seq, "ll_residue": ll_residue, "ll_exponents": ll_exponents,
	"jacobi_sweep": jacobi_sweep, "jacobi_first_zero": jacobi_first_zero,
	"prp3": prp3, "prp3_known": prp3_known, "gerbicz_ok": gerbicz_ok,
	"gerbicz_detects": gerbicz_detects, "divides_mersenne": divides_mersenne,
	"has_factor_form": has_factor_form, "factor_form": factor_form, "pm1": pm1,
	"dwt_widths": dwt_widths, "dwt_product_ok": dwt_product_ok,
}


# --- The evaluator -----------------------------------------------------------------------------

_BINOPS = {
	ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
	ast.FloorDiv: operator.floordiv, ast.Mod: operator.mod, ast.Pow: None,
}
_CMPOPS = {
	ast.Eq: operator.eq, ast.NotEq: operator.ne, ast.Lt: operator.lt, ast.LtE: operator.le,
	ast.Gt: operator.gt, ast.GtE: operator.ge, ast.In: lambda a, b: a in b,
	ast.NotIn: lambda a, b: a not in b,
}


def _ints(v, out):
	# a scalar result only: the elements of a tuple a function returns are never "shown" by
	# the check; a literal list's elements are literals already
	if isinstance(v, int) and not isinstance(v, bool):
		out.add(v)


class Evaluation:
	def __init__(self, text):
		self.text = text
		self.literals = set()
		self.values = set()
		self.calls = []      # (name, args, result)
		bad = re.search(r"[^0-9A-Za-z_ +\-*/%(),\[\]<>=!\n\t]", text)
		if bad:
			raise ClaimError("character %r not allowed in a check" % bad.group(0))
		try:
			tree = ast.parse(text.strip(), mode="eval")
		except SyntaxError as e:
			raise ClaimError("not an expression: %s" % e.msg)
		self.result = self._eval(tree.body)
		if self.result is not True:
			raise ClaimError("evaluates to %r, not True" % (self.result,))

	def _eval(self, node):
		v = self._node(node)
		_ints(v, self.values)
		return v

	def _node(self, node):
		if isinstance(node, ast.Constant):
			if isinstance(node.value, bool) or not isinstance(node.value, int):
				raise ClaimError("only integer literals are allowed, got %r" % (node.value,))
			self.literals.add(node.value)
			return node.value
		if isinstance(node, (ast.List, ast.Tuple)):
			return tuple(self._eval(e) for e in node.elts)
		if isinstance(node, ast.UnaryOp):
			if isinstance(node.op, ast.USub) and isinstance(node.operand, ast.Constant):
				c = node.operand.value
				if isinstance(c, bool) or not isinstance(c, int):
					raise ClaimError("only integer literals are allowed, got %r" % (c,))
				self.literals.add(-c)    # a negative literal is shown signed
				return -c
			v = self._eval(node.operand)
			if isinstance(node.op, ast.USub):
				return -_int(v)
			if isinstance(node.op, ast.Not):
				return not v
			raise ClaimError("operator not allowed")
		if isinstance(node, ast.BinOp):
			if type(node.op) not in _BINOPS:
				raise ClaimError("operator %s not allowed (write powers as **)" % type(node.op).__name__)
			a, b = _int(self._eval(node.left)), _int(self._eval(node.right))
			if isinstance(node.op, ast.Pow):
				if b < 0 or b > 200000 or a.bit_length() * b > 4000000:
					raise ClaimError("power too large")
				return a ** b
			if isinstance(node.op, (ast.FloorDiv, ast.Mod)) and b == 0:
				raise ClaimError("division by zero")
			return _BINOPS[type(node.op)](a, b)
		if isinstance(node, ast.BoolOp):
			vals = [self._eval(v) for v in node.values]
			if any(not isinstance(v, bool) for v in vals):
				raise ClaimError("and/or need comparisons on both sides")
			return all(vals) if isinstance(node.op, ast.And) else any(vals)
		if isinstance(node, ast.Compare):
			left = self._eval(node.left)
			ok = True
			for op, right in zip(node.ops, node.comparators):
				r = self._eval(right)
				if type(op) not in _CMPOPS:
					raise ClaimError("comparison not allowed")
				ok = ok and _CMPOPS[type(op)](left, r)
				left = r
			return ok
		if isinstance(node, ast.Call):
			if not isinstance(node.func, ast.Name) or node.func.id not in FUNCS or node.keywords:
				raise ClaimError("call not allowed: %s" % ast.dump(node.func))
			args = tuple(self._eval(a) for a in node.args)
			try:
				v = FUNCS[node.func.id](*args)
			except ClaimError:
				raise
			except (TypeError, ValueError, ZeroDivisionError) as e:
				raise ClaimError("%s%r: %s" % (node.func.id, args, e))
			if isinstance(v, list):
				v = tuple(v)
			self.calls.append((node.func.id, args, v))
			return v
		raise ClaimError("syntax not allowed: %s" % type(node).__name__)


# --- Self-test ---------------------------------------------------------------------------------

def self_test():
	# The helpers against independent methods, before any claim is judged.
	ps = primes(20000)
	if [n for n in range(20001) if isprime(n)] != list(ps):
		raise ClaimError("self-test: isprime disagrees with the sieve below 20000")
	for n in (561, 1105, 2047, 3215031751, (1 << 67) - 1, 193707721 * 761838257287 * 3):
		if isprime(n):
			raise ClaimError("self-test: isprime(%d) says prime" % n)
	for q in ps[1:200]:
		for a in range(1, 60):
			if a % q and jacobi(a, q) != (1 if pow(a, (q - 1) // 2, q) == 1 else -1):
				raise ClaimError("self-test: jacobi(%d, %d) disagrees with Euler's criterion" % (a, q))
	for n in range(3, 400, 2):
		for a in range(0, 50):
			want = 1
			for q in factor(n):
				want *= jacobi(a, q)
			if jacobi(a, n) != want:
				raise ClaimError("self-test: jacobi(%d, %d) is not multiplicative" % (a, n))
	if order(7, (1 << 64) - (1 << 32) + 1) != (1 << 64) - (1 << 32):
		raise ClaimError("self-test: order")
	if not isprime(761838257287) or not isprime((1 << 61) - 1) or not isprime((1 << 64) - (1 << 32) + 1):
		raise ClaimError("self-test: isprime on known primes")
	# The zero branch of the Jacobi sweep, on composite exponents: 7 = M_3 divides s_1, so
	# J(s_3 - 2 | M_9) is the first 0.
	if _sweep([9, 15, 25]) [:1] != [(9, 3)] or jacobi_first_zero(27) != ((9, 3), (15, 3), (21, 3), (25, 5), (27, 3)):
		raise ClaimError("self-test: the Jacobi sweep's zero cases")
	if pm1(67, 30, 3000) != (1, 193707721):
		raise ClaimError("self-test: pm1 on M_67")
	if not gerbicz_ok(127, 10, 5) or not gerbicz_detects(127, 10, 5, 23):
		raise ClaimError("self-test: gerbicz")
	if not dwt_product_ok(127, 8, 5) or dwt_widths(31, 4) != (8, 8, 8, 7):
		raise ClaimError("self-test: dwt")
	ev = Evaluation("pm1(67, 30, 3000) == pm1(67, 30, 3000) and jacobi(3, 127) == -1")
	if 193707721 in ev.values or -1 not in ev.literals or 1 in ev.literals:
		raise ClaimError("self-test: values and literals")
	for bad in ("1 == 1  # hidden", "2 ^ 3 == 1", "1.5 == 1.5", "'a' == 'a'", "__import__('os')", "(lambda: 1)() == 1",
	            "2 ** 3", "True", "pow(2, 3) == 9"):
		try:
			Evaluation(bad)
		except ClaimError:
			continue
		raise ClaimError("self-test: evaluator accepted %r" % bad)
	return True
