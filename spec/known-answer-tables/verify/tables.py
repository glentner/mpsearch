# The gates' own reader of the known-answer tables, and what each phase pins (a cycle record, not a
# standing gate). Independent of src/known.* and of tests/known/generate.py: it reads PLAN § 2's
# grammar itself, holds the rows research found (research/01, research/02), and recomputes the small
# ones by a second method. Run with python3 -I -B. Subcommands:
#
#   tables.py check PHASE ROOT          the tables of phases 1..PHASE in ROOT, against the pins
#   tables.py tampers PHASE             the scratch-copy edits of phases 1..PHASE: name stem label
#   tables.py tamper NAME FILE          apply one edit to FILE; print the line it names, or "green"
#   tables.py coverage ROOT TRACE       R6: the reference tests' calls in a control run's trace
#
# The model is imported by probe.py, which compares it with the loader on tables of its making.

import functools
import os
import re
import subprocess
import sys

MAGIC = "# mps known 1"
OPS = {"mersenne-exponents": ("mersenne",), "final-residues": ("ll", "prp1"), "partial-residues": ("ll", "prp")}
FIELDS = {"mersenne-exponents": "P", "final-residues": "PH", "partial-residues": "PSKH"}
STEM_OF_PHASE = {1: "mersenne-exponents", 2: "final-residues", 3: "partial-residues"}
BOUND = 10000
RECOMPUTED_BELOW = 200000
RETRIEVED = "2026-10-17"


class Refused(Exception):

	def __init__(self, line, reason):
		Exception.__init__(self, "%d: %s" % (line, reason))
		self.line = line
		self.reason = reason


def is_decimal(t):
	return bool(re.fullmatch(r"0|[1-9][0-9]{0,19}", t)) and int(t) <= (1 << 64) - 1


def is_res64(t):
	return bool(re.fullmatch(r"[0-9A-F]{16}", t))


@functools.lru_cache(maxsize=None)
def is_prime(n):
	if n < 2:
		return False
	d = 2
	while d * d <= n:
		if n % d == 0:
			return False
		d += 1
	return True


def is_date(t):
	m = re.fullmatch(r"([0-9]{4})-([0-9]{2})-([0-9]{2})", t)
	return bool(m) and 1 <= int(m.group(2)) <= 12 and 1 <= int(m.group(3)) <= 31


def key_of(row):
	if row["k"]:
		return (row["op"], row["p"], row["start"], row["k"])
	return (row["op"], row["p"])


def read_source(n, line):
	colon = line.find(": ")
	if colon < 0 or colon + 2 >= len(line):
		raise Refused(n, "source without ': ' and text")
	head = line[7:colon].split(" ")
	src = {"line": n, "origin": None, "key": "", "date": "", "text": line[colon + 2:]}
	if head == ["computed"]:
		src["origin"] = "computed"
	elif head == ["runs"]:
		src["origin"] = "runs"
	elif len(head) == 4 and head[0] == "published" and head[2] == "retrieved":
		if not re.fullmatch(r"[A-Za-z][A-Za-z0-9-]*", head[1]):
			raise Refused(n, "bad key")
		if not is_date(head[3]):
			raise Refused(n, "bad date")
		src.update(origin="published", key=head[1], date=head[3])
	else:
		raise Refused(n, "unknown source")
	return src


def read_row(n, line, stem, src):
	head, reason = line, ""
	b = line.find(" because ")
	if b >= 0:
		head, reason = line[:b], line[b + 9:]
		if not reason or reason[0] == " ":
			raise Refused(n, "empty reason")
	tokens = head.split(" ")
	if any(t == "" for t in tokens):
		raise Refused(n, "double space")
	row = {"line": n, "op": tokens[0], "p": 0, "start": 0, "k": 0, "res64": "", "agreed": 0, "runs": [],
	       "supersedes": "", "reason": "", "source": None, "superseded": False}
	if row["op"] not in OPS[stem]:
		raise Refused(n, "op")
	kinds = FIELDS[stem]
	if len(tokens) < len(kinds) + 1:
		raise Refused(n, "fields")
	for i, kind in enumerate(kinds):
		f = tokens[i + 1]
		if kind == "H":
			if not is_res64(f):
				raise Refused(n, "res64")
			row["res64"] = f
			continue
		if not is_decimal(f):
			raise Refused(n, "decimal")
		v = int(f)
		if kind == "P":
			if v >= 1 << 32 or not is_prime(v) or (stem != "mersenne-exponents" and v < 3):
				raise Refused(n, "exponent")
			row["p"] = v
		elif kind == "S":
			row["start"] = v
		else:
			if v == 0:
				raise Refused(n, "k")
			row["k"] = v
	has_agreed = False
	for t in tokens[len(kinds) + 1:]:
		name, eq, value = t.partition("=")
		if row["supersedes"]:
			raise Refused(n, "supersedes not last")
		if name == "agreed" and eq and stem == "final-residues" and not has_agreed:
			if not is_decimal(value) or int(value) < 2:
				raise Refused(n, "agreed")
			row["agreed"] = int(value)
			has_agreed = True
		elif name == "run" and eq and stem == "partial-residues":
			parts = value.split(":")
			if (len(parts) != 3 or not re.fullmatch(r"[a-z0-9-]+", parts[0]) or not is_decimal(parts[1])
			        or not is_decimal(parts[2]) or int(parts[1]) == 0 or int(parts[2]) >= row["p"]):
				raise Refused(n, "run")
			row["runs"].append((parts[0], int(parts[1]), int(parts[2])))
		elif name == "supersedes" and eq and stem != "mersenne-exponents":
			if not is_res64(value):
				raise Refused(n, "supersedes value")
			row["supersedes"] = value
		else:
			raise Refused(n, "attribute")
	if bool(row["supersedes"]) != bool(reason):
		raise Refused(n, "because")
	row["reason"] = reason
	if (src["origin"] == "published" and stem == "final-residues") != has_agreed:
		raise Refused(n, "agreed rule")
	if src["origin"] == "runs":
		if len({r[0] for r in row["runs"]}) < 2:
			raise Refused(n, "engines")
		if row["supersedes"]:
			raise Refused(n, "a fingerprint supersedes")
	elif row["runs"]:
		raise Refused(n, "runs outside runs")
	if stem == "partial-residues" and row["op"] == "ll" and row["start"] == 4 and row["k"] + 2 == row["p"]:
		raise Refused(n, "a final residue in the partial table")
	return row


def parse(path, text):
	# PLAN § 2: top to bottom, the first line that breaks a line rule; then the trailer's count; then
	# the order of exponents; then corrections; then conflicts
	if isinstance(text, bytes):
		text = text.decode("latin-1")
	lines = text.split("\n")
	if text.endswith("\n"):
		lines.pop()
	if not text or not text.endswith("\n"):
		raise Refused(max(len(lines), 1), "no final LF")
	for i, line in enumerate(lines):
		if not line or any(not (0x20 <= ord(c) <= 0x7E) for c in line):
			raise Refused(i + 1, "not printable")
	if lines[0] != MAGIC:
		raise Refused(1, "magic")
	if len(lines) < 3:
		raise Refused(len(lines), "short")
	stem = None
	for s in OPS:
		if lines[1] == "# command: python3 tests/known/generate.py %s > tests/known/%s.txt" % (s, s):
			stem = s
	if stem is None or os.path.basename(path) != stem + ".txt":
		raise Refused(2, "command")
	m = re.fullmatch(r"# end (0|[1-9][0-9]{0,19}) rows", lines[-1])
	if not m:
		raise Refused(len(lines), "trailer")
	table = {"path": path, "stem": stem, "notes": [], "sources": [], "rows": []}
	for i in range(2, len(lines) - 1):
		line, n = lines[i], i + 1
		if line.startswith("# from "):
			table["sources"].append(read_source(n, line))
		elif line.startswith("# end "):
			raise Refused(n, "trailer early")
		elif line.startswith("# "):
			if table["sources"]:
				raise Refused(n, "late note")
			table["notes"].append(line[2:])
		elif line[0] == "#":
			raise Refused(n, "comment")
		else:
			if not table["sources"]:
				raise Refused(n, "no source")
			row = read_row(n, line, stem, table["sources"][-1])
			row["source"] = len(table["sources"]) - 1
			table["rows"].append(row)
	if int(m.group(1)) != len(table["rows"]):
		raise Refused(len(lines), "count")
	rows = table["rows"]
	if stem == "mersenne-exponents":
		for a, b in zip(rows, rows[1:]):
			if b["p"] <= a["p"]:
				raise Refused(b["line"], "ascending")
	for i, row in enumerate(rows):
		if not row["supersedes"]:
			continue
		for earlier in rows[:i]:
			if not earlier["superseded"] and key_of(earlier) == key_of(row) and earlier["res64"] == row["supersedes"]:
				if (stem == "final-residues" and table["sources"][row["source"]]["origin"] == "computed"
				        and table["sources"][earlier["source"]]["origin"] == "published" and earlier["p"] >= RECOMPUTED_BELOW):
					raise Refused(row["line"], "a weak correction")
				earlier["superseded"] = True
				break
		else:
			raise Refused(row["line"], "supersedes nothing")
	first = {}
	for row in rows:
		if row["superseded"] or not row["res64"]:
			continue
		k = key_of(row)
		if k not in first:
			first[k] = row
		elif first[k]["res64"] != row["res64"]:
			raise Refused(row["line"], "conflict")
	return table


def describe(table):
	# The form probe.cpp prints an accepted table in
	out = []
	for s in table["sources"]:
		out.append("s %d %s %s %s" % (s["line"], s["origin"], s["key"] or "-", s["date"] or "-"))
	for r in table["rows"]:
		runs = ";".join("%s:%d:%d" % run for run in r["runs"]) or "-"
		out.append("r %d %s %d %d %d %s %d %s %s %d %d" % (r["line"], r["op"], r["p"], r["start"], r["k"], r["res64"] or "-",
		                                                   r["agreed"], runs, r["supersedes"] or "-", r["source"], r["superseded"]))
	return out


# --- What each phase pins ------------------------------------------------------------------------

GIMPS_PRIMES = [
	2, 3, 5, 7, 13, 17, 19, 31, 61, 89, 107, 127, 521, 607, 1279, 2203, 2281, 3217, 4253, 4423, 9689, 9941,
	11213, 19937, 21701, 23209, 44497, 86243, 110503, 132049, 216091, 756839, 859433, 1257787, 1398269,
	2976221, 3021377, 6972593, 13466917, 20996011, 24036583, 25964951, 30402457, 32582657, 37156667,
	42643801, 43112609, 57885161, 74207281, 77232917, 82589933, 136279841,
]

# (op, p) -> (Res64, agreed, key)
FINAL = {
	("ll", 1237): ("DF5E6622F83F5BE2", 2, "GIMPSReportLL"),
	("ll", 1277): ("5613A480590E78BA", 3, "GIMPSReportLL"),
	("ll", 10303): ("3DD943A77EC9238B", 3, "GIMPSReportLL"),
	("ll", 100069): ("1ECC98522D52594C", 5, "GIMPSReportLL"),
	("ll", 1000193): ("8F5354551BC18373", 5, "GIMPSReportLL"),
	("ll", 10000139): ("76BB6A7CE0BCA62B", 6, "GIMPSReportLL"),
	("ll", 100000007): ("F9042256B193FAA0", 12, "GIMPSReportLL"),
	("prp1", 28603): ("F899E5F1E2B40323", 2, "GIMPSReportPRP"),
	("prp1", 1000193): ("CA0B0B10C609A027", 4, "GIMPSReportPRP"),
	("prp1", 10000019): ("9DD181D73794E731", 4, "GIMPSReportPRP"),
	("prp1", 100005533): ("ADF278E588B2CDCE", 2, "GIMPSReportPRP"),
}

# (op, p, start, k) -> (Res64, key or "" for computed)
PARTIAL = {
	("ll", 3888509, 4, 100): ("71E61322CCFB396C", "MlucasREADME"),
	("ll", 1277, 4, 100): ("EE3E8EF89BAFBDCD", ""), ("prp", 1277, 3, 100): ("C6D7DFE3FCAEDC31", ""),
	("ll", 1277, 4, 1000): ("3D43F5D86DECC8F5", ""), ("prp", 1277, 3, 1000): ("36F834F2D5842CF5", ""),
	("ll", 10303, 4, 100): ("443391E534C210F2", ""), ("prp", 10303, 3, 100): ("088D6D3D863CB0C7", ""),
	("ll", 10303, 4, 1000): ("7841BFAB20749B83", ""), ("prp", 10303, 3, 1000): ("3C30472597BE7B6C", ""),
	("ll", 10303, 4, 10000): ("FB3AC33F5A26CC71", ""), ("prp", 10303, 3, 10000): ("65CE30B46EF0AA1B", ""),
	("ll", 100069, 4, 100): ("03DFC5B4407ED178", ""), ("prp", 100069, 3, 100): ("05DC064E3050BCD0", ""),
	("ll", 100069, 4, 1000): ("7AA9194B9ED5BBBC", ""), ("prp", 100069, 3, 1000): ("D87224FAC22ABDEF", ""),
	("ll", 1000193, 4, 100): ("F1234B20ACFD642F", ""), ("prp", 1000193, 3, 100): ("44DEBFC2AA1782BA", ""),
	("prp", 3888509, 3, 100): ("79FA4A9B6D077D31", ""),
	("ll", 10000019, 4, 32): ("3DC2608FC9C15BE6", ""), ("prp", 10000019, 3, 32): ("B6DF55F5DE80E01A", ""),
}


def fail(msg):
	print("tables.py: " + msg, file=sys.stderr)
	sys.exit(1)


def load(root, stem):
	path = os.path.join(root, "tests/known/%s.txt" % stem)
	try:
		text = open(path, "rb").read()
	except OSError as e:
		fail("%s: %s" % (path, e))
	try:
		return parse(path, text), text.decode("ascii")
	except Refused as e:
		fail("tests/known/%s.txt:%s" % (stem, e))


def body(text):
	# the source lines and rows, in order (R4: header and trailer lines excepted)
	lines = text.split("\n")[:-1]
	return [l for l in lines[2:-1] if not l.startswith("# ") or l.startswith("# from ")]


def ll_by_remainder(p, start, k):
	m = (1 << p) - 1
	s = start % m
	for _ in range(k):
		s = (s * s - 2) % m
	return s


def res64(v):
	return "%016X" % (v & ((1 << 64) - 1))


def check_exponents(root, base):
	t, text = load(root, "mersenne-exponents")
	rows = t["rows"]
	if [r["p"] for r in rows] != GIMPS_PRIMES:
		fail("mersenne-exponents: the exponents are not the 52 GIMPS listed on %s, ascending (R5)" % RETRIEVED)
	for r in rows:
		s = t["sources"][r["source"]]
		want = ("computed", "", "") if r["p"] <= BOUND else ("published", "GIMPSPrimes", RETRIEVED)
		if (s["origin"], s["key"], s["date"]) != want:
			fail("mersenne-exponents:%d: source %r, want %r (R2, R5)" % (r["line"], (s["origin"], s["key"], s["date"]), want))
	if len(t["sources"]) != 2:
		fail("mersenne-exponents: %d source lines, want 2 (the sweep and the list)" % len(t["sources"]))
	# R4, R5: the 22 records of the old file, byte for byte, first and in order
	old = subprocess.run(["git", "-C", root, "show", "%s:tests/vectors/mersenne-exponents.txt" % base],
	                     capture_output=True, text=True)
	if old.returncode != 0:
		fail("cannot read %s:tests/vectors/mersenne-exponents.txt" % base)
	records = [l for l in old.stdout.split("\n") if l and not l.startswith("#")]
	rowlines = [l for l in body(text) if not l.startswith("#")]
	if len(records) != 22 or rowlines[:22] != records:
		fail("mersenne-exponents: its first rows are not the 22 record lines of %s's tests/vectors/mersenne-exponents.txt, "
		     "byte for byte (R4, R5)" % base)
	return "52 exponents, 22 records kept"


def check_final(root):
	t, _ = load(root, "final-residues")
	got = {}
	for r in t["rows"]:
		s = t["sources"][r["source"]]
		if (r["op"], r["p"]) in got:
			fail("final-residues:%d: %s %d twice" % (r["line"], r["op"], r["p"]))
		got[(r["op"], r["p"])] = (r["res64"], r["agreed"], s["key"])
		if s["origin"] != "published" or s["date"] != RETRIEVED:
			fail("final-residues:%d: not under a published source retrieved %s" % (r["line"], RETRIEVED))
		if r["supersedes"]:
			fail("final-residues:%d: a correction in the first version of the table" % r["line"])
	if got != FINAL:
		fail("final-residues: the rows are not PLAN § 2's eleven, each with its Res64, agreed= and source (R7): "
		     "missing %r, unexpected %r" % (sorted(set(FINAL.items()) - set(got.items())), sorted(set(got.items()) - set(FINAL.items()))))
	for test in ("ll", "prp1"):
		for d in range(3, 9):
			hit = [p for (op, p) in got if op == test and 10 ** d <= p < 10 ** (d + 1)]
			if not hit and not (test == "prp1" and d in (3, 5)):
				fail("final-residues: no %s row in the decade from 10^%d (R7)" % (test, d))
	return "11 final residues"


def check_partial(root):
	t, _ = load(root, "partial-residues")
	got = {}
	for r in t["rows"]:
		s = t["sources"][r["source"]]
		k = key_of(r)
		if k in got:
			fail("partial-residues:%d: %r twice" % (r["line"], k))
		if s["origin"] == "published" and s["date"] != RETRIEVED:
			fail("partial-residues:%d: retrieved %s, want %s" % (r["line"], s["date"], RETRIEVED))
		if s["origin"] == "runs" or r["runs"] or r["supersedes"]:
			fail("partial-residues:%d: a fingerprint or a correction in the first version of the table (R10)" % r["line"])
		got[k] = (r["res64"], s["key"] if s["origin"] == "published" else "")
	if got != PARTIAL:
		fail("partial-residues: the rows are not PLAN § 2's twenty, each with its Res64 and source (R8): missing %r, "
		     "unexpected %r" % (sorted(set(PARTIAL.items()) - set(got.items())), sorted(set(got.items()) - set(PARTIAL.items()))))
	for (op, p, start, k), (_, key) in got.items():
		if not key and k < p.bit_length() + 8:
			fail("partial-residues: %s %d %d %d: k is below ceil(log2 p) + 8" % (op, p, start, k))
	for test in ("ll", "prp"):
		for d in range(3, 8):
			if not [1 for (op, p, _, _), (_, key) in got.items() if op == test and not key and 10 ** d <= p < 10 ** (d + 1)]:
				fail("partial-residues: no computed %s row in the decade from 10^%d (R8)" % (test, d))
	return "20 partial residues"


def check(phase, root):
	base = subprocess.run(["git", "-C", root, "rev-parse", "origin/main"], capture_output=True, text=True).stdout.strip()
	if not base:
		fail("no origin/main")
	said = [check_exponents(root, base)]
	if phase >= 2:
		said.append(check_final(root))
	if phase >= 3:
		said.append(check_partial(root))
	# R4: every table origin/main holds keeps its body as a prefix
	prior = 0
	for stem in OPS:
		old = subprocess.run(["git", "-C", root, "show", "%s:tests/known/%s.txt" % (base, stem)], capture_output=True, text=True)
		if old.returncode == 0:
			prior += 1
			new = open(os.path.join(root, "tests/known/%s.txt" % stem)).read()
			if body(new)[:len(body(old.stdout))] != body(old.stdout):
				fail("tests/known/%s.txt: its source lines and rows on origin/main are not a prefix of the new ones (R4)" % stem)
	print("tables: " + "; ".join(said) + ("; R4: %d prior tables kept as prefixes" % prior if prior
	                                       else "; R4: no table on origin/main yet, the 22 moved records checked instead"))


# --- A second computation of every row the command recomputes ------------------------------------

# Second computations, written apart from tests/known/generate.py: Python's remainder for a
# Lucas-Lehmer residue and pow() for a PRP one wherever they finish in seconds; above that, the
# gate's own reduction (x = q 2^p + r, so x = q + r mod M_p), the only one fast enough there
def mod_mersenne(x, p, m):
	while x > m:
		x = (x >> p) + (x & m)
	return 0 if x == m else x


def second(op, p, start, k):
	m = (1 << p) - 1
	cheap = p * k <= 2 * 10 ** 7 if op != "prp1" else p <= 30000
	if op == "prp1":
		return pow(3, m - 1, m) if cheap else None
	if op == "ll":
		if cheap:
			return ll_by_remainder(p, start, k)
		s = start % m
		for _ in range(k):
			s = mod_mersenne(s * s + m - 2, p, m)
		return s
	if p * k <= 2 * 10 ** 6:
		return pow(start, 1 << k, m)
	x = start % m
	for _ in range(k):
		x = mod_mersenne(x * x, p, m)
	return x


def recompute(phase, part):
	# PART i of 4: four processes share the rows
	rows = []
	if phase >= 2:
		rows += [(op, p, 4, p - 2, v) if op == "ll" else (op, p, 0, 0, v) for (op, p), (v, _, _) in FINAL.items() if p < RECOMPUTED_BELOW]
	if phase >= 3:
		rows += [(op, p, start, k, v) for (op, p, start, k), (v, _) in PARTIAL.items()]
	# the longest first, each to the least loaded of four processes, by a rough cost p^1.6 k
	cost = lambda r: r[1] ** 1.6 * (r[3] if r[3] else r[1])
	load, share = [0.0] * 4, [[] for _ in range(4)]
	for r in sorted(rows, key=cost, reverse=True):
		i = load.index(min(load))
		load[i] += cost(r)
		share[i].append(r)
	mine = share[part]
	for op, p, start, k, value in mine:
		got = second(op, p, start, k)
		if res64(got) != value:
			fail("%s %d %d %d: PLAN § 2 pins %s, a second computation gives %s" % (op, p, start, k, value, res64(got)))
	print("recompute %d: %d rows agree with a second computation" % (part, len(mine)))


# --- Scratch-copy edits (R3 and the checks of PLAN § 2) -----------------------------------------

C_LOADS = "every known-answer table loads"
C_BIB = "every published source has a bibliography entry"
C_BORDER = "exponents above 10000 cite a published list"
C_R9 = "final residues agree with the reference residues"
C_COMPOSITE = "final residues are of composite Mersenne numbers"


def row_index(lines, pred):
	for i, l in enumerate(lines):
		if i >= 2 and not l.startswith("#") and pred(l.split(" ")):
			return i
	fail("no row to edit")


def fix_count(lines):
	n = sum(1 for l in lines[2:-1] if not l.startswith("#"))
	lines[-1] = "# end %d rows" % n


def t_missing(lines):
	return None, "missing"


def t_drop_first_source(lines):
	i = next(i for i, l in enumerate(lines) if l.startswith("# from "))
	del lines[i]
	return lines, i + 1  # the row that was below it is now at its line


def t_append(*rows):
	def edit(lines):
		new = lines[:-1] + list(rows) + [lines[-1]]
		fix_count(new)
		first_row = len(lines)  # 1-based line of the first appended line
		named = [first_row + j for j, r in enumerate(rows) if not r.startswith("#")][-1]
		return new, named
	return edit


def t_append_named(named, *rows):
	# rows appended, and what the edit should do: "green" (make check passes) or "-" (the label fails,
	# no line asked for)
	def edit(lines):
		new, _ = t_append(*rows)(lines)
		return new, named
	return edit


def t_replace(pred, fn, named=None):
	# one row rewritten; make check fails naming its line, or as `named` says
	def edit(lines):
		i = row_index(lines, pred)
		lines[i] = fn(lines[i])
		return lines, (named or i + 1)
	return edit


def t_miscount(lines):
	lines[-1] = re.sub(r"# end (\d+) rows", lambda m: "# end %d rows" % (int(m.group(1)) + 1), lines[-1])
	return lines, len(lines)


def t_source_key(old, new):
	def edit(lines):
		i = next(i for i, l in enumerate(lines) if l.startswith("# from published %s " % old))
		lines[i] = lines[i].replace("published %s " % old, "published %s " % new, 1)
		return lines, "-"
	return edit


def t_border(lines):
	i = next(i for i, l in enumerate(lines) if l.startswith("# from published GIMPSPrimes "))
	del lines[i]
	return lines, "-"


def t_out_of_order(lines):
	return t_append("mersenne 11")(lines)


# name: (stem, label, edit, phase)
TAMPERS = {
	"exp-missing": ("mersenne-exponents", C_LOADS, t_missing, 1),
	"exp-no-source": ("mersenne-exponents", C_LOADS, t_drop_first_source, 1),
	"exp-out-of-order": ("mersenne-exponents", C_LOADS, t_out_of_order, 1),
	"exp-composite": ("mersenne-exponents", C_LOADS, t_replace(lambda t: t[1] == "136279841", lambda l: "mersenne 136279845"), 1),
	"exp-miscount": ("mersenne-exponents", C_LOADS, t_miscount, 1),
	"exp-unknown-key": ("mersenne-exponents", C_BIB, t_source_key("GIMPSPrimes", "GIMPSPrimez"), 1),
	"exp-border": ("mersenne-exponents", C_BORDER, t_border, 1),
	"final-missing": ("final-residues", C_LOADS, t_missing, 2),
	"final-no-source": ("final-residues", C_LOADS, t_drop_first_source, 2),
	"final-conflict": ("final-residues", C_LOADS, t_append("ll 1237 0123456789ABCDEF agreed=2"), 2),
	"final-short-res64": ("final-residues", C_LOADS, t_replace(lambda t: t[1] == "10303", lambda l: l.replace("3DD943A77EC9238B", "3DD943A77EC9238")), 2),
	"final-agreed-1": ("final-residues", C_LOADS, t_replace(lambda t: t[1] == "28603", lambda l: l.replace("agreed=2", "agreed=1")), 2),
	"final-r9": ("final-residues", C_R9, t_replace(lambda t: t[1] == "1237", lambda l: l.replace("DF5E6622F83F5BE2", "DF5E6622F83F5BE3"), "-"), 2),
	"final-at-prime": ("final-residues", C_COMPOSITE, t_append_named("-", "ll 9941 0123456789ABCDEF agreed=2"), 2),
	"final-correction": ("final-residues", None, t_append_named("green", "# from computed: a gate's correction",
	                                                            "ll 10303 0123456789ABCDEF supersedes=3DD943A77EC9238B because a gate's control"), 2),
	"final-weak-correction": ("final-residues", C_LOADS, t_append("# from computed: a gate's edit",
	                                                              "ll 10000139 0123456789ABCDEF supersedes=76BB6A7CE0BCA62B because a gate's edit"), 2),
	"final-r9-correction": ("final-residues", C_R9, t_append_named("-", "# from computed: a gate's edit",
	                                                               "ll 1237 0123456789ABCDEF supersedes=DF5E6622F83F5BE2 because a gate's edit"), 2),
	"final-r9-and-back": ("final-residues", None, t_append_named("green", "# from computed: a gate's control",
	                                                             "ll 1237 0123456789ABCDEF supersedes=DF5E6622F83F5BE2 because away",
	                                                             "ll 1237 DF5E6622F83F5BE2 supersedes=0123456789ABCDEF because back"), 2),
	"partial-missing": ("partial-residues", C_LOADS, t_missing, 3),
	"partial-fingerprint-supersedes": ("partial-residues", C_LOADS, t_append("# from runs: a gate's edit",
	                                                                         "ll 1277 4 100 0123456789ABCDEF run=fp64:64:0 run=ntt:72:5 "
	                                                                         "supersedes=EE3E8EF89BAFBDCD because a gate's edit"), 3),
	"partial-final-ll": ("partial-residues", C_LOADS, t_append("# from computed: a gate's edit", "ll 1277 4 1275 5613A480590E78BA"), 3),
	"partial-k0": ("partial-residues", C_LOADS, t_replace(lambda t: t[1:4] == ["1277", "4", "100"], lambda l: l.replace(" 4 100 ", " 4 0 ")), 3),
	"partial-one-engine": ("partial-residues", C_LOADS, t_append("# from runs: a gate's edit", "ll 1277 4 100 EE3E8EF89BAFBDCD run=fp64:64:0"), 3),
	"partial-same-engine": ("partial-residues", C_LOADS, t_append("# from runs: a gate's edit",
	                                                              "ll 1277 4 100 EE3E8EF89BAFBDCD run=fp64:64:0 run=fp64:128:5"), 3),
	"partial-fingerprint-conflict": ("partial-residues", C_LOADS, t_append("# from runs: a gate's edit",
	                                                                       "ll 1277 4 100 0123456789ABCDEF run=fp64:64:0 run=ntt:72:5"), 3),
	"partial-fingerprint": ("partial-residues", None, t_append_named("green", "# from runs: a gate's control",
	                                                                  "ll 1277 4 100 EE3E8EF89BAFBDCD run=fp64:64:0 run=ntt:72:5"), 3),
	"partial-correction": ("partial-residues", None, t_append_named("green", "# from computed: a gate's correction",
	                                                                 "ll 1277 4 100 0123456789ABCDEF supersedes=EE3E8EF89BAFBDCD because a gate's control"), 3),
}


def tamper(name, path):
	stem, label, edit, _ = TAMPERS[name]
	if edit is t_missing:
		# delete only inside the gate's own temporary directory (common.sh exports it)
		work = os.path.realpath(os.environ.get("GATE_WORK", "/nonexistent"))
		if not os.path.realpath(path).startswith(work + os.sep) or not os.path.basename(work).startswith("mps-gate."):
			fail("refusing to delete %s, outside the gate's own temporary directory" % path)
		os.remove(path)
		print("missing")
		return
	lines = open(path).read().split("\n")[:-1]
	new, named = edit(lines)
	open(path, "w").write("".join(l + "\n" for l in new))
	print(named)


# --- R6 -----------------------------------------------------------------------------------------

def coverage(root, trace):
	t, _ = load(root, "mersenne-exponents")
	listed = {r["p"] for r in t["rows"] if 3 <= r["p"] <= BOUND}
	calls = {"ll": set(), "prp": set()}
	for line in open(trace):
		op, p = line.split()
		calls[op].add(int(p))
	for op in calls:
		if not listed <= calls[op]:
			fail("make check runs the reference %s test on none of %s (R6)" % (op, sorted(listed - calls[op])))
		if any(p > BOUND for p in calls[op]):
			fail("make check runs the reference %s test above %d: %s (R6)" % (op, BOUND, sorted(p for p in calls[op] if p > BOUND)))
	print("coverage: both reference tests on all %d listed exponents from 3 to %d, none above" % (len(listed), BOUND))


def main(argv):
	if argv[1:2] == ["check"]:
		check(int(argv[2]), argv[3])
	elif argv[1:2] == ["tampers"]:
		for name, (stem, label, _, phase) in TAMPERS.items():
			if phase <= int(argv[2]):
				print("%s %s %s" % (name, stem, label or "-"))
	elif argv[1:2] == ["tamper"]:
		tamper(argv[2], argv[3])
	elif argv[1:2] == ["recompute"]:
		recompute(int(argv[2]), int(argv[3]))
	elif argv[1:2] == ["coverage"]:
		coverage(argv[2], argv[3])
	else:
		fail("usage: tables.py check PHASE ROOT | tampers PHASE | tamper NAME FILE | coverage ROOT TRACE")


if __name__ == "__main__":
	main(sys.argv)
