# The tables' command, run from inside (a cycle record, not a standing gate; PLAN § 6, step 1). A
# command that embeds its values can pass any test of its output alone, so this imports the tree's
# tests/known/generate.py and wraps the three functions PLAN § 2 pins (lucas_lehmer, squarings,
# prp_type1): the trace run records every call and its answer and must write the table byte for
# byte, calling the function of every row PLAN § 2 says it recomputes; each later run answers from
# that record, so it costs seconds, with one answer wrong (a sabotage) or one embedded Res64 changed
# in the source (a literal), and the command must then exit 1, with no traceback, naming the row.
#
#   command.py trace ROOT STEM RECORD TABLE      the trace run; the table it writes goes to TABLE
#   command.py refuse ROOT STEM RECORD           every sabotage and every literal change for STEM

import contextlib
import importlib.util
import inspect
import io
import os
import pickle
import re
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tables  # noqa: E402

PINNED = ("lucas_lehmer", "squarings", "prp_type1")
BOUND = 200000


def load_module(root, source=None):
	path = os.path.join(root, "tests/known/generate.py")
	if source is None:
		source = open(path).read()
	spec = importlib.util.spec_from_loader("gate_generate", loader=None, origin=path)
	module = importlib.util.module_from_spec(spec)
	module.__file__ = path
	exec(compile(source, path, "exec"), module.__dict__)
	for name in PINNED:
		if not callable(module.__dict__.get(name)):
			fail("tests/known/generate.py has no function %s (PLAN § 2 pins it)" % name)
	return module


def fail(msg):
	print("command.py: " + msg, file=sys.stderr)
	sys.exit(1)


def normalize(fn, name, args, kwargs):
	bound = inspect.signature(fn).bind(*args, **kwargs)
	return (name,) + tuple(int(v) for v in bound.args)


def wrap(module, record, calls, sabotage=None, cached=False):
	for name in PINNED:
		real = module.__dict__[name]

		def wrapper(*args, _real=real, _name=name, **kwargs):
			key = normalize(_real, _name, args, kwargs)
			calls.append(key)
			if cached and key in record:
				value = record[key]
			else:
				value = _real(*args, **kwargs)
				record[key] = value
			if key == sabotage:
				return 1 if value == 0 else (0 if _name == "lucas_lehmer" and key[2:] == (4, key[1] - 2) else value ^ 1)
			return value
		module.__dict__[name] = wrapper


def run(module, stem):
	# main(argv) with stdout and stderr captured; (status, stdout bytes, stderr text, traceback or "")
	out, err = io.BytesIO(), io.StringIO()
	stdout = io.TextIOWrapper(out, encoding="ascii")
	status, tb = 0, ""
	with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(err):
		try:
			r = module.main(["generate.py", stem])
			status = r if isinstance(r, int) else 0
		except SystemExit as e:
			status = e.code if isinstance(e.code, int) else 1
		except BaseException:
			status, tb = -1, traceback.format_exc()
		try:
			stdout.flush()
		except ValueError:
			pass
	return status, out.getvalue(), err.getvalue(), tb


def expected_calls(root, stem):
	t = tables.parse("tests/known/%s.txt" % stem, open(os.path.join(root, "tests/known/%s.txt" % stem), "rb").read())
	want = set()
	if stem == "mersenne-exponents":
		sieve = [p for p in range(3, 10001) if tables.is_prime(p)]
		want = {("lucas_lehmer", p, 4, p - 2) for p in sieve}
	for r in t["rows"]:
		if stem == "final-residues" and r["p"] < BOUND:
			want.add(("lucas_lehmer", r["p"], 4, r["p"] - 2) if r["op"] == "ll" else ("prp_type1", r["p"]))
		if stem == "partial-residues":
			want.add(("lucas_lehmer" if r["op"] == "ll" else "squarings", r["p"], r["start"], r["k"]))
	return t, want


def trace(root, stem, record_path, table_path):
	module = load_module(root)
	record, calls = {}, []
	wrap(module, record, calls)
	status, out, err, tb = run(module, stem)
	if status != 0:
		fail("%s: the command exits %r in the trace run: %s%s" % (stem, status, err.strip()[-300:], tb[-300:]))
	open(table_path, "wb").write(out)
	_, want = expected_calls(root, stem)
	missing = sorted(want - set(calls))
	if missing:
		fail("%s: the command never calls %s for %d rows it must recompute, e.g. %r (R9, R5; PLAN § 2)" % (stem, missing[0][0], len(missing), missing[:3]))
	pickle.dump({k: record[k] for k in want}, open(record_path, "wb"))
	print("%s: traced, %d recomputations called" % (stem, len(want)))


def words(stem, key):
	# what the message must name: the row's op and the numbers of its key (PLAN § 2)
	if key[0] == "prp_type1":
		return ["prp1", str(key[1])]
	if stem == "final-residues":
		return ["ll", str(key[1])]
	return ["ll" if key[0] == "lucas_lehmer" else "prp"] + [str(v) for v in key[1:]]


def refuse(root, stem, record_path):
	record = pickle.load(open(record_path, "rb"))
	t, want = expected_calls(root, stem)
	runs = []
	if stem == "mersenne-exponents":
		listed = [r["p"] for r in t["rows"] if r["p"] <= 10000 and r["p"] > 2]
		for p in listed + [9973]:
			runs.append(("sabotage", ("lucas_lehmer", p, 4, p - 2), [str(p)]))
	else:
		for key in sorted(want):
			runs.append(("sabotage", key, None))
		source = open(os.path.join(root, "tests/known/generate.py")).read()
		for r in t["rows"]:
			key = ("lucas_lehmer", r["p"], 4, r["p"] - 2) if r["op"] == "ll" and stem == "final-residues" else \
			      ("prp_type1", r["p"]) if r["op"] == "prp1" else \
			      ("lucas_lehmer" if r["op"] == "ll" else "squarings", r["p"], r["start"], r["k"])
			if key not in want:
				continue
			if source.count(r["res64"]) != 1:
				fail("%s: the Res64 %s appears %d times in tests/known/generate.py, want once (PLAN § 2)" % (stem, r["res64"], source.count(r["res64"])))
			changed = r["res64"][:-1] + ("0" if r["res64"][-1] != "0" else "1")
			runs.append(("literal", key, (r["res64"], changed)))
	for kind, key, extra in runs:
		if kind == "sabotage":
			module = load_module(root)
			wrap(module, dict(record), [], sabotage=key, cached=True)
			named = extra or words(stem, key)
		else:
			module = load_module(root, open(os.path.join(root, "tests/known/generate.py")).read().replace(extra[0], extra[1]))
			wrap(module, dict(record), [], cached=True)
			named = words(stem, key)
		status, out, err, tb = run(module, stem)
		what = "%s %s%s" % (kind, " ".join(map(str, key)), " (%s -> %s)" % extra if kind == "literal" else "")
		if tb or status == -1:
			fail("%s: the command crashes under %s, which is no refusal: %s" % (stem, what, tb.strip().splitlines()[-1] if tb else err[-200:]))
		if status != 1:
			fail("%s: the command exits %r under %s; it must refuse with 1 (R9, R5)" % (stem, status, what))
		if not all(re.search(r"(?<![0-9A-Za-z])%s(?![0-9A-Za-z])" % w, err) for w in named):
			fail("%s: under %s the command's message does not name the row (%s): %r" % (stem, what, " ".join(named), err.strip()[-200:]))
	print("%s: %d sabotaged answers and %d changed literals refused, each naming its row"
	      % (stem, sum(1 for r in runs if r[0] == "sabotage"), sum(1 for r in runs if r[0] == "literal")))


if __name__ == "__main__":
	a = sys.argv
	if a[1:2] == ["trace"]:
		trace(a[2], a[3], a[4], a[5])
	elif a[1:2] == ["refuse"]:
		refuse(a[2], a[3], a[4])
	else:
		fail("usage: command.py trace ROOT STEM RECORD TABLE | refuse ROOT STEM RECORD")
