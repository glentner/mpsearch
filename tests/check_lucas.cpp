// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: Apache-2.0
//
// Checks: the Lucas-Lehmer test
//
// The test through its own interface, lucas::test with its options, never through bin/mps
// (tests/ll.sh runs that): the known answers of tests/known/ at two given shifts, the fresh shift
// of a default run, the refusals, the views of a run, and every error check and recovery proved by
// the hooks, which corrupt a residue, refuse a squaring or report a round-off distance at a chosen
// iteration and attempt, and every value of the progress line. Every injected value is computed
// here on BigInt, and every expected answer is read from tests/known/, but M83's, which has no row
// there and is the run without the injection. The hook checks run at small exponents (5, 83, 127,
// 1237, 1279), where a run takes milliseconds, and M86243 for one squaring. The progress line's
// times come from the system clock: its check makes them long with sleeps in a hook and bounds
// them.
//


// Standard libs
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Internal libs
#include "check.hpp"
#include "engine_support.hpp"
#include "ibdwt.hpp"
#include "known.hpp"
#include "lengths.hpp"
#include "log.hpp"
#include "log_peer.hpp"
#include "lucas.hpp"
#include "main.hpp"
#include "math.hpp"
#include "mp.hpp"
#include "result.hpp"


using engine_support::Tally;


// A progress interval no run here reaches, so that only the progress check sees progress lines
static const double QUIET = 1e9;

// The Res64 of a prime's run
static const std::string ZERO = "0000000000000000";


// An exponent with a known answer: its Res64, ZERO for a prime
struct Known {
	std::uint64_t p;
	std::string res64;
	bool prime;
};


static std::vector<Known> known_answers() {
	//
	// The odd Mersenne prime exponents up to 10,000 and the ll rows up to 20,000
	//
	// Read from tests/known/ through the loader, superseded rows left out: the bounds tests/ll.sh
	// runs bin/mps to.

	std::vector<Known> answers;
	for (const known::Row& row: known::load("tests/known/mersenne-exponents.txt").rows)
		if (not row.superseded and row.op == "mersenne" and row.exponent % 2 == 1 and row.exponent <= 10'000)
			answers.push_back({row.exponent, ZERO, true});
	for (const known::Row& row: known::load("tests/known/final-residues.txt").rows)
		if (not row.superseded and row.op == "ll" and row.exponent <= 20'000)
			answers.push_back({row.exponent, row.res64, false});
	return answers;
}


static Known known_answer(std::uint64_t p) {
	//
	// The known answer of one exponent the hook checks run at

	for (const Known& known: known_answers())
		if (known.p == p)
			return known;
	check::require(false, "a known answer for " + std::to_string(p));
	return {};
}


static lucas::Options at_shift(std::uint64_t shift) {
	lucas::Options options;
	options.shift = shift;
	options.progress_seconds = QUIET;
	return options;
}


static BigInt held(const BigInt& s, std::uint64_t shift, std::uint64_t p) {
	//
	// The residue the test holds for s at a shift: 2^shift s mod M_p

	return reduce_mersenne(shift_left(s, shift), p);
}


static std::string res64_text(const Result& result) {
	return result.res64() ? hex64(*result.res64()) : "none";
}


static bool has(const std::string& text, const std::string& part) {
	return text.find(part) != std::string::npos;
}


static std::size_t count_of(const std::string& text, const std::string& part) {
	std::size_t count = 0;
	for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size()))
		count++;
	return count;
}


static std::string distance_text(double distance) {
	//
	// A distance as the log prints it, to four places

	char text[32];
	std::snprintf(text, sizeof text, "%.4f", distance);
	return text;
}


static std::uint64_t interval_of(std::uint64_t p) {
	//
	// The iterations from one checked good state to the next: the p - 2 iterations in GOOD_STATES
	// parts

	return (p - 2 + lucas::GOOD_STATES - 1) / lucas::GOOD_STATES;
}


static std::uint64_t checks_of(std::uint64_t p) {
	//
	// How many iterations a run without a failure checks: every multiple of the interval, and the
	// last

	std::uint64_t count = 0;
	for (std::uint64_t i = 1; i <= p - 2; i++)
		if (i % interval_of(p) == 0 or i == p - 2)
			count++;
	return count;
}


static std::uint64_t shift_at(std::uint64_t shift, std::uint64_t iteration, std::uint64_t p) {
	//
	// The shift of s_iteration in a run that holds s_0 at `shift`: doubled at each squaring

	for (std::uint64_t i = 0; i < iteration; i++)
		shift = 2 * shift % p;
	return shift;
}


static std::uint64_t next_holding(std::uint64_t length, std::uint64_t p) {
	//
	// The supported length after `length` that holds p, or 0 when none does

	for (const std::uint64_t candidate: ibdwt::supported_lengths())
		if (candidate > length and ibdwt::holds(candidate, p))
			return candidate;
	return 0;
}


static BigInt failing_jacobi(std::uint64_t p) {
	//
	// The least s >= 3 with J(s - 2 | M_p) = +1 that is no degenerate state
	//
	// Held at a checked iteration, it fails the Jacobi check and nothing before it.

	const BigInt m = mersenne(p);
	for (std::uint64_t s = 3;; s++)
		if (jacobi(BigInt(s - 2), m) == 1 and not lucas::degenerate(BigInt(s), p, false))
			return BigInt(s);
}


static BigInt failing_jacobi_after_a_squaring(std::uint64_t p) {
	//
	// The least w >= 3 whose square less 2 fails the Jacobi check: J(w^2 - 4 | M_p) = +1, with
	// w^2 - 2 no degenerate state
	//
	// Held at the iteration before a checked one, it is caught one squaring later.

	const BigInt m = mersenne(p);
	for (std::uint64_t w = 3;; w++)
		if (jacobi(BigInt(w * w - 4), m) == 1 and not lucas::degenerate(BigInt(w * w - 2), p, false))
			return BigInt(w);
}


static std::uint64_t least_factor_of_mersenne(std::uint64_t p) {
	//
	// The least prime factor of M_p for a prime p whose M_p is composite
	//
	// Every factor of M_p is 2kp + 1 (Euler), and q divides M_p when 2^p = 1 modulo q, so the
	// first such q is the least divisor above 1, which is prime.

	for (std::uint64_t q = 2 * p + 1;; q += 2 * p) {
		std::uint64_t power = 1;
		for (std::uint64_t i = 0; i < p; i++)
			power = 2 * power % q;
		if (power == 1)
			return q;
	}
}


static lucas::Options holding(std::uint64_t p, std::uint64_t shift, std::uint64_t iteration, const BigInt& s,
                              std::uint64_t attempts = 1) {
	//
	// Options that hold s, shifted as the test holds it, in place of the residue made at the
	// iteration, on its first `attempts` attempts

	lucas::Options options = at_shift(shift);
	options.corrupt = [p, iteration, s, attempts](const lucas::Step& step, const BigInt&) -> std::optional<BigInt> {
		if (step.iteration != iteration or step.attempt >= attempts)
			return std::nullopt;
		return held(s, step.shift, p);
	};
	return options;
}


static roundoff_error refusal(std::uint64_t p, const lucas::Step& step) {
	return roundoff_error("an injected refusal", p, step.length, 0.47, ibdwt::THRESHOLD);
}


static void the_test_reports_the_known_answers_at_two_given_shifts() {
	//
	// Every odd Mersenne prime exponent up to 10,000 and every ll row up to 20,000, at shift 0
	// and at shift p/3 + 1
	//
	// A prime with Res64 0, or a composite with the row's Res64; the shift reported as given;
	// status 0 or 40; every error check passed, the prime confirmation included; and the two runs
	// of an exponent agree. A shift given at p or past it is reduced modulo p.

	log_peer::CaptureCerr quiet;
	Tally answers("known answers at shifts 0 and p/3 + 1");
	Tally pairs("exponents whose two shifts agree");
	for (const Known& known: known_answers()) {
		std::vector<Result> results;
		for (const std::uint64_t shift: {std::uint64_t(0), known.p / 3 + 1}) {
			const Result result = lucas::test(known.p, at_shift(shift));
			const std::optional<RunReport>& run = result.run();
			const bool right = result.outcome() == (known.prime ? OUTCOME_PRIME : OUTCOME_COMPOSITE) and
			                   res64_text(result) == known.res64 and run and run->shift == shift and
			                   status_of({result}) == (known.prime ? 0 : 40) and run->jacobi_checks == checks_of(known.p) and
			                   run->jacobi_failures == 0 and run->degenerate_failures == 0 and
			                   run->confirmation_failures == 0 and run->transient_faults == 0 and run->length_retries == 0;
			answers.add(right, "M" + std::to_string(known.p) + " at shift " + std::to_string(shift) + ": " + verdict_line(result));
			results.push_back(result);
		}
		pairs.add(results[0].outcome() == results[1].outcome() and res64_text(results[0]) == res64_text(results[1]),
		          "M" + std::to_string(known.p) + ": " + res64_text(results[0]) + " and " + res64_text(results[1]));
	}
	answers.expect_none();
	pairs.expect_none();

	// A shift given at p or past it is taken modulo p, and reported so
	const Result wrapped = lucas::test(1279, at_shift(1279 + 21));
	check::require(wrapped.run().has_value(), "M1279 at shift 1300 reports its run");
	check::expect(wrapped.outcome() == OUTCOME_PRIME, "M1279 at shift 1300: prime");
	check::expect_equal(wrapped.run()->shift, std::uint64_t(21), "M1279 at shift 1300: the shift reported is 21");
}


static void the_test_chooses_a_fresh_shift_each_run() {
	//
	// Four default runs of M1279, through lucas::test and through test_lucas_lehmer, report shifts
	// in [0, p) that are not all equal
	//
	// Four equal shifts from a uniform choice have probability 1279^-3, about 5 in 10^10. A larger
	// exponent would make that smaller still and the check slower.

	static const std::uint64_t p = 1279;
	log_peer::CaptureCerr quiet;
	lucas::Options defaults;
	defaults.progress_seconds = QUIET;
	for (const bool driver: {true, false}) {
		const std::string label = driver ? "lucas::test" : "test_lucas_lehmer";
		std::set<std::uint64_t> shifts;
		bool in_range = true;
		bool prime = true;
		for (int run = 0; run < 4; run++) {
			const Result result = driver ? lucas::test(p, defaults) : test_lucas_lehmer(p);
			check::require(result.run().has_value(), label + " reports its run");
			shifts.insert(result.run()->shift);
			in_range = in_range and result.run()->shift < p;
			prime = prime and result.outcome() == OUTCOME_PRIME;
		}
		check::expect(in_range, label + ": every shift in [0, p)");
		check::expect(shifts.size() > 1, label + ": four runs report more than one shift");
		check::expect(prime, label + ": every run reports M1279 prime");
	}

	// The whole range and nothing past it: 200 default runs of M5 report each shift from 0 to 4,
	// and no other. A shift of the five missing from all of them has probability 5 (4/5)^200,
	// about 2 in 10^19.
	static const std::uint64_t q = 5;
	std::set<std::uint64_t> small;
	for (int run = 0; run < 200; run++) {
		const Result result = lucas::test(q, defaults);
		check::require(result.run().has_value(), "M5 reports its run");
		small.insert(result.run()->shift);
	}
	check::expect(small == std::set<std::uint64_t> {0, 1, 2, 3, 4}, "200 runs of M5 report every shift in [0, 5) and no other");
}


static void the_test_checks_the_jacobi_symbol_at_intervals_and_on_the_final_residue() {
	//
	// A run checks every interval's end and the last iteration, and a value whose symbol is +1 is
	// caught where it is checked
	//
	// The counts: a run without a failure counts a check at every multiple of the interval and at
	// the last iteration. The catches, each injected once into M1237: at the last iteration; at a
	// checked iteration mid-run; and at the iteration before a checked one, a value whose square
	// less 2 has symbol +1, caught one squaring later. Each run recovers to the row's Res64 with
	// one Jacobi failure, and the log names the iteration that failed. A symbol of 0, the value
	// sharing a factor with M_p, fails as +1 does.

	log_peer::CaptureCerr quiet;
	for (const std::uint64_t p: {3, 5, 7, 13, 31, 127, 521, 1237, 1279}) {
		const Result result = lucas::test(p, at_shift(0));
		check::require(result.run().has_value(), "M" + std::to_string(p) + " reports its run");
		check::expect_equal(result.run()->jacobi_checks, checks_of(p), "M" + std::to_string(p) + "'s Jacobi checks");
		check::expect_equal(result.run()->jacobi_failures, std::uint64_t(0), "M" + std::to_string(p) + "'s Jacobi failures");
	}

	static const std::uint64_t p = 1237;
	const Known known = known_answer(p);
	const std::uint64_t interval = interval_of(p);
	const BigInt s = failing_jacobi(p);
	const BigInt w = failing_jacobi_after_a_squaring(p);
	const BigInt m = mersenne(p);
	check::expect_equal(jacobi(subtract(s, BigInt(2)), m), 1, "the injected value's symbol is +1");
	check::expect(not lucas::degenerate(s, p, true) and not s.is_zero(), "the injected value is nonzero and no degenerate state");
	check::expect(lucas::unshifted(held(s, 77, p), 77, p) == s, "the injected value unshifts to itself");

	struct Catch {
		std::string label;
		std::uint64_t iteration;   // where the value is held
		BigInt value;
		std::uint64_t checked;     // where it is caught
	};
	const std::vector<Catch> catches = {
		{"at the last iteration",                 p - 2,            s, p - 2},
		{"at a checked iteration mid-run",        3 * interval,     s, 3 * interval},
		{"the iteration before a checked one",    5 * interval - 1, w, 5 * interval},
	};
	for (const Catch& c: catches) {
		log_peer::CaptureCerr log;
		const Result result = lucas::test(p, holding(p, 400, c.iteration, c.value));
		const std::string text = log.text();
		check::require(result.run().has_value(), c.label + ": the run is reported");
		check::expect_equal(res64_text(result), known.res64, c.label + ": the row's Res64");
		check::expect(result.outcome() == OUTCOME_COMPOSITE, c.label + ": composite");
		check::expect_equal(result.run()->jacobi_failures, std::uint64_t(1), c.label + ": one Jacobi failure");
		check::expect_equal(result.run()->degenerate_failures, std::uint64_t(0), c.label + ": no degenerate state");
		check::expect_equal(result.run()->jacobi_checks, checks_of(p) + 1, c.label + ": the failed check counted with the rest");
		check::expect(has(text, "the Jacobi check (symbol +1) failed at iteration " + std::to_string(c.checked) + ";"),
		              c.label + ": the log names iteration " + std::to_string(c.checked));
	}

	// A symbol of 0 fails the check as +1 does. M83 is composite, and s = q + 2 for its least
	// factor q gives s - 2 = q, which shares q with M83: held at a checked iteration mid-run and
	// at the last, each run recovers to the Res64 of the run without it; held on every attempt,
	// the run ends in an error naming the symbol.
	static const std::uint64_t z = 83;
	const std::uint64_t q = least_factor_of_mersenne(z);
	const BigInt zero = BigInt(q + 2);
	check::expect_equal(q, std::uint64_t(167), "the least factor of M83");
	check::expect_equal(jacobi(BigInt(q), mersenne(z)), 0, "the value held in M83 has symbol 0");
	check::expect(not lucas::degenerate(zero, z, false) and not lucas::degenerate(zero, z, true),
	              "the value held in M83 is no degenerate state");
	const Result clean = lucas::test(z, at_shift(40));
	check::require(clean.run().has_value(), "M83 without an injection reports its run");
	check::expect(clean.outcome() == OUTCOME_COMPOSITE, "M83 without an injection: composite");
	check::expect_equal(clean.run()->jacobi_failures, std::uint64_t(0), "M83 without an injection: no Jacobi failure");
	for (const std::uint64_t at: {5 * interval_of(z), z - 2}) {
		log_peer::CaptureCerr log;
		const std::string label = "M83 holding a symbol of 0 at iteration " + std::to_string(at);
		const Result result = lucas::test(z, holding(z, 40, at, zero));
		const std::string text = log.text();
		check::require(result.run().has_value(), label + ": the run is reported");
		check::expect(result.outcome() == OUTCOME_COMPOSITE, label + ": composite");
		check::expect_equal(res64_text(result), res64_text(clean), label + ": the Res64 of the run without it");
		check::expect_equal(result.run()->jacobi_failures, std::uint64_t(1), label + ": one Jacobi failure");
		check::expect_equal(result.run()->degenerate_failures, std::uint64_t(0), label + ": no degenerate state");
		check::expect_equal(result.run()->jacobi_checks, checks_of(z) + 1, label + ": the failed check counted with the rest");
		check::expect(has(text, "the Jacobi check (symbol 0) failed at iteration " + std::to_string(at) + ";"),
		              label + ": the log names the symbol and the iteration");
	}
	const Result held_always = lucas::test(z, holding(z, 40, 5 * interval_of(z), zero, lucas::CHECK_FAILURES + 1));
	check::require(held_always.run().has_value(), "M83 holding a symbol of 0 on every attempt: the run is reported");
	check::expect(held_always.outcome() == OUTCOME_ERROR, "M83 holding a symbol of 0 on every attempt: an error");
	check::expect_equal(status_of({held_always}), 20, "M83 holding a symbol of 0 on every attempt: status 20");
	check::expect(has(held_always.reason(), "the Jacobi check (symbol 0)"),
	              "M83 holding a symbol of 0 on every attempt: the reason names the symbol");
	check::expect_equal(held_always.run()->jacobi_failures, lucas::CHECK_FAILURES + 1,
	                    "M83 holding a symbol of 0 on every attempt: every failure counted");
}


static void the_test_catches_degenerate_states() {
	//
	// 1, -1, 2, -2 and 0 held at a checked iteration, and 0 held mid-way to the next, are each
	// caught as degenerate and recovered from
	//
	// In M1279, prime, and M1237, composite. The degenerate check runs before the Jacobi check, so
	// -1 and 2, which the Jacobi check would catch too, count as degenerate, and 0, 1 and -2, which
	// it would pass, are caught. A 0 held mid-way squares to -2 and then 2, which the next check
	// catches.

	log_peer::CaptureCerr quiet;
	for (const std::uint64_t p: {1279, 1237}) {
		const Known known = known_answer(p);
		const BigInt m = mersenne(p);
		const std::uint64_t checked = 2 * interval_of(p);
		const std::uint64_t mid_way = checked + interval_of(p) / 2;
		struct Held {
			std::string label;
			std::uint64_t iteration;
			BigInt value;
		};
		const std::vector<Held> states = {
			{"1",            checked, BigInt(1)},
			{"-1",           checked, subtract(m, BigInt(1))},
			{"2",            checked, BigInt(2)},
			{"-2",           checked, subtract(m, BigInt(2))},
			{"0",            checked, BigInt(0)},
			{"0 mid-way",    mid_way, BigInt(0)},
		};
		for (const Held& state: states) {
			const std::string label = "M" + std::to_string(p) + " holding " + state.label;
			const Result result = lucas::test(p, holding(p, p / 2, state.iteration, state.value));
			check::require(result.run().has_value(), label + ": the run is reported");
			check::expect_equal(res64_text(result), known.res64, label + ": the known Res64");
			check::expect_equal(result.run()->degenerate_failures, std::uint64_t(1), label + ": one degenerate state");
			check::expect_equal(result.run()->jacobi_failures, std::uint64_t(0), label + ": no Jacobi failure");
			check::expect_equal(result.run()->jacobi_checks, checks_of(p), label + ": no Jacobi check of the degenerate state");
		}
	}
}


static void the_test_confirms_a_zero_residue_before_it_reports_prime() {
	//
	// A zero held at the last iteration of M1237, which passes the degenerate and Jacobi checks,
	// fails the prime confirmation and is recovered from
	//
	// And the runs of a prime confirm their zero: M1279 at two shifts counts no failure.

	log_peer::CaptureCerr quiet;
	static const std::uint64_t p = 1237;
	const Result result = lucas::test(p, holding(p, 9, p - 2, BigInt(0)));
	check::require(result.run().has_value(), "the run is reported");
	check::expect(result.outcome() == OUTCOME_COMPOSITE, "M1237 with a zero held at the end: composite");
	check::expect_equal(res64_text(result), known_answer(p).res64, "M1237 with a zero held at the end: the row's Res64");
	check::expect_equal(result.run()->confirmation_failures, std::uint64_t(1), "one confirmation failure");
	check::expect_equal(result.run()->degenerate_failures, std::uint64_t(0), "no degenerate state");
	check::expect_equal(result.run()->jacobi_failures, std::uint64_t(0), "no Jacobi failure");

	for (const std::uint64_t shift: {0, 640}) {
		const Result prime = lucas::test(1279, at_shift(shift));
		check::require(prime.run().has_value(), "M1279's run is reported");
		check::expect(prime.outcome() == OUTCOME_PRIME, "M1279 at shift " + std::to_string(shift) + ": prime");
		check::expect_equal(prime.run()->confirmation_failures, std::uint64_t(0),
		                    "M1279 at shift " + std::to_string(shift) + ": no confirmation failure");
	}
}


static void the_test_refuses_exponents_it_cannot_run() {
	//
	// Every exponent the test cannot run is refused, naming every reason that applies to it and
	// no other: not odd or below 3, past P_max, odd and composite
	//
	// The exponents cover each reason alone and every combination that can occur: every one up
	// to 64 but the odd primes, P_max and its neighbours, the 64 above P_max, and large ones of
	// each kind, an even one past P_max among them. Each is a no verdict, status 42, with no run,
	// before any squaring: a refuse hook, which runs before every squaring, is never called.
	// P_max is the largest exponent a supported length holds.

	std::uint64_t largest = 0;
	for (const std::uint64_t length: ibdwt::supported_lengths())
		largest = std::max(largest, ibdwt::largest_exponent(length));
	check::expect_equal(lucas::largest_exponent(), largest, "P_max is the largest exponent a length holds");

	struct Reason {
		std::string name;
		std::string words;
		bool (*applies)(std::uint64_t p, std::uint64_t largest);
	};
	const std::vector<Reason> reasons = {
		{"not odd or below 3", "the Lucas-Lehmer test applies to odd exponents of 3 or more",
		 [](std::uint64_t p, std::uint64_t) { return p < 3 or p % 2 == 0; }},
		{"past P_max", "the largest the length table holds (P_max)",
		 [](std::uint64_t p, std::uint64_t largest) { return p > largest; }},
		{"composite", "the Lucas-Lehmer test runs on prime exponents",
		 [](std::uint64_t p, std::uint64_t) { return p >= 3 and p % 2 == 1 and smallest_prime_factor(p) != p; }},
	};

	std::vector<std::uint64_t> exponents;
	for (std::uint64_t p = 0; p <= 64; p++)
		exponents.push_back(p);
	for (std::uint64_t p = largest - 3; p <= largest + 64; p++)
		exponents.push_back(p);
	for (const std::uint64_t p: {std::uint64_t(1277 * 3), std::uint64_t(999'999'937), std::uint64_t(999'999'998),
	                             std::uint64_t(999'999'999), std::uint64_t(1'000'000'007), std::uint64_t(1) << 40,
	                             (std::uint64_t(1) << 40) + 1})
		exponents.push_back(p);

	log_peer::CaptureCerr quiet;
	std::set<std::string> combinations;
	for (const std::uint64_t p: exponents) {
		std::string applying;
		for (const Reason& reason: reasons)
			if (reason.applies(p, largest))
				applying += (applying.empty() ? "" : " and ") + reason.name;
		if (applying.empty())
			continue;   // an odd prime the table holds, which the test runs
		combinations.insert(applying);

		std::uint64_t squarings = 0;
		lucas::Options options = at_shift(0);
		options.refuse = [&squarings](const lucas::Step&) { squarings++; };
		const Result result = lucas::test(p, options);
		const std::string label = "M" + std::to_string(p) + " (" + applying + ")";
		check::expect(result.outcome() == OUTCOME_NO_VERDICT, label + ": no verdict");
		check::expect_equal(status_of({result}), 42, label + ": status 42");
		for (const Reason& reason: reasons)
			check::expect(has(result.reason(), reason.words) == reason.applies(p, largest),
			              label + ": the reason " + (reason.applies(p, largest) ? "names" : "does not name") + " '" +
			              reason.name + "': " + result.reason());
		if (reasons[1].applies(p, largest))
			check::expect(has(result.reason(), "past " + std::to_string(largest) + ","), label + ": the reason names P_max's value");
		check::expect(not result.run(), label + ": no run to report");
		check::expect_equal(squarings, std::uint64_t(0), label + ": no squaring");
	}
	for (const char* combination: {"not odd or below 3", "past P_max", "composite", "not odd or below 3 and past P_max",
	                               "past P_max and composite"})
		check::expect(combinations.count(combination) == 1, std::string("the exponents refused include one ") + combination);
}


static void the_result_of_the_test_names_its_run_in_both_views() {
	//
	// A run of M1237 that counts every kind of failure, and a run that ends in an error, each
	// in its verdict line and its JSON record, exactly
	//
	// The first run, at shift 1000: refusals replayed at iterations 50 and 60, a refusal that
	// reproduces at iteration 100 and moves the run to the next length, a value with symbol +1 at
	// the second checked iteration, a 1 at the fourth and a 0 at the last, and four distances of
	// 0.36. Its Jacobi checks: those of a clean run, the failed one, and the last one whose zero
	// fails only the confirmation; the degenerate state is not Jacobi-checked. The second, M127
	// at shift 5 with every distance reported as 0.125, holds a value with symbol +1 at its
	// second checked iteration on every attempt: one check passes, and CHECK_FAILURES + 1 fail.

	static const std::uint64_t p = 1237;
	const std::uint64_t interval = interval_of(p);
	const std::uint64_t first = ibdwt::length_for(p);
	const std::uint64_t second = next_holding(first, p);
	const BigInt s = failing_jacobi(p);

	lucas::Options options = at_shift(1000);
	options.refuse = [first](const lucas::Step& step) {
		if ((step.iteration == 50 or step.iteration == 60) and step.attempt == 0)
			throw refusal(p, step);
		if (step.iteration == 100 and step.length == first)
			throw refusal(p, step);
	};
	options.corrupt = [interval, s](const lucas::Step& step, const BigInt&) -> std::optional<BigInt> {
		if (step.attempt != 0)
			return std::nullopt;
		if (step.iteration == 2 * interval)
			return held(s, step.shift, p);
		if (step.iteration == 4 * interval)
			return held(BigInt(1), step.shift, p);
		if (step.iteration == p - 2)
			return BigInt(0);
		return std::nullopt;
	};
	options.distance = [](const lucas::Step& step, double distance) {
		return step.iteration >= 700 and step.iteration < 704 and step.attempt == 0 ? 0.36 : distance;
	};

	log_peer::CaptureCerr quiet;
	const Result result = lucas::test(p, options);
	const std::string res64 = known_answer(p).res64;
	const std::string checks = std::to_string(checks_of(p) + 2);
	check::expect_equal(verdict_line(result),
	                    "M1237 ll: composite, res64 " + res64 + ", lengths " + std::to_string(first) + " " +
	                    std::to_string(second) + ", shift 1000, jacobi " + checks + " checked 1 failed, degenerate 1, "
	                    "confirmation 1, transient 2, retries 1, large distances 4, largest distance 0.3600 (mps " +
	                    APP_VERSION + ")",
	                    "the verdict line of a run with every count");
	check::expect_equal(json_record(result),
	                    "{\"exponent\": 1237, \"worktype\": \"ll\", \"status\": \"composite\", \"res64\": \"" + res64 +
	                    "\", \"lengths\": [" + std::to_string(first) + ", " + std::to_string(second) + "], \"shift\": 1000, "
	                    "\"jacobi_checks\": " + checks + ", \"jacobi_failures\": 1, \"degenerate_failures\": 1, "
	                    "\"confirmation_failures\": 1, \"transient_faults\": 2, \"length_retries\": 1, \"large_distances\": 4, "
	                    "\"largest_distance\": 0.36, \"program\": {\"name\": \"mps\", \"version\": \"" + APP_VERSION +
	                    "\"}, \"record_version\": 2}",
	                    "the record of a run with every count");

	static const std::uint64_t q = 127;
	lucas::Options failing = holding(q, 5, 2 * interval_of(q), failing_jacobi(q), UINT64_MAX);
	failing.distance = [](const lucas::Step&, double) { return 0.125; };
	const Result error = lucas::test(q, failing);
	const std::string length = std::to_string(ibdwt::length_for(q));
	const std::string jacobi = std::to_string(lucas::CHECK_FAILURES + 2);
	const std::string failed = std::to_string(lucas::CHECK_FAILURES + 1);
	check::expect(error.outcome() == OUTCOME_ERROR, "M127 failing every attempt: an error");
	check::expect_equal(verdict_line(error),
	                    "M127 ll: error, " + error.reason() + ", length " + length + ", shift 5, jacobi " + jacobi +
	                    " checked " + failed + " failed, degenerate 0, confirmation 0, transient 0, retries 0, "
	                    "large distances 0, largest distance 0.1250 (mps " + APP_VERSION + ")",
	                    "the line of an error");
	check::expect_equal(json_record(error),
	                    "{\"exponent\": 127, \"worktype\": \"ll\", \"status\": \"error\", \"reason\": \"" + error.reason() +
	                    "\", \"lengths\": [" + length + "], \"shift\": 5, \"jacobi_checks\": " + jacobi +
	                    ", \"jacobi_failures\": " + failed + ", \"degenerate_failures\": 0, \"confirmation_failures\": 0, "
	                    "\"transient_faults\": 0, \"length_retries\": 0, \"large_distances\": 0, \"largest_distance\": 0.125, "
	                    "\"program\": {\"name\": \"mps\", \"version\": \"" + APP_VERSION + "\"}, \"record_version\": 2}",
	                    "the record of an error");
	check::expect_equal(RECORD_VERSION, 2, "the record's format version is 2");
}


static void the_test_recovers_from_a_failed_check_up_to_its_limit() {
	//
	// A value with symbol +1 held at a checked iteration on its first CHECK_FAILURES attempts is
	// recovered from; held on every attempt, it ends the run in an error
	//
	// In M1237 and M1279. After each failure the run goes back to the last good state, the
	// checked iteration before, and redoes the iteration after it. The recovered run ends with the
	// known Res64 and that many failures; the error names the Jacobi check, has status 20 and no
	// Res64.

	log_peer::CaptureCerr quiet;
	for (const std::uint64_t p: {1237, 1279}) {
		const Known known = known_answer(p);
		const std::uint64_t checked = 4 * interval_of(p);
		const std::uint64_t good = checked - interval_of(p);
		const std::string label = "M" + std::to_string(p);

		lucas::Options recovering = holding(p, 3, checked, failing_jacobi(p), lucas::CHECK_FAILURES);
		std::vector<lucas::Step> steps;
		recovering.refuse = [&steps](const lucas::Step& step) { steps.push_back(step); };
		const Result recovered = lucas::test(p, recovering);
		check::require(recovered.run().has_value(), label + ": the recovered run is reported");
		check::expect_equal(res64_text(recovered), known.res64, label + " recovered: the known Res64");
		check::expect(is_decisive(recovered), label + " recovered: a verdict");
		check::expect_equal(recovered.run()->jacobi_failures, lucas::CHECK_FAILURES, label + " recovered: every failure counted");
		std::uint64_t returns = 0;
		for (std::size_t i = 0; i + 1 < steps.size(); i++)
			if (steps[i].iteration == checked and steps[i].attempt < lucas::CHECK_FAILURES) {
				const lucas::Step& next = steps[i + 1];
				returns += next.iteration == good + 1 and next.attempt == steps[i].attempt + 1 and
				           next.shift == shift_at(3, good + 1, p);
			}
		check::expect_equal(returns, lucas::CHECK_FAILURES,
		                    label + " recovered: each failure goes back to iteration " + std::to_string(good) + " and redoes the next");

		const Result failed = lucas::test(p, holding(p, 3, checked, failing_jacobi(p), UINT64_MAX));
		check::require(failed.run().has_value(), label + ": the failed run is reported");
		check::expect(failed.outcome() == OUTCOME_ERROR, label + " failing every attempt: an error");
		check::expect(has(failed.reason(), "Jacobi check"), label + " failing every attempt: the reason names the Jacobi check");
		check::expect_equal(status_of({failed}), 20, label + " failing every attempt: status 20");
		check::expect(not failed.res64(), label + " failing every attempt: no Res64");
		check::expect_equal(failed.run()->jacobi_failures, lucas::CHECK_FAILURES + 1, label + " failing every attempt: the failures counted");
	}

	// The limit counts the run's failures, not one good state's: in M1279, a value with symbol +1
	// held once each at CHECK_FAILURES + 1 checked iterations, each failure recovered from and a
	// good state passed between them, ends the run in an error at the last of them
	static const std::uint64_t p = 1279;
	const std::uint64_t interval = interval_of(p);
	const BigInt s = failing_jacobi(p);
	lucas::Options spread = at_shift(3);
	spread.corrupt = [interval, s](const lucas::Step& step, const BigInt&) -> std::optional<BigInt> {
		if (step.attempt == 0 and step.iteration % (2 * interval) == 0 and step.iteration / (2 * interval) >= 2 and
		    step.iteration / (2 * interval) <= lucas::CHECK_FAILURES + 2)
			return held(s, step.shift, p);
		return std::nullopt;
	};
	const Result ended = lucas::test(p, spread);
	const std::string at = std::to_string(2 * interval * (lucas::CHECK_FAILURES + 2));
	check::require(ended.run().has_value(), "M1279 failing once at each of CHECK_FAILURES + 1 checks: the run is reported");
	check::expect(ended.outcome() == OUTCOME_ERROR, "M1279 failing once at each of CHECK_FAILURES + 1 checks: an error");
	check::expect_equal(status_of({ended}), 20, "M1279 failing once at each of CHECK_FAILURES + 1 checks: status 20");
	check::expect_equal(ended.run()->jacobi_failures, lucas::CHECK_FAILURES + 1,
	                    "M1279 failing once at each of CHECK_FAILURES + 1 checks: every failure counted");
	check::expect(has(ended.reason(), "failed at iteration " + at + ": " + std::to_string(lucas::CHECK_FAILURES + 1) +
	                                  " failed checks in the run, past the limit of " + std::to_string(lucas::CHECK_FAILURES)),
	              "M1279 failing once at each of CHECK_FAILURES + 1 checks: the reason names iteration " + at + " and the count: " +
	              ended.reason());
}


static void the_test_replays_a_refused_squaring_once() {
	//
	// A squaring refused on its first attempt only is replayed at the same length, from the same
	// residue, and counted as one transient fault; an engine error that is not a refusal is never
	// replayed
	//
	// In M1279 and M1237, each ending with its known answer. The replay is the same iteration at
	// its next attempt, the same length and the same shift. An engine_error thrown before the
	// squaring at iteration 400 ends the run in an error that carries its message, with no replay.

	log_peer::CaptureCerr quiet;
	for (const std::uint64_t p: {1279, 1237}) {
		const std::string label = "M" + std::to_string(p);
		lucas::Options options = at_shift(11);
		std::vector<lucas::Step> steps;
		options.refuse = [p, &steps](const lucas::Step& step) {
			steps.push_back(step);
			if (step.iteration == 400 and step.attempt == 0)
				throw refusal(p, step);
		};
		const Result result = lucas::test(p, options);
		check::require(result.run().has_value(), label + ": the run is reported");
		check::expect_equal(res64_text(result), known_answer(p).res64, label + ": the known Res64");
		check::expect_equal(result.run()->transient_faults, std::uint64_t(1), label + ": one transient fault");
		check::expect_equal(result.run()->length_retries, std::uint64_t(0), label + ": no length retry");
		check::expect(result.run()->lengths == std::vector<std::uint64_t> {ibdwt::length_for(p)}, label + ": one length, the table's");
		check::require(steps.size() == p - 1, label + ": p - 2 squarings and the one replay");
		check::expect(steps[399].iteration == 400 and steps[400].iteration == 400 and steps[400].attempt == 1 and
		              steps[400].length == steps[399].length and steps[400].shift == steps[399].shift and
		              steps[401].iteration == 401,
		              label + ": the replay is iteration 400 again, at the same length and shift, and the run goes on");
	}

	static const std::uint64_t p = 1279;
	lucas::Options broken = at_shift(11);
	std::uint64_t calls = 0;
	broken.refuse = [&calls](const lucas::Step& step) {
		if (step.iteration == 400) {
			calls++;
			throw engine_error("an injected engine failure");
		}
	};
	const Result failed = lucas::test(p, broken);
	check::expect(failed.outcome() == OUTCOME_ERROR, "an engine error: an error");
	check::expect_equal(status_of({failed}), 20, "an engine error: status 20");
	check::expect(has(failed.reason(), "an injected engine failure"), "an engine error: the reason carries its message");
	check::expect_equal(calls, std::uint64_t(1), "an engine error: never replayed");
	check::expect(failed.run() and failed.run()->transient_faults == 0 and failed.run()->length_retries == 0,
	              "an engine error: no transient fault and no length retry");
}


static void the_test_moves_to_a_larger_length_when_a_refusal_reproduces() {
	//
	// A refusal that reproduces at the first length and not the next moves the run there, from its
	// last good state; one at every length ends the run at the limit; and one at the largest length
	// that holds p ends it at once
	//
	// M1279 refused at iteration 400 at its first length ends prime at two lengths, the first
	// squaring at the second length the one after the last good state, and each squaring after the
	// move on an engine of the second length, by the distances it reports; refused there at every
	// length, it is an error naming p, the length and the distance once it has moved
	// LENGTH_RETRIES times; M31 started at the largest length that holds it and refused is an
	// error saying no larger length holds it.

	log_peer::CaptureCerr quiet;
	static const std::uint64_t p = 1279;
	const std::uint64_t first = ibdwt::length_for(p);
	const std::uint64_t good = 399 / interval_of(p) * interval_of(p);

	lucas::Options once = at_shift(21);
	std::vector<lucas::Step> steps;
	once.refuse = [first, &steps](const lucas::Step& step) {
		steps.push_back(step);
		if (step.iteration == 400 and step.length == first)
			throw refusal(p, step);
	};
	std::vector<std::pair<std::uint64_t, double>> moved_distances;
	once.distance = [first, &moved_distances](const lucas::Step& step, double distance) {
		if (step.length != first)
			moved_distances.push_back({step.iteration, distance});
		return distance;
	};
	const Result moved = lucas::test(p, once);
	check::require(moved.run().has_value(), "the moved run is reported");
	check::expect(moved.outcome() == OUTCOME_PRIME, "the moved run: M1279 prime");
	check::expect_equal(moved.run()->length_retries, std::uint64_t(1), "the moved run: one length retry");
	check::expect_equal(moved.run()->transient_faults, std::uint64_t(0), "the moved run: no transient fault");
	check::expect(moved.run()->lengths == std::vector<std::uint64_t> {first, next_holding(first, p)},
	              "the moved run: the first length and the next that holds p");
	const auto at_second = std::find_if(steps.begin(), steps.end(), [first](const lucas::Step& step) { return step.length != first; });
	check::expect(at_second != steps.end() and at_second->iteration == good + 1 and at_second->shift == shift_at(21, good + 1, p),
	              "the moved run: back to the last good state, iteration " + std::to_string(good) + ", at the second length");

	// The squarings after the move run on an engine of the second length: each reports the
	// distance a run at that length from the start reports for the same iteration, from the same
	// residue at the same shift
	lucas::Options second = at_shift(21);
	second.length = next_holding(first, p);
	std::vector<std::pair<std::uint64_t, double>> second_distances;
	second.distance = [good, &second_distances](const lucas::Step& step, double distance) {
		if (step.iteration > good)
			second_distances.push_back({step.iteration, distance});
		return distance;
	};
	lucas::test(p, second);
	const bool some = std::any_of(moved_distances.begin(), moved_distances.end(), [](const auto& d) { return d.second > 0; });
	check::expect(some and moved_distances == second_distances,
	              "the moved run: after the move, every distance is that of a run at the second length (" +
	              std::to_string(moved_distances.size()) + " and " + std::to_string(second_distances.size()) + " squarings)");

	lucas::Options always = at_shift(21);
	always.refuse = [](const lucas::Step& step) {
		if (step.iteration == 400)
			throw refusal(p, step);
	};
	const Result limited = lucas::test(p, always);
	check::require(limited.run().has_value(), "the limited run is reported");
	std::vector<std::uint64_t> lengths = {first};
	for (std::uint64_t retry = 0; retry < lucas::LENGTH_RETRIES; retry++)
		lengths.push_back(next_holding(lengths.back(), p));
	check::expect(limited.outcome() == OUTCOME_ERROR, "refused at every length: an error");
	check::expect_equal(status_of({limited}), 20, "refused at every length: status 20");
	check::expect(not limited.res64(), "refused at every length: no Res64");
	check::expect_equal(limited.run()->length_retries, lucas::LENGTH_RETRIES, "refused at every length: the limit of retries");
	check::expect(limited.run()->lengths == lengths, "refused at every length: each next length that holds p");
	for (const std::string& part: {std::string("M1279"), "length " + std::to_string(lengths.back()), std::string("0.4700")})
		check::expect(has(limited.reason(), part), "refused at every length: the reason names '" + part + "'");

	static const std::uint64_t q = 31;
	std::uint64_t largest = 0;
	for (const std::uint64_t length: ibdwt::supported_lengths())
		if (ibdwt::holds(length, q))
			largest = length;
	lucas::Options at_largest = at_shift(0);
	at_largest.length = largest;
	at_largest.refuse = [](const lucas::Step& step) {
		if (step.iteration == 10)
			throw refusal(q, step);
	};
	const Result ended = lucas::test(q, at_largest);
	check::require(ended.run().has_value(), "the run at the largest length is reported");
	check::expect(ended.outcome() == OUTCOME_ERROR, "refused at the largest length: an error");
	check::expect_equal(status_of({ended}), 20, "refused at the largest length: status 20");
	check::expect(has(ended.reason(), "no larger supported length holds 31"), "refused at the largest length: no larger length holds 31");
	check::expect(ended.run()->lengths == std::vector<std::uint64_t> {largest}, "refused at the largest length: that length alone");
	check::expect_equal(ended.run()->length_retries, std::uint64_t(0), "refused at the largest length: no retry");
}


static void the_test_counts_and_logs_large_round_off_distances() {
	//
	// Twelve squarings of M1279 reported at 0.36, past the sizing distance, are counted, the first
	// DISTANCES_LOGGED logged one by one and the rest in one folded line; and the distances the
	// engine reports are the run's
	//
	// Each logged line names its iteration, the length and the distance. Through a hook that
	// passes every distance on unchanged, the largest the run reports is the largest the engine
	// gave, above zero.

	static const std::uint64_t p = 1279;
	log_peer::SavedLogger saved;
	logging::level = logging::INFO;
	logging::style = logging::DEFAULT;

	std::vector<std::uint64_t> iterations;
	for (std::uint64_t i = 1; i <= 12; i++)
		iterations.push_back(100 * i);
	lucas::Options options = at_shift(0);
	options.distance = [iterations](const lucas::Step& step, double distance) {
		return std::count(iterations.begin(), iterations.end(), step.iteration) ? 0.36 : distance;
	};

	std::string text;
	Result result = Result::no_verdict(WORK_LL, p, "not run");
	{
		log_peer::CaptureCerr log;
		result = lucas::test(p, options);
		text = log.text();
	}
	check::require(result.run().has_value(), "the run is reported");
	check::expect(result.outcome() == OUTCOME_PRIME, "M1279 prime");
	check::expect_equal(result.run()->large_distances, std::uint64_t(12), "twelve large distances counted");
	check::expect_equal(result.run()->largest_distance, 0.36, "the largest distance");
	check::expect_equal(count_of(text, "has round-off distance"), std::size_t(lucas::DISTANCES_LOGGED), "DISTANCES_LOGGED lines logged");
	const std::string length = std::to_string(ibdwt::length_for(p));
	for (std::size_t i = 0; i < lucas::DISTANCES_LOGGED; i++)
		check::expect(has(text, "ll M1279: iteration " + std::to_string(iterations[i]) + " at length " + length +
		                        " has round-off distance 0.3600, past 0.35"),
		              "the line of iteration " + std::to_string(iterations[i]));
	check::expect(has(text, "ll M1279: " + std::to_string(12 - lucas::DISTANCES_LOGGED) +
	                        " more squarings past the sizing distance, counted and not logged"),
	              "the rest folded into one line");

	// DISTANCES_LOGGED squarings past the sizing distance, each a distance of its own, and one at
	// the sizing distance itself, which it does not exceed: the eight counted and logged, each line
	// naming its own squaring's distance, nothing folded, and the one at the limit neither counted
	// nor logged
	lucas::Options exact = at_shift(0);
	auto large = [](std::uint64_t i) { return 0.36 + double(lucas::DISTANCES_LOGGED - i / 100) / 100; };
	exact.distance = [large](const lucas::Step& step, double distance) {
		if (step.iteration % 100 == 0 and step.iteration / 100 >= 1 and step.iteration / 100 <= lucas::DISTANCES_LOGGED)
			return large(step.iteration);
		return step.iteration == 900 ? ibdwt::SIZING_DISTANCE : distance;
	};
	std::string exact_text;
	Result exact_result = Result::no_verdict(WORK_LL, p, "not run");
	{
		log_peer::CaptureCerr log;
		exact_result = lucas::test(p, exact);
		exact_text = log.text();
	}
	check::require(exact_result.run().has_value(), "the run with DISTANCES_LOGGED large distances is reported");
	check::expect_equal(exact_result.run()->large_distances, std::uint64_t(lucas::DISTANCES_LOGGED),
	                    "DISTANCES_LOGGED large distances counted, and not the one at the sizing distance");
	check::expect_equal(exact_result.run()->largest_distance, large(100), "the largest of them, the run's largest");
	check::expect_equal(count_of(exact_text, "has round-off distance"), std::size_t(lucas::DISTANCES_LOGGED),
	                    "DISTANCES_LOGGED lines logged, none for the one at the sizing distance");
	for (std::uint64_t i = 100; i <= 100 * lucas::DISTANCES_LOGGED; i += 100)
		check::expect(has(exact_text, "ll M1279: iteration " + std::to_string(i) + " at length " + length +
		                              " has round-off distance " + distance_text(large(i)) + ", past 0.35"),
		              "the line of iteration " + std::to_string(i) + " names its own distance, " + distance_text(large(i)));
	check::expect(not has(exact_text, "more squarings past the sizing distance"), "nothing folded when no more than DISTANCES_LOGGED");

	double largest = 0;
	lucas::Options passed = at_shift(0);
	passed.distance = [&largest](const lucas::Step&, double distance) {
		largest = std::max(largest, distance);
		return distance;
	};
	log_peer::CaptureCerr quiet;
	const Result plain = lucas::test(p, passed);
	check::require(plain.run().has_value(), "the plain run is reported");
	check::expect(largest > 0, "the engine reports distances above zero");
	check::expect_equal(plain.run()->largest_distance, largest, "the largest distance reported is the engine's largest");
	check::expect_equal(plain.run()->large_distances, std::uint64_t(0), "no distance of the plain run past the sizing distance");
}


static double rate_of(const std::string& line) {
	//
	// The milliseconds per iteration a progress line gives, or -1

	const std::size_t end = line.find(" ms per iteration");
	const std::size_t start = line.rfind(", ", end);
	if (end == std::string::npos or start == std::string::npos)
		return -1;
	return std::stod(line.substr(start + 2, end - start - 2));
}


// One progress line, read back from the log
struct Progress {
	std::uint64_t iteration;
	std::uint64_t total;
	double ms;              // the time per iteration it prints, in milliseconds
	double to_go;           // the time to go it prints, in seconds
	std::string largest;    // the largest distance it prints, as printed
};


static std::vector<Progress> progress_of(const std::string& text, std::uint64_t p) {
	//
	// Every line of the log in the form the progress line takes, in order
	//
	// "ll M<p>: iteration <i> of <total>, <x.xxx> ms per iteration, about <h>h<mm>m<ss>s to go,
	// largest distance <d.dddd> since the last line"; a line of the log that starts so and is not
	// in this form is left out, so a count of lines sees it.

	const std::regex form("ll M" + std::to_string(p) + R"(: iteration (\d+) of (\d+), (\d+\.\d{3}) ms per iteration, )"
	                      R"(about (\d+)h([0-5]\d)m([0-5]\d)s to go, largest distance (\d\.\d{4}) since the last line$)");
	std::vector<Progress> lines;
	for (std::size_t at = 0; at < text.size();) {
		const std::size_t end = std::min(text.find('\n', at), text.size());
		const std::string line = text.substr(at, end - at);
		at = end + 1;
		std::smatch m;
		if (std::regex_search(line, m, form))
			lines.push_back({std::stoull(m[1]), std::stoull(m[2]), std::stod(m[3]),
			                 3600.0 * std::stod(m[4]) + 60.0 * std::stod(m[5]) + std::stod(m[6]), m[7]});
	}
	return lines;
}


static bool to_go_agrees(const Progress& line) {
	//
	// Whether a line's time to go is its time per iteration times the iterations left after it, to
	// the nearest second
	//
	// The time per iteration is printed to the microsecond, so each iteration left adds up to half
	// a microsecond to the difference.

	const double left = double(line.total - line.iteration);
	return std::abs(line.to_go - line.ms / 1e3 * left) <= 0.5 + left * 0.5e-6 + 1e-9;
}


static void the_test_logs_its_progress() {
	//
	// Each value a progress line prints is what it means: the iteration and the run's last, the
	// time per iteration since the last line, the time to go from the iterations left, and the
	// largest distance since the last line
	//
	// With a progress interval of 0, M127 logs one line per iteration, each giving the distance its
	// squaring reported. With an interval of 25 ms and the distance hook sleeping at chosen
	// iterations, M127 logs a line at each of them, each spanning the squarings since the line
	// before: its time per iteration, times that span, is at least the sleep and at most the sleep
	// plus the interval plus 100 ms; its largest distance is the largest of the span. The run's
	// largest distance is the run's, not the last line's. A line of M86243 after a sleep of 60 ms
	// gives over an hour to go in hours, minutes and seconds. On every line, the time to go is the
	// time per iteration times the iterations left. The clock is the system's, so the times are
	// bounded, not fixed.
	//
	// And a run that goes back to a good state keeps timing its squarings: the line after the
	// recovery gives a time per iteration within a factor of ten of the run's median.

	static const std::uint64_t p = 127;
	log_peer::SavedLogger saved;
	logging::level = logging::INFO;
	logging::style = logging::DEFAULT;

	lucas::Options options = at_shift(0);
	options.progress_seconds = 0;
	options.distance = [](const lucas::Step& step, double) { return double(step.iteration % 10) / 100; };
	std::string text;
	{
		log_peer::CaptureCerr log;
		check::expect(lucas::test(p, options).outcome() == OUTCOME_PRIME, "M127 prime");
		text = log.text();
	}
	check::expect_equal(count_of(text, "ll M127: iteration "), std::size_t(p - 2), "one progress line per iteration");
	bool every = true;
	for (std::uint64_t i = 1; i <= p - 2; i++) {
		const std::size_t at = text.find("ll M127: iteration " + std::to_string(i) + " of 125, ");
		const std::string line = at == std::string::npos ? "" : text.substr(at, text.find('\n', at) - at);
		every = every and has(line, " ms per iteration, about ") and has(line, "s to go, largest distance ") and
		        has(line, " since the last line");
	}
	check::expect(every, "every line names the iteration, the time per iteration, the time to go and the distance");
	const std::vector<Progress> each = progress_of(text, p);
	check::require(each.size() == p - 2, "every progress line in its form (" + std::to_string(each.size()) + " lines)");
	bool in_order = true, own = true, agrees = true;
	for (std::uint64_t i = 1; i <= p - 2; i++) {
		const Progress& line = each[i - 1];
		in_order = in_order and line.iteration == i and line.total == p - 2;
		own = own and line.largest == distance_text(double(i % 10) / 100);
		agrees = agrees and to_go_agrees(line);
	}
	check::expect(in_order, "the lines give iterations 1 to 125 in order, each of 125");
	check::expect(own, "each line gives the distance of its own squaring, the only one since the line before");
	check::expect(agrees, "each line's time to go is its time per iteration times the iterations left");

	// Lines that span several squarings, at the iterations where the distance hook sleeps
	struct Sleep {
		std::uint64_t iteration;
		int ms;
	};
	static const std::vector<Sleep> sleeps = {{40, 150}, {41, 40}, {100, 40}};
	static const double interval = 0.025;
	auto injected = [](std::uint64_t i) { return i == 30 ? 0.3 : i == 41 ? 0.2 : i == 70 ? 0.1 : 0.01; };
	lucas::Options spanning = at_shift(0);
	spanning.progress_seconds = interval;
	spanning.distance = [injected](const lucas::Step& step, double) {
		for (const Sleep& sleep: sleeps)
			if (step.iteration == sleep.iteration)
				std::this_thread::sleep_for(std::chrono::milliseconds(sleep.ms));
		return injected(step.iteration);
	};
	std::string spanned;
	Result spanning_result = Result::no_verdict(WORK_LL, p, "not run");
	{
		log_peer::CaptureCerr log;
		spanning_result = lucas::test(p, spanning);
		spanned = log.text();
	}
	check::require(spanning_result.run().has_value(), "M127 with sleeps: the run is reported");
	check::expect_equal(spanning_result.run()->largest_distance, 0.3, "M127 with sleeps: the run's largest distance, 0.3 at iteration 30");
	const std::vector<Progress> lines = progress_of(spanned, p);
	check::expect_equal(lines.size(), count_of(spanned, "ll M127: iteration "), "M127 with sleeps: every progress line in its form");
	std::uint64_t before = 0;
	for (const Progress& line: lines) {
		const std::string label = "M127 with sleeps, the line at iteration " + std::to_string(line.iteration);
		const std::uint64_t span = line.iteration - before;
		double largest = 0;
		for (std::uint64_t i = before + 1; i <= line.iteration; i++)
			largest = std::max(largest, injected(i));
		check::expect_equal(line.largest, distance_text(largest),
		                    label + ": the largest distance of iterations " + std::to_string(before + 1) + " to " + std::to_string(line.iteration));
		check::expect(line.total == p - 2 and to_go_agrees(line), label + ": of 125, the time to go its time per iteration times the iterations left");
		int slept = 0;
		for (const Sleep& sleep: sleeps)
			slept += sleep.iteration == line.iteration ? sleep.ms : 0;
		const double seconds = line.ms * double(span) / 1e3;
		check::expect(seconds >= slept / 1e3 - double(span) * 0.5e-6 and seconds <= slept / 1e3 + interval + 0.1,
		              label + ": the time per iteration times its " + std::to_string(span) + " squarings (" +
		              std::to_string(seconds) + " s) covers the sleep of " + std::to_string(slept) + " ms and little more");
		before = line.iteration;
	}
	for (const Sleep& sleep: sleeps)
		check::expect(std::any_of(lines.begin(), lines.end(), [&sleep](const Progress& line) { return line.iteration == sleep.iteration; }),
		              "M127 with sleeps: a line at iteration " + std::to_string(sleep.iteration));

	// Over an hour to go: one squaring of M86243 that takes at least 60 ms, then an engine error that
	// ends the run
	static const std::uint64_t q = 86243;
	lucas::Options long_run = at_shift(0);
	long_run.progress_seconds = interval;
	long_run.distance = [](const lucas::Step& step, double distance) {
		if (step.iteration == 1)
			std::this_thread::sleep_for(std::chrono::milliseconds(60));
		return distance;
	};
	long_run.refuse = [](const lucas::Step& step) {
		if (step.iteration == 2)
			throw engine_error("an injected end of the run");
	};
	std::string long_text;
	{
		log_peer::CaptureCerr log;
		check::expect(lucas::test(q, long_run).outcome() == OUTCOME_ERROR, "M86243 ended at iteration 2: an error");
		long_text = log.text();
	}
	const std::vector<Progress> first = progress_of(long_text, q);
	check::require(first.size() == 1 and first[0].iteration == 1 and first[0].total == q - 2,
	               "M86243: one line, at iteration 1 of 86241, in its form");
	check::expect(first[0].ms >= 60 and first[0].to_go >= 3600 and to_go_agrees(first[0]),
	              "M86243: at least 60 ms per iteration, over an hour to go, and the time to go its time per iteration times the "
	              "iterations left (" + std::to_string(first[0].ms) + " ms, " + std::to_string(first[0].to_go) + " s)");

	lucas::Options recovering = holding(p, 0, 2 * interval_of(p), BigInt(1));
	recovering.progress_seconds = 0;
	std::string recovered;
	{
		log_peer::CaptureCerr log;
		check::expect(lucas::test(p, recovering).outcome() == OUTCOME_PRIME, "M127 recovered: prime");
		recovered = log.text();
	}
	std::vector<double> rates;
	double after = -1;
	bool next = false;
	for (std::size_t at = 0; at < recovered.size();) {
		const std::size_t end = recovered.find('\n', at);
		const std::string line = recovered.substr(at, end - at);
		at = end == std::string::npos ? recovered.size() : end + 1;
		if (has(line, "going back to iteration")) {
			next = true;
		} else if (has(line, " ms per iteration")) {
			rates.push_back(rate_of(line));
			if (next)
				after = rate_of(line);
			next = false;
		}
	}
	check::require(not rates.empty(), "the recovered run logs its progress");
	std::sort(rates.begin(), rates.end());
	const double median = rates[rates.size() / 2];
	check::expect(after > median / 10 and after < median * 10,
	              "the line after the recovery within ten times the median (" + std::to_string(after) + " ms against " +
	              std::to_string(median) + " ms)");
}


static check::Suite
check_by_name {
	{"the test reports the known answers at two given shifts",                   the_test_reports_the_known_answers_at_two_given_shifts},
	{"the test chooses a fresh shift each run",                                  the_test_chooses_a_fresh_shift_each_run},
	{"the test checks the Jacobi symbol at intervals and on the final residue",  the_test_checks_the_jacobi_symbol_at_intervals_and_on_the_final_residue},
	{"the test catches degenerate states",                                       the_test_catches_degenerate_states},
	{"the test confirms a zero residue before it reports prime",                 the_test_confirms_a_zero_residue_before_it_reports_prime},
	{"the test refuses exponents it cannot run",                                 the_test_refuses_exponents_it_cannot_run},
	{"the result of the test names its run in both views",                       the_result_of_the_test_names_its_run_in_both_views},
	{"the test recovers from a failed check, up to its limit",                   the_test_recovers_from_a_failed_check_up_to_its_limit},
	{"the test replays a refused squaring once",                                 the_test_replays_a_refused_squaring_once},
	{"the test moves to a larger length when a refusal reproduces",              the_test_moves_to_a_larger_length_when_a_refusal_reproduces},
	{"the test counts and logs large round-off distances",                       the_test_counts_and_logs_large_round_off_distances},
	{"the test logs its progress",                                               the_test_logs_its_progress},
};
