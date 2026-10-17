# The documents (a cycle record, not a standing gate; PLAN § 6, step 9): AGENTS.md's list and map,
# the comment above COMPOSITE, the bibliography's entries and the two sentences naming the tables.
#
#   python3 -I -B docs.py PHASE ROOT BASE

import os
import re
import subprocess
import sys

phase, root, base = int(sys.argv[1]), sys.argv[2], sys.argv[3]
bad = []


def old(path):
	return subprocess.run(["git", "-C", root, "show", "%s:%s" % (base, path)], capture_output=True, text=True, check=True).stdout


def new(path):
	return open(os.path.join(root, path)).read()


def exists(path):
	return os.path.exists(os.path.join(root, path))


# --- AGENTS.md: the high-blast-radius list only widens, each table's line exactly when it exists --

LIST = {
	"tests/known": "- `tests/known/` (the tables of known answers and their command: expected values)",
	"src/known.cpp": "- `src/known.hpp`, `src/known.cpp` (the tables' loader)",
	"tests/known/mersenne-exponents.txt": "- `tests/known/mersenne-exponents.txt` (the Mersenne prime exponents)",
	"tests/known/final-residues.txt": "- `tests/known/final-residues.txt` (the final residues of composite exponents)",
	"tests/known/partial-residues.txt": "- `tests/known/partial-residues.txt` (the residues after a stated number of iterations, and the fingerprints)",
}


def the_list(text):
	a = text.index("### High-blast-radius paths")
	b = text.index("### 1. Verdicts are earned")
	return [l for l in text[a:b].splitlines() if l.startswith("- ")]


agents, agents_old = new("AGENTS.md"), old("AGENTS.md")
now, was = the_list(agents), the_list(agents_old)
if now[:len(was)] != was:
	bad.append("AGENTS.md: the high-blast-radius list does not keep its lines first and in order (it only widens)")
for path, line in LIST.items():
	if exists(path) and line not in now:
		bad.append("AGENTS.md: the list lacks %r though %s exists (R12)" % (line, path))
	if not exists(path) and line in now:
		bad.append("AGENTS.md: the list names %s, which does not exist yet: a path joins in the commit that creates it (R12)" % path)
for line in now[len(was):]:
	if line not in LIST.values():
		bad.append("AGENTS.md: the list gains %r, which PLAN § 2 does not give" % line)
if "\n- `src/known.cpp`, `src/known.hpp`:" not in agents:
	bad.append("AGENTS.md: § Map has no line beginning '- `src/known.cpp`, `src/known.hpp`:' (R12)")
tests = re.search(r"\n- `tests/`:(.*?)(?=\n- `)", agents, re.S)
if not tests:
	bad.append("AGENTS.md: § Map has no `tests/` bullet")
else:
	words = tests.group(1)
	for name in ["`known/`", "`known/generate.py`"] + ["`%s`" % os.path.basename(p) for p in LIST if p.endswith(".txt") and exists(p)]:
		if name not in words.replace("\n  ", " "):
			bad.append("AGENTS.md: § Map's tests/ bullet does not name %s (R12)" % name)
	if "`vectors/mersenne-exponents.txt` is" in words.replace("\n  ", " "):
		bad.append("AGENTS.md: § Map still calls vectors/mersenne-exponents.txt the exponents")


def bullet(text, opening):
	# the § Map bullet that starts with OPENING: its first line and the indented lines after it
	lines = text.split("\n")
	i = next((i for i, l in enumerate(lines) if l.startswith(opening)), None)
	if i is None:
		return ""
	j = i + 1
	while j < len(lines) and lines[j].startswith("  "):
		j += 1
	return "\n".join(lines[i:j]) + "\n"


# Everything else in AGENTS.md is c41950c's, byte for byte: take out the pinned list lines and the
# src/known bullet, put the old tests/ bullet back, and the file must be the old one
rest = agents
for line in LIST.values():
	rest = rest.replace(line + "\n", "", 1)
rest = rest.replace(bullet(agents, "- `src/known.cpp`, `src/known.hpp`:"), "", 1)
rest = rest.replace(bullet(agents, "- `tests/`:"), bullet(agents_old, "- `tests/`:"), 1)
if rest != agents_old:
	bad.append("AGENTS.md changes outside the five list lines, the src/known bullet and the tests/ bullet of § Map (PLAN § 2)")

# --- tests/vectors/generate.py: the comment above COMPOSITE points at the residue table (P2) ------

if phase >= 2:
	lines = new("tests/vectors/generate.py").splitlines()
	i = next(i for i, l in enumerate(lines) if l.startswith("COMPOSITE = ["))
	j = i
	while j > 0 and lines[j - 1].strip():
		j -= 1
	comment = " ".join(l for l in lines[j:i] if l.startswith("#"))
	if "tests/known/final-residues.txt" not in comment or "issues/" in comment:
		bad.append("tests/vectors/generate.py: the comment above COMPOSITE does not point at tests/known/final-residues.txt "
		           "instead of the seed (R12)")

# --- doc/references.md and doc/README.md --------------------------------------------------------

ENTRIES = {
	1: {"GIMPSPrimes": "https://www.mersenne.org/primes/"},
	2: {"GIMPSReportLL": "https://www.mersenne.org/report_LL/", "GIMPSReportPRP": "https://www.mersenne.org/report_PRP/"},
	3: {"MlucasREADME": "https://www.mersenneforum.org/mayer/README.html"},
}
refs, refs_old = new("doc/references.md"), old("doc/references.md")
heads = re.findall(r"^### (\S+)$", refs, re.M)
if heads != sorted(heads, key=str.lower):
	bad.append("doc/references.md: the entries are not sorted by key")
old_lines = refs_old.splitlines()
intro = next(l for l in old_lines if l.startswith("Every work"))
it = iter(refs.splitlines())
if not all(any(l == m for m in it) for l in old_lines if l != intro):
	bad.append("doc/references.md: an existing line changed or moved (only the opening sentence may change)")
opening = next((l for l in refs.splitlines() if l.startswith("Every work")), "")
if "`tests/known/`" not in opening:
	bad.append("doc/references.md: the opening sentence does not name the tables in `tests/known/` (R2)")
for n in range(1, phase + 1):
	for key, url in ENTRIES[n].items():
		m = re.search(r"^### %s\n\n((?:- \*\*[A-Za-z]+:\*\* .*\n)+)" % re.escape(key), refs, re.M)
		if not m:
			bad.append("doc/references.md: no entry %s (R2)" % key)
			continue
		fields = re.findall(r"^- \*\*([A-Za-z]+):\*\* (.*)$", m.group(1), re.M)
		if [f for f, _ in fields] != ["Authors", "Title", "Venue", "Year", "URL", "Verified"]:
			bad.append("doc/references.md: %s's fields are %r, want Authors, Title, Venue, Year, URL, Verified" % (key, [f for f, _ in fields]))
			continue
		d = dict(fields)
		if d["URL"] != "<%s>" % url:
			bad.append("doc/references.md: %s's URL is %s, want <%s>" % (key, d["URL"], url))
		v = re.match(r"(\d{4}-\d{2}-\d{2}) against ", d["Verified"])
		if not v or v.group(1) < "2026-10-17":
			bad.append("doc/references.md: %s's Verified line does not open with its date, 2026-10-17 or later, and 'against'" % key)
readme, readme_old = new("doc/README.md").splitlines(), old("doc/README.md").splitlines()
diff = [(a, b) for a, b in zip(readme_old, readme) if a != b]
if len(readme) != len(readme_old) or len(diff) != 1 or not diff[0][0].startswith("The bibliography is") or "`tests/known/`" not in diff[0][1]:
	bad.append("doc/README.md: the only change is not the bibliography sentence naming the tables in `tests/known/`")

if bad:
	print("docs: " + "\n      ".join(bad), file=sys.stderr)
	sys.exit(1)
print("docs: AGENTS.md, the COMPOSITE comment and %d bibliography entries as PLAN § 2 gives them"
      % sum(len(ENTRIES[n]) for n in range(1, phase + 1)))
