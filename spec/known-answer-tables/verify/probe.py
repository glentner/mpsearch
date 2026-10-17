# The gates' probe of the loader (a cycle record, not a standing gate; PLAN § 6, step 6). Makes
# tables from the tree's own and from samples of its own, each unchanged or with one edit, asks the
# probe driver (probe.cpp, built against src/known.cpp) to read them, and requires its answer to
# equal tables.py's: refused at the same line, or read with the same sources and rows.
#
#   python3 -I -B probe.py PHASE ROOT DRIVER [CASES]     CASES: a file `prepare` wrote, to skip remaking them
#   python3 -I -B probe.py prepare PHASE ROOT CASES

import os
import pickle
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tables  # noqa: E402


def header(stem):
	return ["# mps known 1", "# command: python3 tests/known/generate.py %s > tests/known/%s.txt" % (stem, stem)]


def with_trailer(lines):
	n = sum(1 for l in lines[2:] if not l.startswith("#"))
	return lines + ["# end %d rows" % n]


SAMPLES = {
	"mersenne-exponents": with_trailer(header("mersenne-exponents") + [
		"# a note", "# from computed: a sweep", "mersenne 2", "mersenne 3", "mersenne 5",
		"# from published GIMPSPrimes retrieved 2026-10-17: the list", "mersenne 11213", "mersenne 19937"]),
	"final-residues": with_trailer(header("final-residues") + [
		"# a note", "# from published GIMPSReportLL retrieved 2026-10-17: a report",
		"ll 1237 DF5E6622F83F5BE2 agreed=2", "prp1 28603 F899E5F1E2B40323 agreed=3", "ll 1000193 8F5354551BC18373 agreed=5",
		"# from computed: this command", "ll 1277 5613A480590E78BA", "ll 1237 DF5E6622F83F5BE2",
		"ll 1279 0123456789ABCDEF", "ll 1279 1111111111111111 supersedes=0123456789ABCDEF because a correction",
		"ll 1279 0123456789ABCDEF supersedes=1111111111111111 because back again"]),
	"partial-residues": with_trailer(header("partial-residues") + [
		"# from computed: this command", "ll 1277 4 100 EE3E8EF89BAFBDCD", "prp 1277 3 100 C6D7DFE3FCAEDC31",
		"ll 1277 10 100 0000000000000001",
		"# from runs: two engines", "ll 1277 4 100 EE3E8EF89BAFBDCD run=fp64:64:0 run=ntt:72:5",
		"prp 10303 3 7 ABCDEF0123456789 run=fp64:512:10302 run=ntt:640:0 run=fp64:576:3",
		"# from computed: a correction", "ll 1277 10 100 0000000000000002 supersedes=0000000000000001 because a test"]),
}

# Every token of a row, replaced by each of these in turn (PLAN § 6, step 6: single-token fuzzing)
FUZZ = ["0", "1", "2", "3", "4", "00", "01", "-1", "+1", "1a", "a1", "0x10", "a", "A", "=", "x=1", "agreed=", "agreed=2",
        "run=", "run=a:1:0", "run=:64:0", "run=A:64:0", "run=a_b:64:0", "supersedes=", "supersedes=0123456789ABCDEF",
        "18446744073709551615", "18446744073709551616", "99999999999999999999", "10000000000000000000",
        "4294967291", "4294967311", "4294967296", "0123456789ABCDEF", "0123456789abcdef", "0123456789ABCDEG",
        "0123456789ABCDE", "0123456789ABCDEF0", "because", "mersenne", "ll", "prp", "prp1"]

KEYS = ["A", "Az-9", "a", "Z", "z", "Zz", "zZ", "AZ0", "A09", "a-", "9A", "-A", "A_B", "A.B", "[A", "@A", "`a", "{a"]
ENGINES = ["a", "z", "0", "9", "a0", "z9", "a-9", "-", "A", "a_b", "a.b", "@", "[", "`", "{", "/", ":"]
DATES = ["2026-01-01", "2026-12-31", "2026-01-31", "2026-00-17", "2026-10-00", "2026-10-32", "2026-13-01", "2026-10-170",
         "2026-1-17", "2026/10/17", "202a-10-17", "2026-10-1a", "0000-01-01", "9999-12-31"]

OTHER_OP = {"mersenne-exponents": "ll", "final-residues": "prp", "partial-residues": "prp1"}


def is_row(lines, i):
	return 2 <= i < len(lines) - 1 and not lines[i].startswith("#")


def text_of(lines, lf=True):
	t = "\n".join(lines)
	return t + "\n" if lf else t


def recount(lines):
	return with_trailer(lines[:-1])


def row_edits(stem, line):
	t = line.split(" ")
	head = line.split(" because ")[0]
	out = []
	r = {"mersenne-exponents": None, "final-residues": 2, "partial-residues": 4}[stem]
	if r is not None and r < len(t):
		for v in (t[r].lower(), t[r][:15], t[r] + "0", "G" + t[r][1:]):
			out.append(" ".join(t[:r] + [v] + t[r + 1:]))
	p = t[1]
	for v in ("0" + p, str(int(p) * 3), "2", str((1 << 32) + 15), "1"):
		out.append(" ".join(t[:1] + [v] + t[2:]))
	out += ["xx " + " ".join(t[1:]), OTHER_OP[stem] + " " + " ".join(t[1:]), " ".join(t[:-1]), line + " x=1",
	        line.replace(" ", "  ", 1), line + " ", line.replace(" ", "\t", 1), line + "\r", line + " because why"]
	if r is not None:
		out.append(head + " supersedes=" + t[r])
		out.append(head + " supersedes=" + t[r] + " because ")
	if stem == "final-residues":
		out += [line.replace("agreed=", "agreed=1x") if "agreed=" in line else line + " agreed=3",
		        " ".join(x for x in t if not x.startswith("agreed=")), line + " agreed=1", line + " agreed=2"]
	if stem == "partial-residues":
		out += [" ".join(t[:3] + ["0"] + t[4:]), " ".join(t[:2] + ["0" + t[2]] + t[3:]),
		        head + " run=fp64:64:" + p, head + " run=FP64:64:0", head + " run=fp64:0:0", head + " run=fp64:64",
		        head + " run=fp64:64:0", head + " run=ntt:72:5 run=fp64:64:0"]
	return out


def source_edits(line):
	colon = line.find(": ")
	out = []
	if line.startswith("# from published "):
		head = line[:colon].split(" ")
		for k in KEYS:
			out.append(" ".join(head[:3] + [k] + head[4:]) + line[colon:])
		for d in DATES:
			out.append(" ".join(head[:5] + [d]) + line[colon:])
	return out + ["# from recalled" + line[colon:], line.replace("published ", "published Bad_Key ", 1),
	        line.replace("retrieved 2026-10-17", "retrieved 2026-13-17"), line.replace("retrieved 2026-10-17", "retrieved 2026-10-32"),
	        line.replace(" retrieved 2026-10-17", ""), line[:colon + 2], line.replace(": ", " ", 1), "# from  computed: x"]


def fuzz(lines, i):
	# every token of row i replaced by each FUZZ value, and each FUZZ value appended
	t = lines[i].split(" ")
	out = []
	for j in range(len(t)):
		for v in FUZZ:
			if v != t[j]:
				out.append(lines[:i] + [" ".join(t[:j] + [v] + t[j + 1:])] + lines[i + 1:])
	for v in FUZZ:
		out.append(lines[:i] + [lines[i] + " " + v] + lines[i + 1:])
	return out


def cases_for(stem, lines, every):
	cases = [("identity", lines)]
	cases += [("magic-only", ["# mps known 1"]), ("two-lines", lines[:2]), ("magic-trailer", [lines[0], lines[-1]])]
	cases += [("empty-table", lines[:2] + ["# end 0 rows"]), ("empty-table-note", lines[:2] + ["# a note", "# end 0 rows"])]
	cases += [("early-end", lines[:2] + ["# end 1 rows"] + lines[2:]), ("tilde", lines[:2] + ["# a note ~"] + lines[2:]),
	          ("del", lines[:2] + ["# a note \x7f"] + lines[2:])]
	cases.append(("magic", ["# mps known 2"] + lines[1:]))
	for other in tables.OPS:
		if other != stem:
			cases.append(("command", lines[:1] + header(other)[1:] + lines[2:]))
	cases += [("count+1", lines[:-1] + ["# end %d rows" % (int(lines[-1].split()[2]) + 1)]),
	          ("no-trailer", lines[:-1]), ("two-trailers", lines + [lines[-1]]),
	          ("after-trailer", lines + ["mersenne 3"]), ("empty-line", lines[:2] + [""] + lines[2:]),
	          ("hash", lines[:2] + ["#x"] + lines[2:]), ("bad-trailer", lines[:-1] + ["# end rows"])]
	first_source = next(i for i, l in enumerate(lines) if l.startswith("# from "))
	cases.append(("late-note", lines[:first_source + 1] + ["# a late note"] + lines[first_source + 1:]))
	for i, l in enumerate(lines):
		if l.startswith("# from "):
			for j, v in enumerate(source_edits(l)):
				cases.append(("source%d.%d" % (i, j), lines[:i] + [v] + lines[i + 1:]))
			cases.append(("drop-source%d" % i, lines[:i] + lines[i + 1:]))
	rows = [i for i in range(len(lines)) if is_row(lines, i)]
	picked = rows if every else sorted(set(rows[:2] + rows[-2:] + rows[::max(1, len(rows) // 4)]))
	for i in picked:
		for j, v in enumerate(row_edits(stem, lines[i])):
			cases.append(("row%d.%d" % (i, j), lines[:i] + [v] + lines[i + 1:]))
		if every or i in (rows[0], rows[-1]):
			for j, ls in enumerate(fuzz(lines, i)):
				cases.append(("fuzz%d.%d" % (i, j), ls))
		head = lines[i].split(" because ")[0]
		bare = " ".join(x for x in head.split(" ") if not x.startswith(("agreed=", "run=")))
		cases += [("as-fingerprint%d" % i, recount(lines[:-1] + ["# from runs: x", bare + " run=fp64:64:0 run=ntt:72:1"] + lines[-1:])),
		          ("as-published%d" % i, recount(lines[:-1] + ["# from published GIMPSReportLL retrieved 2026-10-17: x",
		                                                      bare + " agreed=2"] + lines[-1:]))]
		cases += [("because-two%d" % i, lines[:i] + [head + " supersedes=0123456789ABCDEF because  x"] + lines[i + 1:]),
		          ("because-end%d" % i, lines[:i] + [head + " supersedes=0123456789ABCDEF because"] + lines[i + 1:]),
		          ("other-attrs%d" % i, lines[:i] + [head + {"mersenne-exponents": " supersedes=0123456789ABCDEF because x",
		                                                       "final-residues": " run=fp64:64:0 run=ntt:72:1",
		                                                       "partial-residues": " agreed=2"}[stem]] + lines[i + 1:])]
		t = lines[i].split(" ")
		if stem == "mersenne-exponents":
			cases.append(("dup%d" % i, recount(lines[:-1] + [lines[i]] + lines[-1:])))
			if i + 1 in rows:
				cases.append(("swap%d" % i, lines[:i] + [lines[i + 1], lines[i]] + lines[i + 2:]))
			continue
		r = 2 if stem == "final-residues" else 4
		key = " ".join(t[:r])
		cases.append(("above%d" % i, recount(lines[:i] + [key + " 0123456789ABCDEF supersedes=%s because early" % t[r]] + lines[i:])))
		for j, reason in enumerate(["  two spaces", " ", "x", "~", "because"]):
			cases.append(("reason%d.%d" % (i, j), recount(lines[:-1] + ["# from computed: x", key + " 0123456789ABCDEF supersedes=%s because %s" % (t[r], reason)]
			                                                  + lines[-1:])))
		if stem == "final-residues":
			cases.append(("after-supersedes%d" % i, recount(lines[:-1] + ["# from published GIMPSReportLL retrieved 2026-10-17: x",
			                                                              key + " 0123456789ABCDEF supersedes=%s agreed=2 because x" % t[r]] + lines[-1:])))
			cases.append(("published-correction%d" % i, recount(lines[:-1] + ["# from published GIMPSReportLL retrieved 2026-10-18: x",
			                                                                  key + " 0123456789ABCDEF agreed=2 supersedes=%s because x" % t[r]] + lines[-1:])))
		if stem == "partial-residues":
			for j, e in enumerate(ENGINES):
				cases.append(("engine%d.%d" % (i, j), recount(lines[:-1] + ["# from runs: x", key + " %s run=%s:64:0 run=ntt:72:1" % (t[r], e)] + lines[-1:])))
		cases.append(("weak%d" % i, recount(lines[:-1] + ["# from published GIMPSReportLL retrieved 2026-10-17: x",
		                                                  key + " " + t[r] + (" agreed=2" if stem == "final-residues" else ""),
		                                                  "# from computed: x",
		                                                  key + " 0123456789ABCDEF supersedes=%s because weak" % t[r]] + lines[-1:])))
		cases.append(("runs-correct%d" % i, recount(lines[:-1] + ["# from runs: x",
		                                                          key + " 0123456789ABCDEF run=fp64:64:0 run=ntt:72:1 supersedes=%s because r" % t[r]]
		                                            + lines[-1:])))
		if stem == "partial-residues":
			p_ = int(t[1])
			cases.append(("final-ll%d" % i, recount(lines[:-1] + ["# from computed: x", "ll %d 4 %d %s" % (p_, p_ - 2, t[r])] + lines[-1:])))
			cases.append(("near-final-ll%d" % i, recount(lines[:-1] + ["# from computed: x", "ll %d 4 %d %s" % (p_, p_ - 3, t[r]),
			                                                           "ll %d 3 %d %s" % (p_, p_ - 2, t[r])] + lines[-1:])))
		for kind, appended in (
		        ("conflict", ["# from computed: x", key + " 0123456789ABCDEF"]),
		        ("agree", ["# from computed: x", key + " " + t[r]]),
		        ("correct", ["# from computed: x", key + " 0123456789ABCDEF supersedes=%s because a test" % t[r]]),
		        ("correct-none", ["# from computed: x", key + " 0123456789ABCDEF supersedes=FEDCBA9876543210 because a test"]),
		        ("correct-twice", ["# from computed: x", key + " 0123456789ABCDEF supersedes=%s because a" % t[r],
		                           key + " 1123456789ABCDEF supersedes=%s because b" % t[r]])):
			cases.append(("%s%d" % (kind, i), recount(lines[:-1] + appended + lines[-1:])))
		if stem == "partial-residues":
			for kind, runs, value in (("fp", "run=fp64:64:0 run=ntt:72:1", t[r]), ("fp-one", "run=fp64:64:0", t[r]),
			                          ("fp-same", "run=fp64:64:0 run=fp64:72:1", t[r]), ("fp-conflict", "run=fp64:64:0 run=ntt:72:1", "0123456789ABCDEF"),
			                          ("fp-none", "", t[r])):
				row = (key + " " + value + " " + runs).rstrip()
				cases.append(("%s%d" % (kind, i), recount(lines[:-1] + ["# from runs: x", row] + lines[-1:])))
	return cases


def prepare(phase, root):
	# the tables, with tables.py's answers: (cases, feed, expected)
	want_tables = ["tests/known/%s.txt" % tables.STEM_OF_PHASE[n] for n in range(1, phase + 1)]
	bases = []
	for n in range(1, phase + 1):
		stem = tables.STEM_OF_PHASE[n]
		real = open(os.path.join(root, "tests/known/%s.txt" % stem)).read().split("\n")[:-1]
		bases.append((stem, real, False))
	for stem, lines in SAMPLES.items():
		bases.append((stem, lines, True))
	cases = []
	for stem, lines, every in bases:
		for name, ls in cases_for(stem, lines, every):
			cases.append((stem, name, text_of(ls)))
		cases.append((stem, "no-lf", text_of(lines, lf=False)))
		cases.append((stem, "empty", ""))
		cases.append((stem, "non-ascii", text_of(lines[:2] + ["# caf\xe9"] + lines[2:])))
	feed = b""
	expected = {}
	for n, (stem, name, text) in enumerate(cases):
		path = "tests/known/%s.txt" % stem
		data = text.encode("latin-1")
		feed += b"case %d %s %d\n" % (n, path.encode(), len(data)) + data + b"\n"
		try:
			expected[n] = ("ok", tables.describe(tables.parse(path, data)))
		except tables.Refused as e:
			expected[n] = ("refused", e.line)
	return want_tables, cases, feed, expected


def main(argv):
	if argv[1] == "prepare":
		pickle.dump(prepare(int(argv[2]), argv[3]), open(argv[4], "wb"))
		return
	phase, root, driver = int(argv[1]), argv[2], argv[3]
	want_tables, cases, feed, expected = pickle.load(open(argv[4], "rb")) if len(argv) > 4 else prepare(phase, root)
	r = subprocess.run([driver], input=feed, capture_output=True)
	if r.returncode != 0:
		print("probe: the loader's driver crashes (status %d): %s" % (r.returncode, r.stderr.decode("ascii", "replace")[-200:]), file=sys.stderr)
		sys.exit(1)
	out = r.stdout.decode("ascii", "replace")
	got = {}
	first = out.split("\n", 1)[0].split(" ")
	if first[0] != "tables" or first[1:] != want_tables:
		print("probe: known::TABLES is %r, want %r, in that order (PLAN § 2)" % (first[1:], want_tables), file=sys.stderr)
		sys.exit(1)
	for line in out.split("\n")[1:]:
		if not line:
			continue
		n, rest = line.split(" ", 1)
		n = int(n)
		if rest == "ok":
			got[n] = ("ok", [])
		elif rest.startswith("refused "):
			got[n] = ("refused", rest[8:])
		else:
			got[n][1].append(rest)
	bad = 0
	refused = 0
	for n, (stem, name, _) in enumerate(cases):
		want, have = expected[n], got.get(n)
		path = "tests/known/%s.txt" % stem
		if want[0] == "refused":
			refused += 1
			ok = have is not None and have[0] == "refused" and have[1].startswith("%s:%d: " % (path, want[1]))
		else:
			ok = have == want
		if not ok:
			bad += 1
			if bad <= 5:
				print("probe: %s %s: tables.py says %r, the loader %r" % (stem, name, want, have), file=sys.stderr)
	if bad:
		print("probe: the loader disagrees with tables.py on %d of %d tables" % (bad, len(cases)), file=sys.stderr)
		sys.exit(1)
	print("probe: %d tables, %d refused at the line tables.py names, %d read alike" % (len(cases), refused, len(cases) - refused))


if __name__ == "__main__":
	main(sys.argv)
