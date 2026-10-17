// The gates' wrappers (a cycle record, not a standing gate; PLAN § 6, step 7). Linked into a copy
// of the tree whose src/known.cpp and src/reference.cpp have their public definitions renamed to
// gate_real_<name>. Under GATE_MUTANT=<id>, known::parse rewrites a table's text before the real
// loader reads it, as a loader that skips one rule would read it, and appends a line to
// GATE_MUTANT_LOG when it changed the text. With GATE_TRACE set, the reference tests append
// "ll <p>" and "prp <p>" there and answer as the real ones do. Without GATE_MUTANT nothing changes.

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "known.hpp"
#include "mp.hpp"
#include "reference.hpp"


namespace known {
	Table gate_real_parse(const std::string& path, const std::string& text);
}

namespace reference {
	BigInt gate_real_lucas_lehmer(long unsigned p);
	BigInt gate_real_prp3(long unsigned p);
}


static std::vector<std::string> gate_split(const std::string& text, char sep) {
	std::vector<std::string> parts;
	std::size_t start = 0;
	while (true) {
		std::size_t end = text.find(sep, start);
		parts.push_back(text.substr(start, end - start));
		if (end == std::string::npos)
			return parts;
		start = end + 1;
	}
}


static std::string gate_join(const std::vector<std::string>& lines) {
	std::string text;
	for (const std::string& line: lines)
		text += line + "\n";
	return text;
}


static bool gate_is_row(const std::vector<std::string>& lines, std::size_t i) {
	return i >= 2 and i + 1 < lines.size() and not lines[i].empty() and lines[i][0] != '#';
}


static std::string gate_stem(const std::vector<std::string>& lines) {
	if (lines.size() < 2)
		return "";
	for (const char* stem: {"mersenne-exponents", "final-residues", "partial-residues"})
		if (lines[1].find(std::string("generate.py ") + stem + " ") != std::string::npos)
			return stem;
	return "";
}


// The number of key fields after the op, and the index of the Res64, by table
static std::size_t gate_key_fields(const std::string& stem) {
	return stem == "partial-residues" ? 3 : 1;
}


static std::string gate_head(const std::string& row) {
	std::size_t because = row.find(" because ");
	return because == std::string::npos ? row : row.substr(0, because);
}


static std::string gate_key(const std::string& row, const std::string& stem) {
	std::vector<std::string> t = gate_split(gate_head(row), ' ');
	std::string key;
	for (std::size_t i = 0; i <= gate_key_fields(stem) and i < t.size(); i++)
		key += t[i] + " ";
	return key;
}


static std::string gate_res64(const std::string& row, const std::string& stem) {
	std::vector<std::string> t = gate_split(gate_head(row), ' ');
	std::size_t i = gate_key_fields(stem) + 1;
	return i < t.size() ? t[i] : "";
}


static bool gate_has(const std::string& row, const std::string& what) {
	return gate_head(row).find(what) != std::string::npos;
}


static void gate_recount(std::vector<std::string>& lines) {
	std::size_t rows = 0;
	for (std::size_t i = 2; i + 1 < lines.size(); i++)
		if (gate_is_row(lines, i))
			rows++;
	if (lines.size() >= 3 and lines.back().rfind("# end ", 0) == 0)
		lines.back() = "# end " + std::to_string(rows) + " rows";
}


static bool gate_prime(std::uint64_t n) {
	if (n < 2)
		return false;
	for (std::uint64_t d = 2; d * d <= n; d++)
		if (n % d == 0)
			return false;
	return true;
}


static std::string gate_rewrite(const std::string& mutant, const std::string& text) {
	//
	// The text a loader without one rule would, in effect, accept

	bool final_lf = not text.empty() and text.back() == '\n';
	std::vector<std::string> lines = gate_split(final_lf ? text.substr(0, text.size() - 1) : text, '\n');
	std::string stem = gate_stem(lines);
	if (lines.size() < 3 or stem.empty())
		return text;
	std::vector<std::string> out = lines;

	if (mutant == "source-default") {
		// a row above every source line takes a default source
		for (std::size_t i = 2; i + 1 < out.size(); i++) {
			if (out[i].rfind("# from ", 0) == 0)
				break;
			if (gate_is_row(out, i)) {
				out.insert(out.begin() + i, "# from computed: a default");
				break;
			}
		}
	} else if (mutant == "conflict-blind" or mutant == "fingerprint-blind") {
		// a later row of a key with another Res64, not a correction, is dropped
		std::map<std::string, std::string> first;
		bool under_runs = false;
		std::vector<std::string> kept;
		for (std::size_t i = 0; i < out.size(); i++) {
			if (out[i].rfind("# from ", 0) == 0)
				under_runs = out[i].rfind("# from runs:", 0) == 0;
			if (gate_is_row(out, i) and not gate_has(out[i], "supersedes=")) {
				std::string key = gate_key(out[i], stem), value = gate_res64(out[i], stem);
				if (first.count(key) and first[key] != value and (mutant == "conflict-blind" or under_runs))
					continue;
				first.emplace(key, value);
			}
			kept.push_back(out[i]);
		}
		out = kept;
		gate_recount(out);
	} else if (mutant == "supersede-blind") {
		// corrections read as plain rows
		for (std::size_t i = 2; i + 1 < out.size(); i++)
			if (gate_is_row(out, i) and gate_has(out[i], "supersedes=")) {
				std::string head = gate_head(out[i]);
				out[i] = head.substr(0, head.find(" supersedes="));
			}
	} else if (mutant == "one-engine") {
		// a fingerprint of fewer than two engines gains one
		for (std::size_t i = 2; i + 1 < out.size(); i++)
			if (gate_is_row(out, i) and gate_has(out[i], "run=")) {
				std::set<std::string> engines;
				for (const std::string& t: gate_split(gate_head(out[i]), ' '))
					if (t.rfind("run=", 0) == 0)
						engines.insert(t.substr(4, t.find(':') - 4));
				if (engines.size() < 2)
					out[i] = gate_head(out[i]) + " run=gate:1:0" + out[i].substr(gate_head(out[i]).size());
			}
	} else if (mutant == "runs-anywhere") {
		// runs on a row that is not a fingerprint are dropped
		bool under_runs = false;
		for (std::size_t i = 2; i + 1 < out.size(); i++) {
			if (out[i].rfind("# from ", 0) == 0)
				under_runs = out[i].rfind("# from runs:", 0) == 0;
			if (gate_is_row(out, i) and not under_runs and gate_has(out[i], "run=")) {
				std::string kept;
				for (const std::string& t: gate_split(out[i], ' '))
					if (t.rfind("run=", 0) != 0)
						kept += (kept.empty() ? "" : " ") + t;
				out[i] = kept;
			}
		}
	} else if (mutant == "res64-case") {
		// a Res64 in lowercase read as uppercase
		for (std::size_t i = 2; i + 1 < out.size(); i++)
			if (gate_is_row(out, i)) {
				std::string head = gate_head(out[i]);
				std::vector<std::string> t = gate_split(head, ' ');
				std::size_t r = gate_key_fields(stem) + 1;
				if (stem != "mersenne-exponents" and r < t.size() and t[r].size() == 16)
					for (char& c: t[r])
						if (c >= 'a' and c <= 'f')
							c = char(c - 'a' + 'A');
				std::string joined;
				for (const std::string& x: t)
					joined += (joined.empty() ? "" : " ") + x;
				out[i] = joined + out[i].substr(head.size());
			}
	} else if (mutant == "count-blind") {
		// the trailer's count taken from the rows
		gate_recount(out);
	} else if (mutant == "ascending-blind") {
		// exponents not above the one before are dropped
		if (stem == "mersenne-exponents") {
			std::vector<std::string> kept;
			std::uint64_t last = 0;
			for (std::size_t i = 0; i < out.size(); i++) {
				if (gate_is_row(out, i)) {
					std::vector<std::string> t = gate_split(out[i], ' ');
					std::uint64_t p = t.size() > 1 ? std::strtoull(t[1].c_str(), nullptr, 10) : 0;
					if (p != 0 and p <= last)
						continue;
					last = p;
				}
				kept.push_back(out[i]);
			}
			out = kept;
			gate_recount(out);
		}
	} else if (mutant == "prime-blind") {
		// a composite exponent read as the next prime
		for (std::size_t i = 2; i + 1 < out.size(); i++)
			if (gate_is_row(out, i)) {
				std::string head = gate_head(out[i]);
				std::vector<std::string> t = gate_split(head, ' ');
				if (t.size() > 1 and not t[1].empty() and t[1].find_first_not_of("0123456789") == std::string::npos
				    and t[1].size() < 10) {
					std::uint64_t p = std::strtoull(t[1].c_str(), nullptr, 10);
					if (p > 3 and not gate_prime(p)) {
						while (not gate_prime(p))
							p++;
						t[1] = std::to_string(p);
					}
				}
				std::string joined;
				for (const std::string& x: t)
					joined += (joined.empty() ? "" : " ") + x;
				out[i] = joined + out[i].substr(head.size());
			}
	} else if (mutant == "agreed-blind") {
		// agreed=1 read as agreed=2
		for (std::size_t i = 2; i + 1 < out.size(); i++) {
			std::size_t at = out[i].find(" agreed=1");
			if (gate_is_row(out, i) and at != std::string::npos and (at + 9 == out[i].size() or out[i][at + 9] == ' '))
				out[i].replace(at, 9, " agreed=2");
		}
	} else if (mutant == "origin-blind") {
		// a source of an unknown kind read as computed
		for (std::size_t i = 2; i + 1 < out.size(); i++)
			if (out[i].rfind("# from ", 0) == 0 and out[i].rfind("# from computed:", 0) != 0
			    and out[i].rfind("# from runs:", 0) != 0 and out[i].rfind("# from published ", 0) != 0) {
				std::size_t colon = out[i].find(": ");
				if (colon != std::string::npos)
					out[i] = "# from computed" + out[i].substr(colon);
			}
	} else if (mutant == "drop-9941") {
		// the exponent 9941 gone from the exponent table, for the trace of the reference tests' calls
		std::vector<std::string> kept;
		for (std::size_t i = 0; i < out.size(); i++)
			if (not (stem == "mersenne-exponents" and gate_is_row(out, i) and out[i] == "mersenne 9941"))
				kept.push_back(out[i]);
		out = kept;
		gate_recount(out);
	} else if (mutant == "weak-correction") {
		// a computed correction of a published final residue read as a published one
		std::string source;
		std::vector<std::string> kept;
		for (std::size_t i = 0; i < out.size(); i++) {
			if (out[i].rfind("# from ", 0) == 0)
				source = out[i];
			if (stem == "final-residues" and gate_is_row(out, i) and gate_has(out[i], "supersedes=")
			    and source.rfind("# from computed:", 0) == 0) {
				std::string head = gate_head(out[i]);
				std::size_t at = head.find(" supersedes=");
				kept.push_back("# from published GateKey retrieved 2026-10-17: a gate's rewrite");
				kept.push_back(head.substr(0, at) + " agreed=2" + out[i].substr(at));
				kept.push_back(source);
				continue;
			}
			kept.push_back(out[i]);
		}
		out = kept;
		gate_recount(out);
	} else if (mutant == "runs-supersede") {
		// a fingerprint's correction honoured: the row it names dropped, the clause with it
		std::string source;
		for (std::size_t i = 0; i < out.size(); i++) {
			if (out[i].rfind("# from ", 0) == 0)
				source = out[i];
			if (not (gate_is_row(out, i) and gate_has(out[i], "supersedes=") and source.rfind("# from runs:", 0) == 0))
				continue;
			std::string head = gate_head(out[i]);
			std::string named = head.substr(head.find(" supersedes=") + 12);
			named = named.substr(0, named.find(' '));
			for (std::size_t j = 2; j < i; j++)
				if (gate_is_row(out, j) and gate_key(out[j], stem) == gate_key(out[i], stem) and gate_res64(out[j], stem) == named) {
					out.erase(out.begin() + j);
					i--;
					break;
				}
			head = gate_head(out[i]);
			out[i] = head.substr(0, head.find(" supersedes="));
		}
		gate_recount(out);
	} else if (mutant == "final-in-partial") {
		// a final Lucas-Lehmer residue in the partial table dropped, not refused
		std::vector<std::string> kept;
		for (std::size_t i = 0; i < out.size(); i++) {
			if (stem == "partial-residues" and gate_is_row(out, i)) {
				std::vector<std::string> t = gate_split(gate_head(out[i]), ' ');
				if (t.size() > 3 and t[0] == "ll" and t[2] == "4"
				    and std::strtoull(t[3].c_str(), nullptr, 10) + 2 == std::strtoull(t[1].c_str(), nullptr, 10))
					continue;
			}
			kept.push_back(out[i]);
		}
		out = kept;
		gate_recount(out);
	} else if (mutant == "tail-blind") {
		// lines after the first trailer are dropped
		for (std::size_t i = 2; i + 1 < out.size(); i++)
			if (out[i].rfind("# end ", 0) == 0) {
				out.resize(i + 1);
				break;
			}
	}

	std::string rewritten = gate_join(out);
	if (not final_lf)
		rewritten.pop_back();
	return rewritten;
}


known::Table known::parse(const std::string& path, const std::string& text) {
	const char* mutant = std::getenv("GATE_MUTANT");
	if (mutant == nullptr or *mutant == '\0')
		return gate_real_parse(path, text);
	std::string rewritten = gate_rewrite(mutant, text);
	if (rewritten != text) {
		const char* log = std::getenv("GATE_MUTANT_LOG");
		if (log != nullptr)
			std::ofstream(log, std::ios::app) << mutant << " " << path << "\n";
	}
	return gate_real_parse(path, rewritten);
}


static void gate_trace(const char* op, long unsigned p) {
	const char* trace = std::getenv("GATE_TRACE");
	if (trace != nullptr)
		std::ofstream(trace, std::ios::app) << op << " " << p << "\n";
}


BigInt reference::lucas_lehmer(long unsigned p) {
	gate_trace("ll", p);
	return gate_real_lucas_lehmer(p);
}


BigInt reference::prp3(long unsigned p) {
	gate_trace("prp", p);
	return gate_real_prp3(p);
}
