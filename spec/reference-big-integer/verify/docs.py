# The note and AGENTS.md: a cycle record, never part of mps. Python 3.8+, standard library only.
#
#     python3 -I -B docs.py PHASE ROOT
#
# Checks doc/reference-arithmetic.md, doc/README.md, doc/references.md and, from P4, the "What mps
# uses" sections of doc/lucas-lehmer.md and doc/prp-and-gerbicz.md, with the landed checker of the
# math-notes cycle (spec/math-notes-and-references/verify/notes.py, imported, never edited): the five
# sections, citations that resolve and are listed under Sources, the clean-room rules, and every
# number in a check or cited span whose check evaluates true (R12). Then the headings and names PLAN
# § 2 gives each phase, and AGENTS.md's lines (§ Invariants 3). `GATE FAIL: <reason>` and exit 1, or
# one summary line.

import datetime
import os
import re
import subprocess
import sys

PHASE = int(sys.argv[1])
ROOT = sys.argv[2]
sys.path.insert(0, os.path.join(ROOT, "spec", "math-notes-and-references", "verify"))
import notes  # noqa: E402

NOTE = "doc/reference-arithmetic.md"
USES = {
	1: ["src/mp.cpp", "reduce_mersenne", "res64", "hex64", "to_hex", "from_hex"],
	2: ["multiply_schoolbook", "multiply_karatsuba", "square_schoolbook", "square_karatsuba", "KARATSUBA_THRESHOLD"],
	3: ["divide", "reduce", "power_mod", "to_decimal", "from_decimal"],
	4: ["src/reference.cpp", "reference::lucas_lehmer", "reference::prp3"],
}
HEADINGS = {
	1: ["Folding modulo $M_p$", "Res64"],
	2: ["Schoolbook", "Karatsuba"],
	3: ["Division", "Powers", "Radix conversion"],
	4: [],
}
CITES = {1: ["BrentZimmermannMCA2010"], 2: ["KaratsubaOfman1962"], 3: [], 4: []}
OLD_NOTES = ["mersenne-numbers.md", "factors-and-trial-factoring.md", "p-1.md", "weighted-transforms.md",
             "number-theoretic-transforms.md"]
LIST_LINES = {
	1: ["- `tests/vectors/` (the generator and every vector file: expected values)"],
	4: ["- `src/reference.hpp`, `src/reference.cpp`",
	    "- `tests/vectors/mersenne-exponents.txt` (the Mersenne prime exponents)"],
}


def fail(msg):
	print("GATE FAIL: %s" % msg, file=sys.stderr)
	sys.exit(1)


def read(rel):
	return open(os.path.join(ROOT, rel), encoding="utf-8").read()


def base_text(rel):
	return subprocess.run(["git", "-C", ROOT, "show", "c9b82ff:" + rel], capture_output=True, text=True, check=True).stdout


def section(text, head):
	m = re.search(r"^## %s\n(.*?)(?=^## |\Z)" % re.escape(head), text, re.M | re.S)
	return m.group(1) if m else ""


def main():
	today = datetime.date.today().isoformat()
	try:
		entries = notes.parse_references(today)
		results = []
		uses = [n for p in range(1, PHASE + 1) for n in USES[p]]
		cites = notes.check_note(NOTE, uses, [], entries, results)
		text = read(NOTE)
		keys = {c[0] for c in cites}
		for p in range(1, PHASE + 1):
			for h in HEADINGS[p]:
				if not re.search(r"^### %s(?=[ (]|$)" % re.escape(h), text, re.M):
					fail("%s: no '### %s' heading (PLAN § 2, the note)" % (NOTE, h))
			for k in CITES[p]:
				if k not in entries:
					fail("doc/references.md has no entry %s (PLAN § 2, the note)" % k)
				if k not in keys:
					fail("%s does not cite %s (R12)" % (NOTE, k))
		present = sorted(f for f in os.listdir(os.path.join(ROOT, "doc")) if f.endswith(".md") and f not in ("README.md", "references.md"))
		notes.check_readme(present, entries)
		for f in OLD_NOTES:
			if read("doc/" + f) != base_text("doc/" + f):
				fail("doc/%s changed: PLAN § 2 leaves it alone" % f)
		if PHASE >= 4:
			notes.check_note("doc/lucas-lehmer.md", ["src/math.cpp", "test_lucas_lehmer", "reference::lucas_lehmer"],
			                 ["stub", "decides nothing"], entries, results)
			notes.check_note("doc/prp-and-gerbicz.md", ["reference::prp3"], ["does not exist yet", "type-1 residue"],
			                 entries, results)
		for f in ("lucas-lehmer.md", "prp-and-gerbicz.md"):
			old, new = base_text("doc/" + f), read("doc/" + f)
			for head in ("The result", "Proof", "Sources", "Provenance"):
				if section(old, head) != section(new, head) and head != "Sources":
					fail("doc/%s: only 'What mps uses' and Sources change (PLAN § 2, the note)" % f)
	except notes.Fail as e:
		fail(str(e))

	agents, old = read("AGENTS.md"), base_text("AGENTS.md")
	hb = re.search(r"^### High-blast-radius paths.*?\n(.*?)(?=^### )", agents, re.M | re.S)
	hb_old = re.search(r"^### High-blast-radius paths.*?\n(.*?)(?=^### )", old, re.M | re.S)
	if not hb or not hb_old:
		fail("AGENTS.md: no '### High-blast-radius paths' section")
	have = hb.group(1).split("\n")
	for l in hb_old.group(1).split("\n"):
		if l.startswith("- ") and l not in have:
			fail("AGENTS.md: the high-blast-radius list lost %r; it only widens" % l)
	for p, lines in LIST_LINES.items():
		if PHASE >= p:
			for l in lines:
				if have.count(l) != 1:
					fail("AGENTS.md: the high-blast-radius list needs exactly once the line %r (PLAN § 2)" % l)
	mp = [l for l in agents.split("\n") if l.startswith("- `src/mp.cpp`, `src/mp.hpp`:")]
	if len(mp) != 1 or "a stub" in mp[0] or "hex64" not in agents.split("- `src/mp.cpp`, `src/mp.hpp`:")[1].split("\n- ")[0]:
		fail("AGENTS.md § Map: the src/mp.* line no longer says 'a stub' and names hex64 (PLAN § 2)")
	tests = agents.split("- `tests/`:")[1].split("\n- ")[0] if "- `tests/`:" in agents else ""
	if "vector_reader" not in tests or "vectors/" not in tests:
		fail("AGENTS.md § Map: the tests/ bullet names vector_reader.* and vectors/ (PLAN § 2)")
	if PHASE >= 4 and sum(1 for l in agents.split("\n") if l.startswith("- `src/reference.cpp`, `src/reference.hpp`:")) != 1:
		fail("AGENTS.md § Map: one line beginning '- `src/reference.cpp`, `src/reference.hpp`:' (PLAN § 2)")
	print("docs: %s and its neighbours pass the notes checker, %d checks true; AGENTS.md as PLAN § 2 gives it" % (NOTE, len(results)))


main()
