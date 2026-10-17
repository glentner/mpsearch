# Point mutants of the loader (a cycle record, not a standing gate; PLAN § 6, step 7b), after the
# design review's own generator: each single-line `if (…)` of src/known.cpp made never true, and each
# comparison operator flipped (< and <=, > and >=, == and !=), from the first static helper on.
# Three at a time (the gate's memory footprint), each mutant is built with probe.cpp and must
# disagree with tables.py on probe.py's tables; a
# survivor passes only when spec/known-answer-tables/equivalent-mutants.txt lists it, one line each:
#   <the mutated line, stripped> | <the mutation> | <why no table can tell it from the original>
#
#   python3 -I -B pointmut.py PHASE ROOT WORK

import concurrent.futures
import os
import re
import subprocess
import sys

phase, root, work = int(sys.argv[1]), sys.argv[2], sys.argv[3]
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(root, "src/known.cpp")).read().split("\n")
start = next((i for i, l in enumerate(src) if l.startswith("static bool ")), None)
if start is None:
	print("pointmut: src/known.cpp has no static helper to start from", file=sys.stderr)
	sys.exit(1)
muts = []
for i in range(start, len(src)):
	line = src[i]
	if line.strip().startswith("//"):
		continue
	m = re.match(r"^(\s*(?:\} )?)(if|else if) \((.*)\)( \{)?$", line)
	if m and m.group(3).count("(") == m.group(3).count(")"):
		muts.append((i, m.group(1) + m.group(2) + " (false and (" + m.group(3) + "))" + (m.group(4) or ""), "never"))
	if "for (" in line:
		continue
	for a, b in ((" <= ", " < "), (" < ", " <= "), (" >= ", " > "), (" > ", " >= "), (" != ", " == "), (" == ", " != ")):
		for k in [j for j in range(len(line)) if line.startswith(a, j)]:
			muts.append((i, line[:k] + b + line[k + len(a):], "flip %s to %s at column %d" % (a.strip(), b.strip(), k + 1)))

allowed = {}
listing = os.path.join(root, "spec/known-answer-tables/equivalent-mutants.txt")
if os.path.exists(listing):
	for entry in open(listing).read().splitlines():
		parts = [p.strip() for p in entry.split(" | ")]
		if len(parts) == 3 and parts[2]:
			allowed[(parts[0], parts[1])] = parts[2]


cases = os.path.join(work, "cases.pickle")
subprocess.run([sys.executable, "-I", "-B", os.path.join(here, "probe.py"), "prepare", str(phase), root, cases], check=True)


def one(n):
	i, new, what = muts[n]
	d = os.path.join(work, "m%03d" % n)
	os.makedirs(d, exist_ok=True)
	lines = list(src)
	lines[i] = new
	open(os.path.join(d, "known.cpp"), "w").write("\n".join(lines))
	b = subprocess.run(["clang++", "-O0", "-std=c++20", "-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE",
	                    "-I", os.path.join(root, "src"), os.path.join(here, "probe.cpp"), os.path.join(d, "known.cpp"),
	                    "-o", os.path.join(d, "probe")], capture_output=True)
	if b.returncode != 0:
		return n, "nobuild"
	try:
		r = subprocess.run([sys.executable, "-I", "-B", os.path.join(here, "probe.py"), str(phase), root, os.path.join(d, "probe"), cases],
		                   capture_output=True, timeout=30)
	except subprocess.TimeoutExpired:
		return n, "killed"
	return n, "killed" if r.returncode != 0 else "survived"


with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
	results = dict(pool.map(one, range(len(muts))))
survivors = [(muts[n][1].strip(), muts[n][2]) for n in sorted(results) if results[n] == "survived"]
unlisted = [s for s in survivors if s not in allowed]
if unlisted:
	for line, what in unlisted:
		print("pointmut: survives the probe and is not argued equivalent: %s | %s" % (line, what), file=sys.stderr)
	sys.exit(1)
print("pointmut: %d point mutants of the loader, %d killed by the probe, %d argued equivalent, %d do not build"
      % (len(muts), sum(1 for v in results.values() if v == "killed"), len(survivors), sum(1 for v in results.values() if v == "nobuild")))
