// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Fast Fourier Transform
//
// The complex discrete Fourier transform of every length m 2^k with odd m up to 15, by the mixed
// radix decimation in time of Cooley and Tukey, "An algorithm for the machine calculation of
// complex Fourier series", 1965. Written for correctness and the same bits on every run: complex
// values are two doubles with their products written out, and every root of unity is computed on
// its own from an exactly reduced angle, never by a recurrence, since Percival, "Rapid
// multiplication modulo the sum and difference of highly composite numbers", 2003, bounds the
// transform's error by the error of each computed root. The mathematics is in
// doc/weighted-transforms.md.
//

#pragma once


// Standard libs
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Internal libs
#include "exc.hpp"


// A length the transform has no plan for: zero, or with an odd factor above 15
class fft_error: public mps_error {
public:
	fft_error(const std::string& msg): mps_error(msg) {}

};


namespace fft {


	// A complex number, re + i im
	struct Complex { double re = 0; double im = 0; };


	// The largest odd factor of a length the transform takes
	static const std::uint64_t LARGEST_ODD_FACTOR = 15;


	// Whether n = m 2^k with n >= 1 and odd m <= LARGEST_ODD_FACTOR
	bool is_supported(std::uint64_t n);


	// The root of unity e^(2 pi i k / n), n >= 1
	Complex root(std::uint64_t k, std::uint64_t n);


	// The transform of one length: forward, X_k = sum_j x_j e^(-2 pi i jk / n), and the
	// unnormalized inverse, x_j = sum_k X_k e^(2 pi i jk / n), so inverse(forward(x)) = n x
	class Plan {
	public:
		explicit Plan(std::uint64_t n);
		std::uint64_t size() const;
		void forward(std::vector<Complex>& data) const;
		void inverse(std::vector<Complex>& data) const;

	private:
		std::uint64_t n_;
		std::vector<std::uint64_t> radices_;  // the odd factor first, then 4s, then a 2
		std::vector<Complex> roots_;          // root(k, n) for 0 <= k < n

		template <bool INVERSE>
		void pass(const Complex* in, std::uint64_t stride, Complex* out, std::uint64_t n, std::size_t level) const;

		template <bool INVERSE>
		void run(std::vector<Complex>& data) const;
	};

}
