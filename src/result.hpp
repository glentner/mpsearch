// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Results
//
// What a test or the exponent check found, and its three views: the verdict line, the JSON
// record and the exit status. The printer streams each no-verdict line as its test completes
// and holds every verdict until the run is known to hold no error and no contradiction.
//

#pragma once


// Standard libs
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>


enum Outcome {
	OUTCOME_PRIME,
	OUTCOME_PROBABLE_PRIME,
	OUTCOME_COMPOSITE,
	OUTCOME_NO_VERDICT,
	OUTCOME_ERROR,
};


// What produced a result: one of the tests, or the exponent check
enum Worktype {
	WORK_TF,
	WORK_P1,
	WORK_LL,
	WORK_PTEST,
};


static std::map<Worktype, std::string>
name_by_worktype {
	{WORK_TF,     "tf"},
	{WORK_P1,     "p1"},
	{WORK_LL,     "ll"},
	{WORK_PTEST,  "ptest"},
};


// The JSON record's own format version, bumped with any change to the record
static const int RECORD_VERSION = 1;


class Result {
public:

	static Result prime(Worktype worktype, long unsigned p);
	static Result probable_prime(Worktype worktype, long unsigned p);
	static Result composite_by_factor(Worktype worktype, long unsigned p, const std::string& factor);
	static Result composite_by_residue(Worktype worktype, long unsigned p, std::uint64_t res64);
	static Result no_verdict(Worktype worktype, long unsigned p, const std::string& reason);
	static Result error(Worktype worktype, long unsigned p, const std::string& reason);

	Outcome outcome() const { return outcome_; }
	Worktype worktype() const { return worktype_; }
	long unsigned exponent() const { return exponent_; }
	const std::vector<std::string>& factors() const { return factors_; }
	std::optional<std::uint64_t> res64() const { return res64_; }
	const std::string& reason() const { return reason_; }

private:

	// Only the named constructors build a result, so each holds exactly one outcome and what
	// that outcome must carry
	Result(Outcome outcome, Worktype worktype, long unsigned p);

	Outcome outcome_;
	Worktype worktype_;
	long unsigned exponent_;
	std::vector<std::string> factors_ {};
	std::optional<std::uint64_t> res64_ {};
	std::string reason_ {};
};


bool is_decisive(const Result& result);
std::string verdict_line(const Result& result);
std::string json_record(const Result& result);
int status_of(const std::vector<Result>& results);
void report_result(std::vector<Result>& run, const Result& result, bool json, std::ostream& out);
int report_run(const std::vector<Result>& run, bool json, std::ostream& out);
