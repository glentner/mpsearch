// Gate mutant interposer: a cycle record, never part of mps.
//
// The gates copy the tree, prepend `#define parse_args gate_real_parse_args` to src/cli.cpp and
// `#define prime_sieve gate_real_prime_sieve` / `#define display_primes gate_real_display_primes`
// to src/math.cpp, and build this file as one more module. Both bin/mps and bin/check then reach
// the parser and the sieve through the wrappers below. With GATE_MUTANT unset the wrappers pass
// everything through unchanged; with GATE_MUTANT=<id> they inject one defect, and append <id> to
// the file GATE_MUTANT_LOG each time the defect is actually injected. A gate requires `make check`
// to go red under every mutant it names, and the log to show the mutant was exercised. The help
// text's mutants are edits to src/main.hpp, made by the gate itself (common.sh, help_mutant).


// Standard libs
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Internal libs
#include "cli.hpp"
#include "math.hpp"


namespace cli {
	Interface gate_real_parse_args(const int argc, const char** argv);
}
std::vector<bool> gate_real_prime_sieve(size_t n);
void gate_real_display_primes(size_t n);


static std::string mutant() {
	const char* m = std::getenv("GATE_MUTANT");
	return m ? m : "";
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


// A valued option's value: the word that holds it and where in that word it starts.
struct Value {
	char option = 0;     // 't' or 'f'
	int spelling = 0;    // 1 -x V, 2 -xV, 3 -x=V, 4 --long V, 5 --long=V, 6 inside a group
	size_t word = 0;
	size_t offset = 0;
};


struct Scan {
	std::vector<Value> values;
	std::vector<size_t> positional;
	bool primes = false;
	bool info = false;    // -h, -v, --help or --version
};


static Scan scan(const std::vector<std::string>& args) {
	Scan s;
	bool only = false;
	for (size_t i = 1; i < args.size(); i++) {
		const std::string& w = args[i];
		if (only) {
			s.positional.push_back(i);
		} else if (w == "--") {
			only = true;
		} else if (w.rfind("--", 0) == 0) {
			std::string name = w.substr(0, w.find('='));
			bool eq = w.find('=') != std::string::npos;
			if (name == "--test" or name == "--file") {
				char o = name == "--test" ? 't' : 'f';
				if (eq)
					s.values.push_back({o, 5, i, name.size() + 1});
				else if (i + 1 < args.size()) {
					s.values.push_back({o, 4, i + 1, 0});
					i++;
				}
			} else if (name == "--primes") {
				s.primes = true;
			} else if (name == "--help" or name == "--version") {
				s.info = true;
			}
		} else if (w.size() > 1 and w[0] == '-') {
			for (size_t k = 1; k < w.size(); k++) {
				char c = w[k];
				if (c == 't' or c == 'f') {
					if (k + 1 == w.size()) {
						if (i + 1 < args.size()) {
							s.values.push_back({c, k == 1 ? 1 : 6, i + 1, 0});
							i++;
						}
					} else if (w[k + 1] == '=') {
						s.values.push_back({c, k == 1 ? 3 : 6, i, k + 2});
					} else {
						s.values.push_back({c, k == 1 ? 2 : 6, i, k + 1});
					}
					break;
				}
				if (c == 'p')
					s.primes = true;
				if (c == 'h' or c == 'v')
					s.info = true;
			}
		} else {
			s.positional.push_back(i);
		}
	}
	return s;
}


static std::string value_of(const std::vector<std::string>& args, const Value& v) {
	return args[v.word].substr(v.offset);
}


static void set_value(std::vector<std::string>& args, const Value& v, const std::string& to) {
	args[v.word] = args[v.word].substr(0, v.offset) + to;
	applied();
}


static bool all_digits(const std::string& s) {
	if (s.empty())
		return false;
	for (char c: s)
		if (c < '0' or c > '9')
			return false;
	return true;
}


// The value of a digit string, saturating above the bound; -1 for anything else.
static long long digits_value(const std::string& s) {
	if (not all_digits(s))
		return -1;
	long long v = 0;
	for (char c: s) {
		v = v * 10 + (c - '0');
		if (v > 10'000'000'000LL)
			return v;
	}
	return v;
}


// A digit string whose value is 2^64 or more, leading zeros aside
static bool wraps(const std::string& s) {
	if (not all_digits(s))
		return false;
	std::string d = s.substr(std::min(s.find_first_not_of('0'), s.size()));
	return d.size() > 20 or (d.size() == 20 and d >= "18446744073709551616");
}


// Another valid -t name, so a corrupted spelling still parses and only its value is wrong
static std::string other_name(const std::string& v) {
	static const std::vector<std::pair<std::string, std::string>> swap = {
		{"tf", "ll"}, {"trial-factor", "p1"}, {"p1", "tf"}, {"p-minus-1", "ll"},
		{"ll", "tf"}, {"lucas-lehmer", "p1"}, {"all", "p1"},
	};
	for (const auto& [from, to]: swap)
		if (v == from)
			return to;
	return v + "x";
}


static bool is_group(const std::string& w) {
	if (w.size() < 3 or w[0] != '-' or w[1] == '-')
		return false;
	for (size_t k = 1; k < w.size(); k++)
		if (std::string("hvpsdD").find(w[k]) == std::string::npos)
			return false;
	return true;
}


static void rewrite_args(std::vector<std::string>& args, const std::string& m) {

	Scan s = scan(args);

	// R2, R3: the -t value, in any of the five spellings
	struct Rename { const char* id; const char* from; const char* to; };
	static const Rename renames[] = {
		{"n-tf", "tf", "all"}, {"n-trial-factor", "trial-factor", "all"},
		{"n-p1", "p1", "all"}, {"n-p-minus-1", "p-minus-1", "all"},
		{"n-ll", "ll", "all"}, {"n-lucas-lehmer", "lucas-lehmer", "all"},
		{"n-all", "all", "ll"},
		{"r-qf", "qf", "tf"}, {"r-quick", "quick", "tf"}, {"r-factor", "factor", "tf"},
		{"r-pm", "pm", "p1"}, {"r-p-min", "p-min", "p1"}, {"r-none", "none", "all"},
		{"r-empty", "", "all"},
	};
	for (const Rename& r: renames)
		if (m == r.id)
			for (const Value& v: s.values)
				if (v.option == 't' and value_of(args, v) == r.from)
					set_value(args, v, r.to);

	if (m == "r-unknown") {
		static const std::vector<std::string> known = {"tf", "trial-factor", "p1", "p-minus-1", "ll",
			"lucas-lehmer", "all", "qf", "quick", "factor", "pm", "p-min", "none", ""};
		for (const Value& v: s.values) {
			bool k = false;
			for (const std::string& n: known)
				k = k or value_of(args, v) == n;
			if (v.option == 't' and not k)
				set_value(args, v, "all");
		}
	}

	if (m == "n-absent") {
		bool has_t = false;
		for (const Value& v: s.values)
			has_t = has_t or v.option == 't';
		bool numeric = s.positional.size() == 1 and digits_value(args[s.positional[0]]) >= 2
		               and digits_value(args[s.positional[0]]) <= 999'999'999;
		if (not has_t and not s.primes and not s.info and numeric) {
			args.insert(args.begin() + 1, "-tll");
			applied();
		}
	}

	// R4: one spelling's value corrupted, for each option and spelling
	for (char o: {'t', 'f'})
		for (int sp = 1; sp <= 5; sp++)
			if (m == std::string("s-") + o + std::to_string(sp))
				for (const Value& v: s.values)
					if (v.option == o and v.spelling == sp)
						set_value(args, v, o == 't' ? other_name(value_of(args, v)) : value_of(args, v) + "x");

	if (m == "s-file-as-test")
		for (size_t i = 1; i < args.size(); i++)
			if (args[i] == "--file") {
				args[i] = "--test";
				applied();
			}

	// R5: grouped boolean flags
	for (size_t i = 1; i < args.size(); i++) {
		std::string& w = args[i];
		if (m == "g-first" and is_group(w)) {
			w = w.substr(0, 2);
			applied();
		} else if (m == "g-reverse" and is_group(w)) {
			std::string r = "-" + std::string(w.rbegin(), w.rend() - 1);
			if (r != w) {
				w = r;
				applied();
			}
		} else if (m == "g-drop-info" and w.size() > 2 and w[0] == '-' and (w[1] == 'h' or w[1] == 'v')) {
			w = "-" + w.substr(2);
			applied();
		} else if (m == "g-drop-bad" and w.size() > 2 and w[0] == '-' and w[1] != '-') {
			std::string kept = "-";
			bool dropped = false;
			for (size_t k = 1; k < w.size(); k++) {
				char c = w[k];
				if (c == 'h' or c == 'v' or c == 't' or c == 'f') {
					kept += w.substr(k);
					break;
				}
				if (std::string("psdD").find(c) == std::string::npos)
					dropped = true;
				else
					kept += c;
			}
			if (dropped and kept.size() > 1) {
				w = kept;
				applied();
			}
		}
	}

	// R6, R7: PVAL
	for (size_t i: s.positional) {
		std::string& w = args[i];
		long long d = digits_value(w);
		if (m == "v-junk" and not s.primes and not all_digits(w)) {
			w = "31";
			applied();
		} else if (m == "v-small" and not s.primes and d >= 0 and d < 2) {
			w = "31";
			applied();
		} else if (m == "v-big" and not s.primes and d > 999'999'999) {
			w = "999999999";
			applied();
		} else if (m == "v-zeros" and not s.primes and w.size() > 1 and w[0] == '0' and all_digits(w)) {
			w = "abc";
			applied();
		} else if (m == "v-wrap" and not s.primes and wraps(w)) {
			w = "31";
			applied();
		} else if (m == "pv-wrap" and s.primes and wraps(w)) {
			w = "30";
			applied();
		} else if (m == "pv-junk" and s.primes and not all_digits(w)) {
			w = "30";
			applied();
		} else if (m == "pv-big" and s.primes and d > 999'999'999) {
			w = "30";
			applied();
		} else if (m == "pv-zero" and s.primes and d == 0) {
			w = "abc";
			applied();
		}
	}
}


static std::string replace_all(std::string text, const std::string& from, const std::string& to) {
	for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
		text.replace(at, from.size(), to);
	return text;
}


namespace cli {

	Interface parse_args(const int argc, const char** argv) {

		const std::string m = mutant();
		std::vector<std::string> args(argv, argv + argc);
		if (not m.empty())
			rewrite_args(args, m);
		std::vector<const char*> v;
		for (const std::string& a: args)
			v.push_back(a.c_str());
		const bool primes = scan(args).primes;

		try {
			return gate_real_parse_args(static_cast<int>(v.size()), v.data());

		} catch (argument_error const& error) {
			std::string text = error.what();
			const Scan after = scan(args);
			if (text.find("PVAL '") != std::string::npos and after.positional.size() == 1) {
				if (m == "v-name" or (m == "pv-name" and primes)) {
					applied();
					throw argument_error(replace_all(text, "PVAL", "argument"));
				}
				if (m == "v-run" and not primes) {
					applied();
					std::cerr << "    info [mps] Running quick factor test" << std::endl;
					throw;
				}
				if (m == "v-uncaught") {
					applied();
					throw std::invalid_argument("stoi: no conversion");
				}
				if (m == "pv-stdout" and primes) {
					applied();
					std::cout << "2" << std::endl;
					throw;
				}
			}
			if (m == "r-message") {
				for (const Value& val: scan(args).values) {
					std::string quoted = "'" + value_of(args, val) + "'";
					if (val.option == 't' and text.find(quoted) != std::string::npos) {
						applied();
						throw argument_error(replace_all(text, quoted, "the value"));
					}
				}
			}
			throw;
		}
	}

}


std::vector<bool> prime_sieve(size_t n) {

	std::vector<bool> is_prime = gate_real_prime_sieve(n);
	const std::string m = mutant();

	// R9: the sieve's entries, each small n on its own
	if (m == "q-size0" and n == 0) {
		is_prime.resize(1, false);
		applied();
	} else if (m == "q-size1" and n == 1) {
		is_prime.resize(2, false);
		applied();
	} else if (m == "q-size2" and n == 2) {
		is_prime.resize(3, false);
		applied();
	} else if (m == "q-zero1" and n == 1) {
		is_prime[0] = true;
		applied();
	} else if (m == "q-one2" and n == 2) {
		is_prime[1] = true;
		applied();
	} else if (m == "q-one3" and n == 3) {
		is_prime[1] = true;
		applied();
	} else if (m == "q-three" and n == 3) {
		is_prime[2] = false;
		applied();
	} else if (m == "q-one-large" and n > 3) {
		is_prime[1] = true;
		applied();
	} else if (m == "q-zero-large" and n > 3) {
		is_prime[0] = true;
		applied();
	} else if (m == "q-square" and n >= 10) {
		for (size_t p = 3; p * p < n; p += 2)
			if (p * p == n - 1) {
				is_prime[p * p] = true;
				applied();
			}
	} else if (m == "q-mid" and n > 91) {
		is_prime[91] = true;
		applied();
	} else if ((m == "q-stray1" and n == 1) or (m == "q-stray2" and n == 2)) {
		// The old defect: a write at index 2 whatever n is. For n = 1 and 2 the bit lies inside
		// the vector's storage, so only the library's bounds assertions can see it. Logged first,
		// since the assertion stops the program at the write.
		applied();
		is_prime[2] = true;
	}
	return is_prime;
}


void display_primes(size_t n) {

	const std::string m = mutant();
	if (m != "d-no2" and m != "d-inclusive" and m != "d-extra" and m != "d-descending") {
		gate_real_display_primes(n);
		return;
	}

	// R8: the listing, rebuilt from the real sieve with one defect
	std::vector<bool> is_prime = gate_real_prime_sieve(n + 1);
	std::vector<size_t> listed;
	for (size_t i = 0; i < n; i++)
		if (is_prime[i] and not (m == "d-no2" and i == 2))
			listed.push_back(i);
	if (m == "d-inclusive" and is_prime[n])
		listed.push_back(n);
	if (m == "d-descending")
		listed.assign(listed.rbegin(), listed.rend());
	for (size_t p: listed)
		std::cout << p << std::endl;
	if (m == "d-extra")
		std::cout << std::endl;

	bool changed = (m == "d-no2" and n > 2) or (m == "d-inclusive" and is_prime[n]) or m == "d-extra"
	               or (m == "d-descending" and listed.size() > 1);
	if (changed)
		applied();
}
