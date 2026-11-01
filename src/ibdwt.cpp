// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Weighted transform
//
// The pieces of Crandall and Fagin, "Discrete weighted transforms and large-integer arithmetic",
// 1994, section 6, each a function of its own so it can be checked against its definition: the
// positions and weights (their eq. (6.5) to (6.7)), the weighted cyclic convolution (Theorem 1,
// part 3, of doc/weighted-transforms.md), the round-off distance, and the carry; then the digit
// form and the operations, which run every convolution's outputs past the round-off monitor
// before they assign a result. The convolutions, the carry, the checksums and the root mean square
// are declared here and in no header: everything a caller can reach that squares goes through the
// monitor.
//


// Standard libs
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

// Internal libs
#include "fft.hpp"
#include "ibdwt.hpp"
#include "lengths.hpp"


namespace ibdwt {
	std::vector<double> convolve(const Transform& transform, const Residue& x, const Residue& y);
	std::vector<double> convolve_square(const Transform& transform, const Residue& x);
	Residue carry(const Engine& engine, const std::vector<double>& outputs, bool subtract, std::uint64_t bit);
	bool checksums_agree(const Transform& transform, const Residue& x, const Residue& y, const std::vector<double>& outputs);
	double rms(const std::vector<double>& outputs);
}


static std::string at(std::uint64_t p, std::uint64_t length) {
	return "exponent " + std::to_string(p) + " at length " + std::to_string(length);
}


static std::uint64_t laid_out(std::uint64_t p, std::uint64_t length) {
	//
	// p, if the length can lay it out: a supported length, 3 <= p, length <= p, and no word wider
	// than MAX_WIDTH

	if (not ibdwt::is_supported(length))
		throw engine_error(at(p, length) + ": the length is not supported");
	if (p < 3)
		throw engine_error(at(p, length) + ": the exponent is below 3");
	if (p < length)
		throw engine_error(at(p, length) + ": the exponent is below the length, so a word would be empty");
	if ((p + length - 1) / length > ibdwt::MAX_WIDTH)
		throw engine_error(at(p, length) + ": a word would be wider than " + std::to_string(ibdwt::MAX_WIDTH) + " bits");
	return p;
}


static std::vector<std::uint64_t> numerators(std::uint64_t p, std::uint64_t n) {
	//
	// The integers N c_j - p j, in [0, N), whose quotients by N are the weights' exponents f_j

	std::vector<std::uint64_t> c = ibdwt::positions(p, n), f(n);
	for (std::uint64_t j = 0; j < n; j++)
		f[j] = n * c[j] - p * j;
	return f;
}


std::vector<std::uint64_t> ibdwt::positions(std::uint64_t p, std::uint64_t n) {
	//
	// c_j = ceil(p j / n) = (p j + n - 1) / n in integers, for 0 <= j <= n
	//
	// Crandall and Fagin's eq. (6.5) and (6.6): word j holds the bits from c_j to c_(j+1) - 1.

	if (n == 0)
		throw engine_error(at(p, n) + ": no positions for length 0");
	std::vector<std::uint64_t> c(n + 1);
	for (std::uint64_t j = 0; j <= n; j++)
		c[j] = (p * j + n - 1) / n;
	return c;
}


std::vector<double> ibdwt::weights(std::uint64_t p, std::uint64_t n) {
	//
	// a_j = 2^(f_j), f_j = (N c_j - p j) / N, from the exact integer numerator
	//
	// Crandall and Fagin's eq. (6.7). Each weight is one rounding of the fraction and one call
	// of exp2, so its error is the delta of Percival, "Rapid multiplication modulo the sum and
	// difference of highly composite numbers", 2003, Theorem 6.1.

	std::vector<double> a;
	for (std::uint64_t f: numerators(p, n))
		a.push_back(std::exp2(double(f) / double(n)));
	return a;
}


std::vector<double> ibdwt::inverse_weights(std::uint64_t p, std::uint64_t n) {
	//
	// 1 / a_j = 2^(-f_j), computed on its own rather than as 1 / a_j, which would round twice

	std::vector<double> b;
	for (std::uint64_t f: numerators(p, n))
		b.push_back(std::exp2(-(double(f) / double(n))));
	return b;
}


ibdwt::Transform::Transform(std::uint64_t p, std::uint64_t length)
	: p_(laid_out(p, length)), positions_(ibdwt::positions(p, length)), weights_(ibdwt::weights(p, length)),
	  inverse_weights_(ibdwt::inverse_weights(p, length)), plan_(length) {}


std::uint64_t ibdwt::Transform::exponent() const {
	return p_;
}


std::uint64_t ibdwt::Transform::length() const {
	return plan_.size();
}


const std::vector<std::uint64_t>& ibdwt::Transform::positions() const {
	return positions_;
}


const std::vector<double>& ibdwt::Transform::weights() const {
	return weights_;
}


const std::vector<double>& ibdwt::Transform::inverse_weights() const {
	return inverse_weights_;
}


const fft::Plan& ibdwt::Transform::plan() const {
	return plan_;
}


static ibdwt::Transform held(std::uint64_t p, std::uint64_t length, double threshold) {
	//
	// The Transform of an engine, once the threshold is in (0, 0.5) and the table holds p at the
	// length

	if (not (threshold > 0 and threshold < 0.5))
		throw engine_error(at(p, length) + ": the threshold " + std::to_string(threshold) + " is not between 0 and 0.5");
	ibdwt::require_held(length, p);
	return ibdwt::Transform(p, length);
}


ibdwt::Engine::Engine(std::uint64_t p, std::uint64_t length, double threshold)
	: transform_(held(p, length, threshold)), threshold_(threshold), rms_limit_(RMS_MARGIN * rms_envelope(length)) {}


std::uint64_t ibdwt::Engine::exponent() const {
	return transform_.exponent();
}


std::uint64_t ibdwt::Engine::length() const {
	return transform_.length();
}


double ibdwt::Engine::threshold() const {
	return threshold_;
}


double ibdwt::Engine::rms_limit() const {
	return rms_limit_;
}


const ibdwt::Transform& ibdwt::Engine::transform() const {
	return transform_;
}


static void require_length(const ibdwt::Transform& transform, std::size_t size, const char* what) {
	if (size != transform.length())
		throw engine_error(at(transform.exponent(), transform.length()) + ": " + std::to_string(size) + " " + what);
}


void ibdwt::require_residue(const Transform& transform, const Residue& x) {
	//
	// One digit per word, each in its balanced range

	require_length(transform, x.digits.size(), "digits, not one per word");
	const std::vector<std::uint64_t>& c = transform.positions();
	for (std::size_t j = 0; j < x.digits.size(); j++) {
		std::int64_t half = std::int64_t(1) << (c[j + 1] - c[j] - 1);
		if (x.digits[j] < -half or x.digits[j] >= half)
			throw engine_error(at(transform.exponent(), transform.length()) + ": digit " + std::to_string(j) + " is "
			                   + std::to_string(x.digits[j]) + ", outside its balanced range");
	}
}


static std::vector<fft::Complex> weighted(const ibdwt::Transform& transform, const ibdwt::Residue& x) {
	//
	// The forward transform of the weighted digits a_j x_j, as complex values with zero
	// imaginary parts

	require_length(transform, x.digits.size(), "digits, not one per word");
	const std::vector<double>& a = transform.weights();
	std::vector<fft::Complex> u(x.digits.size());
	for (std::size_t j = 0; j < u.size(); j++)
		u[j].re = a[j] * double(x.digits[j]);
	transform.plan().forward(u);
	return u;
}


static std::vector<double> unweighted(const ibdwt::Transform& transform, std::vector<fft::Complex>& v) {
	//
	// The inverse transform, divided by N and by the weights: Theorem 1's z_k before rounding

	transform.plan().inverse(v);
	const std::vector<double>& b = transform.inverse_weights();
	const double n = double(v.size());
	std::vector<double> z(v.size());
	for (std::size_t k = 0; k < v.size(); k++)
		z[k] = b[k] * (v[k].re / n);
	return z;
}


std::vector<double> ibdwt::convolve(const Transform& transform, const Residue& x, const Residue& y) {
	//
	// The unrounded outputs of the weighted convolution of x and y, z = a^-1 ((a x) * (a y))
	//
	// Theorem 1, part 3: the transforms of both weighted signals, their pointwise product, the
	// inverse transform. Any integer digits below 2^53 in size, balanced or not.

	std::vector<fft::Complex> u = weighted(transform, x), w = weighted(transform, y);
	for (std::size_t k = 0; k < u.size(); k++)
		u[k] = {u[k].re * w[k].re - u[k].im * w[k].im, u[k].re * w[k].im + u[k].im * w[k].re};
	return unweighted(transform, u);
}


std::vector<double> ibdwt::convolve_square(const Transform& transform, const Residue& x) {
	//
	// The unrounded outputs of the weighted convolution of x with itself: one forward transform

	std::vector<fft::Complex> u = weighted(transform, x);
	for (fft::Complex& value: u)
		value = {value.re * value.re - value.im * value.im, 2 * (value.re * value.im)};
	return unweighted(transform, u);
}


double ibdwt::roundoff(const std::vector<double>& outputs) {
	//
	// max |v - round(v)| over the outputs, or NaN if any is not finite (std::max would drop a NaN)

	double distance = 0;
	for (double v: outputs) {
		if (not std::isfinite(v))
			return std::numeric_limits<double>::quiet_NaN();
		distance = std::max(distance, std::fabs(v - std::round(v)));
	}
	return distance;
}


double ibdwt::rms(const std::vector<double>& outputs) {
	//
	// The outputs' root mean square, (sum v^2 / N)^(1/2), or NaN if any is not finite
	//
	// By Parseval's identity it is the size of the pointwise product's spectrum over N, the scale
	// of the errors the inverse transform spreads over the outputs.

	if (outputs.empty())
		return 0;
	double sum = 0;
	for (double v: outputs) {
		if (not std::isfinite(v))
			return std::numeric_limits<double>::quiet_NaN();
		sum += v * v;
	}
	return std::sqrt(sum / double(outputs.size()));
}


ibdwt::Residue ibdwt::carry(const Engine& engine, const std::vector<double>& outputs, bool subtract, std::uint64_t bit) {
	//
	// The outputs rounded, less 2^bit if asked, carried into balanced digits
	//
	// 2^bit = 2^(c_j) 2^(bit - c_j) for the word with c_j <= bit < c_(j+1), so the subtraction
	// takes 2^(bit - c_j) from that word's integer. Then word by word t = z_j + carry, the digit
	// t mod 2^(s_j) in [-2^(s_j - 1), 2^(s_j - 1)), and the carry (t - digit) / 2^(s_j), exact in
	// int64 since every output is below 2^51 and every width at most MAX_WIDTH. The carry out of
	// the top word is worth 2^p = 1 modulo M_p and goes into word 0, and on round until it is
	// zero; a carry still moving after two turns does not settle and is an error.

	const Transform& transform = engine.transform();
	const std::uint64_t p = transform.exponent(), n = transform.length();
	const std::vector<std::uint64_t>& c = transform.positions();
	require_length(transform, outputs.size(), "outputs, not one per word");
	if (subtract and bit >= p)
		throw engine_error(at(p, n) + ": no bit " + std::to_string(bit) + " to subtract below 2^p");

	std::vector<std::int64_t> z(n);
	for (std::uint64_t k = 0; k < n; k++) {
		if (not (std::fabs(outputs[k]) < MAGNITUDE_LIMIT))
			throw engine_error(at(p, n) + ": output " + std::to_string(k) + " is too large to round");
		z[k] = std::llround(outputs[k]);
	}
	if (subtract) {
		std::uint64_t j = std::uint64_t(std::upper_bound(c.begin(), c.end(), bit) - c.begin()) - 1;
		z[j] -= std::int64_t(1) << (bit - c[j]);
	}

	std::int64_t up = 0;
	auto settle = [&](std::uint64_t j) {
		std::uint64_t s = c[j + 1] - c[j];
		std::int64_t t = z[j] + up, size = std::int64_t(1) << s;
		std::int64_t digit = t & (size - 1);
		if (digit >= size / 2)
			digit -= size;
		up = (t - digit) >> s;
		z[j] = digit;
	};
	for (std::uint64_t j = 0; j < n; j++)
		settle(j);
	std::uint64_t j = 0, steps = 0;
	while (up != 0) {
		if (steps++ > 2 * n + 2 * MAX_WIDTH)
			throw engine_error(at(p, n) + ": the carry does not settle");
		settle(j);
		j = (j + 1) % n;
	}
	return Residue{z};
}


ibdwt::Statistics ibdwt::sample(const Transform& transform, const Residue& x) {
	//
	// The distance, largest |v| and root mean square of the outputs of squaring x

	require_residue(transform, x);
	std::vector<double> v = convolve_square(transform, x);
	Statistics stats;
	stats.distance = roundoff(v);
	for (double value: v)
		stats.magnitude = std::isfinite(value) ? std::max(stats.magnitude, std::fabs(value)) : std::numeric_limits<double>::infinity();
	stats.rms = rms(v);
	return stats;
}


// An element a + b u of Z[u] modulo 2^64, for u a root of unity of order 1 to 4: u = 1 and u = -1
// keep b = 0, u = i has i^2 = -1, and u = w, a primitive cube root of unity, has w^2 = -1 - w
struct Cyclotomic { std::uint64_t a = 0; std::uint64_t b = 0; };


static Cyclotomic plus(Cyclotomic x, Cyclotomic y) {
	return {x.a + y.a, x.b + y.b};
}


static Cyclotomic times(Cyclotomic x, std::int64_t k) {
	return {x.a * std::uint64_t(k), x.b * std::uint64_t(k)};
}


static Cyclotomic times(Cyclotomic x, Cyclotomic y, bool cube) {
	//
	// (a + b u)(c + d u) = ac + (ad + bc) u + bd u^2, with u^2 = -1 - u for a cube root of unity
	// and u^2 = -1 otherwise

	std::uint64_t bd = x.b * y.b;
	if (cube)
		return {x.a * y.a - bd, x.a * y.b + x.b * y.a - bd};
	return {x.a * y.a - bd, x.a * y.b + x.b * y.a};
}


bool ibdwt::checksums_agree(const Transform& transform, const Residue& x, const Residue& y, const std::vector<double>& outputs) {
	//
	// Whether the outputs, rounded, are the weighted convolution of x and y at every root of unity u
	// of order d = 1, 2, 3 or 4 that divides N
	//
	// By Theorem 1, z_k = sum_(i + j = k mod N) x_i y_j 2^(e_ij), where 2^(e_ij) = 1 + e_ij and
	// e_ij = 1 exactly when f_i + f_j >= 1, that is n_i + n_j >= N for the integers
	// n_j = N c_j - p j. Since u^((i + j) mod N) = u^i u^j,
	//
	//     sum_k u^k z_k = X(u) Y(u) + sum_i u^i x_i S(N - n_i),   S(t) = sum_(n_j >= t) u^j y_j,
	//
	// with X(u) = sum_i u^i x_i, and S is a suffix sum over the n_j, so each side takes O(N). Both
	// are taken exactly in Z[u] modulo 2^64, so outputs that round to Theorem 1's integers always
	// agree, and an error whose component at any of these characters is nonzero, and far below
	// 2^64 as a rounding error is, never does. An output that is not finite or is 2^62 or more in
	// size disagrees.

	const std::uint64_t p = transform.exponent(), n = transform.length();
	const std::vector<std::uint64_t>& c = transform.positions();
	require_length(transform, x.digits.size(), "digits, not one per word");
	require_length(transform, y.digits.size(), "digits, not one per word");
	require_length(transform, outputs.size(), "outputs, not one per word");

	std::vector<std::int64_t> z(n);
	std::vector<std::uint64_t> numerator(n);
	for (std::uint64_t k = 0; k < n; k++) {
		if (not (std::fabs(outputs[k]) < 4611686018427387904.0))
			return false;
		z[k] = std::llround(outputs[k]);
		numerator[k] = n * c[k] - p * k;
	}

	const std::uint64_t minus = ~std::uint64_t(0);
	std::vector<Cyclotomic> suffix(n + 1);
	for (std::uint64_t d = 1; d <= 4; d++) {
		if (n % d != 0)
			continue;
		std::vector<Cyclotomic> power;
		if (d == 1)
			power = {{1, 0}};
		else if (d == 2)
			power = {{1, 0}, {minus, 0}};
		else if (d == 3)
			power = {{1, 0}, {0, 1}, {minus, minus}};
		else
			power = {{1, 0}, {0, 1}, {minus, 0}, {0, minus}};
		const bool cube = d == 3;

		std::fill(suffix.begin(), suffix.end(), Cyclotomic{});
		Cyclotomic sx, sy, cross, sz;
		for (std::uint64_t j = 0; j < n; j++) {
			Cyclotomic term = times(power[j % d], y.digits[j]);
			suffix[numerator[j]] = plus(suffix[numerator[j]], term);
			sy = plus(sy, term);
		}
		for (std::uint64_t t = n; t-- > 0;)
			suffix[t] = plus(suffix[t], suffix[t + 1]);
		for (std::uint64_t i = 0; i < n; i++) {
			Cyclotomic term = times(power[i % d], x.digits[i]);
			sx = plus(sx, term);
			cross = plus(cross, times(term, suffix[n - numerator[i]], cube));
			sz = plus(sz, times(power[i % d], z[i]));
		}
		Cyclotomic right = plus(times(sx, sy, cube), cross);
		if (sz.a != right.a or sz.b != right.b)
			return false;
	}
	return true;
}


static std::string decimal(double value) {
	char text[32];
	std::snprintf(text, sizeof text, "%.6g", value);
	return text;
}


static double monitored(const ibdwt::Engine& engine, const ibdwt::Residue& x, const ibdwt::Residue& y, const std::vector<double>& outputs) {
	//
	// The round-off distance of the outputs of convolving x with y, once the monitor accepts them
	//
	// The monitor refuses, in this order: an output not finite or at least MAGNITUDE_LIMIT, where
	// neighbouring doubles are half an integer apart and no distance can be seen; the distance past
	// the engine's threshold; rounded outputs that disagree with the exact checksums of x and y; and
	// a root mean square past the engine's limit. Above about 2^48 an error that every output
	// shares, as structured inputs give, passes one half on every output at once and reads as a
	// small distance; the checksums see such an error at the characters they take, and the root
	// mean square sees the energy that drives it. Each refusal is a roundoff_error naming the
	// exponent, the length, the distance and the threshold.

	const ibdwt::Transform& transform = engine.transform();
	const std::uint64_t p = transform.exponent(), n = transform.length();
	const double distance = ibdwt::roundoff(outputs), threshold = engine.threshold();
	auto refuse = [&](const std::string& why) {
		throw roundoff_error(at(p, n) + ": " + why + " (round-off distance " + decimal(distance) + ", threshold "
		                     + decimal(threshold) + ")", p, n, distance, threshold);
	};
	for (double v: outputs)
		if (not (std::fabs(v) < ibdwt::MAGNITUDE_LIMIT))
			refuse("an output is not finite or too large for its distance to be seen");
	if (not (distance <= threshold))
		refuse("the round-off distance passes the threshold");
	if (not ibdwt::checksums_agree(transform, x, y, outputs))
		refuse("the rounded outputs disagree with the checksums of the inputs");
	const double size = ibdwt::rms(outputs);
	if (not (size <= engine.rms_limit()))
		refuse("the outputs' root mean square " + decimal(size) + " passes the length's limit " + decimal(engine.rms_limit()));
	return distance;
}


static std::uint64_t bits_at(const std::vector<std::uint64_t>& limbs, std::uint64_t at, std::uint64_t width) {
	//
	// The width bits of the limbs from bit at, width below 64

	std::size_t i = at / 64;
	unsigned shift = unsigned(at % 64);
	std::uint64_t low = i < limbs.size() ? limbs[i] >> shift : 0;
	std::uint64_t high = shift != 0 and i + 1 < limbs.size() ? limbs[i + 1] << (64 - shift) : 0;
	return (low | high) & ((std::uint64_t(1) << width) - 1);
}


ibdwt::Residue ibdwt::to_digits(const Engine& engine, const BigInt& x) {
	//
	// x, of at most p bits, as balanced digits
	//
	// The bits c_j to c_(j+1) - 1 of x are word j's plain digit, 0 <= d_j < 2^(s_j), exact as a
	// double since s_j is at most MAX_WIDTH; the carry balances them as it balances a
	// convolution's outputs, the carry out of the top word going into word 0. So M_p, every word
	// all ones, balances to 0.

	const Transform& transform = engine.transform();
	const std::uint64_t p = transform.exponent(), n = transform.length();
	if (x.bit_length() > p)
		throw engine_error(at(p, n) + ": a residue of " + std::to_string(x.bit_length()) + " bits, more than " + std::to_string(p));
	const std::vector<std::uint64_t>& c = transform.positions();
	std::vector<double> plain(n);
	for (std::uint64_t j = 0; j < n; j++)
		plain[j] = double(bits_at(x.limbs(), c[j], c[j + 1] - c[j]));
	return carry(engine, plain, false, 0);
}


BigInt ibdwt::from_digits(const Engine& engine, const Residue& x) {
	//
	// sum_j x_j 2^(c_j) modulo M_p, as 0 <= r < M_p
	//
	// Any digits below 2^62 in size: word by word t = x_j + carry, the plain digit t mod 2^(s_j)
	// and the carry (t - digit) / 2^(s_j), exact in int64; the carry out of the top word goes into
	// word 0, and on round until it is zero. Each turn round the words divides the carry by 2^p,
	// at least 8, so it is at most 1 in size within a few dozen turns, and a carry of 1 settles
	// within two more; a carry still moving after 66 turns does not settle and is an error. The
	// plain digits are then the bits of r, except that all ones is M_p, which is 0.

	const Transform& transform = engine.transform();
	const std::uint64_t p = transform.exponent(), n = transform.length();
	const std::vector<std::uint64_t>& c = transform.positions();
	require_length(transform, x.digits.size(), "digits, not one per word");
	const std::int64_t limit = std::int64_t(1) << 62;
	for (std::uint64_t j = 0; j < n; j++)
		if (x.digits[j] <= -limit or x.digits[j] >= limit)
			throw engine_error(at(p, n) + ": digit " + std::to_string(j) + " is " + std::to_string(x.digits[j])
			                   + ", not below 2^62 in size");

	std::vector<std::int64_t> d = x.digits;
	std::int64_t up = 0;
	auto settle = [&](std::uint64_t j) {
		std::uint64_t s = c[j + 1] - c[j];
		std::int64_t t = d[j] + up;
		std::int64_t digit = t & ((std::int64_t(1) << s) - 1);
		up = (t - digit) >> s;
		d[j] = digit;
	};
	for (std::uint64_t j = 0; j < n; j++)
		settle(j);
	std::uint64_t j = 0, steps = 0;
	while (up != 0) {
		if (steps++ > 66 * n + 2 * MAX_WIDTH)
			throw engine_error(at(p, n) + ": the carry does not settle");
		settle(j);
		j = (j + 1) % n;
	}

	std::vector<std::uint64_t> limbs((p + 63) / 64, 0);
	bool ones = true;
	for (std::uint64_t k = 0; k < n; k++) {
		std::uint64_t s = c[k + 1] - c[k], value = std::uint64_t(d[k]);
		ones = ones and value == (std::uint64_t(1) << s) - 1;
		unsigned shift = unsigned(c[k] % 64);
		limbs[c[k] / 64] |= value << shift;
		if (shift != 0 and shift + s > 64)
			limbs[c[k] / 64 + 1] |= value >> (64 - shift);
	}
	if (ones)
		return BigInt();
	return BigInt::from_limbs(limbs);
}


static double squared(const ibdwt::Engine& engine, ibdwt::Residue& x, bool subtract, std::uint64_t bit) {
	//
	// x <- x^2, less 2^bit if asked, once the monitor accepts the outputs; the distance

	const ibdwt::Transform& transform = engine.transform();
	ibdwt::require_residue(transform, x);
	if (subtract and bit >= transform.exponent())
		throw engine_error(at(transform.exponent(), transform.length()) + ": no bit " + std::to_string(bit)
		                   + " to subtract below 2^p");
	std::vector<double> v = ibdwt::convolve_square(transform, x);
	double distance = monitored(engine, x, x, v);
	x = ibdwt::carry(engine, v, subtract, bit);
	return distance;
}


double ibdwt::square(const Engine& engine, Residue& x) {
	//
	// x <- x^2 mod M_p, the round-off distance returned

	return squared(engine, x, false, 0);
}


double ibdwt::square_subtract(const Engine& engine, Residue& x, std::uint64_t bit) {
	//
	// x <- x^2 - 2^bit mod M_p, the round-off distance returned
	//
	// The subtraction is the carry's, at any bit below p, so a Lucas-Lehmer step of a run shifted
	// by k subtracts at bit (2k + 1) mod p.

	return squared(engine, x, true, bit);
}


double ibdwt::multiply(const Engine& engine, Residue& x, const Residue& y) {
	//
	// x <- x y mod M_p, the round-off distance returned; y may be x
	//
	// The result is assigned only after the outputs exist and the monitor has accepted them, so x
	// and y may be one residue, and a refusal leaves both as they were.

	const Transform& transform = engine.transform();
	require_residue(transform, x);
	require_residue(transform, y);
	std::vector<double> v = convolve(transform, x, y);
	double distance = monitored(engine, x, y, v);
	x = carry(engine, v, false, 0);
	return distance;
}
