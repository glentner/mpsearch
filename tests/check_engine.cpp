// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Checks: the weighted transform's engine tier
//
// The engine at every supported length, which takes minutes rather than seconds: `bin/check
// engine` runs these, and `make check-engine` runs them after the default tier. The residues and
// the operations are checked as the default tier checks them at its sample lengths, here at all of
// them, and the squarings and products print a digest of their residues, which a clang build and
// a GCC build must share. Every expected value is computed here or in tests/engine_support.cpp,
// never by the engine.
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


static void residues_round_trip_at_every_supported_length() {
	engine_support::expect_round_trips(ibdwt::supported_lengths(), 0x7e51d0e6);
}


static void squarings_equal_bigints_at_every_supported_length() {
	engine_support::Digest digest;
	engine_support::expect_squarings(ibdwt::supported_lengths(), 0x5917a7e6, digest);
	digest.print("squarings");
}


static void products_equal_bigints_at_every_supported_length() {
	engine_support::Digest digest;
	engine_support::expect_products(ibdwt::supported_lengths(), 0x9a0d0c76, digest);
	digest.print("products");
}


static void the_round_off_monitor_reports_and_refuses_at_every_supported_length() {
	engine_support::expect_monitor(ibdwt::supported_lengths(), 0x3017a0e6);
}


static void the_engine_repeats_itself_at_every_supported_length() {
	engine_support::expect_repeats(ibdwt::supported_lengths(), 0x0e9ea7ee);
}


static void structured_inputs_are_refused_or_squared_right_at_every_supported_length() {
	//
	// Every family at ten fractions of the digits' range up to 2^14 words; above, the constant and
	// the alternating digits at half and all of it

	std::vector<std::uint64_t> small, large;
	for (std::uint64_t n: ibdwt::supported_lengths())
		(n <= 16'384 ? small : large).push_back(n);
	engine_support::Outcomes below = engine_support::expect_structured(small, {0, 1, 2, 3, 4, 5, 6, 7, 8},
	                                                                   {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0});
	engine_support::Outcomes above = engine_support::expect_structured(large, {0, 1}, {0.5, 1.0});
	check::expect(below.refused + above.refused > 0 and below.squared + above.squared > 0, "structured inputs both refused and squared");
}


static check::Suite
engine_check_by_name {check::Tier::ENGINE, {
	{"every supported length is accepted at both its assigned exponents",          every_supported_length_is_accepted_at_both_its_assigned_exponents},
	{"residues round-trip at every supported length",                              residues_round_trip_at_every_supported_length},
	{"squarings equal BigInt's at every supported length",                         squarings_equal_bigints_at_every_supported_length},
	{"products equal BigInt's at every supported length",                          products_equal_bigints_at_every_supported_length},
	{"the round-off monitor reports and refuses at every supported length",        the_round_off_monitor_reports_and_refuses_at_every_supported_length},
	{"the engine repeats itself at every supported length",                        the_engine_repeats_itself_at_every_supported_length},
	{"structured inputs are refused or squared right at every supported length",   structured_inputs_are_refused_or_squared_right_at_every_supported_length},
}};
