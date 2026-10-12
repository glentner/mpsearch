# 01 — Result type and driver

Base `25d13b5`. Experiments ran on a copy in scratch (Apple clang 21), never in the worktree.

## Where the code lives

- `bin/check` links `$(CHECKS) $(CHECKED)`, and `CHECKED` comes from `MODULES := ansi cli io mp math`
  (`Makefile:17-19,27-28`). `lib/main.o` is linked only into `bin/mps` (`Makefile:23`). So the
  type, the line and JSON formatting, the status mapping and the R9 precedence go in a module.
- **New module `src/result.hpp`/`src/result.cpp`** (one `.hpp`/`.cpp` pair, `AGENTS.md:199`). It
  holds the type, `verdict_line`, `json_record`, the status mapping and the printing `Reporter`
  (see below). Add `result` to `MODULES`. One edit covers both binaries.
- **Checks:** `tests/check_result.cpp` is picked up with no Makefile edit. `CHECKS` is
  `$(wildcard tests/*.cpp)` (`Makefile:20`), and `tests` is a prerequisite of `bin/check`, so a
  new file relinks it (`Makefile:26-27`). `lib/check/check_result.o` misses the `src/%.cpp` rule
  (no `src/check_result.cpp`) and falls through to the tests rule (`Makefile:32-36`). No name
  clash with `lib/check/result.o`.
- **Tests and the exponent check stay in `src/math.*`.** The stubs' start lines stay in
  `math.cpp:64,71,78`, which keeps R2's stderr text byte-identical (see Logging). The exponent
  check is arithmetic, so it goes in `math.cpp` as well: something like
  `std::optional<result::Result> check_exponent(p)`, empty for a prime p. A prime exponent
  "prints nothing" (R4) because no result exists, not because one is filtered out.
- **The dispatch stays in `main.cpp`** (`main.cpp:40-57`, coupled core `AGENTS.md:67`). It is
  thin: gate on `opt.skip_ptest`, run the check, switch on `test_case`, feed each result to the
  `Reporter`, and return its status. `tests/cli.sh` covers it. Everything with logic is in a
  module that `bin/check` can call.
- **R12:** the same commit adds `src/result.hpp`, `src/result.cpp` and `src/main.cpp` to the
  high-blast-radius list (`AGENTS.md:134-136`). `main.cpp` belongs there because it is the dispatch
  that drives verdict printing. That commit also rewords `AGENTS.md:138` and `:179`, and adds a
  § Map line for `result.*` (`AGENTS.md:29-39`).

## The type

```cpp
namespace result {
	enum Outcome  { PRIME, PROBABLE_PRIME, COMPOSITE, NO_VERDICT, ERROR };
	enum Worktype { TRIAL_FACTOR, P_MINUS_ONE, LUCAS_LEHMER, EXPONENT_CHECK };  // tf p1 ll ptest
	class Result {
	public:
		static Result prime(Worktype, long unsigned p);                      // res64 = 0
		static Result probable_prime(Worktype, long unsigned p, std::uint64_t res64);
		static Result composite_by_factor(Worktype, long unsigned p, const std::string& factor);
		static Result composite_by_residue(Worktype, long unsigned p, std::uint64_t res64);
		static Result no_verdict(Worktype, long unsigned p, const std::string& reason);
		static Result error(Worktype, long unsigned p, const std::string& reason);
		// const accessors: outcome(), worktype(), exponent(), factor(), res64(), reason()
	private:
		Result(...);   // only the named constructors can build one
		Outcome outcome_; Worktype worktype_; long unsigned exponent_;
		std::optional<std::string> factor_; std::optional<std::uint64_t> res64_;
		std::optional<std::string> reason_;
	};
}
```

- **Exactly one outcome.** The constructor is private and there are six named constructors, so a
  composite can't exist without a factor or a residue, and a no verdict can't exist without a
  reason. Make the members private, not public `const`. Public `const` members would delete
  copy-assignment, which `std::optional` and vector reassignment use.
- **By factor or by residue** is the presence of `factor_` or `res64_`. JSON writes `factors` iff
  `factor()` is set (R10). One string is enough today. JSON wraps it as `[factor]`. A
  `std::vector<std::string>` costs little if P-1 may report several factors later.
- **Namespace it.** `Outcome` must not share the global scope with `enum Status`. `COMPOSITE`,
  `PROBABLE_PRIME` and `NO_VERDICT` would be redeclared there, and that fails to compile. With
  `namespace result` the two coexist: verified, `result::COMPOSITE` beside `::COMPOSITE`. A local
  variable named `result` does not hide the namespace in `result::X`, because qualified lookup
  ignores variables (verified). `result::ERROR` and `logging::ERROR` (`log.hpp:51`) don't clash.
  Precedent: `cli::TestCase` (`cli.hpp:39-44`) and `logging::Level` (`log.hpp:47-53`).
- **Plain `enum`, not `enum class`.** This is house style, and `check::expect_equal` streams both
  sides with `<<` (`tests/check.hpp:89`). An unscoped enum streams as its integer, while an
  `enum class` would need an `operator<<`. To compare whole `Result`s it needs `==` and `<<`.
  Comparing strings (line, record) and `Status` avoids both.
- **Not `cli::TestCase`.** It has `TEST_ALL` and lacks the exponent check, and `math` must not
  depend on `cli`. Keep `name_by_worktype` (`tf`, `p1`, `ll`, `ptest`) as a `<value>_by_<key>`
  table (`AGENTS.md:194`).
- **The R3 re-check.** Put `p % a == 0` in the exponent check, next to building the string `2^a-1`
  (format: the planner's call). A dedicated `Result::composite_by_exponent(p, a)` that refuses
  when `a` doesn't divide `p` would make the re-check impossible to skip. That is an option, not a
  must.

## Status and precedence

- `enum Status` (`main.hpp:18-25`) gains `COMPOSITE = 40`, `PROBABLE_PRIME = 41` and
  `NO_VERDICT = 42`, aligned with the existing comments. Prime is `OK` (0).
- **The ERROR outcome maps to `ERR_RUN` (20).** The table calls it "Generic runtime error"
  (`main.hpp:23`), and `main` already maps `std::runtime_error` to it (`main.cpp:67-69`). An
  error result is a run whose checks failed, which is a runtime failure, not an argument, config
  or internal bug. Prime with composite maps to `ERR_UNKNOWN` (30), as the GOAL fixes.
- **Use a `switch`, not a map, for outcome→status.** `-Wswitch` (in `-Wall`) flags a missing
  enumerator (verified). A `std::map` with `operator[]` returns a default for a missing key, and
  that default is `0`, which is **prime** (verified: the missing key gave 0). That would be an
  unearned verdict (§ 1). Close the switch with a trailing `return ERR_UNKNOWN;` for
  `-Wreturn-type`.
- **Precedence, as a pure function** `Status status_of(const std::vector<Result>&)`:
  1. any ERROR → `ERR_RUN`;
  2. PRIME and COMPOSITE together → `ERR_UNKNOWN`. Planner's call: count PROBABLE_PRIME with
     COMPOSITE as the same contradiction. Nothing in the GOAL forbids it;
  3. else the decisive result's status;
  4. else `NO_VERDICT`. An empty set can't occur, because a run always produces ≥ 1 result. The
     planner picks 42 (the literal reading) or 30.

## Streaming or collect: the `Reporter`

- **Can two decisive results co-occur today? No.** A composite exponent runs no test (R3), `-s`
  skips the check (R5), and `-t all` stops at the first decisive result (R7). So the R9
  contradiction is reachable only with constructed results, which is exactly what R9 asks
  `make check` to show.
- Collecting and then printing would hold back `tf`'s line until `ll` ends, which takes days for a
  real LL, and it loses printed results on a crash. That conflicts with R6's "WHEN a test
  completes".
- **Recommend a streaming sink that validates before it prints:**

```cpp
class Reporter {
public:
	Reporter(std::ostream& out, bool json);
	void add(const Result& result);  // check against the results so far, then print one line
	Status status() const;           // status_of(results)
};
class result_error: public mps_error { public: Status status; ... };
```

- `add` appends the result to its vector and runs `status_of` on it. If that gives `ERR_RUN` or
  `ERR_UNKNOWN`, `add` **throws `result_error` before writing anything for that result**, so R9
  holds: no line and no record for it. Otherwise it writes `verdict_line` or `json_record` and a
  newline to `out`.
- `main` catches `result_error` ahead of `std::exception`. `mps_error` is not a
  `std::runtime_error` (`exc.hpp:15`), so without its own catch it would fall into the "Uncaught
  exception" branch with 30 (`main.cpp:71-73`). The catch logs `logging::critical(error.what())`
  and returns `error.status`. This mirrors the `cli::argument_error` path (`main.cpp:63-65`).
- Checks build a `Reporter` over an `std::ostringstream` with constructed results:
  - every outcome → status (R8);
  - an ERROR → throws with 20, and `out` is empty;
  - COMPOSITE then PRIME → throws with 30, and `out` holds only the first line;
  - JSON on and off.

## `test_all` under R1 and R7

R1 requires `test_all` to return `Result`. R7 requires one line per completed test as each one
completes. Only a sink meets both literally:

```cpp
result::Result test_all(const long unsigned p,
                        const std::function<void(const result::Result&)>& on_result);
```

`test_all` runs tf, p1 and ll in order. It calls `on_result` after each, stops at the first
decisive result, and returns that result (or the last no verdict). `main` passes
`[&](const result::Result& r) { reporter.add(r); }`. For a single `-t` it calls
`reporter.add(test_x(p))`. The stop rule lives only in `math.cpp`, not duplicated in `main`.

The rejected options:

- `main` running the three tests itself would leave `test_all` dead or duplicated.
- Returning a vector breaks R1.

`math.hpp` then needs `<functional>` and `result.hpp`, nothing from `cli`. The stubs use `p` now,
so drop `[[maybe_unused]]` (`math.cpp:62,69,76`). Leaving it is harmless.

## Logging

- **Stub start lines are unchanged** if the `logging::info` calls stay in `math.cpp`. Any non-main
  TU logs in the default style at the zero-initialised level (`log.hpp:93-94`,
  `issues/logger-state-per-translation-unit.md:19-21`), which is what `tests/cli.sh:165-167` pins.
- **Never call `logging::debug` from `math.cpp` or `result.cpp`.** In a non-main TU the level is
  `DEBUG`, so debug output prints with no `-d` (verified: a debug line appeared). Any extra stderr
  line also breaks the exact `"$err" = "$start_…"` comparisons (`tests/cli.sh:178,183,188`). The
  exponent check must log nothing for a prime p.
- **A `logging::critical` from a new module** prints `critical [mps] …` in the default style and
  ignores `-D`. It is acceptable under the non-goal, but there is a sharper hazard:
  - The logger's templates are `inline` and read TU-`static` state. When `result.cpp` and
    `main.cpp` instantiate the same template, such as `critical<const char*>` (main already does
    at `main.cpp:64`), the linker keeps one copy.
  - Verified at `-O0`: with `result.o` linked before `main.o`, **main's own** critical line lost
    its `-D` stamp. At `-O3` both calls were inlined, so each TU read its own state.
  - That is undefined (an ODR violation) and depends on link order.
  - Logging only from `main.cpp`, via `result_error`, sidesteps it.

## Other pitfalls

- **Header helpers must be `inline`.** A `static` function in `result.hpp` that some check file
  doesn't call fails `-Werror=unused-function` (`Makefile:36`; verified). `math.hpp` will pull
  `result.hpp` into `check_math.cpp`, `check_cli.cpp` and others.
- **Header-level `static` tables** (`name_by_worktype`), like `test_by_name` (`cli.hpp:51`), give
  one copy per TU. That is harmless: the content is identical and no warning fires for unused
  class-type statics.
- `static const std::string APP_VERSION` (`main.hpp:15`) gives per-TU copies with identical
  values, which is fine. `result.cpp` includes `main.hpp` for `APP_NAME`, `APP_VERSION` and
  `Status`, as `io.cpp:16` and `cli.cpp:16` already do.
- Inside `namespace result`, unqualified `COMPOSITE` is the outcome. Write the status as
  `::COMPOSITE`, or keep the status mapping outside the namespace body, for readability.
- R10 needs `res64` as a fixed-width hex string and the reason JSON-escaped. Escape `"` and `\` in
  `json_record`, even though the reasons are ours.

## Recommendations

1. Add `src/result.{hpp,cpp}`: `namespace result`, plain `enum Outcome` and `enum Worktype`, and a
   `Result` with a private constructor, six named constructors and `std::optional`
   factor/res64/reason.
2. Put the pure `verdict_line`, `json_record` and `status_of(vector)` in `result.cpp`. Map outcome
   to status with a `switch` (never a map, whose default is 0, prime). ERROR → `ERR_RUN` (20);
   prime with composite → `ERR_UNKNOWN` (30).
3. Add a streaming `Reporter(out, json)`. `add()` validates against the results so far and throws
   `result_error{status}` before printing. `main` catches it, logs `critical` and returns its
   status.
4. `test_all(p, on_result)` returns the decisive (or last) `Result` and streams each one through
   the sink. The tests and `check_exponent(p) -> std::optional<Result>` stay in `math.cpp`; the
   thin dispatch stays in `main.cpp`.
5. Add `result` to `MODULES`. `tests/check_result.cpp` is automatic.
6. Add `Status` values `COMPOSITE` 40, `PROBABLE_PRIME` 41 and `NO_VERDICT` 42.
7. In the format commit, add `src/result.hpp`, `src/result.cpp` and `src/main.cpp` to the
   high-blast-radius list and a § Map line.
8. No `logging::debug` and no extra stderr in new code. Keep the start lines in `math.cpp`. Header
   helpers are `inline`.
