// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Multiple-Precision
//
// Types allowing for arbitrary precision arithmetic.
// The `BigInt` implements large integer methods.
//

#pragma once


// Standard libs
#include <iostream>
#include <string>
#include <vector>

class BigInt {
private:
	std::vector<uint16_t> digits {};

public:

	BigInt();
	BigInt(std::vector<uint16_t> const& digits);
	void load(const std::string& other);

	size_t size();
	friend std::ostream& operator<<(std::ostream& s, const BigInt& m);
};

