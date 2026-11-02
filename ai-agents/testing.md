# Testing

The C++ tests use the CppUnit framework of the pulled POCO (`Poco::CppUnit`); the CMake modules and the tools have
CMake script tests. Tests are built with `POCO_OSP_ENABLE_TESTS=ON` (see [cmake.md](cmake.md#options)).

## Structure

- Each component with tests has a test runner `OSP<Component>-testrunner` built from `<Component>/testsuite/`; the core
  library has `OSP-testrunner` (`testsuite/`), the samples `OSPSamples-testrunner` (`samples/testsuite/`).
- A test class derives from `CppUnit::TestCase` and has a static `suite()` that registers its test methods with
  `CppUnit_addTest(pSuite, Class, method)`. The skeleton is in POCO's
  [testing.md](https://github.com/pocoproject/poco/blob/main/ai-agents/testing.md#structure).
- The class's suite is added to the component suite in `<Component>/testsuite/src/<Name>TestSuite.cpp`
  (`Web/testsuite/src/WebTestSuite.cpp`). The core library groups its classes in `OSPCoreTestSuite`,
  `OSPUtilTestSuite`, `OSPBundleTestSuite` and `ServiceTestSuite`.
- Helpers that several test suites use are in `Web/testsuite/src/` and are built as the static libraries
  `OSPTestSupport` (`LogCapture`, `TemporaryBundle`) and `OSPTestSupportNet` (`ServerRequestStub`).
- Test data and test bundles live in `<Component>/testsuite/`. Tests that read files from the source tree build the
  path from the `POCO_BASE` environment variable, which points at the repository root.
- `cmake/testsuite/` holds the script tests of the CMake modules and of the installed packages, run with `cmake -P`.
  `cmake/PocoOSPScriptTest.cmake` has their helper functions.
- Adding a test file needs the configure step again, or an entry in the source list (see
  [cmake.md](cmake.md#source-files)).

## Running

Through CTest, from `<build-dir>`. The test names are the runner names without `-testrunner` (`OSP`, `OSPWeb`,
`OSPAuthData`, `OSPSamples`); the script tests are `OSPFetchPoco`, `OSPInstallConsumer`, `OSPBundleFunction`,
`OSPConfigure`, `OSPRemoteGen`, `OSPDirectoryCopy` and others:

```bash
ctest --output-on-failure -R "^OSPWeb$"
```

CTest sets the working directory and the environment of each test; `ctest -N -V -R "^OSPWeb$"` shows them. To run a
test runner directly, from `<build-dir>/bin`, with `POCO_BASE` set:

```bash
POCO_BASE=<repo-root> ./OSPWeb-testrunner -all
```

The core library's tests also need the directory of the test bundles:

```bash
POCO_BASE=<repo-root> POCO_OSP_TEST_BUNDLES=<build-dir>/testsuite/bundles ./OSP-testrunner -all
```

- In a Debug build the runners have the suffix `d` (`OSPWeb-testrunnerd`), as POCO's programs do.
- Run one suite, one test class or one test method by name instead of `-all` (`./OSP-testrunner BundleTest`).
- `-ignore <repo-root>/cppignore.lnx` (`cppignore.win` on Windows) names tests whose failures are not counted; CTest
  passes it. `CPPUNIT_IGNORE` takes a comma-separated list of test names with the same effect.
- `-print` lists the registered tests.

## Test quality

- A test for a fix fails without the fix. Check this by reverting the fix, or by removing each new check, and running
  the test.
- Test names say what is checked (`testLoginWithPasswordHashQuery`, not `testLogin2`). Names, comments and test data
  name the behavior, never an issue or a pull request.
- Assert exception types, not exception messages: with `ENABLE_TRACE=ON` the messages carry a stack trace.
- Cover error and timeout paths, not only the success path.
- No sleeps for synchronization. Wait on an event, a condition or a deadline loop, as POCO's
  [waits and timeouts](https://github.com/pocoproject/poco/blob/main/ai-agents/writing-code.md#waits-and-timeouts)
  describe.
- Tests need no hardware, network service or database server. A test that needs one is skipped with a message when it
  is not available.
- Before calling a flaky test fixed, run it repeatedly (for example 20 times), also in a ThreadSanitizer build
  ([analysis.md](analysis.md)).

## Sanitizers and loaded libraries

The tests load and unload bundle libraries at run time. On macOS, ASan runs set
`ASAN_OPTIONS=detect_odr_violation=0:report_globals=0`: Apple clang emits the ASan destructor that unregisters the
globals of a library into `__mod_term_func`, which dyld does not run on `dlclose()`. The globals and the metadata of an
unloaded library stay registered and poisoned, and a library mapped later at the same address reports false
global-buffer-overflow and ODR errors. The Web test runner sets these options itself on macOS. The checks of globals
stay on in the Linux runs.

## CI

- `.github/workflows/ci.yml` runs CTest through `.github/actions/retry-action` (up to three attempts). A pass on a later
  attempt hides a flaky test: check the attempt count in the job log and report the flaky test.
- `.github/path-filters.yml` decides which narrow jobs run for a pull request; the sanitizer jobs always run.
- The test jobs are `linux-gcc-cmake-sanitizers`, `macos-clang-cmake-sanitizers`, `windows-2025-msvc-cmake`, the probes
  `linux-gcc-cmake-poco-release`, `linux-gcc11-cmake` and `linux-gcc-cmake-superbuild`, and
  `linux-gcc-cmake-install-consumer`, which runs `OSPInstallConsumer` and `OSPBundleFunction`.
