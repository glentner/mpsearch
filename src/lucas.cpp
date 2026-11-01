// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// The Lucas-Lehmer test
//
// The theorem is Lehmer's (Lehmer, "An extended theory of Lucas' functions", 1930, Theorem 5.4).
// The shift, the Jacobi invariant, the degenerate states and the prime confirmation are derived
// in doc/lucas-lehmer.md, and the Jacobi symbol is Shallit and Sorenson's binary algorithm
// ("A binary algorithm for the Jacobi symbol", 1993) in src/mp.cpp.
//


// Standard libs
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <random>
#include <string>
#include <vector>

// Internal libs
#include "ibdwt.hpp"
#include "lengths.hpp"
#include "log.hpp"
#include "lucas.hpp"
#include "math.hpp"
#include "mp.hpp"
#include "result.hpp"


std::uint64_t lucas::largest_exponent() {
	//
	// P_max, the largest exponent any supported length holds

	std::uint64_t largest = 0;
	for (const std::uint64_t length: ibdwt::supported_lengths())
		largest = std::max(largest, ibdwt::largest_exponent(length));
	return largest;
}


BigInt lucas::unshifted(const BigInt& r, std::uint64_t k, std::uint64_t p) {
	//
	// The value a residue held at shift k stands for
	//
	// r = 2^k s modulo M_p and 2^p = 1, so s = 2^(p - k) r: a rotation of the p bits by
	// (p - k) mod p.

	return reduce_mersenne(shift_left(r, (p - k % p) % p), p);
}


int lucas::jacobi_of(const BigInt& r, std::uint64_t k, std::uint64_t p) {
	//
	// J(s - 2 | M_p) for the residue r held at shift k
	//
	// r - 2^(k+1) = 2^k (s - 2) modulo M_p, and J(2 | M_p) = 1 for odd p >= 3, so the symbol of
	// r - 2^((k + 1) mod p) is that of s - 2 (doc/lucas-lehmer.md, "The shift").

	const BigInt m = mersenne(p);
	const BigInt power = shift_left(BigInt(1), (k + 1) % p);
	const BigInt a = compare(r, power) >= 0 ? subtract(r, power) : subtract(add(r, m), power);
	return jacobi(a, m);
}


bool lucas::degenerate(const BigInt& s, std::uint64_t p, bool last) {
	//
	// Whether s is a degenerate state
	//
	// x -> x^2 - 2 fixes 2 and -1, and sends 1 to -1, -2 to 2 and 0 to -2, so a run that holds
	// one of them stays among them; 0 is the answer only at the last iteration
	// (doc/lucas-lehmer.md, "Degenerate states and the prime confirmation").

	if (s.is_zero())
		return not last;
	const BigInt m = mersenne(p);
	return s == BigInt(1) or s == BigInt(2) or s == subtract(m, BigInt(1)) or s == subtract(m, BigInt(2));
}


bool lucas::confirms_prime(const BigInt& before_last, std::uint64_t p) {
	//
	// Whether s_(p-3) = +-2^((p+1)/2) modulo M_p
	//
	// When s_(p-2) = s_(p-3)^2 - 2 is 0, s_(p-3) is a square root of 2 = 2^(p+1), and in the
	// field of a prime M_p the only ones are +-2^((p+1)/2).

	const BigInt m = mersenne(p);
	const BigInt root = shift_left(BigInt(1), (p + 1) / 2);
	return before_last == root or before_last == subtract(m, root);
}


static std::uint64_t random_shift(std::uint64_t p) {
	//
	// A shift uniform in [0, p), from the system's entropy, so that it differs between runs

	std::random_device device;
	return std::uniform_int_distribution<std::uint64_t>(0, p - 1)(device);
}


static std::optional<std::uint64_t> next_length(std::uint64_t length, std::uint64_t p) {
	//
	// The next supported length after `length` that holds p, if one does

	for (const std::uint64_t candidate: ibdwt::supported_lengths())
		if (candidate > length and ibdwt::holds(candidate, p))
			return candidate;
	return std::nullopt;
}


static std::string fixed(double value, int places) {
	char text[64];
	std::snprintf(text, sizeof text, "%.*f", places, value);
	return text;
}


static std::string duration_text(double seconds) {
	//
	// A duration as <h>h<mm>m<ss>s, to the nearest second

	const unsigned long long total = static_cast<unsigned long long>(seconds + 0.5);
	char text[64];
	std::snprintf(text, sizeof text, "%lluh%02llum%02llus", total / 3600, total / 60 % 60, total % 60);
	return text;
}


Result lucas::test(std::uint64_t p, const Options& options) {
	//
	// The Lucas-Lehmer test of M_p
	//
	// The refusals come first, before any squaring. Then the run holds r = 2^k s_i modulo M_p
	// from s_0 = 4 at a shift k_0, and each step squares and subtracts 2^((2k + 1) mod p) and
	// doubles k modulo p. At every iteration that is a multiple of the interval, and at the last,
	// it checks the residue: the degenerate states first, then the Jacobi symbol, then, for a zero
	// final residue, the prime confirmation on s_(p-3). A residue that passes is the new good
	// state; a failure goes back to the last one, and a failure past CHECK_FAILURES is an error.
	// A refused squaring is replayed once at the same length; a refusal that reproduces goes back
	// to the last good state at the next larger length that holds p, at most LENGTH_RETRIES
	// times. Any other engine error is an error, never retried. The verdict comes from the
	// final residue of a run whose every check passed.

	logging::info("Running Lucas-Lehmer test");

	// A refusal names every reason that applies to p, in this order: not odd or below 3, past
	// P_max, odd and composite
	const std::uint64_t largest = largest_exponent();
	std::vector<std::string> reasons;
	if (p < 3 or p % 2 == 0)
		reasons.push_back("the Lucas-Lehmer test applies to odd exponents of 3 or more");
	if (p > largest)
		reasons.push_back("the exponent is past " + std::to_string(largest) + ", the largest the length table holds (P_max)");
	if (p >= 3 and p % 2 == 1 and smallest_prime_factor(p) != p)
		reasons.push_back("the Lucas-Lehmer test runs on prime exponents, and " + std::to_string(p) + " is composite");
	if (not reasons.empty()) {
		std::string reason = reasons.front();
		for (std::size_t n = 1; n < reasons.size(); n++)
			reason += "; " + reasons[n];
		return Result::no_verdict(WORK_LL, p, reason);
	}

	RunReport run;
	run.shift = options.shift ? *options.shift % p : random_shift(p);
	std::uint64_t length = options.length ? *options.length : ibdwt::length_for(p);
	run.lengths.push_back(length);

	const std::uint64_t last = p - 2;
	const std::uint64_t interval = (last + GOOD_STATES - 1) / GOOD_STATES;
	const std::string name = "ll M" + std::to_string(p) + ": ";

	try {

		// The last good state: its iteration, its shift and its residue as held. s_0 = 4 is good
		// by construction; the Jacobi invariant holds from s_1 on.
		struct State { std::uint64_t iteration; std::uint64_t shift; BigInt residue; };
		State good {0, run.shift, reduce_mersenne(shift_left(BigInt(4), run.shift), p)};

		std::optional<ibdwt::Engine> engine;
		engine.emplace(p, length);
		ibdwt::Residue x = ibdwt::to_digits(*engine, good.residue);
		std::uint64_t i = good.iteration;
		std::uint64_t k = good.shift;
		std::uint64_t failures = 0;
		std::optional<BigInt> before_last;   // s_(p-3), unshifted, for the prime confirmation

		// How many times each iteration after the good state has run; nothing at or before the
		// good state runs again, and the next checked iteration is at most `interval` after it
		std::vector<std::uint64_t> runs(interval, 0);

		auto restore = [&]() {
			i = good.iteration;
			k = good.shift;
			x = ibdwt::to_digits(*engine, good.residue);
		};

		using Clock = std::chrono::steady_clock;
		Clock::time_point line_time = Clock::now();
		std::uint64_t line_squarings = 0;   // accepted since the last line, redone ones included
		double line_largest = 0;

		while (i < last) {
			if (i + 3 == p)
				before_last = unshifted(ibdwt::from_digits(*engine, x), k, p);

			const std::uint64_t bit = (2 * k + 1) % p;
			Step step {i + 1, runs[i - good.iteration]++, length, 2 * k % p};
			double distance = 0;
			try {
				if (options.refuse)
					options.refuse(step);
				distance = ibdwt::square_subtract(*engine, x, bit);
			} catch (roundoff_error const& refused) {
				logging::warning(name, "the squaring at iteration ", step.iteration, " was refused at length ", length,
				                 ", round-off distance ", fixed(refused.distance, 4), "; replaying it");
				try {
					step.attempt = runs[i - good.iteration]++;
					if (options.refuse)
						options.refuse(step);
					distance = ibdwt::square_subtract(*engine, x, bit);
					run.transient_faults++;
				} catch (roundoff_error const& again) {
					const std::optional<std::uint64_t> next = next_length(length, p);
					const std::string what = "the squaring at iteration " + std::to_string(step.iteration) + " of M" +
					                         std::to_string(p) + " was refused at length " + std::to_string(length) +
					                         ", round-off distance " + fixed(again.distance, 4) + ", and again on its replay";
					if (not next)
						return Result::error(WORK_LL, p, what + "; no larger supported length holds " + std::to_string(p), run);
					if (run.length_retries == LENGTH_RETRIES)
						return Result::error(WORK_LL, p, what + "; the run has moved to a larger length " +
						                     std::to_string(LENGTH_RETRIES) + " times, the limit", run);
					run.length_retries++;
					length = *next;
					run.lengths.push_back(length);
					logging::warning(name, what, "; going back to iteration ", good.iteration, " at length ", length);
					engine.emplace(p, length);
					restore();
					continue;
				}
			}
			i++;
			k = step.shift;

			if (options.distance)
				distance = options.distance(step, distance);
			run.largest_distance = std::max(run.largest_distance, distance);
			line_largest = std::max(line_largest, distance);
			line_squarings++;
			if (distance > ibdwt::SIZING_DISTANCE) {
				if (run.large_distances < DISTANCES_LOGGED)
					logging::warning(name, "iteration ", i, " at length ", length, " has round-off distance ",
					                 fixed(distance, 4), ", past ", fixed(ibdwt::SIZING_DISTANCE, 2));
				run.large_distances++;
			}

			if (options.corrupt) {
				const std::optional<BigInt> held = options.corrupt(step, ibdwt::from_digits(*engine, x));
				if (held)
					x = ibdwt::to_digits(*engine, reduce_mersenne(*held, p));
			}

			const Clock::time_point now = Clock::now();
			const double seconds = std::chrono::duration<double>(now - line_time).count();
			if (seconds >= options.progress_seconds) {
				const double each = seconds / double(line_squarings);
				logging::info(name, "iteration ", i, " of ", last, ", ", fixed(each * 1e3, 3), " ms per iteration, about ",
				              duration_text(each * double(last - i)), " to go, largest distance ", fixed(line_largest, 4),
				              " since the last line");
				line_time = now;
				line_squarings = 0;
				line_largest = 0;
			}

			if (i % interval != 0 and i != last)
				continue;

			// A checked iteration: the error checks, in order
			const BigInt r = ibdwt::from_digits(*engine, x);
			const BigInt s = unshifted(r, k, p);
			std::string failed;
			if (degenerate(s, p, i == last)) {
				run.degenerate_failures++;
				failed = "the degenerate-state check";
			} else {
				run.jacobi_checks++;
				const int symbol = jacobi_of(r, k, p);
				if (symbol != -1) {
					run.jacobi_failures++;
					failed = std::string("the Jacobi check (symbol ") + (symbol == 1 ? "+1" : "0") + ")";
				} else if (i == last and s.is_zero() and not (before_last and confirms_prime(*before_last, p))) {
					run.confirmation_failures++;
					failed = "the prime confirmation";
				}
			}
			if (failed.empty()) {
				good = {i, k, r};
				std::fill(runs.begin(), runs.end(), 0);
				continue;
			}
			failures++;
			if (failures > CHECK_FAILURES)
				return Result::error(WORK_LL, p, failed + " failed at iteration " + std::to_string(i) + ": " +
				                     std::to_string(failures) + " failed checks in the run, past the limit of " +
				                     std::to_string(CHECK_FAILURES), run);
			logging::warning(name, failed, " failed at iteration ", i, "; going back to iteration ", good.iteration);
			restore();
		}

		if (run.large_distances > DISTANCES_LOGGED)
			logging::warning(name, run.large_distances - DISTANCES_LOGGED,
			                 " more squarings past the sizing distance, counted and not logged");

		const BigInt s = unshifted(good.residue, good.shift, p);
		if (s.is_zero())
			return Result::prime_by_residue(WORK_LL, p, run);
		return Result::composite_by_residue(WORK_LL, p, res64(s, p), run);

	} catch (engine_error const& error) {
		return Result::error(WORK_LL, p, std::string("the engine failed: ") + error.what(), run);
	}
}
