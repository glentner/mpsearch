# Gate note checks: a cycle record, never part of mps. Python 3.8+, standard library only.
#
#   docs.py note PHASE BASE_NOTE      doc/weighted-transforms.md against 0bc49cb's (R13)
#   docs.py references                doc/references.md and the engine's comments' citations (R13)
#
# Run from the root of the tree under test.
#
# The note. Its "What mps uses" section describes the engine as built: the operations by name, the
# estimate that guided the sizing (a link to CrandallMayerPapadopoulos2003), where the measurement
# lives (src/lengths.txt, src/lengths.cpp, `make lengths`) and the engine tier (`make check-engine`),
# with CooleyTukey1965 cited where the transform is described; no stale sentence of 0bc49cb's
# survives. Every line the cycle adds or changes is read for numbers outside the marker pairs of
# doc/README.md's rule (decision 2026-10-16): digits, decimals, powers and number words alike. A
# number may stand unmarked only if it is one of the definitions and format constants PLAN § 2
# lists (EXEMPT below); any other must be marked, and none may be a value of the length table or
# of its measurement in any form, marked or not, since the note points at the data and states
# none of it (lesson of cycle 7).

import re
import sys

NOTE = "doc/weighted-transforms.md"
REFS = "doc/references.md"
ENGINE = ["src/fft.hpp", "src/fft.cpp", "src/ibdwt.hpp", "src/ibdwt.cpp", "src/lengths.hpp", "src/lengths.cpp",
          "src/measure.cpp"]

# Definitions and format constants (PLAN § 2.7), allowed unmarked only where they define: the
# largest odd factor ("odd m up to 15", "m <= 15"), the largest length and the magnitude limits as
# powers of 2 (2^{20}, 2^{50}, 2^{51}, 2^{52}), the sizing distance 0.35, the default threshold 7/16,
# the double's 53 mantissa bits, 64-bit limbs and the modulus 2^{64}, the radix-2 and radix-4
# passes, word 0, the margin 2^{1/2}, Theorem 1's name; 0, 1 and 2 only inside a formula ($...$). notes.py claims, which the gate runs too, refuses any integer of 100 or
# more outside a marker pair, so 0.4375 is written 7/16. Any other number, even one of these
# digits in another place ("about 20 bits"), must be marked or dropped.
EXEMPT_CONTEXTS = [r"2\^\{?(?:20|50|51|52|64)\}?", r"\bTheorem 1\b", r"2\^\{1/2\}", r"\b0\.35\b", r"\b7/16\b", r"\bodd \$?m\$? (?:up to|of at most|\\le|≤) 15\b",
                   r"\$?m\$? ?(?:\\le|≤|<=) ?15\b", r"\b53 (?:mantissa )?bits\b", r"\b64-bit\b", r"\bradix-[24]\b", r"\bword 0\b"]
EXEMPT_NUMBERS = set()
# Numbers as a note can write them: hexadecimal, and decimals with separators, fractions and
# exponents ("2e7", "1.9e+7", "20,512,768", "19.6")
NUMBER = r"(?<![\w.])(0[xX][0-9a-fA-F]+|\d[\d,']*(?:\.\d+)?(?:[eE][+-]?\d+)?)(?!\d)"
NUMBER_WORDS = ["two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve",
                "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty",
                "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety", "hundred", "thousand",
                "million", "millions", "billion", "dozen", "twice", "thrice", "triple",
                "percent", "quarter", "third", "fourth", "fifth", "tenth", "hundredth"]
STALE = ["does not exist yet", "keeps decimal digits", "can only load and print", "The planned engine", "will use Theorem 1",
         "will size each length", "will be tested against"]
MUST = {1: ["`src/lengths.txt`", "`src/lengths.cpp`", "`make lengths`", "`make check-engine`", "`ibdwt::carry`",
            "(references.md#crandallmayerpapadopoulos2003)", "(references.md#cooleytukey1965)", "0.35"],
        2: ["`ibdwt::to_digits`", "`ibdwt::from_digits`", "`ibdwt::square`", "`ibdwt::square_subtract`", "`ibdwt::multiply`",
            "7/16"]}

bad = []


def fail(msg):
    bad.append(msg)


def unmarked(text):
    # The text with marker pairs, citation links' text and targets, inline code and LaTeX commands
    # removed: what is left is prose and formulas a number could hide in
    text = re.sub(r"<!-- check: .*? -->.*?<!-- /check -->", " ", text, flags=re.S)
    text = re.sub(r"<!-- cited -->.*?<!-- /cited -->", " ", text, flags=re.S)
    text = re.sub(r"\[[^\]]*\]\(references\.md#[a-z0-9-]+\)", " ", text)
    text = re.sub(r"\[[^\]]*\]\([a-z-]+\.md\)", " ", text)
    text = re.sub(r"`", " ", text)  # code spans are read like the prose around them
    text = re.sub(r"\\[A-Za-z]+", " ", text)
    return text


def formula_constants(text):
    # Inside a formula, 0, 1 and 2 are formula constants (2^p, x_0, a + 1): drop them there only
    return re.sub(r"\$[^$\n]*\$", lambda m: re.sub(r"(?<![\w.])[012](?![\w.])", " ", m.group(0)), text)


def table_values():
    # The table's values: its lengths and largest exponents (src/lengths.cpp), three digits or more
    vals = set()
    try:
        for m in re.finditer(r"\t\{\s*(\d+), (\d+)\},", open("src/lengths.cpp").read()):
            vals.update([m.group(1), m.group(2)])
    except OSError:
        fail("src/lengths.cpp does not open")
    return {v for v in vals if len(v) >= 3}


def note(phase, base_path):
    text, base = open(NOTE).read(), open(base_path).read()
    m = re.search(r"^## What mps uses\n(.*?)^## ", text, re.S | re.M)
    if not m:
        fail("%s has no '## What mps uses' section" % NOTE)
        return
    uses = m.group(1)
    for s in STALE:
        if s in text:
            fail("%s still says '%s' (R13: the note describes the engine as built)" % (NOTE, s))
    for s in [m for ph in MUST if ph <= phase for m in MUST[ph]]:
        if s not in uses:
            fail("%s, 'What mps uses', does not name %s (R13)" % (NOTE, s))
    old = set(base.split("\n"))
    added = "\n".join(l for l in text.split("\n") if l not in old)
    prose = unmarked(added)
    for rx in EXEMPT_CONTEXTS:
        prose = re.sub(rx, " ", prose)
    prose = formula_constants(prose)
    for m in re.finditer(NUMBER, prose):
        tok = m.group(1).replace(",", "").replace("'", "")
        if tok not in EXEMPT_NUMBERS:
            ctx = prose[max(0, m.start() - 30):m.end() + 30].replace("\n", " ")
            fail("%s: the number %s stands unmarked in an added line ('...%s...'); mark it or drop it (R13, doc/README.md)" % (NOTE, m.group(1), ctx))
    for w in NUMBER_WORDS:
        for m in re.finditer(r"\b%s\b" % w, prose, re.I):
            ctx = prose[max(0, m.start() - 30):m.end() + 30].replace("\n", " ")
            fail("%s: the number word '%s' stands unmarked in an added line ('...%s...') (R13, doc/README.md)" % (NOTE, m.group(0), ctx))
    # Table values may not appear at all, marked or not: read the added lines with only the marker
    # comments (not what they mark) and the citations' link texts and targets removed
    plain = re.sub(r"<!-- (?:check:.*?|/check|cited|/cited) -->", " ", added, flags=re.S)
    plain = re.sub(r"\[[^\]]*\]\([a-z0-9-]+\.md(?:#[a-z0-9-]+)?\)", " ", plain)
    shown = set()
    for m in re.finditer(NUMBER, plain):
        tok = m.group(1).replace(",", "").replace("'", "")
        shown.add(tok)
        try:
            shown.add(str(int(float(tok))))
        except (ValueError, OverflowError):
            pass
    for v in sorted(table_values() & shown):
        fail("%s states %s, a value of the length table or its measurement; point at the data instead" % (NOTE, v))
    for m in re.finditer(r"(?<![\w.])(\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\s*(?:million|thousand|M\b|bits a word|bits per word)", unmarked(added)):
        fail("%s states a size, '%s', that only the measurement can give" % (NOTE, m.group(0)))


def references():
    refs = open(REFS).read()
    keys = re.findall(r"^### (\S+)$", refs, re.M)
    for k in ["CooleyTukey1965", "SteeleLeaFlood2014"]:
        if k not in keys:
            fail("%s has no entry %s (PLAN § 2)" % (REFS, k))
    if keys != sorted(keys, key=lambda s: s.lower()):
        fail("%s: the entries are not in key order" % REFS)
    for k in keys:
        body = refs.split("### %s\n" % k, 1)[1].split("\n### ", 1)[0]
        for field in ["**Authors:**", "**Title:**", "**Venue:**", "**Year:**", "**Verified:**"]:
            if field not in body:
                fail("%s: %s has no %s line" % (REFS, k, field))
    titles = [t.lower() for t in re.findall(r"^- \*\*Title:\*\* (.*)$", refs, re.M)]
    for p in ENGINE:
        comments = re.sub(r"\n\s*//\s*", " ", open(p).read())
        for m in re.finditer(r'"([^"]{12,})", (?:19|20)\d\d', comments):
            if m.group(1).lower() not in titles:
                fail("%s cites \"%s\", which %s does not list (R13)" % (p, m.group(1), REFS))
    for k in re.findall(r"references\.md#([a-z0-9-]+)", open(NOTE).read()):
        if k not in [x.lower() for x in keys]:
            fail("%s links #%s, which %s does not hold" % (NOTE, k, REFS))


cmd = sys.argv[1]
if cmd == "note":
    note(int(sys.argv[2]), sys.argv[3])
elif cmd == "references":
    references()
else:
    sys.exit("docs.py: unknown command %s" % cmd)
for b in bad:
    print("docs.py: " + b, file=sys.stderr)
sys.exit(1 if bad else 0)
