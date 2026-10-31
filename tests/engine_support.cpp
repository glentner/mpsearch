// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Checks: what the engine's two tiers share
//


// Standard libs
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Internal libs
#include "check.hpp"
#include "engine_support.hpp"
#include "ibdwt.hpp"
#include "lengths.hpp"
#include "mp.hpp"


namespace engine_support {


	const std::string MEASUREMENT = "src/lengths.txt";


	Tally::Tally(const std::string& what): what_(what) {}


	void Tally::add(bool agree, const std::string& detail) {
		//
		// Count one case, printing the first three that disagree

		cases_++;
		if (agree)
			return;
		disagree_++;
		if (disagree_ <= 3)
			std::cout << "   " << what_ << ": " << detail << std::endl;
	}


	void Tally::expect_none(std::source_location location) {
		std::cout << "   tally " << what_ << ": " << cases_ << " cases, " << disagree_ << " disagree" << std::endl;
		check::expect(cases_ > 0 and disagree_ == 0, "all " + std::to_string(cases_) + " " + what_ + " to agree ("
		              + std::to_string(disagree_) + " did not)", location);
	}


	Random::Random(std::uint64_t seed): state_(seed) {}


	std::uint64_t Random::next() {
		std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ull);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
		return z ^ (z >> 31);
	}


	ibdwt::Residue random_digits(const ibdwt::Transform& transform, Random& random) {
		const std::vector<std::uint64_t>& c = transform.positions();
		ibdwt::Residue x;
		for (std::size_t j = 0; j + 1 < c.size(); j++) {
			std::uint64_t s = c[j + 1] - c[j];
			x.digits.push_back(std::int64_t(random.next() >> (64 - s)) - (std::int64_t(1) << (s - 1)));
		}
		return x;
	}


	bool balanced(const ibdwt::Transform& transform, const ibdwt::Residue& x) {
		const std::vector<std::uint64_t>& c = transform.positions();
		if (x.digits.size() + 1 != c.size())
			return false;
		for (std::size_t j = 0; j < x.digits.size(); j++) {
			std::int64_t half = std::int64_t(1) << (c[j + 1] - c[j] - 1);
			if (x.digits[j] < -half or x.digits[j] >= half)
				return false;
		}
		return true;
	}


	static void place(std::vector<std::uint64_t>& limbs, std::uint64_t value, std::uint64_t at) {
		//
		// limbs += value 2^at, carrying upward

		std::size_t i = at / 64;
		unsigned shift = unsigned(at % 64);
		std::uint64_t parts[2] = {value << shift, shift ? value >> (64 - shift) : 0};
		std::uint64_t carry = 0;
		for (std::size_t k = 0; i + k < limbs.size() and (k < 2 or carry); k++) {
			std::uint64_t add = (k < 2 ? parts[k] : 0);
			std::uint64_t sum = limbs[i + k] + add;
			std::uint64_t over = sum < add;
			std::uint64_t total = sum + carry;
			over += total < carry;
			limbs[i + k] = total;
			carry = over;
		}
	}


	BigInt value_of(const std::vector<std::uint64_t>& positions, const std::vector<std::int64_t>& digits, std::uint64_t p) {
		//
		// The positive and the negative digits are placed in two limb vectors, each reduced
		// modulo M_p on BigInt, and the difference taken modulo M_p

		std::size_t size = (positions.back() + 128) / 64 + 2;
		std::vector<std::uint64_t> plus(size), minus(size);
		for (std::size_t j = 0; j < digits.size(); j++) {
			std::int64_t d = digits[j];
			if (d >= 0)
				place(plus, std::uint64_t(d), positions[j]);
			else
				place(minus, std::uint64_t(-(d + 1)) + 1, positions[j]);
		}
		BigInt a = reduce_mersenne(BigInt::from_limbs(plus), p), b = reduce_mersenne(BigInt::from_limbs(minus), p);
		if (compare(a, b) < 0)
			a = add(a, mersenne(p));
		return reduce_mersenne(subtract(a, b), p);
	}


	BigInt less_power(const BigInt& a, std::uint64_t bit, std::uint64_t p) {
		BigInt r = reduce_mersenne(a, p), power = shift_left(BigInt(1), bit);
		if (compare(r, power) < 0)
			r = add(r, mersenne(p));
		return reduce_mersenne(subtract(r, power), p);
	}


	std::vector<Assigned> assigned() {
		std::vector<Assigned> out;
		std::uint64_t below = 2;
		for (std::uint64_t n: ibdwt::supported_lengths()) {
			std::uint64_t top = ibdwt::largest_exponent(n);
			out.push_back({n, std::max({below + 1, n, std::uint64_t(3)}), top});
			below = std::max(below, top);
		}
		return out;
	}


	std::uint64_t next_length(std::uint64_t length) {
		std::vector<std::uint64_t> lengths = ibdwt::supported_lengths();
		auto it = std::upper_bound(lengths.begin(), lengths.end(), length);
		return it == lengths.end() ? 0 : *it;
	}


	struct Line {
		std::uint64_t length, exponent, squarings, outputs;
		double distance, magnitude, rms;
	};


	static std::uint64_t planned(std::uint64_t length) {
		return std::max(ibdwt::PROBE_SQUARINGS, (ibdwt::PROBE_OUTPUTS + length - 1) / length);
	}


	static std::uint64_t grid(std::uint64_t length, std::uint64_t i) {
		std::uint64_t steps = std::uint64_t(ibdwt::GRID_STEPS);
		return (length * i + steps - 1) / steps;
	}


	static std::string where_probe(const Line& line) {
		return "probe " + std::to_string(line.length) + " " + std::to_string(line.exponent);
	}


	void expect_table_matches(const std::string& path) {
		std::ifstream in(path);
		check::require(bool(in), path + " to open");
		std::vector<std::string> text;
		for (std::string line; std::getline(in, line);)
			text.push_back(line);
		check::require(text.size() >= 3, path + " to hold a header, probes and a trailer");
		check::expect(text[0] == "# mps lengths 2", path + " to open with '# mps lengths 2'");
		check::expect(text[1].rfind("# command: make lengths", 0) == 0, path + " to name its command on its second line");

		std::vector<Line> lines;
		bool format = true;
		for (std::size_t i = 0; i + 1 < text.size(); i++) {
			if (text[i].empty() or text[i][0] == '#')
				continue;
			std::istringstream fields(text[i]);
			Line l;
			std::string extra;
			if (not (fields >> l.length >> l.exponent >> l.squarings >> l.outputs >> l.distance >> l.magnitude >> l.rms) or (fields >> extra)) {
				format = false;
				continue;
			}
			lines.push_back(l);
		}
		check::expect(format, path + ": every probe line to have its seven fields");
		check::expect(text.back() == "# end " + std::to_string(lines.size()) + " probes", path + " to end by counting its probes");

		std::vector<std::uint64_t> lengths = ibdwt::supported_lengths(), measured;
		std::vector<std::vector<Line>> by;
		for (const Line& l: lines) {
			if (measured.empty() or measured.back() != l.length) {
				measured.push_back(l.length);
				by.emplace_back();
			}
			by.back().push_back(l);
		}
		check::expect(measured == lengths, path + " to measure the " + std::to_string(lengths.size())
		                                   + " supported lengths once each, ascending");

		Tally tally("lengths held to their measurement");
		for (std::size_t k = 0; k < measured.size(); k++) {
			std::uint64_t n = measured[k];
			const std::vector<Line>& probes = by[k];
			std::string wrong;
			if (probes.size() < ibdwt::BACKOFF + 2) {
				tally.add(false, "length " + std::to_string(n) + ": " + std::to_string(probes.size()) + " probes");
				continue;
			}
			for (std::size_t i = 0; i < probes.size() and wrong.empty(); i++) {
				const Line& l = probes[i];
				bool last = i + 1 == probes.size();
				bool held = l.distance <= ibdwt::SIZING_DISTANCE and l.magnitude < ibdwt::SIZING_MAGNITUDE;
				if (held == last)
					wrong = where_probe(l) + (last ? " holds, but the scan must end at the first failure" : " fails before the last");
				else if (l.outputs != l.squarings * n)
					wrong = where_probe(l) + " counts outputs that are not its squarings times the length";
				else if (last ? (l.squarings < 1 or l.squarings > planned(n)) : l.squarings != planned(n))
					wrong = where_probe(l) + " took " + std::to_string(l.squarings) + " squarings, not the protocol's "
					        + std::to_string(planned(n));
				else if (not (l.rms > 0 and l.rms <= l.magnitude and l.distance >= 0))
					wrong = where_probe(l) + ": its rms is not in (0, magnitude] or its distance is negative";
				std::uint64_t at = (std::uint64_t(ibdwt::GRID_STEPS) * l.exponent) / n;
				if (wrong.empty() and (grid(n, at) != l.exponent or l.exponent < std::max<std::uint64_t>(3, n)))
					wrong = where_probe(l) + ": not an exponent of the grid ceil(N i / " + std::to_string(ibdwt::GRID_STEPS) + ")";
				if (wrong.empty() and not last and probes[i + 1].exponent != grid(n, at + 1))
					wrong = where_probe(l) + ": the next probe is not the next exponent of the grid";
			}
			const Line& chosen = probes[probes.size() - 2 - ibdwt::BACKOFF];
			if (wrong.empty() and ibdwt::largest_exponent(n) != chosen.exponent)
				wrong = "length " + std::to_string(n) + ": the table says " + std::to_string(ibdwt::largest_exponent(n))
				        + ", the measurement " + std::to_string(chosen.exponent);
			if (wrong.empty() and ibdwt::rms_envelope(n) != chosen.rms)
				wrong = "length " + std::to_string(n) + ": the envelope is not the rms of the probe the table takes";
			tally.add(wrong.empty(), wrong);
		}
		tally.expect_none();

		Tally holders("lengths assigned exponents, each held first by them");
		for (const Assigned& a: assigned()) {
			std::string wrong;
			if (a.first > a.last)
				wrong = "length " + std::to_string(a.length) + " is assigned no exponent";
			else if (ibdwt::length_for(a.first) != a.length or ibdwt::length_for(a.last) != a.length)
				wrong = "length " + std::to_string(a.length) + " is not the smallest holder of its assigned exponents";
			else if (a.first > 3 and ibdwt::length_for(a.first - 1) >= a.length)
				wrong = "exponent " + std::to_string(a.first - 1) + " is given length " + std::to_string(a.length) + " or above";
			holders.add(wrong.empty(), wrong);
		}
		holders.expect_none();

		std::uint64_t p_max = ibdwt::largest_exponent(ibdwt::LARGEST_LENGTH);
		check::expect(p_max >= 10'000'019, "P_max, " + std::to_string(p_max) + ", to be at least 10,000,019");
		check::expect(ibdwt::holds(ibdwt::LARGEST_LENGTH, p_max) and not ibdwt::holds(ibdwt::LARGEST_LENGTH, p_max + 1),
		              "the largest length to hold P_max and not P_max + 1");
	}

}
