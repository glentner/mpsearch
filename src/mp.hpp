// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Multiple-Precision
//
// `BigInt`, the reference arithmetic every engine is checked against: a non-negative integer of
// 64-bit limbs, written for clarity first, and its operations as free functions. The mathematics
// is in doc/reference-arithmetic.md.
//

#pragma once


// Standard libs
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Internal libs
#include "exc.hpp"


// An operation given an argument outside its domain: a difference below zero, a zero modulus, a
// string that is not a number in its base, or an exponent outside a reference test's theorem
class mp_error: public mps_error {
public:
	mp_error(const std::string& msg): mps_error(msg) {}

};


// From this many limbs in the smaller operand up, products and squares split by Karatsuba's method
// rather than the schoolbook one. How the value was chosen, measured on this code when it was set:
// one split paid from about 72 limbs for a product and 132 for a square, whose schoolbook form
// does half the work, and over both together the full recursion was fastest, within the noise,
// from 96 to 104.
static const std::size_t KARATSUBA_THRESHOLD = 96;


class BigInt {
public:
	BigInt();
	BigInt(std::uint64_t value);
	static BigInt from_limbs(const std::vector<std::uint64_t>& limbs);

	const std::vector<std::uint64_t>& limbs() const;
	bool is_zero() const;
	std::size_t bit_length() const;

private:
	std::vector<std::uint64_t> limbs_;  // little-endian, no high zero limb; empty is 0
};


// x = quotient * m + remainder, 0 <= remainder < m
struct Division {
	BigInt quotient;
	BigInt remainder;
};


int compare(const BigInt& a, const BigInt& b);
bool operator==(const BigInt& a, const BigInt& b);

BigInt add(const BigInt& a, const BigInt& b);
BigInt subtract(const BigInt& a, const BigInt& b);
BigInt shift_left(const BigInt& a, std::size_t bits);
BigInt shift_right(const BigInt& a, std::size_t bits);

BigInt multiply_schoolbook(const BigInt& a, const BigInt& b);
BigInt multiply_karatsuba(const BigInt& a, const BigInt& b, std::size_t cutoff = KARATSUBA_THRESHOLD);
BigInt multiply(const BigInt& a, const BigInt& b);
BigInt square_schoolbook(const BigInt& a);
BigInt square_karatsuba(const BigInt& a, std::size_t cutoff = KARATSUBA_THRESHOLD);
BigInt square(const BigInt& a);

BigInt mersenne(long unsigned p);
BigInt reduce_mersenne(const BigInt& x, long unsigned p);

Division divide(const BigInt& x, const BigInt& m);
BigInt reduce(const BigInt& x, const BigInt& m);
BigInt power_mod(const BigInt& base, const BigInt& exponent, const BigInt& m);

std::string hex64(std::uint64_t value);
std::uint64_t res64(const BigInt& x, long unsigned p);

std::string to_hex(const BigInt& x);
BigInt from_hex(const std::string& text);

std::string to_decimal(const BigInt& x);
BigInt from_decimal(const std::string& text);
