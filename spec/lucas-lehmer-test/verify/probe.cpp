// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// The gate's own probe (a cycle record, not a standing gate)
//
// Built by gate.sh against a tree's lib/*.o (bin/mps's objects, no bounds assertions) with
// -DGATE_PHASE=<n>, and run from the tree's root: `probe PHASE MODE`. It never trusts the checks
// under test. Every expected value is its own: a Jacobi symbol by the reciprocity loop on BigInt,
// residues by power_mod, the tables read through src/known.*, and the formats of PLAN § 2.5.
//   P1: jacobi against the probe's own symbol on residues modulo M_p and on general odd moduli,
//       and its errors; run_in_order against stand-in tests (the stop rule).
//   P2: the test's helpers against their definitions; the test on every exponent R1 and R2 cover in
//       make check at two given shifts (four for the small ones), its schedule traced through the
//       refuse hook; the random shift; the refusals of R14; and each injection R4 and R6-R11 name,
//       with the counts, the outcomes and the views of R12 and the log lines of R11 and R13.
// Prints `probe: FAIL <what>` for each failure (at most 40), then `probe: ok <n> cases` or
// `probe: failed <k> of <n>`, and exits 0 only when nothing failed.
//

// Standard libs
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// Internal libs
#include "known.hpp"
#include "main.hpp"
#include "math.hpp"
#include "mp.hpp"
#include "result.hpp"
#if GATE_PHASE >= 2
#include "ibdwt.hpp"
#include "lengths.hpp"
#include "lucas.hpp"
#endif


static std::size_t cases = 0, failures = 0;


static void expect(bool ok, const std::string& what) {
	cases++;
	if (ok)
		return;
	failures++;
	if (failures <= 40)
		std::cout << "probe: FAIL " << what << std::endl;
}


static std::uint64_t mix_state = 0x243F6A8885A308D3ull;
static std::uint64_t mix() {
	// SplitMix64 (Steele, Lea and Flood, 2014)
	std::uint64_t z = (mix_state += 0x9E3779B97F4A7C15ull);
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
	return z ^ (z >> 31);
}


static BigInt random_below_bits(std::size_t bits) {
	std::vector<std::uint64_t> limbs((bits + 63) / 64);
	for (auto& l: limbs)
		l = mix();
	if (bits % 64)
		limbs.back() &= (std::uint64_t(1) << (bits % 64)) - 1;
	return BigInt::from_limbs(limbs);
}


static bool is_odd(const BigInt& x) { return not x.is_zero() and (x.limbs()[0] & 1); }
static unsigned low(const BigInt& x, unsigned m) { return x.is_zero() ? 0 : unsigned(x.limbs()[0] % m); }


static int own_jacobi(BigInt a, BigInt n) {
	// The reciprocity loop: reduce, strip twos, swap
	a = reduce(a, n);
	int t = 1;
	while (not a.is_zero()) {
		while (not is_odd(a)) {
			a = shift_right(a, 1);
			if (low(n, 8) == 3 or low(n, 8) == 5)
				t = -t;
		}
		std::swap(a, n);
		if (low(a, 4) == 3 and low(n, 4) == 3)
			t = -t;
		a = reduce(a, n);
	}
	return n == BigInt(1) ? t : 0;
}


static std::string dec(std::uint64_t x) { return std::to_string(x); }


static void phase1() {
	const std::vector<std::uint64_t> exponents = {3, 5, 7, 13, 31, 61, 89, 127, 521, 1237, 1277, 1279, 2203, 4423, 9941, 10303,
	                                              21701, 44497};
	for (std::uint64_t p: exponents) {
		const BigInt m = mersenne(p);
		std::vector<BigInt> as = {BigInt(0), BigInt(1), BigInt(2), BigInt(3), BigInt(12), subtract(m, BigInt(1)),
		                          subtract(m, BigInt(2)), m, add(m, BigInt(5)), shift_left(BigInt(1), p - 1)};
		for (int i = 0; i < (p > 20000 ? 2 : 8); i++)
			as.push_back(reduce(random_below_bits(p), m));
		if (p > 20000)
			as.resize(std::min<std::size_t>(as.size(), 6));
		for (const BigInt& a: as)
			expect(jacobi(a, m) == own_jacobi(a, m), "jacobi(a | M_" + dec(p) + ") for a of " + dec(a.bit_length()) + " bits");
	}
	for (int i = 0; i < 60; i++) {
		BigInt n = add(shift_left(random_below_bits(1 + mix() % 1500), 1), BigInt(1));
		BigInt a = random_below_bits(1 + mix() % 1600);
		if (i % 5 == 0)
			a = multiply(a, n);                // a symbol of 0
		if (i % 7 == 0)
			n = multiply(n, n);                // a square modulus
		expect(jacobi(a, n) == own_jacobi(a, n), "jacobi on general odd moduli, case " + dec(i));
	}
	expect(jacobi(BigInt(0), BigInt(1)) == 1, "J(0 | 1) = 1");
	for (const BigInt& n: {BigInt(0), BigInt(4), mersenne(61).is_zero() ? BigInt(2) : shift_left(BigInt(1), 70)}) {
		bool threw = false;
		try { jacobi(BigInt(3), n); } catch (mp_error const&) { threw = true; } catch (...) {}
		expect(threw, "jacobi refuses the modulus of " + dec(n.bit_length()) + " bits, zero or even, with mp_error");
	}

	// The stop rule, with stand-in tests
	static std::vector<std::string> ran;
	struct Stand { static Result nv(const long unsigned p) { ran.push_back("nv"); return Result::no_verdict(WORK_TF, p, "stand-in"); }
	               static Result err(const long unsigned p) { ran.push_back("err"); return Result::error(WORK_LL, p, "stand-in"); }
	               static Result prime(const long unsigned p) { ran.push_back("prime"); return Result::prime(WORK_LL, p); } };
	struct Order { std::vector<TestFunction> tests; std::vector<std::string> want; int status; };
	const std::vector<Order> orders = {
		{{Stand::nv, Stand::err, Stand::prime}, {"nv", "err"}, 20},
		{{Stand::err, Stand::nv}, {"err"}, 20},
		{{Stand::nv, Stand::nv, Stand::err}, {"nv", "nv", "err"}, 20},
		{{Stand::nv, Stand::prime, Stand::err}, {"nv", "prime"}, 0},
	};
	for (const Order& o: orders) {
		ran.clear();
		std::vector<Result> handed;
		const Result last = run_in_order(7, o.tests, [&](const Result& r) { handed.push_back(r); });
		expect(ran == o.want, "run_in_order runs exactly the tests up to the first verdict or error");
		expect(handed.size() == o.want.size(), "run_in_order hands over each result it ran");
		expect(status_of(handed) == o.status, "the run's status after the stop rule");
		expect(verdict_line(last) == verdict_line(handed.back()), "run_in_order returns the last result handed over");
	}
}


#if GATE_PHASE >= 2

static BigInt held(const BigInt& s, std::uint64_t k, std::uint64_t p) {
	return reduce(multiply(s, power_mod(BigInt(2), BigInt(k % p), mersenne(p))), mersenne(p));
}


static BigInt unshift_own(const BigInt& r, std::uint64_t k, std::uint64_t p) {
	return reduce(multiply(r, power_mod(BigInt(2), BigInt((p - k % p) % p), mersenne(p))), mersenne(p));
}


static std::string shortest(double v) {
	char text[64];
	std::string out(text, std::to_chars(text, text + sizeof text, v).ptr);
	if (out.find_first_of(".e") == std::string::npos)
		out += ".0";
	return out;
}


static std::string fixed4(double v) {
	char text[64];
	std::snprintf(text, sizeof text, "%.4f", v);
	return text;
}


// The views PLAN § 2.5 pins, built here from the result's run
static std::string want_line(const Result& r, const std::string& middle) {
	const RunReport& run = *r.run();
	std::string lengths;
	for (auto n: run.lengths)
		lengths += (lengths.empty() ? "" : " ") + dec(n);
	return "M" + dec(r.exponent()) + " ll: " + middle + (run.lengths.size() == 1 ? ", length " : ", lengths ") + lengths +
	       ", shift " + dec(run.shift) + ", jacobi " + dec(run.jacobi_checks) + " checked " + dec(run.jacobi_failures) +
	       " failed, degenerate " + dec(run.degenerate_failures) + ", confirmation " + dec(run.confirmation_failures) +
	       ", transient " + dec(run.transient_faults) + ", retries " + dec(run.length_retries) + ", large distances " +
	       dec(run.large_distances) + ", largest distance " + fixed4(run.largest_distance) + " (mps " + APP_VERSION + ")";
}


static std::string want_record(const Result& r, const std::string& status, const std::string& detail) {
	const RunReport& run = *r.run();
	std::string lengths;
	for (auto n: run.lengths)
		lengths += (lengths.empty() ? "" : ", ") + dec(n);
	return "{\"exponent\": " + dec(r.exponent()) + ", \"worktype\": \"ll\", \"status\": \"" + status + "\", " + detail +
	       ", \"lengths\": [" + lengths + "], \"shift\": " + dec(run.shift) + ", \"jacobi_checks\": " + dec(run.jacobi_checks) +
	       ", \"jacobi_failures\": " + dec(run.jacobi_failures) + ", \"degenerate_failures\": " + dec(run.degenerate_failures) +
	       ", \"confirmation_failures\": " + dec(run.confirmation_failures) + ", \"transient_faults\": " + dec(run.transient_faults) +
	       ", \"length_retries\": " + dec(run.length_retries) + ", \"large_distances\": " + dec(run.large_distances) +
	       ", \"largest_distance\": " + shortest(run.largest_distance) + ", \"program\": {\"name\": \"mps\", \"version\": \"" +
	       APP_VERSION + "\"}, \"record_version\": 2}";
}


struct Known { std::uint64_t p; bool prime; std::string res64; };


static std::vector<Known> known_answers() {
	std::vector<Known> out;
	for (const auto& row: known::load("tests/known/mersenne-exponents.txt").rows)
		if (row.op == "mersenne" and row.exponent >= 3 and row.exponent <= 10000)
			out.push_back({row.exponent, true, "0000000000000000"});
	for (const auto& row: known::load("tests/known/final-residues.txt").rows)
		if (row.op == "ll" and row.exponent <= 20000)
			out.push_back({row.exponent, false, row.res64});
	return out;
}


static std::uint64_t checks_expected(std::uint64_t p) {
	const std::uint64_t last = p - 2, interval = (last + lucas::GOOD_STATES - 1) / lucas::GOOD_STATES;
	std::uint64_t n = 0;
	for (std::uint64_t i = 1; i <= last; i++)
		n += (i % interval == 0 or i == last);
	return n;
}


static bool answers(const Result& r, const Known& k) {
	if (k.prime)
		return r.outcome() == OUTCOME_PRIME and r.res64() and *r.res64() == 0 and status_of({r}) == 0;
	return r.outcome() == OUTCOME_COMPOSITE and r.res64() and hex64(*r.res64()) == k.res64 and status_of({r}) == 40;
}


static lucas::Options quiet(std::uint64_t shift) {
	lucas::Options o;
	o.shift = shift;
	o.progress_seconds = 1e9;
	return o;
}


// The shift of s_i in a run that starts at shift k0
static std::uint64_t held_shift(std::uint64_t k0, std::uint64_t i, std::uint64_t p) {
	std::uint64_t k = k0 % p;
	for (std::uint64_t j = 0; j < i; j++)
		k = 2 * k % p;
	return k;
}


static std::uint64_t interval_of(std::uint64_t p) { return (p - 2 + lucas::GOOD_STATES - 1) / lucas::GOOD_STATES; }


// A value whose symbol J(s - 2 | M_p) is +1, held at shift k: s = 2^(k+1)... unshifted, s - 2 = 1
static BigInt plus_one(std::uint64_t k, std::uint64_t p) { return held(BigInt(3), k, p); }


struct Capture {
	std::ostringstream buffer;
	std::streambuf* saved;
	Capture(): saved(std::cerr.rdbuf(buffer.rdbuf())) {}
	~Capture() { std::cerr.rdbuf(saved); }
};


static void phase2(const std::string& mode) {
	// The helpers against their definitions
	for (std::uint64_t p: {7ull, 31ull, 127ull, 521ull, 1279ull, 21701ull, 44497ull}) {
		const BigInt m = mersenne(p);
		for (int i = 0; i < (p > 2000 ? 3 : 12); i++) {
			const BigInt r = reduce(random_below_bits(p), m);
			const std::uint64_t k = mix() % p;
			const BigInt s = unshift_own(r, k, p);
			expect(lucas::unshifted(r, k, p) == s, "unshifted(r, k) = r 2^(p-k) mod M_" + dec(p));
			const BigInt s2 = compare(s, BigInt(2)) >= 0 ? subtract(s, BigInt(2)) : subtract(add(s, m), BigInt(2));
			expect(lucas::jacobi_of(r, k, p) == own_jacobi(s2, m), "jacobi_of(r, k) = J(s - 2 | M_" + dec(p) + ")");
		}
		const BigInt root = shift_left(BigInt(1), (p + 1) / 2);
		for (bool last: {false, true}) {
			expect(lucas::degenerate(BigInt(0), p, last) == not last, "0 is degenerate exactly before the last iteration");
			for (const BigInt& d: {BigInt(1), BigInt(2), subtract(m, BigInt(1)), subtract(m, BigInt(2))})
				expect(lucas::degenerate(d, p, last), "1, -1, 2 and -2 are degenerate");
			for (const BigInt& d: {BigInt(3), subtract(m, BigInt(3)), root, BigInt(4)})
				expect(not lucas::degenerate(d, p, last), "3, -3, 4 and 2^((p+1)/2) are not degenerate");
		}
		expect(lucas::confirms_prime(root, p) and lucas::confirms_prime(subtract(m, root), p), "+-2^((p+1)/2) confirm M_" + dec(p));
		for (const BigInt& d: {BigInt(0), add(root, BigInt(1)), shift_left(BigInt(1), (p - 1) / 2), BigInt(2)})
			expect(not lucas::confirms_prime(d, p), "only +-2^((p+1)/2) confirm M_" + dec(p));
	}

	std::uint64_t largest = 0;
	for (auto n: ibdwt::supported_lengths())
		largest = std::max(largest, ibdwt::largest_exponent(n));
	expect(lucas::largest_exponent() == largest, "P_max is the largest exponent a supported length holds");

	// The known answers at given shifts, the schedule traced
	for (const Known& k: known_answers()) {
		std::vector<std::uint64_t> shifts = {0, k.p - 1};
		if (k.p <= 1279 or mode == "full")
			shifts = {0, 1, k.p / 2, k.p - 1};
		for (std::uint64_t shift: shifts) {
			lucas::Options o = quiet(shift);
			std::uint64_t next = 1, kk = shift, squarings = 0;
			bool traced = true;
			o.refuse = [&](const lucas::Step& st) {
				squarings++;
				kk = 2 * kk % k.p;
				traced = traced and st.iteration == next and st.attempt == 0 and st.shift == kk and
				         st.length == ibdwt::length_for(k.p);
				next++;
			};
			const Result r = lucas::test(k.p, o);
			const std::string what = "M" + dec(k.p) + " at shift " + dec(shift);
			expect(answers(r, k), what + ": the known answer, its Res64 and status");
			expect(r.run() and r.run()->shift == shift, what + ": reports the shift given");
			expect(r.run() and r.run()->lengths == std::vector<std::uint64_t>{ibdwt::length_for(k.p)}, what + ": at the length the table names");
			expect(r.run() and r.run()->jacobi_checks == checks_expected(k.p) and r.run()->jacobi_failures == 0 and
			       r.run()->degenerate_failures == 0 and r.run()->confirmation_failures == 0 and r.run()->transient_faults == 0 and
			       r.run()->length_retries == 0, what + ": the Jacobi count of its schedule and no failure");
			expect(traced and squarings == k.p - 2, what + ": p - 2 squarings in order, each shift twice the last");
			if (r.run())
				expect(verdict_line(r) == want_line(r, k.prime ? "prime, res64 0000000000000000" : "composite, res64 " + k.res64) and
				       json_record(r) == want_record(r, k.prime ? "prime" : "composite", "\"res64\": \"" + k.res64 + "\""),
				       what + ": the verdict line and record of PLAN § 2.5");
		}
	}

	// An error's record, the exponent check's too, prints on stdout under -j; its line goes to stderr
	for (const Result& e: {Result::error(WORK_PTEST, 9, "the probe's error"), Result::error(WORK_LL, 31, "the probe's error", RunReport{{2}, 1})}) {
		std::vector<Result> run;
		std::ostringstream out;
		Capture cap;
		report_result(run, e, true, out);
		const int status = report_run(run, true, out);
		expect(out.str() == json_record(e) + "\n" and json_record(e).find("\"status\": \"error\", \"reason\": \"the probe's error\"") != std::string::npos,
		       "under -j an error prints its record, status error, on stdout and nothing else");
		expect(cap.buffer.str().find(verdict_line(e)) != std::string::npos and status == 20, "the error's line goes to stderr; status 20");
	}

	// The random shift
	std::set<std::uint64_t> seen;
	bool in_range = true;
	for (int i = 0; i < 6; i++) {
		lucas::Options o;
		o.progress_seconds = 1e9;
		const Result r = lucas::test(1279, o);
		if (r.run()) {
			seen.insert(r.run()->shift);
			in_range = in_range and r.run()->shift < 1279;
		}
	}
	expect(seen.size() > 1 and in_range, "six runs of M1279 choose shifts in [0, p) that are not all equal");

	// The refusals, before any squaring
	struct Refusal { std::uint64_t p; std::string reason; };
	const std::vector<Refusal> refusals = {{0, "odd"}, {1, "odd"}, {2, "odd"}, {4, "odd"}, {10, "odd"},
	                                       {9, "prime exponents"}, {33, "prime exponents"}, {3831, "prime exponents"},
	                                       {largest + 1, dec(largest)}, {1'000'000'007, dec(largest)}};
	for (const Refusal& f: refusals) {
		std::uint64_t squarings = 0;
		lucas::Options o = quiet(0);
		o.refuse = [&](const lucas::Step&) { squarings++; };
		const Result r = lucas::test(f.p, o);
		const std::string what = "M" + dec(f.p);
		expect(r.outcome() == OUTCOME_NO_VERDICT and status_of({r}) == 42, what + ": no verdict, status 42");
		expect(r.reason().find(f.reason) != std::string::npos, what + ": the reason names '" + f.reason + "'");
		expect(squarings == 0, what + ": no squaring");
	}

	// The injections. A corrupt hook at an iteration, on the attempts `on`
	auto corrupt_at = [](std::uint64_t at, std::function<bool(std::uint64_t)> on, std::function<BigInt(const lucas::Step&)> value) {
		return [=](const lucas::Step& st, const BigInt&) -> std::optional<BigInt> {
			if (st.iteration == at and on(st.attempt))
				return value(st);
			return std::nullopt;
		};
	};
	const auto once = [](std::uint64_t a) { return a == 0; };
	const auto always = [](std::uint64_t) { return true; };
	const Known c1237 {1237, false, "DF5E6622F83F5BE2"}, p1279 {1279, true, "0000000000000000"};
	for (const Known& k: {c1237, p1279}) {
		const std::uint64_t p = k.p, c = 3 * interval_of(p);
		const BigInt m = mersenne(p);
		struct Inject { std::string what; std::uint64_t at; BigInt s; std::uint64_t jac, deg, conf; };
		std::vector<Inject> injections = {
			{"a symbol of +1 at the last iteration", p - 2, BigInt(3), 1, 0, 0},
			{"a symbol of +1 at a checked iteration", c, BigInt(3), 1, 0, 0},
			{"1 at a checked iteration", c, BigInt(1), 0, 1, 0},
			{"-1 at a checked iteration", c, subtract(m, BigInt(1)), 0, 1, 0},
			{"2 at a checked iteration", c, BigInt(2), 0, 1, 0},
			{"-2 at a checked iteration", c, subtract(m, BigInt(2)), 0, 1, 0},
			{"0 at a checked iteration", c, BigInt(0), 0, 1, 0},
			{"0 between checked iterations", c - 1, BigInt(0), 0, 1, 0},
		};
		if (not k.prime)
			injections.push_back({"0 at the last iteration", p - 2, BigInt(0), 0, 0, 1});
		for (const Inject& in: injections) {
			lucas::Options o = quiet(17);
			const BigInt s = in.s;
			o.corrupt = corrupt_at(in.at, once, [s, p](const lucas::Step& st) { return held(s, st.shift, p); });
			std::vector<lucas::Step> steps;
			o.refuse = [&steps](const lucas::Step& st) { steps.push_back(st); };
			const Result r = lucas::test(p, o);
			const std::string what = "M" + dec(p) + ", " + in.what + ", once";
			expect(answers(r, k), what + ": recovers to the known answer");
			// The failure is seen at the first checked iteration at or after the injection; the run
			// goes back to the good state before it and redoes the next iteration, its second run
			const std::uint64_t iv = interval_of(p);
			const std::uint64_t seen = std::min(p - 2, (in.at + iv - 1) / iv * iv);
			const std::uint64_t good = seen == p - 2 ? (p - 3) / iv * iv : seen - iv;
			std::size_t at = 0;
			while (at < steps.size() and not (steps[at].iteration == seen and steps[at].attempt == 0))
				at++;
			expect(at + 1 < steps.size() and steps[at + 1].iteration == good + 1 and steps[at + 1].attempt == 1 and
			       steps[at + 1].shift == 2 * held_shift(17, good, p) % p,
			       what + ": goes back to the last good state, iteration " + dec(good) + ", and redoes the next");
			expect(r.run() and r.run()->jacobi_failures == in.jac and r.run()->degenerate_failures == in.deg and
			       r.run()->confirmation_failures == in.conf, what + ": counts the one failure as its check's");
		}
		// The limit
		for (bool every: {false, true}) {
			lucas::Options o = quiet(17);
			o.corrupt = corrupt_at(c, [every](std::uint64_t a) { return every or a < lucas::CHECK_FAILURES; },
			                       [p](const lucas::Step& st) { return plus_one(st.shift, p); });
			const Result r = lucas::test(p, o);
			const std::string what = "M" + dec(p) + ", a symbol of +1 " + (every ? "on every attempt" : "on CHECK_FAILURES attempts");
			if (every) {
				expect(r.outcome() == OUTCOME_ERROR and status_of({r}) == 20 and not r.res64(), what + ": an error, status 20, no Res64");
				expect(r.reason().find("Jacobi") != std::string::npos, what + ": the error names the Jacobi check");
				expect(r.run() and r.run()->jacobi_failures == lucas::CHECK_FAILURES + 1, what + ": counts CHECK_FAILURES + 1 failures");
				if (r.run())
					expect(json_record(r) == want_record(r, "error", "\"reason\": \"" + r.reason() + "\"") and
					       verdict_line(r) == want_line(r, "error, " + r.reason()), what + ": the error's line and record of PLAN § 2.5");
			} else {
				expect(answers(r, k) and r.run() and r.run()->jacobi_failures == lucas::CHECK_FAILURES, what + ": recovers, every failure counted");
			}
		}
		// Refusals
		const std::uint64_t first = ibdwt::length_for(p);
		std::uint64_t next = 0;
		for (auto n: ibdwt::supported_lengths())
			if (n > first and ibdwt::holds(n, p)) { next = n; break; }
		auto refuse_at = [p](std::uint64_t at, std::function<bool(const lucas::Step&)> when) {
			return [=](const lucas::Step& st) {
				if (st.iteration == at and when(st))
					throw roundoff_error("refused by the probe", p, st.length, 0.47, 0.4375);
			};
		};
		{
			lucas::Options o = quiet(5);
			std::vector<lucas::Step> steps;
			auto inner = refuse_at(100, [](const lucas::Step& st) { return st.attempt == 0; });
			o.refuse = [&steps, inner](const lucas::Step& st) { steps.push_back(st); inner(st); };
			const Result r = lucas::test(p, o);
			expect(answers(r, k) and r.run() and r.run()->transient_faults == 1 and r.run()->length_retries == 0 and
			       r.run()->lengths.size() == 1, "M" + dec(p) + ": a refusal once is replayed and counted transient");
			expect(steps.size() >= 101 and steps[99].iteration == 100 and steps[99].attempt == 0 and steps[100].iteration == 100 and
			       steps[100].attempt == 1 and steps[100].length == first and steps[100].shift == steps[99].shift,
			       "M" + dec(p) + ": the replay is the same squaring, at the same length");
		}
		{
			lucas::Options o = quiet(5);
			std::vector<lucas::Step> steps;
			auto inner = refuse_at(100, [first](const lucas::Step& st) { return st.length == first; });
			o.refuse = [&steps, inner](const lucas::Step& st) { steps.push_back(st); inner(st); };
			const Result r = lucas::test(p, o);
			expect(answers(r, k) and r.run() and r.run()->length_retries == 1 and
			       r.run()->lengths == std::vector<std::uint64_t>{first, next}, "M" + dec(p) + ": a refusal that reproduces at its length moves to the next that holds p");
			const std::uint64_t good = 99 / interval_of(p) * interval_of(p);
			std::size_t at = 0;
			while (at < steps.size() and steps[at].length == first)
				at++;
			expect(at < steps.size() and steps[at].iteration == good + 1 and steps[at].length == next and
			       steps[at].shift == 2 * held_shift(5, good, p) % p,
			       "M" + dec(p) + ": the move goes back to the last good state, iteration " + dec(good));
			if (r.run())
				expect(verdict_line(r) == want_line(r, k.prime ? "prime, res64 0000000000000000" : "composite, res64 " + k.res64) and
				       json_record(r) == want_record(r, k.prime ? "prime" : "composite", "\"res64\": \"" + k.res64 + "\""),
				       "M" + dec(p) + ": the views name both lengths");
		}
		{
			lucas::Options o = quiet(5);
			o.refuse = refuse_at(100, [](const lucas::Step&) { return true; });
			const Result r = lucas::test(p, o);
			const std::string what = "M" + dec(p) + ": a refusal at every length";
			expect(r.outcome() == OUTCOME_ERROR and status_of({r}) == 20, what + ": an error, status 20");
			expect(r.reason().find(dec(p)) != std::string::npos and r.reason().find("0.47") != std::string::npos and r.run() and
			       r.reason().find(dec(r.run()->lengths.back())) != std::string::npos, what + ": names p, the length and the distance");
			expect(r.run() and r.run()->length_retries == lucas::LENGTH_RETRIES and r.run()->lengths.size() == lucas::LENGTH_RETRIES + 1,
			       what + ": stops at LENGTH_RETRIES moves, every length named");
			if (r.run())
				expect(verdict_line(r) == want_line(r, "error, " + r.reason()) and
				       json_record(r) == want_record(r, "error", "\"reason\": \"" + r.reason() + "\""), what + ": the error's views name every length");
		}
		// The distances the engine reports are the run's
		{
			lucas::Options o = quiet(5);
			double largest = 0;
			o.distance = [&largest](const lucas::Step&, double d) { largest = std::max(largest, d); return d; };
			const Result r = lucas::test(p, o);
			expect(largest > 0 and r.run() and r.run()->largest_distance == largest and r.run()->large_distances == 0,
			       "M" + dec(p) + ": the largest distance reported is the largest the engine gave, and above zero");
		}
		// Large distances
		{
			Capture cap;
			lucas::Options o = quiet(5);
			std::set<std::uint64_t> at;
			for (std::uint64_t i = 1; i <= 12; i++)
				at.insert(40 * i);
			o.distance = [at](const lucas::Step& st, double d) { return at.count(st.iteration) ? 0.36 : d; };
			const Result r = lucas::test(p, o);
			expect(answers(r, k) and r.run() and r.run()->large_distances == 12 and r.run()->largest_distance == 0.36,
			       "M" + dec(p) + ": twelve distances of 0.36 counted, the largest reported");
			const std::string log = cap.buffer.str();
			std::size_t logged = 0;
			for (std::uint64_t i: at) {
				const std::string line = "ll M" + dec(p) + ": iteration " + dec(i) + " at length " + dec(first) +
				                         " has round-off distance 0.3600, past 0.35";
				logged += log.find(line) != std::string::npos;
			}
			expect(logged == lucas::DISTANCES_LOGGED, "M" + dec(p) + ": DISTANCES_LOGGED of them logged one by one with the iteration, the length and the distance");
			expect(log.find("ll M" + dec(p) + ": " + dec(12 - lucas::DISTANCES_LOGGED) + " more squarings past the sizing distance, counted and not logged") != std::string::npos,
			       "M" + dec(p) + ": the rest folded into one line");
		}
	}
	// Starting at the largest length that holds 31, a refusal has nowhere to go
	{
		std::uint64_t top = 0;
		for (auto n: ibdwt::supported_lengths())
			if (ibdwt::holds(n, 31))
				top = n;
		lucas::Options o = quiet(3);
		o.length = top;
		o.refuse = [](const lucas::Step& st) { if (st.iteration == 5) throw roundoff_error("refused by the probe", 31, st.length, 0.47, 0.4375); };
		const Result r = lucas::test(31, o);
		expect(r.outcome() == OUTCOME_ERROR and r.run() and r.run()->length_retries == 0 and r.reason().find(dec(top)) != std::string::npos,
		       "M31 at its largest length: a refusal ends in an error naming that length");
	}
	// Progress
	{
		Capture cap;
		lucas::Options o = quiet(0);
		o.progress_seconds = 0;
		lucas::test(127, o);
		std::istringstream in(cap.buffer.str());
		std::string line;
		const std::regex pattern(R"(.*ll M127: iteration (\d+) of 125, [0-9.]+ ms per iteration, about \d+h\d\dm\d\ds to go, largest distance [0-9.]+ since the last line)");
		std::uint64_t n = 0;
		bool ordered = true;
		while (std::getline(in, line)) {
			std::smatch m;
			if (std::regex_match(line, m, pattern)) {
				n++;
				ordered = ordered and std::stoull(m[1]) == n;
			}
		}
		expect(n == 125 and ordered, "M127 with no interval logs one progress line per iteration, in order, in PLAN § 2.5's form");
	}
}

#endif


// The test's log is not the probe's output: it goes nowhere unless a check captures it
struct Discard: std::streambuf { int overflow(int c) override { return c; } };


int main(int argc, char** argv) {
	static Discard discard;
	std::cerr.rdbuf(&discard);
	const int phase = argc > 1 ? std::atoi(argv[1]) : GATE_PHASE;
	const std::string mode = argc > 2 ? argv[2] : "full";
	phase1();
#if GATE_PHASE >= 2
	if (phase >= 2)
		phase2(mode);
#endif
	if (failures) {
		std::cout << "probe: failed " << failures << " of " << cases << std::endl;
		return 1;
	}
	std::cout << "probe: ok " << cases << " cases" << std::endl;
	return 0;
}
