# Gate file checks: a cycle record, never part of mps. Python 3.8+, standard library only.
#
#   files.py changes PHASE < changed-paths     what changed against 0bc49cb (PLAN § 2, "Files")
#   files.py interface PHASE                   the pinned declarations and definitions (PLAN § 2)
#   files.py labels PHASE                      the pinned check labels, each in its tier's table
#   files.py agents PHASE BASE_AGENTS          AGENTS.md (R13)
#   files.py makefile PHASE BASE_MAKEFILE      the Makefile's targets, flags and variables
#   files.py source                            R10 and R11 on the engine's sources; no spec ids
#
# Run from the root of the tree under test. Prints each violation and exits 1, or exits 0.

import re
import sys

ENGINE = ["src/fft.hpp", "src/fft.cpp", "src/ibdwt.hpp", "src/ibdwt.cpp", "src/lengths.hpp", "src/lengths.cpp",
          "src/measure.cpp"]
NEW = ENGINE + ["src/lengths.txt", "tests/check_ibdwt.cpp", "tests/check_engine.cpp", "tests/engine_support.hpp",
                "tests/engine_support.cpp"]
EDITED = ["Makefile", "AGENTS.md", "doc/weighted-transforms.md", "doc/references.md", "tests/check.hpp", "tests/check.cpp"]
MAY = ["doc/README.md"]
SLUG = "ibdwt-transform-engine"

bad = []


def fail(msg):
    bad.append(msg)


def read(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except OSError as e:
        fail("%s: %s" % (path, e))
        return ""


def squash(text):
    return re.sub(r"\s+", " ", text).strip()


# --- What changed ---------------------------------------------------------------------------------

def changes(phase):
    changed = [p for p in sys.stdin.read().split("\n") if p]
    allowed = set(NEW + EDITED + MAY)
    for p in changed:
        if p in allowed or p.startswith("spec/%s/" % SLUG):
            continue
        if p.startswith("issues/") or p == "ROADMAP.md":
            if phase < 4:
                fail("%s changed before the last phase (PLAN § 2)" % p)
            continue
        fail("%s changed: PLAN § 2 leaves it as 0bc49cb's (GOAL Non-goals; tests/cli.sh, src/mp.*, src/reference.*, "
             "src/known.*, tests/known/ and the command line stay as they are)" % p)
    for p in NEW + EDITED:
        if p not in changed:
            fail("%s is 0bc49cb's: PLAN § 2 has phase 1 create or change it" % p)


# --- The pinned interface --------------------------------------------------------------------------

DECLS = {
    1: {
        "src/fft.hpp": [
            "class fft_error: public mps_error {",
            "struct Complex { double re = 0; double im = 0; };",
            "static const std::uint64_t LARGEST_ODD_FACTOR = 15;",
            "bool is_supported(std::uint64_t n);",
            "Complex root(std::uint64_t k, std::uint64_t n);",
            "explicit Plan(std::uint64_t n);",
            "std::uint64_t size() const;",
            "void forward(std::vector<Complex>& data) const;",
            "void inverse(std::vector<Complex>& data) const;",
        ],
        "src/ibdwt.hpp": [
            "class engine_error: public mps_error {",
            "class roundoff_error: public engine_error {",
            "std::uint64_t exponent; std::uint64_t length; double distance; double threshold;",
            "static const double THRESHOLD = 0.4375;",
            "static const double MAGNITUDE_LIMIT = 2251799813685248.0;",
            "static const std::uint64_t MAX_WIDTH = 50;",
            "std::vector<std::uint64_t> positions(std::uint64_t p, std::uint64_t n);",
            "std::vector<double> weights(std::uint64_t p, std::uint64_t n);",
            "std::vector<double> inverse_weights(std::uint64_t p, std::uint64_t n);",
            "Transform(std::uint64_t p, std::uint64_t length);",
            "const std::vector<std::uint64_t>& positions() const;",
            "const std::vector<double>& weights() const;",
            "const std::vector<double>& inverse_weights() const;",
            "const fft::Plan& plan() const;",
            "Engine(std::uint64_t p, std::uint64_t length, double threshold = THRESHOLD);",
            "double threshold() const;",
            "const Transform& transform() const;",
            "struct Residue { std::vector<std::int64_t> digits; };",
            "void require_residue(const Transform& transform, const Residue& x);",
            "double roundoff(const std::vector<double>& outputs);",
            "static const double RMS_MARGIN = 1.4142135623730951;",
            "double rms_limit() const;",
            "struct Statistics { double distance = 0; double magnitude = 0; double rms = 0; };",
            "Statistics sample(const Transform& transform, const Residue& x);",
        ],
        "src/lengths.hpp": [
            "static const std::uint64_t LARGEST_LENGTH = 1'048'576;",
            "static const double SIZING_DISTANCE = 0.35;",
            "static const double SIZING_MAGNITUDE = 1125899906842624.0;",
            "bool is_supported(std::uint64_t length);",
            "std::vector<std::uint64_t> supported_lengths();",
            "std::uint64_t largest_exponent(std::uint64_t length);",
            "bool holds(std::uint64_t length, std::uint64_t p);",
            "void require_held(std::uint64_t length, std::uint64_t p);",
            "std::uint64_t length_for(std::uint64_t p);",
            "double rms_envelope(std::uint64_t length);",
            "static const std::uint64_t PROBE_SQUARINGS = 64;",
            "static const std::uint64_t PROBE_OUTPUTS = 4'194'304;",
            "static const int GRID_STEPS = 16;",
            "static const double START_BELOW = 1.0;",
            "static const std::size_t BACKOFF = 2;",
        ],
    },
    2: {
        "src/ibdwt.hpp": [
            "Residue to_digits(const Engine& engine, const BigInt& x);",
            "BigInt from_digits(const Engine& engine, const Residue& x);",
            "double square(const Engine& engine, Residue& x);",
            "double square_subtract(const Engine& engine, Residue& x, std::uint64_t bit);",
            "double multiply(const Engine& engine, Residue& x, const Residue& y);",
        ],
    },
}

# Definitions the gates rename and wrap: each once, at file scope, its first line as given
DEFS = {
    1: [
        ("src/fft.cpp", "fft::Complex fft::root(std::uint64_t k, std::uint64_t n) {"),
        ("src/ibdwt.cpp", "std::vector<double> ibdwt::weights(std::uint64_t p, std::uint64_t n) {"),
        ("src/ibdwt.cpp", "double ibdwt::roundoff(const std::vector<double>& outputs) {"),
        ("src/ibdwt.cpp", "ibdwt::Residue ibdwt::carry(const Engine& engine, const std::vector<double>& outputs, bool subtract, std::uint64_t bit) {"),
        ("src/ibdwt.cpp", "std::vector<double> ibdwt::convolve(const Transform& transform, const Residue& x, const Residue& y) {"),
        ("src/ibdwt.cpp", "std::vector<double> ibdwt::convolve_square(const Transform& transform, const Residue& x) {"),
        ("src/lengths.cpp", "void ibdwt::require_held(std::uint64_t length, std::uint64_t p) {"),
    ],
    2: [
        ("src/ibdwt.cpp", "ibdwt::Residue ibdwt::to_digits(const Engine& engine, const BigInt& x) {"),
        ("src/ibdwt.cpp", "BigInt ibdwt::from_digits(const Engine& engine, const Residue& x) {"),
        ("src/ibdwt.cpp", "double ibdwt::square(const Engine& engine, Residue& x) {"),
        ("src/ibdwt.cpp", "double ibdwt::square_subtract(const Engine& engine, Residue& x, std::uint64_t bit) {"),
        ("src/ibdwt.cpp", "double ibdwt::multiply(const Engine& engine, Residue& x, const Residue& y) {"),
        ("src/ibdwt.cpp", "bool ibdwt::checksums_agree(const Transform& transform, const Residue& x, const Residue& y, const std::vector<double>& outputs) {"),
        ("src/ibdwt.cpp", "double ibdwt::rms(const std::vector<double>& outputs) {"),
    ],
}


def phases_upto(table, phase):
    out = {}
    for ph in sorted(table):
        if ph <= phase:
            if isinstance(table[ph], dict):
                for k, v in table[ph].items():
                    out.setdefault(k, []).extend(v)
            else:
                out.setdefault("defs", []).extend(table[ph])
    return out


def interface(phase):
    for path, decls in phases_upto(DECLS, phase).items():
        text = squash(read(path))
        for d in decls:
            if squash(d) not in text:
                fail("%s: no pinned declaration '%s' (PLAN § 2)" % (path, d))
    for path, first in phases_upto(DEFS, phase).get("defs", []):
        lines = read(path).split("\n")
        hits = [l for l in lines if l == first]
        if len(hits) != 1:
            fail("%s: the pinned definition '%s' is not there exactly once, unindented, on one line (PLAN § 2)" % (path, first))
        name = re.search(r"(\w+::\w+)\(", first).group(1)
        others = [l for l in lines if l != first and re.match(r"^\S.*\b%s\(" % re.escape(name), l)]
        if others:
            fail("%s: %s is defined or overloaded at file scope beside its pinned definition: %s" % (path, name, others[0]))
    # The internal pieces are in no header of src/, so no caller squares without the monitor
    for path in ["src/fft.hpp", "src/ibdwt.hpp", "src/lengths.hpp"]:
        text = squash(read(path))
        for name in ["convolve(", "convolve_square(", "carry(", "checksums_agree(", "rms("]:
            if re.search(r"(?<![\w:.])%s" % re.escape(name), text):
                fail("%s declares %s): it is internal to src/ibdwt.cpp (PLAN § 2.3)" % (path, name))
    # The engine's own calls go through the pinned functions the gates wrap: the weights from
    # phase 1 (Transform), carry, roundoff, the checksums and the rms from phase 2 (the operations)
    if phase >= 1:
        ib = read("src/ibdwt.cpp")
        for call in ["weights("] + (["carry(", "roundoff(", "checksums_agree(", "rms("] if phase >= 2 else []):
            uses = [l for l in ib.split("\n") if re.search(r"(?<![\w])%s" % re.escape(call), l) and l.startswith("\t") and not l.lstrip().startswith("//")]
            if not uses:
                fail("src/ibdwt.cpp never calls %s): the engine must reach it through the pinned function (PLAN § 2)" % call)
        if "require_held(" not in re.sub(r"//.*", "", ib):
            fail("src/ibdwt.cpp never calls require_held: Engine must refuse through it (PLAN § 2)")


# --- The labels ------------------------------------------------------------------------------------

DEFAULT_LABELS = {
    1: ["the weighted transform's positions and weights are Crandall and Fagin's",
        "the transform matches its definition at every odd factor",
        "the convolution rounds to Theorem 1's integers and the carry balances them",
        "every supported length is accepted",
        "the length table holds to its measurement",
        "the engine refuses lengths and exponents it does not hold"],
    2: ["residues round-trip through the balanced digits",
        "squarings and products equal BigInt's at every odd factor",
        "the round-off monitor reports and refuses past the threshold",
        "the engine repeats itself bit for bit",
        "structured inputs are refused or squared right"],
    3: ["shifted chains reproduce the partial residues"],
}
ENGINE_LABELS = {
    1: ["every supported length is accepted at both its assigned exponents"],
    2: ["residues round-trip at every supported length",
        "squarings equal BigInt's at every supported length",
        "products equal BigInt's at every supported length",
        "the round-off monitor reports and refuses at every supported length",
        "the engine repeats itself at every supported length",
        "structured inputs are refused or squared right at every supported length"],
    3: ["squaring chains reproduce the known partial residues",
        "squaring chains reproduce the known final residues",
        "Lucas-Lehmer chains end in zero for the Mersenne prime exponents"],
}


def labels(phase):
    dflt = read("tests/check_ibdwt.cpp")
    eng = read("tests/check_engine.cpp")
    m = re.search(r"static check::Suite\s*\ncheck_by_name \{(.*?)\n\};", dflt, re.S)
    e = re.search(r"static check::Suite\s*\nengine_check_by_name \{check::Tier::ENGINE, \{(.*?)\n\}\};", eng, re.S)
    if not m:
        fail("tests/check_ibdwt.cpp: no default-tier `check_by_name` table")
    if not e:
        fail("tests/check_engine.cpp: no `engine_check_by_name {check::Tier::ENGINE, {...}}` table")
    for ph in range(1, 4):
        for l in DEFAULT_LABELS[ph]:
            present = bool(m) and ('{"%s",' % l) in m.group(1)
            if ph <= phase and not present:
                fail("tests/check_ibdwt.cpp: no default-tier check labelled \"%s\" (PLAN § 2)" % l)
        for l in ENGINE_LABELS[ph]:
            present = bool(e) and ('{"%s",' % l) in e.group(1)
            if ph <= phase and not present:
                fail("tests/check_engine.cpp: no engine-tier check labelled \"%s\" (PLAN § 2)" % l)
    if "engine" not in read("tests/check.cpp") or "Tier::ENGINE" not in read("tests/check.hpp"):
        fail("tests/check.*: the runner has no engine tier (PLAN § 2)")


# --- AGENTS.md -------------------------------------------------------------------------------------

def section(text, heading):
    m = re.search(r"^%s\n(.*?)(?=^#{2,3} )" % re.escape(heading), text, re.S | re.M)
    return m.group(1) if m else ""


def agents(phase, base_path):
    text, base = read("AGENTS.md"), read(base_path)
    hb, hb0 = section(text, "### High-blast-radius paths (human gate)"), section(base, "### High-blast-radius paths (human gate)")
    bullets = [l for l in hb.split("\n") if l.startswith("- ")]
    for l in [l for l in hb0.split("\n") if l.startswith("- ")]:
        if l not in bullets:
            fail("AGENTS.md: the high-blast-radius line '%s' is gone; the list only widens" % l)
    for group in [["src/fft.hpp", "src/fft.cpp"], ["src/ibdwt.hpp", "src/ibdwt.cpp"], ["src/lengths.hpp", "src/lengths.cpp"],
                  ["src/lengths.txt", "src/measure.cpp"]]:
        if not any(all("`%s`" % p in b for p in group) for b in bullets):
            fail("AGENTS.md: no high-blast-radius line naming %s (R13)" % " and ".join(group))
    mp = section(text, "## Map")
    for p in ENGINE + ["src/lengths.txt", "tests/check_engine.cpp", "tests/engine_support.hpp"]:
        if "`%s`" % p not in mp and "`%s`" % p.split("/")[-1] not in mp:
            fail("AGENTS.md § Map does not name %s (R13)" % p)
    br = section(text, "## Build and run")
    for t in ["make check-engine", "make lengths"]:
        if t not in br:
            fail("AGENTS.md § Build and run does not name `%s` (R13)" % t)
    prof = re.search(r"^\| verify \| (.*) \|$", text, re.M)
    prof0 = re.search(r"^\| verify \| (.*) \|$", base, re.M)
    if not prof or not prof0 or prof.group(1) != prof0.group(1):
        fail("AGENTS.md: the profile's verify row changed; the engine tier stays out of it (GOAL Non-goals)")


# --- The Makefile ----------------------------------------------------------------------------------

def makefile(phase, base_path):
    text, base = read("Makefile"), read(base_path)
    for var in ["IEEE", "CXXFLAGS", "BOUNDS", "CPPFLAGS"]:
        a = re.search(r"^%s\s*:=(.*)$" % var, text, re.M)
        b = re.search(r"^%s\s*:=(.*)$" % var, base, re.M)
        if not a or not b or a.group(1).strip() != b.group(1).strip():
            fail("Makefile: %s changed; the numeric flags stay 0bc49cb's (R10)" % var)
    mods = re.search(r"^MODULES\s*:=(.*)$", text, re.M)
    if not mods or not {"fft", "ibdwt", "lengths"} <= set(mods.group(1).split()):
        fail("Makefile: MODULES does not name fft, ibdwt and lengths")
    rule = re.search(r"^check-engine:.*\n\t(.*)$", text, re.M)
    if not rule or not re.search(r"\$\(CHECK\) \|\| status=1; sh tests/cli\.sh \$\(EXE\) \|\| status=1; \$\(CHECK\) engine \|\| status=1", rule.group(1)):
        fail("Makefile: check-engine does not run make check's two steps and then `$(CHECK) engine`, each always (PLAN § 2)")
    chk = re.search(r"^check:.*\n\t(.*)$", text, re.M)
    chk0 = re.search(r"^check:.*\n\t(.*)$", base, re.M)
    if not chk or not chk0 or chk.group(1) != chk0.group(1):
        fail("Makefile: the check recipe changed; make check runs the default tier as before")
    if not re.search(r"^lengths:.*\$\(MEASURE\)", text, re.M):
        fail("Makefile: no `lengths` target on $(MEASURE)")
    if not re.search(r"^BUILT\s*:=.*\$\(MEASURE\)", text, re.M):
        fail("Makefile: BUILT does not name $(MEASURE), so make clean leaves it")
    if not re.search(r"^\.PHONY:.*\bcheck-engine\b.*\blengths\b", text, re.M):
        fail("Makefile: check-engine and lengths are not .PHONY")


# --- The sources ------------------------------------------------------------------------------------

WEAKEN = [r"-Ofast", r"-ffast-math", r"-funsafe-math-optimizations", r"-fassociative-math", r"-freciprocal-math",
          r"-ffinite-math-only", r"-fno-signed-zeros", r"-fno-trapping-math", r"-ffp-contract=(?:fast|on)",
          r"-fcx-limited-range", r"-fexcess-precision=fast", r"-ffp-model=(?:fast|aggressive)", r"-fapprox-func",
          r"-mrecip", r"-mdaz-ftz", r"-fdenormal-fp-math", r"-ffp-exception-behavior=ignore"]
PRAGMAS = [r"#\s*pragma\s+(?:STDC|clang\s+fp|float_control|fp_contract|GCC\s+optimize|GCC\s+target|fenv_access|omp)",
           r"_Pragma", r"__attribute__\s*\(\(\s*(?:optimize|target|flatten|hot|fast)", r"\[\[\s*gnu::(?:optimize|target)"]
NONSTANDARD = [r"\b__int128\b", r"\basm\b", r"__asm", r"\b__builtin_", r"__attribute__", r"\[\[\s*(?:gnu|clang|msvc)::",
               r"#\s*include\s*<(?:immintrin|x86intrin|emmintrin|xmmintrin|arm_neon|arm_sve|arm_sme|intrin|thread|future|"
               r"execution|pthread|omp|dispatch/[a-z_]+|Accelerate/[A-Za-z_]+|fenv|cfenv)\.?h?>",
               r"std::(?:thread|async|jthread|execution)", r"#\s*pragma(?!\s+once\s*$)", r"\b__restrict", r"\balloca\b", r"__declspec",
               r"#\s*if(?:def)?\s+.*(?:__clang__|__GNUC__|_MSC_VER|__x86_64__|__aarch64__|__APPLE__|__linux__)"]


def normalized(text):
    # Alternative tokens spelled out (digraphs; %:%: before %:), so a scan reads what the compiler reads
    for a, b in [("%:%:", "##"), ("%:", "#"), ("<%", "{"), ("%>", "}"), ("<:", "["), (":>", "]")]:
        text = text.replace(a, b)
    return text


def source():
    import os
    for root in ["src", "tests", "ci"]:
        for dirpath, _, names in os.walk(root):
            for n in names:
                p = os.path.join(dirpath, n)
                if not n.endswith((".cpp", ".hpp", ".h", ".sh", ".py")) and n != "Makefile":
                    continue
                text = normalized(read(p))
                for rx in PRAGMAS:
                    if re.search(rx, text):
                        fail("%s: '%s' can weaken the numeric flags (R10)" % (p, re.search(rx, text).group(0)))
    for p in ["Makefile"] + [os.path.join("ci", "targets", n) for n in sorted(os.listdir("ci/targets"))] if os.path.isdir("ci/targets") else ["Makefile"]:
        text = read(p)
        for rx in WEAKEN:
            m = re.search(r"(?<![\w-])%s(?![\w-])" % rx, text)
            if m:
                fail("%s: '%s' weakens the numeric flags (R10)" % (p, m.group(0)))
    for p in ENGINE:
        code = re.sub(r"//.*", "", normalized(read(p)))
        for rx in NONSTANDARD:
            m = re.search(rx, code, re.M)
            if m:
                fail("%s: '%s' is not standard C++20 alone (R11)" % (p, m.group(0).strip()))
        for m in re.finditer(r'#\s*include\s*<([^>]+)>', code):
            if not re.fullmatch(r"[a-z_]+", m.group(1)):
                fail("%s: <%s> is not a standard library header (R11)" % (p, m.group(1)))
    for p in NEW:
        if p.endswith(".txt"):
            continue
        text = read(p)
        head = text.split("\n")[:3]
        if len(head) < 2 or not head[0].startswith("// SPDX-FileCopyrightText: 2026 Geoffrey Lentner") or \
                head[1] != "// SPDX-License-Identifier: Apache-2.0":
            fail("%s: does not open with the SPDX block (AGENTS.md § Conventions)" % p)
        code = re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)
        m = re.search(r"\b[RP]\d{1,2}\b", code)
        if m:
            fail("%s: '%s' looks like a spec id in source (AGENTS.md § Conventions)" % (p, m.group(0)))


def main():
    cmd = sys.argv[1]
    if cmd == "changes":
        changes(int(sys.argv[2]))
    elif cmd == "interface":
        interface(int(sys.argv[2]))
    elif cmd == "labels":
        labels(int(sys.argv[2]))
    elif cmd == "agents":
        agents(int(sys.argv[2]), sys.argv[3])
    elif cmd == "makefile":
        makefile(int(sys.argv[2]), sys.argv[3])
    elif cmd == "source":
        source()
    else:
        sys.exit("files.py: unknown command %s" % cmd)
    for b in bad:
        print("files.py: " + b, file=sys.stderr)
    sys.exit(1 if bad else 0)


main()
