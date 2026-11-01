// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Checks: what the engine's two tiers share
//


// Standard libs
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
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
#include "vector_reader.hpp"


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



	static std::string at(std::uint64_t p, std::uint64_t n) {
		return "exponent " + std::to_string(p) + " at length " + std::to_string(n);
	}


	std::vector<std::uint64_t> sample_lengths(const std::vector<std::uint64_t>& powers) {
		std::vector<std::uint64_t> out;
		for (std::uint64_t m = 1; m <= 15; m += 2)
			for (std::uint64_t k: powers)
				if ((m << k) <= ibdwt::LARGEST_LENGTH)
					out.push_back(m << k);
		std::sort(out.begin(), out.end());
		out.erase(std::unique(out.begin(), out.end()), out.end());
		return out;
	}


	Assigned assigned_to(std::uint64_t length) {
		for (const Assigned& span: assigned())
			if (span.length == length)
				return span;
		check::require(false, "length " + std::to_string(length) + " to be supported");
		return {length, 1, 0};
	}


	BigInt random_residue(std::uint64_t p, Random& random) {
		std::vector<std::uint64_t> limbs((p + 63) / 64);
		for (std::uint64_t& limb: limbs)
			limb = random.next();
		if (p % 64 != 0)
			limbs.back() &= (std::uint64_t(1) << (p % 64)) - 1;
		BigInt x = BigInt::from_limbs(limbs);
		return x == mersenne(p) ? BigInt() : x;
	}


	double distance_of(const std::vector<double>& outputs) {
		double distance = 0;
		for (double v: outputs)
			distance = std::max(distance, std::fabs(v - std::round(v)));
		return distance;
	}


	double rms_of(const std::vector<double>& outputs) {
		double sum = 0;
		for (double v: outputs)
			sum += v * v;
		return outputs.empty() ? 0 : std::sqrt(sum / double(outputs.size()));
	}


	ibdwt::Residue pattern(const ibdwt::Transform& transform, int family, double fraction) {
		const std::vector<std::uint64_t>& c = transform.positions();
		const std::uint64_t n = transform.length();
		const double two_pi = 6.283185307179586;
		ibdwt::Residue x;
		for (std::uint64_t j = 0; j < n; j++) {
			double m = 1;
			switch (family) {
				case 1: m = j % 2 ? -1 : 1; break;
				case 2: m = j % 3 == 2 ? -1 : 1; break;
				case 3: m = j % 4 < 2 ? 1 : -1; break;
				case 4: m = j % 5 == 0 ? -1 : 1; break;
				case 5: m = j % 6 < 3 ? 1 : -1; break;
				case 6: m = 2.0 * double(j % 8) / 7.0 - 1; break;
				case 7: m = std::cos(two_pi * double(j) / double(n)); break;
				case 8: m = std::cos(two_pi * double(j) * double(n / 4) / double(n) + 0.3); break;
			}
			std::int64_t top = (std::int64_t(1) << (c[j + 1] - c[j] - 1)) - 1;
			x.digits.push_back(std::int64_t(std::llround(fraction * double(top) * m)));
		}
		return x;
	}


	void Digest::add(const BigInt& x) {
		std::string bytes;
		for (std::uint64_t limb: x.limbs())
			for (int i = 0; i < 8; i++)
				bytes.push_back(char((limb >> (8 * i)) & 0xff));
		digests_ += vectors::sha256(bytes);
	}


	void Digest::print(const std::string& what) const {
		std::cout << "   residues " << what << ": " << vectors::sha256(digests_) << std::endl;
	}


	void expect_round_trips(const std::vector<std::uint64_t>& lengths, std::uint64_t seed) {
		//
		// Every case goes through to_digits and from_digits both, and agrees only if the digits
		// are balanced and both directions give the value BigInt gives

		Random random(seed);
		Tally tally("round trips");
		for (std::uint64_t n: lengths) {
			Assigned span = assigned_to(n);
			for (std::uint64_t p: {span.first, span.last}) {
				ibdwt::Engine engine(p, n);
				const ibdwt::Transform& transform = engine.transform();
				const std::vector<std::uint64_t>& c = transform.positions();
				const BigInt m = mersenne(p);
				const std::vector<BigInt> residues = {BigInt(), BigInt(1), subtract(m, BigInt(1)), m, random_residue(p, random),
				                                      random_residue(p, random)};
				const char* names[] = {"0", "1", "M_p - 1", "M_p", "pseudorandom", "pseudorandom"};
				for (std::size_t i = 0; i < residues.size(); i++) {
					const BigInt want = residues[i] == m ? BigInt() : residues[i];
					bool agree = false;
					try {
						ibdwt::Residue r = ibdwt::to_digits(engine, residues[i]);
						agree = balanced(transform, r) and value_of(c, r.digits, p) == want and ibdwt::from_digits(engine, r) == want;
					} catch (std::exception const&) {
					}
					tally.add(agree, at(p, n) + ": " + names[i]);
				}

				std::vector<std::vector<std::int64_t>> vectors(4);
				for (std::uint64_t j = 0; j < n; j++) {
					std::int64_t half = std::int64_t(1) << (c[j + 1] - c[j] - 1);
					vectors[0].push_back(-half);
					vectors[1].push_back(half - 1);
					vectors[2].push_back(j % 2 ? half - 1 : -half);
					vectors[3].push_back(2 * half - 1);
				}
				const char* shapes[] = {"every digit at the bottom", "every digit at the top", "digits alternating bottom and top",
				                        "every digit all ones (M_p)"};
				for (std::size_t i = 0; i < vectors.size(); i++) {
					const BigInt want = value_of(c, vectors[i], p);
					bool agree = false;
					try {
						BigInt got = ibdwt::from_digits(engine, ibdwt::Residue{vectors[i]});
						ibdwt::Residue r = ibdwt::to_digits(engine, got);
						agree = got == want and (i != 3 or got.is_zero()) and balanced(transform, r)
						        and ibdwt::from_digits(engine, r) == want;
					} catch (std::exception const&) {
					}
					tally.add(agree, at(p, n) + ": " + shapes[i]);
				}
			}
		}
		tally.expect_none();
	}


	void expect_squarings(const std::vector<std::uint64_t>& lengths, std::uint64_t seed, Digest& digest) {
		Random random(seed);
		Tally tally("squarings");
		for (std::uint64_t n: lengths) {
			Assigned span = assigned_to(n);
			for (std::uint64_t p: {span.first, span.last}) {
				ibdwt::Engine engine(p, n);
				for (int i = 0; i < 2; i++) {
					const BigInt x = random_residue(p, random), square = reduce_mersenne(::square(x), p);
					const std::uint64_t bits[] = {0, 1, p - 1, random.next() % p};
					for (int v = -1; v < 4; v++) {
						const bool subtract = v >= 0;
						const std::uint64_t bit = subtract ? bits[v] : 0;
						const BigInt want = subtract ? less_power(square, bit, p) : square;
						std::string what = at(p, n) + (subtract ? ": square less 2^" + std::to_string(bit) : ": square");
						bool agree = false;
						try {
							ibdwt::Residue r = ibdwt::to_digits(engine, x);
							const bool loaded = ibdwt::from_digits(engine, r) == x;
							if (subtract)
								ibdwt::square_subtract(engine, r, bit);
							else
								ibdwt::square(engine, r);
							BigInt got = ibdwt::from_digits(engine, r);
							digest.add(got);
							agree = loaded and balanced(engine.transform(), r) and got == want;
						} catch (std::exception const& error) {
							what += std::string(", raised ") + error.what();
						}
						tally.add(agree, what);
					}
				}
			}
		}
		tally.expect_none();
	}


	void expect_products(const std::vector<std::uint64_t>& lengths, std::uint64_t seed, Digest& digest) {
		Random random(seed);
		Tally tally("products");
		for (std::uint64_t n: lengths) {
			Assigned span = assigned_to(n);
			for (std::uint64_t p: {span.first, span.last}) {
				ibdwt::Engine engine(p, n);
				for (int i = 0; i < 2; i++) {
					const BigInt x = random_residue(p, random), y = random_residue(p, random);
					const BigInt want = reduce_mersenne(::multiply(x, y), p);
					std::string what = at(p, n) + ": product";
					bool agree = false;
					try {
						ibdwt::Residue r = ibdwt::to_digits(engine, x), s = ibdwt::to_digits(engine, y);
						const ibdwt::Residue before = s;
						const bool loaded = ibdwt::from_digits(engine, r) == x and ibdwt::from_digits(engine, s) == y;
						ibdwt::multiply(engine, r, s);
						BigInt got = ibdwt::from_digits(engine, r);
						digest.add(got);
						agree = loaded and balanced(engine.transform(), r) and got == want and s.digits == before.digits;
					} catch (std::exception const& error) {
						what += std::string(", raised ") + error.what();
					}
					tally.add(agree, what);
				}
			}
		}
		tally.expect_none();
	}


	static double operate(int op, const ibdwt::Engine& engine, ibdwt::Residue& x, const ibdwt::Residue& y, std::uint64_t bit) {
		//
		// A square (0), a square less 2^bit (1) or a product with y (2)

		if (op == 0)
			return ibdwt::square(engine, x);
		if (op == 1)
			return ibdwt::square_subtract(engine, x, bit);
		return ibdwt::multiply(engine, x, y);
	}


	void expect_monitor(const std::vector<std::uint64_t>& lengths, std::uint64_t seed) {
		Random random(seed);
		Tally reported("reported distances"), refused("refusals below the outputs' own distance"),
		      top("top-of-range squarings refused or right");
		const char* names[] = {"square", "square less 2^b", "product"};
		std::size_t exact = 0, by_distance = 0;
		for (std::uint64_t n: lengths) {
			Assigned span = assigned_to(n);
			for (std::uint64_t p: {span.first, span.last}) {
				ibdwt::Engine engine(p, n);
				const ibdwt::Transform& transform = engine.transform();
				const ibdwt::Residue x = random_digits(transform, random), y = random_digits(transform, random);
				const std::uint64_t bit = random.next() % p;
				for (int op = 0; op < 3; op++) {
					std::string what = at(p, n) + ", " + names[op];
					const double own = distance_of(op == 2 ? ibdwt::convolve(transform, x, y) : ibdwt::convolve_square(transform, x));
					ibdwt::Residue r = x, s = y;
					try {
						double got = operate(op, engine, r, s, bit);
						reported.add(got == own, what + ": reported " + std::to_string(got) + ", the outputs' own " + std::to_string(own));
					} catch (std::exception const& error) {
						reported.add(false, what + ": raised " + error.what());
					}
					if (own == 0) {
						exact++;
						continue;
					}
					ibdwt::Engine strict(p, n, own / 2);
					r = x;
					std::string wrong = "accepted";
					try {
						operate(op, strict, r, s, bit);
					} catch (roundoff_error const& error) {
						std::string text = error.what();
						bool names_all = text.find(std::to_string(p)) != std::string::npos and text.find(std::to_string(n)) != std::string::npos;
						if (error.exponent != p or error.length != n or error.distance != own or error.threshold != own / 2 or not names_all)
							wrong = "the error says: " + text;
						else if (r.digits != x.digits or s.digits != y.digits)
							wrong = "the residue or the operand changed";
						else
							wrong.clear();
					} catch (std::exception const& error) {
						wrong = std::string("raised ") + error.what();
					}
					refused.add(wrong.empty(), what + " at threshold half its distance: " + wrong);
				}

				// Every digit at the top of its range: refused, the residue unchanged, or squared right
				if (p == span.last) {
					ibdwt::Residue r;
					for (std::uint64_t j = 0; j + 1 < transform.positions().size(); j++) {
						const std::vector<std::uint64_t>& c = transform.positions();
						r.digits.push_back((std::int64_t(1) << (c[j + 1] - c[j] - 1)) - 1);
					}
					const ibdwt::Residue before = r;
					std::string wrong;
					try {
						ibdwt::square(engine, r);
						BigInt value = value_of(transform.positions(), before.digits, p);
						if (ibdwt::from_digits(engine, r) != reduce_mersenne(::square(value), p))
							wrong = "squared wrong without a refusal";
					} catch (roundoff_error const& error) {
						if (r.digits != before.digits)
							wrong = "refused, but the residue changed";
						else if (error.distance > error.threshold)
							by_distance++;
					} catch (std::exception const& error) {
						wrong = std::string("raised ") + error.what();
					}
					top.add(wrong.empty(), at(p, n) + ": every digit at the top: " + wrong);
				}
			}
		}
		reported.expect_none();
		refused.expect_none();
		top.expect_none();
		std::cout << "   exact outputs: " << exact << " cases with no distance to refuse below" << std::endl;
		check::expect(by_distance > 0, "an input with every digit at the top to be refused for its distance at some length");
	}


	void expect_repeats(const std::vector<std::uint64_t>& lengths, std::uint64_t seed) {
		Random random(seed);
		Tally tally("repeats");
		const char* names[] = {"square", "square less 2^1", "product"};
		for (std::uint64_t n: lengths) {
			Assigned span = assigned_to(n);
			for (std::uint64_t p: {span.first, span.last}) {
				ibdwt::Engine one(p, n), two(p, n);
				const ibdwt::Residue x = random_digits(one.transform(), random), y = random_digits(one.transform(), random);
				for (int op = 0; op < 3; op++) {
					bool agree = false;
					try {
						ibdwt::Residue a = x, b = x, c = x;
						double da = operate(op, one, a, y, 1), db = operate(op, two, b, y, 1), dc = operate(op, one, c, y, 1);
						agree = a.digits == b.digits and b.digits == c.digits and da == db and db == dc;
					} catch (std::exception const&) {
					}
					tally.add(agree, at(p, n) + ", " + names[op] + ": not the same twice");
				}
			}
		}
		tally.expect_none();
	}


	Outcomes expect_structured(const std::vector<std::uint64_t>& lengths, const std::vector<int>& families,
	                           const std::vector<double>& fractions) {
		Tally tally("structured squarings refused or right");
		std::size_t refusals = 0, accepted = 0;
		for (std::uint64_t n: lengths) {
			Assigned span = assigned_to(n);
			for (std::uint64_t p: {span.first, span.last}) {
				ibdwt::Engine engine(p, n);
				const ibdwt::Transform& transform = engine.transform();
				for (int family: families)
					for (double fraction: fractions) {
						const ibdwt::Residue x = pattern(transform, family, fraction);
						const double size = rms_of(ibdwt::convolve_square(transform, x));
						std::string what = at(p, n) + ", family " + std::to_string(family) + ", fraction " + std::to_string(fraction);
						ibdwt::Residue r = x;
						std::string wrong;
						try {
							ibdwt::square(engine, r);
							accepted++;
							BigInt want = reduce_mersenne(::square(value_of(transform.positions(), x.digits, p)), p);
							if (ibdwt::from_digits(engine, r) != want)
								wrong = "squared wrong without a refusal";
							else if (not (size <= engine.rms_limit()))
								wrong = "accepted with a root mean square past the limit";
						} catch (roundoff_error const&) {
							refusals++;
							if (r.digits != x.digits)
								wrong = "refused, but the residue changed";
						} catch (std::exception const& error) {
							wrong = std::string("raised ") + error.what();
						}
						tally.add(wrong.empty(), what + ": " + wrong);
					}
			}
		}
		tally.expect_none();
		std::cout << "   structured: " << refusals << " refused, " << accepted << " squared" << std::endl;
		return {refusals, accepted};
	}

}
