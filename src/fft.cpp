// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Fast Fourier Transform
//
// Decimation in time (Cooley and Tukey, "An algorithm for the machine calculation of complex
// Fourier series", 1965): for n = r m, the r subsequences of the samples r apart are transformed
// at length m, and then each group of r values, one from each, twiddled by w_n^(qk), is replaced
// by its length-r transform. The odd factor of n is taken by the definition of the transform,
// O(r^2) for its r <= 15, the powers of 2 by radix-4 passes, whose root of unity -i is exact, and
// by one radix-2 pass when the power is odd.
//


// Standard libs
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Internal libs
#include "fft.hpp"


// pi / 4, the double nearest it
static const double QUARTER_PI = 0.78539816339744830962;


static fft::Complex times(const fft::Complex& a, const fft::Complex& b) {
	return {a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re};
}


static fft::Complex plus(const fft::Complex& a, const fft::Complex& b) {
	return {a.re + b.re, a.im + b.im};
}


static fft::Complex minus(const fft::Complex& a, const fft::Complex& b) {
	return {a.re - b.re, a.im - b.im};
}


static fft::Complex conjugate(const fft::Complex& a) {
	return {a.re, -a.im};
}


bool fft::is_supported(std::uint64_t n) {
	if (n == 0)
		return false;
	while (n % 2 == 0)
		n /= 2;
	return n <= LARGEST_ODD_FACTOR;
}


fft::Complex fft::root(std::uint64_t k, std::uint64_t n) {
	//
	// The root of unity e^(2 pi i k / n), from an angle reduced to the first octant
	//
	// With 8k = o n + r in integers (k reduced modulo n first), the angle 2 pi k / n is
	// (pi / 4)(o + r / n). cos and sin are called only on [0, pi / 4]: on (pi / 4)(r / n) in an
	// even octant, and on (pi / 4)((n - r) / n), measured back from the octant's upper end, in an
	// odd one; the octant's symmetry places the value. Each root is so one evaluation of cos and
	// sin on an argument with one rounding in it, which is what Percival, "Rapid multiplication
	// modulo the sum and difference of highly composite numbers", 2003, asks of the roots for his
	// error bound; the axes come out exact.

	if (n == 0)
		throw fft_error("no root of unity of order 0");
	k %= n;
	std::uint64_t o = (8 * k) / n, r = (8 * k) % n;
	bool odd = o % 2 == 1;
	double phi = QUARTER_PI * (double(odd ? n - r : r) / double(n));
	double c = std::cos(phi), s = std::sin(phi);
	switch (o) {
		case 0: return {c, s};
		case 1: return {s, c};
		case 2: return {-s, c};
		case 3: return {-c, s};
		case 4: return {-c, -s};
		case 5: return {-s, -c};
		case 6: return {s, -c};
		default: return {c, -s};
	}
}


fft::Plan::Plan(std::uint64_t n): n_(n) {
	//
	// The radices of n, odd factor first, and its n roots of unity

	if (not is_supported(n))
		throw fft_error("no transform of length " + std::to_string(n) + ": its odd factor is above "
		                + std::to_string(LARGEST_ODD_FACTOR));
	std::uint64_t odd = n, twos = 0;
	while (odd % 2 == 0) {
		odd /= 2;
		twos++;
	}
	if (odd > 1)
		radices_.push_back(odd);
	for (; twos >= 2; twos -= 2)
		radices_.push_back(4);
	if (twos == 1)
		radices_.push_back(2);
	roots_.reserve(n);
	for (std::uint64_t k = 0; k < n; k++)
		roots_.push_back(root(k, n));
}


std::uint64_t fft::Plan::size() const {
	return n_;
}


template <bool INVERSE>
void fft::Plan::pass(const Complex* in, std::uint64_t stride, Complex* out, std::uint64_t n, std::size_t level) const {
	//
	// The transform of the n samples in[0], in[stride], ... into out[0 .. n - 1]
	//
	// Out of place: the r subsequences go to the r blocks of m = n / r in out, each transformed
	// in turn, and then each group of r values k apart is twiddled and transformed in place. The
	// roots are w_n^j = roots_[j N / n] (N the plan's length), conjugated forward.

	std::uint64_t r = radices_[level], m = n / r;
	if (m == 1) {
		for (std::uint64_t q = 0; q < r; q++)
			out[q] = in[q * stride];
	} else {
		for (std::uint64_t q = 0; q < r; q++)
			pass<INVERSE>(in + q * stride, stride * r, out + q * m, m, level + 1);
	}

	const std::uint64_t step = n_ / n;
	auto w = [&](std::uint64_t j) {
		return INVERSE ? roots_[j] : conjugate(roots_[j]);
	};
	if (r == 2) {
		for (std::uint64_t k = 0; k < m; k++) {
			Complex a = out[k], b = times(out[m + k], w(k * step));
			out[k] = plus(a, b);
			out[m + k] = minus(a, b);
		}
	} else if (r == 4) {
		for (std::uint64_t k = 0; k < m; k++) {
			Complex t0 = out[k];
			Complex t1 = times(out[m + k], w(k * step));
			Complex t2 = times(out[2 * m + k], w(2 * k * step));
			Complex t3 = times(out[3 * m + k], w(3 * k * step));
			Complex a0 = plus(t0, t2), a1 = minus(t0, t2), b0 = plus(t1, t3), d = minus(t1, t3);
			// (t1 - t3) times w_4 = -i forward, +i inverse: exact
			Complex b1 = INVERSE ? Complex{-d.im, d.re} : Complex{d.im, -d.re};
			out[k] = plus(a0, b0);
			out[m + k] = plus(a1, b1);
			out[2 * m + k] = minus(a0, b0);
			out[3 * m + k] = minus(a1, b1);
		}
	} else {
		// The odd factor, by the definition: y_s = sum_q t_q w_r^(qs), w_r^j = roots_[j N / r]
		Complex t[LARGEST_ODD_FACTOR];
		const std::uint64_t unit = n_ / r;
		for (std::uint64_t k = 0; k < m; k++) {
			for (std::uint64_t q = 0; q < r; q++)
				t[q] = times(out[q * m + k], w(q * k * step));
			for (std::uint64_t s = 0; s < r; s++) {
				Complex y = t[0];
				for (std::uint64_t q = 1; q < r; q++)
					y = plus(y, times(t[q], w(((q * s) % r) * unit)));
				out[s * m + k] = y;
			}
		}
	}
}


template <bool INVERSE>
void fft::Plan::run(std::vector<Complex>& data) const {
	if (data.size() != n_)
		throw fft_error("a transform of length " + std::to_string(n_) + " given " + std::to_string(data.size()) + " values");
	if (n_ == 1)
		return;
	const std::vector<Complex> in = data;
	pass<INVERSE>(in.data(), 1, data.data(), n_, 0);
}


void fft::Plan::forward(std::vector<Complex>& data) const {
	run<false>(data);
}


void fft::Plan::inverse(std::vector<Complex>& data) const {
	run<true>(data);
}
