# Shared helpers for this cycle's phase gates (a cycle record, not a standing gate).
# Sourced by p1.sh … p4.sh. POSIX sh. The job under test is the working tree's ci/test-commit.sh; it
# runs against a bare fixture repository built from the working tree, with a fake login profile and
# module command, shadowed interpreters, and a sandboxed scratch, home, temp and working directory
# (research/04-gate-design.md).

# The tree under test is the one holding this script, whatever the caller's directory.
root=$(cd "$(dirname "$0")" && git rev-parse --show-toplevel) || { echo "gate: not in a git tree" >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/mps-gate.XXXXXX") || { echo "gate: mktemp failed" >&2; exit 1; }
logs="$work/logs"
mkdir -p "$logs" || { echo "gate: mkdir $logs" >&2; exit 1; }
trap 'chmod -R u+rwx "$work" 2>/dev/null; /bin/rm -rf "$work"' EXIT INT TERM

JOB="$root/ci/test-commit.sh"
TARGETS="$root/ci/targets"
DEFAULT_URL='https://github.com/glentner/mpsearch.git'
STEPS='clone checkout toolchain build check'
TARGET_KEYS='description partition gpus cpus mem time modules make_vars'
REAL_TARGETS='x86-64-avx512 x86-64-avx2 aarch64-gb10'
WRAPPER_LINE='gate-c++ 1.0 (gate wrapper around clang++)'

fail() {
	echo "GATE FAIL: $*" >&2
	exit 1
}

need() {
	command -v "$1" >/dev/null 2>&1 || fail "required tool not found: $1"
}

require_job() {
	test -f "$JOB" || fail "ci/test-commit.sh is missing (R1)"
}

# show NAME: the captured output of run NAME, for a failure report
show() {
	echo "--- stdout of $1" >&2; cat "$logs/$1.out" >&2
	echo "--- stderr of $1" >&2; cat "$logs/$1.err" >&2
}

now() {
	date -u +%Y-%m-%dT%H:%M:%SZ
}

# copy_tree DEST: the files git knows about (tracked, plus untracked and unignored), as a clone would
# hold them once committed. Ignored build output never comes along.
copy_tree() {
	mkdir -p "$1" || fail "mkdir $1"
	( cd "$root" && git ls-files -z --cached --others --exclude-standard ) \
		| rsync -a --from0 --files-from=- "$root/" "$1/" || fail "copy of the tree into $1"
	test ! -e "$1/lib" || fail "copy of the tree already has lib/"
	test ! -e "$1/bin" || fail "copy of the tree already has bin/"
}

# fx_commit MESSAGE: commit everything in the fixture; prints the new hash
fx_commit() {
	( cd "$fx" && git add -A && git -c user.name=gate -c user.email=gate@invalid -c commit.gpgSign=false commit -q -m "$1" ) >/dev/null \
		|| fail "fixture commit: $1"
	git -C "$fx" rev-parse HEAD
}

# write_target NAME LINES: a gate-only target file in the fixture
write_target() {
	printf '%s\n' "$2" >"$fx/ci/targets/$1" || fail "write fixture target $1"
}

STUB_TARGET="# A gate-only target: the fake module accepts stub/*, which puts gate-c++ on PATH.
description='local gate stub'
partition=local
gpus=
cpus=1
mem=1G
time=00:10:00
modules='stub/1.0'
make_vars='CXX=gate-c++ CXXFLAGS=-O1'"

# make_fixture: a fixture repository with these commits, bare-cloned to $url (a file:// URL):
#   good       the working tree, plus the gate-only targets local-stub and local-badmodule
#   red_build  good + a compile error in src/main.cpp
#   red_check  good + a smoke test that fails
#   slow       good + a default make goal that sleeps (for the signal test)
#   newer      good + one more commit on the default branch, so HEAD is never a tested commit
# and a tag named like hex (trap_ref) on good, which must never pass for a commit.
make_fixture() {
	fx="$work/fixture"
	copy_tree "$fx"
	mkdir -p "$fx/ci/targets" || fail "mkdir fixture ci/targets"
	write_target local-stub "$STUB_TARGET"
	write_target local-badmodule "$(printf '%s\n' "$STUB_TARGET" | sed "s|^modules=.*|modules='stub/1.0 nosuch/9'|")"
	if [ -n "${EXTRA_TARGETS:-}" ]; then $EXTRA_TARGETS; fi
	( cd "$fx" && git init -q ) || fail "git init fixture"
	good=$(fx_commit good) || exit 1
	default_branch=$(git -C "$fx" symbolic-ref --short HEAD) || fail "fixture default branch"

	git -C "$fx" checkout -q -b red-build "$good" || fail "fixture branch red-build"
	printf '\n#error deliberate gate failure\n' >>"$fx/src/main.cpp"
	red_build=$(fx_commit red-build) || exit 1

	git -C "$fx" checkout -q -b red-check "$good" || fail "fixture branch red-check"
	{ head -n 1 "$fx/tests/cli.sh"; echo 'echo " FAIL gate deliberate failure"; exit 1'; tail -n +2 "$fx/tests/cli.sh"; } >"$work/cli.sh" \
		&& mv "$work/cli.sh" "$fx/tests/cli.sh" || fail "fixture red-check edit"
	red_check=$(fx_commit red-check) || exit 1

	git -C "$fx" checkout -q -b slow "$good" || fail "fixture branch slow"
	printf '\n.DEFAULT_GOAL := gate-slow\ngate-slow:\n\tsleep 120\n' >>"$fx/Makefile"
	slow=$(fx_commit slow) || exit 1

	git -C "$fx" checkout -q "$default_branch" || fail "fixture checkout $default_branch"
	echo 'a later commit the job must not build' >"$fx/NEWER"
	newer=$(fx_commit newer) || exit 1

	trap_ref=deadbeefdeadbeef
	for h in "$good" "$red_build" "$red_check" "$slow" "$newer"; do
		case $h in "$trap_ref"*) fail "fixture hash $h starts with the trap ref; rerun" ;; esac
	done
	git -C "$fx" -c tag.gpgSign=false tag "$trap_ref" "$good" || fail "fixture tag"
	git clone -q --bare "$fx" "$work/repo.git" || fail "bare clone of the fixture"
	test "$(git -C "$work/repo.git" rev-parse HEAD)" = "$newer" || fail "the fixture's HEAD is not the newer commit"
	url="file://$work/repo.git"
}

# make_stubs: the gate's tools.
#   $work/bin        shadowed interpreters (any use leaves a marker in $work/forbidden); no module
#   $work/modbin     an executable fake module, for the branch where module is already a command
#   $work/toolchain  gate-c++, a clang++ wrapper with its own --version line
#   $work/etc/profile  the fake login profile: a module() function whose load puts the toolchain on
#                    PATH, SCRATCH set, MAKEFLAGS, CXX and CDPATH set to harmful values, and a record
#                    in $work/profile.log of whether HOME existed
# and the job copy that runs, outside any repository, with /etc/profile pointed at the fake.
make_stubs() {
	bin="$work/bin"
	mkdir -p "$bin" "$work/modbin" "$work/toolchain" "$work/etc" "$work/forbidden" "$work/submit" || fail "mkdir stubs"
	for t in python python3 jq perl ruby node; do
		printf '#!/bin/sh\n: >"%s/%s"\necho "forbidden tool: %s" >&2\nexit 127\n' "$work/forbidden" "$t" "$t" >"$bin/$t"
	done
	cat >"$work/modbin/module" <<'EOF'
#!/bin/sh
# Fake module command: logs each call, accepts purge and load stub/*, refuses any other load.
echo "fake-module: $*" >&2
case ${1-} in
	purge) exit 0 ;;
	load)
		shift
		for m in "$@"; do
			case $m in
				stub/*) ;;
				*) echo "fake-module: no module named $m" >&2; exit 1 ;;
			esac
		done
		exit 0 ;;
	*) exit 0 ;;
esac
EOF
	printf '#!/bin/sh\nif [ "${1-}" = --version ]; then echo "%s"; exit 0; fi\nexec clang++ "$@"\n' "$WRAPPER_LINE" >"$work/toolchain/gate-c++"
	cat >"$work/etc/profile" <<EOF
# Fake login profile for the gates.
if [ -e "\$HOME" ]; then gate_home=present; else gate_home=absent; fi
echo "job=\${SLURM_JOB_ID-} home=\$gate_home" >>"$work/profile.log"
module() {
	echo "fake-module: \$*" >&2
	case \${1-} in
		purge) return 0 ;;
		load)
			shift
			for gate_m in "\$@"; do
				case \$gate_m in
					stub/*) PATH="$work/toolchain:\$PATH"; export PATH ;;
					*) echo "fake-module: no module named \$gate_m" >&2; return 1 ;;
				esac
			done
			return 0 ;;
	esac
	return 0
}
if [ -z "\${GATE_PROFILE_NO_SCRATCH-}" ]; then SCRATCH=\$GATE_SANDBOX/scr; export SCRATCH; fi
# Site profiles set build variables too; the job must clear them again after loading this.
MAKEFLAGS=n CXX=false CDPATH=$work/decoy
export MAKEFLAGS CXX CDPATH
EOF
	chmod +x "$bin"/* "$work/modbin/module" "$work/toolchain/gate-c++" || fail "chmod stubs"
	: >"$work/profile.log"
	grep -q '/etc/profile' "$JOB" || fail "the job does not name /etc/profile, the login environment it loads"
	sed "s|/etc/profile|$work/etc/profile|g" "$JOB" >"$work/submit/test-commit.sh" || fail "copy the job outside the repository"
	sed "s|$work/etc/profile||g" "$work/submit/test-commit.sh" | grep -q '/etc/profile' && fail "the profile path was not rewritten in the job copy"
	mkdir -p "$work/decoy" && git -C "$work/decoy" init -q || fail "decoy repository"
}

# profile_line JOBID: the fake profile's record for that job, or nothing
profile_line() {
	grep "^job=$1 " "$work/profile.log"
}

# reset_g: clear the per-run overrides read by run_job.
#   G_NO_MPS_SCRATCH=1  leave MPS_SCRATCH unset      G_MPS_SCRATCH  MPS_SCRATCH (default: the sandbox's)
#   G_SCRATCH           set SCRATCH                  G_URL          MPS_REPO_URL (default: the fixture)
#   G_CLUSTER           SLURM_CLUSTER_NAME           G_PARTITION    SLURM_JOB_PARTITION
#   G_MODULE_EXE=1      module and the toolchain already on PATH (the profile is not needed)
#   G_PATH_PREFIX       directories put first on PATH
#   G_NO_PROFILE_SCRATCH=1  the fake profile sets no SCRATCH
#   G_POLLUTE=1         inherited variables that would redirect git, make or the compiler
reset_g() {
	G_NO_MPS_SCRATCH= G_MPS_SCRATCH= G_SCRATCH= G_URL= G_CLUSTER=localtest G_PARTITION=local
	G_MODULE_EXE= G_PATH_PREFIX= G_NO_PROFILE_SCRATCH= G_POLLUTE=
}

# run_job NAME SANDBOX SHELL JOBID [JOB-ARGS...]: run the job copy under `env -i` from SANDBOX/cwd.
# Sets rc, sb, t_before, t_after; stdout and stderr land in $logs/NAME.out and .err.
run_job() {
	name=$1 sb="$work/sb-$2" shell=$3 jid=$4
	shift 4
	mkdir -p "$sb/scr" "$sb/home" "$sb/tmp" "$sb/cwd" || fail "mkdir sandbox $sb"
	p="$work/bin:/usr/bin:/bin:/usr/sbin:/sbin"
	[ -z "$G_MODULE_EXE" ] || p="$work/bin:$work/modbin:$work/toolchain:/usr/bin:/bin:/usr/sbin:/sbin"
	[ -z "$G_PATH_PREFIX" ] || p="$G_PATH_PREFIX:$p"
	envfile="$work/env-$name"
	{
		printf '%s\n' "PATH=$p"
		printf '%s\n' "HOME=$sb/home"
		printf '%s\n' "TMPDIR=$sb/tmp"
		printf '%s\n' "LC_ALL=C"
		printf '%s\n' "GATE_SANDBOX=$sb"
		printf '%s\n' "SLURM_JOB_ID=$jid"
		printf '%s\n' "SLURM_CLUSTER_NAME=$G_CLUSTER"
		printf '%s\n' "SLURM_JOB_PARTITION=$G_PARTITION"
		printf '%s\n' "MPS_REPO_URL=${G_URL:-$url}"
		[ -n "$G_NO_MPS_SCRATCH" ] || printf '%s\n' "MPS_SCRATCH=${G_MPS_SCRATCH:-$sb/scr}"
		[ -z "$G_SCRATCH" ] || printf '%s\n' "SCRATCH=$G_SCRATCH"
		[ -z "$G_NO_PROFILE_SCRATCH" ] || printf '%s\n' "GATE_PROFILE_NO_SCRATCH=1"
		if [ -n "$G_POLLUTE" ]; then
			printf '%s\n' "CDPATH=$work/decoy"
			printf '%s\n' "GIT_DIR=$work/decoy/.git"
			printf '%s\n' "GIT_WORK_TREE=$work/decoy"
			printf '%s\n' "GIT_CONFIG_PARAMETERS='url.file:///nonexistent/.insteadof'='file://'"
			printf '%s\n' "MAKEFLAGS=n"
			printf '%s\n' "MAKELEVEL=3"
			printf '%s\n' "MFLAGS=-n"
			printf '%s\n' "CXX=false"
			printf '%s\n' "CXXFLAGS=-O3 -DGATE_POISON"
		fi
	} >"$envfile"
	t_before=$(now)
	# The environment goes through a file, one assignment per line, so no value is word-split.
	(
		cd "$sb/cwd" || exit 125
		set -f
		IFS='
'
		# shellcheck disable=SC2046
		exec env -i $(cat "$envfile") "$shell" "$work/submit/test-commit.sh" "$@"
	) >"$logs/$name.out" 2>"$logs/$name.err"
	rc=$?
	t_after=$(now)
}

# rundir SANDBOX HASH TARGET JOBID: where the job must put that run
rundir() {
	echo "$work/sb-$1/scr/mpsearch/runs/$2/$3/$4"
}

# outside_runs SANDBOX-PATH RUN...: print each path in the sandbox that is not allowed
outside_runs() {
	osb=$1
	shift
	( cd "$osb" && find . -mindepth 1 ) | while IFS= read -r p; do
		full="$osb/${p#./}"
		case $p in
			./scr|./home|./tmp|./cwd|./other) continue ;;
		esac
		ok=no
		for o_r in "$@"; do
			case "$o_r/" in
				"$full/"*) ok=yes ;;
			esac
			case $full in
				"$o_r"/*) ok=yes ;;
			esac
		done
		[ "$ok" = yes ] || echo "$p"
	done
}

# contained SANDBOX [RUN...]: nothing exists in the sandbox but its four directories, the runs named,
# their contents and their ancestors; home, tmp and cwd are empty.
contained() {
	csb="$work/sb-$1"
	shift
	test -d "$csb/scr" || fail "contained: no sandbox $csb (gate bug)"
	outside_runs "$csb" "$@" >"$work/outside" || fail "listing the sandbox"
	extra=$(cat "$work/outside")
	test -z "$extra" || { echo "$extra" | head -20 >&2; fail "the job wrote outside its run directory in sandbox ${csb##*/} (R5)"; }
	for d in home tmp cwd; do
		test -z "$(ls -A "$csb/$d")" || fail "the job wrote into the caller's $d directory (R5)"
	done
}

forbidden_none() {
	test -z "$(ls -A "$work/forbidden")" || fail "the job ran a forbidden interpreter: $(ls "$work/forbidden" | tr '\n' ' ')(R10)"
}

# step_lines NAME: the step announcements in run NAME's stdout, space-separated
step_lines() {
	grep -E '^mps-test: (clone|checkout|toolchain|build|check)$' "$logs/$1.out" | sed 's/^mps-test: //' | tr '\n' ' ' | sed 's/ $//'
}

# script_line NAME: the script hash the job printed in run NAME
script_line() {
	sed -n 's/^mps-test: script \([0-9a-f]\{40\}\)$/\1/p' "$logs/$1.out" | head -n 1
}

# compiled_with LOG CXX MIN: at least MIN compile lines, each run by CXX with -O1 and without -O3
compiled_with() {
	n=$(grep -c -- ' -c ' "$1")
	test "$n" -ge "$3" || fail "${1##*/}: $n compile lines, want at least $3"
	other=$(grep -E -- ' -c | -o bin/' "$1" | grep -v "^$2 " || true)
	test -z "$other" || { echo "$other" >&2; fail "${1##*/}: build lines not run by $2: the loaded toolchain or the target's CXX did not reach make (R6)"; }
	no1=$(grep -- ' -c ' "$1" | grep -v -- ' -O1 ' || true)
	test -z "$no1" || { echo "$no1" >&2; fail "${1##*/}: compile lines without the target's CXXFLAGS=-O1 (R6)"; }
	! grep -- ' -c ' "$1" | grep -q -- '-O3' || fail "${1##*/}: -O3 on a compile line: the target's CXXFLAGS did not override (R6)"
}

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
	test -f "$f" || fail "no result.json at ${f#"$work"/} (R7)"
	python3 -I "$work/check_result.py" "$f" "$@" || fail "result.json is wrong (R7)"
}

# expect_logs RUN STEPS-THAT-RAN: exactly those step logs exist (R6)
expect_logs() {
	for el_s in $STEPS; do
		case " $2 " in
			*" $el_s "*) test -f "$1/$el_s.log" || fail "$el_s ran but $el_s.log is missing (R6)" ;;
			*) test ! -e "$1/$el_s.log" || fail "$el_s did not run but $el_s.log exists (R6)" ;;
		esac
	done
}

# target_value FILE KEY: the value a line parser reads (single quotes stripped); empty if absent
target_value() {
	sed -n "s/^$2=//p" "$1" | sed "s/^'\(.*\)'\$/\1/"
}

# The site-private scan (R9). Literal values come from a site file outside the repository; the scan
# fails when the file is missing or any value is empty. Words match case-insensitively as words, QoS
# names only in Slurm-option contexts, path prefixes as substrings; the public repository URL is the
# one allowed appearance of its owner's name.
SITE_ENV=${MPS_SITE_ENV:-$HOME/.config/mpsearch/site.env}
cat >"$work/scan.py" <<'EOF'
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
allowed = re.compile(re.escape(url), re.I)
hits = 0
for name in sys.argv[3:]:
    try:
        text = open(name, errors="replace").read()
    except OSError as e:
        print("%s: %s" % (name, e), file=sys.stderr)
        sys.exit(2)
    for n, line in enumerate(text.splitlines(), 1):
        clean = allowed.sub("<public-url>", line)
        for rx, what in ((word_re, "site name"), (qos_re, "QoS"), (path_re, "site path")):
            m = rx.search(clean)
            if m:
                hits += 1
                print("%s:%d: %s %r" % (name, n, what, m.group(0)), file=sys.stderr)
sys.exit(1 if hits else 0)
EOF

# scan_site_private FILE...: R9 over the given files
scan_site_private() {
	python3 -I "$work/scan.py" "$SITE_ENV" "${DEFAULT_URL%.git}" "$@"
	case $? in
		0) ;;
		2) fail "the site-private scan cannot run: site file or input missing (R9; set MPS_SITE_ENV)" ;;
		*) fail "site-private detail in what this cycle commits (R9)" ;;
	esac
}

# cycle_files: every file this cycle adds or changes against origin/main, committed or not
cycle_files() {
	( cd "$root" && { git diff --name-only origin/main...HEAD; git diff --name-only HEAD; git ls-files --others --exclude-standard; } \
		| sort -u | while IFS= read -r p; do [ -f "$p" ] && echo "$root/$p"; done )
}

# scan_cycle: the scan over every file the cycle touches and over the messages and patches of
# origin/main...HEAD; the file list must be non-empty and cover the job.
scan_cycle() {
	cycle_files >"$work/files"
	test -s "$work/files" || fail "no cycle files found to scan"
	grep -q '/ci/test-commit.sh$' "$work/files" || fail "the scan does not cover ci/test-commit.sh"
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
}
