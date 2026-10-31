// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Weighted transform
//
// Squaring modulo M_p = 2^p - 1 by the irrational-base discrete weighted transform of Crandall
// and Fagin, "Discrete weighted transforms and large-integer arithmetic", 1994: a residue as
// balanced digits of widths s_j = c_(j+1) - c_j, c_j = ceil(p j / N), the digits weighted by
// a_j = 2^(c_j - p j / N), a cyclic convolution of length N by the transform of src/fft.hpp, and a
// carry that brings the unweighted, rounded outputs back to balanced digits, the carry out of the
// top word going into word 0 since 2^p = 1 modulo M_p. Theorem 1 of doc/weighted-transforms.md is
// the mathematics; the lengths each exponent may run at are src/lengths.hpp's.
//
// A Transform holds one exponent and length and takes any exponent the length can lay out; what
// takes a Transform returns statistics, never a residue. An Engine is a Transform whose length
// holds its exponent by the length table, and only an Engine forms residues. Every round-off
// distance is max |v - round(v)| over a convolution's unweighted outputs v.
//

#pragma once


// Standard libs
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Internal libs
#include "exc.hpp"
#include "fft.hpp"


// A request the engine refuses: a length it does not support or whose table does not hold the
// exponent, a residue that is not the engine's, a carry that does not settle. The message names
// the exponent and the length.
class engine_error: public mps_error {
public:
	engine_error(const std::string& msg): mps_error(msg) {}

};


// A convolution whose outputs cannot be trusted to round right: the message names the exponent,
// the length, the distance and the threshold
class roundoff_error: public engine_error {
public:
	std::uint64_t exponent;
	std::uint64_t length;
	double distance;
	double threshold;
	roundoff_error(const std::string& msg, std::uint64_t exponent, std::uint64_t length, double distance, double threshold)
		: engine_error(msg), exponent(exponent), length(length), distance(distance), threshold(threshold) {}

};


namespace ibdwt {


	// The default round-off threshold, 7/16: a distance past it is an error
	static const double THRESHOLD = 0.4375;

	// 2^51: at and above it neighbouring doubles are half an integer apart or more, so no
	// distance can be seen; an output this large is an error whatever its distance
	static const double MAGNITUDE_LIMIT = 2251799813685248.0;

	// The widest word a Transform lays out, in bits, so that digits and outputs fit int64
	static const std::uint64_t MAX_WIDTH = 50;

	// 2^(1/2): an engine refuses outputs whose root mean square passes this times the largest
	// root mean square its length's measurement saw
	static const double RMS_MARGIN = 1.4142135623730951;


	// The positions c_j = ceil(p j / n) for 0 <= j <= n
	std::vector<std::uint64_t> positions(std::uint64_t p, std::uint64_t n);

	// The weights a_j = 2^(c_j - p j / n) for 0 <= j < n
	std::vector<double> weights(std::uint64_t p, std::uint64_t n);

	// The inverse weights 1 / a_j = 2^(p j / n - c_j), each computed on its own
	std::vector<double> inverse_weights(std::uint64_t p, std::uint64_t n);


	// One exponent at one supported length: positions, weights and the transform
	class Transform {
	public:
		Transform(std::uint64_t p, std::uint64_t length);
		std::uint64_t exponent() const;
		std::uint64_t length() const;
		const std::vector<std::uint64_t>& positions() const;
		const std::vector<double>& weights() const;
		const std::vector<double>& inverse_weights() const;
		const fft::Plan& plan() const;

	private:
		std::uint64_t p_;
		std::vector<std::uint64_t> positions_;
		std::vector<double> weights_;
		std::vector<double> inverse_weights_;
		fft::Plan plan_;
	};


	// A Transform at a length the table says holds the exponent, and a round-off threshold in
	// (0, 0.5)
	class Engine {
	public:
		Engine(std::uint64_t p, std::uint64_t length, double threshold = THRESHOLD);
		std::uint64_t exponent() const;
		std::uint64_t length() const;
		double threshold() const;
		double rms_limit() const;
		const Transform& transform() const;

	private:
		Transform transform_;
		double threshold_;
		double rms_limit_;
	};


	// A residue modulo M_p as the engine's digits, one per word
	struct Residue { std::vector<std::int64_t> digits; };


	// Throw engine_error unless x has one balanced digit per word, -2^(s_j - 1) <= x_j < 2^(s_j - 1)
	void require_residue(const Transform& transform, const Residue& x);


	// The round-off distance of a convolution's outputs, max |v - round(v)|; NaN if any output
	// is not finite
	double roundoff(const std::vector<double>& outputs);


	// What the measurement sees of one squaring: its distance, its largest |v|, its root mean
	// square
	struct Statistics { double distance = 0; double magnitude = 0; double rms = 0; };


	// The statistics of squaring x, whose digits must be balanced; never the outputs or a residue
	Statistics sample(const Transform& transform, const Residue& x);

}
