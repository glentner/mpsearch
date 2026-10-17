// Gate mutant interposer: a cycle record, never part of mps.
//
// The gates copy the tree, rename the *definitions* (only) of the functions below in src/mp.cpp to
// gate_real_NAME, and build this file as one more module. Every call, from the checks or from
// mp.cpp itself, reaches the wrappers here. With GATE_MUTANT unset they pass everything through.
// With GATE_MUTANT=<id> one wrapper computes its result as a BigInt with that one bug would, using
// only the public interface of src/mp.hpp, and appends <id> to GATE_MUTANT_LOG the first time that
// result differs from the real one (so "reached" means "changed an answer"). gate_phase.h, written
// by the gate, says which phase's functions exist. Each defect stands for a class of real bug;
// PLAN § 6 lists the classes.


// Standard libs
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Internal libs
#include "gate_phase.h"
#include "mp.hpp"
#if GATE_PHASE >= 4
#include "reference.hpp"
#endif


using u64 = std::uint64_t;
using Limbs = std::vector<u64>;

static const u64 MASK64 = ~u64(0);


int gate_real_compare(const BigInt& a, const BigInt& b);
BigInt gate_real_add(const BigInt& a, const BigInt& b);
BigInt gate_real_subtract(const BigInt& a, const BigInt& b);
BigInt gate_real_shift_left(const BigInt& a, std::size_t bits);
BigInt gate_real_shift_right(const BigInt& a, std::size_t bits);
BigInt gate_real_reduce_mersenne(const BigInt& x, long unsigned p);
std::uint64_t gate_real_res64(const BigInt& x, long unsigned p);
std::string gate_real_hex64(std::uint64_t value);
std::string gate_real_to_hex(const BigInt& x);
BigInt gate_real_from_hex(const std::string& text);
#if GATE_PHASE >= 2
BigInt gate_real_multiply_schoolbook(const BigInt& a, const BigInt& b);
BigInt gate_real_multiply_karatsuba(const BigInt& a, const BigInt& b, std::size_t cutoff);
BigInt gate_real_multiply(const BigInt& a, const BigInt& b);
BigInt gate_real_square_schoolbook(const BigInt& a);
BigInt gate_real_square_karatsuba(const BigInt& a, std::size_t cutoff);
BigInt gate_real_square(const BigInt& a);
#endif
#if GATE_PHASE >= 3
Division gate_real_divide(const BigInt& x, const BigInt& m);
BigInt gate_real_reduce(const BigInt& x, const BigInt& m);
std::string gate_real_to_decimal(const BigInt& x);
BigInt gate_real_from_decimal(const std::string& text);
BigInt gate_real_power_mod(const BigInt& base, const BigInt& exponent, const BigInt& m);
#endif
#if GATE_PHASE >= 4
namespace reference {
	BigInt gate_real_lucas_lehmer(long unsigned p);
	BigInt gate_real_prp3(long unsigned p);
}
#endif


static const std::string& mutant() {
	static const std::string m = std::getenv("GATE_MUTANT") ? std::getenv("GATE_MUTANT") : "";
	return m;
}


// Answers changed so far. After CAP changes the mutant stands down: the check it targets has seen
// enough, and a bug that compounds (a fold never repeated, inside Lucas-Lehmer) cannot run away.
static long changes = 0;
static const long CAP = 2000;


static bool is(const char* id) {
	return changes < CAP and mutant() == id;
}


// Record that the mutant changed an answer
static void applied() {
	if (changes++ > 0)
		return;
	const char* path = std::getenv("GATE_MUTANT_LOG");
	if (not path)
		return;
	if (FILE* f = std::fopen(path, "a")) {
		std::fprintf(f, "%s\n", mutant().c_str());
		std::fclose(f);
	}
}


// --- the trace ------------------------------------------------------------------------------------
//
// With GATE_TRACE=<file>, every wrapped call records its entry point and a fingerprint of its
// arguments, and at exit the distinct records are written to <file>, one per line, for vectors.py's
// coverage check: every vector record must reach the entry points PLAN § 2 names. A fingerprint is
// bits.low.high.xor of a value's limbs (hexadecimal; 0.0.0.0 for zero), length.FNV-1a-64 of a
// string, or a decimal number. gate_log_start and gate_log_stop record the nested calls of one call,
// with their depths, for verify/trace.cpp.

static std::string fp(const BigInt& x) {
	const Limbs& l = x.limbs();
	u64 low = l.empty() ? 0 : l.front(), high = l.empty() ? 0 : l.back(), mix = 0;
	for (u64 v: l)
		mix ^= v;
	char buf[96];
	std::snprintf(buf, sizeof buf, "%zx.%llx.%llx.%llx", x.bit_length(), (unsigned long long)low,
	              (unsigned long long)high, (unsigned long long)mix);
	return buf;
}


static std::string fp(const std::string& s) {
	u64 h = 0xcbf29ce484222325ull;
	for (unsigned char c: s)
		h = (h ^ c) * 0x100000001b3ull;
	char buf[64];
	std::snprintf(buf, sizeof buf, "s%zx.%llx", s.size(), (unsigned long long)h);
	return buf;
}


static std::string fp(unsigned long long n) {
	return std::to_string(n);
}


struct Trace {
	std::set<std::string> calls;
	std::vector<std::pair<int, std::string>> log;
	int depth = 0;
	bool logging = false;
	const char* path = std::getenv("GATE_TRACE");

	~Trace() {
		if (not path)
			return;
		if (FILE* f = std::fopen(path, "w")) {
			for (const std::string& c: calls)
				std::fprintf(f, "%s\n", c.c_str());
			std::fclose(f);
		}
	}
};


static Trace& trace() {
	static Trace t;
	return t;
}


class Enter {
public:
	Enter(const char* name, const std::string& args) {
		Trace& t = trace();
		if (t.path)
			t.calls.insert(std::string(name) + " " + args);
		if (t.logging)
			t.log.push_back({t.depth, name});
		t.depth++;
	}

	~Enter() {
		trace().depth--;
	}
};


static bool tracing() {
	return trace().path or trace().logging;
}


#define ENTER(name, args) Enter enter_(name, tracing() ? (args) : std::string())


void gate_log_start() {
	trace().log.clear();
	trace().logging = true;
}


std::vector<std::pair<int, std::string>> gate_log_stop() {
	trace().logging = false;
	return trace().log;
}


// The mutant's value, logged when it differs from the real one
static BigInt differs(const BigInt& mutated, const BigInt& real) {
	if (gate_real_compare(mutated, real) != 0)
		applied();
	return mutated;
}


static BigInt limbs_of(const Limbs& x) {
	return BigInt::from_limbs(x);
}


static BigInt low_limbs(const BigInt& a, std::size_t n) {
	const Limbs& x = a.limbs();
	return limbs_of(Limbs(x.begin(), x.begin() + std::min(n, x.size())));
}


static BigInt or_of(const BigInt& a, const BigInt& b) {
	Limbs x = a.limbs(), y = b.limbs();
	x.resize(std::max(x.size(), y.size()), 0);
	for (std::size_t i = 0; i < y.size(); i++)
		x[i] |= y[i];
	return limbs_of(x);
}


// --- P1: comparison, sums, differences, shifts, folding, Res64, conversions -----------------------

int compare(const BigInt& a, const BigInt& b) {
	ENTER("compare", fp(a) + " " + fp(b));
	int real = gate_real_compare(a, b);
	if (is("cmp-top")) {
		// The limb loop stops after the top limb: lengths, then the top limb only
		const Limbs& x = a.limbs();
		const Limbs& y = b.limbs();
		int m = x.size() != y.size() ? (x.size() < y.size() ? -1 : 1)
		      : x.empty() or x.back() == y.back() ? 0 : (x.back() < y.back() ? -1 : 1);
		if (m != real)
			applied();
		return m;
	}
	return real;
}


BigInt add(const BigInt& a, const BigInt& b) {
	ENTER("add", fp(a) + " " + fp(b));
	BigInt real = gate_real_add(a, b);
	const Limbs& x = a.limbs();
	const Limbs& y = b.limbs();
	std::size_t n = std::max(x.size(), y.size());
	if (is("add-carry-out")) {
		// The carry out of the top limb is never stored
		return differs(low_limbs(real, n), real);
	}
	if (is("add-carry-in")) {
		// s = x + y + c; carry = s < x: wrong when y is all ones and a carry comes in
		Limbs out(n + 1, 0);
		u64 c = 0;
		for (std::size_t i = 0; i < n; i++) {
			u64 xi = i < x.size() ? x[i] : 0, yi = i < y.size() ? y[i] : 0;
			u64 s = xi + yi + c;
			c = s < xi;
			out[i] = s;
		}
		out[n] = c;
		return differs(limbs_of(out), real);
	}
	return real;
}


BigInt subtract(const BigInt& a, const BigInt& b) {
	ENTER("subtract", fp(a) + " " + fp(b));
	if (is("sub-below") and gate_real_compare(a, b) < 0) {
		// No check for a negative difference: the magnitude comes back
		applied();
		return gate_real_subtract(b, a);
	}
	BigInt real = gate_real_subtract(a, b);
	const Limbs& x = a.limbs();
	const Limbs& y = b.limbs();
	if (is("sub-borrow-in")) {
		// d = x - y - b; borrow = d > x: wrong when y is all ones and a borrow comes in
		Limbs out(x.size(), 0);
		u64 br = 0;
		for (std::size_t i = 0; i < x.size(); i++) {
			u64 yi = i < y.size() ? y[i] : 0;
			u64 d = x[i] - yi - br;
			br = d > x[i];
			out[i] = d;
		}
		return differs(limbs_of(out), real);
	}
	if (is("sub-short")) {
		// The borrow stops at the end of the shorter operand
		Limbs out(x.size(), 0);
		u64 br = 0;
		for (std::size_t i = 0; i < x.size(); i++) {
			if (i < y.size()) {
				u64 d = x[i] - y[i];
				u64 b1 = x[i] < y[i];
				u64 e = d - br;
				u64 b2 = d < br;
				out[i] = e;
				br = b1 + b2;
			} else {
				out[i] = x[i];
			}
		}
		return differs(limbs_of(out), real);
	}
	return real;
}


BigInt shift_left(const BigInt& a, std::size_t bits) {
	ENTER("shift_left", fp(a) + " " + fp(bits));
	BigInt real = gate_real_shift_left(a, bits);
	if (is("shl-word") and bits % 64 == 0) {
		// x >> (64 - r) with r = 0: the hardware masks the count, so each limb is ORed one limb up
		return differs(or_of(real, gate_real_shift_left(a, bits + 64)), real);
	}
	return real;
}


BigInt shift_right(const BigInt& a, std::size_t bits) {
	ENTER("shift_right", fp(a) + " " + fp(bits));
	BigInt real = gate_real_shift_right(a, bits);
	if (is("shr-word") and bits % 64 == 0) {
		// x << (64 - r) with r = 0: each limb is ORed one limb down
		return differs(or_of(real, gate_real_shift_right(a, bits + 64)), real);
	}
	if (is("shr-top") and not a.is_zero()) {
		// The limb loop stops one short: the top limb's bits never come down
		return differs(gate_real_shift_right(low_limbs(a, a.limbs().size() - 1), bits), real);
	}
	return real;
}


static BigInt fold_bits(const BigInt& x, long unsigned p) {
	// x mod 2^p, through the public interface
	return gate_real_subtract(x, gate_real_shift_left(gate_real_shift_right(x, p), p));
}


BigInt reduce_mersenne(const BigInt& x, long unsigned p) {
	ENTER("reduce_mersenne", fp(x) + " " + fp(p));
	BigInt real = gate_real_reduce_mersenne(x, p);
	if (p == 0)
		return real;
	BigInt m = gate_real_subtract(gate_real_shift_left(BigInt(1), p), BigInt(1));
	if (is("fold-once")) {
		// One fold, never repeated
		BigInt r = x.bit_length() > p ? gate_real_add(fold_bits(x, p), gate_real_shift_right(x, p)) : x;
		if (gate_real_compare(r, m) == 0)
			r = BigInt();
		return differs(r, real);
	}
	if (is("fold-carry")) {
		// The fold's sum kept to p bits: its carry, worth 1, is lost
		BigInt r = x;
		while (r.bit_length() > p)
			r = fold_bits(gate_real_add(fold_bits(r, p), gate_real_shift_right(r, p)), p);
		if (gate_real_compare(r, m) == 0)
			r = BigInt();
		return differs(r, real);
	}
	if (is("fold-mp")) {
		// Folded below 2^p, but M_p itself is left as it is
		BigInt r = x;
		while (r.bit_length() > p)
			r = gate_real_add(fold_bits(r, p), gate_real_shift_right(r, p));
		return differs(r, real);
	}
	return real;
}


std::string hex64(std::uint64_t value) {
	ENTER("hex64", fp(value));
	std::string real = gate_real_hex64(value);
	if (is("hex64-unpadded")) {
		// The residue printed without its leading zeros
		std::size_t n = real.find_first_not_of('0');
		std::string t = n == std::string::npos ? "0" : real.substr(n);
		if (t != real)
			applied();
		return t;
	}
	return real;
}


std::uint64_t res64(const BigInt& x, long unsigned p) {
	ENTER("res64", fp(x) + " " + fp(p));
	std::uint64_t real = gate_real_res64(x, p);
	std::uint64_t m = real;
	if (is("res64-raw"))
		// The low limb of x itself, never reduced
		m = x.is_zero() ? 0 : x.limbs()[0];
	if (is("res64-mp") and p > 0) {
		// Reduced into [0, M_p]: x = M_p gives its own low limb
		BigInt r = x;
		while (r.bit_length() > p)
			r = gate_real_add(fold_bits(r, p), gate_real_shift_right(r, p));
		m = r.is_zero() ? 0 : r.limbs()[0];
	}
	if (m != real)
		applied();
	return m;
}


static std::string say(const std::string& mutated, const std::string& real) {
	if (mutated != real)
		applied();
	return mutated;
}


std::string to_hex(const BigInt& x) {
	ENTER("to_hex", fp(x));
	std::string real = gate_real_to_hex(x);
	if (is("hex-lower")) {
		std::string t = real;
		for (char& c: t)
			if (c >= 'A' and c <= 'F')
				c = char(c - 'A' + 'a');
		return say(t, real);
	}
	if (is("hex-lead") and not x.is_zero()) {
		// Every limb printed in full, the top one with its leading zeros
		std::string t;
		for (std::size_t i = x.limbs().size(); i-- > 0;)
			t += hex64(x.limbs()[i]);
		return say(t, real);
	}
	return real;
}


// The longest prefix made of digits of the base
static std::string prefix_in(const std::string& text, const std::string& digits) {
	std::size_t n = text.find_first_not_of(digits);
	return n == std::string::npos ? text : text.substr(0, n);
}


static const std::string DECIMAL_DIGITS = "0123456789";
static const std::string HEX_DIGITS = "0123456789ABCDEFabcdef";


// The text with every character beyond its first `chunk` that is not a digit of the base made 0,
// when there are such characters and none earlier: a parser that validates only its first chunk
static bool junk_past(const std::string& text, const std::string& digits, std::size_t chunk, std::string& cleaned) {
	std::size_t first = text.find_first_not_of(digits);
	if (first == std::string::npos or first < chunk)
		return false;
	cleaned = text;
	for (char& c: cleaned)
		if (digits.find(c) == std::string::npos)
			c = '0';
	return true;
}


BigInt from_hex(const std::string& text) {
	ENTER("from_hex", fp(text));
	std::string cleaned;
	if (is("parse-tail") and junk_past(text, HEX_DIGITS, 16, cleaned)) {
		applied();
		return gate_real_from_hex(cleaned);
	}
	if (is("parse-prefix") and not text.empty()) {
		std::string head = prefix_in(text, HEX_DIGITS);
		if (head != text) {
			applied();
			return head.empty() ? BigInt() : gate_real_from_hex(head);
		}
	}
	if (is("parse-empty") and text.empty()) {
		applied();
		return BigInt();
	}
	return gate_real_from_hex(text);
}


// --- P2: products and squares --------------------------------------------------------------------

#if GATE_PHASE >= 2

BigInt multiply_schoolbook(const BigInt& a, const BigInt& b) {
	ENTER("multiply_schoolbook", fp(a) + " " + fp(b));
	BigInt real = gate_real_multiply_schoolbook(a, b);
	if (is("mul-row-carry") and not a.is_zero() and not b.is_zero()) {
		// Each row's last carry is never stored
		const Limbs& x = a.limbs();
		const Limbs& y = b.limbs();
		Limbs out(x.size() + y.size(), 0);
		for (std::size_t i = 0; i < x.size(); i++) {
			unsigned __int128 carry = 0;
			for (std::size_t j = 0; j < y.size(); j++) {
				unsigned __int128 t = (unsigned __int128)x[i] * y[j] + out[i + j] + carry;
				out[i + j] = u64(t);
				carry = t >> 64;
			}
		}
		return differs(limbs_of(out), real);
	}
	return real;
}


// One Karatsuba split with the low halves of m limbs, the middle term placed at shift_mid limbs and
// the top at 2 shift_mid, and the half-sums kept to keep limbs when keep > 0
static BigInt karatsuba_with(const BigInt& a, const BigInt& b, std::size_t m, std::size_t shift_mid, std::size_t keep) {
	BigInt a0 = low_limbs(a, m), a1 = gate_real_shift_right(a, 64 * m);
	BigInt b0 = low_limbs(b, m), b1 = gate_real_shift_right(b, 64 * m);
	BigInt z0 = gate_real_multiply(a0, b0), z2 = gate_real_multiply(a1, b1);
	BigInt sa = gate_real_add(a0, a1), sb = gate_real_add(b0, b1);
	if (keep > 0) {
		sa = low_limbs(sa, keep);
		sb = low_limbs(sb, keep);
	}
	BigInt mid = gate_real_multiply(sa, sb);
	BigInt z1 = gate_real_compare(mid, gate_real_add(z0, z2)) >= 0 ? gate_real_subtract(mid, gate_real_add(z0, z2)) : BigInt();
	return gate_real_add(gate_real_add(z0, gate_real_shift_left(z1, 64 * shift_mid)), gate_real_shift_left(z2, 128 * shift_mid));
}


BigInt multiply_karatsuba(const BigInt& a, const BigInt& b, std::size_t cutoff) {
	ENTER("multiply_karatsuba", fp(a) + " " + fp(b) + " " + fp(cutoff));
	BigInt real = gate_real_multiply_karatsuba(a, b, cutoff);
	std::size_t n = std::max(a.limbs().size(), b.limbs().size());
	if (std::min(a.limbs().size(), b.limbs().size()) < std::max<std::size_t>(cutoff, 4))
		return real;
	if (is("kara-odd") and n % 2 == 1)
		// Split at floor(n/2) limbs, recombined as if at ceil(n/2)
		return differs(karatsuba_with(a, b, n / 2, n - n / 2, 0), real);
	if (is("kara-sum-carry")) {
		// The half-sums kept to the halves' width: their carry limb is lost
		std::size_t m = n / 2;
		return differs(karatsuba_with(a, b, m, m, n - m), real);
	}
	if (is("kara-own-split")) {
		// Each operand split at its own middle, recombined at the larger one's
		std::size_t ma = std::max<std::size_t>(a.limbs().size() / 2, 1), mb = std::max<std::size_t>(b.limbs().size() / 2, 1);
		if (ma == mb)
			return real;
		BigInt a0 = low_limbs(a, ma), a1 = gate_real_shift_right(a, 64 * ma);
		BigInt b0 = low_limbs(b, mb), b1 = gate_real_shift_right(b, 64 * mb);
		std::size_t m = std::max(ma, mb);
		BigInt z0 = gate_real_multiply(a0, b0), z2 = gate_real_multiply(a1, b1);
		BigInt mid = gate_real_multiply(gate_real_add(a0, a1), gate_real_add(b0, b1));
		BigInt s = gate_real_add(z0, z2);
		BigInt z1 = gate_real_compare(mid, s) >= 0 ? gate_real_subtract(mid, s) : BigInt();
		return differs(gate_real_add(gate_real_add(z0, gate_real_shift_left(z1, 64 * m)), gate_real_shift_left(z2, 128 * m)), real);
	}
	return real;
}


BigInt multiply(const BigInt& a, const BigInt& b) {
	ENTER("multiply", fp(a) + " " + fp(b));
	return gate_real_multiply(a, b);
}


BigInt square_schoolbook(const BigInt& a) {
	ENTER("square_schoolbook", fp(a));
	BigInt real = gate_real_square_schoolbook(a);
	if (is("sqr-double") and not a.is_zero()) {
		// The cross products doubled limb by limb: the bit shifted out of each limb is lost
		const Limbs& x = a.limbs();
		Limbs diag(2 * x.size(), 0);
		for (std::size_t i = 0; i < x.size(); i++) {
			unsigned __int128 t = (unsigned __int128)x[i] * x[i];
			diag[2 * i] = u64(t);
			diag[2 * i + 1] = u64(t >> 64);
		}
		BigInt d = limbs_of(diag);
		Limbs cross = gate_real_shift_right(gate_real_subtract(real, d), 1).limbs();
		for (u64& c: cross)
			c <<= 1;
		return differs(gate_real_add(limbs_of(cross), d), real);
	}
	return real;
}


BigInt square_karatsuba(const BigInt& a, std::size_t cutoff) {
	ENTER("square_karatsuba", fp(a) + " " + fp(cutoff));
	BigInt real = gate_real_square_karatsuba(a, cutoff);
	std::size_t n = a.limbs().size();
	if (n < std::max<std::size_t>(cutoff, 4))
		return real;
	if (is("kara-sqr-odd") and n % 2 == 1)
		return differs(karatsuba_with(a, a, n / 2, n - n / 2, 0), real);
	if (is("kara-sqr-sum-carry"))
		return differs(karatsuba_with(a, a, n / 2, n / 2, n - n / 2), real);
	return real;
}


BigInt square(const BigInt& a) {
	ENTER("square", fp(a));
	return gate_real_square(a);
}

#endif


// --- P3: general reduction and powers -----------------------------------------------------------

#if GATE_PHASE >= 3

// Long division as MCA Algorithm 1.6 or Knuth's 4.3.1 lay it out, with one defect: `refined` uses
// Knuth's two-limb estimate and leaves out the add-back step; otherwise the one-limb estimate is
// corrected at most once, where it may need two corrections
static Division division_with(const BigInt& x, const BigInt& m, bool refined) {
	if (gate_real_compare(x, m) < 0 or m.limbs().size() < 2)
		return gate_real_divide(x, m);
	int s = __builtin_clzll(m.limbs().back());
	Limbs v = gate_real_shift_left(m, s).limbs();
	Limbs u = gate_real_shift_left(x, s).limbs();
	u.resize(x.limbs().size() + 1, 0);
	std::size_t n = v.size();
	Limbs q(u.size() - n, 0);
	BigInt vb = limbs_of(v);
	for (std::size_t j = u.size() - n; j-- > 0;) {
		unsigned __int128 num = ((unsigned __int128)u[j + n] << 64) | u[j + n - 1];
		unsigned __int128 qhat = num / v[n - 1], rhat = num % v[n - 1];
		if (refined) {
			while ((qhat >> 64) or qhat * v[n - 2] > ((rhat << 64) | u[j + n - 2])) {
				qhat--;
				rhat += v[n - 1];
				if (rhat >> 64)
					break;
			}
		} else if (qhat >> 64) {
			qhat = MASK64;
		}
		// The window u[j .. j+n] as a number, and qhat times the divisor
		BigInt window = limbs_of(Limbs(u.begin() + j, u.begin() + j + n + 1));
		BigInt t = gate_real_multiply(vb, BigInt(u64(qhat)));
		if (not refined and gate_real_compare(t, window) > 0) {
			qhat--;
			t = gate_real_subtract(t, vb);
		}
		// The subtraction, in n + 1 limbs: a borrow out of the top is lost, as in limb code
		BigInt diff = gate_real_compare(window, t) >= 0 ? gate_real_subtract(window, t)
		            : gate_real_subtract(gate_real_add(window, gate_real_shift_left(BigInt(1), 64 * (n + 1))), t);
		Limbs d = diff.limbs();
		d.resize(n + 1, 0);
		for (std::size_t i = 0; i <= n; i++)
			u[j + i] = d[i];
		q[j] = u64(qhat);
	}
	u.resize(n);
	return {limbs_of(q), gate_real_shift_right(limbs_of(u), s)};
}


Division divide(const BigInt& x, const BigInt& m) {
	ENTER("divide", fp(x) + " " + fp(m));
	if (is("mod-zero") and m.is_zero()) {
		applied();
		return {BigInt(), x};
	}
	Division real = gate_real_divide(x, m);
	if (is("div-add-back") or is("div-one-correction")) {
		Division d = division_with(x, m, is("div-add-back"));
		if (gate_real_compare(d.quotient, real.quotient) != 0 or gate_real_compare(d.remainder, real.remainder) != 0)
			applied();
		return d;
	}
	return real;
}


BigInt reduce(const BigInt& x, const BigInt& m) {
	ENTER("reduce", fp(x) + " " + fp(m));
	if (is("mod-zero") and m.is_zero()) {
		applied();
		return x;
	}
	BigInt real = gate_real_reduce(x, m);
	if (is("mod-to-m") and real.is_zero() and not x.is_zero())
		// A multiple of m left as m: reduced into [0, m], not [0, m)
		return differs(m, real);
	return real;
}


BigInt power_mod(const BigInt& base, const BigInt& exponent, const BigInt& m) {
	ENTER("power_mod", fp(base) + " " + fp(exponent) + " " + fp(m));
	if (is("mod-zero") and m.is_zero()) {
		applied();
		return BigInt();
	}
	BigInt real = gate_real_power_mod(base, exponent, m);
	if (is("pow-one") and m == BigInt(1))
		// The result starts at 1 and is never reduced: modulo 1 it stays 1
		return differs(exponent.is_zero() ? BigInt(1) : real, real);
	if (is("pow-start")) {
		// The result starts at the base and the top bit is skipped: exponent 0 gives the base
		if (exponent.is_zero())
			return differs(gate_real_reduce(base, m), real);
		return real;
	}
	if (is("pow-reversed")) {
		// The exponent's bits read from the low end by the left-to-right ladder
		BigInt r = gate_real_reduce(BigInt(1), m), b = gate_real_reduce(base, m);
		std::size_t nb = exponent.bit_length();
		for (std::size_t i = 0; i < nb; i++) {
			r = gate_real_reduce(gate_real_square(r), m);
			if ((exponent.limbs()[i / 64] >> (i % 64)) & 1)
				r = gate_real_reduce(gate_real_multiply(r, b), m);
		}
		return differs(r, real);
	}
	return real;
}

static std::string decimal_unpadded(const BigInt& x) {
	// Base 10^19 digits, each printed without its leading zeros
	static const u64 TEN_19 = 10'000'000'000'000'000'000ull;
	if (x.is_zero())
		return "0";
	Limbs u = x.limbs();
	std::vector<u64> chunks;
	while (not u.empty()) {
		unsigned __int128 r = 0;
		for (std::size_t i = u.size(); i-- > 0;) {
			unsigned __int128 cur = (r << 64) | u[i];
			u[i] = u64(cur / TEN_19);
			r = cur % TEN_19;
		}
		chunks.push_back(u64(r));
		while (not u.empty() and u.back() == 0)
			u.pop_back();
	}
	std::string text;
	for (std::size_t i = chunks.size(); i-- > 0;)
		text += std::to_string(chunks[i]);
	return text;
}


std::string to_decimal(const BigInt& x) {
	ENTER("to_decimal", fp(x));
	std::string real = gate_real_to_decimal(x);
	if (is("dec-pad"))
		return say(decimal_unpadded(x), real);
	return real;
}


BigInt from_decimal(const std::string& text) {
	ENTER("from_decimal", fp(text));
	std::string cleaned;
	if (is("parse-tail-dec") and junk_past(text, DECIMAL_DIGITS, 19, cleaned)) {
		applied();
		return gate_real_from_decimal(cleaned);
	}
	if (is("parse-prefix-dec") and not text.empty()) {
		std::string head = prefix_in(text, DECIMAL_DIGITS);
		if (head != text) {
			// Parsing stops at the first character that is not a digit, and keeps what it read
			applied();
			return head.empty() ? BigInt() : gate_real_from_decimal(head);
		}
	}
	if (is("parse-empty-dec") and text.empty()) {
		applied();
		return BigInt();
	}
	return gate_real_from_decimal(text);
}


#endif


// --- P4: the reference tests ---------------------------------------------------------------------

#if GATE_PHASE >= 4

static BigInt ll_step(const BigInt& s, long unsigned p) {
	BigInt m = gate_real_subtract(gate_real_shift_left(BigInt(1), p), BigInt(1));
	BigInt r = gate_real_reduce_mersenne(gate_real_square(s), p);
	if (gate_real_compare(r, BigInt(2)) < 0)
		r = gate_real_add(r, m);
	return gate_real_subtract(r, BigInt(2));
}


BigInt reference::lucas_lehmer(long unsigned p) {
	ENTER("lucas_lehmer", fp(p));
	if (is("domain") and p == 2) {
		// The theorem's hypothesis p odd is not checked
		applied();
		return BigInt();
	}
	BigInt real = reference::gate_real_lucas_lehmer(p);
	if (is("ll-extra"))
		// One iteration too many: s_(p-1)
		return differs(ll_step(real, p), real);
	if (is("ll-seed")) {
		// Started from s_0 = 10, which also proves the primes, so only the composites show it
		BigInt s(10);
		for (long unsigned i = 0; i + 2 < p; i++)
			s = ll_step(s, p);
		return differs(s, real);
	}
	return real;
}


BigInt reference::prp3(long unsigned p) {
	ENTER("prp3", fp(p));
	if (is("domain") and p == 2) {
		applied();
		return BigInt(1);
	}
	BigInt real = reference::gate_real_prp3(p);
	if (is("prp-x"))
		// x_p = 3^(2^p), not the type-1 residue: 9 times it
		return differs(gate_real_reduce_mersenne(gate_real_multiply(real, BigInt(9)), p), real);
	if (is("prp-base2")) {
		// Base 2, which says nothing for M_p: 2^(M_p - 1) = 1 for every p
		return differs(BigInt(1), real);
	}
	return real;
}

#endif
