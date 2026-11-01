// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Checks: what the engine's two tiers share
//
// Pseudorandom digits and residues, the value of a digit vector on BigInt, the table's assignment
// of exponents to lengths, the tally a check grades its cases with, the length table held against
// its measurement, and the bodies of the checks both tiers run on the residues and the operations,
// over the lengths each tier gives them. Every expected value is computed here on BigInt or read
// from the files, never from the engine.
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
	bool checksums_agree(const Transform& transform, const Residue& x, const Residue& y, const std::vector<double>& outputs);
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



	// The supported lengths m 2^k for every odd m up to 15 and each k given, ascending
	std::vector<std::uint64_t> sample_lengths(const std::vector<std::uint64_t>& powers);


	// The exponents the table assigns a supported length
	Assigned assigned_to(std::uint64_t length);


	// A pseudorandom residue 0 <= x < M_p
	BigInt random_residue(std::uint64_t p, Random& random);


	// The outputs' round-off distance, max |v - round(v)|, and root mean square, computed here in
	// the engine's order of operations and not by roundoff or rms
	double distance_of(const std::vector<double>& outputs);
	double rms_of(const std::vector<double>& outputs);


	// The structured inputs: digit j is the fraction of the top of its balanced range times the
	// family's pattern at j, a constant (0), alternating signs (1), periods 3, 4, 5 and 6 (2 to 5),
	// a sawtooth of period 8 (6), and two tones (7, 8)
	static const int FAMILIES = 9;
	ibdwt::Residue pattern(const ibdwt::Transform& transform, int family, double fraction);


	// The SHA-256 of a sequence of residues, each by the SHA-256 of its limbs, so that two builds
	// can compare what they computed; print writes `   residues <what>: <digest>`
	class Digest {
	public:
		void add(const BigInt& x);
		void print(const std::string& what) const;

	private:
		std::string digests_;
	};


	// Round trips through the balanced digits at each length, at both ends of its assigned
	// exponents: 0, 1, M_p - 1, M_p and pseudorandom residues to digits, checked balanced and
	// valued on BigInt, and back; digit vectors at both ends of the balanced range and all ones
	// read back as their values on BigInt and round-tripped again
	void expect_round_trips(const std::vector<std::uint64_t>& lengths, std::uint64_t seed);


	// Two pseudorandom residues at each length and end, each squared, and squared less 2^0, 2^1,
	// 2^(p - 1) and a pseudorandom bit, every result equal in full to BigInt's. Each case also
	// reads back the digits it loaded, so a wrong load is graded even where, at the smallest
	// exponents, a wrong input happens to square to the right residue
	void expect_squarings(const std::vector<std::uint64_t>& lengths, std::uint64_t seed, Digest& digest);


	// Two pseudorandom pairs at each length and end multiplied, every product equal in full to
	// BigInt's, the operands read back as loaded and the second unchanged
	void expect_products(const std::vector<std::uint64_t>& lengths, std::uint64_t seed, Digest& digest);


	// At each length and end, for a square, a square less a pseudorandom bit and a product: the
	// distance reported is the outputs' own, computed here; and an engine whose threshold is half
	// that distance refuses with a roundoff_error naming the exponent, the length, the distance and
	// the threshold, leaving the residue and the operand as they were. Outputs that come out exact
	// (weights of 1 and exact roots, as at lengths 1, 2 and 4 when N divides p) are counted, not
	// refused. Every digit at the top of its range at each length's largest exponent is refused,
	// the residue left as it was, or squared right, and refused for its distance at some length
	void expect_monitor(const std::vector<std::uint64_t>& lengths, std::uint64_t seed);


	// At each length and end, two fresh engines given the same digits return the same digits and
	// distances for a square, a square less 2^1 and a product, and so does one engine asked twice
	void expect_repeats(const std::vector<std::uint64_t>& lengths, std::uint64_t seed);


	// At each length and end, the structured inputs of each family and fraction given are refused,
	// the residue left as it was, or squared equal in full to BigInt's; and refused whenever the
	// outputs' root mean square, computed here, passes the engine's limit. Returns how many were
	// refused and how many squared, so a check can expect both
	struct Outcomes { std::size_t refused = 0; std::size_t squared = 0; };
	Outcomes expect_structured(const std::vector<std::uint64_t>& lengths, const std::vector<int>& families,
	                           const std::vector<double>& fractions);

}
