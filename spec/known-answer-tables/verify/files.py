# The footprint (a cycle record, not a standing gate; PLAN § 6, step 3): every file PLAN § 2 does
# not list is c41950c's; the listed ones change only as § 2 says.
#
#   python3 -I -B files.py PHASE ROOT BASE

import difflib
import re
import subprocess
import sys

phase, root, base = int(sys.argv[1]), sys.argv[2], sys.argv[3]
bad = []


def git(*args):
	return subprocess.run(["git", "-C", root] + list(args), capture_output=True, text=True, check=True).stdout


def old(path):
	return git("show", "%s:%s" % (base, path))


def new(path):
	return open("%s/%s" % (root, path)).read()


# What PLAN § 2 lets change (checked here or by docs.py, tables.py and the build); spec/, issues/
# and ROADMAP.md are the factory's records
CHANGED = {"Makefile", "AGENTS.md", "doc/README.md", "doc/references.md", "tests/check_reference.cpp",
           "tests/vector_reader.cpp", "tests/vectors/generate.py"}
REMOVED = {"tests/vectors/mersenne-exponents.txt"}
ADDED = {"src/known.hpp", "src/known.cpp", "tests/check_known.cpp", "tests/known/generate.py",
         "tests/known/mersenne-exponents.txt", "tests/known/final-residues.txt", "tests/known/partial-residues.txt"}
REQUIRED = {1: {"src/known.hpp", "src/known.cpp", "tests/check_known.cpp", "tests/known/generate.py",
                "tests/known/mersenne-exponents.txt"},
            2: {"tests/known/final-residues.txt"}, 3: {"tests/known/partial-residues.txt"}}


def free(path):
	# this cycle's own records and the deferral homes; every landed record in spec/ stays as it is
	return path.startswith("spec/known-answer-tables/") or path.startswith("issues/") or path == "ROADMAP.md"


base_files = set(git("ls-tree", "-r", "--name-only", base).split())
tree_files = set(git("ls-files", "--cached", "--others", "--exclude-standard").split())
tree_files = {f for f in tree_files if subprocess.run(["test", "-e", "%s/%s" % (root, f)]).returncode == 0}
for f in sorted(base_files - tree_files):
	if f not in REMOVED and not free(f):
		bad.append("%s is gone" % f)
for f in sorted(tree_files - base_files):
	if f not in ADDED and not free(f):
		bad.append("new file %s is not one PLAN § 2 places" % f)
for f in sorted(REMOVED & tree_files):
	bad.append("%s is still there: the exponent table moves to tests/known/ (R1, R5)" % f)
for n in range(1, phase + 1):
	for f in sorted(REQUIRED[n] - tree_files):
		bad.append("%s is missing (PLAN § 2, P%d)" % (f, n))
for f in sorted(base_files & tree_files):
	if f in CHANGED or free(f):
		continue
	if subprocess.run(["git", "-C", root, "diff", "--quiet", base, "--", f]).returncode != 0:
		bad.append("%s differs from %s, and PLAN § 2 leaves it alone" % (f, base))


def changed_lines(a, b):
	removed, added = [], []
	for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
		if tag in ("replace", "delete"):
			removed += a[i1:i2]
		if tag in ("replace", "insert"):
			added += b[j1:j2]
	return removed, added


# The Makefile: the one MODULES line
removed, added = changed_lines(old("Makefile").splitlines(), new("Makefile").splitlines())
if (removed, added) != (["MODULES  := ansi cli io mp math reference result"], ["MODULES  := ansi cli io known mp math reference result"]):
	bad.append("the Makefile differs from %s other than MODULES gaining known: removed %r, added %r" % (base, removed, added))

# The vector reader: the stem and the op leave it, the STEMS line becomes the one pinned line, and
# nothing else changes, byte for byte
was = old("tests/vector_reader.cpp")
STEMS_OLD = ('\tconst std::vector<std::string> STEMS = {"arith", "folding", "hex", "products", "modular", "decimal", "reference",\n'
             '\t                                        "mersenne-exponents"};\n')
STEMS_NEW = '\tconst std::vector<std::string> STEMS = {"arith", "folding", "hex", "products", "modular", "decimal", "reference"};\n'
if was.count(STEMS_OLD) != 1 or was.count('\t{"mersenne", "D"},\n') != 1:
	bad.append("tests/vector_reader.cpp at %s is not as PLAN § 2 read it" % base)
want = was.replace(STEMS_OLD, STEMS_NEW).replace('\t{"mersenne", "D"},\n', "")
if new("tests/vector_reader.cpp") != want:
	bad.append("tests/vector_reader.cpp differs from %s other than the mersenne-exponents stem and the mersenne op leaving it "
	           "(PLAN § 2 pins the new STEMS line)" % base)

# The vector generator: the family's block and its FAMILIES entry go, and from P2 the comment above
# COMPOSITE changes (docs.py says how); every other byte stays
was = old("tests/vectors/generate.py")
start = was.index("# --- mersenne-exponents: the prime list")
end = was.index("FAMILIES = {")
want = (was[:start] + was[end:]).replace('\t"mersenne-exponents": mersenne_exponents,\n', "")


def comment_above_composite(text):
	lines = text.split("\n")
	i = next(i for i, l in enumerate(lines) if l.startswith("COMPOSITE = ["))
	j = i
	while j > 0 and lines[j - 1].strip():
		j -= 1
	return "\n".join(l for l in lines[j:i] if l.startswith("#"))


now = new("tests/vectors/generate.py")
if now.replace(comment_above_composite(now), comment_above_composite(want), 1) != want:
	bad.append("tests/vectors/generate.py differs from %s other than the mersenne-exponents family's removal and the comment "
	           "above COMPOSITE" % base)

if bad:
	print("files: " + "\n       ".join(bad), file=sys.stderr)
	sys.exit(1)
print("files: the footprint is PLAN § 2's")
