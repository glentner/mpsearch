// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Vector reader
//
// The file format is the one tests/vectors/generate.py writes: LF line ends, printable ASCII,
// single spaces; the magic line, the command, one or more source lines, notes, then the records,
// one op and its fields per line, and last the trailer, `# end <count> records <sha256>`, whose
// digest covers every byte before it.
//


// Standard libs
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "mp.hpp"
#include "vector_reader.hpp"


static const std::string MAGIC = "# mps vectors 1";
static const std::size_t FULL_BITS = 1024;


// The kinds of an op's fields, in order: operand (O), value (V), full value (F), string (S),
// decimal number (D), Res64 (H), comparison (C)
static std::map<std::string, std::string>
fields_by_op {
	{"cmp",      "OOC"},
	{"add",      "OOV"},
	{"sub",      "OOV"},
	{"shl",      "ODV"},
	{"shr",      "ODV"},
	{"fold",     "DOV"},
	{"res64",    "DOH"},
	{"hex",      "OS"},
	{"mul",      "OOV"},
	{"sqr",      "OV"},
	{"mod",      "OOV"},
	{"powmod",   "OOOV"},
	{"dec",      "OS"},
	{"ll",       "DF"},
	{"prp",      "DF"},
	{"mersenne", "D"},
};


namespace vectors {


	const std::vector<std::string> STEMS = {"arith", "folding", "hex", "products", "modular", "decimal", "reference",
	                                        "mersenne-exponents"};


	std::string Record::where() const {
		return file + ":" + std::to_string(line) + ": " + fields[0];
	}

}


// --- SHA-256 (FIPS 180-4) -------------------------------------------------------------------------


static const std::uint32_t SHA256_K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};


static std::uint32_t rotate_right(std::uint32_t x, int n) {
	return (x >> n) | (x << (32 - n));
}


static void sha256_block(std::uint32_t h[8], const unsigned char* block) {
	//
	// One 512-bit block of the SHA-256 compression function (FIPS 180-4, 6.2.2)

	std::uint32_t w[64];
	for (int t = 0; t < 16; t++)
		w[t] = std::uint32_t(block[4 * t]) << 24 | std::uint32_t(block[4 * t + 1]) << 16
		     | std::uint32_t(block[4 * t + 2]) << 8 | std::uint32_t(block[4 * t + 3]);
	for (int t = 16; t < 64; t++) {
		std::uint32_t s0 = rotate_right(w[t - 15], 7) ^ rotate_right(w[t - 15], 18) ^ (w[t - 15] >> 3);
		std::uint32_t s1 = rotate_right(w[t - 2], 17) ^ rotate_right(w[t - 2], 19) ^ (w[t - 2] >> 10);
		w[t] = s1 + w[t - 7] + s0 + w[t - 16];
	}
	std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
	for (int t = 0; t < 64; t++) {
		std::uint32_t big_s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
		std::uint32_t choose = (e & f) ^ (~e & g);
		std::uint32_t t1 = k + big_s1 + choose + SHA256_K[t] + w[t];
		std::uint32_t big_s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
		std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
		std::uint32_t t2 = big_s0 + majority;
		k = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;
	h[4] += e;
	h[5] += f;
	h[6] += g;
	h[7] += k;
}


namespace vectors {


	std::string sha256(const std::string& bytes) {
		//
		// The SHA-256 digest of bytes, as 64 lowercase hexadecimal digits
		//
		// FIPS 180-4: the constants of 4.2.2, the initial hash value of 5.3.3, the padding of
		// 5.1.1 (a one bit, zeros, and the message length in bits as 64 big-endian bits), and
		// the blocks of 6.2.2.

		std::uint32_t h[8] = {
			0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
		};
		std::size_t whole = bytes.size() / 64 * 64;
		const unsigned char* data = reinterpret_cast<const unsigned char*>(bytes.data());
		for (std::size_t i = 0; i < whole; i += 64)
			sha256_block(h, data + i);

		std::string tail = bytes.substr(whole);
		tail += char(0x80);
		while (tail.size() % 64 != 56)
			tail += char(0);
		std::uint64_t bits = std::uint64_t(bytes.size()) * 8;
		for (int i = 7; i >= 0; i--)
			tail += char((bits >> (8 * i)) & 0xff);
		for (std::size_t i = 0; i < tail.size(); i += 64)
			sha256_block(h, reinterpret_cast<const unsigned char*>(tail.data()) + i);

		static const char DIGITS[] = "0123456789abcdef";
		std::string digest;
		for (int i = 0; i < 8; i++)
			for (int shift = 28; shift >= 0; shift -= 4)
				digest += DIGITS[(h[i] >> shift) & 0xf];
		return digest;
	}


	SplitMix64::SplitMix64(std::uint64_t seed): state(seed) {}


	std::uint64_t SplitMix64::next() {
		//
		// The next output
		//
		// Steele, Lea and Flood, "Fast splittable pseudorandom number generators", 2014, Fig. 16:
		// the state advances by the golden gamma, and the output is the state mixed, all modulo
		// 2^64.

		state += 0x9e3779b97f4a7c15ull;
		std::uint64_t z = state;
		z = (z ^ (z >> 33)) * 0xff51afd7ed558ccdull;
		z = (z ^ (z >> 33)) * 0xc4ceb9fe1a85ec53ull;
		return z ^ (z >> 33);
	}

}


// --- Fields ---------------------------------------------------------------------------------------


static bool is_decimal(const std::string& text) {
	//
	// Decimal digits with no leading zero, at most 2^64 - 1

	if (text.empty() or text.size() > 20 or (text.size() > 1 and text[0] == '0'))
		return false;
	for (char c: text)
		if (c < '0' or c > '9')
			return false;
	return text.size() < 20 or text <= "18446744073709551615";
}


static bool is_lower_hex(const std::string& text, bool leading_zero) {
	if (text.empty() or (not leading_zero and text.size() > 1 and text[0] == '0'))
		return false;
	for (char c: text)
		if (not ((c >= '0' and c <= '9') or (c >= 'a' and c <= 'f')))
			return false;
	return true;
}


static bool is_upper_hex(const std::string& text) {
	if (text.empty())
		return false;
	for (char c: text)
		if (not ((c >= '0' and c <= '9') or (c >= 'A' and c <= 'F')))
			return false;
	return true;
}


static bool is_digest(const std::string& text) {
	return text.size() == 64 and is_lower_hex(text, true);
}


static std::vector<std::string> split(const std::string& text, char separator) {
	std::vector<std::string> parts;
	std::size_t start = 0;
	while (true) {
		std::size_t end = text.find(separator, start);
		parts.push_back(text.substr(start, end - start));
		if (end == std::string::npos)
			return parts;
		start = end + 1;
	}
}


static bool is_operand(const std::string& field) {
	//
	// x:hex, u:lo-hi,..., r:bits:seed or c:bits:seed

	if (field.size() < 3 or field[1] != ':')
		return false;
	std::string rest = field.substr(2);
	switch (field[0]) {
	case 'x':
		return is_lower_hex(rest, false);
	case 'u':
		for (const std::string& range: split(rest, ',')) {
			std::vector<std::string> ends = split(range, '-');
			if (ends.size() != 2 or not is_decimal(ends[0]) or not is_decimal(ends[1]))
				return false;
			if (not (std::stoull(ends[0]) < std::stoull(ends[1])) or std::stoull(ends[1]) > (1ull << 32))
				return false;
		}
		return true;
	case 'r':
	case 'c': {
		std::vector<std::string> parts = split(rest, ':');
		return parts.size() == 2 and is_decimal(parts[0]) and is_decimal(parts[1]) and parts[0] != "0"
		       and std::stoull(parts[0]) <= (1ull << 32);
	}
	default:
		return false;
	}
}


static bool is_value(const std::string& field) {
	//
	// x:hex of at most 1024 bits, or h:bits:low16:sha256 above

	if (field.rfind("x:", 0) == 0) {
		std::string hex = field.substr(2);
		return is_lower_hex(hex, false) and hex.size() <= FULL_BITS / 4;
	}
	if (field.rfind("h:", 0) != 0)
		return false;
	std::vector<std::string> parts = split(field.substr(2), ':');
	return parts.size() == 3 and is_decimal(parts[0]) and std::stoull(parts[0]) > FULL_BITS
	       and parts[1].size() == 16 and is_lower_hex(parts[1], true) and is_digest(parts[2]);
}


static bool is_string(const std::string& field) {
	//
	// s:chars, digits and uppercase letters, or g:length:sha256

	if (field.rfind("s:", 0) == 0)
		return is_upper_hex(field.substr(2));
	if (field.rfind("g:", 0) != 0)
		return false;
	std::vector<std::string> parts = split(field.substr(2), ':');
	return parts.size() == 2 and is_decimal(parts[0]) and is_digest(parts[1]);
}


static bool is_field(char kind, const std::string& field) {
	switch (kind) {
	case 'O':
		return is_operand(field);
	case 'V':
		return is_value(field);
	case 'F':
		return field.rfind("x:", 0) == 0 and is_lower_hex(field.substr(2), false);
	case 'S':
		return is_string(field);
	case 'D':
		return is_decimal(field);
	case 'H':
		return field.size() == 16 and is_upper_hex(field);
	case 'C':
		return field == "lt" or field == "eq" or field == "gt";
	default:
		return false;
	}
}


// --- Loading --------------------------------------------------------------------------------------


static vectors::Family read_family(const std::string& stem) {
	//
	// Read one family, requiring every rule of its envelope

	std::string file = "tests/vectors/" + stem + ".txt";
	std::ifstream in(file, std::ios::binary);
	check::require(in.good(), file + " opens");
	std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	check::require(not text.empty() and text.back() == '\n', file + " ends in LF");

	std::vector<std::string> lines;
	std::size_t last_start = 0;
	for (std::size_t start = 0; start < text.size();) {
		std::size_t end = text.find('\n', start);
		last_start = start;
		lines.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	for (std::size_t i = 0; i < lines.size(); i++) {
		bool plain = not lines[i].empty();
		for (char c: lines[i])
			if (c < 0x20 or c > 0x7e)
				plain = false;
		check::require(plain, file + ":" + std::to_string(i + 1) + " is nonempty printable ASCII");
	}

	vectors::Family family;
	family.stem = stem;
	family.command = "# command: python3 tests/vectors/generate.py " + stem + " > tests/vectors/" + stem + ".txt";
	check::require(lines.size() >= 5, file + " holds a header, a record and a trailer");
	check::require(lines[0] == MAGIC, file + " opens with '" + MAGIC + "'");
	check::require(lines[1] == family.command, file + " carries its command, naming its own stem");

	std::size_t i = 2;
	for (; i + 1 < lines.size() and lines[i].rfind("# source: ", 0) == 0; i++) {
		check::require(lines[i].size() > 10, file + ":" + std::to_string(i + 1) + " names a source");
		family.sources.push_back(lines[i].substr(10));
	}
	check::require(not family.sources.empty(), file + " has at least one source line");
	while (i + 1 < lines.size() and lines[i].rfind("# ", 0) == 0)
		i++;

	for (; i + 1 < lines.size(); i++) {
		std::string where = file + ":" + std::to_string(i + 1);
		check::require(lines[i][0] != '#', where + " is a record (notes come before the first record)");
		vectors::Record record {file, i + 1, split(lines[i], ' ')};
		auto kinds = fields_by_op.find(record.fields[0]);
		check::require(kinds != fields_by_op.end(), where + " has a known op");
		check::require(record.fields.size() == kinds->second.size() + 1,
		               where + " has " + std::to_string(kinds->second.size()) + " fields for " + record.fields[0]);
		for (std::size_t k = 0; k < kinds->second.size(); k++)
			check::require(is_field(kinds->second[k], record.fields[k + 1]),
			               where + " field " + std::to_string(k + 1) + " is well-formed");
		family.records.push_back(record);
	}
	check::require(not family.records.empty(), file + " has at least one record");

	std::string digest = vectors::sha256(text.substr(0, last_start));
	std::string trailer = "# end " + std::to_string(family.records.size()) + " records " + digest;
	check::require(lines.back() == trailer, file + " ends in its trailer, with its record count and SHA-256");
	return family;
}


namespace vectors {


	const Family& load(const std::string& stem) {
		//
		// The family `stem`, read once and kept

		static std::map<std::string, Family> family_by_stem;
		auto found = family_by_stem.find(stem);
		if (found != family_by_stem.end())
			return found->second;
		return family_by_stem.emplace(stem, read_family(stem)).first->second;
	}


	std::vector<const Record*> records(const std::string& stem, const std::string& op) {
		//
		// The records of one op in a family, requiring at least one

		std::vector<const Record*> out;
		for (const Record& record: load(stem).records)
			if (record.fields[0] == op)
				out.push_back(&record);
		check::require(not out.empty(), "tests/vectors/" + stem + ".txt has a " + op + " record");
		return out;
	}


	std::uint64_t number(const std::string& field) {
		return std::stoull(field);
	}


	static std::vector<std::uint64_t> hex_limbs(const std::string& hex) {
		//
		// Lowercase hexadecimal digits as limbs, 16 digits each from the bottom

		std::vector<std::uint64_t> limbs((hex.size() + 15) / 16, 0);
		for (std::size_t i = 0; i < hex.size(); i++) {
			char c = hex[hex.size() - 1 - i];
			std::uint64_t v = c <= '9' ? c - '0' : c - 'a' + 10;
			limbs[i / 16] |= v << (4 * (i % 16));
		}
		return limbs;
	}


	static std::vector<std::uint64_t> topped(std::vector<std::uint64_t> limbs, std::uint64_t bits) {
		//
		// The low `bits` bits of the limbs, with bit `bits - 1` set

		std::size_t n = (bits + 63) / 64;
		limbs.resize(n);
		unsigned top = unsigned(bits - 64 * (n - 1));
		if (top < 64)
			limbs.back() &= (std::uint64_t(1) << top) - 1;
		limbs.back() |= std::uint64_t(1) << (top - 1);
		return limbs;
	}


	BigInt operand(const std::string& field) {
		//
		// An operand field as a value, built into limbs with BigInt::from_limbs
		//
		// x: in full; u: the OR of its bit ranges; r: SplitMix64 outputs as limbs; c: runs of
		// 1 + (v >> 32) mod 32 limbs per output v, all zero for v mod 3 = 0, all ones for 1, the
		// next outputs for 2. r: and c: keep exactly `bits` bits, the top one set.

		std::string rest = field.substr(2);
		std::vector<std::uint64_t> limbs;
		if (field[0] == 'x')
			return BigInt::from_limbs(hex_limbs(rest));

		if (field[0] == 'u') {
			for (const std::string& range: split(rest, ',')) {
				std::vector<std::string> ends = split(range, '-');
				std::uint64_t lo = std::stoull(ends[0]), hi = std::stoull(ends[1]);
				if (limbs.size() < (hi + 63) / 64)
					limbs.resize((hi + 63) / 64, 0);
				for (std::uint64_t bit = lo; bit < hi; bit++)
					limbs[bit / 64] |= std::uint64_t(1) << (bit % 64);
			}
			return BigInt::from_limbs(limbs);
		}

		std::vector<std::string> parts = split(rest, ':');
		std::uint64_t bits = std::stoull(parts[0]);
		SplitMix64 generator(std::stoull(parts[1]));
		std::size_t n = (bits + 63) / 64;
		if (field[0] == 'r') {
			for (std::size_t i = 0; i < n; i++)
				limbs.push_back(generator.next());
		} else {
			while (limbs.size() < n) {
				std::uint64_t v = generator.next();
				std::uint64_t run = 1 + (v >> 32) % 32;
				for (std::uint64_t k = 0; k < run; k++)
					limbs.push_back(v % 3 == 0 ? 0 : v % 3 == 1 ? ~std::uint64_t(0) : generator.next());
			}
		}
		return BigInt::from_limbs(topped(limbs, bits));
	}


	static std::string limb_bytes(const BigInt& value) {
		//
		// The limbs, least significant first, each as eight little-endian bytes

		std::string bytes;
		bytes.reserve(8 * value.limbs().size());
		for (std::uint64_t limb: value.limbs())
			for (int i = 0; i < 8; i++)
				bytes += char((limb >> (8 * i)) & 0xff);
		return bytes;
	}


	static std::string low16(const BigInt& value) {
		//
		// The low limb as 16 lowercase hexadecimal digits

		static const char DIGITS[] = "0123456789abcdef";
		std::uint64_t low = value.is_zero() ? 0 : value.limbs()[0];
		std::string text(16, '0');
		for (int i = 15; i >= 0; i--, low >>= 4)
			text[i] = DIGITS[low & 0xf];
		return text;
	}


	bool matches(const BigInt& value, const std::string& field) {
		//
		// The value a value field states: in full, or by bit length, low limb and digest

		if (field[0] == 'x')
			return value == BigInt::from_limbs(hex_limbs(field.substr(2)));
		std::vector<std::string> parts = split(field.substr(2), ':');
		return value.bit_length() == std::stoull(parts[0]) and low16(value) == parts[1]
		       and sha256(limb_bytes(value)) == parts[2];
	}


	bool matches_string(const std::string& text, const std::string& field) {
		//
		// The string a string field states: in full, or by length and digest

		if (field[0] == 's')
			return text == field.substr(2);
		std::vector<std::string> parts = split(field.substr(2), ':');
		return text.size() == std::stoull(parts[0]) and sha256(text) == parts[1];
	}


	std::string describe(const BigInt& value) {
		return std::to_string(value.bit_length()) + " bits, low limb " + low16(value);
	}


	std::string describe_expected(const std::string& field) {
		//
		// What a value or string field states, short enough to print

		std::vector<std::string> parts = split(field.substr(2), ':');
		switch (field[0]) {
		case 'x':
			return describe(BigInt::from_limbs(hex_limbs(field.substr(2))));
		case 'h':
			return parts[0] + " bits, low limb " + parts[1];
		case 'g':
			return "a string of " + parts[0] + " characters";
		default:
			return field.size() <= 34 ? "'" + field.substr(2) + "'"
			                          : "a string of " + std::to_string(field.size() - 2) + " characters";
		}
	}


	Mismatches::Mismatches(const std::string& what): what(what) {}


	void Mismatches::add(const Record& record, const std::string& detail) {
		//
		// Count a record that disagrees, printing the first three

		count++;
		if (count <= 3)
			std::cout << "   " << record.where() << ": " << detail << std::endl;
	}


	void Mismatches::expect_none(std::size_t total, std::source_location location) {
		check::expect(count == 0, "all " + std::to_string(total) + " " + what + " to agree with the vectors", location);
	}

}
