// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Length measurement
//
// The command that measures the length table (src/lengths.hpp holds the protocol, src/lengths.txt
// what it measured, src/lengths.cpp the table). Never linked into bin/mps.
//
//     bin/measure                          every supported length: the file `make lengths` writes
//     bin/measure LENGTH ...               those lengths' probe lines alone, in the order given
//     bin/measure --probe LENGTH EXPONENT  one probe's line
//     bin/measure --table FILE             the table's two maps from a measurement, for src/lengths.cpp
//
// A probe's line is `<length> <exponent> <squarings> <outputs> <distance> <magnitude> <rms>`, the
// last three the largest over its squarings, printed with 17 significant digits so that a probe
// measured again reproduces its line byte for byte. Each probe's digits come from SplitMix64
// seeded by its length and exponent, so every length is measured the same way on every run, alone
// or with the others.
//


// Standard libs
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Internal libs
#include "ibdwt.hpp"
#include "lengths.hpp"


static const char* USAGE = R"USAGE(usage: bin/measure [LENGTH ...]
       bin/measure --probe LENGTH EXPONENT
       bin/measure --table FILE
)USAGE";


// The constant C of the sizing estimate, as Crandall, Mayer and Papadopoulos fit it
static const double ESTIMATE_C = 2.0;

// The double's mantissa, B_mant in the estimate
static const double MANTISSA_BITS = 53.0;


class SplitMix64 {
public:
	//
	// The generator of Steele, Lea and Flood, "Fast splittable pseudorandom number generators",
	// 2014: a Weyl sequence of increment 0x9e3779b97f4a7c15 (the golden gamma), each state mixed
	// to 64 output bits; the mixing constants are the ones the name SplitMix64 commonly denotes

	explicit SplitMix64(std::uint64_t seed): state_(seed) {}

	std::uint64_t next() {
		std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ull);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
		return z ^ (z >> 31);
	}

private:
	std::uint64_t state_;
};


struct Probe {
	std::uint64_t length = 0;
	std::uint64_t exponent = 0;
	std::uint64_t squarings = 0;
	double distance = 0;
	double magnitude = 0;
	double rms = 0;
	bool held = true;
};


static std::string line_of(const Probe& probe) {
	char text[256];
	std::snprintf(text, sizeof text, "%llu %llu %llu %llu %.17g %.17g %.17g", (unsigned long long)probe.length,
	              (unsigned long long)probe.exponent, (unsigned long long)probe.squarings,
	              (unsigned long long)(probe.squarings * probe.length), probe.distance, probe.magnitude, probe.rms);
	return text;
}


static std::uint64_t planned(std::uint64_t length) {
	//
	// A probe's squarings: PROBE_SQUARINGS, or enough for PROBE_OUTPUTS outputs if that is more

	return std::max(ibdwt::PROBE_SQUARINGS, (ibdwt::PROBE_OUTPUTS + length - 1) / length);
}


static Probe probe(std::uint64_t length, std::uint64_t p) {
	//
	// Square fresh pseudorandom balanced digits until the protocol's count or the first squaring
	// past a sizing limit, keeping the largest distance, |output| and root mean square

	ibdwt::Transform transform(p, length);
	const std::vector<std::uint64_t>& c = transform.positions();
	SplitMix64 random((length << 40) ^ p);
	ibdwt::Residue x;
	x.digits.resize(length);
	Probe result;
	result.length = length;
	result.exponent = p;
	for (std::uint64_t i = 0; i < planned(length); i++) {
		for (std::uint64_t j = 0; j < length; j++) {
			std::uint64_t s = c[j + 1] - c[j];
			x.digits[j] = std::int64_t(random.next() >> (64 - s)) - (std::int64_t(1) << (s - 1));
		}
		ibdwt::Statistics stats = ibdwt::sample(transform, x);
		result.squarings++;
		result.distance = std::max(result.distance, stats.distance);
		result.magnitude = std::max(result.magnitude, stats.magnitude);
		result.rms = std::max(result.rms, stats.rms);
		if (not (stats.distance <= ibdwt::SIZING_DISTANCE and stats.magnitude < ibdwt::SIZING_MAGNITUDE)) {
			result.held = false;
			break;
		}
	}
	return result;
}


static double estimate(std::uint64_t length) {
	//
	// Bits a word by the heuristic of Crandall, Mayer and Papadopoulos, "The twenty-fourth Fermat
	// number is composite", 2003, eq. (8): the largest log2 W with
	// 2 log2 W + log2 C + (log2 N + log2 log2 N) / 2 + (3/2) log2 log log N < B_mant. It only picks
	// where a length's probes start; lengths below 16 take the estimate at 16, where every
	// logarithm in it is positive.

	double n = std::max(double(length), 16.0), lg = std::log2(n);
	return (MANTISSA_BITS - std::log2(ESTIMATE_C) - (lg + std::log2(lg)) / 2 - 1.5 * std::log2(std::log(std::log(n)))) / 2;
}


static std::vector<Probe> scan(std::uint64_t length) {
	//
	// A length's probes: the grid exponents ceil(N i / GRID_STEPS), each once, from START_BELOW
	// bits a word below the estimate up to the first that fails; with fewer than BACKOFF + 1 held
	// before it, the start moves down a bit and the length is measured again

	const std::uint64_t steps = std::uint64_t(ibdwt::GRID_STEPS), least = std::max<std::uint64_t>(3, length);
	std::int64_t start = std::int64_t(std::floor((estimate(length) - ibdwt::START_BELOW) * double(steps)));
	for (;;) {
		std::vector<Probe> probes;
		std::uint64_t last = 0;
		for (std::uint64_t i = std::uint64_t(std::max<std::int64_t>(start, 0));; i++) {
			std::uint64_t q = (length * i + steps - 1) / steps;
			if (q < least or q <= last)
				continue;
			last = q;
			probes.push_back(probe(length, q));
			if (not probes.back().held)
				break;
		}
		if (probes.size() >= ibdwt::BACKOFF + 2)
			return probes;
		if (start <= 0)
			throw engine_error("length " + std::to_string(length) + ": no start holds enough probes");
		start -= std::int64_t(steps);
	}
}


static void header() {
	std::cout << "# mps lengths 2\n"
	          << "# command: make lengths (bin/measure >src/lengths.txt.new, then moved over src/lengths.txt;"
	          << " bin/measure LENGTH ... measures those lengths alone, bin/measure --probe LENGTH EXPONENT one probe)\n"
	          << "# protocol (src/lengths.hpp): a probe of (N, p) squares at least " << ibdwt::PROBE_SQUARINGS
	          << " vectors of fresh SplitMix64 balanced digits, seeded by N and p, and enough for " << ibdwt::PROBE_OUTPUTS
	          << " outputs, ending at the first squaring whose distance passes " << ibdwt::SIZING_DISTANCE
	          << " or whose largest |output| reaches 2^50; it holds if none did.\n"
	          << "# A length's probes are the exponents ceil(N i / " << ibdwt::GRID_STEPS << "), consecutive, from "
	          << ibdwt::START_BELOW << " bit a word below the Crandall-Mayer-Papadopoulos estimate (eq. (8), C = 2)"
	          << " to the first that fails, the start moving down a bit while fewer than " << ibdwt::BACKOFF + 1
	          << " hold before it.\n"
	          << "# The table gives each length the probe " << ibdwt::BACKOFF
	          << " grid points below the last held, and that probe's rms as its envelope (bin/measure --table).\n"
	          << "# <length> <exponent> <squarings> <outputs> <distance> <magnitude> <rms>\n";
}


static int measure(const std::vector<std::uint64_t>& lengths, bool whole) {
	if (whole)
		header();
	std::uint64_t count = 0;
	for (std::uint64_t length: lengths) {
		if (not ibdwt::is_supported(length))
			throw engine_error("length " + std::to_string(length) + " is not supported");
		for (const Probe& p: scan(length)) {
			std::cout << line_of(p) << "\n";
			count++;
		}
		std::cout.flush();
	}
	if (whole)
		std::cout << "# end " << count << " probes" << std::endl;
	return 0;
}


static int table(const std::string& path) {
	//
	// The two maps of src/lengths.cpp from a measurement: each length's probe BACKOFF below its
	// last held one (the last probe is the first that failed), and that probe's rms, as written

	std::ifstream in(path);
	if (not in)
		throw engine_error(path + ": does not open");
	std::vector<std::uint64_t> lengths;
	std::vector<std::vector<std::pair<std::uint64_t, std::string>>> probes;
	std::string line;
	while (std::getline(in, line)) {
		if (line.empty() or line[0] == '#')
			continue;
		std::istringstream fields(line);
		std::uint64_t n = 0, p = 0;
		std::string squarings, outputs, distance, magnitude, rms;
		if (not (fields >> n >> p >> squarings >> outputs >> distance >> magnitude >> rms))
			throw engine_error(path + ": not a probe line: " + line);
		if (lengths.empty() or lengths.back() != n) {
			lengths.push_back(n);
			probes.emplace_back();
		}
		probes.back().push_back({p, rms});
	}
	std::ostringstream exponents, envelopes;
	for (std::size_t i = 0; i < lengths.size(); i++) {
		if (probes[i].size() < ibdwt::BACKOFF + 2)
			throw engine_error(path + ": length " + std::to_string(lengths[i]) + " has too few probes");
		const auto& chosen = probes[i][probes[i].size() - 2 - ibdwt::BACKOFF];
		char text[64];
		std::snprintf(text, sizeof text, "\t{%7llu, %llu},\n", (unsigned long long)lengths[i], (unsigned long long)chosen.first);
		exponents << text;
		std::snprintf(text, sizeof text, "\t{%7llu, ", (unsigned long long)lengths[i]);
		envelopes << text << chosen.second << "},\n";
	}
	std::cout << "static std::map<std::uint64_t, std::uint64_t>\nlargest_exponent_by_length {\n" << exponents.str()
	          << "};\n\n\nstatic std::map<std::uint64_t, double>\nrms_envelope_by_length {\n" << envelopes.str() << "};\n";
	return 0;
}


static std::uint64_t number(const std::string& text) {
	if (text.empty() or text.find_first_not_of("0123456789") != std::string::npos or text.size() > 12)
		throw engine_error("not a number: '" + text + "'");
	return std::stoull(text);
}


static int run(const std::vector<std::string>& args) {
	if (args.size() == 3 and args[0] == "--probe") {
		std::cout << line_of(probe(number(args[1]), number(args[2]))) << std::endl;
		return 0;
	}
	if (args.size() == 2 and args[0] == "--table")
		return table(args[1]);
	if (args.empty())
		return measure(ibdwt::supported_lengths(), true);
	std::vector<std::uint64_t> lengths;
	for (const std::string& arg: args) {
		if (arg.empty() or arg[0] == '-') {
			std::cerr << USAGE;
			return 2;
		}
		lengths.push_back(number(arg));
	}
	return measure(lengths, false);
}


int main(int argc, char** argv) {
	try {
		return run(std::vector<std::string>(argv + 1, argv + argc));
	} catch (std::exception const& error) {
		std::cerr << "measure: " << error.what() << std::endl;
		return 1;
	}
}
