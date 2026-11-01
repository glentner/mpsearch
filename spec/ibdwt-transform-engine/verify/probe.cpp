// Gate probe: a cycle record, never part of mps.
//
// The gates' own driver of the engine, apart from the builder's checks: it links the tree's
// modules and calls only the pinned interface of src/ibdwt.hpp and src/lengths.hpp (PLAN § 2), and
// computes every expected value on BigInt itself. Usage:
//
//     probe PHASE quick|full
//     probe measure FILE
//
// From phase 1: the refusals of unsupported lengths and of exponents a length does not hold, each
// naming both (R8), and the table's assignment of every exponent from 3 to P_max to its smallest
// holding length (R3). From phase 2, at sample lengths (each odd m at k = 0, 1, 5 and 10, and in
// full mode the largest m 2^k of each m too), at the smallest and largest exponent the table
// assigns: round trips of 0, 1, M_p - 1, M_p and a pseudorandom residue through balanced digits
// (R1); the square, the square less 2^e for e = 0, 1, p - 1, and a product, against BigInt (R4,
// R14); every operation's reported distance against the distance of its own outputs, computed here
// from convolve and convolve_square (R6); a refusal below that distance, leaving the residue and
// the operands unchanged (R7, R14); a repeat from a fresh engine, bit for bit (R9); and inputs with
// every digit at the top of its range, which must be refused at the default threshold at the
// largest length (their outputs pass 2^52, where no distance can be seen) and refused for their
// distance at one or more sample lengths at least; every one of those squarings, where accepted,
// equal to BigInt's. Structured inputs (constant, alternating, period 3 to 6 and 8 patterns, tones)
// at ten fractions of the digits' range, at the sample lengths (quick) or every length up to 4,096
// words and the largest of each m (full): each refused, the residue untouched, or squared exactly
// right, and refused whenever its outputs' root mean square (computed here) passes the engine's
// limit. In full mode, a squaring at every supported length's largest exponent compared with
// BigInt in full. Prints `probe: ok <counts>` and then `probe: digest <FNV-1a of every residue>`,
// or each violation and exit status 1.
//
// `probe measure FILE` is the gate's own measurement: for each "<length> <exponent> <squarings>"
// line it squares that many vectors of balanced digits from its own generator (xorshift64*, not
// the measurement's SplitMix64) through the engine's convolution and takes each squaring's
// distance itself; a line fails when any distance passes 7/16 or more than a sixteenth of them pass
// 0.35. Prints one line per length and exit status 1 on any failure.


// Standard libs
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Internal libs
#include "ibdwt.hpp"
#include "lengths.hpp"
#include "mp.hpp"


// The engine's internal pieces, in no header: declared here as the checks declare them
namespace ibdwt {
	std::vector<double> convolve(const Transform& transform, const Residue& x, const Residue& y);
	std::vector<double> convolve_square(const Transform& transform, const Residue& x);
}


static int failures = 0;
static int passes = 0;
static std::uint64_t checksum = 1469598103934665603ull;


static void expect(bool ok, const std::string& what) {
	if (ok) {
		passes++;
		return;
	}
	failures++;
	if (failures <= 20)
		std::cout << "probe: FAIL " << what << std::endl;
}


static void fold(const BigInt& x) {
	// FNV-1a over the limbs, for comparing two builds' residues
	for (std::uint64_t limb: x.limbs())
		for (int i = 0; i < 8; i++) {
			checksum ^= (limb >> (8 * i)) & 0xff;
			checksum *= 1099511628211ull;
		}
	checksum ^= 0xff;
	checksum *= 1099511628211ull;
}


static std::uint64_t state = 0x6a7e5eedull;
static std::uint64_t next() {
	std::uint64_t z = (state += 0x9e3779b97f4a7c15ull);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
	return z ^ (z >> 31);
}


static BigInt random_below_mersenne(std::uint64_t p) {
	std::vector<std::uint64_t> l((p + 63) / 64);
	for (std::uint64_t& v: l)
		v = next();
	if (p % 64)
		l.back() &= (std::uint64_t(1) << (p % 64)) - 1;
	BigInt x = BigInt::from_limbs(l);
	return x == mersenne(p) ? BigInt() : x;
}


static BigInt fold_mersenne(BigInt r, std::uint64_t p) {
	// r mod M_p by folding the bits above the low p onto them (2^p = 1), written here
	BigInt m = mersenne(p);
	while (r.bit_length() > p) {
		BigInt high = shift_right(r, p);
		r = add(subtract(r, shift_left(high, p)), high);
	}
	return r == m ? BigInt() : r;
}


static BigInt sq(const BigInt& x, std::uint64_t p, bool less, std::uint64_t e) {
	// x^2 mod M_p, less 2^e
	BigInt r = fold_mersenne(square(x), p);
	if (less) {
		BigInt power = shift_left(BigInt(1), e);
		if (compare(r, power) < 0)
			r = add(r, mersenne(p));
		r = subtract(r, power);
	}
	return r;
}


static BigInt value_of(const std::vector<std::uint64_t>& c, const std::vector<std::int64_t>& d, std::uint64_t p) {
	// sum_j d_j 2^(c_j) mod M_p, by shifts and sums, whatever the digits' sizes
	BigInt plus, minus;
	for (std::size_t j = 0; j < d.size(); j++) {
		std::uint64_t a = d[j] < 0 ? std::uint64_t(-(d[j] + 1)) + 1 : std::uint64_t(d[j]);
		if (a == 0)
			continue;
		BigInt t = shift_left(BigInt(a), c[j]);
		if (d[j] < 0)
			minus = add(minus, t);
		else
			plus = add(plus, t);
	}
	plus = fold_mersenne(plus, p);
	minus = fold_mersenne(minus, p);
	if (compare(plus, minus) < 0)
		plus = add(plus, mersenne(p));
	return fold_mersenne(subtract(plus, minus), p);
}


static std::int64_t pattern_digit(int family, double fraction, std::int64_t top, std::uint64_t j, std::uint64_t n) {
	double m = 1;
	switch (family) {
		case 1: m = j % 2 ? -1 : 1; break;
		case 2: m = j % 3 == 2 ? -1 : 1; break;
		case 3: m = j % 4 < 2 ? 1 : -1; break;
		case 4: m = j % 5 == 0 ? -1 : 1; break;
		case 5: m = j % 6 < 3 ? 1 : -1; break;
		case 6: m = 2.0 * double(j % 8) / 7.0 - 1; break;
		case 7: m = std::cos(6.283185307179586 * double(j) / double(n)); break;
		case 8: m = std::cos(6.283185307179586 * double(j) * double(n / 4) / double(n) + 0.3); break;
	}
	return std::int64_t(std::llround(fraction * double(top) * m));
}


static int measure(const char* path) {
	FILE* in = std::fopen(path, "r");
	if (not in) {
		std::cout << "probe: FAIL cannot read " << path << std::endl;
		return 1;
	}
	unsigned long long n = 0, p = 0, k = 0;
	int bad = 0;
	std::uint64_t xs = 0x2545f4914f6cdd1dull;
	while (std::fscanf(in, "%llu %llu %llu", &n, &p, &k) == 3) {
		ibdwt::Transform t(p, n);
		const std::vector<std::uint64_t>& c = t.positions();
		ibdwt::Residue x;
		x.digits.resize(n);
		std::uint64_t over = 0, beyond = 0;
		double worst = 0;
		for (unsigned long long i = 0; i < k; i++) {
			for (std::uint64_t j = 0; j < n; j++) {
				xs ^= xs >> 12;
				xs ^= xs << 25;
				xs ^= xs >> 27;
				std::uint64_t r = xs * 0x2545f4914f6cdd1dull, s = c[j + 1] - c[j];
				x.digits[j] = std::int64_t(r >> (64 - s)) - (std::int64_t(1) << (s - 1));
			}
			double d = 0;
			for (double v: ibdwt::convolve_square(t, x))
				d = std::isfinite(v) ? std::max(d, std::fabs(v - std::round(v))) : 1;
			worst = std::max(worst, d);
			over += d > 0.35;
			beyond += d > 0.4375;
		}
		bool ok = beyond == 0 and over * 16 <= k;
		bad += not ok;
		std::printf("probe: measure %llu %llu: %llu squarings, %llu past 0.35, %llu past 7/16, largest %.4f%s\n", n, p, k,
		            (unsigned long long)over, (unsigned long long)beyond, worst, ok ? "" : " FAIL");
	}
	std::fclose(in);
	return bad ? 1 : 0;
}


static std::string at(std::uint64_t p, std::uint64_t n) {
	return "exponent " + std::to_string(p) + " at length " + std::to_string(n);
}


static bool refused(const std::function<void()>& call, std::uint64_t p, std::uint64_t n) {
	try {
		call();
	} catch (engine_error const& error) {
		std::string text = error.what();
		return text.find(std::to_string(p)) != std::string::npos and text.find(std::to_string(n)) != std::string::npos;
	} catch (...) {
		return false;
	}
	return false;
}


static std::vector<std::uint64_t> all_lengths() {
	std::vector<std::uint64_t> v;
	for (std::uint64_t n = 1; n <= (1u << 20); n++) {
		std::uint64_t m = n;
		while (m % 2 == 0)
			m /= 2;
		if (m <= 15)
			v.push_back(n);
	}
	return v;
}


static int run(int argc, char** argv);


int main(int argc, char** argv) {
	try {
		return run(argc, argv);
	} catch (std::exception const& error) {
		std::cout << "probe: FAIL an unexpected exception: " << error.what() << std::endl;
		return 1;
	}
}


static int run(int argc, char** argv) {
	if (argc == 3 and std::string(argv[1]) == "measure")
		return measure(argv[2]);
	if (argc != 3) {
		std::cerr << "usage: probe PHASE quick|full" << std::endl;
		return 2;
	}
	int phase = std::atoi(argv[1]);
	bool full = std::string(argv[2]) == "full";
	std::vector<std::uint64_t> lengths = all_lengths();

	// The table: every supported length, its largest exponent, the exponents it is assigned
	std::vector<std::uint64_t> first(lengths.size()), last(lengths.size());
	std::uint64_t below = 2;
	for (std::size_t i = 0; i < lengths.size(); i++) {
		std::uint64_t n = lengths[i], top = 0;
		expect(ibdwt::is_supported(n), "length " + std::to_string(n) + " supported");
		try {
			top = ibdwt::largest_exponent(n);
		} catch (std::exception const&) {
			expect(false, "length " + std::to_string(n) + " has a largest exponent");
			continue;
		}
		first[i] = std::max({below + 1, n, std::uint64_t(3)});
		last[i] = top;
		expect(first[i] <= top, "length " + std::to_string(n) + " is assigned an exponent");
		below = std::max(below, top);
	}
	std::uint64_t p_max = below;
	expect(p_max >= 10'000'019, "P_max at least 10,000,019");
	for (std::size_t i = 0; i < lengths.size(); i++)
		for (std::uint64_t p: {first[i], last[i]})
			expect(ibdwt::length_for(p) == lengths[i], "length_for(" + std::to_string(p) + ") is " + std::to_string(lengths[i]));
	for (std::uint64_t n: {0ull, 17ull, 2'097'152ull, 19'456ull, 1'048'577ull})
		expect(not ibdwt::is_supported(n) and refused([n] { ibdwt::Engine(100'000, n); }, 100'000, n),
		       "length " + std::to_string(n) + " refused, naming both");
	for (std::size_t i = 0; i < lengths.size(); i += 7) {
		std::uint64_t n = lengths[i], over = last[i] + 1;
		expect(refused([n, over] { ibdwt::Engine(over, n); }, over, n), at(over, n) + " refused, naming both");
	}
	expect(refused([] { ibdwt::Engine(2, 1); }, 2, 1), at(2, 1) + " refused");
	expect(refused([] { ibdwt::Engine(1000, 1024); }, 1000, 1024), at(1000, 1024) + " refused");

#if GATE_PHASE >= 2
	if (phase >= 2) {
		std::vector<std::uint64_t> sample;
		for (std::uint64_t m = 1; m <= 15; m += 2) {
			for (std::uint64_t k: {0, 1, 5, 10})
				sample.push_back(m << k);
			if (full) {
				std::uint64_t top = m;
				while (top * 2 <= (1u << 20))
					top *= 2;
				sample.push_back(top);
			}
		}
		std::size_t by_distance = 0;
		for (std::uint64_t n: sample) {
			std::size_t i = 0;
			while (lengths[i] != n)
				i++;
			for (std::uint64_t p: {first[i], last[i]}) {
				ibdwt::Engine engine(p, n);
				const std::vector<std::uint64_t>& c = engine.transform().positions();
				BigInt m = mersenne(p);
				auto balanced = [&](const ibdwt::Residue& r) {
					if (r.digits.size() != n)
						return false;
					for (std::uint64_t j = 0; j < n; j++) {
						std::int64_t half = std::int64_t(1) << (c[j + 1] - c[j] - 1);
						if (r.digits[j] < -half or r.digits[j] >= half)
							return false;
					}
					return true;
				};
				for (const BigInt& x: {BigInt(), BigInt(1), subtract(m, BigInt(1)), random_below_mersenne(p)}) {
					ibdwt::Residue r = ibdwt::to_digits(engine, x);
					expect(balanced(r) and ibdwt::from_digits(engine, r) == x, at(p, n) + ": round trip");
				}
				ibdwt::Residue zero = ibdwt::to_digits(engine, m);
				expect(balanced(zero) and ibdwt::from_digits(engine, zero).is_zero(), at(p, n) + ": M_p reads back as 0");

				BigInt x = random_below_mersenne(p), y = random_below_mersenne(p);
				for (int e = -1; e < 3; e++) {
					std::uint64_t bit = e == 2 ? p - 1 : std::uint64_t(e < 0 ? 0 : e);
					ibdwt::Residue r = ibdwt::to_digits(engine, x);
					std::vector<double> v = ibdwt::convolve_square(engine.transform(), r);
					double own = 0;
					for (double value: v)
						own = std::max(own, std::fabs(value - std::round(value)));
					double got = e < 0 ? ibdwt::square(engine, r) : ibdwt::square_subtract(engine, r, bit);
					BigInt result = ibdwt::from_digits(engine, r);
					fold(result);
					expect(balanced(r) and result == sq(x, p, e >= 0, bit),
					       at(p, n) + (e < 0 ? ": square" : ": square less 2^" + std::to_string(bit)));
					expect(got == own, at(p, n) + ": reported distance " + std::to_string(got) + " is its own outputs' " + std::to_string(own));
				}
				ibdwt::Residue r = ibdwt::to_digits(engine, x), s = ibdwt::to_digits(engine, y);
				std::vector<double> v = ibdwt::convolve(engine.transform(), r, s);
				double own = 0;
				for (double value: v)
					own = std::max(own, std::fabs(value - std::round(value)));
				double got = ibdwt::multiply(engine, r, s);
				BigInt product = ibdwt::from_digits(engine, r);
				fold(product);
				expect(product == fold_mersenne(multiply(x, y), p) and got == own, at(p, n) + ": product");

				// Refusal below its own distance, everything left as it was
				r = ibdwt::to_digits(engine, x);
				v = ibdwt::convolve(engine.transform(), r, s);
				own = 0;
				for (double value: v)
					own = std::max(own, std::fabs(value - std::round(value)));
				if (own > 0) {
					ibdwt::Engine strict(p, n, own / 2);
					ibdwt::Residue before = r, other = s;
					bool raised = false;
					try {
						ibdwt::multiply(strict, r, s);
					} catch (roundoff_error const& error) {
						raised = error.exponent == p and error.length == n and error.distance == own and error.threshold == own / 2;
					}
					expect(raised and r.digits == before.digits and s.digits == other.digits, at(p, n) + ": refusal leaves the operands");
				}

				// Repeat
				ibdwt::Engine again(p, n);
				ibdwt::Residue a = ibdwt::to_digits(engine, x), b = ibdwt::to_digits(again, x);
				double da = ibdwt::square_subtract(engine, a, 1), db = ibdwt::square_subtract(again, b, 1);
				expect(a.digits == b.digits and da == db, at(p, n) + ": repeat");

				// Every digit at the top of its range
				if (p == last[i]) {
					ibdwt::Residue top;
					for (std::uint64_t j = 0; j < n; j++)
						top.digits.push_back((std::int64_t(1) << (c[j + 1] - c[j] - 1)) - 1);
					ibdwt::Residue before = top;
					bool raised = false, distance = false;
					try {
						ibdwt::square(engine, top);
						expect(ibdwt::from_digits(engine, top) == sq(value_of(c, before.digits, p), p, false, 0),
						       at(p, n) + ": top digits squared wrong without a refusal");
					} catch (roundoff_error const& error) {
						raised = true;
						distance = error.distance > error.threshold;
					} catch (std::exception const& error) {
						expect(false, at(p, n) + ": top digits raised '" + error.what() + "', not roundoff_error");
					}
					if (raised and distance and top.digits == before.digits)
						by_distance++;
					if (n == (1u << 20) or n == 15u << 16)
						expect(raised and top.digits == before.digits, at(p, n) + ": top digits refused");
				}
			}
		}
		expect(by_distance > 0, "an input with every digit at the top refused for its distance at a sample length");

		// Structured inputs: refused or squared right, and refused past the root-mean-square limit
		std::vector<std::uint64_t> shaped;
		if (full) {
			for (std::uint64_t n: lengths)
				if (n <= 4096)
					shaped.push_back(n);
		} else {
			shaped = sample;
		}
		std::size_t refusals = 0, squared = 0;
		auto structured = [&](std::uint64_t n, std::uint64_t p, const std::vector<int>& families, const std::vector<double>& fractions) {
			ibdwt::Engine engine(p, n);
			const std::vector<std::uint64_t>& c = engine.transform().positions();
			for (int f: families)
				for (double fraction: fractions) {
					ibdwt::Residue x;
					for (std::uint64_t j = 0; j < n; j++)
						x.digits.push_back(pattern_digit(f, fraction, (std::int64_t(1) << (c[j + 1] - c[j] - 1)) - 1, j, n));
					double sum = 0;
					for (double v: ibdwt::convolve_square(engine.transform(), x))
						sum += v * v;
					double own_rms = std::sqrt(sum / double(n));
					ibdwt::Residue r = x;
					std::string what = at(p, n) + ", family " + std::to_string(f) + ", fraction " + std::to_string(fraction);
					try {
						ibdwt::square(engine, r);
						squared++;
						BigInt got = ibdwt::from_digits(engine, r);
						fold(got);
						expect(got == sq(value_of(c, x.digits, p), p, false, 0), what + ": squared wrong without a refusal");
						expect(own_rms <= engine.rms_limit(), what + ": accepted past the root-mean-square limit");
					} catch (roundoff_error const&) {
						refusals++;
						expect(r.digits == x.digits, what + ": refused, but the residue changed");
					}
				}
		};
		for (std::uint64_t n: shaped) {
			std::size_t i = 0;
			while (lengths[i] != n)
				i++;
			for (std::uint64_t p: {first[i], last[i]})
				structured(n, p, {0, 1, 2, 3, 4, 5, 6, 7, 8}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0});
		}
		if (full)
			for (std::uint64_t m = 1; m <= 15; m += 2) {
				std::uint64_t top = m;
				while (top * 2 <= (1u << 20))
					top *= 2;
				std::size_t i = 0;
				while (lengths[i] != top)
					i++;
				structured(top, last[i], {0, 1}, {0.5, 1.0});
			}
		expect(refusals > 0 and squared > 0, "structured inputs both refused and squared");

		// Full mode: every length, a squaring less a pseudorandom bit at its largest exponent, in full
		if (full)
			for (std::size_t i = 0; i < lengths.size(); i++) {
				std::uint64_t n = lengths[i], p = last[i], bit = next() % p;
				ibdwt::Engine engine(p, n);
				BigInt x = random_below_mersenne(p);
				ibdwt::Residue r = ibdwt::to_digits(engine, x);
				ibdwt::square_subtract(engine, r, bit);
				BigInt got = ibdwt::from_digits(engine, r);
				fold(got);
				expect(got == sq(x, p, true, bit), at(p, n) + ": a squaring less 2^" + std::to_string(bit) + ", in full");
			}
	}
#endif

	if (failures > 0) {
		std::cout << "probe: " << failures << " failed, " << passes << " passed" << std::endl;
		return 1;
	}
	std::cout << "probe: ok (" << passes << " expectations, phase " << phase << (full ? ", full" : ", quick") << ")" << std::endl;
	char text[32];
	std::snprintf(text, sizeof text, "%016llx", (unsigned long long)checksum);
	std::cout << "probe: digest " << text << std::endl;
	return 0;
}
