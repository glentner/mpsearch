// Gate probe of the sieve: a cycle record, never part of mps.
//
// Linked against the tree's own modules (everything in src/ but main.cpp), it reports what
// prime_sieve does, independently of the tree's checks:
//
//   probe-sieve sieve N...   for each N, "N ok" when prime_sieve(N) has N entries, true exactly
//                            at the primes below N; otherwise "N bad: <first difference>"
//   probe-sieve range A B    the same over every N in [A, B], one summary line


// Standard libs
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Internal libs
#include "math.hpp"


static bool is_prime(size_t n) {
	if (n < 2)
		return false;
	for (size_t d = 2; d * d <= n; d++)
		if (n % d == 0)
			return false;
	return true;
}


// A plain sieve, written for the gate, for large N where trial division is slow.
static std::vector<char> reference(size_t n) {
	std::vector<char> p(n, 1);
	for (size_t i = 0; i < n and i < 2; i++)
		p[i] = 0;
	for (size_t i = 2; i * i < n; i++)
		if (p[i])
			for (size_t j = i * i; j < n; j += i)
				p[j] = 0;
	return p;
}


static std::string sieve_report(size_t n) {
	std::vector<bool> got = prime_sieve(n);
	if (got.size() != n)
		return std::to_string(n) + " bad: " + std::to_string(got.size()) + " entries";
	std::vector<char> want;
	if (n > 100'000)
		want = reference(n);
	for (size_t i = 0; i < n; i++) {
		bool w = n > 100'000 ? want[i] != 0 : is_prime(i);
		if (got[i] != w)
			return std::to_string(n) + " bad: entry " + std::to_string(i) + " is " + (got[i] ? "true" : "false");
	}
	return std::to_string(n) + " ok";
}


int main(int argc, char** argv) {

	if (argc < 2)
		return 2;
	const std::string mode = argv[1];

	if (mode == "sieve") {
		for (int i = 2; i < argc; i++)
			std::cout << sieve_report(std::strtoull(argv[i], nullptr, 10)) << std::endl;
		return 0;
	}

	if (mode == "range" and argc == 4) {
		size_t a = std::strtoull(argv[2], nullptr, 10), b = std::strtoull(argv[3], nullptr, 10);
		size_t bad = 0;
		std::string first;
		for (size_t n = a; n <= b; n++) {
			std::string r = sieve_report(n);
			if (r.find(" ok") == std::string::npos) {
				if (bad == 0)
					first = r;
				bad++;
			}
		}
		std::cout << "range " << a << ".." << b << ": " << bad << " bad" << (bad ? " (" + first + ")" : "") << std::endl;
		return 0;
	}

	return 2;
}
