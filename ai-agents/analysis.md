# Analysis

Dynamic and static checks for a change. Run them on the components you changed; report a tool that is missing instead of
skipping it silently (installation hints are in POCO's
[analysis.md](https://github.com/pocoproject/poco/blob/main/ai-agents/analysis.md#installing-missing-tools)). Unless
asked otherwise, only findings in changed lines count, and only in the files of this repository, not in the pulled POCO
under `<build-dir>/_deps/`.

## Sanitizers

Sanitizers are passed with POCO's `POCO_SANITIZEFLAGS` (flags separated by `;` or spaces); the root `CMakeLists.txt`
applies them to the OSP targets as well. Use a separate build directory per sanitizer and add `-DENABLE_TRACE=ON` for
stack traces in exceptions.

| Sanitizer | Configure option | Runtime environment |
|---|---|---|
| AddressSanitizer | `-DPOCO_SANITIZEFLAGS="-fsanitize=address"` | on macOS `ASAN_OPTIONS=detect_odr_violation=0:report_globals=0` (see [testing.md](testing.md#sanitizers-and-loaded-libraries)) |
| UndefinedBehaviorSanitizer | `-DPOCO_SANITIZEFLAGS="-fsanitize=undefined;-fno-sanitize=vptr"` | |
| ThreadSanitizer | `-DPOCO_SANITIZEFLAGS="-fsanitize=thread"` | `TSAN_OPTIONS="suppressions=<repo-root>/tsan.suppress,second_deadlock_stack=1"` |

CI jobs: `linux-gcc-cmake-sanitizers`, `macos-clang-cmake-sanitizers` (matrix entries `asan`, `ubsan`, `tsan` and
`hidden-asan`). The macOS ASan entries run with `ASAN_OPTIONS=detect_odr_violation=0:report_globals=0`; the Linux
`hidden-asan` entry runs with `ASAN_OPTIONS=detect_odr_violation=2`. The ThreadSanitizer entries use `tsan.suppress`
of the repository and of the pulled POCO.

```bash
cmake -S <repo-root> -B <build-dir>-tsan -DPOCO_OSP_ENABLE_TESTS=ON -DENABLE_TRACE=ON -DPOCO_SANITIZEFLAGS="-fsanitize=thread"
```

```bash
TSAN_OPTIONS="suppressions=<repo-root>/tsan.suppress,second_deadlock_stack=1" ctest --test-dir <build-dir>-tsan --output-on-failure -R "^OSPWeb$"
```

- The vptr check of UBSan is disabled, as in POCO.
- Fix a race instead of adding a suppression. A new entry in `tsan.suppress` gets a comment that explains why the
  reported race is harmless.
- Where sanitizers are not available, run the affected test class under valgrind memcheck:

  ```bash
  valgrind --leak-check=full --error-exitcode=1 ./OSP<Component>-testrunner <TestClass>
  ```

## Compiler warnings

Build with `-DENABLE_COMPILER_WARNINGS=ON` and fix every new warning in changed code. The root `CMakeLists.txt` applies
POCO's maintainer warnings to the OSP targets; the option does not add `-Werror`, so read the build log. CI jobs:
`macos-clang-cmake-warnings`, `windows-msvc-cmake-warnings`.

Functions that always throw and should be `[[noreturn]]` are found with Clang's `-Wmissing-noreturn`:

```bash
cmake -S <repo-root> -B <build-dir>-noreturn -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-Wmissing-noreturn
```

```bash
cmake --build <build-dir>-noreturn --parallel 2>&1 | grep -- -Wmissing-noreturn
```

## clang-tidy

The repository has no `.clang-tidy` file; pass the checks explicitly. Configure with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` to get `<build-dir>/compile_commands.json`.

```bash
clang-tidy -p <build-dir> -checks='-*,bugprone-*,concurrency-*,performance-*,cppcoreguidelines-*,modernize-*,misc-include-cleaner' <Component>/src/<File>.cpp
```

```bash
run-clang-tidy -p <build-dir> -checks='-*,bugprone-*,concurrency-*,performance-*,cppcoreguidelines-*,modernize-*,misc-include-cleaner' '<Component>/src/<File>.cpp'
```

Many `cppcoreguidelines-*` and `modernize-*` findings describe the existing style of the code. Report those only where
they concern changed code and agree with CODING_STYLE.md.

## cppcheck

```bash
cppcheck --project=<build-dir>/compile_commands.json --enable=warning,performance,portability --inline-suppr --file-filter='*/<Component>/src/*'
```

## Extra quality gates

Changes to transport, concurrency or storage code (the web server dispatcher and sessions, WebEvent, the shell
connections, the bundle loader and the code cache, the SQL user database) also get the checks of POCO's
[extra quality gates](https://github.com/pocoproject/poco/blob/main/ai-agents/analysis.md#extra-quality-gates): a
ThreadSanitizer run of the touched suites, repeated; coverage of the changed code; and tests with truncated and
malformed input for receive and parse paths, run under ASan and UBSan.
