# The doc/ gate for math-notes-and-references (a cycle record, not a standing gate). Python 3.8+,
# standard library only. Run from anywhere inside the tree:
#
#     python3 -I -B spec/math-notes-and-references/verify/notes.py gate N     # phase N's checks
#     python3 -I -B spec/math-notes-and-references/verify/notes.py report     # the claims record
#     python3 -I -B spec/math-notes-and-references/verify/notes.py report --write
#
# `gate N` checks the files, the structure, the citations, the claims and the clean-room rules
# for every note up to phase N (PLAN § 2, § 6), and that the committed records under evidence/
# match what it computes. It prints `GATE FAIL: <reason>` and exits 1 on the first failure.

import datetime
import os
import re
import subprocess
import sys
import unicodedata

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import claims  # noqa: E402

SLUG = "math-notes-and-references"
BASE = "61f5ce7"
GOAL_DATE = "2026-10-12"
ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=os.path.dirname(os.path.abspath(__file__)),
                      capture_output=True, text=True, check=True).stdout.strip()
SPEC = "spec/" + SLUG
EVIDENCE = os.path.join(ROOT, SPEC, "evidence")
REPORT_COMMAND = "python3 -I -B %s/verify/notes.py report" % SPEC

# The notes, the phase that lands each, and what its "What mps uses" section must name
# (backticked) and say. PLAN § 2.
NOTES = [
	("mersenne-numbers.md", 1, ["src/math.cpp", "check_exponent", "composite_by_exponent", "smallest_prime_factor"], []),
	("lucas-lehmer.md", 2, ["src/math.cpp", "test_lucas_lehmer"], ["stub", "decides nothing"]),
	("prp-and-gerbicz.md", 2, [], ["does not exist yet"]),
	("factors-and-trial-factoring.md", 3, ["src/math.cpp", "test_quick_factor"], ["stub", "decides nothing"]),
	("p-1.md", 3, ["src/math.cpp", "test_p_minus_one"], ["stub", "decides nothing"]),
	("weighted-transforms.md", 4, [], ["does not exist yet"]),
	("number-theoretic-transforms.md", 4, [], ["does not exist yet"]),
]
SECTIONS = ["The result", "Proof", "What mps uses", "Sources", "Provenance"]
OWN = "(the project's own derivation)"

# R6: the seeds' reference lists, the starting set. Each is in doc/references.md by the last
# phase unless PLAN § 4 dropped it (DROPPED, with the reason there). Matched by DOI when the
# seed gave one, otherwise by title.
STARTING_SET = [
	("Crandall and Fagin 1994", "10.1090/s0025-5718-1994-1185244-1", None),
	("Percival 2003", "10.1090/s0025-5718-02-01419-9", None),
	("Crandall, Mayer and Papadopoulos 2003", "10.1090/s0025-5718-02-01479-5", None),
	("Sorensen et al. 1987", None, "Real-valued fast Fourier transform algorithms"),
	("Lehmer 1930", "10.2307/1968235", None),
	("Lehmer 1935", "10.1112/jlms/s1-10.2.162", None),
	("Bruce 1993", "10.2307/2324959", None),
	("GIMPS, The Math", None, "The Math"),
	("Prime95 undoc.txt", None, None),
	("Prime95 readme.txt", None, None),
	("Li and Gallot 2022", None, "An efficient modular exponentiation proof scheme"),
	("Pietrzak 2019", "10.4230/lipics.itcs.2019.60", None),
	("Wesolowski 2020", "10.1007/s00145-020-09364-x", None),
	("Pollard 1974", "10.1017/s0305004100049252", None),
	("Montgomery 1987", "10.1090/s0025-5718-1987-0866113-7", None),
	("Montgomery and Silverman 1990", "10.2307/2008514", None),
	("Montgomery and Kruppa 2008", "10.1007/978-3-540-79456-1_12", None),
	("Schonhage 1971", None, None),
	("Moller 2008", "10.1090/s0025-5718-07-02017-0", None),
	("Stehle and Zimmermann 2004", None, None),
	("Brent and Zimmermann 2010", None, "algorithm for the Jacobi symbol"),
	("Pollard 1971", None, "The fast Fourier transform in a finite field"),
	("Reed and Truong 1975", None, "The use of finite fields to compute convolutions"),
	("Miller, Reed and Truong 1981", None, "A theorem for computing primitive elements in the field of complex integers of a characteristic Mersenne prime"),
	("Harvey 2014", None, "Faster arithmetic for number-theoretic transforms"),
	("Montgomery 1985", None, "Modular multiplication without trial division"),
	("Barrett 1987", None, "Implementing the Rivest Shamir and Adleman public key encryption algorithm on a standard digital signal processor"),
	("Crandall and Pomerance 2005", None, "Prime Numbers: A Computational Perspective"),
	("mfaktc README", None, None),
	("mfakto README", None, None),
]
DROPPED = {"Prime95 undoc.txt", "Prime95 readme.txt", "Wesolowski 2020", "Schonhage 1971",
           "Stehle and Zimmermann 2004", "mfaktc README", "mfakto README"}

PROGRAMS = re.compile(r"\b(prime95|mprime|mlucas|gpuowl|prpll|cudalucas|mfaktc|mfakto|gmp|gmp-ecm|gwnum|glucas|"
                      r"llr|pfgw|genefer|y-cruncher|cllucas)\b", re.I)
C_ISMS = re.compile(r"#\s*include|\b(u?int\d+_t|size_t|void|typedef|template|struct|unsigned|__global__|"
                    r"__device__|__m\d+\w*|std)\b|->|::|\+\+|/\*|\*/|(^|\s)//|\)\s*\{|^\s*[{}]\s*$|;\s*$|"
                    r"\b\w+\[[^\]]*\]\s*=|\b(mulmod|umulh|__builtin_\w+)\b")
C_PROSE = re.compile(r"#\s*include|->|::|/\*|\*/|(?<!C)\+\+|\)\s*\{|\b(u?int\d+_t|size_t|typedef|__global__|__device__|"
                     r"__builtin_\w+|unsigned)\b|\breturn\s+[^.;]*;|\bfor\s*\([^)]*;|\bwhile\s*\([^)]*\)\s*[{;]|"
                     r">>=|<<=|\+=|-=|\*=|/=|%=|&=|\|=|\b[A-Za-z_]\w*(\[[^\]]*\])?\s*=\s*[^=;]*;|"
                     r"\(\s*(unsigned|int|long|short|double|float|char|u?int\d+_t)\b[^)]*\)\s*[\w(]")
# inline code holds names only: identifiers, paths, options, commands
CODE_WORD = re.compile(r"--?[A-Za-z][A-Za-z0-9-]*(=[A-Za-z0-9_./-]+)?|[A-Za-z0-9_./:#+-]+(\(\))?")
LOCATOR_PART = re.compile(r"(§\s*[0-9][0-9.]*|(p|pp)\.\s*[0-9ivxlc]+([–-][0-9ivxlc]+)?|(Theorem|Lemma|Corollary|"
                          r"Proposition|Definition|Section|Chapter|Equation|Table|Algorithm|Example|Exercise|Remark|"
                          r"Figure|Appendix|Step|Property|Fact|eq\.|Eq\.|ch\.|Ch\.|Thm\.|Prop\.|Sec\.|Fig\.|Alg\.)"
                          r"\s*[0-9A-Za-z(][0-9A-Za-z.()]*([–-][0-9A-Za-z.()]+)?|the data section|the abstract)")
BAD_HOSTS = re.compile(r"https?://([^/\s>)]*\.)?(github\.com|githubusercontent\.com|gitlab\.com|sourceforge\.net|"
                       r"bitbucket\.org|codeberg\.org|sr\.ht)\b", re.I)
BAD_PATHS = re.compile(r"\.(c|cc|cpp|cxx|h|hh|hpp|cu|cuh|cl|asm|s|mac|zip|tar|gz|tgz|bz2|xz|7z)([?#>)\s]|$)", re.I)


class Fail(Exception):
	pass


def fail(msg):
	raise Fail(msg)


def read(rel):
	with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
		return f.read()


def norm(s):
	s = unicodedata.normalize("NFKD", s)
	s = "".join(c for c in s if not unicodedata.combining(c))
	s = s.replace("\u2212", "-").replace("\u2013", "-").replace("\u2014", "-")
	return re.sub(r"[^a-z0-9]+", " ", s.lower()).strip()


def squash(s):
	return norm(s).replace(" ", "")


# --- Markdown scanning --------------------------------------------------------------------------

CITE = re.compile(r"\[([^\]\n]+)\]\(references\.md#([A-Za-z0-9_-]+)\)")
LINK = re.compile(r"(!?)\[([^\]\n]*)\]\(([^)\s]*)\)")
CHECK = re.compile(r"<!--\s*check:(.*?)-->(.*?)<!--\s*/check\s*-->", re.S)
CITED = re.compile(r"<!--\s*cited\s*-->(.*?)<!--\s*/cited\s*-->", re.S)
COMMENT = re.compile(r"<!--.*?-->", re.S)
FENCE = re.compile(r"^(```|~~~)(.*)$")


def blank(text, start, end):
	# keep line structure: replace a span with spaces, newlines kept
	return text[:start] + re.sub(r"[^\n]", " ", text[start:end]) + text[end:]


def blank_all(text, pattern, group=0):
	for m in reversed(list(pattern.finditer(text))):
		text = blank(text, m.start(group), m.end(group))
	return text


def line_of(text, pos):
	return text.count("\n", 0, pos) + 1


def fences(path, text):
	# returns text with fenced blocks blanked, and the blocks as (line, info, body)
	lines = text.split("\n")
	out, blocks, i = [], [], 0
	while i < len(lines):
		m = FENCE.match(lines[i].strip())
		if m and lines[i].startswith(m.group(1)):
			info = m.group(2).strip()
			j = i + 1
			while j < len(lines) and lines[j].strip() != m.group(1):
				j += 1
			if j == len(lines):
				fail("%s:%d: unclosed code fence" % (path, i + 1))
			blocks.append((i + 1, info, "\n".join(lines[i + 1:j])))
			out.extend([""] * (j - i + 1))
			i = j + 1
		else:
			out.append(lines[i])
			i += 1
	return "\n".join(out), blocks


INLINE_CODE = re.compile(r"(?<!`)`[^`\n]+`(?!`)")
DISPLAY_MATH = re.compile(r"\$\$.*?\$\$", re.S)
INLINE_MATH = re.compile(r"(?<![\\$])\$(?!\$)[^$\n]+?(?<![\\$])\$(?!\$)")


def mask_math(path, text):
	# blank display and inline math, after checking the delimiters (R1)
	m = re.search(r"(?<!\\)\\[()\[\]]", text)
	if m:
		fail("%s:%d: math delimited by \\( \\) or \\[ \\]; R1 allows only $...$ and $$...$$"
		     % (path, line_of(text, m.start())))
	text = blank_all(text, DISPLAY_MATH)
	text = blank_all(text, INLINE_MATH)
	m = re.search(r"(?<!\\)\$", text)
	if m:
		fail("%s:%d: unbalanced or multi-line $ math" % (path, line_of(text, m.start())))
	return text


# --- references.md ------------------------------------------------------------------------------

FIELDS = ["Authors", "Title", "Venue", "Year", "DOI|URL", "Verified"]


def parse_references(today):
	path = "doc/references.md"
	text = read(path)
	if not text.startswith("# References\n"):
		fail("%s: must open with '# References'" % path)
	if re.search(r"^## ", text, re.M):
		fail("%s: entries are '### Key' headings; no '##' sections" % path)
	entries = {}
	parts = re.split(r"^### ", text, flags=re.M)
	for part in parts[1:]:
		head, _, body = part.partition("\n")
		key = head.strip()
		if not re.fullmatch(r"[A-Z][A-Za-z0-9-]*", key):
			fail("%s: bad key %r (letters, digits and '-', starting with a capital)" % (path, key))
		if key.lower() in (k.lower() for k in entries):
			fail("%s: duplicate key %s" % (path, key))
		for l in body.split("\n"):
			if l.strip() and not re.match(r"^- \*\*[A-Za-z]+:\*\* \S", l):
				fail("%s: entry %s holds a line that is not one of its fields: %s" % (path, key, l.strip()[:60]))
		fields = re.findall(r"^- \*\*([A-Za-z]+):\*\* (.+)$", body, re.M)
		names = [f for f, _ in fields]
		want = ["Authors", "Title", "Venue", "Year", "DOI" if "DOI" in names else "URL", "Verified"]
		if names != want:
			fail("%s: entry %s has fields %s; want %s" % (path, key, names, want))
		e = dict(fields)
		e["key"] = key
		if not re.fullmatch(r"\d{4}", e["Year"]):
			fail("%s: entry %s: Year must be four digits" % (path, key))
		if "DOI" in e:
			m = re.fullmatch(r"\[(10\.\d{4,9}/\S+)\]\(https://doi\.org/(10\.\d{4,9}/\S+)\)", e["DOI"])
			if not m or m.group(1) != m.group(2):
				fail("%s: entry %s: DOI must read [10.x/y](https://doi.org/10.x/y), the same DOI twice" % (path, key))
			e["doi"] = m.group(1)
		else:
			m = re.fullmatch(r"<(https://[^>\s]+)>", e["URL"])
			if not m:
				fail("%s: entry %s: URL must read <https://...>" % (path, key))
			e["url"] = m.group(1)
		v = re.match(r"(\d{4}-\d{2}-\d{2})\b", e["Verified"])
		if not v or not (GOAL_DATE <= v.group(1) <= today) or " against " not in e["Verified"]:
			fail("%s: entry %s: Verified must start with a date from %s to %s and say what it was checked against"
			     % (path, key, GOAL_DATE, today))
		entries[key] = e
	if not entries:
		fail("%s: no entries" % path)
	for m in re.finditer(r"https?://[^\s>)]+", text):
		if BAD_HOSTS.match(m.group(0)) or BAD_PATHS.search(m.group(0)):
			fail("%s:%d: %s points at a code host or a source or archive file (R8)"
			     % (path, line_of(text, m.start()), m.group(0)))
	scan_hidden(path, text, allow_markers=False)
	body, blocks = fences(path, text)
	if blocks:
		fail("%s:%d: no code fences in the bibliography" % (path, blocks[0][0]))
	t = blank_all(blank_all(body, LINK, 2), re.compile(r"https?://[^\s>)]+"))
	check_math_syntax(path, t)
	for k, l in enumerate(mask_math(path, blank_all(t, INLINE_CODE)).split("\n"), 1):
		if C_PROSE.search(l):
			fail("%s:%d: line reads as C or C++: %s" % (path, k, l.strip()))
	return entries


# --- Checks shared by every doc file ------------------------------------------------------------

def scan_hidden(path, text, allow_markers):
	# HTML: only the check and cited markers, and no other tags (R8: nothing hidden)
	for m in COMMENT.finditer(text):
		c = m.group(0)
		if not allow_markers or not re.fullmatch(r"<!--\s*(check:.*?|/check|cited|/cited)\s*-->", c, re.S):
			fail("%s:%d: HTML comment not allowed here: %s" % (path, line_of(text, m.start()), c[:60]))
	t = blank_all(text, COMMENT)
	for m in INLINE_CODE.finditer(fences(path, t)[0]):
		words = m.group(0)[1:-1].split(" ")
		if not 1 <= len(words) <= 6 or not all(CODE_WORD.fullmatch(w) for w in words):
			fail("%s:%d: inline code names code (an identifier, path, option or command), never writes it: %s"
			     % (path, line_of(t, m.start()), m.group(0)))
	m = re.search(r"^ {0,3}\[[^\]\n]+\]:", t, re.M) or re.search(r"&#?[A-Za-z0-9]+;", t)
	if m:
		fail("%s:%d: link-reference definitions and HTML entities are not allowed" % (path, line_of(t, m.start())))
	t = blank_all(t, INLINE_CODE)
	t = re.sub(r"<https://[^>\s]+>", "", t)
	m = re.search(r"<[A-Za-z/!?]", t)
	if m:
		fail("%s:%d: raw HTML not allowed" % (path, line_of(t, m.start())))


def check_clean_room(path, text, blocks, is_note):
	# R8: no code from another program, in any form the notes can carry
	for line, info, body in blocks:
		if info != "text":
			fail("%s:%d: code fence '%s'; only ```text pseudocode is allowed (R8)" % (path, line, info))
	for line, info, body in blocks:
		for k, l in enumerate(body.split("\n")):
			if C_ISMS.search(l):
				fail("%s:%d: pseudocode line reads as C or C++: %s" % (path, line + 1 + k, l.strip()))
	plain = blank_all(blank_all(text, COMMENT), INLINE_CODE)
	prose = mask_math(path, fences(path, plain)[0])
	for k, l in enumerate(prose.split("\n")):
		if C_PROSE.search(l):
			fail("%s:%d: line reads as C or C++: %s" % (path, k + 1, l.strip()))
	m = re.search(r"https?://|\bwww\.|\b[a-z0-9-]+(\.[a-z0-9-]+)*\.(com|org|net|io|dev|edu|ht)\b", blank_all(text, INLINE_CODE), re.I)
	if path != "doc/references.md" and m:
		fail("%s:%d: external addresses belong only in doc/references.md" % (path, line_of(text, m.start())))
	if is_note:
		if re.search(r"source[ -]code", plain, re.I):
			fail("%s: says 'source code'; the notes cite papers and documentation only (R8)" % path)
		m = re.search(r"\b[\w-]+\.(c|cc|cpp|cxx|h|hpp|cu|cuh|cl|asm|mac)\b", plain)
		if m:
			fail("%s:%d: names a source file outside backticks: %s" % (path, line_of(plain, m.start()), m.group(0)))
	for para in re.split(r"\n\s*\n", text):
		if PROGRAMS.search(blank_all(para, INLINE_CODE)) and not CITE.search(para):
			fail("%s: a paragraph names %s without citing its documentation: %s"
			     % (path, PROGRAMS.search(para).group(0), para.strip()[:80]))


def check_math_syntax(path, text):
	# R1: math only inside $...$ and $$...$$; nothing LaTeX-like outside it
	t = blank_all(text, INLINE_CODE)
	t = blank_all(t, COMMENT)
	t = mask_math(path, t)
	t = blank_all(t, LINK, 3)
	if "```math" in text:
		fail("%s: ```math blocks are not allowed (R1)" % path)
	for k, l in enumerate(t.split("\n")):
		m = re.search(r"\\[A-Za-z]+|\^|_\{|[A-Za-z0-9]_[A-Za-z0-9{]", l)
		if m:
			fail("%s:%d: math or a code name outside $...$ or backticks: %r" % (path, k + 1, l.strip()[:80]))


# --- Citations ----------------------------------------------------------------------------------

def citations(path, text, entries):
	out = []
	for m in CITE.finditer(text):
		label, anchor = m.group(1), m.group(2)
		key, _, locator = label.partition(",")
		key, locator = key.strip(), locator.strip()
		if key not in entries:
			fail("%s:%d: cites %s, which has no entry in doc/references.md" % (path, line_of(text, m.start()), key))
		if anchor != key.lower():
			fail("%s:%d: [%s] links to #%s; the anchor is #%s" % (path, line_of(text, m.start()), key, anchor, key.lower()))
		if "," in label:
			parts = [x.strip() for x in re.split(r",|\band\b", locator)]
			if not all(LOCATOR_PART.fullmatch(x) for x in parts):
				fail("%s:%d: locator %r: write § 1.3, p. 12, pp. 12–14, Theorem 5.4, eq. (3), Table 2 and the like"
				     % (path, line_of(text, m.start()), locator))
		out.append((key, locator, line_of(text, m.start()), m))
	t = blank_all(text, CITE)
	m = re.search(r"\[([A-Z][A-Za-z-]*[0-9][A-Za-z0-9-]*)(,[^\]]*)?\](?!\()", t)
	if m:
		fail("%s:%d: [%s] is not a link to its doc/references.md entry" % (path, line_of(t, m.start()), m.group(1)))
	return out


def check_links(path, text):
	here = os.path.dirname(path)
	for m in LINK.finditer(text):
		bang, target = m.group(1), m.group(3)
		if bang:
			fail("%s:%d: images are not allowed" % (path, line_of(text, m.start())))
		if target.startswith("references.md#") or target.startswith("https://"):
			continue
		file = target.split("#")[0]
		rel = os.path.normpath(os.path.join(here, file))
		if not file or not os.path.isfile(os.path.join(ROOT, rel)):
			fail("%s:%d: link to %s, which is not a file in the tree" % (path, line_of(text, m.start()), target))


# --- Claims -------------------------------------------------------------------------------------

SUBSCRIPT = re.compile(r"(?<=_)(\{\d\}|\d)(?![0-9])")
NUMBER = re.compile(r"(?<![0-9A-Za-z_])(0[xX][0-9A-Fa-f]+|[0-9A-Fa-f]{16}|[0-9]+)(?![0-9A-Za-z_])")
SIGN_CONTEXT = " \t([{=,$>"


def tokens(text):
	# The integers a text shows, line by line, as (line, value, is_hex). Unicode digits are folded
	# (NFKC). A sign belongs to its number when it touches the digits and follows a space, an
	# opening bracket, =, a comma, $ or the start: "$= -1$" is -1, "$2^p - 1$" is 1. A one-digit
	# subscript names a variable (s_0, B_1) and is not a number.
	out = []
	for k, line in enumerate(text.split("\n"), 1):
		line = unicodedata.normalize("NFKC", line).replace("\u2212", "-")
		line = blank_all(line, SUBSCRIPT)
		for m in NUMBER.finditer(line):
			s, hexa = m.group(1), False
			if s[:2] in ("0x", "0X"):
				v, hexa = int(s, 16), True
			elif len(s) == 16 and re.search(r"[A-Fa-f]", s):
				if not re.search(r"[0-9]", s):
					continue
				v, hexa = int(s, 16), True
			elif re.fullmatch(r"[0-9]+", s):
				v = int(s)
			else:
				continue
			i = m.start()
			if i > 0 and line[i - 1] in "-+" and (i == 1 or line[i - 2] in SIGN_CONTEXT):
				v = -v if line[i - 1] == "-" else v
			out.append((k, v, hexa))
	return out


def visible(text):
	# what a span or a note shows as numbers: citations' link texts and every link target are
	# blanked, inline code and fences are not
	t = blank_all(text, CITE)
	return blank_all(t, LINK, 3)


def judge_claims(path, text, results):
	# text is the whole file, fences included
	for m in CHECK.finditer(text):
		expr, span = m.group(1), m.group(2)
		line = line_of(text, m.start())
		where = "%s:%d" % (path, line)
		if "<!--" in span or "\n\n" in span:
			fail("%s: a check span holds one statement: no nested marker, no blank line" % where)
		if not span.strip():
			fail("%s: empty check span" % where)
		try:
			ev = claims.Evaluation(expr)
		except claims.ClaimError as e:
			fail("%s: check %s: %s" % (where, " ".join(expr.split()), e))
		toks = [v for _, v, _ in tokens(visible(span))]
		known = ev.literals | ev.values
		stray = [t for t in toks if t not in known]
		if stray:
			fail("%s: the text shows %s, which the check neither writes nor computes" % (where, stray))
		missing = sorted(x for x in ev.literals if x not in toks and x not in (0, 1))
		if missing:
			fail("%s: the check writes %s, which the text does not show" % (where, missing))
		results.append((path, line, " ".join(expr.split()), ev, span))
	for m in CITED.finditer(text):
		span = m.group(1)
		if "<!--" in span:
			fail("%s:%d: a cited span holds no other marker" % (path, line_of(text, m.start())))
		if not any(c.group(1).count(",") for c in CITE.finditer(span)):
			fail("%s:%d: a cited span needs a citation with a locator, [Key, locator](references.md#key)"
			     % (path, line_of(text, m.start())))
	stripped = text
	for pat in (CHECK, CITED):
		stripped = blank_all(stripped, pat)
	scan_hidden(path, stripped, allow_markers=False)
	for k, v, hexa in tokens(visible(stripped)):
		if hexa or abs(v) >= 100:
			fail("%s:%d: the number %s stands outside a check or cited span (R7)" % (path, k, hex(v) if hexa else v))


def required_checks(path, results):
	mine = [r for r in results if r[0] == path]
	calls = [(name, args, v, r) for r in mine for (name, args, v) in r[3].calls]
	name = os.path.basename(path)

	def need(fn, what):
		if not any(c[0] == fn for c in calls):
			fail("%s: R7 needs a check calling %s (%s)" % (path, fn, what))

	if name == "mersenne-numbers.md" and not any(abs(v) >= 10 for r in mine for _, v, _ in tokens(visible(r[4]))):
		fail("%s: needs a worked example: a check span showing a number of 10 or more (R7)" % path)
	if name == "lucas-lehmer.md":
		need("ll_exponents", "the Lucas-Lehmer test on every prime exponent up to %d at least" % claims.MIN_LL_BOUND)
		need("jacobi_sweep", "the Jacobi invariant for every odd prime p up to %d at least" % claims.MIN_JACOBI_BOUND)
		need("jacobi_first_zero", "the zero cases on odd composite exponents up to %d at least" % claims.MIN_FIRST_ZERO_BOUND)
		for c in calls:
			if c[0] == "ll_exponents" and "](references.md#oeis-a000043)" not in c[3][4]:
				fail("%s: the ll_exponents check compares with the published exponents; its span cites OEIS-A000043" % path)
	if name == "prp-and-gerbicz.md":
		need("prp3_known", "the base-3 Fermat PRP property on the known Mersenne primes up to %d at least" % claims.MIN_PRP_BOUND)
		for c in calls:
			if c[0] == "prp3_known" and "](references.md#oeis-a000043)" not in c[3][4]:
				fail("%s: the prp3_known check uses the published exponents; its span cites OEIS-A000043" % path)
	if name == "factors-and-trial-factoring.md":
		need("factor_form", "the form on every factor up to 2^20 of M_p, odd prime p up to 100")
		ok = False
		for r in mine:
			cs = r[3].calls
			for (fn, args, v) in cs:
				if fn == "has_factor_form" and v is True and args[0] > 1 << 20:
					q, p = args
					if (("divides_mersenne", (q, p), True) in cs and ("isprime", (q,), True) in cs
					        and any(x.group(1).count(",") for x in CITE.finditer(r[4]))):
						ok = True
		if not ok:
			fail("%s: R7 needs a published factor q > 2^20 of some M_p, checked in one span with "
			     "isprime(q), divides_mersenne(q, p) and has_factor_form(q, p), citing its source with a locator" % path)
	if name == "p-1.md":
		ok = False
		for (fn, args, v, r) in calls:
			if fn == "pm1":
				M = (1 << args[0]) - 1
				if any(1 < g < M for g in v):
					ok = True
		if not ok:
			fail("%s: R7 needs one worked P-1 example: a check calling pm1(p, B1, B2) that finds a proper factor" % path)


def describe(ev):
	out = []
	for (fn, args, v) in ev.calls:
		if fn == "ll_exponents":
			found, composite = claims._ll_cache[args[0]]
			out.append("    ll_exponents(%d): M_p prime for p in %s (p = 2 by M_2 = 3; the rest by a zero "
			           "Lucas-Lehmer residue); composite for the other %d prime p <= %d, each by a nonzero "
			           "residue and again by a failed base-3 Fermat test; equal to OEIS A000043 up to %d"
			           % (args[0], list(found), composite, args[0], args[0]))
		elif fn == "jacobi_sweep":
			out.append("    jacobi_sweep(%d): every odd prime p <= %d, every iteration 1 <= i <= p - 2, unshifted "
			           "and with three shifts; cases where the symbol is not -1: %s"
			           % (args[0], args[0], "none" if not v else
			              ", ".join("(p=%d, i=%d): 0, excluded (some s_j, j <= i - 2, shares a factor with M_p)" % c for c in v)))
		elif fn == "jacobi_first_zero":
			out.append("    jacobi_first_zero(%d): every odd p <= %d, prime or not; first 0 at (p, i): %s; every "
			           "such case excluded by the hypothesis" % (args[0], args[0], list(v)))
		elif fn == "prp3_known":
			out.append("    prp3_known(%d): 3^(M_p - 1) = 1 mod M_p for all %d published odd exponents p <= %d" % (args[0], v, args[0]))
		elif fn == "factor_form":
			out.append("    factor_form(%d, %d): %d prime factors q <= %d of M_p, odd prime p <= %d, found by trying every "
			           "prime; each is 2kp + 1 and +-1 mod 8" % (args[0], args[1], v, args[1], args[0]))
		elif fn == "pm1":
			out.append("    pm1%r: stage 1 gcd %d, stage 2 gcd %d" % (args, v[0], v[1]))
	return out


# --- Notes and README -------------------------------------------------------------------------

def sections(path, text):
	lines = text.split("\n")
	first = next((l for l in lines if l.strip()), "")
	if not first.startswith("# "):
		fail("%s: must open with a '# ' title" % path)
	if sum(1 for l in lines if re.match(r"^# ", l)) != 1:
		fail("%s: exactly one '# ' title" % path)
	heads = [(i, l[3:].strip()) for i, l in enumerate(lines) if l.startswith("## ")]
	if [h for _, h in heads] != SECTIONS:
		fail("%s: its '##' sections are %s; R2 wants exactly %s, in order" % (path, [h for _, h in heads], SECTIONS))
	out = {}
	for n, (i, h) in enumerate(heads):
		end = heads[n + 1][0] if n + 1 < len(heads) else len(lines)
		body = "\n".join(lines[i + 1:end])
		if not re.search(r"^(?!#)\s*\S", body, re.M):
			fail("%s: section '%s' is empty" % (path, h))
		out[h] = body
	return out


def check_note(path, uses, phrases, entries, results):
	text = read(path)
	body, blocks = fences(path, text)
	check_clean_room(path, text, blocks, True)
	check_math_syntax(path, body)
	scan_hidden(path, body, allow_markers=True)
	secs = sections(path, body)
	cites = citations(path, body, entries)
	check_links(path, body)
	keys = {c[0] for c in cites}
	src_keys = {c[0] for c in citations(path, secs["Sources"], entries)}
	if not src_keys:
		fail("%s: 'Sources' cites nothing" % path)
	if keys - src_keys:
		fail("%s: cites %s outside 'Sources' without listing them there" % (path, sorted(keys - src_keys)))
	if not citations(path, secs["Provenance"], entries):
		fail("%s: 'Provenance' cites nothing" % path)
	u = secs["What mps uses"]
	ticked = set(re.findall(r"`([^`\n]+)`", u))
	for n in uses:
		if n not in ticked:
			fail("%s: 'What mps uses' must name `%s` (R2)" % (path, n))
	for ph in phrases:
		if ph not in u:
			fail("%s: 'What mps uses' must say '%s' (R2)" % (path, ph))
	src = "\n".join(read("src/" + f) for f in sorted(os.listdir(os.path.join(ROOT, "src"))) if f.endswith((".cpp", ".hpp")))
	for t in ticked:
		if t.startswith("src/") and not os.path.isfile(os.path.join(ROOT, t)):
			fail("%s: names `%s`, which does not exist" % (path, t))
		if re.fullmatch(r"[a-z][a-z0-9]*(_[a-z0-9]+)+", t) and not re.search(r"\b%s\s*\(" % t, src):
			fail("%s: names `%s` as code, but src/ defines no such function" % (path, t))
	for m in re.finditer(r"^### (.*)$", body, re.M):
		if "project's own" in m.group(1) and not m.group(1).endswith(OWN):
			fail("%s: a project derivation's heading ends with '%s'" % (path, OWN))
	if os.path.basename(path) == "lucas-lehmer.md":
		proof = secs["Proof"]
		m = re.search(r"^### .*%s\s*$" % re.escape(OWN), proof, re.M)
		if not m:
			fail("%s: 'Proof' needs a '### ... %s' subsection for the Jacobi invariant (R4)" % (path, OWN))
		sub = proof[m.end():]
		sub = sub[:re.search(r"^### ", sub, re.M).start()] if re.search(r"^### ", sub, re.M) else sub
		for label in ("**Hypotheses.**", "**What the symbol means for a run.**"):
			if label not in sub:
				fail("%s: the derivation needs a paragraph opening %s (R4)" % (path, label))
	judge_claims(path, text, results)
	return cites


def check_readme(present, entries):
	path = "doc/README.md"
	text = read(path)
	body, blocks = fences(path, text)
	check_clean_room(path, text, blocks, False)
	check_math_syntax(path, body)
	scan_hidden(path, body, allow_markers=False)
	check_links(path, body)
	if not body.startswith("# "):
		fail("%s: must open with a '# ' title" % path)
	for name in present:
		n = len(re.findall(r"^\s*[-*] .*\]\(%s\)" % re.escape(name), body, re.M))
		if n != 1:
			fail("%s: lists %s %d times; R1 wants one line with a link" % (path, name, n))
	for m in LINK.finditer(body):
		t = m.group(3)
		if t.endswith(".md") and "/" not in t and t not in present + ["references.md"]:
			fail("%s: links %s, which this phase does not hold" % (path, t))
	if not re.search(r"\]\(\.\./AGENTS\.md(#[^)]*)?\)", body) or "Invariants 4" not in body:
		fail("%s: the clean-room sentence cites ../AGENTS.md § Invariants 4 (R1)" % path)
	if "](references.md)" not in body:
		fail("%s: links references.md" % path)
	if CHECK.search(text) or CITED.search(text):
		fail("%s: the index carries no claims" % path)
	for k, v, hexa in tokens(visible(text)):
		if hexa or abs(v) >= 100:
			fail("%s:%d: the index carries no numeric claims" % (path, k))


# --- Files, AGENTS.md and the footprint ---------------------------------------------------------

def git(*args):
	return subprocess.run(["git"] + list(args), cwd=ROOT, capture_output=True, text=True, check=True).stdout


def check_footprint():
	changed = set(git("diff", "--name-only", BASE, "--").split())
	changed |= set(git("ls-files", "--others", "--exclude-standard").split())
	allowed = ("doc/", SPEC + "/", "issues/")
	for f in sorted(changed):
		if not (f.startswith(allowed) or f in ("AGENTS.md", "ROADMAP.md")):
			fail("%s changed; this cycle writes only doc/, AGENTS.md, %s/ and deferrals (PLAN § 2)" % (f, SPEC))


def split_sections(text):
	out, key = {}, "(top)"
	for l in text.split("\n"):
		if re.match(r"^#{1,6} ", l):
			key = l.strip()
			if key in out:
				key = key + " (again)"
			out[key] = []
		else:
			out.setdefault(key, []).append(l)
	return out


def check_agents():
	old = split_sections(git("show", BASE + ":AGENTS.md"))
	new = split_sections(read("AGENTS.md"))
	if list(old) != list(new):
		fail("AGENTS.md: its headings changed; R9 adds lines to § Map and § Conventions only")
	for k in old:
		if k in ("## Map", "### Conventions (same-commit rules)"):
			it = iter(new[k])
			if not all(any(l == n for n in it) for l in old[k]):
				fail("AGENTS.md: %s lost or reordered a line; R9 only adds" % k)
		elif old[k] != new[k]:
			fail("AGENTS.md: %s changed; R9 touches only § Map and § Conventions" % k)
	def bullets(lines):
		out = []
		for l in lines:
			if l.startswith((" ", "\t")) and out:
				out[-1] += " " + l.strip()
			else:
				out.append(l)
		return out

	if not any(l.startswith("- `doc/`") for l in bullets(new["## Map"])):
		fail("AGENTS.md: § Map needs a line opening - `doc/` (R9)")
	conv = [l for l in bullets(new["### Conventions (same-commit rules)"]) if "`doc/`" in l and "same commit" in l]
	if len(conv) != 1:
		fail("AGENTS.md: § Conventions needs one bullet naming `doc/` and 'same commit' (R9)")


def src_comments():
	# the text of every // comment in src/, a file's consecutive comment lines joined
	out = []
	for f in sorted(os.listdir(os.path.join(ROOT, "src"))):
		if f.endswith((".cpp", ".hpp")):
			parts = [m.group(1).strip() for m in re.finditer(r"//(.*)$", read("src/" + f), re.M)]
			out.append((f, " ".join(parts)))
	return out


def check_src_citations(entries):
	# R6: each work a src/ comment cites has an entry; returns the keys so cited. A work is
	# cited as (Author, "Title", ...) or as RFC NNNN.
	cited = set()
	for f, text in src_comments():
		for m in re.finditer(r"\(([A-Z][^\"()]*?), \"([^\"]+)\"", text):
			keys = [k for k, e in entries.items() if squash(e["Title"]) == squash(m.group(2))]
			if not keys:
				fail("src/%s cites \"%s\", which has no entry in doc/references.md (R6)" % (f, m.group(2)))
			cited.update(keys)
		for m in re.finditer(r"\bRFC (\d{3,5})\b", text):
			keys = [k for k, e in entries.items() if re.search(r"\bRFC %s\b" % m.group(1), e["Venue"])]
			if len(keys) != 1:
				fail("src/%s cites RFC %s, which has no entry (its Venue names RFC %s) in doc/references.md (R6)"
				     % (f, m.group(1), m.group(1)))
			cited.update(keys)
	return cited


def check_starting_set(entries):
	dois = {e.get("doi", "").lower() for e in entries.values()}
	titles = [squash(e["Title"]) for e in entries.values()]
	for label, doi, title in STARTING_SET:
		if label in DROPPED:
			continue
		if doi and doi in dois:
			continue
		if title and any(squash(title) in t for t in titles):
			continue
		fail("doc/references.md: the seeds' %s is neither in it nor dropped by PLAN § 4 (R6)" % label)


# --- The records --------------------------------------------------------------------------------

LEDGER = re.compile(r"^- `(doc/[a-z0-9-]+\.md)` · (.+?) · confirmed in (.+?) on (\d{4}-\d{2}-\d{2}): (.+)$")


def check_cited_ledger(located, today):
	path = os.path.join(EVIDENCE, "cited.md")
	if not os.path.isfile(path):
		fail("%s/evidence/cited.md is missing: one row per located citation (PLAN § 6)" % SPEC)
	rows = set()
	with open(path, encoding="utf-8") as f:
		for n, l in enumerate(f, 1):
			if l.startswith("- "):
				m = LEDGER.match(l.rstrip("\n"))
				if not m or not (GOAL_DATE <= m.group(4) <= today):
					fail("%s/evidence/cited.md:%d: a row reads - `doc/<note>.md` · Key, locator · confirmed in <where> on <date>: <what it says>" % (SPEC, n))
				rows.add((m.group(1), m.group(2).strip()))
	want = set(located)
	if want - rows:
		fail("%s/evidence/cited.md: no row for %s" % (SPEC, sorted(want - rows)[:5]))
	if rows - want:
		fail("%s/evidence/cited.md: rows for citations the notes no longer make: %s" % (SPEC, sorted(rows - want)[:5]))


def report(results):
	lines = ["# Claims record for %s, written by: %s" % (SLUG, REPORT_COMMAND),
	         "# Each check span's expression evaluated to True, and its visible integers match it.", ""]
	for path, line, expr, ev, span in results:
		lines.append("%s:%d: %s" % (path, line, expr))
		lines.extend(describe(ev))
	return "\n".join(lines) + "\n"


def run(phase, today):
	if not claims.self_test():
		fail("claims self-test")
	check_footprint()
	if phase >= 1:
		check_agents()
	present = [n for n, ph, _, _ in NOTES if ph <= phase]
	files = sorted(os.listdir(os.path.join(ROOT, "doc"))) if os.path.isdir(os.path.join(ROOT, "doc")) else []
	want = sorted(["README.md", "references.md"] + present)
	if files != want:
		fail("doc/ holds %s; phase %d wants exactly %s (R1)" % (files, phase, want))
	for name in files:
		m = re.search(r"TODO|TBD|FIXME|XXX|NEEDS CLARIFICATION", read("doc/" + name))
		if m:
			fail("doc/%s holds a placeholder: %s" % (name, m.group(0)))
	entries = parse_references(today)
	results = []
	located = []
	cited_keys = set()
	for name, ph, uses, phrases in NOTES:
		if ph > phase:
			continue
		path = "doc/" + name
		for key, locator, line, m in check_note(path, uses, phrases, entries, results):
			cited_keys.add(key)
			if locator:
				located.append((path, "%s, %s" % (key, locator)))
		required_checks(path, results)
	check_readme(present, entries)
	uncited = sorted(set(entries) - cited_keys - check_src_citations(entries))
	if uncited:
		fail("doc/references.md: %s cited by no note and no src/ comment (R6)" % uncited)
	if phase >= 5:
		check_starting_set(entries)
	check_cited_ledger(located, today)
	return results


def main(argv):
	today = datetime.date.today().isoformat()
	try:
		if len(argv) == 3 and argv[1] == "gate" and argv[2] in "12345":
			results = run(int(argv[2]), today)
			rec = os.path.join(EVIDENCE, "claims.txt")
			got = report(results)
			if not os.path.isfile(rec) or open(rec, encoding="utf-8").read() != got:
				fail("%s/evidence/claims.txt differs from what the checks compute; rerun %s --write and commit it"
				     % (SPEC, REPORT_COMMAND))
			print("notes gate %s: green (%d notes, %d checks, %d references)"
			      % (argv[2], len([n for n in NOTES if n[1] <= int(argv[2])]), len(results),
			         len(parse_references(today))))
			return 0
		if len(argv) in (2, 3) and argv[1] == "report":
			present = [n for n, ph, _, _ in NOTES if os.path.isfile(os.path.join(ROOT, "doc", n))]
			phase = max([ph for n, ph, _, _ in NOTES if n in present] or [1])
			results = run(phase, today)
			got = report(results)
			if len(argv) == 3 and argv[2] == "--write":
				os.makedirs(EVIDENCE, exist_ok=True)
				with open(os.path.join(EVIDENCE, "claims.txt"), "w", encoding="utf-8") as f:
					f.write(got)
			sys.stdout.write(got)
			return 0
	except Fail as e:
		print("GATE FAIL: %s" % e, file=sys.stderr)
		return 1
	if len(argv) >= 3 and argv[1] == "claims":
		# evaluate the check spans of the notes given, nothing else: for a later cycle that
		# edits a note
		try:
			results = []
			for p in argv[2:]:
				judge_claims(p, read(p), results)
			sys.stdout.write(report(results))
			return 0
		except Fail as e:
			print("GATE FAIL: %s" % e, file=sys.stderr)
			return 1
	print("usage: notes.py gate N | report [--write] | claims doc/<note>.md ...", file=sys.stderr)
	return 2


if __name__ == "__main__":
	sys.exit(main(sys.argv))
