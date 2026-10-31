// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Checks: what the engine's two tiers share
//
// Pseudorandom digits, the value of a digit vector on BigInt, the table's assignment of exponents
// to lengths, the tally a check grades its cases with, and the length table held against its
// measurement. Every expected value is computed here on BigInt or read from the files, never
// from the engine.
//

#pragma once


// Standard libs
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string>
#include <vector>

// Internal libs
#include "ibdwt.hpp"
#include "mp.hpp"


// The engine's internal pieces, in no header of src/: declared here for the checks
namespace ibdwt {
	std::vector<double> convolve(const Transform& transform, const Residue& x, const Residue& y);
	std::vector<double> convolve_square(const Transform& transform, const Residue& x);
	Residue carry(const Engine& engine, const std::vector<double>& outputs, bool subtract, std::uint64_t bit);
	double rms(const std::vector<double>& outputs);
}


namespace engine_support {


	// The measurement the length table is set from, from the repository root
	extern const std::string MEASUREMENT;


	// A check's verdict over its cases: each case agrees or not, the first few that do not are
	// printed, and expect_none prints `   tally <what>: <K> cases, <W> disagree` whatever the
	// outcome and then expects, as `all <K> <what> to agree (<W> did not)`, that there were cases
	// and none disagreed
	class Tally {
	public:
		explicit Tally(const std::string& what);
		void add(bool agree, const std::string& detail = "");
		void expect_none(std::source_location location = std::source_location::current());

	private:
		std::string what_;
		std::size_t cases_ = 0;
		std::size_t disagree_ = 0;
	};


	// SplitMix64, the checks' own pseudorandom numbers (Steele, Lea and Flood, 2014)
	class Random {
	public:
		explicit Random(std::uint64_t seed);
		std::uint64_t next();

	private:
		std::uint64_t state_;
	};


	// Pseudorandom balanced digits for a transform: digit j uniform in [-2^(s_j - 1), 2^(s_j - 1))
	ibdwt::Residue random_digits(const ibdwt::Transform& transform, Random& random);


	// Whether x has one digit per word, each in its balanced range
	bool balanced(const ibdwt::Transform& transform, const ibdwt::Residue& x);


	// sum_j d_j 2^(c_j) mod M_p, for any int64 digits, by placing each digit's bits in limbs
	BigInt value_of(const std::vector<std::uint64_t>& positions, const std::vector<std::int64_t>& digits, std::uint64_t p);


	// (a - 2^bit) mod M_p
	BigInt less_power(const BigInt& a, std::uint64_t bit, std::uint64_t p);


	// A supported length and the exponents the table assigns it: those it is the smallest
	// length to hold, first to last
	struct Assigned {
		std::uint64_t length;
		std::uint64_t first;
		std::uint64_t last;
	};


	// Every supported length's assigned exponents, ascending; first > last if it has none
	std::vector<Assigned> assigned();


	// The supported length after this one, or 0 after the largest
	std::uint64_t next_length(std::uint64_t length);


	// The length table against its measurement: the format, every length's probes, the table's
	// exponent and envelope from the probe the protocol names, every length assigned an exponent,
	// length_for the smallest holder, and P_max at least 10,000,019
	void expect_table_matches(const std::string& path);

}
