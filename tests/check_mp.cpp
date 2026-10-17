// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: multiple-precision arithmetic
//
// BigInt against the vectors Python's integers computed (tests/vectors/, written by
// tests/vectors/generate.py with the command each file carries). Each check runs every record of
// its ops through the functions under check, prints at most three that disagree, by bit length and
// low limb, never a whole large value, and then expects that all of them agree. Large results
// compare by the SHA-256 of their limbs.
//


// Standard libs
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <source_location>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "mp.hpp"
#include "vector_reader.hpp"


static void expect_mp_error(const std::function<void()>& call, const std::string& what,
                            std::source_location location = std::source_location::current()) {
	//
	// Expect a call to throw mp_error and return nothing
	//
	// A normal return records "returned a value"; any other exception, "threw <what>".

	try {
		call();
	} catch (mp_error const&) {
		check::record(true, what + " to report an error", location);
		return;
	} catch (std::exception const& error) {
		check::record(false, what + " to report an error", location, std::string("threw ") + error.what());
		return;
	}
	check::record(false, what + " to report an error", location, "returned a value");
}


static std::string mismatch(const BigInt& got, const std::string& expected) {
	//
	// A result and the value its record states, by bit length and low limb

	return "got " + vectors::describe(got) + ", expected " + vectors::describe_expected(expected);
}


static void sha256_matches_the_fips_examples() {
	//
	// SHA-256 of the examples for FIPS 180-4
	//
	// The one-block and two-block messages of NIST's examples, the empty message, and a million
	// repetitions of 'a', which spans many blocks.

	check::expect_equal(vectors::sha256("abc"),
	                    std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
	                    "the SHA-256 of 'abc'");
	check::expect_equal(vectors::sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
	                    std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"),
	                    "the SHA-256 of the two-block message");
	check::expect_equal(vectors::sha256(""),
	                    std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
	                    "the SHA-256 of the empty message");
	check::expect_equal(vectors::sha256(std::string(1'000'000, 'a')),
	                    std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"),
	                    "the SHA-256 of a million 'a'");
}


static void splitmix64_matches_its_published_outputs() {
	//
	// SplitMix64 from seed 0, and the operands built from it
	//
	// The first two outputs from seed 0 (Steele, Lea and Flood 2014, Fig. 16), operands of exactly
	// the bits they name, and an r: operand made of the outputs with its top bit set.

	vectors::SplitMix64 generator(0);
	check::expect_equal(hex64(generator.next()), std::string("9CA066F1A4AB2EEA"), "the first output from seed 0");
	check::expect_equal(hex64(generator.next()), std::string("D30B054265133DD7"), "the second output from seed 0");

	for (std::size_t bits: {1, 63, 64, 65, 1000}) {
		std::string size = std::to_string(bits);
		check::expect_equal(vectors::operand("r:" + size + ":7").bit_length(), bits, "r:" + size + ":7 to have its bit length");
		check::expect_equal(vectors::operand("c:" + size + ":7").bit_length(), bits, "c:" + size + ":7 to have its bit length");
	}
	vectors::SplitMix64 again(0);
	std::uint64_t low = again.next(), high = again.next();
	check::expect(vectors::operand("r:128:0") == BigInt::from_limbs({low, high | (std::uint64_t(1) << 63)}),
	              "r:128:0 to be the first two outputs with the top bit set");
}


static void every_vector_file_carries_its_command_and_source() {
	//
	// Every family loads, which requires its envelope, and carries its command and a source

	for (const std::string& stem: vectors::STEMS) {
		const vectors::Family& family = vectors::load(stem);
		check::expect_equal(family.command,
		                    "# command: python3 tests/vectors/generate.py " + stem + " > tests/vectors/" + stem + ".txt",
		                    stem + " to carry its command");
		check::expect(not family.sources.empty(), stem + " to carry its source");
	}
}


static void comparisons_match_the_vectors() {
	//
	// compare both ways round, and operator== both ways round

	std::vector<const vectors::Record*> records = vectors::records("arith", "cmp");
	vectors::Mismatches mismatches("comparisons");
	for (const vectors::Record* record: records) {
		BigInt a = vectors::operand(record->fields[1]), b = vectors::operand(record->fields[2]);
		const std::string& want = record->fields[3];
		int expected = want == "lt" ? -1 : want == "eq" ? 0 : 1;
		int forward = compare(a, b), backward = compare(b, a);
		bool equal = expected == 0;
		if (forward != expected or backward != -expected)
			mismatches.add(*record, "compare gives " + std::to_string(forward) + " and " + std::to_string(backward)
			                        + ", expected " + want);
		else if ((a == b) != equal or (b == a) != equal)
			mismatches.add(*record, "operator== disagrees with compare, expected " + want);
	}
	mismatches.expect_none(records.size());
}


static void sums_match_the_vectors() {
	//
	// add on every sum, carries through runs of all-ones limbs included

	std::vector<const vectors::Record*> records = vectors::records("arith", "add");
	vectors::Mismatches mismatches("sums");
	for (const vectors::Record* record: records) {
		BigInt sum = add(vectors::operand(record->fields[1]), vectors::operand(record->fields[2]));
		if (not vectors::matches(sum, record->fields[3]))
			mismatches.add(*record, mismatch(sum, record->fields[3]));
	}
	mismatches.expect_none(records.size());
}


static void differences_match_the_vectors() {
	//
	// A - B, and A - (A - B) = B

	std::vector<const vectors::Record*> records = vectors::records("arith", "sub");
	vectors::Mismatches mismatches("differences");
	for (const vectors::Record* record: records) {
		BigInt a = vectors::operand(record->fields[1]), b = vectors::operand(record->fields[2]);
		BigInt difference = subtract(a, b);
		if (not vectors::matches(difference, record->fields[3]))
			mismatches.add(*record, mismatch(difference, record->fields[3]));
		else if (not (subtract(a, difference) == b))
			mismatches.add(*record, "A - (A - B) is not B");
	}
	mismatches.expect_none(records.size());
}


static void shifts_match_the_vectors() {
	//
	// shift_left and shift_right by 0, by multiples of 64 and around them, and past the operand

	std::size_t total = 0;
	vectors::Mismatches mismatches("shifts");
	for (const std::string op: {"shl", "shr"}) {
		std::vector<const vectors::Record*> records = vectors::records("arith", op);
		total += records.size();
		for (const vectors::Record* record: records) {
			BigInt a = vectors::operand(record->fields[1]);
			std::size_t bits = vectors::number(record->fields[2]);
			BigInt shifted = op == "shl" ? shift_left(a, bits) : shift_right(a, bits);
			if (not vectors::matches(shifted, record->fields[3]))
				mismatches.add(*record, mismatch(shifted, record->fields[3]));
		}
	}
	mismatches.expect_none(total);
}


static void folding_modulo_mp_matches_the_vectors() {
	//
	// reduce_mersenne, compared exactly, so a result outside [0, M_p) fails

	std::vector<const vectors::Record*> records = vectors::records("folding", "fold");
	vectors::Mismatches mismatches("folds");
	for (const vectors::Record* record: records) {
		BigInt folded = reduce_mersenne(vectors::operand(record->fields[2]), vectors::number(record->fields[1]));
		if (not vectors::matches(folded, record->fields[3]))
			mismatches.add(*record, mismatch(folded, record->fields[3]));
	}
	mismatches.expect_none(records.size());
}


static void res64_matches_the_vectors() {
	//
	// hex64(res64(X, p)), and hex64 at its ends

	check::expect_equal(hex64(0), std::string("0000000000000000"), "hex64(0)");
	check::expect_equal(hex64(~std::uint64_t(0)), std::string("FFFFFFFFFFFFFFFF"), "hex64(2^64 - 1)");

	std::vector<const vectors::Record*> records = vectors::records("folding", "res64");
	vectors::Mismatches mismatches("Res64 values");
	for (const vectors::Record* record: records) {
		std::string text = hex64(res64(vectors::operand(record->fields[2]), vectors::number(record->fields[1])));
		if (text != record->fields[3])
			mismatches.add(*record, "got " + text + ", expected " + record->fields[3]);
	}
	mismatches.expect_none(records.size());
}


static std::string lowercase(std::string text) {
	//
	// Hexadecimal text with its letters in lowercase

	for (char& c: text)
		if (c >= 'A' and c <= 'F')
			c = char(c - 'A' + 'a');
	return text;
}


static void hexadecimal_matches_the_vectors_and_round_trips() {
	//
	// to_hex gives the vector's string, from_hex of it gives back A, and to_hex of that gives back
	// the string; from_hex with leading zeros and in lowercase gives back A too

	std::vector<const vectors::Record*> records = vectors::records("hex", "hex");
	vectors::Mismatches mismatches("hexadecimal strings");
	for (const vectors::Record* record: records) {
		BigInt a = vectors::operand(record->fields[1]);
		std::string text = to_hex(a);
		if (not vectors::matches_string(text, record->fields[2])) {
			mismatches.add(*record, "got a string of " + std::to_string(text.size()) + " characters, expected "
			                        + vectors::describe_expected(record->fields[2]));
			continue;
		}
		BigInt parsed = from_hex(text);
		if (not (parsed == a))
			mismatches.add(*record, "from_hex of the string gives " + vectors::describe(parsed));
		else if (to_hex(parsed) != text)
			mismatches.add(*record, "to_hex of the parse is not the string");
		else if (not (from_hex("00" + text) == a))
			mismatches.add(*record, "from_hex with leading zeros is not A");
		else if (not (from_hex(lowercase(text)) == a))
			mismatches.add(*record, "from_hex in lowercase is not A");
	}
	mismatches.expect_none(records.size());
}


static void errors_are_reported_never_values() {
	//
	// Every error case throws mp_error and returns nothing

	expect_mp_error([] { subtract(BigInt(1), BigInt(2)); }, "subtract(1, 2)");
	expect_mp_error([] { subtract(BigInt(0), BigInt(1)); }, "subtract(0, 1)");
	expect_mp_error([] { subtract(vectors::operand("u:0-256"), vectors::operand("u:256-257")); },
	                "subtract(2^256 - 1, 2^256)");
	expect_mp_error([] { subtract(vectors::operand("u:0-192,193-256"), vectors::operand("u:0-256")); },
	                "subtract of four limbs differing in the top limb");
	expect_mp_error([] { reduce_mersenne(BigInt(5), 0); }, "reduce_mersenne(5, 0)");
	expect_mp_error([] { res64(BigInt(5), 0); }, "res64(5, 0)");

	// The empty string, a sign, whitespace, a prefix, a separator, a non-digit, a full-width digit
	static const std::vector<std::string> not_numbers = {
		"", "-1", "+1", " 1", "1 ", "0x1F", "G", "1_0", "\xef\xbc\x91",
	};
	for (const std::string& text: not_numbers)
		expect_mp_error([&] { from_hex(text); }, "from_hex(\"" + text + "\")");

	// One bad digit in strings of one to three chunks of 16: at the first, the last, a chunk
	// boundary and an interior position, so a parser that reads only some chunks is seen
	for (std::size_t n: {16, 17, 32, 48})
		for (std::size_t at: {std::size_t(0), n - 1, std::size_t(15), std::size_t(16), n / 2}) {
			if (at >= n)
				continue;
			std::string text(n, '7');
			text[at] = 'G';
			expect_mp_error([&] { from_hex(text); },
			                "from_hex of " + std::to_string(n) + " digits, bad at " + std::to_string(at + 1));
		}

	// The same kinds of strings in decimal, a hexadecimal digit among them, and one bad digit in
	// strings of one to three chunks of 19
	for (const std::string& text: not_numbers)
		expect_mp_error([&] { from_decimal(text); }, "from_decimal(\"" + text + "\")");
	expect_mp_error([] { from_decimal("12a"); }, "from_decimal(\"12a\")");
	expect_mp_error([] { from_decimal("1F"); }, "from_decimal(\"1F\")");
	for (std::size_t n: {19, 20, 38, 57})
		for (std::size_t at: {std::size_t(0), n - 1, std::size_t(18), std::size_t(19), n / 2}) {
			if (at >= n)
				continue;
			std::string text(n, '7');
			text[at] = 'a';
			expect_mp_error([&] { from_decimal(text); },
			                "from_decimal of " + std::to_string(n) + " digits, bad at " + std::to_string(at + 1));
		}

	// A zero modulus, with x of one limb and of several
	expect_mp_error([] { divide(BigInt(5), BigInt()); }, "divide(5, 0)");
	expect_mp_error([] { divide(vectors::operand("r:300:1"), BigInt()); }, "divide of five limbs by 0");
	expect_mp_error([] { divide(BigInt(), BigInt()); }, "divide(0, 0)");
	expect_mp_error([] { reduce(BigInt(5), BigInt()); }, "reduce(5, 0)");
	expect_mp_error([] { reduce(vectors::operand("r:300:1"), BigInt()); }, "reduce of five limbs by 0");
	expect_mp_error([] { power_mod(BigInt(3), BigInt(5), BigInt()); }, "power_mod(3, 5, 0)");
	expect_mp_error([] { power_mod(BigInt(3), BigInt(), BigInt()); }, "power_mod(3, 0, 0)");
}


// The schoolbook functions are quadratic, so they are checked at full size on one product and one
// square only, and on every record whose operands are below this many bits
static const std::size_t FULL_SIZE_BITS = std::size_t(1) << 17;


// Which records of an op a product or square function is checked on
enum class Reach {
	EVERY,            // every record
	BELOW_FULL_SIZE,  // the records whose operands are all below FULL_SIZE_BITS
	AND_ONE_FULL,     // those, and the first record at full size
};


static void expect_products(const std::string& op, const std::string& what, Reach reach,
                            const std::function<BigInt(const std::vector<BigInt>&)>& compute) {
	//
	// One product or square function on the records of op it reaches
	//
	// The operands are the record's fields between the op and the result, in the record's order.

	std::vector<const vectors::Record*> records = vectors::records("products", op);
	vectors::Mismatches mismatches(what);
	std::size_t total = 0;
	bool full_size_taken = false;
	for (const vectors::Record* record: records) {
		std::vector<BigInt> operands;
		bool full_size = false;
		for (std::size_t i = 1; i + 1 < record->fields.size(); i++) {
			operands.push_back(vectors::operand(record->fields[i]));
			full_size = full_size or operands.back().bit_length() >= FULL_SIZE_BITS;
		}
		if (full_size and reach != Reach::EVERY) {
			if (reach == Reach::BELOW_FULL_SIZE or full_size_taken)
				continue;
			full_size_taken = true;
		}
		total++;
		const std::string& expected = record->fields.back();
		BigInt result = compute(operands);
		if (not vectors::matches(result, expected))
			mismatches.add(*record, mismatch(result, expected));
	}
	if (reach == Reach::AND_ONE_FULL)
		check::expect(full_size_taken, what + " to include one at full size");
	mismatches.expect_none(total);
}


static void schoolbook_products_match_the_vectors() {
	//
	// multiply_schoolbook below full size, and on the first full-size product, A * C

	expect_products("mul", "schoolbook products", Reach::AND_ONE_FULL,
	                [](const std::vector<BigInt>& x) { return multiply_schoolbook(x[0], x[1]); });
}


static void karatsuba_products_match_the_vectors() {
	//
	// multiply_karatsuba at cutoff 4 below full size, so that every level splits down to the
	// smallest it can, and at the default cutoff on every record

	expect_products("mul", "Karatsuba products at cutoff 4", Reach::BELOW_FULL_SIZE,
	                [](const std::vector<BigInt>& x) { return multiply_karatsuba(x[0], x[1], 4); });
	expect_products("mul", "Karatsuba products at the default cutoff", Reach::EVERY,
	                [](const std::vector<BigInt>& x) { return multiply_karatsuba(x[0], x[1]); });
}


static void products_match_the_vectors() {
	//
	// multiply, which chooses between the two by KARATSUBA_THRESHOLD, on every record

	expect_products("mul", "products", Reach::EVERY,
	                [](const std::vector<BigInt>& x) { return multiply(x[0], x[1]); });
}


static void schoolbook_squares_match_the_vectors() {
	//
	// square_schoolbook below full size, and on the first full-size square, A^2

	expect_products("sqr", "schoolbook squares", Reach::AND_ONE_FULL,
	                [](const std::vector<BigInt>& x) { return square_schoolbook(x[0]); });
}


static void karatsuba_squares_match_the_vectors() {
	//
	// square_karatsuba at cutoff 4 below full size, and at the default cutoff on every record

	expect_products("sqr", "Karatsuba squares at cutoff 4", Reach::BELOW_FULL_SIZE,
	                [](const std::vector<BigInt>& x) { return square_karatsuba(x[0], 4); });
	expect_products("sqr", "Karatsuba squares at the default cutoff", Reach::EVERY,
	                [](const std::vector<BigInt>& x) { return square_karatsuba(x[0]); });
}


static void squares_match_the_vectors() {
	//
	// square, which chooses between the two by KARATSUBA_THRESHOLD, on every record

	expect_products("sqr", "squares", Reach::EVERY, [](const std::vector<BigInt>& x) { return square(x[0]); });
}


static void general_reduction_matches_the_vectors() {
	//
	// reduce on every record, compared exactly, so a result outside [0, m) fails; and divide, whose
	// remainder is the same and whose quotient q has q m + r = x with r < m

	std::vector<const vectors::Record*> records = vectors::records("modular", "mod");
	vectors::Mismatches mismatches("reductions");
	for (const vectors::Record* record: records) {
		BigInt x = vectors::operand(record->fields[1]), m = vectors::operand(record->fields[2]);
		const std::string& expected = record->fields[3];
		BigInt remainder = reduce(x, m);
		if (not vectors::matches(remainder, expected)) {
			mismatches.add(*record, mismatch(remainder, expected));
			continue;
		}
		Division d = divide(x, m);
		if (not vectors::matches(d.remainder, expected))
			mismatches.add(*record, "divide's remainder: " + mismatch(d.remainder, expected));
		else if (not (add(multiply(d.quotient, m), d.remainder) == x))
			mismatches.add(*record, "q m + r is not x, with q " + vectors::describe(d.quotient));
		else if (compare(d.remainder, m) >= 0)
			mismatches.add(*record, "divide's remainder is not below m");
	}
	mismatches.expect_none(records.size());
}


static void modular_powers_match_the_vectors() {
	//
	// power_mod on every record, compared exactly, so a result outside [0, m) fails

	std::vector<const vectors::Record*> records = vectors::records("modular", "powmod");
	vectors::Mismatches mismatches("modular powers");
	for (const vectors::Record* record: records) {
		BigInt power = power_mod(vectors::operand(record->fields[1]), vectors::operand(record->fields[2]),
		                         vectors::operand(record->fields[3]));
		if (not vectors::matches(power, record->fields[4]))
			mismatches.add(*record, mismatch(power, record->fields[4]));
	}
	mismatches.expect_none(records.size());
}


static void decimal_matches_the_vectors_and_round_trips() {
	//
	// to_decimal gives the vector's string, from_decimal of it gives back A, and to_decimal of that
	// gives back the string; from_decimal with leading zeros gives back A too. At full size the
	// conversions are the slowest calls here, so there the parse giving back A ends the record:
	// printing is a function of the value, so printing the parse is printing A, already compared.

	std::vector<const vectors::Record*> records = vectors::records("decimal", "dec");
	vectors::Mismatches mismatches("decimal strings");
	for (const vectors::Record* record: records) {
		BigInt a = vectors::operand(record->fields[1]);
		std::string text = to_decimal(a);
		if (not vectors::matches_string(text, record->fields[2])) {
			mismatches.add(*record, "got a string of " + std::to_string(text.size()) + " characters, expected "
			                        + vectors::describe_expected(record->fields[2]));
			continue;
		}
		BigInt parsed = from_decimal(text);
		if (not (parsed == a))
			mismatches.add(*record, "from_decimal of the string gives " + vectors::describe(parsed));
		else if (a.bit_length() >= FULL_SIZE_BITS)
			continue;
		else if (to_decimal(parsed) != text)
			mismatches.add(*record, "to_decimal of the parse is not the string");
		else if (not (from_decimal("00" + text) == a))
			mismatches.add(*record, "from_decimal with leading zeros is not A");
	}
	mismatches.expect_none(records.size());
}


static check::Suite
check_by_name {
	{"SHA-256 matches the FIPS 180-4 examples",           sha256_matches_the_fips_examples},
	{"SplitMix64 matches its published outputs",          splitmix64_matches_its_published_outputs},
	{"every vector file carries its command and source",  every_vector_file_carries_its_command_and_source},
	{"comparisons match the vectors",                     comparisons_match_the_vectors},
	{"sums match the vectors",                            sums_match_the_vectors},
	{"differences match the vectors",                     differences_match_the_vectors},
	{"shifts match the vectors",                          shifts_match_the_vectors},
	{"folding modulo M_p matches the vectors",            folding_modulo_mp_matches_the_vectors},
	{"Res64 matches the vectors",                         res64_matches_the_vectors},
	{"hexadecimal matches the vectors and round-trips",   hexadecimal_matches_the_vectors_and_round_trips},
	{"errors are reported, never values",                 errors_are_reported_never_values},
	{"schoolbook products match the vectors",             schoolbook_products_match_the_vectors},
	{"Karatsuba products match the vectors",              karatsuba_products_match_the_vectors},
	{"products match the vectors",                        products_match_the_vectors},
	{"schoolbook squares match the vectors",              schoolbook_squares_match_the_vectors},
	{"Karatsuba squares match the vectors",               karatsuba_squares_match_the_vectors},
	{"squares match the vectors",                         squares_match_the_vectors},
	{"general reduction matches the vectors",             general_reduction_matches_the_vectors},
	{"modular powers match the vectors",                  modular_powers_match_the_vectors},
	{"decimal matches the vectors and round-trips",       decimal_matches_the_vectors_and_round_trips},
};
