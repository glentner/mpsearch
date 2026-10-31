// Gate interposer: a cycle record, never part of mps.
//
// The gates copy the tree, rename the *definitions* (only) of the pinned functions below to
// gate_real_NAME in the same namespace (PLAN § 2, "Pinned for the gates"), declare those names at
// the end of a copy of their header, and build this file, as tests/gate_interpose.cpp beside the
// gate's tests/gate_phase.h, into bin/check. The functions of phase 2 on are wrapped from phase 2. Every
// call, from the checks or from the engine itself, then reaches the wrappers here.
//
// GATE_TRACE=<file>: every wrapped operation is counted by (operation, p, N, bit class), and the
// counts are written to <file> when the program exits, one "op p N bit count" line each; the bit
// class of a subtraction is its bit when that is 0, 1 or p - 1, and "other" otherwise.
//
// GATE_MUTANT=<id>: one wrapper computes its result as an engine with that one defect would,
// using only the public interface of src/ibdwt.hpp, src/fft.hpp and src/lengths.hpp and the real
// functions; with GATE_MUTANT unset everything passes through. Each defect stands for a class of
// real bug (PLAN § 6, "Mutants"):
//   weights          every weight but a_0 off by a factor 2^(1/N)
//   twiddle          the root of unity w^1 off by 1e-9 in its real part
//   carry-drop       the carry out of the top word dropped instead of added into word 0
//   unbalanced       digits carried into 0 <= d < 2^s instead of the balanced range
//   subtract-sign    2^bit added instead of subtracted
//   subtract-place   2^(bit + 1) subtracted instead of 2^bit
//   silent-monitor   every round-off distance reported as 0
//   product-square   a product returning the square of its first operand
//   no-table         the length table never consulted
//   no-checksum      the exact checksums always agreeing
//   no-envelope      the outputs' root mean square always reported as 0
//   sabotage         every squaring, product and conversion to digits off by one in digit 0
//   sabotage-top     every residue read back off by 2^(c_(N-1)), the top word's place: wrong only
//                    above the low 64 bits for N > 1, so a check grading Res64 alone misses it


// Standard libs
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <tuple>
#include <vector>

// Internal libs
#include "gate_phase.h"
#include "fft.hpp"
#include "ibdwt.hpp"
#include "lengths.hpp"
#include "mp.hpp"


// The engine's internal pieces, in no header: declared here as the checks declare them
namespace ibdwt {
	std::vector<double> convolve(const Transform& transform, const Residue& x, const Residue& y);
	std::vector<double> convolve_square(const Transform& transform, const Residue& x);
	Residue carry(const Engine& engine, const std::vector<double>& outputs, bool subtract, std::uint64_t bit);
	bool checksums_agree(const Transform& transform, const Residue& x, const Residue& y, const std::vector<double>& outputs);
	double rms(const std::vector<double>& outputs);
}


static std::string mutant() {
	const char* id = std::getenv("GATE_MUTANT");
	return id ? id : "";
}


static bool is(const char* id) {
	static const std::string chosen = mutant();
	return chosen == id;
}


#if GATE_PHASE >= 2
// --- The trace ----------------------------------------------------------------------------------

using Key = std::tuple<std::string, std::uint64_t, std::uint64_t, std::string>;

static std::map<Key, std::uint64_t>& counts() {
	static std::map<Key, std::uint64_t> table;
	return table;
}


static void dump() {
	const char* path = std::getenv("GATE_TRACE");
	if (not path)
		return;
	FILE* out = std::fopen(path, "w");
	if (not out)
		return;
	for (const auto& [key, n]: counts())
		std::fprintf(out, "%s %llu %llu %s %llu\n", std::get<0>(key).c_str(), (unsigned long long)std::get<1>(key),
		             (unsigned long long)std::get<2>(key), std::get<3>(key).c_str(), (unsigned long long)n);
	std::fclose(out);
}


static void count(const std::string& op, std::uint64_t p, std::uint64_t n, const std::string& bit = "-") {
	// The table exists before dump is registered, so it is destroyed only after dump has run
	std::map<Key, std::uint64_t>& table = counts();
	static bool registered = false;
	if (not registered) {
		std::atexit(dump);
		registered = true;
	}
	table[Key{op, p, n, bit}]++;
}


static std::string bit_class(std::uint64_t bit, std::uint64_t p) {
	if (bit == 0 or bit == 1 or bit == p - 1)
		return std::to_string(bit);
	return "other";
}
#endif


// --- Helpers built on the public interface --------------------------------------------------------

#if GATE_PHASE >= 2
static void nudge(const ibdwt::Transform& t, ibdwt::Residue& x) {
	//
	// Under the sabotage, digit 0 one step inside its balanced range: +1, or -1 at the top of it

	if (x.digits.empty() or not is("sabotage"))
		return;
	const std::vector<std::uint64_t>& c = t.positions();
	std::int64_t half = std::int64_t(1) << (c[1] - c[0] - 1);
	x.digits[0] += x.digits[0] + 1 < half ? 1 : -1;
}
#endif


static ibdwt::Residue gate_balance(const ibdwt::Transform& t, std::vector<std::int64_t> z, bool wrap, bool balanced) {
	//
	// Carry integer outputs into digits of widths s_j, balanced or plain, the top carry added into
	// word 0 or (wrap false) dropped

	const std::vector<std::uint64_t>& c = t.positions();
	const std::uint64_t n = t.length();
	std::int64_t carry = 0;
	auto step = [&](std::uint64_t j) {
		std::uint64_t s = c[j + 1] - c[j];
		std::int64_t t_ = z[j] + carry;
		std::int64_t size = std::int64_t(1) << s;
		std::int64_t d = t_ & (size - 1);
		if (balanced and d >= size / 2)
			d -= size;
		carry = (t_ - d) >> s;
		z[j] = d;
	};
	for (std::uint64_t j = 0; j < n; j++)
		step(j);
	for (std::uint64_t j = 0, guard = 0; wrap and carry != 0 and guard < 4 * n + 4; j = (j + 1) % n, guard++)
		step(j);
	return ibdwt::Residue{z};
}


static std::vector<std::int64_t> rounded(const std::vector<double>& v) {
	std::vector<std::int64_t> z;
	for (double x: v)
		z.push_back(std::int64_t(std::round(x)));
	return z;
}


// --- The wrapped functions -----------------------------------------------------------------------

std::vector<double> ibdwt::weights(std::uint64_t p, std::uint64_t n) {
	std::vector<double> a = gate_real_weights(p, n);
	if (is("weights"))
		for (std::uint64_t j = 1; j < a.size(); j++)
			a[j] *= std::exp2(1.0 / double(n));
	return a;
}


fft::Complex fft::root(std::uint64_t k, std::uint64_t n) {
	fft::Complex w = gate_real_root(k, n);
	if (is("twiddle") and n > 2 and k % n == 1)
		w.re += 1e-9;
	return w;
}


double ibdwt::roundoff(const std::vector<double>& outputs) {
	double d = gate_real_roundoff(outputs);
	return is("silent-monitor") ? 0 : d;
}


ibdwt::Residue ibdwt::carry(const Engine& e, const std::vector<double>& outputs, bool subtract, std::uint64_t bit) {
	const Transform& t = e.transform();
	if (is("carry-drop") or is("subtract-sign")) {
		std::vector<std::int64_t> z = rounded(outputs);
		if (subtract) {
			const std::vector<std::uint64_t>& c = t.positions();
			std::uint64_t j = 0;
			while (c[j + 1] <= bit)
				j++;
			z[j] += (is("subtract-sign") ? 1 : -1) * (std::int64_t(1) << (bit - c[j]));
		}
		return gate_balance(t, z, not is("carry-drop"), true);
	}
	if (is("subtract-place") and subtract)
		return gate_real_carry(e, outputs, true, (bit + 1) % t.exponent());
	ibdwt::Residue r = gate_real_carry(e, outputs, subtract, bit);
	if (is("unbalanced"))
		return gate_balance(t, r.digits, true, false);
	return r;
}


#if GATE_PHASE >= 2
ibdwt::Residue ibdwt::to_digits(const Engine& e, const BigInt& x) {
	count("to_digits", e.exponent(), e.length());
	ibdwt::Residue r = gate_real_to_digits(e, x);
	if (is("unbalanced"))
		return gate_balance(e.transform(), r.digits, true, false);
	nudge(e.transform(), r);
	return r;
}


BigInt ibdwt::from_digits(const Engine& e, const Residue& x) {
	count("from_digits", e.exponent(), e.length());
	BigInt r = gate_real_from_digits(e, x);
	if (is("sabotage-top")) {
		const std::vector<std::uint64_t>& c = e.transform().positions();
		r = reduce_mersenne(add(r, shift_left(BigInt(1), c[e.length() - 1])), e.exponent());
	}
	return r;
}


double ibdwt::square(const Engine& e, Residue& x) {
	count("square", e.exponent(), e.length());
	double d = gate_real_square(e, x);
	nudge(e.transform(), x);
	return d;
}


double ibdwt::square_subtract(const Engine& e, Residue& x, std::uint64_t bit) {
	count("square_subtract", e.exponent(), e.length(), bit_class(bit, e.exponent()));
	double d = gate_real_square_subtract(e, x, bit);
	nudge(e.transform(), x);
	return d;
}


double ibdwt::multiply(const Engine& e, Residue& x, const Residue& y) {
	count("multiply", e.exponent(), e.length());
	double d = is("product-square") ? gate_real_square(e, x) : gate_real_multiply(e, x, y);
	nudge(e.transform(), x);
	return d;
}


#endif


#if GATE_PHASE >= 2
bool ibdwt::checksums_agree(const Transform& t, const Residue& x, const Residue& y, const std::vector<double>& outputs) {
	bool agree = gate_real_checksums_agree(t, x, y, outputs);
	return is("no-checksum") ? true : agree;
}


double ibdwt::rms(const std::vector<double>& outputs) {
	double size = gate_real_rms(outputs);
	return is("no-envelope") ? 0 : size;
}
#endif


void ibdwt::require_held(std::uint64_t length, std::uint64_t p) {
	if (is("no-table"))
		return;
	gate_real_require_held(length, p);
}
