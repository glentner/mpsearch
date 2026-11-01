// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Transform lengths
//
// Which lengths the engine supports, and which exponents each holds. A length holds an exponent
// when the table, set by measurement, says so: each supported length's largest exponent is a
// probe of pseudorandom balanced digits whose squarings stayed within the sizing limits below,
// a margin below the largest probe that did. The protocol is pinned here, the measurement and the
// command that made it are src/lengths.txt and src/measure.cpp (`make lengths`), and the table
// in src/lengths.cpp holds what that measurement printed.
//

#pragma once


// Standard libs
#include <cstddef>
#include <cstdint>
#include <vector>


namespace ibdwt {


	// The largest supported length, 2^20 words
	static const std::uint64_t LARGEST_LENGTH = 1'048'576;

	// A probe holds when the largest round-off distance of its squarings is at most this
	static const double SIZING_DISTANCE = 0.35;

	// and their largest |output| is below this, 2^50, one bit inside MAGNITUDE_LIMIT
	static const double SIZING_MAGNITUDE = 1125899906842624.0;


	// The protocol. A probe of (length N, exponent p) squares at least PROBE_SQUARINGS vectors of
	// fresh pseudorandom balanced digits, and enough of them for PROBE_OUTPUTS outputs, ending at
	// the first squaring past a sizing limit. A length's probes are the exponents
	// ceil(N i / GRID_STEPS), consecutive, from START_BELOW bits a word below the Crandall,
	// Mayer and Papadopoulos estimate up to the first probe that fails; with fewer than
	// BACKOFF + 1 probes held before it, the start moves down a bit and the length is measured
	// again. The length's largest exponent is the probe BACKOFF grid points below the last held,
	// and its root-mean-square envelope that probe's largest.
	static const std::uint64_t PROBE_SQUARINGS = 64;
	static const std::uint64_t PROBE_OUTPUTS = 4'194'304;
	static const int GRID_STEPS = 16;
	static const double START_BELOW = 1.0;
	static const std::size_t BACKOFF = 2;


	// Whether length = m 2^k <= LARGEST_LENGTH with odd m up to 15
	bool is_supported(std::uint64_t length);

	// Every supported length, ascending
	std::vector<std::uint64_t> supported_lengths();

	// The largest exponent the table gives a supported length
	std::uint64_t largest_exponent(std::uint64_t length);

	// Whether the length is supported and max(3, length) <= p <= its largest exponent
	bool holds(std::uint64_t length, std::uint64_t p);

	// Throw engine_error, naming both, unless the length holds p
	void require_held(std::uint64_t length, std::uint64_t p);

	// The smallest supported length that holds p
	std::uint64_t length_for(std::uint64_t p);

	// The largest root mean square of the outputs the measurement saw at the length's largest
	// exponent
	double rms_envelope(std::uint64_t length);

}
