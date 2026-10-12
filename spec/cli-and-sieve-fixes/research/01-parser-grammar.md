# Research 01 — Option-parser grammar

Observed on a scratch build of `1516ae4` (clang++, `NO_COLOR=1 LC_ALL=C`), then re-run against a
scratch prototype of the structure recommended below (`make check` green, no new warnings).

## 1. Today's behaviour

| Invocation (`… 31` unless noted) | Exit | Result / stderr |
|---|---|---|
| `-t ll`, `-tll`, `--test ll`, `--test=ll` | 0 | LL only |
| `-t=ll` | 1 | `Unrecognized option value '=ll' for -t/--test` |
| `-t tf`, `-t p1` | 1 | `Unrecognized option value 'tf'` / `'p1'` |
| `-t none` | 0 | all three tests (dispatch `default:`) |
| `-f a.mps`, `-fb.mps`, `--file=e.mps` | 0 | all three; file created in cwd |
| `-f=c.mps` | 0 | path `=c.mps`; **file `=c.mps` created in cwd** |
| `--file d.mps` | 1 | `Unrecognized option value 'd.mps' for -t/--test` (calls `set_test_case`) |
| `--file ll` | 0 | **LL only, no file** — `--file` silently acts as `--test` |
| `--file out.mps -t ll` | 1 | `Unrecognized option value 'out.mps'` |
| `-dD`, `-pd`, `-sdD`, `-xh`, `-dx` | 1 | `Unrecognized option: -dD` (whole word) |
| `-hv`, `-hx`, `-hfoo` | 0 | help (prefix match on `-h`) |
| `-vh` | 0 | version |
| `--` (alone) | **30** | `Uncaught exception: stoi: no conversion` |
| `-- 31`, `-- -1 31` | 1 | `Unexpected positional argument: 31` — **`--` itself is pushed as PVAL** (`cli.cpp:53-56`) |
| `-`, `- 31` | 1 | `Unrecognized option: -` |
| `-t ''`, `--test=` | 1 | `Unrecognized option value '' for -t/--test` |
| `-t=` | 1 | `… value '=' …` |
| `-t` (last) / `--test` (last) | 1 | `Missing option value for -t/--test` / `… for --test` (inconsistent) |
| `-f ''`, `--file=` | 1 | `Could not open file: ` |
| `-f /nonexistent/x` | 1 | `Could not open file: /nonexistent/x` |
| `-t ll -t tf` | 1 | ` warning … Duplicate use of -t/--test`, then the `tf` error |
| `-d -d` | 0 | duplicate warning, runs |
| `31 61` | 1 | `Unexpected positional argument: 61` |
| `--help=x`, `--hel` | 1 | `Unrecognized option: …` |
| (no args) | 0 | usage on stdout |

The file-creation side effect comes from `Interface::set_file_path` (`cli.cpp:235-244`), which opens
the path with `std::ofstream::app` during parsing. The default `pval + ".mps"` is assigned without
calling it, so no file appears when `-f` is absent.

## 2. Recommended parser structure

Keep `cli::Interface`, `pop_opt`, `check_option_long`, `check_option_short`, `set_test_case`,
`set_file_path`; replace the prefix chains with a per-character walk sharing one code path.

- **Main loop:** `positional_only` → positional; `opt == "--"` → set `positional_only` and *do not*
  push it (fixes the `--` bug); `--…` → long; `-` followed by ≥1 char → short; anything else,
  including a lone `-`, → positional (then PVAL validation rejects it, naming PVAL).
- **Long:** split at the *first* `=`. Map the name to its short char via a lookup table
  (`static std::map<std::string, char> option_by_name {{"--test", 't'}, …}`, repo table style). A
  valued option takes the text after `=` (possibly empty) or, with no `=`, the next word. A boolean
  option with `=` → argument error (`Option --debug takes no value`); unknown → `Unrecognized
  option: --x`.
- **Short word:** walk chars from index 1. Valued char (`t`, `f`): the rest of the word is the value
  with one leading `=` stripped; if the rest is empty, take the next word; stop. Boolean char:
  `h`/`v` throw `show_info` at once (ending the run, so `-hx` is help); `p s d D` set their flag;
  anything else → `Unrecognized option: -x` naming the char.
- **Valued option inside a group** (`-dtll`, `-dt ll`): the walk gives getopt behaviour for free
  — preceding flags apply, `t` takes the rest of the word or the next word. Forbidding it would
  cost code. Recommend: allow it, and leave it unpinned by `make check` (GOAL lists it as neither
  required nor forbidden). `-tdll` means `-t dll` → error naming `dll`.
- **Pitfall:** `pop_opt` overwrites the member `opt`. Copy the word before walking it, or the
  value pop clobbers it.
- **Missing value** (`-t` or `-dt` last, `--file` last): one message per option,
  `Missing value for -t/--test` / `-f/--file`, whatever the spelling. A next word starting with `-`
  is still taken as the value (`-f -d` → path `-d`), as today.
- **Empty value:** `-t ''`, `-t=`, `--test=` → `set_test_case("")` → R3's error naming `''`. Strip
  the `=` *before* the emptiness test, so `-t= 31` names `''` rather than consuming `31`.
  `-f ''`, `-f=`, `--file=` → `Empty value for -f/--file` (once parsing stops touching the
  filesystem, nothing else catches it). Consequence: a path starting with `=` needs a separate
  word (`-f =x`). `--file=a=b` gives `a=b`.
- **Duplicate-use warnings:** keep them (they're on stderr and don't change the exit status). One
  helper per flag keeps `-dd` and `-d -d` identical, as R5 requires. Unit checks should avoid
  duplicates so stderr stays quiet.

The prototype's `check_flag` is a `switch` on the char. It ran every row of §1 to the expected
outcome and created no files.

## 3. `-t` names and dispatch

```cpp
static std::map<std::string, TestCase>
test_by_name {
	{"all",          TEST_ALL},
	{"tf",           TEST_FACTOR},
	{"trial-factor", TEST_FACTOR},
	{"p1",           TEST_P_MINUS_ONE},
	{"p-minus-1",    TEST_P_MINUS_ONE},
	{"ll",           TEST_LUCAS_LEHMER},
	{"lucas-lehmer", TEST_LUCAS_LEHMER},
};
```

Delete `TEST_NONE`. Keeping the enumerator names avoids churn. In `src/main.cpp`, replace
`default: test_all(pval);` with `case cli::TEST_ALL: test_all(pval); break;`. With no `default`,
`-Wall`'s `-Wswitch` flags any enumerator the switch misses. Clean under clang++ in the prototype.

R2 start lines, exact under `NO_COLOR=1` (level right-aligned to 8 columns, from
`src/math.cpp:55,62,69`):
`    info [mps] Running quick factor test`, `    info [mps] Running P-1 test`,
`    info [mps] Running Lucas-Lehmer test`. `all` and no `-t` give all three, in that order.
Errors read `critical [mps] …` and warnings ` warning [mps] …`.

## 4. `--file` side effect

Recommend **parsing stops touching the filesystem**: `set_file_path` only records the path (and
rejects empty). This matches the non-goal "Checkpoints: `-f` sets the path and nothing reads or
writes the file yet". It also keeps `make check` (run from the repo root) from littering
`out.mps` or `=out.mps`, and lets R4 be checked purely in memory.

Observable change: `-f /nonexistent/x 31` goes from exit 1 `Could not open file: …` to running.
`<fstream>` drops out of `cli.cpp`. Nothing relies on the old behaviour: no test, issue or doc
mentions "Could not open". The checkpoint cycle can add a writability check when it writes. Keep
the default `file_path = pval + ".mps"` (non-goal). Compute it after PVAL validation so a bad PVAL
errors first.

## 5. Help text draft (`src/main.hpp`, max 75 columns)

```
Options:
  -p, --primes            Print primes below PVAL.
  -s, --skip-ptest        Suppress initial prime test on PVAL.
  -t, --test        NAME  One of 'tf' ('trial-factor'), 'p1' ('p-minus-1'),
                          'll' ('lucas-lehmer') or 'all' (default: 'all').
  -f, --file        PATH  File path for checkpointing.
  …(unchanged)…
  -h, --help              Show this message and exit.

  Give a valued option as -x VALUE, -xVALUE, -x=VALUE, --long VALUE or
  --long=VALUE. Short flags may be grouped (-dD).
```

The usage line, `[-hv]`, `-s` and `-f` are unchanged. Cosmetic: `colorize_usage`'s `option_re`
(`ansi.cpp:138`) matches `-minus`, `-factor` and `-lehmer` inside the aliases. With colour on,
the quoted alias goes cyan mid-word and loses its yellow after that. `VALUE` isn't in
`metavar_re`. `NO_COLOR` output, which is what the checks see, is unaffected.

## 6. Making `make check` non-hollow

- Add a `describe(const cli::Interface&)` helper that renders *every* parsed field (`pval`,
  `test_case`, `opt_test_case`, `file_path`, `opt_file_path`, `show_primes`, `skip_ptest`,
  `debug_mode`, `detailed_mode`) into one string. `expect_equal` on two such strings compares all
  fields and prints both on failure, so no `operator<<` is needed.
- **Spellings (R4):** loop over all seven names × five spellings, each paired with
  `-f` × five spellings, against one baseline. Also assert the baseline directly
  (`TEST_LUCAS_LEHMER` ≠ default `TEST_ALL`; path ≠ `31.mps`), so equality isn't vacuous.
  - Use a path whose directory doesn't exist (`no-such-dir/x=y.mps`). It parses without
    touching disk, exposes the `=` bug (old `-f=` yields `=no-such-dir/…`), and tests the
    first-`=` split.
- **Groups (R5):** for `-dD`, `-pd`, `-sdD`, `-Dd`, compare `describe` of the grouped form with
  the separated form, and assert the expected flags are true.
  - `-hv`/`-hx`: `show_info` whose `what()` equals `APP_HELP`. `-vh`: the version string.
  - `-xh`, `-dx`: `argument_error` whose `what()` contains `-x`.
- **Errors (R3):** for each rejected value (`qf quick factor pm p-min none` and `""`) × each
  spelling, assert `argument_error` and that `what()` contains `'VALUE'`. Add a small local
  `expect_argument_error(argv, needle)` helper, since `check.hpp` has none.
- **Shell (R2, R4):** in `tests/cli.sh`, compare *whole* stderr for each name/alias against the
  exact line(s), so a single test can't pass while all three run. Run
  `--file PATH -t ll 31` the same way.

## Recommendations

1. Rewrite the short-option path as a per-character walk: valued `t`/`f` take the rest of the word
   (one leading `=` stripped) or the next word; `h`/`v` end the run; `p s d D` set flags; anything
   else is `Unrecognized option: -<c>`.
2. Parse long options by splitting at the first `=` and mapping the name through
   `option_by_name` to the same short-char handlers. `=` on a boolean option is an argument error.
3. Make `--` end options without becoming PVAL; treat a lone `-` as a positional.
4. Allow valued options inside a group (falls out of 1); don't pin it in `make check`.
5. Unify missing-value messages as `Missing value for -t/--test` / `-f/--file`. Empty `-t` goes
   to R3's error naming `''`. Empty `-f` → `Empty value for -f/--file`.
6. Keep the duplicate-use warnings.
7. Replace `test_by_name` with the seven R1 names, delete `TEST_NONE`, and make the `main.cpp`
   switch exhaustive (`case cli::TEST_ALL`, no `default`).
8. Make `set_file_path` record only; drop the `std::ofstream` probe and `<fstream>`. Keep the
   default `pval + ".mps"`, computed after PVAL validation.
9. Adopt §5's help text.
10. Check via a whole-`Interface` `describe` string, cross-product spelling loops, a
   `no-such-dir/x=y.mps` path, exact `what()` checks, and whole-stderr comparisons in `cli.sh`.

## Open questions

- Lone `-` as positional (→ PVAL error) or as `Unrecognized option: -`? Both exit 1. Positional
  is the POSIX reading; GOAL is silent.
- Fix `colorize_usage`'s mid-word hyphen match now (cosmetic, touches `ansi.cpp`), or leave it?
