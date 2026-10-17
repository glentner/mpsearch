// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Multiple-Precision
//
// The algorithms are the textbook ones, from Brent and Zimmermann, "Modern Computer Arithmetic",
// 2010 (MCA below), each citing its source. The mathematics, and the proofs of what is the
// project's own, are in doc/reference-arithmetic.md.
//


// Standard libs
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Internal libs
#include "mp.hpp"


// The one spelling of the double-width type, for limb sums and products
using u128 = unsigned __int128;


static const std::uint64_t ALL_ONES = ~std::uint64_t(0);


// 10^19, the largest power of 10 below 2^64, and its digits: decimal text moves 19 digits a limb
static const std::uint64_t TEN_19 = 10'000'000'000'000'000'000ull;
static const std::size_t DECIMAL_LIMB = 19;


BigInt::BigInt() {}


BigInt::BigInt(std::uint64_t value) {
	if (value != 0)
		limbs_.push_back(value);
}


BigInt BigInt::from_limbs(const std::vector<std::uint64_t>& limbs) {
	//
	// A value from little-endian limbs
	//
	// High zero limbs are dropped, so every value has one representation.

	BigInt x;
	x.limbs_ = limbs;
	while (not x.limbs_.empty() and x.limbs_.back() == 0)
		x.limbs_.pop_back();
	return x;
}


const std::vector<std::uint64_t>& BigInt::limbs() const {
	return limbs_;
}


bool BigInt::is_zero() const {
	return limbs_.empty();
}


std::size_t BigInt::bit_length() const {
	//
	// The number of bits up to the highest one bit, 0 for zero

	if (limbs_.empty())
		return 0;
	return 64 * limbs_.size() - std::countl_zero(limbs_.back());
}


int compare(const BigInt& a, const BigInt& b) {
	//
	// -1, 0 or 1 as a is below, equal to or above b
	//
	// No value has a high zero limb, so the longer is the larger; equal lengths compare from the
	// top limb down (MCA, chapter 1).

	const std::vector<std::uint64_t>& x = a.limbs();
	const std::vector<std::uint64_t>& y = b.limbs();
	if (x.size() != y.size())
		return x.size() < y.size() ? -1 : 1;
	for (std::size_t i = x.size(); i-- > 0;)
		if (x[i] != y[i])
			return x[i] < y[i] ? -1 : 1;
	return 0;
}


bool operator==(const BigInt& a, const BigInt& b) {
	return compare(a, b) == 0;
}


BigInt add(const BigInt& a, const BigInt& b) {
	//
	// a + b
	//
	// Limb by limb from the bottom, the carry held in a double-width sum (MCA, 1.2).

	const std::vector<std::uint64_t>& x = a.limbs();
	const std::vector<std::uint64_t>& y = b.limbs();
	std::size_t n = std::max(x.size(), y.size());
	std::vector<std::uint64_t> out(n + 1, 0);
	u128 carry = 0;
	for (std::size_t i = 0; i < n; i++) {
		u128 sum = carry;
		if (i < x.size())
			sum += x[i];
		if (i < y.size())
			sum += y[i];
		out[i] = std::uint64_t(sum);
		carry = sum >> 64;
	}
	out[n] = std::uint64_t(carry);
	return BigInt::from_limbs(out);
}


BigInt subtract(const BigInt& a, const BigInt& b) {
	//
	// a - b, an error when b exceeds a
	//
	// Limb by limb from the bottom, the borrow found by comparing before each step rather than
	// from the wrapped result (MCA, 1.2). A difference below zero throws and returns nothing.

	if (compare(a, b) < 0)
		throw mp_error("difference below zero");

	const std::vector<std::uint64_t>& x = a.limbs();
	const std::vector<std::uint64_t>& y = b.limbs();
	std::vector<std::uint64_t> out(x.size(), 0);
	std::uint64_t borrow = 0;
	for (std::size_t i = 0; i < x.size(); i++) {
		std::uint64_t yi = i < y.size() ? y[i] : 0;
		std::uint64_t difference = x[i] - yi;
		std::uint64_t below = x[i] < yi;
		out[i] = difference - borrow;
		borrow = below + (difference < borrow);
	}
	return BigInt::from_limbs(out);
}


BigInt shift_left(const BigInt& a, std::size_t bits) {
	//
	// a * 2^bits
	//
	// By bits / 64 whole limbs and bits % 64 bits. A whole number of limbs is its own branch, so no
	// limb is ever shifted by 64, which C++ leaves undefined.

	if (a.is_zero())
		return a;
	const std::vector<std::uint64_t>& x = a.limbs();
	std::size_t whole = bits / 64, part = bits % 64;
	std::vector<std::uint64_t> out(x.size() + whole + 1, 0);
	if (part == 0) {
		for (std::size_t i = 0; i < x.size(); i++)
			out[i + whole] = x[i];
	} else {
		for (std::size_t i = 0; i < x.size(); i++) {
			out[i + whole] |= x[i] << part;
			out[i + whole + 1] = x[i] >> (64 - part);
		}
	}
	return BigInt::from_limbs(out);
}


BigInt shift_right(const BigInt& a, std::size_t bits) {
	//
	// floor(a / 2^bits)
	//
	// Split as for the left shift; a shift past the length gives 0.

	const std::vector<std::uint64_t>& x = a.limbs();
	std::size_t whole = bits / 64, part = bits % 64;
	if (whole >= x.size())
		return BigInt();
	std::vector<std::uint64_t> out(x.size() - whole, 0);
	if (part == 0) {
		for (std::size_t i = whole; i < x.size(); i++)
			out[i - whole] = x[i];
	} else {
		for (std::size_t i = whole; i < x.size(); i++) {
			out[i - whole] = x[i] >> part;
			if (i + 1 < x.size())
				out[i - whole] |= x[i + 1] << (64 - part);
		}
	}
	return BigInt::from_limbs(out);
}


static BigInt low_limbs(const BigInt& a, std::size_t n) {
	//
	// a mod 2^(64 n), the low n limbs of a

	const std::vector<std::uint64_t>& x = a.limbs();
	return BigInt::from_limbs(std::vector<std::uint64_t>(x.begin(), x.begin() + std::min(n, x.size())));
}


BigInt multiply_schoolbook(const BigInt& a, const BigInt& b) {
	//
	// a * b, the schoolbook way
	//
	// One row per limb of a: that limb times every limb of b, added into the result at its offset,
	// the carry held in a double-width product, and the row's last carry stored above it (MCA,
	// Algorithm 1.2 BasecaseMultiply).

	const std::vector<std::uint64_t>& x = a.limbs();
	const std::vector<std::uint64_t>& y = b.limbs();
	if (x.empty() or y.empty())
		return BigInt();
	std::vector<std::uint64_t> out(x.size() + y.size(), 0);
	for (std::size_t i = 0; i < x.size(); i++) {
		u128 carry = 0;
		for (std::size_t j = 0; j < y.size(); j++) {
			u128 t = u128(x[i]) * y[j] + out[i + j] + carry;
			out[i + j] = std::uint64_t(t);
			carry = t >> 64;
		}
		out[i + y.size()] = std::uint64_t(carry);
	}
	return BigInt::from_limbs(out);
}


BigInt multiply_karatsuba(const BigInt& a, const BigInt& b, std::size_t cutoff) {
	//
	// a * b by Karatsuba's method, down to cutoff limbs
	//
	// The additive variant (Karatsuba and Ofman 1962; MCA, 1.3.2). With k half the longer
	// operand's limbs, rounded up, and B = 2^(64 k), write a = a1 B + a0 and b = b1 B + b0, a high
	// part being 0 where an operand has at most k limbs. Then a b = z2 B^2 + z1 B + z0 with
	// z0 = a0 b0, z2 = a1 b1 and z1 = (a0 + a1)(b0 + b1) - z0 - z2 = a0 b1 + a1 b0, which is never
	// below zero. The three products recurse; when the shorter operand has fewer than cutoff limbs,
	// or fewer than 4, the product is the schoolbook one. From 4 limbs up every operand of the
	// three products is shorter than the longer operand here, so the recursion ends.

	std::size_t na = a.limbs().size(), nb = b.limbs().size();
	if (std::min(na, nb) < std::max<std::size_t>(cutoff, 4))
		return multiply_schoolbook(a, b);
	std::size_t k = (std::max(na, nb) + 1) / 2;
	BigInt a0 = low_limbs(a, k), a1 = shift_right(a, 64 * k);
	BigInt b0 = low_limbs(b, k), b1 = shift_right(b, 64 * k);
	BigInt z0 = multiply_karatsuba(a0, b0, cutoff);
	BigInt z2 = multiply_karatsuba(a1, b1, cutoff);
	BigInt sum = multiply_karatsuba(add(a0, a1), add(b0, b1), cutoff);
	BigInt z1 = subtract(subtract(sum, z0), z2);
	return add(add(z0, shift_left(z1, 64 * k)), shift_left(z2, 128 * k));
}


BigInt multiply(const BigInt& a, const BigInt& b) {
	//
	// a * b
	//
	// Karatsuba's method from KARATSUBA_THRESHOLD limbs in the shorter operand up, the schoolbook
	// product below.

	if (std::min(a.limbs().size(), b.limbs().size()) >= KARATSUBA_THRESHOLD)
		return multiply_karatsuba(a, b, KARATSUBA_THRESHOLD);
	return multiply_schoolbook(a, b);
}


BigInt square_schoolbook(const BigInt& a) {
	//
	// a^2, the symmetric schoolbook way
	//
	// Every cross product a_i a_j B^(i+j) with i < j once, row by row as in the product, the sum
	// doubled by a one-bit shift of the whole number, plus the diagonal a_i^2 B^(2i), with B = 2^64
	// (MCA, 1.3.6). That is about half the limb products of multiply_schoolbook(a, a).

	const std::vector<std::uint64_t>& x = a.limbs();
	if (x.empty())
		return BigInt();
	std::size_t n = x.size();
	std::vector<std::uint64_t> cross(2 * n, 0);
	for (std::size_t i = 0; i < n; i++) {
		u128 carry = 0;
		for (std::size_t j = i + 1; j < n; j++) {
			u128 t = u128(x[i]) * x[j] + cross[i + j] + carry;
			cross[i + j] = std::uint64_t(t);
			carry = t >> 64;
		}
		cross[i + n] = std::uint64_t(carry);
	}
	std::vector<std::uint64_t> diagonal(2 * n, 0);
	for (std::size_t i = 0; i < n; i++) {
		u128 t = u128(x[i]) * x[i];
		diagonal[2 * i] = std::uint64_t(t);
		diagonal[2 * i + 1] = std::uint64_t(t >> 64);
	}
	return add(shift_left(BigInt::from_limbs(cross), 1), BigInt::from_limbs(diagonal));
}


BigInt square_karatsuba(const BigInt& a, std::size_t cutoff) {
	//
	// a^2 by Karatsuba's method, down to cutoff limbs
	//
	// The product's identity with b = a (Karatsuba and Ofman 1962; MCA, 1.3.2): a^2 = z2 B^2 +
	// z1 B + z0 with z0 = a0^2, z2 = a1^2 and z1 = (a0 + a1)^2 - z0 - z2 = 2 a0 a1, three squares
	// that recurse, split and ended as in multiply_karatsuba, with square_schoolbook below.

	std::size_t n = a.limbs().size();
	if (n < std::max<std::size_t>(cutoff, 4))
		return square_schoolbook(a);
	std::size_t k = (n + 1) / 2;
	BigInt a0 = low_limbs(a, k), a1 = shift_right(a, 64 * k);
	BigInt z0 = square_karatsuba(a0, cutoff);
	BigInt z2 = square_karatsuba(a1, cutoff);
	BigInt sum = square_karatsuba(add(a0, a1), cutoff);
	BigInt z1 = subtract(subtract(sum, z0), z2);
	return add(add(z0, shift_left(z1, 64 * k)), shift_left(z2, 128 * k));
}


BigInt square(const BigInt& a) {
	//
	// a^2
	//
	// Karatsuba's method from KARATSUBA_THRESHOLD limbs up, the schoolbook square below.

	if (a.limbs().size() >= KARATSUBA_THRESHOLD)
		return square_karatsuba(a, KARATSUBA_THRESHOLD);
	return square_schoolbook(a);
}


static std::vector<std::uint64_t> ones(long unsigned p) {
	//
	// The limbs of 2^p - 1: p / 64 limbs of all ones, then the p % 64 low bits of one more

	std::vector<std::uint64_t> out(p / 64, ALL_ONES);
	if (p % 64 != 0)
		out.push_back(ALL_ONES >> (64 - p % 64));
	return out;
}


BigInt mersenne(long unsigned p) {
	//
	// M_p = 2^p - 1, and 0 for p = 0

	return BigInt::from_limbs(ones(p));
}


static BigInt low_bits(const BigInt& a, long unsigned p) {
	//
	// a mod 2^p, a masked by the limbs of 2^p - 1

	std::vector<std::uint64_t> mask = ones(p);
	const std::vector<std::uint64_t>& x = a.limbs();
	if (x.size() < mask.size())
		return a;
	std::vector<std::uint64_t> out(mask.size());
	for (std::size_t i = 0; i < mask.size(); i++)
		out[i] = x[i] & mask[i];
	return BigInt::from_limbs(out);
}


BigInt reduce_mersenne(const BigInt& x, long unsigned p) {
	//
	// x mod M_p, by folding
	//
	// Since 2^p = 1 modulo M_p, x = (x mod 2^p) + floor(x / 2^p) modulo M_p, and each such fold
	// lowers x by floor(x / 2^p) M_p. Folding while x has more than p bits ends with x <= M_p,
	// after at most two folds for x < 2^(2p), and M_p itself is then 0 (the project's own
	// derivation, in doc/reference-arithmetic.md; MCA, 2.4.4, for the special moduli it belongs
	// to). M_p is the one value of at most p bits with all p of them set. p = 0 throws.

	if (p == 0)
		throw mp_error("modulus M_0 = 0");
	BigInt r = x;
	while (r.bit_length() > p)
		r = add(low_bits(r, p), shift_right(r, p));
	std::size_t set = 0;
	for (std::uint64_t limb: r.limbs())
		set += std::popcount(limb);
	return set == p ? BigInt() : r;
}


static Division divide_by_limb(const BigInt& x, std::uint64_t d) {
	//
	// Quotient and remainder of x by a one-limb divisor d > 0
	//
	// From the top limb down, the remainder so far, below d, and the next limb make a double-width
	// number whose quotient by d is the next digit, exactly: with one limb in the divisor the
	// estimate of MCA, Algorithm 1.6, is the digit itself, and needs no normalization or correction.

	const std::vector<std::uint64_t>& u = x.limbs();
	std::vector<std::uint64_t> q(u.size(), 0);
	std::uint64_t r = 0;
	for (std::size_t i = u.size(); i-- > 0;) {
		u128 current = (u128(r) << 64) | u[i];
		q[i] = std::uint64_t(current / d);
		r = std::uint64_t(current - u128(q[i]) * d);
	}
	return {BigInt::from_limbs(q), BigInt(r)};
}


Division divide(const BigInt& x, const BigInt& m) {
	//
	// Quotient and remainder of x by m, an error for m = 0
	//
	// Long division (MCA, 1.4.1, Algorithm 1.6 BasecaseDivRem). Both are shifted
	// left until the divisor's top limb has its top bit set, which keeps the quotient and scales the
	// remainder. Each quotient digit, from the top, is estimated from the remainder's top two limbs
	// over the divisor's top limb and clamped below 2^64, and that many divisors are subtracted. The
	// estimate is never too small and, with the divisor so normalized, at most two too large (MCA,
	// Theorem 1.4.1, which cites Knuth 1997, 4.3.1, for it), so while the remainder is below zero
	// the digit drops by one and the divisor is added back, at most twice. A one-limb divisor takes
	// short division.

	if (m.is_zero())
		throw mp_error("modulus zero");
	if (compare(x, m) < 0)
		return {BigInt(), x};
	if (m.limbs().size() == 1)
		return divide_by_limb(x, m.limbs()[0]);

	int shift = std::countl_zero(m.limbs().back());
	std::vector<std::uint64_t> v = shift_left(m, shift).limbs();
	std::vector<std::uint64_t> u = shift_left(x, shift).limbs();
	std::size_t n = v.size();
	u.resize(x.limbs().size() + 1, 0);
	std::vector<std::uint64_t> q(u.size() - n, 0);
	for (std::size_t j = u.size() - n; j-- > 0;) {

		// The window u[j .. j + n] is below 2^64 times the divisor, so its digit fits a limb
		u128 top = (u128(u[j + n]) << 64) | u[j + n - 1];
		u128 estimate = top / v[n - 1];
		std::uint64_t digit = estimate > ALL_ONES ? ALL_ONES : std::uint64_t(estimate);

		// The window less digit times the divisor, the product's high limb and the borrow carried
		// together as one limb; a borrow out of the window's top limb leaves it below zero
		std::uint64_t carry = 0;
		for (std::size_t i = 0; i < n; i++) {
			u128 product = u128(digit) * v[i] + carry;
			std::uint64_t low = std::uint64_t(product);
			carry = std::uint64_t(product >> 64) + (u[j + i] < low);
			u[j + i] -= low;
		}
		bool below_zero = u[j + n] < carry;
		u[j + n] -= carry;

		// Add back: one divisor fewer, until the carry out of the window's top limb cancels the
		// borrow
		while (below_zero) {
			digit--;
			u128 sum = 0;
			for (std::size_t i = 0; i < n; i++) {
				sum = u128(u[j + i]) + v[i] + (sum >> 64);
				u[j + i] = std::uint64_t(sum);
			}
			sum = u128(u[j + n]) + (sum >> 64);
			u[j + n] = std::uint64_t(sum);
			below_zero = (sum >> 64) == 0;
		}
		q[j] = digit;
	}
	u.resize(n);
	return {BigInt::from_limbs(q), shift_right(BigInt::from_limbs(u), shift)};
}


BigInt reduce(const BigInt& x, const BigInt& m) {
	//
	// x mod m, in [0, m): the remainder of the long division; m = 0 throws

	return divide(x, m).remainder;
}


BigInt power_mod(const BigInt& base, const BigInt& exponent, const BigInt& m) {
	//
	// base^exponent mod m, in [0, m); m = 0 throws
	//
	// Left to right over the exponent's bits: from the base reduced, for each bit below the top
	// one, square and, for a one bit, multiply by the base, reducing after each product (MCA,
	// 2.6.1, Algorithm 2.12 LeftToRightBinaryExp). The algorithm needs a top bit, so exponent 0
	// is taken first and gives 1; modulus 1, before it, gives 0.

	if (m.is_zero())
		throw mp_error("modulus zero");
	if (m == BigInt(1))
		return BigInt();
	if (exponent.is_zero())
		return BigInt(1);
	const std::vector<std::uint64_t>& e = exponent.limbs();
	BigInt b = reduce(base, m);
	BigInt r = b;
	for (std::size_t i = exponent.bit_length() - 1; i-- > 0;) {
		r = reduce(square(r), m);
		if ((e[i / 64] >> (i % 64)) & 1)
			r = reduce(multiply(r, b), m);
	}
	return r;
}


std::string hex64(std::uint64_t value) {
	//
	// A 64-bit residue as 16 uppercase hexadecimal digits, zero-padded

	static const char DIGITS[] = "0123456789ABCDEF";
	std::string text(16, '0');
	for (int i = 15; i >= 0; i--) {
		text[i] = DIGITS[value & 0xf];
		value >>= 4;
	}
	return text;
}


std::uint64_t res64(const BigInt& x, long unsigned p) {
	//
	// Res64: the low 64 bits of x mod M_p, fully reduced, so 0 for x = 0 and x = M_p
	//
	// The low-order 64 bits of the final value, as GIMPS, "The Math", 2024, section
	// Double-Checking, defines it. p = 0 throws.

	BigInt r = reduce_mersenne(x, p);
	return r.is_zero() ? 0 : r.limbs()[0];
}


std::string to_hex(const BigInt& x) {
	//
	// The canonical hexadecimal string: uppercase, no prefix, no leading zero, "0" for zero
	//
	// 16 digits per limb from the top, then the top limb's leading zeros dropped.

	if (x.is_zero())
		return "0";
	const std::vector<std::uint64_t>& limbs = x.limbs();
	std::string text;
	text.reserve(16 * limbs.size());
	for (std::size_t i = limbs.size(); i-- > 0;)
		text += hex64(limbs[i]);
	return text.substr(text.find_first_not_of('0'));
}


static int hex_digit(char c) {
	//
	// The value of one hexadecimal digit, either case, or -1 for any other character

	if (c >= '0' and c <= '9')
		return c - '0';
	if (c >= 'A' and c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' and c <= 'f')
		return c - 'a' + 10;
	return -1;
}


BigInt from_hex(const std::string& text) {
	//
	// A value from a string of hexadecimal digits
	//
	// A nonempty string of 0-9, A-F and a-f, leading zeros allowed. Anything else, the empty
	// string included, throws: every character is read before a value is returned. 16 digits per
	// limb from the bottom.

	if (text.empty())
		throw mp_error("not a hexadecimal number: the empty string");
	std::vector<std::uint64_t> limbs((text.size() + 15) / 16, 0);
	for (std::size_t i = 0; i < text.size(); i++) {
		std::size_t at = text.size() - 1 - i;
		int digit = hex_digit(text[at]);
		if (digit < 0)
			throw mp_error("not a hexadecimal number: character " + std::to_string(at + 1) + " of "
			               + std::to_string(text.size()));
		limbs[i / 16] |= std::uint64_t(digit) << (4 * (i % 16));
	}
	return BigInt::from_limbs(limbs);
}


static void append_digits(std::string& out, std::uint64_t value, std::size_t width) {
	//
	// One limb's decimal digits appended to out, zero-padded to width digits (0: unpadded)

	char digits[20];
	std::size_t n = 0;
	do {
		digits[n++] = char('0' + value % 10);
		value /= 10;
	} while (value != 0);
	for (std::size_t i = n; i < width; i++)
		out += '0';
	while (n > 0)
		out += digits[--n];
}


static void append_decimal(std::string& out, const BigInt& x, const std::vector<BigInt>& powers, std::size_t k,
                           bool padded) {
	//
	// The decimal digits of 0 < x < powers[k]^2 appended to out
	//
	// powers[i] is 10^(19 2^i). Unpadded, the digits start at x's first nonzero one; padded, they
	// are exactly 19 2^(k+1), leading zeros included. x splits by powers[k] into a high part, below
	// powers[k] and printed as x is, and a low part, padded, each the same way one level down
	// (MCA, 1.7.2, Algorithm 1.26 FastIntegerOutput). At the bottom, one division by 10^19 leaves
	// two limbs to print.

	if (k == 0) {
		Division d = divide(x, powers[0]);
		std::uint64_t high = d.quotient.is_zero() ? 0 : d.quotient.limbs()[0];
		std::uint64_t low = d.remainder.is_zero() ? 0 : d.remainder.limbs()[0];
		if (padded or high != 0) {
			append_digits(out, high, padded ? DECIMAL_LIMB : 0);
			append_digits(out, low, DECIMAL_LIMB);
		} else {
			append_digits(out, low, 0);
		}
		return;
	}
	if (not padded and compare(x, powers[k]) < 0) {
		append_decimal(out, x, powers, k - 1, false);
		return;
	}
	Division d = divide(x, powers[k]);
	append_decimal(out, d.quotient, powers, k - 1, padded);
	append_decimal(out, d.remainder, powers, k - 1, true);
}


std::string to_decimal(const BigInt& x) {
	//
	// The canonical decimal string: digits only, no sign, no leading zero, "0" for zero
	//
	// Divide and conquer (MCA, 1.7.2, Algorithm 1.26 FastIntegerOutput): the powers 10^(19 2^i),
	// each the square of the one before, up to the first whose square exceeds x, then x split by
	// them from the top. A power of b bits has a square of at least 2b - 1 bits, so one whose square
	// is that long and x shorter is not squared at all.

	if (x.is_zero())
		return "0";
	std::vector<BigInt> powers {BigInt(TEN_19)};
	while (2 * powers.back().bit_length() - 1 <= x.bit_length()) {
		BigInt next = square(powers.back());
		if (compare(next, x) > 0)
			break;
		powers.push_back(next);
	}
	std::string text;
	text.reserve(x.bit_length() * 30103 / 100000 + 2);
	append_decimal(text, x, powers, powers.size() - 1, false);
	return text;
}


BigInt from_decimal(const std::string& text) {
	//
	// A value from a string of decimal digits
	//
	// A nonempty string of 0-9, leading zeros allowed. Anything else, the empty string included,
	// throws: every character is read before a value is built. Then chunks of 19 digits from the
	// top, the first one shorter, each step x <- x 10^19 + chunk, one row of the schoolbook product
	// (MCA, 1.7.1, Algorithm 1.23 IntegerInput).

	if (text.empty())
		throw mp_error("not a decimal number: the empty string");
	for (std::size_t i = 0; i < text.size(); i++)
		if (text[i] < '0' or text[i] > '9')
			throw mp_error("not a decimal number: character " + std::to_string(i + 1) + " of "
			               + std::to_string(text.size()));
	std::vector<std::uint64_t> limbs;
	limbs.reserve(text.size() / DECIMAL_LIMB + 2);
	std::size_t length = text.size() % DECIMAL_LIMB == 0 ? DECIMAL_LIMB : text.size() % DECIMAL_LIMB;
	for (std::size_t start = 0; start < text.size(); start += length, length = DECIMAL_LIMB) {
		std::uint64_t chunk = 0;
		for (std::size_t i = start; i < start + length; i++)
			chunk = 10 * chunk + std::uint64_t(text[i] - '0');
		u128 carry = chunk;
		for (std::uint64_t& limb: limbs) {
			u128 t = u128(limb) * TEN_19 + carry;
			limb = std::uint64_t(t);
			carry = t >> 64;
		}
		if (carry != 0)
			limbs.push_back(std::uint64_t(carry));
	}
	return BigInt::from_limbs(limbs);
}
