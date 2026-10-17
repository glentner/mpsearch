# Cluster-evidence helpers for this cycle's pre-push checks, its last gate and redact.sh (a cycle
# record, not a standing gate). Sourced after common.sh. Copied from
# spec/known-answer-tables/verify/cluster.sh: the result.json checker, the site-private scan with
# its self-test, the redaction, the file list and code_committed unchanged; the ledger holds this
# cycle's deferrals (PLAN § 6).

need python3
need hostname
HOST=$(hostname)
test -n "$HOST" || fail "hostname prints nothing"
HOST_SHORT=${HOST%%.*}
DEFAULT_URL='https://github.com/glentner/mpsearch.git'
REAL_TARGETS='x86-64-avx512 x86-64-avx2 aarch64-gb10'

# result_check FILE KEY=VALUE...: validate result.json's shape, and each expected value given.
# Keys: requested commit target cluster host arch job_id compiler make (value "null" means JSON
# null), failed=none|clone|checkout|toolchain|build|check, not_before and not_after (UTC times that
# bound start and end), min_seconds=STEP:N.
cat >"$work/check_result.py" <<'EOF'
import datetime, json, re, sys

path = sys.argv[1]
exp = dict(a.split("=", 1) for a in sys.argv[2:])

def bad(msg):
    print("result.json (%s): %s" % (path, msg), file=sys.stderr)
    sys.exit(1)

def no_dups(pairs):
    keys = [k for k, _ in pairs]
    if len(keys) != len(set(keys)):
        bad("duplicate keys in an object: %s" % keys)
    return dict(pairs)

def no_constant(name):
    bad("non-JSON number %s" % name)

try:
    with open(path) as f:
        r = json.load(f, object_pairs_hook=no_dups, parse_constant=no_constant)
except SystemExit:
    raise
except Exception as e:
    bad("not valid JSON: %s" % e)
keys = ["format", "requested", "commit", "target", "cluster", "host", "arch", "job_id",
        "start", "end", "compiler", "make", "steps", "status"]
if not isinstance(r, dict) or sorted(r) != sorted(keys):
    bad("keys are %s, want exactly %s" % (sorted(r) if isinstance(r, dict) else type(r).__name__, sorted(keys)))
if r["format"] != 1 or isinstance(r["format"], bool):
    bad("format is %r, want 1" % r["format"])
for k in ["requested", "target", "host", "arch", "job_id", "start", "end"]:
    if not isinstance(r[k], str) or not r[k]:
        bad("%s is %r, want a non-empty string" % (k, r[k]))
for k in ["commit", "cluster", "compiler", "make"]:
    if r[k] is not None and (not isinstance(r[k], str) or not r[k]):
        bad("%s is %r, want a non-empty string or null" % (k, r[k]))
if r["commit"] is not None and not re.fullmatch(r"[0-9a-f]{40}", r["commit"]):
    bad("commit %r is not a full hash" % r["commit"])
steps = r["steps"]
names = ["clone", "checkout", "toolchain", "build", "check"]
if not isinstance(steps, list) or [s.get("name") if isinstance(s, dict) else None for s in steps] != names:
    bad("steps are %r, want the five steps in order %s" % (steps, names))
code = ""
for s in steps:
    if sorted(s) != ["name", "seconds", "status"]:
        bad("step %r has keys %s" % (s.get("name"), sorted(s)))
    st, sec = s["status"], s["seconds"]
    if st == "not_run":
        if sec is not None:
            bad("step %s did not run but has seconds %r" % (s["name"], sec))
        code += "N"
    elif st in ("passed", "failed"):
        if not isinstance(sec, int) or isinstance(sec, bool) or sec < 0:
            bad("step %s seconds %r is not a non-negative integer" % (s["name"], sec))
        code += "P" if st == "passed" else "F"
    else:
        bad("step %s status %r" % (s["name"], st))
if not re.fullmatch(r"P*(FN*)?", code):
    bad("step statuses %s: want passes, then at most one failure, then only not_run" % code)
want_status = "passed" if code == "PPPPP" else "failed"
if r["status"] != want_status:
    bad("status %r with steps %s, want %r" % (r["status"], code, want_status))
if code[2] == "P" and (r["compiler"] is None or r["make"] is None):
    bad("toolchain passed but compiler or make is null")
if code[1] == "P" and r["commit"] is None:
    bad("checkout passed but commit is null")
if code[1] != "P" and r["commit"] is not None:
    bad("checkout did not pass but commit is %r" % r["commit"])
fmt = "%Y-%m-%dT%H:%M:%SZ"
def when(v, what):
    try:
        return datetime.datetime.strptime(v, fmt)
    except ValueError:
        bad("%s %r is not UTC ISO 8601 (YYYY-MM-DDTHH:MM:SSZ)" % (what, v))
t0 = when(r["start"], "start")
t1 = when(r["end"], "end")
if t1 < t0:
    bad("end is before start")
total = sum(s["seconds"] or 0 for s in steps)
if total > (t1 - t0).total_seconds() + 2:
    bad("step seconds sum to %d, more than the run's %d" % (total, (t1 - t0).total_seconds()))
if "not_before" in exp and t0 < when(exp.pop("not_before"), "not_before"):
    bad("start %s is before the gate started the job" % r["start"])
if "not_after" in exp and t1 > when(exp.pop("not_after"), "not_after"):
    bad("end %s is after the job returned" % r["end"])
if "min_seconds" in exp:
    name, n = exp.pop("min_seconds").split(":")
    got = steps[names.index(name)]["seconds"]
    if got is None or got < int(n):
        bad("step %s took %r seconds, want at least %s" % (name, got, n))
failed = exp.pop("failed", None)
if failed is not None:
    got = names[code.index("F")] if "F" in code else "none"
    if got != failed:
        bad("failed step is %s, want %s" % (got, failed))
for k, v in exp.items():
    want = None if v == "null" else v
    if r.get(k) != want:
        bad("%s is %r, want %r" % (k, r.get(k), want))
EOF

result_check() {
	f=$1
	shift
	test -f "$f" || fail "no result.json at ${f#"$work"/} (the job's result format)"
	python3 -I "$work/check_result.py" "$f" "$@" || fail "result.json is wrong (the job's result format)"
}


# The site-private scan. Literal values come from a site file outside the repository; the scan
# fails when the file is missing or any value is empty. Words match case-insensitively as words, QoS
# names only in Slurm-option contexts, path prefixes as substrings; the public repository URL is the
# one allowed appearance of its owner's name. A detailed stamp (a date, a space, a time and a space,
# escape sequences removed) must be followed by <host>: a node's name is a site detail, whatever its
# spelling.
SITE_ENV=${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}
cat >"$work/scan.py" <<'PY'
import re, sys

site, url = sys.argv[1], sys.argv[2]
want = ["MPS_SITE_ACCOUNTS", "MPS_SITE_QOS", "MPS_SITE_USER", "MPS_SITE_CLUSTERS",
        "MPS_SITE_SCRATCH_PREFIX", "MPS_SITE_PATH_PREFIXES"]
try:
    lines = open(site).read().splitlines()
except OSError as e:
    print("site file %s: %s" % (site, e), file=sys.stderr)
    sys.exit(2)
vals = {}
for line in lines:
    m = re.fullmatch(r'\s*([A-Z_]+)=(?:"([^"]*)"|\'([^\']*)\'|(\S*))\s*', line)
    if m:
        vals[m.group(1)] = (m.group(2) or m.group(3) or m.group(4) or "").split()
for k in want:
    if not vals.get(k):
        print("site file %s: %s is missing or empty" % (site, k), file=sys.stderr)
        sys.exit(2)
words = vals["MPS_SITE_ACCOUNTS"] + vals["MPS_SITE_USER"] + vals["MPS_SITE_CLUSTERS"]
word_re = re.compile(r"(?<![A-Za-z0-9])(%s)(?![A-Za-z0-9])" % "|".join(map(re.escape, words)), re.I)
qos = "|".join(map(re.escape, vals["MPS_SITE_QOS"]))
qos_re = re.compile(r"(?:(?<![A-Za-z0-9-])-q[ =]+|--qos[ =]+|qos\s*[=:]\s*)['\"]?(%s)(?![A-Za-z0-9])" % qos, re.I)
paths = vals["MPS_SITE_SCRATCH_PREFIX"] + vals["MPS_SITE_PATH_PREFIXES"]
path_re = re.compile("|".join(map(re.escape, paths)), re.I)
stamp_re = re.compile(r"(?<![0-9])[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2} (?!<host>(?:\s|$))(\S*)")
ansi = re.compile(r"\x1b\[[0-9;]*m")
allowed = re.compile(re.escape(url), re.I)
hits = 0
for name in sys.argv[3:]:
    try:
        text = open(name, errors="replace").read()
    except OSError as e:
        print("%s: %s" % (name, e), file=sys.stderr)
        sys.exit(2)
    for n, line in enumerate(text.splitlines(), 1):
        clean = ansi.sub("", allowed.sub("<public-url>", line))
        for rx, what in ((word_re, "site name"), (qos_re, "QoS"), (path_re, "site path"), (stamp_re, "host after a detailed stamp")):
            m = rx.search(clean)
            if m:
                hits += 1
                shown = "<redacted>" if rx is stamp_re else repr(m.group(0))
                print("%s:%d: %s %s" % (name, n, what, shown), file=sys.stderr)
sys.exit(1 if hits else 0)
PY

# The redaction (redact.sh). A result.json (IN ending in .json) gets "cluster": "<cluster>" and
# "host": "<host>"; then, in any file, the host after a detailed stamp, plain or coloured, becomes
# <host>, the site's scratch paths <scratch>, its other site paths <site-path>, and its account, user
# and cluster names <site>. The public repository URL is left alone.
cat >"$work/redact.py" <<'PY'
import json, re, sys

site, url, src, dst = sys.argv[1:5]
vals = {}
try:
    lines = open(site).read().splitlines()
except OSError as e:
    sys.exit("site file %s: %s" % (site, e))
for line in lines:
    m = re.fullmatch(r'\s*([A-Z_]+)=(?:"([^"]*)"|\'([^\']*)\'|(\S*))\s*', line)
    if m:
        vals[m.group(1)] = (m.group(2) or m.group(3) or m.group(4) or "").split()
for k in ["MPS_SITE_ACCOUNTS", "MPS_SITE_USER", "MPS_SITE_CLUSTERS", "MPS_SITE_SCRATCH_PREFIX", "MPS_SITE_PATH_PREFIXES"]:
    if not vals.get(k):
        sys.exit("site file %s: %s is missing or empty" % (site, k))
text = open(src, errors="surrogateescape").read()
if src.endswith(".json"):
    r = json.loads(text)
    if r.get("cluster") is not None:
        r["cluster"] = "<cluster>"
    r["host"] = "<host>"
    text = json.dumps(r, indent=2) + "\n"
text = re.sub(r"((?<![0-9])[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}(?:\x1b\[[0-9;]*m)* (?:\x1b\[[0-9;]*m)*)[^\s\x1b]*",
              lambda m: m.group(1) + "<host>", text)
keep = "\0URL\0"
text = re.sub(re.escape(url), keep, text, flags=re.I)
for p in vals["MPS_SITE_SCRATCH_PREFIX"]:
    text = re.sub(re.escape(p) + r"[^\s'\"]*", "<scratch>", text, flags=re.I)
for p in vals["MPS_SITE_PATH_PREFIXES"]:
    text = re.sub(re.escape(p) + r"[^\s'\"]*", "<site-path>", text, flags=re.I)
words = vals["MPS_SITE_ACCOUNTS"] + vals["MPS_SITE_USER"] + vals["MPS_SITE_CLUSTERS"]
text = re.sub(r"(?<![A-Za-z0-9])(%s)(?![A-Za-z0-9])" % "|".join(map(re.escape, words)), "<site>", text, flags=re.I)
text = text.replace(keep, url)
open(dst, "w", errors="surrogateescape").write(text)
PY

# scan_site_private FILE...: the scan over the given files
scan_site_private() {
	python3 -I "$work/scan.py" "$SITE_ENV" "${DEFAULT_URL%.git}" "$@"
	case $? in
		0) ;;
		2) fail "the site-private scan cannot run: site file or input missing (set MPS_SITE_ENV)" ;;
		*) fail "site-private detail in what this cycle commits" ;;
	esac
}

# redact IN OUT: the redaction, then the scan over OUT
redact() {
	python3 -I "$work/redact.py" "$SITE_ENV" "${DEFAULT_URL%.git}" "$1" "$2" || fail "redaction of $1 failed"
	scan_site_private "$2"
}

# scan_selftest: the scan flags a node's name after a detailed stamp, in a log line, a coloured log
# line and a check's echo of one, and passes each once the redaction has made it <host>. The date and
# the time are kept apart here, so this file holds no stamp of its own.
scan_selftest() {
	st_d=2026-01-02
	st_t=03:04:05
	st_h=node-0042
	printf '%s %s %s [abc]     info [mps] Running Lucas-Lehmer test\n' "$st_d" "$st_t" "$st_h" >"$work/stamp-plain.log"
	printf '\033[2m%s %s\033[0m \033[2m%s\033[0m \033[2m[abc] \033[0m\033[1m\033[33m warning\033[0m\033[2m [mps] \033[0mx\n' \
		"$st_d" "$st_t" "$st_h" >"$work/stamp-colour.log"
	printf "   got exit 1, stderr '%s %s %s critical [mps] x'\n" "$st_d" "$st_t" "$st_h" >"$work/stamp-got.log"
	for st_f in plain colour got; do
		python3 -I "$work/scan.py" "$SITE_ENV" "${DEFAULT_URL%.git}" "$work/stamp-$st_f.log" 2>/dev/null
		st_rc=$?
		test "$st_rc" -ne 2 || fail "the site-private scan cannot run: site file missing or incomplete (set MPS_SITE_ENV)"
		test "$st_rc" -eq 1 || fail "the scan misses a node's name after a detailed stamp ($st_f line)"
		python3 -I "$work/redact.py" "$SITE_ENV" "${DEFAULT_URL%.git}" "$work/stamp-$st_f.log" "$work/stamp-$st_f.red" \
			|| fail "the redaction fails on a $st_f stamp"
		! grep -q "$st_h" "$work/stamp-$st_f.red" || fail "the redaction leaves a node's name after a $st_f stamp"
		python3 -I "$work/scan.py" "$SITE_ENV" "${DEFAULT_URL%.git}" "$work/stamp-$st_f.red" \
			|| fail "the scan flags a $st_f stamp the redaction made <host>"
	done
}

# cycle_files: every file this cycle adds or changes against origin/main, committed or not
cycle_files() {
	( cd "$root" && { git diff --name-only origin/main...HEAD; git diff --name-only HEAD; git ls-files --others --exclude-standard; } \
		| sort -u | while IFS= read -r p; do [ -f "$p" ] && echo "$root/$p"; done )
}

# scan_cycle [evidence]: the scan's self-test, then the scan over every file the cycle touches and
# over the messages and patches of origin/main...HEAD; the file list must be non-empty and, with
# `evidence`, cover the evidence ledger
scan_cycle() {
	scan_selftest
	cycle_files >"$work/files"
	test -s "$work/files" || fail "no cycle files found to scan"
	if [ "${1:-}" = evidence ]; then
		grep -q "/$spec/evidence/jobs.tsv\$" "$work/files" || fail "the scan does not cover evidence/jobs.tsv"
	fi
	( cd "$root" && git log -p --format=%B origin/main...HEAD ) >"$work/history.txt" || fail "git log of origin/main...HEAD"
	echo "$work/history.txt" >>"$work/files"
	sc_ifs=$IFS
	IFS='
'
	set -f
	# shellcheck disable=SC2046
	scan_site_private $(cat "$work/files")
	set +f
	IFS=$sc_ifs
	# This machine's own name, outside any stamp, never printed
	if [ "${#HOST_SHORT}" -ge 6 ]; then
		while IFS= read -r sc_f; do
			! grep -qiF "$HOST_SHORT" "$sc_f" || fail "this machine's host name in ${sc_f#"$root"/}: a gate's output or a check's echo was committed"
		done <"$work/files"
	fi
}


# ledger: every issues/*.md is indexed by exactly one **Seed:** line in ROADMAP.md and every Seed line
# names an existing issue; every issue TECH.md names exists; this cycle's seed is still adopted; and
# this cycle's deferral is recorded where PLAN § 6 sends it: a Notes bullet saying "deferred from
# `objects-follow-the-compiler`" in issues/packaging-and-install.md, and evidence/ledger.md with a
# '## Deferrals' section listing every deferral, that one included
ledger() {
	( cd "$root" || exit 1
	  seeds=$(ls issues/*.md | wc -l | tr -d ' ')
	  entries=$(grep -c '^\*\*Seed:\*\*' ROADMAP.md)
	  test "$seeds" -eq "$entries" || { echo "ROADMAP has $entries Seed lines for $seeds seeds"; exit 1; }
	  for p in issues/*.md; do
		test "$(grep -c "^\*\*Seed:\*\* .*($p)" ROADMAP.md)" -eq 1 || { echo "$p is not indexed exactly once in ROADMAP.md"; exit 1; }
	  done
	  for p in $(grep -o 'issues/[a-z0-9-]*\.md' ROADMAP.md | sort -u); do
		test -f "$p" || { echo "ROADMAP.md names $p, which does not exist"; exit 1; }
	  done
	  for p in $(grep -o 'issues/[a-z0-9-]*\.md' "$spec/TECH.md" | sort -u); do
		test -f "$p" || { echo "TECH.md names $p, which does not exist"; exit 1; }
	  done
	  grep -qx "status: adopted:$slug" "issues/$slug.md" \
		|| { echo "this cycle's seed is no longer adopted:$slug"; exit 1; }
	  grep -q 'deferred from `objects-follow-the-compiler`' issues/packaging-and-install.md \
		|| { echo "issues/packaging-and-install.md has no Notes bullet saying what is deferred from \`objects-follow-the-compiler\` (PLAN § 6)"; exit 1; }
	  test -f "$spec/evidence/ledger.md" && grep -q '^## Deferrals$' "$spec/evidence/ledger.md" \
		|| { echo "no $spec/evidence/ledger.md with a '## Deferrals' section"; exit 1; }
	  grep -q 'issues/packaging-and-install.md' "$spec/evidence/ledger.md" \
		|| { echo "$spec/evidence/ledger.md does not list the deferral to issues/packaging-and-install.md"; exit 1; }
	) >"$work/ledger" 2>&1 || { cat "$work/ledger" >&2; fail "deferral ledger"; }
}


# code_committed: no uncommitted or untracked code outside spec/, issues/ and ROADMAP.md, and HEAD
# contains origin/main, so what is pushed is what the gate built
code_committed() {
	test -z "$(cd "$root" && git status --porcelain -- . ':(exclude)spec' ':(exclude)issues' ':(exclude)ROADMAP.md')" \
		|| fail "uncommitted or untracked code in the working tree: commit it before the push"
	( cd "$root" && git merge-base --is-ancestor origin/main HEAD ) || fail "HEAD does not contain origin/main"
}
