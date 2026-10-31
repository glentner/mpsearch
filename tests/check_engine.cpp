// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Checks: the weighted transform's engine tier
//
// The engine at every supported length, which takes minutes rather than seconds: `bin/check
// engine` runs these, and `make check-engine` runs them after the default tier. Every expected
// value is computed here or in tests/engine_support.cpp, never by the engine.
//


// Standard libs
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "engine_support.hpp"
#include "ibdwt.hpp"
#include "lengths.hpp"


using engine_support::Tally;


static std::string at(std::uint64_t p, std::uint64_t n) {
	return "exponent " + std::to_string(p) + " at length " + std::to_string(n);
}


static void every_supported_length_is_accepted_at_both_its_assigned_exponents() {
	//
	// At each of the 144 lengths, an engine at the smallest and at the largest exponent the table
	// assigns it, each the length length_for gives, and a squaring of pseudorandom balanced digits
	// whose outputs are within the limits: distance at most the threshold, every |output| below
	// MAGNITUDE_LIMIT, the root mean square within the engine's limit

	engine_support::Random random(0xacce97ed);
	Tally tally("engines and squarings within the limits");
	for (const engine_support::Assigned& span: engine_support::assigned()) {
		for (std::uint64_t p: {span.first, span.last}) {
			std::string wrong;
			try {
				ibdwt::Engine engine(p, span.length);
				const ibdwt::Transform& transform = engine.transform();
				std::vector<double> v = ibdwt::convolve_square(transform, engine_support::random_digits(transform, random));
				double distance = 0, magnitude = 0, sum = 0;
				for (double value: v) {
					distance = std::max(distance, std::fabs(value - std::round(value)));
					magnitude = std::max(magnitude, std::fabs(value));
					sum += value * value;
				}
				double size = std::sqrt(sum / double(v.size()));
				if (ibdwt::length_for(p) != span.length)
					wrong = "length_for gives " + std::to_string(ibdwt::length_for(p));
				else if (v.size() != span.length or not (distance <= engine.threshold()))
					wrong = "distance " + std::to_string(distance);
				else if (not (magnitude < ibdwt::MAGNITUDE_LIMIT))
					wrong = "magnitude " + std::to_string(magnitude);
				else if (not (size <= engine.rms_limit()))
					wrong = "root mean square " + std::to_string(size) + " past " + std::to_string(engine.rms_limit());
			} catch (std::exception const& error) {
				wrong = std::string("raised ") + error.what();
			}
			tally.add(wrong.empty(), at(p, span.length) + ": " + wrong);
		}
	}
	tally.expect_none();
}


static check::Suite
engine_check_by_name {check::Tier::ENGINE, {
	{"every supported length is accepted at both its assigned exponents",  every_supported_length_is_accepted_at_both_its_assigned_exponents},
}};
