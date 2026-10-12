// Gate mutant interposer: a cycle record, never part of mps.
//
// The gates copy the tree and rename the *definitions* (only) of the functions below to
// gate_real_NAME in src/result.cpp (is_decisive, verdict_line, json_record, status_of,
// report_result, report_run) and src/math.cpp (the three tests, run_in_order, test_all,
// smallest_prime_factor, composite_by_exponent, check_exponent), and prepend
// `#define parse_args gate_real_parse_args` to src/cli.cpp; then they build this file as one more
// module. Every call, from main.cpp, the checks or the same file, reaches the wrappers below. With
// GATE_MUTANT unset they pass everything through. With GATE_MUTANT=<id>[@<instance>] they inject one
// defect, only at that instance when one is named, and append the mutant to GATE_MUTANT_LOG each
// time it changes something. src/gate_phase.h, written by the gate, says which phase's functions
// exist.


// Standard libs
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

// Internal libs
#include "gate_phase.h"
#include "cli.hpp"
#include "main.hpp"
#include "math.hpp"
#include "result.hpp"


bool gate_real_is_decisive(const Result& result);
std::string gate_real_verdict_line(const Result& result);
std::string gate_real_json_record(const Result& result);
int gate_real_status_of(const std::vector<Result>& results);
void gate_real_report_result(std::vector<Result>& run, const Result& result, bool json, std::ostream& out);
int gate_real_report_run(const std::vector<Result>& run, bool json, std::ostream& out);
Result gate_real_test_quick_factor(const long unsigned p);
Result gate_real_test_p_minus_one(const long unsigned p);
Result gate_real_test_lucas_lehmer(const long unsigned p);
Result gate_real_run_in_order(const long unsigned p, const std::vector<TestFunction>& tests, const ResultSink& completed);
Result gate_real_test_all(const long unsigned p, const ResultSink& completed);
#if GATE_PHASE >= 2
long unsigned gate_real_smallest_prime_factor(const long unsigned p);
Result gate_real_composite_by_exponent(const long unsigned p, const long unsigned a);
std::optional<Result> gate_real_check_exponent(const long unsigned p);
#endif
namespace cli {
	Interface gate_real_parse_args(const int argc, const char** argv);
}


static std::string mutant() {
	const char* m = std::getenv("GATE_MUTANT");
	return m ? m : "";
}


// The mutant's id, without its instance
static bool is(const char* id) {
	const std::string m = mutant();
	return m.substr(0, m.find('@')) == id;
}


// The mutant's instance, or "" when it names none
static std::string instance() {
	const std::string m = mutant();
	const size_t at = m.find('@');
	return at == std::string::npos ? "" : m.substr(at + 1);
}


// Whether the mutant applies to this instance
static bool at(const std::string& here) {
	return instance().empty() or instance() == here;
}


// The kind of a printed line or record: nv, prime, pp, factor or residue
static std::string kind_of(const std::string& text) {
	if (text.find("no verdict") != std::string::npos or text.find("\"no-verdict\"") != std::string::npos)
		return "nv";
	if (text.find("probable prime") != std::string::npos or text.find("\"probable-prime\"") != std::string::npos)
		return "pp";
	if (text.find("res64") != std::string::npos)
		return "residue";
	if (text.find("factor") != std::string::npos)
		return "factor";
	if (text.find(": prime") != std::string::npos or text.find("\"prime\"") != std::string::npos)
		return "prime";
	return "other";
}


static void applied() {
	const char* path = std::getenv("GATE_MUTANT_LOG");
	if (not path)
		return;
	if (FILE* f = std::fopen(path, "a")) {
		std::fputs((mutant() + "\n").c_str(), f);
		std::fclose(f);
	}
}


// replace TEXT's first FROM with TO, reporting whether it did
static bool swap(std::string& text, const std::string& from, const std::string& to) {
	const size_t at = text.find(from);
	if (at == std::string::npos)
		return false;
	text.replace(at, from.size(), to);
	return true;
}


// cut TEXT from the first FROM up to and including the next UNTIL
static bool cut(std::string& text, const std::string& from, const std::string& until) {
	const size_t at = text.find(from);
	if (at == std::string::npos)
		return false;
	const size_t end = text.find(until, at + from.size());
	if (end == std::string::npos)
		return false;
	text.erase(at, end + until.size() - at);
	return true;
}


static std::string tail() {
	return " (" + APP_NAME + " " + APP_VERSION + ")";
}


// One verdict line, as the mutant breaks it
static std::string break_line(std::string line) {
	bool changed = false;
	if (not at(kind_of(line)))
		return line;
	if (is("l-noversion"))
		changed = swap(line, tail(), "");
	else if (is("l-version"))
		changed = swap(line, tail(), " (" + APP_NAME + " 9.9.9)");
	else if (is("l-noreason")) {
		const size_t at = line.find("no verdict, ");
		const size_t end = at == std::string::npos ? at : line.find(" (", at);
		if (end != std::string::npos) {
			line = line.substr(0, at) + "no verdict" + line.substr(end);
			changed = true;
		}
	}
	else if (is("l-exponent") and line.size() > 1 and line[0] == 'M') {
		const size_t sp = line.find(' ');
		line = "M" + std::to_string(std::stoul(line.substr(1, sp - 1)) + 1) + line.substr(sp);
		changed = true;
	} else if (is("l-word"))
		changed = swap(line, "no verdict", "composite");
	if (changed)
		applied();
	return line;
}


// One JSON record, as the mutant breaks it
static std::string break_record(std::string record) {
	bool changed = false;
	const std::string version = "\"record_version\": 1";
	if (not at(kind_of(record)))
		return record;
	if (is("j-invalid"))
		changed = not record.empty() and (record.pop_back(), true);
	else if (is("j-noversion"))
		changed = swap(record, ", " + version, "");
	else if (is("j-version2"))
		changed = swap(record, version, "\"record_version\": 2");
	else if (is("j-progversion"))
		changed = swap(record, "\"version\": \"" + APP_VERSION + "\"", "\"version\": \"1\"");
	else if (is("j-noprogram"))
		changed = cut(record, "\"program\": {", "}, ");
	else if (is("j-expstring")) {
		const size_t at = record.find("\"exponent\": ");
		const size_t end = record.find(',', at);
		if (at != std::string::npos and end != std::string::npos) {
			const size_t start = at + 12;
			record = record.substr(0, start) + "\"" + record.substr(start, end - start) + "\"" + record.substr(end);
			changed = true;
		}
	} else if (is("j-worktype"))
		changed = swap(record, "\"worktype\": \"tf\"", "\"worktype\": \"p1\"") or
		          swap(record, "\"worktype\": \"ptest\"", "\"worktype\": \"tf\"");
	else if (is("j-statusword"))
		changed = swap(record, "\"no-verdict\"", "\"no_verdict\"") or swap(record, "\"probable-prime\"", "\"probable_prime\"");
	else if (is("j-noreason"))
		changed = cut(record, ", \"reason\": \"", "\"");
	if (changed)
		applied();
	return record;
}


static bool holds(const std::vector<Result>& run, Outcome outcome) {
	for (const Result& result: run)
		if (result.outcome() == outcome)
			return true;
	return false;
}


// A run's status, as the mutant breaks it
static int break_status(const std::vector<Result>& run, int status) {
	int broken = status;
	if (is("s-nv-0") and status == NO_VERDICT)
		broken = OK;
	else if (is("s-nv-50") and status == NO_VERDICT)
		broken = 50;
	else if (is("s-comp-42") and status == COMPOSITE)
		broken = NO_VERDICT;
	else if (is("s-prime-41") and status == OK and holds(run, OUTCOME_PRIME))
		broken = PROBABLE_PRIME;
	else if (is("s-pp-0") and status == PROBABLE_PRIME)
		broken = OK;
	else if (is("s-conflict") and status == ERR_UNKNOWN)
		broken = COMPOSITE;
	else if (is("s-error-0") and status == ERR_RUN)
		broken = OK;
	if (broken != status)
		applied();
	return broken;
}


bool is_decisive(const Result& result) {
	const bool decisive = gate_real_is_decisive(result);
	if ((is("d-prime-not") and result.outcome() == OUTCOME_PRIME and decisive) or
	    (is("d-pp-not") and result.outcome() == OUTCOME_PROBABLE_PRIME and decisive) or
	    (is("d-nv-yes") and result.outcome() == OUTCOME_NO_VERDICT and not decisive)) {
		applied();
		return not decisive;
	}
	return decisive;
}


std::string verdict_line(const Result& result) {
	return break_line(gate_real_verdict_line(result));
}


std::string json_record(const Result& result) {
	return break_record(gate_real_json_record(result));
}


int status_of(const std::vector<Result>& results) {
	return break_status(results, gate_real_status_of(results));
}


// Run FN with std::cerr discarded
template <typename Fn>
static void quietly(Fn fn) {
	std::ostringstream sink;
	std::streambuf* saved = std::cerr.rdbuf(sink.rdbuf());
	fn();
	std::cerr.rdbuf(saved);
}


void report_result(std::vector<Result>& run, const Result& result, bool json, std::ostream& out) {
	if (is("r-json-ignored") and json) {
		json = false;
		applied();
	}
	if (is("r-quiet-error") and result.outcome() == OUTCOME_ERROR) {
		applied();
		quietly([&]() { gate_real_report_result(run, result, json, out); });
	} else {
		gate_real_report_result(run, result, json, out);
	}
	if (is("r-error-prints") and result.outcome() == OUTCOME_ERROR) {
		out << gate_real_verdict_line(result) << "\n";
		applied();
	}
	if (is("r-hold-off") and gate_real_is_decisive(result)) {
		out << (json ? gate_real_json_record(result) : gate_real_verdict_line(result)) << "\n";
		applied();
	}
}


int report_run(const std::vector<Result>& run, bool json, std::ostream& out) {
	if (is("r-json-ignored") and json) {
		json = false;
		applied();
	}
	int status = 0;
	if (is("r-quiet") and gate_real_status_of(run) == ERR_UNKNOWN) {
		applied();
		quietly([&]() { status = gate_real_report_run(run, json, out); });
	} else {
		status = gate_real_report_run(run, json, out);
	}
	if (is("r-conflict-prints") and status == ERR_UNKNOWN and at(json ? "json" : "plain")) {
		for (const Result& result: run)
			if (gate_real_is_decisive(result))
				out << (json ? gate_real_json_record(result) : gate_real_verdict_line(result)) << "\n";
		applied();
	}
	if (is("s-json") and json and status == COMPOSITE) {
		applied();
		return NO_VERDICT;
	}
	return status;
}


static Result break_test(Result result, Worktype worktype, const long unsigned p) {
	if (is("t-ll-prime") and worktype == WORK_LL) {
		applied();
		return Result::prime(WORK_LL, p);
	}
	if (is("t-tf-composite") and worktype == WORK_TF) {
		applied();
		return Result::composite_by_factor(WORK_TF, p, "3");
	}
	if (is("t-noreason") and worktype == WORK_P1) {
		applied();
		return Result::no_verdict(WORK_P1, p, "");
	}
	return result;
}


Result test_quick_factor(const long unsigned p) {
	return break_test(gate_real_test_quick_factor(p), WORK_TF, p);
}


Result test_p_minus_one(const long unsigned p) {
	return break_test(gate_real_test_p_minus_one(p), WORK_P1, p);
}


Result test_lucas_lehmer(const long unsigned p) {
	return break_test(gate_real_test_lucas_lehmer(p), WORK_LL, p);
}


Result run_in_order(const long unsigned p, const std::vector<TestFunction>& tests, const ResultSink& completed) {
	if (is("a-nostop") and tests.size() > 1) {
		std::optional<Result> last;
		bool decided = false;
		for (const TestFunction test: tests) {
			if (decided)
				applied();
			last = test(p);
			completed(*last);
			decided = decided or is_decisive(*last);
		}
		return *last;
	}
	if (is("a-stopfirst") and tests.size() > 1) {
		applied();
		Result result = tests[0](p);
		completed(result);
		return result;
	}
	if (is("a-batch") and tests.size() > 1) {
		std::vector<Result> held;
		Result last = gate_real_run_in_order(p, tests, [&](const Result& result) { held.push_back(result); });
		for (const Result& result: held)
			completed(result);
		applied();
		return last;
	}
	return gate_real_run_in_order(p, tests, completed);
}


Result test_all(const long unsigned p, const ResultSink& completed) {
	if (is("a-order")) {
		std::vector<Result> held;
		Result last = gate_real_test_all(p, [&](const Result& result) { held.push_back(result); });
		if (held.size() > 1) {
			std::swap(held[0], held[1]);
			applied();
		}
		for (const Result& result: held)
			completed(result);
		return last;
	}
	return gate_real_test_all(p, completed);
}


#if GATE_PHASE >= 2

static long unsigned largest_prime_factor(long unsigned p) {
	long unsigned largest = 1;
	for (long unsigned d = 2; d <= p / d; d++)
		while (p % d == 0) {
			largest = d;
			p /= d;
		}
	return p > 1 ? p : largest;
}


static long unsigned next_prime(long unsigned a) {
	for (long unsigned n = a + 1;; n++) {
		bool prime = true;
		for (long unsigned d = 2; d <= n / d; d++)
			if (n % d == 0) {
				prime = false;
				break;
			}
		if (prime)
			return n;
	}
}


static bool prime_square(const long unsigned p) {
	for (long unsigned d = 2; d <= p / d; d++)
		if (d * d == p)
			return gate_real_smallest_prime_factor(d) == d;
	return false;
}


long unsigned smallest_prime_factor(const long unsigned p) {
	if (is("x-square") and prime_square(p)) {
		applied();
		return p;
	}
	if (is("g-liar") and p == 33) {
		applied();
		return 5;
	}
	return gate_real_smallest_prime_factor(p);
}


Result composite_by_exponent(const long unsigned p, const long unsigned a) {
	Result result = gate_real_composite_by_exponent(p, a);
	if (is("x-guard") and result.outcome() == OUTCOME_ERROR) {
		applied();
		return Result::composite_by_factor(WORK_PTEST, p, "2^" + std::to_string(a) + "-1");
	}
	return result;
}


std::optional<Result> check_exponent(const long unsigned p) {
	std::optional<Result> result = gate_real_check_exponent(p);
	const long unsigned a = gate_real_smallest_prime_factor(p);
	if (is("x-next") and result) {
		applied();
		return Result::composite_by_factor(WORK_PTEST, p, "2^" + std::to_string(next_prime(a)) + "-1");
	}
	if (is("x-largest") and result and largest_prime_factor(p) != a) {
		applied();
		return Result::composite_by_factor(WORK_PTEST, p, "2^" + std::to_string(largest_prime_factor(p)) + "-1");
	}
	if (is("x-none") and result and at(std::to_string(p))) {
		applied();
		return std::nullopt;
	}
	if (is("x-prime-composite") and not result and p >= 2 and at(std::to_string(p))) {
		applied();
		return Result::composite_by_factor(WORK_PTEST, p, "2^" + std::to_string(p) + "-1");
	}
	return result;
}

#endif


// The -t value as spelled (-t NAME, -tNAME, -t=NAME, --test NAME, --test=NAME), or "none"
static std::string spelled_test(const int argc, const char** argv) {
	for (int i = 1; i < argc; i++) {
		const std::string w = argv[i];
		if ((w == "-t" or w == "--test") and i + 1 < argc)
			return argv[i + 1];
		if (w.rfind("--test=", 0) == 0)
			return w.substr(7);
		if (w.rfind("-t", 0) == 0 and w.size() > 2)
			return w.substr(w[2] == '=' ? 3 : 2);
	}
	return "none";
}


// Whether -j came inside a group of short flags (-jd, -sj)
static bool grouped_j(const int argc, const char** argv) {
	for (int i = 1; i < argc; i++) {
		const std::string w = argv[i];
		if (w.size() > 2 and w[0] == '-' and w[1] != '-' and w.find('j') != std::string::npos)
			return true;
	}
	return false;
}


namespace cli {

	Interface parse_args(const int argc, const char** argv) {
		Interface opt = gate_real_parse_args(argc, argv);
		if (is("x-skip-name") and not opt.skip_ptest and not opt.show_primes and spelled_test(argc, argv) == instance()) {
			opt.skip_ptest = true;
			applied();
		} else if (is("x-skip-ignored") and opt.skip_ptest) {
			opt.skip_ptest = false;
			applied();
		} else if (is("x-skip-always") and not opt.skip_ptest and not opt.show_primes) {
			opt.skip_ptest = true;
			applied();
		}
#if GATE_PHASE >= 3
		if (is("j-group-ignored") and opt.json_mode and grouped_j(argc, argv)) {
			opt.json_mode = false;
			applied();
		} else if (is("j-flag-ignored") and opt.json_mode) {
			opt.json_mode = false;
			applied();
		} else if (is("j-flag-always") and not opt.json_mode and not opt.show_primes) {
			opt.json_mode = true;
			applied();
		}
#endif
		return opt;
	}

}
