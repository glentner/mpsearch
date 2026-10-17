// Gate probe: a cycle record, never part of mps.
//
// Reads one operation per line on stdin and writes one answer per line on stdout, so verify/probe.py
// can compare the tree's BigInt with Python's integers on cases of its own, independent of the
// committed vectors. Values cross as hexadecimal through this file's own converters, never through
// the tree's to_hex or from_hex, except in the operations that test those. A string argument
// crosses as the hexadecimal of its bytes. An operation that throws mp_error answers `ERR`; any
// other exception answers `EXC <what>`. GATE_PHASE (a -D on the compile line) says which phase's
// functions exist.


// Standard libs
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Internal libs
#include "mp.hpp"
#if GATE_PHASE >= 4
#include "reference.hpp"
#endif


static BigInt value_of(const std::string& text) {
	// Hexadecimal, most significant digit first, to limbs
	std::vector<std::uint64_t> limbs((text.size() + 15) / 16, 0);
	for (std::size_t i = 0; i < text.size(); i++) {
		char c = text[text.size() - 1 - i];
		std::uint64_t v = c <= '9' ? c - '0' : c - 'a' + 10;
		limbs[i / 16] |= v << (4 * (i % 16));
	}
	return BigInt::from_limbs(limbs);
}


static std::string text_of(const BigInt& x) {
	// Limbs to lowercase hexadecimal, checking the representation on the way: no high zero limb
	static const char DIGITS[] = "0123456789abcdef";
	const std::vector<std::uint64_t>& limbs = x.limbs();
	if (not limbs.empty() and limbs.back() == 0)
		return "BADREP";
	std::string out;
	for (std::size_t i = limbs.size(); i-- > 0;)
		for (int k = 15; k >= 0; k--)
			out += DIGITS[(limbs[i] >> (4 * k)) & 0xf];
	std::size_t n = out.find_first_not_of('0');
	return n == std::string::npos ? "0" : out.substr(n);
}


static std::string bytes_of(const std::string& hex) {
	std::string out;
	for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
		out += char(std::stoi(hex.substr(i, 2), nullptr, 16));
	return out;
}


static std::string answer(const std::string& op, std::istringstream& in) {
	std::string a, b, c;
	in >> a >> b >> c;
	if (op == "cmp")
		return std::to_string(compare(value_of(a), value_of(b)));
	if (op == "eq")
		return value_of(a) == value_of(b) ? "1" : "0";
	if (op == "bits")
		return std::to_string(value_of(a).bit_length());
	if (op == "add")
		return text_of(add(value_of(a), value_of(b)));
	if (op == "sub")
		return text_of(subtract(value_of(a), value_of(b)));
	if (op == "shl")
		return text_of(shift_left(value_of(a), std::stoull(b)));
	if (op == "shr")
		return text_of(shift_right(value_of(a), std::stoull(b)));
	if (op == "fold")
		return text_of(reduce_mersenne(value_of(a), std::stoul(b)));
	if (op == "mersenne")
		return text_of(mersenne(std::stoul(a)));
	if (op == "res64")
		return hex64(res64(value_of(a), std::stoul(b)));
	if (op == "hex64")
		return hex64(std::stoull(a, nullptr, 16));
	if (op == "hex")
		return to_hex(value_of(a));
	if (op == "phex")
		return text_of(from_hex(bytes_of(a)));
#if GATE_PHASE >= 2
	if (op == "mul_s")
		return text_of(multiply_schoolbook(value_of(a), value_of(b)));
	if (op == "mul_k")
		return text_of(multiply_karatsuba(value_of(a), value_of(b)));
	if (op == "mul")
		return text_of(multiply(value_of(a), value_of(b)));
	if (op == "sqr_s")
		return text_of(square_schoolbook(value_of(a)));
	if (op == "sqr_k")
		return text_of(square_karatsuba(value_of(a)));
	if (op == "sqr")
		return text_of(square(value_of(a)));
	if (op == "threshold")
		return std::to_string(KARATSUBA_THRESHOLD);
#endif
#if GATE_PHASE >= 3
	if (op == "dec")
		return to_decimal(value_of(a));
	if (op == "pdec")
		return text_of(from_decimal(bytes_of(a)));
	if (op == "div") {
		Division d = divide(value_of(a), value_of(b));
		return text_of(d.quotient) + " " + text_of(d.remainder);
	}
	if (op == "mod")
		return text_of(reduce(value_of(a), value_of(b)));
	if (op == "pow")
		return text_of(power_mod(value_of(a), value_of(b), value_of(c)));
#endif
#if GATE_PHASE >= 4
	if (op == "ll")
		return text_of(reference::lucas_lehmer(std::stoul(a)));
	if (op == "prp")
		return text_of(reference::prp3(std::stoul(a)));
#endif
	return "UNKNOWN " + op;
}


int main() {
	std::string line;
	while (std::getline(std::cin, line)) {
		std::istringstream in(line);
		std::string op;
		in >> op;
		try {
			std::cout << answer(op, in) << "\n";
		} catch (mp_error const&) {
			std::cout << "ERR\n";
		} catch (std::exception const& e) {
			std::cout << "EXC " << e.what() << "\n";
		}
	}
	return 0;
}
