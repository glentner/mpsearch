// The gates' probe of the loader (a cycle record, not a standing gate; PLAN § 6, step 6). Built
// against the tree's src/known.cpp; reads cases from stdin, each "case <id> <path> <length>", a
// newline, that many bytes of a table and a newline; prints known::TABLES first ("tables <path>..."),
// then for each case "<id> refused <message>", or "<id> ok" and one line per source and per row in
// the form tables.py's describe() gives.

#include <cstdio>
#include <iostream>
#include <string>

#include "known.hpp"


static const char* origin_name(known::Origin origin) {
	switch (origin) {
	case known::Origin::PUBLISHED:
		return "published";
	case known::Origin::COMPUTED:
		return "computed";
	default:
		return "runs";
	}
}


int main() {
	std::string word, id, path;
	std::size_t length;
	std::cout << "tables";
	for (const std::string& table: known::TABLES)
		std::cout << " " << table;
	std::cout << "\n";
	while (std::cin >> word >> id >> path >> length) {
		std::cin.get();
		std::string text(length, '\0');
		std::cin.read(&text[0], std::streamsize(length));
		std::cin.get();
		try {
			known::Table table = known::parse(path, text);
			std::cout << id << " ok\n";
			for (const known::Source& s: table.sources)
				std::cout << id << " s " << s.line << " " << origin_name(s.origin) << " "
				          << (s.reference.empty() ? "-" : s.reference) << " " << (s.retrieved.empty() ? "-" : s.retrieved) << "\n";
			for (const known::Row& r: table.rows) {
				std::string runs;
				for (const known::Run& run: r.runs)
					runs += (runs.empty() ? "" : ";") + run.engine + ":" + std::to_string(run.length) + ":" + std::to_string(run.shift);
				std::cout << id << " r " << r.line << " " << r.op << " " << r.exponent << " " << r.start << " " << r.iterations << " "
				          << (r.res64.empty() ? "-" : r.res64) << " " << r.agreed << " " << (runs.empty() ? "-" : runs) << " "
				          << (r.supersedes.empty() ? "-" : r.supersedes) << " " << r.source << " " << (r.superseded ? 1 : 0) << "\n";
			}
		} catch (known_error const& error) {
			std::cout << id << " refused " << error.what() << "\n";
		}
	}
	return 0;
}
