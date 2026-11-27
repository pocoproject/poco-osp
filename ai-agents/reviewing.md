# Reviewing

How to review a pull request or a local change. Tool commands are in [analysis.md](analysis.md); the style rules are in
POCO's [CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md).

## Inputs

```bash
gh pr view <N> -R pocoproject/poco-osp
```

```bash
gh pr diff <N> -R pocoproject/poco-osp
```

- Read the linked issue, the whole diff and the code around each hunk.
- Fetch the branch to build and test it locally (see [git-and-pull-requests.md](git-and-pull-requests.md)).

## Tools first

- Use the review skills or sub-agents of the agent where they exist: general code review; security review for changes to
  input parsing, network, authentication, cryptography, file-system and credential code; simplification and
  code-quality review; error-handling and silent-failure review; test-coverage review.
- Run the checks of [analysis.md](analysis.md) on the changed files: compiler warnings, clang-tidy, cppcheck, and the
  touched test suites under ASan, UBSan and TSan.
- A missing tool is reported, not skipped silently.
- Tool output is input to the review, not the verdict. Verify each finding against the code.

## Dimensions

1. **Correctness and regressions:** behavior changes for existing callers and for bundles of other projects, object
   lifetime and ownership (a bundle library that is unloaded while its objects are in use), data races and lock order,
   deadlocks and hangs at bundle stop and at shutdown, error and timeout paths, cleanup after a partial bundle start,
   integer overflow and size types, platform differences (Windows, macOS, Linux).
2. **Design and pattern fit:** the pattern matches the problem; services, extension points and events are used as the
   rest of the framework uses them; no second way to do one thing where one would do.
3. **Simplification and deduplication:** reuse of the standard library, POCO classes and OSP helpers. Duplicated logic
   is extracted into a helper as described in [writing-code.md](writing-code.md#before-writing).
4. **Dead code and redundant tests:** vestigial parameters and members, unreachable branches, an API without a
   production caller, tests that assert nothing.
5. **Performance:** regressions on hot paths (request dispatch, service lookups, copies, allocations in loops, locking).
6. **Tests:** a test exists for each behavior change and fails without it (see [testing.md](testing.md)). Missing tests
   are named: error, abort and timeout paths, bundle start and stop, and concurrent use.
7. **Clarity, comments and naming:** names, function length, simple control flow. Comments explain why, not what, are
   still accurate after a refactoring, and carry no issue numbers.
8. **Language idioms:** `[[nodiscard]]` and the other forms that
   [Rule 94](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md#rule-94) and the rules around it require in
   new and changed code. POCO OSP requires C++20, so C++20 forms such as `contains()` for a membership test are
   used without the feature guards of the section [Language Standard and Feature
   Guards](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md#20-language-standard-and-feature-guards).
9. **Style:** conformance to CODING_STYLE.md. Its rules are must-fix in new and changed code, also where existing code
   does not follow them.
10. **API and ABI:** changes to public headers are deliberate and documented; a breaking change is listed under
    "Breaking changes" in `CHANGELOG.md`.
11. **Text:** ASCII only; documentation pages, log messages and `CHANGELOG.md` entries match the code.

## Findings

- Grade each finding:
  - **must-fix:** defect, regression, missing test for a behavior change, style rule violation, non-ASCII text;
  - **should-fix:** readability, naming, a missed simplification;
  - **note:** an observation that needs no change.
- Each finding names `file:line` and a concrete fix.
- Prefer fewer, verified findings to many uncertain ones.
- Report in the chat or session by default. Post a review on GitHub only when the maintainer asks for it (see
  [outward writes](git-and-pull-requests.md#outward-writes)); the procedure is POCO's
  [posting a review](https://github.com/pocoproject/poco/blob/main/ai-agents/reviewing.md#posting-a-review), with
  `pocoproject/poco-osp` as the repository.

## Follow-up rounds

- On each new head, check every earlier finding and state which are resolved and which remain.
- A head that does not compile is returned without further review.

## Own pull requests

When reviewing your own change, fix the findings directly instead of writing comments, then run the checks again.

## Definition of done

A change is done, and a pull request may be opened, only after all of these steps ran. The same list applies to your own
changes and to changes you review.

1. A clean build of the affected parts, configured from scratch with `ENABLE_COMPILER_WARNINGS=ON` (see
   [cmake.md](cmake.md#options)), with no new warnings.
2. The full test suites of the affected parts, run again from scratch (see [testing.md](testing.md)); for a change to
   the CMake modules, the script tests in `cmake/testsuite/` too.
3. The analysers of [analysis.md](analysis.md) on the changed files: clang-tidy, cppcheck, and ASan, UBSan and TSan when
   tests cover the changed code. Concurrency, transport or storage code also gets the
   [extra quality gates](analysis.md#extra-quality-gates).
4. The review skills or sub-agents of the agent, as listed under "Tools first".
5. The whole diff, read hunk by hunk against CODING_STYLE.md and the dimensions above, with the tool findings beside it.
   Each hunk gets a verdict.

A step that was skipped is reported as skipped, with the reason (for example a tool that is not installed). "Done" means
that all steps ran, their results are reported, and no must-fix finding is open.
