// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Multiple-Precision
//


// Standard libs
#include <iostream>
#include <string>
#include <vector>

// Internal libs
#include "mp.hpp"


BigInt::BigInt() {}

BigInt::BigInt(std::vector<uint16_t> const& digits) {
	this -> digits = digits;
}

void BigInt::load(const std::string& str) {
	//
	// Load from existing string
	//
	// Clear previous data and load digits from `str`.
	// Characters and converted to integers.

	digits.clear();
	digits.reserve(str.size());
	for (const char& c: str) {
		digits.push_back(c - '0');
	}
}

size_t BigInt::size() {
	//
	// Number of digits
	//
	// A count of the digits in the number.
	// This is not the memory size.

	return digits.size();
}

std::ostream& operator<<(std::ostream& s, const BigInt& m) {
	//
	// Write digits to stream

	for (const uint16_t& n: m.digits) {
		s << n;
	}
	return s;
}

