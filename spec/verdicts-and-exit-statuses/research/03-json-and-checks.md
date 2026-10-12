# 03 — JSON records and the checks

Research brief for `code-plan`. The topic is how the JSON record is written without a dependency,
and how `make check` and the phase gates prove R6–R13. Experiments ran in scratch on a copy of the
tree at `25d13b5` (`make check` gave 20/0 and 126/0). `mps 31`, `mps 0000000031`, `mps 999999999`
and `mps -t ll 31` all exit 0 today.

## 1. Emission

**Shape.** The record is one object per line on stdout, ended by `\n`, with a fixed key order.
Use the order R10 lists:

`exponent`, `worktype`, `status`, then exactly one of `factors` | `res64` | `reason` (and none of
them for a prime or a probable prime), then `program` and `record_version`.

Build the record with string concatenation in the result module. No library is needed.

**Strings.** RFC 8259 § 7 requires three escapes:
- `"` becomes `\"`;
- `\` becomes `\\`;
- every byte below 0x20 becomes `\u00XX`.

Bytes from 0x20 up may pass through. The reasons and the factor strings are ASCII literals we
control, but the escaper must still be correct. A `\n` in a reason would split one record into two
lines, and the checks should be able to prove it can't. Prototyped (in scratch, compiled clean with
`-Wall -Wextra`, and its output parsed by Python):

```cpp
static std::string json_string(const std::string& text) {
	//
	// A JSON string literal for text
	//
	// Escapes the quote, the backslash and each control character below 0x20 (RFC 8259, 2017,
	// section 7). Bytes from 0x20 up pass through unchanged.

	static const char HEX[] = "0123456789abcdef";
	std::string out = "\"";
	for (unsigned char c: text) {
		if (c == '"' or c == '\\') {
			out += '\\';
			out += char(c);
		} else if (c < 0x20) {
			out += "\\u00";
			out += HEX[c >> 4];
			out += HEX[c & 0xf];
		} else {
			out += char(c);
		}
	}
	return out + "\"";
}
```

**Numbers.**
- Write `exponent` from the parsed `opt.pval_number` with `std::to_string`, never from the argv
  text, so `mps -j 0000000031` writes `"exponent": 31` (a leading zero is invalid JSON).
- The verdict line uses the same rule and says `M31`, not `M0000000031`.
- `record_version` is the bare integer `1`.

**`res64`.** Write a 16-digit hex string, zero-padded, in uppercase. This was checked cheaply:
- The one public sample found of a GIMPS manual-submission record writes `"res64":"1CF45E9503C71FD6"`,
  in uppercase. It is a third-party program's README example surfaced by a search, not a
  specification, and its source was not opened.
- `issues/reference-big-integer.md` R5 says only "16 hexadecimal digits".
- `issues/prp-gerbicz-test.md` writes `0000000000000001`, which is case-neutral but shows the padding.

Uppercase makes a later diff against published residues byte-equal. Hold Res64 as `uint64_t` in
the result and format it in one place. A check pins both the padding (`0x1` gives
`0000000000000001`) and the case (`0xABCDEF0123456789`).

**Field names.**
- **`reason`**, for the no-verdict reason: plain, and no GIMPS name collides with it.
- **`record_version`**, for the format version, rather than `format`:
  - The cluster job's `result.json` already has `"format": 1` (`ci/test-commit.sh:57-62, 193`).
    That counter is independent, and R12 deliberately keeps it apart.
  - A reader or a `jq .format` sweep over both kinds of file would conflate the two counters.
  - `record_version` cannot be mistaken for `program.version`, which R10 forbids confusing with it.

**Expected records.** These use the spaced style recommended in § 2, and the reason wording is a
placeholder for brief 02 or the plan.

`mps -j 31` exits 42 and prints three lines:
```
{"exponent": 31, "worktype": "tf", "status": "no-verdict", "reason": "not implemented yet", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
{"exponent": 31, "worktype": "p1", "status": "no-verdict", "reason": "not implemented yet", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
{"exponent": 31, "worktype": "ll", "status": "no-verdict", "reason": "not implemented yet", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
```
`mps -j 33` exits 40. Stderr has no `Running`, and stdout is one line:
```
{"exponent": 33, "worktype": "ptest", "status": "composite", "factors": ["2^3-1"], "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
```
Constructed records (checks only):
```
{"exponent": 31, "worktype": "ll", "status": "prime", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
{"exponent": 31, "worktype": "??", "status": "probable-prime", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
{"exponent": 999999937, "worktype": "ll", "status": "composite", "res64": "0123456789ABCDEF", "program": {"name": "mps", "version": "0.1.0"}, "record_version": 1}
```

Three notes on these records:
- **Probable prime has no worktype.** No test can produce a probable prime, and the worktype set
  (`tf p1 ll ptest`) has no PRP name. The plan must say what a constructed probable prime names.
  The options are a test that can't really produce it (in checks only), or a PRP enumerator that has
  no `-t` name yet. This is brief 01's type question, flagged here.
- **Constructed residues must look synthetic.** Use a residue like `0123456789ABCDEF` and comment it
  as constructed. Never pair a real composite exponent (such as 11) with a plausible residue, which
  would read as a known answer (§ Invariants 3).
- **§ Invariants 1, bullet 4 is wider than R10.** It asks every result to name the residue, the
  transform length and the error-check counts. R10 fixes `res64` only for a composite by residue.
  The plan should note this as the stubs' scope (they have none of these), not silently drop it.

## 2. `program` and spacing

R10 writes `{"name": "mps", "version": "<APP_VERSION>"}`, spaced. Recommend the spaced style for the
whole record (`", "` between members and `": "` after keys). That is exactly Python's default
`json.dumps` output, verified byte-equal in scratch, and it gives three advantages:
- R10's `program` text appears verbatim, so a reviewer reading "exactly" byte-wise has nothing to
  argue.
- `tests/cli.sh` can compare whole lines byte-for-byte against readable expected text.
- A gate can check `line == json.dumps(expected)`, which pins key order, spacing and value types in
  one comparison.

GIMPS's own spacing is not part of the contract (Non-goals). Gates compare parsed values *and* the
canonical bytes; cli.sh can only compare bytes.

## 3. Proving "every such line SHALL parse as JSON" (R10) in `make check`

The recommendation is **(c), both**.

**(a) A strict test-only validator in C++.** Put it in the result module's check file (for example
`tests/check_result.cpp`, picked up by the `tests/*.cpp` wildcard). Apply it to the module's
`json_record(...)` output for a constructed result of every outcome and every payload, including a
reason holding `"`, `\`, `\n`, `\x01` and `\x1f`.
- **Scope.** It accepts objects, arrays, strings with the escapes `\" \\ \/ \b \f \n \r \t \uXXXX`,
  integers, and `true`/`false`/`null`.
- **Rejections.** It rejects duplicate keys, trailing commas, leading zeros, fractions, raw control
  characters in strings, unknown escapes, `NaN`, trailing text and a non-object top level.
- **Size.** The prototype is about 85 dense lines and passed 15 cases. In house style (braces, one
  statement per line, doc blocks) expect about 130–150 lines.
- **Check the validator itself.** It needs its own check that it *rejects* each bad case. Otherwise a
  validator that always returns true passes vacuously, the same check-strength gap as cycle 3's F1
  and F2.
- **ASCII.** Also check that each record is printable ASCII (bytes 0x20–0x7e). That makes "parses"
  independent of UTF-8 handling.

**(b) Byte comparison in `tests/cli.sh`.** Compare whole stdout lines of `bin/mps -j` against
hand-written expected text. That text is valid by inspection, and the gate parses it with Python.
This is the only proof that reaches the real binary, so the formatter and the driver can't diverge.

**§ 5 allows (a).** `AGENTS.md:173` says: "Test oracles (an independent big-integer library, a
script) may use outside tools, live only with the tests, and never link into `bin/mps`." A validator
in `tests/` uses no outside tool and links only into `bin/check`, so it is plainly inside that
wording. The escaper lives in the module (it is product code). The validator never does.

## 4. Driving the statuses and the output

**`tests/cli.sh`** covers what the binary can produce:
- 42: any prime PVAL, and `-s` on a composite.
- 40: a composite PVAL.
- Stdout of both kinds: `run_all` keeps the whole of stdout, so use it for multi-line output.
- Stderr: `run_stderr`.

**C++ checks** cover statuses 0, 41, 30, error, and the never-50–59 rule (R8, R9: "constructed
directly"). `main.cpp` is not linked into `bin/check` (`Makefile:15-17`, `MODULES := ansi cli io mp
math`), so the status and printing logic must live in a module that is added to `MODULES`. From the
checks' side, recommend this split:

```cpp
std::string verdict_line(const Result& result);              // pure: one line, no '\n'
std::string json_record(const Result& result);                // pure: one object, no '\n'
int exit_status(const std::vector<Result>& results);          // pure: 0, 40, 41, 42, or an error class
int report(const std::vector<Result>& results, bool json);    // prints to std::cout / logs to std::cerr; returns exit_status
```

**Why `report` prints after every result is in.** It decides the status first and prints only
afterwards. A conflict (prime and composite) or an error then prints no line, as R9 requires.

**Capturing output in a check.** Swap `std::cout.rdbuf` and `std::cerr.rdbuf`, as
`tests/check_math.cpp:98-107` and `tests/check_log.cpp:68-74` do. The logger writes to `std::cerr`
(`src/log.hpp:150,166`). Restore both buffers before any `expect`.

**Matching stderr in a check.** Match a substring, never the whole line, for two reasons:
- `result.cpp` gets its own logger copy (`issues/logger-state-per-translation-unit.md`).
- `bin/check` never calls `init_tty`, so `bold()` in the logger's name emits escapes there.

**R8 checks.** One check per outcome, comparing against the `Status` enumerators rather than bare
numbers. Add one check that enumerates combinations and asserts no status falls in 50–59.

## 5. `tests/cli.sh`: `planned`, one condition per expect, and R13

**The rules.** `planned=126` must be recounted exactly. TECH (cycle 3) says: "Every check is one
condition … `expect NAME [ a ] && [ b ]` counts only `[ a ]`; write two `expect` lines."

**Expectations that change** (line numbers on `25d13b5`):

| Line | Today | After |
|---|---|---|
| 177 | `-t $name 31 exits 0` (7 names) | 42 |
| 182 | `31 exits 0` | 42 |
| 187 | `--file out.mps -t ll 31 exits 0` | 42 |
| 216 | `-- 31 exits 0` | 42 |
| 233-241 | loop `0000000031` and `999999999`: "exits 0", "runs the tests" | Split the loop. `0000000031` exits 42 and runs the tests. `999999999` exits 40 and runs **no** test (R3), so "runs the tests" inverts to "runs no test" |

**Expectations that stay at 0** are all `--version`/`-v`/`--help`/`-p`: lines 118, 123, 151, 160
and 229.

**New byte checks.** Build the `program` text once:
`prog='"program": {"name": "mps", "version": "'"$version"'"}'`. Then:
- Compare `run_all -j 31` with the exact three lines.
- Compare `-j 33` with the exact line. Also check `-j 4`, `-j 999999999` and `-j 999002449` under
  every `-t` name (R3).
- `-j 0000000031` gives `"exponent": 31`.
- `-sj 33` gives three records with `"exponent": 33` and exits 42.
- `-jd` groups.
- Stderr is unchanged by `-j`, and the status is unchanged by `-j`.

**Showing R13 robustly.** A green `make check` with the exact `planned` count already guarantees that
no *executed* expectation of 0 survives for a non-`-p` PVAL, because `mps` no longer exits 0 there.
What text can still hide is a misleading name or dead code. The gate should add:
1. **Name sweep.** Every `exits 0` in `tests/cli.sh` is in an allow-list of `-p`/help/version
   names. `grep -n 'exits 0' tests/cli.sh`, then fail on any hit whose name doesn't start with
   `$option`, `--help`, `-p ` or `0 -p`.
2. **Pairing sweep.** An awk pass tracks the arguments of the last `run`/`run_all`/`run_stderr` call
   and fails if a `"$status" -eq 0` follows one that has a PVAL but no `-p`/`-h`/`-v`/`--help`/`--version`.
3. **C++ sweep.** In `tests/*.cpp`, every comparison against `OK` (or `0`) for an `exit_status` or
   `report` result is the constructed-prime check (grep, then a short reviewed allow-list).
4. **Mutants.** `exit_status` returns 0 for no verdict, and `report` returns 0 for composite. Each
   must turn `make check` red, through cli.sh *and* through bin/check.

## 6. Gate side (Mac, `python3 -I`)

A helper modelled on cycle 3's `fields.py`, written by the gate into `$work`. It was tested in
scratch: it passes the expected `-j 31` output, and it fails on `"record_version": true`, which plain
`==` would accept because `True == 1` in Python.

```python
import json, sys

def pairs(kv):
    keys = [k for k, _ in kv]
    if len(set(keys)) != len(keys):
        raise ValueError("duplicate key in %r" % keys)
    return dict(kv)

def constant(name):
    raise ValueError("not JSON: " + name)

want = json.loads(sys.argv[1])              # expected records, a JSON array, in order
text = open(sys.argv[2], "rb").read()
if not text.endswith(b"\n") and want:
    sys.exit("stdout does not end in a newline")
lines = text.decode("ascii").split("\n")[:-1]   # non-ASCII raises
if len(lines) != len(want):
    sys.exit("%d lines, want %d" % (len(lines), len(want)))
for i, (line, w) in enumerate(zip(lines, want)):
    json.loads(line, object_pairs_hook=pairs, parse_constant=constant)
    if line != json.dumps(w):               # order, spacing and types (1, true, 1.0) at once
        sys.exit("line %d: %s\n   want: %s" % (i + 1, line, json.dumps(w)))
```

Call it as `python3 -I "$work/records.py" "$want_json" "$logs/NAME.out"` after `run`, with `$want`
built by substituting the version read from `src/main.hpp`. Python's parser already rejects leading
zeros, trailing commas and raw control characters (verified). The hooks add duplicate keys and
`NaN`/`Infinity`. `/usr/bin/python3` is Command Line Tools 3.9, which has everything used here.

## 7. Colour

`format_ansi` (`src/ansi.cpp:58-75`) gates only on `STDERR_COLOR`. `STDOUT_COLOR` is computed but
never read (the file's own TODO).
- **The pipe case.** If a verdict line used `bold()`/`green()`, then `mps 31 | cat` with stderr on a
  tty would put escapes on stdout. `FORCE_COLOR` would put them in JSON.
- **In `bin/check`.** `init_tty` never runs there (it is called only from `logging::init`), so any
  `ansi` call colours, and byte checks of `verdict_line` would see escapes.

**Recommendation:** never colour stdout records. The result module doesn't include `ansi.hpp`. Add
one cli.sh check: `out=$(FORCE_COLOR=1 "$mps" 31 2>/dev/null)` holds no ESC. Make `esc=$(printf
'\033')` (`printf` is a built-in in bash and dash), then `lacks "$out" "$esc"`. Do the same under
`-j`. Colouring the help on stdout is existing behaviour and out of scope.

## Recommendations

1. **Field names.** `reason` for the no-verdict reason, and `record_version: 1` for the format
   version (not `format`, which is the cluster `result.json`'s independent counter).
2. **Key order.** `exponent, worktype, status, factors|res64|reason, program, record_version`.
3. **Spacing.** The spaced `json.dumps` default style (`", "`, `": "`), so R10's `program` text
   appears verbatim.
4. **Values.** `exponent` from the parsed integer; `res64` as 16 uppercase hex digits, zero-padded,
   from a `uint64_t`.
5. **Escaper.** A correct `json_string` escaper (quote, backslash, `< 0x20` as `\u00XX`) in the
   result module, and a check that a hostile reason still yields one valid line.
6. **Module split.** Pure `verdict_line`, `json_record` and `exit_status`, plus `report(results,
   json)` that decides before it prints. Put them in a module added to `MODULES` so `bin/check`
   links them. Checks capture output with an `rdbuf` swap and match stderr by substring.
7. **R10 proof in `make check`.** Both a strict test-only validator in the result's check file, with
   its own rejection check, and byte-exact expected lines in `tests/cli.sh`. Both are within § 5's
   "live only with the tests".
8. **`tests/cli.sh`.** Update lines 177, 182, 187 and 216 to 42, and split the 233-241 loop
   (`999999999` becomes 40 and runs no test). Recount `planned`, with one condition per `expect`.
9. **R13 gate.** A name sweep plus an awk run-to-status pairing sweep over cli.sh, a C++ `OK`
   allow-list, and two status mutants that must turn `make check` red.
10. **Gate helper.** The `records.py` above: strict parse, then byte equality with
    `json.dumps(expected)`.
11. **Colour.** Never colour stdout records, with a `FORCE_COLOR=1` no-ESC check.
12. **Open for brief 01 or the plan.** The worktype of a constructed probable prime, the exit class
    for an error outcome, and the gap between § Invariants 1 bullet 4 and R10.
