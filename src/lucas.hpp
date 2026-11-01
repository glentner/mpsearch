// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// The Lucas-Lehmer test
//
// The test of M_p = 2^p - 1 on the FP64 weighted-transform engine (src/ibdwt.hpp): s_0 = 4 and
// s_(i+1) = s_i^2 - 2 modulo M_p, held shifted as 2^k s_i, with the degenerate-state check, the
// Jacobi check and the prime confirmation of doc/lucas-lehmer.md. A failed check or a refused
// squaring sends the run back to its last good state, held in memory; the result is a verdict
// only when every check of a complete run passed, and otherwise an error.
//

#pragma once


// Standard libs
#include <cstdint>
#include <functional>
#include <optional>

// Internal libs
#include "mp.hpp"
#include "result.hpp"


namespace lucas {


	// The run checks its residue, and keeps it as its good state when it passes, at this many
	// points spread evenly over its iterations, and at the last
	static const std::uint64_t GOOD_STATES = 32;

	// Failed error checks a run recovers from; one more ends it in an error
	static const std::uint64_t CHECK_FAILURES = 3;

	// Moves to a larger length a run makes when a refusal reproduces; one more ends it in an error
	static const std::uint64_t LENGTH_RETRIES = 3;

	// Seconds between two progress lines
	static const double PROGRESS_SECONDS = 30.0;

	// Accepted squarings past the sizing distance logged one by one; the rest are only counted
	static const std::uint64_t DISTANCES_LOGGED = 8;


	// One squaring of a run, as the checks' hooks see it
	struct Step {
		std::uint64_t iteration;  // the squaring that makes s_iteration, 1 <= iteration <= p - 2
		std::uint64_t attempt;    // how many times this iteration ran before in this run
		std::uint64_t length;     // the length it runs at
		std::uint64_t shift;      // the shift of the residue it makes
	};


	// How a run goes. bin/mps takes the defaults; the checks give a shift, a length and hooks.
	struct Options {
		std::optional<std::uint64_t> shift {};    // the shift of s_0; drawn at random when empty
		std::optional<std::uint64_t> length {};   // the first length; the table's for p when empty
		double progress_seconds = PROGRESS_SECONDS;

		// The checks' injections, which bin/mps never sets. `refuse` runs before each squaring and
		// may throw roundoff_error, as the engine does, leaving the residue as it was; `corrupt`
		// runs after each accepted squaring and may return a residue to hold in place of the one
		// made (as held: shifted, modulo M_p); `distance` may replace the round-off distance an
		// accepted squaring reported.
		std::function<void(const Step&)> refuse {};
		std::function<std::optional<BigInt>(const Step&, const BigInt&)> corrupt {};
		std::function<double(const Step&, double)> distance {};
	};


	// P_max: the largest exponent any supported length holds
	std::uint64_t largest_exponent();


	// s = 2^(-k) r mod M_p, the value a residue r held at shift k stands for
	BigInt unshifted(const BigInt& r, std::uint64_t k, std::uint64_t p);


	// J(s - 2 | M_p) for the residue r held at shift k, from r - 2^((k + 1) mod p)
	int jacobi_of(const BigInt& r, std::uint64_t k, std::uint64_t p);


	// Whether s is 1, -1, 2 or -2 modulo M_p, or 0 before the last iteration
	bool degenerate(const BigInt& s, std::uint64_t p, bool last);


	// Whether s_(p-3) is 2^((p+1)/2) or -2^((p+1)/2) modulo M_p, as it must be when s_(p-2) is 0
	bool confirms_prime(const BigInt& before_last, std::uint64_t p);


	// The test of M_p
	Result test(std::uint64_t p, const Options& options = {});

}
