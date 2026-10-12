// Gate probe: a cycle record, never part of mps.
//
// Built by the gates against the tree's own modules (all of src/ but main.cpp), with GATE_PHASE set
// to the gate's phase. Each mode checks the builder's code against the gate's own expectations and
// prints one line per mismatch, then `probe <mode>: ok` when there was none.
//
//   probe printer        each outcome's line, record and status, through status_of and the printer;
//                        R9's failures; the stop and order of run_in_order; test_all; the stubs
//   probe records FILE   writes six constructed records to FILE, for the gate's JSON parser
//   probe exponent       smallest_prime_factor, composite_by_exponent and check_exponent (phase 2)


// Standard libs
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// Internal libs
#include "main.hpp"
#include "math.hpp"
#include "result.hpp"


static int bad = 0;


static void want(bool ok, const std::string& what) {
	if (not ok) {
		bad++;
		std::cout << "FAIL " << what << std::endl;
	}
}


template <typename T>
static void want_equal(const T& got, const T& expected, const std::string& what) {
	if (not (got == expected)) {
		bad++;
		std::cout << "FAIL " << what << "\n   got:  " << got << "\n   want: " << expected << std::endl;
	}
}


static const std::string TAIL = " (mps " + APP_VERSION + ")";
static const std::string PROGRAM = "\"program\": {\"name\": \"mps\", \"version\": \"" + APP_VERSION + "\"}, \"record_version\": 1}";


struct Printed {
	std::string out;
	std::string err;
	int status;
};


// The printer over a run, as main.cpp drives it: report_result per result, then report_run
static Printed print_run(const std::vector<Result>& results, bool json) {
	std::ostringstream out, err;
	std::streambuf* saved = std::cerr.rdbuf(err.rdbuf());
	std::vector<Result> run;
	for (const Result& result: results)
		report_result(run, result, json, out);
	const int status = report_run(run, json, out);
	std::cerr.rdbuf(saved);
	return {out.str(), err.str(), status};
}


static std::string shown(const Result& result, bool json) {
	return (json ? json_record(result) : verdict_line(result)) + "\n";
}


static void printer() {

	const Result nv = Result::no_verdict(WORK_TF, 31, "trial factoring is not implemented yet");
	const Result prime = Result::prime(WORK_LL, 31);
	const Result pp = Result::probable_prime(WORK_LL, 31);
	const Result by_factor = Result::composite_by_factor(WORK_PTEST, 33, "2^3-1");
	const Result by_residue = Result::composite_by_residue(WORK_LL, 999'999'937, 0x0123456789ABCDEF);
	const Result error = Result::error(WORK_LL, 31, "the gate's error");

	struct Case {
		Result result;
		std::string line;
		std::string record;
		int status;
	};
	const std::vector<Case> cases = {
		{nv, "M31 tf: no verdict, trial factoring is not implemented yet" + TAIL,
		 "{\"exponent\": 31, \"worktype\": \"tf\", \"status\": \"no-verdict\", \"reason\": \"trial factoring is not implemented yet\", " + PROGRAM, 42},
		{prime, "M31 ll: prime" + TAIL, "{\"exponent\": 31, \"worktype\": \"ll\", \"status\": \"prime\", " + PROGRAM, 0},
		{pp, "M31 ll: probable prime" + TAIL, "{\"exponent\": 31, \"worktype\": \"ll\", \"status\": \"probable-prime\", " + PROGRAM, 41},
		{by_factor, "M33 ptest: composite, factor 2^3-1" + TAIL,
		 "{\"exponent\": 33, \"worktype\": \"ptest\", \"status\": \"composite\", \"factors\": [\"2^3-1\"], " + PROGRAM, 40},
		{by_residue, "M999999937 ll: composite, res64 0123456789ABCDEF" + TAIL,
		 "{\"exponent\": 999999937, \"worktype\": \"ll\", \"status\": \"composite\", \"res64\": \"0123456789ABCDEF\", " + PROGRAM, 40},
	};

	for (const Case& c: cases) {
		want_equal(verdict_line(c.result), c.line, "verdict_line");
		want_equal(json_record(c.result), c.record, "json_record");
		want_equal(status_of({c.result}), c.status, "status_of for " + c.line);
		for (bool json: {false, true}) {
			const Printed p = print_run({c.result}, json);
			want_equal(p.out, (json ? c.record : c.line) + "\n", "the printer's output for one result");
			want_equal(p.status, c.status, "the printer's status for " + c.line);
		}
	}
	want(verdict_line(Result::composite_by_residue(WORK_LL, 31, 1)).find("res64 0000000000000001 ") != std::string::npos,
	     "res64 written as 16 zero-padded digits");
	want_equal(verdict_line(error), "M31 ll: error, the gate's error" + TAIL, "verdict_line of an error, the text stderr reports");
	want_equal(json_record(error), std::string(""), "no JSON record exists for an error");
	want_equal(status_of({error}), 20, "status_of an error");
	for (const Result& r: {nv, prime, pp, by_factor, by_residue, error})
		want_equal(is_decisive(r), r.outcome() == OUTCOME_PRIME or r.outcome() == OUTCOME_PROBABLE_PRIME or r.outcome() == OUTCOME_COMPOSITE,
		           "is_decisive for " + verdict_line(r));

	struct Run {
		std::vector<Result> results;
		int status;
		std::vector<Result> printed;
		bool reported;   // something on stderr
	};
	const std::vector<Run> runs = {
		{{nv, error}, 20, {nv}, true},
		{{error}, 20, {}, true},
		{{by_factor, error}, 20, {}, true},
		{{error, by_factor}, 20, {}, true},
		{{prime, by_factor}, 30, {}, true},
		{{by_factor, prime}, 30, {}, true},
		{{pp, by_residue}, 30, {}, true},
		{{nv, prime, by_factor}, 30, {nv}, true},
		{{nv, nv, prime}, 0, {nv, nv, prime}, false},
		{{nv, by_factor}, 40, {nv, by_factor}, false},
		{{nv, pp}, 41, {nv, pp}, false},
		{{nv, nv, nv}, 42, {nv, nv, nv}, false},
		{{}, 42, {}, false},
	};
	for (size_t i = 0; i < runs.size(); i++) {
		const Run& r = runs[i];
		const std::string what = "run " + std::to_string(i + 1);
		want_equal(status_of(r.results), r.status, what + ": status_of");
		for (bool json: {false, true}) {
			const Printed p = print_run(r.results, json);
			std::string expected;
			for (const Result& result: r.printed)
				expected += shown(result, json);
			want_equal(p.out, expected, what + ": printed");
			want_equal(p.status, r.status, what + ": the printer's status");
			want_equal(not p.err.empty(), r.reported, what + ": reported on stderr");
			if (r.status == 20)
				want(p.err.find("the gate's error") != std::string::npos, what + ": the error's reason on stderr");
		}
	}

	// A no verdict prints at once; a verdict waits for report_run
	{
		std::ostringstream out;
		std::vector<Result> run;
		report_result(run, nv, false, out);
		want_equal(out.str(), shown(nv, false), "a no verdict prints as it arrives");
		report_result(run, prime, false, out);
		want_equal(out.str(), shown(nv, false), "a verdict is held until the run ends");
		want_equal(report_run(run, false, out), 0, "the held prime's status");
		want_equal(out.str(), shown(nv, false) + shown(prime, false), "the held prime printed at the end");
	}

	// run_in_order: in order, each result handed over before the next test runs, stopping at the
	// first decisive result
	static std::vector<std::string> ran;
	static const TestFunction t_nv = [](const long unsigned p) { ran.push_back("nv"); return Result::no_verdict(WORK_TF, p, "x"); };
	static const TestFunction t_comp = [](const long unsigned p) { ran.push_back("comp"); return Result::composite_by_factor(WORK_P1, p, "3"); };
	static const TestFunction t_prime = [](const long unsigned p) { ran.push_back("prime"); return Result::prime(WORK_LL, p); };
	static const TestFunction t_pp = [](const long unsigned p) { ran.push_back("pp"); return Result::probable_prime(WORK_LL, p); };
	static const TestFunction t_err = [](const long unsigned p) { ran.push_back("err"); return Result::error(WORK_LL, p, "x"); };
	struct Order {
		std::vector<TestFunction> tests;
		std::string handed;
		Outcome last;
	};
	const std::vector<Order> orders = {
		{{t_nv, t_comp, t_prime}, "nv comp ", OUTCOME_COMPOSITE},
		{{t_prime, t_nv, t_nv}, "prime ", OUTCOME_PRIME},
		{{t_nv, t_nv, t_nv}, "nv nv nv ", OUTCOME_NO_VERDICT},
		{{t_nv, t_nv, t_prime}, "nv nv prime ", OUTCOME_PRIME},
		{{t_nv, t_pp, t_nv}, "nv pp ", OUTCOME_PROBABLE_PRIME},
		{{t_err, t_nv, t_comp, t_nv}, "err nv comp ", OUTCOME_COMPOSITE},
	};
	for (const Order& o: orders) {
		ran.clear();
		std::string handed;
		size_t handed_count = 0;
		const Result last = run_in_order(7, o.tests, [&](const Result& result) {
			const Outcome o = result.outcome();
			handed += std::string(o == OUTCOME_NO_VERDICT ? "nv" : o == OUTCOME_PRIME ? "prime" : o == OUTCOME_PROBABLE_PRIME ? "pp" :
			                      o == OUTCOME_ERROR ? "err" : "comp") + " ";
			handed_count++;
			want_equal(ran.size(), handed_count, "run_in_order hands each result over before the next test runs");
		});
		want_equal(handed, o.handed, "run_in_order's results");
		want_equal(ran.size(), size_t(std::count(o.handed.begin(), o.handed.end(), ' ')), "run_in_order runs no test after a verdict");
		want(last.outcome() == o.last, "run_in_order returns the last result handed over");
	}

	// test_all: tf, p1, ll, each no verdict; the stubs never decide
	{
		std::vector<std::string> got;
		std::ostringstream err;
		std::streambuf* saved = std::cerr.rdbuf(err.rdbuf());
		const Result last = test_all(31, [&](const Result& result) { got.push_back(verdict_line(result)); });
		std::cerr.rdbuf(saved);
		want_equal(got.size(), size_t(3), "test_all hands over three results");
		if (got.size() == 3) {
			want_equal(got[0], "M31 " + std::string("tf: no verdict, trial factoring is not implemented yet") + TAIL, "test_all's first");
			want_equal(got[1], "M31 " + std::string("p1: no verdict, P-1 is not implemented yet") + TAIL, "test_all's second");
			want_equal(got[2], "M31 " + std::string("ll: no verdict, Lucas-Lehmer is not implemented yet") + TAIL, "test_all's third");
		}
		want(last.outcome() == OUTCOME_NO_VERDICT, "test_all returns a no verdict");
		const std::string e = err.str();
		const size_t tf = e.find("Running quick factor test"), p1 = e.find("Running P-1 test"), ll = e.find("Running Lucas-Lehmer test");
		want(tf != std::string::npos and p1 != std::string::npos and ll != std::string::npos and tf < p1 and p1 < ll,
		     "test_all logs the three start lines in order");
		std::cerr.rdbuf(err.rdbuf());
		for (long unsigned p: {2ul, 3ul, 4ul, 31ul, 33ul, 999'999'937ul, 999'999'999ul})
			for (const TestFunction test: {test_quick_factor, test_p_minus_one, test_lucas_lehmer})
				want(test(p).outcome() == OUTCOME_NO_VERDICT, "a stub decides nothing for " + std::to_string(p));
		std::cerr.rdbuf(saved);
	}
}


static void records(const char* path) {
	std::ofstream out(path);
	out << json_record(Result::no_verdict(WORK_TF, 31, "trial factoring is not implemented yet")) << "\n"
	    << json_record(Result::prime(WORK_LL, 31)) << "\n"
	    << json_record(Result::probable_prime(WORK_LL, 31)) << "\n"
	    << json_record(Result::composite_by_factor(WORK_PTEST, 33, "2^3-1")) << "\n"
	    << json_record(Result::composite_by_residue(WORK_LL, 999'999'937, 0x0123456789ABCDEF)) << "\n"
	    << json_record(Result::no_verdict(WORK_P1, 5, "a\"b\\c\nd\x01" "e")) << "\n";
}


#if GATE_PHASE >= 2
static void exponent() {

	size_t mismatches = 0;
	for (long unsigned n = 2; n < 200'000; n++) {
		long unsigned spf = n;
		for (long unsigned d = 2; d * d <= n; d++)
			if (n % d == 0) {
				spf = d;
				break;
			}
		if (smallest_prime_factor(n) != spf)
			mismatches++;
	}
	want_equal(mismatches, size_t(0), "smallest_prime_factor below 200000");

	const std::vector<std::pair<long unsigned, long unsigned>> composites = {
		{4, 2}, {9, 3}, {33, 3}, {961, 31}, {999'999'999, 3}, {999'999'998, 2}, {998'623'201, 31'601},
		{998'812'807, 31'601}, {999'002'449, 31'607},
	};
	for (const auto& [p, a]: composites) {
		want_equal(smallest_prime_factor(p), a, "smallest_prime_factor(" + std::to_string(p) + ")");
		const std::optional<Result> r = check_exponent(p);
		want(r.has_value(), "check_exponent answers " + std::to_string(p));
		if (r)
			want_equal(json_record(*r), "{\"exponent\": " + std::to_string(p) + ", \"worktype\": \"ptest\", \"status\": \"composite\", \"factors\": [\"2^" +
			           std::to_string(a) + "-1\"], " + PROGRAM, "check_exponent's record for " + std::to_string(p));
	}
	for (long unsigned p: {2ul, 3ul, 5ul, 31ul, 31'607ul, 999'999'929ul, 999'999'937ul})
		want(not check_exponent(p).has_value(), "check_exponent gives no result for the prime " + std::to_string(p));

	for (const auto& [p, a]: std::vector<std::pair<long unsigned, long unsigned>>{
	         {33, 0}, {33, 1}, {33, 5}, {33, 33}, {33, 66}, {4, 4}, {1, 1}, {0, 0}, {2, 1}})
		want(composite_by_exponent(p, a).outcome() == OUTCOME_ERROR,
		     "composite_by_exponent refuses (" + std::to_string(p) + ", " + std::to_string(a) + ")");
	want_equal(verdict_line(composite_by_exponent(33, 11)), "M33 ptest: composite, factor 2^11-1" + TAIL, "composite_by_exponent(33, 11)");
	want_equal(verdict_line(composite_by_exponent(999'002'449, 31'607)), "M999002449 ptest: composite, factor 2^31607-1" + TAIL,
	           "composite_by_exponent(999002449, 31607)");
}
#endif


int main(int argc, char** argv) {

	const std::string mode = argc > 1 ? argv[1] : "";
	if (mode == "printer")
		printer();
	else if (mode == "records" and argc == 3)
		records(argv[2]);
#if GATE_PHASE >= 2
	else if (mode == "exponent")
		exponent();
#endif
	else {
		std::cout << "usage: probe printer | records FILE | exponent" << std::endl;
		return 2;
	}
	if (bad == 0)
		std::cout << "probe " << mode << ": ok" << std::endl;
	return bad == 0 ? 0 : 1;
}
