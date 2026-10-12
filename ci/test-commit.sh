#!/bin/sh
# SPDX-FileCopyrightText: 2026 Geoffrey Lentner
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
#
# Cluster test job
#
# Usage: test-commit.sh HASH TARGET
#
#   HASH     the commit to test: 7 to 40 lower-case hex digits that begin the name of exactly
#            one commit in the repository (a branch or tag name never counts)
#   TARGET   the name of a file under ci/targets/ in that commit
#
# A Slurm batch job that builds and checks exactly one commit on one target. It clones the
# repository into a new run directory in scratch, checks out HASH, loads the target's modules,
# and runs `make` and `make check` with the target's make variables. Nothing is cloned or built
# before submission and nothing from an earlier run is reused. It needs a POSIX shell and its
# standard utilities, git, the site's module command and the target's toolchain, and nothing else.
#
# Environment:
#
#   MPS_REPO_URL          the repository to clone (default https://github.com/glentner/mpsearch.git)
#   MPS_SCRATCH           the directory runs live under; wins over SCRATCH
#   SCRATCH               the site's scratch directory, used when MPS_SCRATCH is not set
#   SLURM_JOB_ID          names the run directory (local-<pid> outside Slurm)
#   SLURM_JOB_PARTITION   when set, must equal the target's partition
#   SLURM_CLUSTER_NAME    recorded in result.json
#   MPS_SLURM_ACCOUNT     read by the submitter, never by the job: the account to charge
#   MPS_SLURM_QOS         read by the submitter, never by the job: the quality of service
#
# A job submitted from a non-login shell may have no module command and no scratch variable. When
# either is missing, the job sources /etc/profile first, with HOME pointing at a path that cannot
# exist, so the profile and the module system write nothing into the real home directory.
# Inherited variables that would redirect git, make or the compiler (GIT_DIR, MAKEFLAGS, CXX and
# their kin) are cleared before the profile and again after it; the target says what the build uses.
#
# Run directory:
#
#   <scratch>/mpsearch/runs/<hash>/<target>/<jobid>/
#       src/             the clone, checked out at the commit
#       home/  tmp/      HOME and TMPDIR for every step
#       clone.log  checkout.log  toolchain.log  build.log  check.log
#                        each step's output, for the steps that ran
#       result.json      the outcome, written last (through result.json.tmp)
#
# The run directory must not exist beforehand, so a requeued job refuses to run rather than reuse
# anything. Apart from the scheduler's own output file, the job writes nothing outside it.
#
# The steps run in order: clone, checkout, toolchain (module purge, then each module in order, then
# the compiler make will use), build and check. Each announces itself on stdout as `mps-test: STEP`.
# A failed step prints `mps-test: STEP failed` and the last 20 lines of its log on stderr, and the
# steps after it do not run. The checkout step fails with `HASH is not a commit in URL` when no
# commit's name begins with HASH, and with `HASH is ambiguous in URL`, listing the commits, when
# more than one does; blobs, trees and tags never count. Before the steps, the job prints
# `mps-test: script BLOB`, the git blob hash of the script it is running; its last line names the
# outcome and the run directory.
#
# result.json holds format, requested (HASH as given), commit (the full hash, once checked out),
# target, cluster, host, arch, job_id, start and end (UTC), compiler and make (the first line of
# each one's --version), steps (in order, each with name, status and seconds) and status. A value
# that does not exist is null; a step that did not run is not_run with null seconds. The file is
# written on every path that reaches the steps, including a SIGTERM at the time limit. Any change
# to its shape bumps format, now 1.
#
# Exit status: 0 when every step passed; 1 when a step failed or the job was interrupted; 2 for bad
# arguments; 3 when there is no usable scratch directory or the run directory already exists.
#
# Target files:
#
#   ci/targets/<name> holds one key=value per line, with comments (# in the first column) and
#   blank lines allowed. A value is either bare, made only of letters, digits and _./:,+=@%-, or
#   in single quotes with no quote inside, so a POSIX shell can source the file and a line parser
#   reads the same values. The job reads it line by line and never executes it: any other line, an
#   unknown key or a key set twice is refused by line number, and every key but description and
#   gpus is required and must not be empty. The keys and the sbatch options they become:
#
#     description   free text, naming the hardware
#     partition     --partition
#     gpus          --gpus, left out when empty
#     cpus          --cpus-per-task, with --ntasks=1
#     mem           --mem
#     time          --time
#     modules       loaded in order by the toolchain step
#     make_vars     given to make in the build and check steps
#
#   Every submission also sets --output to a file in scratch (the scheduler would otherwise write
#   into the submitting directory), and --account and --qos from MPS_SLURM_ACCOUNT and
#   MPS_SLURM_QOS. Neither is ever committed, and neither is which cluster serves a target.
#
# Submission:
#
#   Copy this script from the commit under test into scratch, then submit it by path with HASH
#   and TARGET as its arguments, so the job is the tested commit's own:
#
#     git show HASH:ci/test-commit.sh > <scratch>/mpsearch/jobs/test-commit-HASH.sh
#     sbatch --partition=... --ntasks=1 --cpus-per-task=... --mem=... --time=... \
#         --output=<scratch>/mpsearch/jobs/%j.out <scratch>/mpsearch/jobs/test-commit-HASH.sh HASH TARGET
#
#   Through a cluster's MCP server, write_file the script into scratch, then call sbatch with
#   script_path="<file> HASH TARGET". Never submit it as inline content: that cannot carry the
#   arguments, and it writes into the home directory.
#

LC_ALL=C
export LC_ALL
set -u
set -f

# The original stderr and stdout, for messages that must reach the scheduler output while a
# step's own output goes to its log.
exec 3>&2 4>&1

DEFAULT_URL=https://github.com/glentner/mpsearch.git
PROFILE=/etc/profile
STEPS='clone checkout toolchain build check'


clear_inherited() {
	#
	# Clear inherited variables
	#
	# The shell's own startup hooks, then everything that would point git at another
	# repository or configuration, change how make runs, or pick the compiler and its
	# flags. The target's make variables and modules decide what the build uses.

	unset CDPATH ENV BASH_ENV \
		GIT_DIR GIT_WORK_TREE GIT_INDEX_FILE GIT_OBJECT_DIRECTORY \
		GIT_ALTERNATE_OBJECT_DIRECTORIES GIT_NAMESPACE GIT_CEILING_DIRECTORIES \
		GIT_CONFIG GIT_CONFIG_PARAMETERS GIT_CONFIG_COUNT GIT_CONFIG_GLOBAL \
		GIT_CONFIG_SYSTEM GIT_TEMPLATE_DIR \
		MAKEFLAGS MAKELEVEL MFLAGS MAKEFILES GNUMAKEFLAGS MAKEOVERRIDES \
		CXX CC CXXFLAGS CPPFLAGS LDFLAGS LDLIBS
	unset IFS
}


refuse() { # STATUS MESSAGE: before any run directory exists, say why and stop
	printf 'mps-test: %s\n' "$2" >&2
	exit "$1"
}


now() {
	date -u +%Y-%m-%dT%H:%M:%SZ
}


json_str() { # VALUE: a JSON string; anything but printable ASCII is dropped
	printf '"%s"' "$(printf '%s' "$1" | tr -cd '\040-\176' | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g')"
}


json_or_null() { # VALUE: a JSON string, or null when VALUE is empty
	if [ -n "$1" ]; then json_str "$1"; else printf 'null'; fi
}


set_step() { # NAME STATUS SECONDS
	case $1 in
		clone)     clone_status=$2     clone_seconds=$3 ;;
		checkout)  checkout_status=$2  checkout_seconds=$3 ;;
		toolchain) toolchain_status=$2 toolchain_seconds=$3 ;;
		build)     build_status=$2     build_seconds=$3 ;;
		check)     check_status=$2     check_seconds=$3 ;;
	esac
}


step_status() { # NAME: print that step's status
	case $1 in
		clone)     printf '%s' "$clone_status" ;;
		checkout)  printf '%s' "$checkout_status" ;;
		toolchain) printf '%s' "$toolchain_status" ;;
		build)     printf '%s' "$build_status" ;;
		check)     printf '%s' "$check_status" ;;
	esac
}


step_seconds() { # NAME: print that step's seconds
	case $1 in
		clone)     printf '%s' "$clone_seconds" ;;
		checkout)  printf '%s' "$checkout_seconds" ;;
		toolchain) printf '%s' "$toolchain_seconds" ;;
		build)     printf '%s' "$build_seconds" ;;
		check)     printf '%s' "$check_seconds" ;;
	esac
}


write_result() { # STATUS: result.json, through a temporary file and a rename
	{
		printf '{\n'
		printf '  "format": 1,\n'
		printf '  "requested": %s,\n' "$(json_str "$hash")"
		printf '  "commit": %s,\n' "$(json_or_null "$commit")"
		printf '  "target": %s,\n' "$(json_str "$target")"
		printf '  "cluster": %s,\n' "$(json_or_null "${SLURM_CLUSTER_NAME:-}")"
		printf '  "host": %s,\n' "$(json_str "$(uname -n)")"
		printf '  "arch": %s,\n' "$(json_str "$(uname -m)")"
		printf '  "job_id": %s,\n' "$(json_str "$jobid")"
		printf '  "start": %s,\n' "$(json_str "$start")"
		printf '  "end": %s,\n' "$(json_str "$(now)")"
		printf '  "compiler": %s,\n' "$(json_or_null "$compiler")"
		printf '  "make": %s,\n' "$(json_or_null "$make_line")"
		printf '  "steps": [\n'
		sep=','
		for s in $STEPS; do
			[ "$s" != check ] || sep=
			printf '    {"name": "%s", "status": "%s", "seconds": %s}%s\n' \
				"$s" "$(step_status "$s")" "$(step_seconds "$s")" "$sep"
		done
		printf '  ],\n'
		printf '  "status": "%s"\n' "$1"
		printf '}\n'
	} >"$run/result.json.tmp" && mv -f "$run/result.json.tmp" "$run/result.json"
}


finish() { # EXIT: write result.json, name the outcome and the run directory, and exit
	trap '' TERM INT HUP
	if [ "$1" -eq 0 ]; then outcome=passed; else outcome=failed; fi
	write_result "$outcome" || printf 'mps-test: could not write %s/result.json\n' "$run" >&3
	printf 'mps-test: %s in %s\n' "$outcome" "$run" >&4
	exit "$1"
}


interrupted() { # SIGNAL: the scheduler is stopping the job; record what was running
	trap '' TERM INT HUP
	if [ -z "$current" ]; then
		for s in $STEPS; do
			if [ "$(step_status "$s")" = not_run ]; then current=$s; break; fi
		done
		# Every step has already passed: the outcome stands.
		[ -n "$current" ] || finish 0
		step_start=$(date +%s)
	fi
	set_step "$current" failed $(( $(date +%s) - step_start ))
	printf 'mps-test: interrupted (%s) during %s\n' "$1" "$current" >&3
	finish 1
}


run_step() { # NAME FUNCTION: run one step in this shell, its output in NAME.log
	current=$1
	printf 'mps-test: %s\n' "$1"
	step_start=$(date +%s)
	"$2" >"$run/$1.log" 2>&1 </dev/null
	step_rc=$?
	if [ "$step_rc" -eq 0 ]; then step_end=passed; else step_end=failed; fi
	set_step "$1" "$step_end" $(( $(date +%s) - step_start ))
	current=
	if [ "$step_rc" -ne 0 ]; then
		printf 'mps-test: %s failed\n' "$1" >&2
		tail -n 20 "$run/$1.log" >&2
		finish 1
	fi
}


do_clone() {
	git clone --quiet --no-checkout -- "$url" "$run/src"
}


do_checkout() {
	#
	# Check out exactly the commit
	#
	# The hash must begin the name of exactly one commit. Every object whose name begins with
	# it is listed and asked its type, so only object names count (a branch or tag whose name
	# looks like hex can never stand in for a commit) and no git message is read. Blobs, trees
	# and tags that share the prefix are not commits and do not compete with one.

	objects=$(git -C "$run/src" rev-parse --disambiguate="$hash") || {
		printf 'mps-test: cannot list the objects named %s in %s\n' "$hash" "$url"
		return 1
	}
	full= candidates= ncommits=0 others=
	for obj in $objects; do
		case $obj in
			"$hash"*) ;;
			*) continue ;;
		esac
		ty=$(git -C "$run/src" cat-file -t "$obj") || return 1
		if [ "$ty" = commit ]; then
			full=$obj
			candidates="$candidates $obj"
			ncommits=$((ncommits + 1))
		else
			others="${others:+$others, }a $ty"
		fi
	done
	case $ncommits in
		0)
			if [ -n "$others" ]; then
				printf 'mps-test: %s is not a commit in %s (it names %s)\n' "$hash" "$url" "$others"
			else
				printf 'mps-test: %s is not a commit in %s\n' "$hash" "$url"
			fi
			return 1 ;;
		1) ;;
		*)
			printf 'mps-test: %s is ambiguous in %s: %s commits begin with it\n' "$hash" "$url" "$ncommits"
			for obj in $candidates; do
				printf 'mps-test:   %s\n' "$obj"
			done
			return 1 ;;
	esac
	git -C "$run/src" checkout --quiet --detach "$full" || return 1
	head=$(git -C "$run/src" rev-parse HEAD) || return 1
	if [ "$head" != "$full" ]; then
		printf 'mps-test: checked out %s, wanted %s\n' "$head" "$full"
		return 1
	fi
	commit=$full
}


target_line() { # NUMBER KEY VALUE: take one line of the target file
	case $2 in
		description|partition|gpus|cpus|mem|time|modules|make_vars) ;;
		*)
			printf 'mps-test: ci/targets/%s line %s: unknown key %s\n' "$target" "$1" "$2"
			return 1 ;;
	esac
	case $3 in
		"'"*"'")
			tl_value=${3#"'"}
			tl_value=${tl_value%"'"}
			case $tl_value in
				*"'"*)
					printf 'mps-test: ci/targets/%s line %s: a quote inside a quoted value\n' "$target" "$1"
					return 1 ;;
			esac ;;
		*[!A-Za-z0-9_./:,+=@%-]*)
			printf 'mps-test: ci/targets/%s line %s: an unquoted value may hold only letters, digits and _./:,+=@%%-\n' "$target" "$1"
			return 1 ;;
		*)
			tl_value=$3 ;;
	esac
	case " $seen " in
		*" $2 "*)
			printf 'mps-test: ci/targets/%s line %s: %s is set twice\n' "$target" "$1" "$2"
			return 1 ;;
	esac
	seen="$seen $2"
	[ -z "$tl_value" ] || filled="$filled $2"
	case $2 in
		description) t_description=$tl_value ;;
		partition)   t_partition=$tl_value ;;
		gpus)        t_gpus=$tl_value ;;
		cpus)        t_cpus=$tl_value ;;
		mem)         t_mem=$tl_value ;;
		time)        t_time=$tl_value ;;
		modules)     t_modules=$tl_value ;;
		make_vars)   t_make_vars=$tl_value ;;
	esac
}


read_target() { # FILE: parse the target file, line by line, without executing any of it
	seen= filled=
	t_description= t_partition= t_gpus= t_cpus= t_mem= t_time= t_modules= t_make_vars=
	n=0
	while IFS= read -r line || [ -n "$line" ]; do
		n=$((n + 1))
		case $line in
			''|'#'*) continue ;;
			[a-z_]*=*) target_line "$n" "${line%%=*}" "${line#*=}" || return 1 ;;
			*)
				printf 'mps-test: ci/targets/%s line %s: not a comment or a key=value assignment\n' "$target" "$n"
				return 1 ;;
		esac
	done <"$1"
	for k in partition cpus mem time modules make_vars; do
		case " $seen " in
			*" $k "*) ;;
			*)
				printf 'mps-test: ci/targets/%s: missing required key %s\n' "$target" "$k"
				return 1 ;;
		esac
		case " $filled " in
			*" $k "*) ;;
			*)
				printf 'mps-test: ci/targets/%s: required key %s is empty\n' "$target" "$k"
				return 1 ;;
		esac
	done
}


do_toolchain() {
	#
	# Load the target's toolchain
	#
	# The target file comes from the commit under test, so a run depends on nothing outside
	# it. The modules load onto a purged baseline, and make itself names the compiler it will
	# use with the target's make variables.

	file="$run/src/ci/targets/$target"
	if [ ! -f "$file" ]; then
		printf 'mps-test: no target file ci/targets/%s in commit %s\n' "$target" "$commit"
		return 1
	fi
	read_target "$file" || return 1
	if [ -n "${SLURM_JOB_PARTITION:-}" ] && [ "$SLURM_JOB_PARTITION" != "$t_partition" ]; then
		printf 'mps-test: running in partition %s, but target %s is for partition %s\n' \
			"$SLURM_JOB_PARTITION" "$target" "$t_partition"
		return 1
	fi
	if ! command -v module >/dev/null 2>&1; then
		printf 'mps-test: no module command, even after %s\n' "$PROFILE"
		return 1
	fi
	set +u
	module purge || { set -u; return 1; }
	for m in $t_modules; do
		module load "$m" || { set -u; printf 'mps-test: module load %s failed\n' "$m"; return 1; }
	done
	set -u
	cxx=$(cd "$run/src" && printf 'mps-print-cxx:\n\t@echo $(CXX)\n' \
		| make -s -f Makefile -f - mps-print-cxx $t_make_vars) || return 1
	if [ -z "$cxx" ]; then
		printf 'mps-test: make names no compiler\n'
		return 1
	fi
	compiler=$($cxx --version 2>/dev/null | head -n 1)
	make_line=$(make --version 2>/dev/null | head -n 1)
	printf 'compiler: %s\nmake: %s\n' "$compiler" "$make_line"
	if [ -z "$compiler" ] || [ -z "$make_line" ]; then
		printf 'mps-test: no version line from %s or from make\n' "$cxx"
		compiler= make_line=
		return 1
	fi
}


do_build() {
	(cd "$run/src" && exec make $t_make_vars)
}


do_check() {
	(cd "$run/src" && exec make $t_make_vars check)
}


clear_inherited

# Arguments: both become path components, so they are checked before anything else happens.
if [ $# -ne 2 ]; then
	refuse 2 'usage: test-commit.sh HASH TARGET'
fi
hash=$1
target=$2
case $hash in
	''|*[!0-9a-f]*) refuse 2 "'$hash' is not a commit hash: want 7 to 40 lower-case hex digits" ;;
esac
if [ ${#hash} -lt 7 ] || [ ${#hash} -gt 40 ]; then
	refuse 2 "'$hash' is not a commit hash: want 7 to 40 lower-case hex digits"
fi
case $target in
	''|[!A-Za-z0-9]*|*[!A-Za-z0-9._-]*) refuse 2 "'$target' is not a target name: want letters, digits, '.', '_' and '-'" ;;
esac

# The script's own path, before any cd.
case $0 in
	/*) script=$0 ;;
	*)  script=$(pwd)/$0 ;;
esac
start=$(now)

# The login environment, when the job was submitted without it. HOME points at a path that
# cannot exist, so the profile and the module system cannot write into the real home directory.
if ! command -v module >/dev/null 2>&1 || [ -z "${MPS_SCRATCH:-}${SCRATCH:-}" ]; then
	if [ -r "$PROFILE" ]; then
		HOME=/dev/null/home
		export HOME
		set +u +f
		. "$PROFILE" >&2 </dev/null
		set -u -f
		LC_ALL=C
		export LC_ALL
		clear_inherited
	fi
fi

# Scratch and a new run directory. A plain mkdir fails when the directory exists, so a run never
# shares it with another, and nothing in it is touched.
scratch=${MPS_SCRATCH:-${SCRATCH:-}}
case $scratch in
	'') refuse 3 'no scratch directory: set MPS_SCRATCH or SCRATCH' ;;
	/*) ;;
	*)  refuse 3 "scratch '$scratch' is not an absolute path" ;;
esac
[ -d "$scratch" ] || refuse 3 "scratch '$scratch' is not a directory"
jobid=${SLURM_JOB_ID:-local-$$}
case $jobid in
	''|[!A-Za-z0-9]*|*[!A-Za-z0-9._-]*) refuse 3 "'$jobid' cannot name a run directory" ;;
esac
run=$scratch/mpsearch/runs/$hash/$target/$jobid
mkdir -p "${run%/*}" || refuse 3 "cannot create ${run%/*}"
if [ -e "$run" ]; then
	refuse 3 "run directory $run already exists"
fi
mkdir "$run" 2>/dev/null || refuse 3 "cannot create run directory $run (it may already exist)"

# Everything from here on writes only inside the run directory.
cd "$run" || refuse 3 "cannot enter $run"
mkdir home tmp || refuse 3 "cannot create home/ and tmp/ in $run"
HOME=$run/home
TMPDIR=$run/tmp
GIT_TERMINAL_PROMPT=0
export HOME TMPDIR GIT_TERMINAL_PROMPT
unset XDG_CONFIG_HOME XDG_CACHE_HOME XDG_DATA_HOME XDG_STATE_HOME
url=${MPS_REPO_URL:-$DEFAULT_URL}

printf 'mps-test: script %s\n' "$(git hash-object --no-filters -- "$script" 2>/dev/null || echo unknown)"

commit= compiler= make_line= current= step_start=0
for s in $STEPS; do
	set_step "$s" not_run null
done
trap 'interrupted TERM' TERM
trap 'interrupted INT' INT
trap 'interrupted HUP' HUP

run_step clone do_clone
run_step checkout do_checkout
run_step toolchain do_toolchain
run_step build do_build
run_step check do_check
finish 0
