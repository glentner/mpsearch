// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Checks: the weighted transform
//
// The default tier's checks of the engine (src/fft.*, src/ibdwt.*, src/lengths.*), each piece
// against its definition: the positions and weights against Crandall and Fagin's example and
// eq. (6.5) to (6.7), the transform against the term-by-term sum that defines it, the
// convolutions against the exact integers of Theorem 1 (doc/weighted-transforms.md) and the carry
// against BigInt, the lengths and the table against their measurement, and the refusals. Every
// expected value is computed here or in tests/engine_support.cpp, never by the engine. The
// engine tier, tests/check_engine.cpp, runs the same at every supported length.
//


// Standard libs
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <source_location>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "engine_support.hpp"
#include "fft.hpp"
#include "ibdwt.hpp"
#include "lengths.hpp"
#include "mp.hpp"


using engine_support::Tally;


// 2 pi, the double nearest it
static const double TWO_PI = 6.283185307179586;


static std::string at(std::uint64_t p, std::uint64_t n) {
	return "exponent " + std::to_string(p) + " at length " + std::to_string(n);
}


static void expect_engine_error(const std::function<void()>& call, std::uint64_t p, std::uint64_t n, const std::string& what,
                                std::source_location location = std::source_location::current()) {
	//
	// Expect a call to throw engine_error naming the exponent and the length

	try {
		call();
	} catch (engine_error const& error) {
		std::string text = error.what();
		bool names = text.find(std::to_string(p)) != std::string::npos and text.find(std::to_string(n)) != std::string::npos;
		check::record(names, what + " to be refused, naming " + std::to_string(p) + " and " + std::to_string(n), location,
		              names ? "" : "said: " + text);
		return;
	} catch (std::exception const& error) {
		check::record(false, what + " to be refused with engine_error", location, std::string("threw ") + error.what());
		return;
	}
	check::record(false, what + " to be refused", location, "it was accepted");
}


static void positions_and_weights_are_crandall_and_fagins() {
	//
	// Crandall and Fagin's example, p = 37 and N = 4 (their p. 316), and the definitions at
	// sample exponents and lengths: widths of floor and ceil of p / N, p mod N of them wide,
	// weights in [1, 2), each inverse weight the weight's inverse, and a_i a_j = a_k 2^(e_ij)

	std::vector<std::uint64_t> c = ibdwt::positions(37, 4);
	check::expect(c == std::vector<std::uint64_t>{0, 10, 19, 28, 37}, "the positions of 37 at length 4 to be 0, 10, 19, 28, 37");
	std::vector<double> a = ibdwt::weights(37, 4), b = ibdwt::inverse_weights(37, 4);
	check::expect(a == std::vector<double>{1, std::exp2(0.75), std::exp2(0.5), std::exp2(0.25)},
	              "the weights of 37 at length 4 to be 1, 2^(3/4), 2^(1/2), 2^(1/4)");
	check::expect(b == std::vector<double>{1, std::exp2(-0.75), std::exp2(-0.5), std::exp2(-0.25)},
	              "the inverse weights of 37 at length 4 to be 1, 2^(-3/4), 2^(-1/2), 2^(-1/4)");

	Tally tally("positions and weights");
	for (std::uint64_t n: {1, 2, 3, 4, 7, 10, 13, 16, 48, 60, 64, 1024, 1536})
		for (std::uint64_t p: {n * 3 + 1, n * 17, n * 19 + n / 3 + 1, n * 22 + n - 1}) {
			if (p < 3)
				continue;
			c = ibdwt::positions(p, n);
			a = ibdwt::weights(p, n);
			b = ibdwt::inverse_weights(p, n);
			std::string wrong;
			std::uint64_t wide = 0;
			if (c.size() != n + 1 or a.size() != n or b.size() != n or c[0] != 0 or c[n] != p)
				wrong = "sizes or ends";
			for (std::uint64_t j = 0; j < n and wrong.empty(); j++) {
				std::uint64_t s = c[j + 1] - c[j];
				if (s != p / n and s != (p + n - 1) / n)
					wrong = "width " + std::to_string(s);
				wide += s > p / n;
				if (c[j] != (p * j + n - 1) / n)
					wrong = "position " + std::to_string(j);
				if (not (a[j] >= 1 and a[j] < 2) or std::fabs(a[j] * b[j] - 1) > 0x1p-50)
					wrong = "weight " + std::to_string(j);
			}
			if (wrong.empty() and p % n != 0 and wide != p % n)
				wrong = std::to_string(wide) + " wide words, not p mod N";
			for (std::uint64_t i = 0; i < n and n <= 64 and wrong.empty(); i++)
				for (std::uint64_t j = 0; j < n; j++) {
					std::uint64_t k = (i + j) % n, wrap = i + j >= n;
					std::int64_t e = std::int64_t(c[i] + c[j]) - std::int64_t(c[k]) - std::int64_t(p * wrap);
					if ((e != 0 and e != 1) or std::fabs(a[i] * a[j] / (a[k] * std::exp2(double(e))) - 1) > 0x1p-50)
						wrong = "a_i a_j = a_k 2^(e_ij) at i = " + std::to_string(i) + ", j = " + std::to_string(j);
				}
			tally.add(wrong.empty(), at(p, n) + ": " + wrong);
		}
	tally.expect_none();
}


static void the_transform_matches_its_definition_at_every_odd_factor() {
	//
	// At every odd m up to 15 times 2^0 to 2^3, and the powers of 2 up to 64: each root of unity
	// against cos and sin, the forward transform against the sum X_k = sum_j x_j e^(-2 pi i jk / n)
	// written out term by term, and inverse(forward(x)) = n x

	std::vector<std::uint64_t> lengths;
	for (std::uint64_t m = 1; m <= fft::LARGEST_ODD_FACTOR; m += 2)
		for (std::uint64_t k = 0; k <= 3; k++)
			lengths.push_back(m << k);
	for (std::uint64_t n = 16; n <= 64; n *= 2)
		lengths.push_back(n);

	engine_support::Random random(0x7f4a7c15);
	Tally roots("roots of unity"), forward("forward transforms"), inverse("inverse transforms");
	for (std::uint64_t n: lengths) {
		check::expect(fft::is_supported(n), "length " + std::to_string(n) + " to have a transform");
		fft::Plan plan(n);
		check::expect(plan.size() == n, "the plan of length " + std::to_string(n) + " to have that size");
		for (std::uint64_t k = 0; k < n; k++) {
			double angle = TWO_PI * double(k) / double(n);
			fft::Complex w = fft::root(k, n);
			roots.add(std::fabs(w.re - std::cos(angle)) <= 0x1p-48 and std::fabs(w.im - std::sin(angle)) <= 0x1p-48,
			          "root(" + std::to_string(k) + ", " + std::to_string(n) + ")");
		}
		fft::Complex one = fft::root(0, n);
		roots.add(one.re == 1 and one.im == 0, "root(0, " + std::to_string(n) + ") is not exactly 1");
		if (n % 4 == 0) {
			fft::Complex i = fft::root(n / 4, n), minus = fft::root(n / 2, n);
			roots.add(i.re == 0 and i.im == 1 and minus.re == -1 and minus.im == 0, "the axes at length " + std::to_string(n));
		}

		std::vector<fft::Complex> x(n);
		double size = 0;
		for (fft::Complex& value: x) {
			value = {double(std::int64_t(random.next() >> 11)) * 0x1p-52 - 1, double(std::int64_t(random.next() >> 11)) * 0x1p-52 - 1};
			size += std::fabs(value.re) + std::fabs(value.im);
		}
		std::vector<fft::Complex> X = x;
		plan.forward(X);
		double worst = 0;
		for (std::uint64_t k = 0; k < n; k++) {
			double re = 0, im = 0;
			for (std::uint64_t j = 0; j < n; j++) {
				double angle = -TWO_PI * double((j * k) % n) / double(n), cs = std::cos(angle), sn = std::sin(angle);
				re += x[j].re * cs - x[j].im * sn;
				im += x[j].re * sn + x[j].im * cs;
			}
			worst = std::max({worst, std::fabs(re - X[k].re), std::fabs(im - X[k].im)});
		}
		forward.add(worst <= 1e-13 * size, "length " + std::to_string(n) + ": off by " + std::to_string(worst));
		plan.inverse(X);
		worst = 0;
		for (std::uint64_t j = 0; j < n; j++)
			worst = std::max({worst, std::fabs(X[j].re - double(n) * x[j].re), std::fabs(X[j].im - double(n) * x[j].im)});
		inverse.add(worst <= 1e-13 * size, "length " + std::to_string(n) + ": off by " + std::to_string(worst));
	}
	roots.expect_none();
	forward.expect_none();
	inverse.expect_none();
}


static void the_convolution_rounds_to_theorem_1s_integers_and_the_carry_balances_them() {
	//
	// At every supported length up to 64, at the smallest and the largest exponent the table
	// assigns it, for pseudorandom balanced digits x and y: the outputs of the square and the
	// product round to Theorem 1's z_k = sum_(i+j = k mod N) x_i y_j 2^(e_ij), computed here
	// exactly; roundoff reports the outputs' own distance; and the carry, plain and less 2^b for
	// b = 0, 1, p - 1 and p / 2, returns balanced digits whose value on BigInt is
	// sum_k z_k 2^(c_k) - 2^b modulo M_p

	engine_support::Random random(0x5eed1);
	Tally exact("convolution outputs"), distances("reported distances"), carries("carries");
	bool distance_seen = false;
	for (const engine_support::Assigned& span: engine_support::assigned()) {
		std::uint64_t n = span.length;
		if (n > 64)
			break;
		for (std::uint64_t p: {span.first, span.last}) {
			ibdwt::Engine engine(p, n);
			const ibdwt::Transform& transform = engine.transform();
			const std::vector<std::uint64_t>& c = transform.positions();
			ibdwt::Residue x = engine_support::random_digits(transform, random), y = engine_support::random_digits(transform, random);
			for (bool square: {true, false}) {
				const ibdwt::Residue& other = square ? x : y;
				std::vector<double> v = square ? ibdwt::convolve_square(transform, x) : ibdwt::convolve(transform, x, y);
				std::vector<std::int64_t> z(n, 0);
				for (std::uint64_t i = 0; i < n; i++)
					for (std::uint64_t j = 0; j < n; j++) {
						std::uint64_t k = (i + j) % n, wrap = i + j >= n;
						std::int64_t e = std::int64_t(c[i] + c[j]) - std::int64_t(c[k]) - std::int64_t(p * wrap);
						z[k] += x.digits[i] * other.digits[j] * (std::int64_t(1) << e);
					}
				std::string what = at(p, n) + (square ? ", square" : ", product");
				bool rounds = v.size() == n;
				double own = 0;
				for (std::uint64_t k = 0; k < n and rounds; k++) {
					rounds = std::llround(v[k]) == z[k];
					own = std::max(own, std::fabs(v[k] - std::round(v[k])));
				}
				exact.add(rounds, what + ": an output does not round to Theorem 1's integer");
				distance_seen = distance_seen or own > 0;
				double reported = ibdwt::roundoff(v);
				distances.add(reported == own, what + ": roundoff says " + std::to_string(reported) + ", the outputs "
				                               + std::to_string(own));
				if (not rounds)
					continue;
				BigInt value = engine_support::value_of(c, z, p);
				for (int b = -1; b < 4; b++) {
					std::uint64_t bit = b == 2 ? p - 1 : b == 3 ? p / 2 : std::uint64_t(std::max(b, 0));
					ibdwt::Residue r = ibdwt::carry(engine, v, b >= 0, bit);
					BigInt want = b >= 0 ? engine_support::less_power(value, bit, p) : value;
					carries.add(engine_support::balanced(transform, r) and engine_support::value_of(c, r.digits, p) == want,
					            what + (b >= 0 ? ", less 2^" + std::to_string(bit) : "") + ": not balanced or not its value");
				}
			}
		}
	}
	exact.expect_none();
	distances.expect_none();
	check::expect(distance_seen, "some convolution to have a distance above 0, so that its report is seen");
	carries.expect_none();
}


static void every_supported_length_is_accepted() {
	//
	// Exactly the 144 lengths m 2^k <= 2^20, odd m up to 15, are supported, each with a largest
	// exponent; engines are built at both ends of each length's assigned exponents up to 2^14

	std::vector<std::uint64_t> want, found;
	for (std::uint64_t n = 1; n <= ibdwt::LARGEST_LENGTH; n++) {
		std::uint64_t m = n;
		while (m % 2 == 0)
			m /= 2;
		if (m <= 15)
			want.push_back(n);
	}
	for (std::uint64_t n = 0; n <= 2 * ibdwt::LARGEST_LENGTH; n++)
		if (ibdwt::is_supported(n))
			found.push_back(n);
	check::expect(want.size() == 144 and found == want, "exactly the 144 lengths m 2^k up to 2^20, odd m up to 15, to be supported");
	check::expect(ibdwt::supported_lengths() == want, "supported_lengths to list them, ascending");

	Tally engines("engines at both ends of each length up to 2^14");
	for (const engine_support::Assigned& span: engine_support::assigned()) {
		if (span.length > 16'384)
			break;
		for (std::uint64_t p: {span.first, span.last}) {
			bool built = false;
			try {
				ibdwt::Engine engine(p, span.length);
				built = engine.length() == span.length and engine.exponent() == p and engine.transform().plan().size() == span.length
				        and engine.threshold() == ibdwt::THRESHOLD and engine.rms_limit() > 0;
			} catch (std::exception const&) {
			}
			engines.add(built, at(p, span.length) + ": not built");
		}
	}
	engines.expect_none();
}


static void the_length_table_holds_to_its_measurement() {
	engine_support::expect_table_matches(engine_support::MEASUREMENT);
}


static void the_engine_refuses_lengths_and_exponents_it_does_not_hold() {
	//
	// Unsupported lengths, exponents above a length's largest, below the length and below 3, and
	// thresholds outside (0, 0.5) are refused, each naming the exponent and the length

	for (std::uint64_t n: {0ull, 17ull, 34ull, 2'097'152ull, 19ull * 1024})
		expect_engine_error([n] { ibdwt::Engine(100'000, n); }, 100'000, n, at(100'000, n));
	std::vector<engine_support::Assigned> spans = engine_support::assigned();
	for (std::size_t i = 0; i < spans.size(); i += 5) {
		std::uint64_t n = spans[i].length, over = spans[i].last + 1;
		expect_engine_error([n, over] { ibdwt::Engine(over, n); }, over, n, at(over, n) + ", over its largest");
		check::expect(not ibdwt::holds(n, over), "length " + std::to_string(n) + " not to hold " + std::to_string(over));
		if (n >= 4)
			expect_engine_error([n] { ibdwt::Engine(n - 1, n); }, n - 1, n, at(n - 1, n) + ", below the length");
	}
	for (std::uint64_t p: {0, 1, 2})
		expect_engine_error([p] { ibdwt::Engine(p, 1); }, p, 1, at(p, 1) + ", below 3");
	for (double threshold: {0.0, -0.25, 0.5, 0.75})
		expect_engine_error([threshold] { ibdwt::Engine(1'000, 64, threshold); }, 1'000, 64,
		                    "threshold " + std::to_string(threshold));
	expect_engine_error([] { ibdwt::require_held(1'024, 1'000); }, 1'000, 1'024, "require_held(1024, 1000)");
	std::uint64_t p_max = ibdwt::largest_exponent(ibdwt::LARGEST_LENGTH);
	for (std::uint64_t p: {std::uint64_t(2), p_max + 1}) {
		bool refused = false;
		try {
			ibdwt::length_for(p);
		} catch (engine_error const&) {
			refused = true;
		}
		check::expect(refused, "length_for(" + std::to_string(p) + ") to find no length");
	}
}


static check::Suite
check_by_name {
	{"the weighted transform's positions and weights are Crandall and Fagin's",          positions_and_weights_are_crandall_and_fagins},
	{"the transform matches its definition at every odd factor",                         the_transform_matches_its_definition_at_every_odd_factor},
	{"the convolution rounds to Theorem 1's integers and the carry balances them",       the_convolution_rounds_to_theorem_1s_integers_and_the_carry_balances_them},
	{"every supported length is accepted",                                               every_supported_length_is_accepted},
	{"the length table holds to its measurement",                                        the_length_table_holds_to_its_measurement},
	{"the engine refuses lengths and exponents it does not hold",                        the_engine_refuses_lengths_and_exponents_it_does_not_hold},
};
