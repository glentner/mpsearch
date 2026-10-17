// Gate trace driver: a cycle record, never part of mps.
//
// Linked against the gates' wrapped build (mutant.cpp's pass-through wrappers), it calls the
// product and square entry points and reads which entry points each call reached, and at what
// depth, so the gates see the structure PLAN § 2 pins, which no result can show: a schoolbook
// product or square calls no product or square; Karatsuba, from cutoff limbs up, recurses into
// itself; `multiply` and `square` call Karatsuba from KARATSUBA_THRESHOLD limbs in the smaller
// operand up and schoolbook below. Prints `trace: ok`, or each violation, and exits 1.


// Standard libs
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// Internal libs
#include "mp.hpp"


void gate_log_start();
std::vector<std::pair<int, std::string>> gate_log_stop();


static int failures = 0;


static BigInt value(std::size_t limbs, std::uint64_t seed) {
	std::vector<std::uint64_t> l(limbs);
	for (std::uint64_t& v: l) {
		seed ^= seed << 13;
		seed ^= seed >> 7;
		seed ^= seed << 17;
		v = seed;
	}
	if (limbs > 0)
		l.back() |= std::uint64_t(1) << 63;
	return BigInt::from_limbs(l);
}


static std::vector<std::pair<int, std::string>> run(const std::function<void()>& call) {
	gate_log_start();
	call();
	return gate_log_stop();
}


// Calls of `name` below the top call
static int nested(const std::vector<std::pair<int, std::string>>& log, const std::string& name) {
	int n = 0;
	for (const auto& [depth, fn]: log)
		if (depth >= 1 and fn == name)
			n++;
	return n;
}


static void need(bool condition, const std::string& what) {
	if (not condition) {
		std::cout << "trace: " << what << std::endl;
		failures++;
	}
}


static const char* PRODUCTS[] = {"multiply_schoolbook", "multiply_karatsuba", "multiply", "square_schoolbook",
                                 "square_karatsuba", "square"};


int main() {
	const std::size_t T = KARATSUBA_THRESHOLD;
	BigInt x = value(300, 1), y = value(300, 2);

	auto log = run([&] { multiply_schoolbook(x, y); });
	for (const char* p: PRODUCTS)
		need(nested(log, p) == 0, std::string("multiply_schoolbook calls ") + p);
	log = run([&] { square_schoolbook(x); });
	for (const char* p: PRODUCTS)
		need(nested(log, p) == 0, std::string("square_schoolbook calls ") + p);

	for (std::size_t n: {std::size_t(8), std::size_t(37), std::size_t(41)}) {
		log = run([&] { multiply_karatsuba(value(n, 3), value(n - 1, 4), 4); });
		need(nested(log, "multiply_karatsuba") >= 3, "multiply_karatsuba(" + std::to_string(n) + " limbs, cutoff 4) does not recurse into itself");
		log = run([&] { square_karatsuba(value(n, 5), 4); });
		need(nested(log, "square_karatsuba") >= 3, "square_karatsuba(" + std::to_string(n) + " limbs, cutoff 4) does not recurse into itself");
	}
	log = run([&] { multiply_karatsuba(value(3, 6), value(40, 7), 4); });
	need(nested(log, "multiply_karatsuba") == 0 and nested(log, "multiply_schoolbook") >= 1,
	     "multiply_karatsuba below its cutoff does not hand the product to multiply_schoolbook");

	log = run([&] { multiply(value(T, 8), value(T, 9)); });
	need(nested(log, "multiply_karatsuba") >= 1, "multiply at KARATSUBA_THRESHOLD limbs does not call multiply_karatsuba");
	log = run([&] { multiply(value(T - 1, 10), value(300, 11)); });
	need(nested(log, "multiply_karatsuba") == 0 and nested(log, "multiply_schoolbook") >= 1,
	     "multiply below KARATSUBA_THRESHOLD limbs in the smaller operand does not call multiply_schoolbook alone");
	log = run([&] { square(value(T, 12)); });
	need(nested(log, "square_karatsuba") >= 1, "square at KARATSUBA_THRESHOLD limbs does not call square_karatsuba");
	log = run([&] { square(value(T - 1, 13)); });
	need(nested(log, "square_karatsuba") == 0 and nested(log, "square_schoolbook") >= 1,
	     "square below KARATSUBA_THRESHOLD limbs does not call square_schoolbook alone");

	if (failures == 0)
		std::cout << "trace: ok" << std::endl;
	return failures == 0 ? 0 : 1;
}
