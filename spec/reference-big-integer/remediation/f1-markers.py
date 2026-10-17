# F1 remediation check for reference-big-integer (a cycle record, not a standing gate). Python 3.8+,
# standard library only. Run from anywhere inside the tree:
#
#     python3 -I -B spec/reference-big-integer/remediation/f1-markers.py
#
# doc/README.md: "Each concrete number a note states, such as a worked example, a factor, a residue,
# a count or a published value, sits between two HTML comment markers." The cycle's docs gate
# (verify/docs.py, through notes.py) enforced that only on numbers of 100 or more. This check
# evaluates the rule over every line this cycle added to a note in doc/, at every magnitude:
#
#   Every numeral (notes.py's own tokenizer: 0 and 1 included, one-digit subscripts excluded), and
#   every number word from two to twelve, with twice and thrice (NUMBER_WORDS; "one" is left out, as
#   it is mostly a pronoun or an article, and so are ordinals), on an added line of a note stands
#   inside a check or cited span, or is not a concrete number the note states. Not a concrete number,
#   as doc/README.md now states Geoffrey's ruling (review cycle 3, escalation decision): definitions,
#   format constants and theorem values, the general results a proof establishes. A number stated
#   about a specific instance, or a restatement of a number the note marks elsewhere, is concrete. So:
#   - a citation's link text (its locator) or a link target, and inline code (it names code);
#   - a coefficient, exponent or bound inside math that has a free variable: part of a general
#     statement, which the note's proof proves and no finite check can evaluate. Math with no free
#     variable, a binding of one symbol to a closed value ($p = 3$), and a closed power inside any
#     math ($10^{38}$) are concrete and need a marker or an entry below;
#   - an entry of EXEMPT, by category, where the reading is a judgment: D, a definition, a format
#     constant or a design parameter of the code the note describes, not a value it computes; G, the
#     value of a statement that holds for every p, x or m, proved in the text; S, a label or a name.
#     No other category exists, and an entry with any other fails. No entry exempts a restatement of
#     a number the note marks elsewhere: the bound on long division's estimate is marked once, cited,
#     so its restatements stand in that cited span or say it without a number, and the chunk length
#     of decimal input is marked once, in the radix check, so the other lines point to it. A value measured on this code (a threshold, a time,
#     a ratio from a benchmark) has none: it is neither computed by a check nor published, so neither
#     marker can carry it, and the notes state none (review cycle 2, F1). A measured number that comes
#     back on an added line therefore fails as unmarked.
#
# Then every check span of those notes evaluates true under notes.py claims, as the gates run it.
# Notes are doc/*.md but the index README.md, which carries no claims (notes.py check_readme refuses
# markers there), and references.md, the bibliography, whose fields are the citations themselves.
# Prints `GATE FAIL: <reason>` for each failure and exits 1, or one green line.

import os
import re
import subprocess
import sys

ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=os.path.dirname(os.path.abspath(__file__)),
					  capture_output=True, text=True, check=True).stdout.strip()
BASE = "c9b82ff"    # the cycle's base, as verify/common.sh and verify/docs.py hold it
NOTES_DIR = os.path.join(ROOT, "spec", "math-notes-and-references", "verify")
sys.path.insert(0, NOTES_DIR)
import notes  # noqa: E402

NOT_NOTES = ("doc/README.md", "doc/references.md")
CATEGORIES = ("D", "G", "S")

# (file, phrase as the source writes it, occurrences on added lines, category, why)
EXEMPT = [
	("doc/lucas-lehmer.md", "from $s_0 = 4$", 1, "D", "the sequence's definition"),
	("doc/lucas-lehmer.md", "where it must end in 0,", 1, "G", "the theorem's verdict for every Mersenne prime"),
	("doc/prp-and-gerbicz.md", "type-1 residue", 2, "S", "the residue's name"),
	("doc/prp-and-gerbicz.md", "which is 1 for a probable prime", 1, "G", "Fermat's condition, for every p"),
	("doc/prp-and-gerbicz.md", "squarings from 3", 1, "D", "the test's base"),
	("doc/prp-and-gerbicz.md", "multiplication by 3", 1, "D", "the test's base"),
	("doc/prp-and-gerbicz.md", "of Theorem 2", 1, "S", "a cross-reference to the note's theorem"),
	("doc/prp-and-gerbicz.md", "congruent to 9 times", 1, "G", "3^(2^p) = 3^2 3^(2^p - 2), for every p"),
	("doc/prp-and-gerbicz.md", "a probable prime has $x_p = 9$", 1, "G", "Theorem 2's verdict, for every p"),
	("doc/prp-and-gerbicz.md", "where it must end in 1,", 1, "G", "Fermat's condition on every Mersenne prime"),
	("doc/reference-arithmetic.md", "little-endian 64-bit limbs", 1, "D", "the limb width"),
	("doc/reference-arithmetic.md", "$\\beta = 2^{64}$", 1, "D", "the radix's definition"),
	("doc/reference-arithmetic.md", "mapping $M_p$ to 0", 1, "G", "the last step of the fold, for every p"),
	("doc/reference-arithmetic.md", "is the low 64 bits of", 1, "D", "Res64's definition"),
	("doc/reference-arithmetic.md", "printed as 16 uppercase hexadecimal digits", 2, "D", "Res64's printed form"),
	("doc/reference-arithmetic.md", "It is 0 for $x = 0$", 1, "G", "Res64 of zero, for every p"),
	("doc/reference-arithmetic.md", "below 4 limbs", 1, "D", "the recursion's floor"),
	("doc/reference-arithmetic.md", "is 1 for $m > 1$, and every power is 0 modulo 1", 1, "G",
	 "the empty power and modulus 1, for every a"),
	("doc/reference-arithmetic.md", "the residue is 0;", 1, "G", "the fold's end at M_p, for every p"),
	("doc/reference-arithmetic.md", "so $h = 1$", 1, "G", "a case of the two-fold bound, for every p"),
	("doc/reference-arithmetic.md", "written in base 2", 1, "S", "the radix the fold is written in"),
	("doc/reference-arithmetic.md", "$(x \\bmod M_p) \\bmod 2^{64}$", 1, "D", "Res64's definition"),
	("doc/reference-arithmetic.md", "and 0 when the residue is 0, which covers $x = 0$", 1, "G", "Res64 of zero, for every p"),
	("doc/reference-arithmetic.md", "rather than 0 gives", 1, "G", "the same, for every p"),
	("doc/reference-arithmetic.md", "has $b_1 = 0$", 1, "G", "a case of the split, for every n"),
	("doc/reference-arithmetic.md", "so $e = 0$ is taken first", 1, "G", "a case of the powers, for every a and m"),
	("doc/reference-arithmetic.md", "modulo 1 every residue is 0, so $m = 1$", 1, "G", "modulus 1, for every a"),
	("doc/reference-arithmetic.md", "$x \\leftarrow x \\cdot 10^{19} + c$", 1, "D", "the chunk base"),
	("doc/reference-arithmetic.md", "Horner's rule in base $10^{19}$", 1, "D", "the chunk base"),
	("doc/reference-arithmetic.md", "Let $P_0 = 10^{19}$", 1, "D", "the first power's definition"),
	("doc/reference-arithmetic.md", "one division by $10^{19}$ per chunk", 1, "D", "the chunk base"),
	("doc/reference-arithmetic.md", "from $s_0 = 4$", 1, "D", "the sequence's definition"),
	("doc/reference-arithmetic.md", "the type-1 residue", 1, "S", "the residue's name"),
	("doc/reference-arithmetic.md", "squarings from 3", 1, "D", "the test's base"),
	("doc/reference-arithmetic.md", "multiplication by 3", 1, "D", "the test's base"),
	("doc/reference-arithmetic.md", "must end in 0 and 1", 1, "G", "the two theorems' verdicts on every Mersenne prime"),
	("doc/reference-arithmetic.md", "the 64-bit residue GIMPS records", 1, "S",
	 "Sources naming what a work holds; the note states the width in a cited span"),
	("doc/reference-arithmetic.md", "recursion at 4 limbs", 1, "D", "the recursion's floor, in Provenance"),
	("doc/reference-arithmetic.md", "it takes at most two folds", 1, "G", "the folding theorem's bound, for every p"),
	("doc/reference-arithmetic.md", "the remainder's top two limbs", 1, "D", "the estimate's definition"),
	("doc/reference-arithmetic.md", "At most two folds below", 1, "G", "the folding theorem's bound, as the proof's heading"),
	("doc/reference-arithmetic.md", "folds at most twice", 1, "G", "the same bound, for every squared residue"),
	("doc/reference-arithmetic.md", "the window's top two limbs", 1, "D", "the estimate's definition"),
	("doc/reference-arithmetic.md", "the three products", 1, "D", "Karatsuba's method, for every split"),
	("doc/reference-arithmetic.md", "the two reference tests", 1, "S", "names the two functions the sentence describes"),
	("doc/reference-arithmetic.md", "three half-size products", 1, "D", "Karatsuba's method, in Sources"),
	("doc/reference-arithmetic.md", "its two-fold bound", 1, "G", "the folding theorem's bound, in Provenance"),
]

GREEK = r"alpha|beta|gamma|delta|epsilon|zeta|eta|theta|kappa|lambda|mu|nu|xi|pi|rho|sigma|tau|phi|chi|psi|omega"
COMMAND = re.compile(r"\\(?!(?:%s)(?![A-Za-z]))[A-Za-z]+" % GREEK)
CLOSED_POWER = re.compile(r"(?<![0-9A-Za-z_}\\])[0-9]+\^(?:\{[0-9]+\}|[0-9](?![0-9]))")
NUMBER_WORDS = re.compile(r"(?<![A-Za-z0-9_-])(two|three|four|five|six|seven|eight|nine|ten|eleven|twelve|twice|thrice)"
						  r"(?![A-Za-z0-9_])", re.I)
BINDING = re.compile(r"\s*\\?[A-Za-z]+(?:_(?:\{[0-9A-Za-z]+\}|[0-9A-Za-z]))?\s*=\s*(.+?)\s*")

failures = []


def bad(msg):
	failures.append(msg)


def git(*args):
	return subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, text=True, check=True).stdout


def free_variable(math):
	return re.search(r"[A-Za-z]", COMMAND.sub(" ", math)) is not None


def added_lines():
	# {note: set of line numbers} for every note line the working tree adds over BASE
	out, path, n = {}, None, 0
	for l in git("diff", "-U0", "--no-color", "--no-ext-diff", BASE, "--", "doc").split("\n"):
		if l.startswith("+++ "):
			path = l[6:] if l.startswith("+++ b/") else None
		elif l.startswith("@@"):
			n = int(re.search(r"\+([0-9]+)", l).group(1))
		elif l.startswith("+") and path:
			out.setdefault(path, set()).add(n)
			n += 1
	for path in git("ls-files", "--others", "--exclude-standard", "--", "doc").split():
		out[path] = set(range(1, notes.read(path).count("\n") + 2))
	return {p: s for p, s in out.items() if p.endswith(".md") and p not in NOT_NOTES}


def words_on(text, lines):
	# the number words on the given lines of text
	return sum(len(NUMBER_WORDS.findall(l)) for k, l in enumerate(text.split("\n"), 1) if k in lines)


def judge(path, lines, exempt, stats):
	text = notes.read(path)
	marked = text
	for pat in (notes.CHECK, notes.CITED):
		marked = notes.blank_all(marked, pat)
	stats["in spans"] += sum(1 for k, _, _ in notes.tokens(notes.visible(text)) if k in lines) \
		- sum(1 for k, _, _ in notes.tokens(notes.visible(marked)) if k in lines)
	stats["words in spans"] += words_on(notes.visible(text), lines) - words_on(notes.visible(marked), lines)
	out = notes.blank_all(marked, notes.COMMENT)
	out = notes.visible(out)
	out = notes.blank_all(out, notes.INLINE_CODE)
	out = notes.fences(path, out)[0]
	for pat in (notes.DISPLAY_MATH, notes.INLINE_MATH):
		for m in reversed(list(pat.finditer(out))):
			body = m.group(0).strip("$")
			b = BINDING.fullmatch(body)
			if not free_variable(body) or (b and not free_variable(b.group(1))):
				continue
			keep = [(m.start() + c.start(), m.start() + c.end()) for c in CLOSED_POWER.finditer(m.group(0))]
			pos = m.start()
			for s, e in keep + [(m.end(), m.end())]:
				out = notes.blank(out, pos, s)
				pos = e
			if not keep and notes.line_of(text, m.start()) in lines:
				stats["general math"] += 1
	starts = [0] + [i + 1 for i, c in enumerate(text) if c == "\n"]
	for f, phrase, count, cat, why in exempt:
		if f != path:
			continue
		seen = 0
		for m in re.finditer(re.escape(phrase), text):
			k = notes.line_of(text, m.start())
			if k not in lines or text[m.start():m.end()] != marked[m.start():m.end()]:
				continue
			seen += 1
			out = notes.blank(out, m.start(), m.end())
		if seen != count:
			bad("%s: the exemption %r (%s, %s) matches %d added lines' text outside spans, not %d: stale"
				% (path, phrase, cat, why, seen, count))
		stats[cat] = stats.get(cat, 0) + seen    # an unknown category already failed in main
	raw = text.split("\n")
	for k, line in enumerate(out.split("\n"), 1):
		if k not in lines:
			continue
		for _, v, hexa in notes.tokens(line):
			at = [m.start() for m in notes.NUMBER.finditer(notes.blank_all(line, notes.SUBSCRIPT))
			      if m.group(1) in (str(v).lstrip("-"), hex(v)[2:].upper(), hex(v)[2:])]
			i = at[0] if at else 0
			bad("%s:%d: the number %s stands outside a check or cited span (doc/README.md): ...%s..."
				% (path, k, hex(v) if hexa else v, raw[k - 1][max(0, i - 50):i + 30]))
		for m in NUMBER_WORDS.finditer(line):
			bad("%s:%d: the number word %r stands outside a check or cited span (doc/README.md): ...%s..."
				% (path, k, m.group(1), raw[k - 1][max(0, m.start() - 50):m.start() + 30]))


def main():
	try:
		git("cat-file", "-e", BASE + "^{commit}")
		git("merge-base", "--is-ancestor", BASE, "HEAD")
	except subprocess.CalledProcessError:
		print("GATE FAIL: the base %s is not an ancestor of HEAD" % BASE, file=sys.stderr)
		return 1
	added = added_lines()
	if not added or not sum(len(s) for s in added.values()):
		print("GATE FAIL: no added note lines found over %s: nothing evaluated" % BASE, file=sys.stderr)
		return 1
	stats = {"in spans": 0, "words in spans": 0, "general math": 0, "D": 0, "G": 0, "S": 0}
	for f, phrase, _, cat, _ in EXEMPT:
		if f not in added:
			bad("%s: an exemption names a note this cycle does not change" % f)
		if cat not in CATEGORIES:
			bad("%s: the exemption %r has category %r, not one of %s: a measured value takes no exemption"
				% (f, phrase, cat, ", ".join(CATEGORIES)))
	try:
		for path in sorted(added):
			judge(path, added[path], EXEMPT, stats)
	except notes.Fail as e:
		bad(str(e))
	if not stats["in spans"]:
		bad("no numeral on an added line stands in a span: the spans were not seen")
	if not stats["words in spans"]:
		bad("no number word on an added line stands in a span: the spans were not seen")
	r = subprocess.run([sys.executable, "-I", "-B", os.path.join(NOTES_DIR, "notes.py"), "claims"] + sorted(added),
					   cwd=ROOT, capture_output=True, text=True)
	checks = [l for l in r.stdout.split("\n") if l.startswith("doc/")]
	if r.returncode != 0:
		bad("notes.py claims: %s" % (r.stderr.strip() or "exit %d" % r.returncode))
	elif not checks:
		bad("notes.py claims evaluated no check in %s" % ", ".join(sorted(added)))
	for msg in failures:
		print("GATE FAIL: %s" % msg, file=sys.stderr)
	if failures:
		return 1
	print("f1-markers: green (%d notes, %d added lines; numerals in spans %d, number words in spans %d, outside only as "
		  "general math in %d spans and %d exemptions: D %d, G %d, S %d; notes.py claims: %d checks true)"
		  % (len(added), sum(len(s) for s in added.values()), stats["in spans"], stats["words in spans"],
			 stats["general math"],
			 stats["D"] + stats["G"] + stats["S"], stats["D"], stats["G"], stats["S"], len(checks)))
	return 0


if __name__ == "__main__":
	sys.exit(main())
