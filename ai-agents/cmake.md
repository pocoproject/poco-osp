# CMake

How to configure POCO OSP and how to write its CMake files. The minimum is CMake 3.26
(`cmake_minimum_required(VERSION 3.26)` in `CMakeLists.txt`). Placeholders: `<repo-root>` is the checkout,
`<build-dir>` a build directory, `<Component>` a component directory such as `Web` or `Auth/Data`. Compilers, packages
and the POCO the build uses are in [building.md](building.md).

## Configure and build

```bash
cmake -S <repo-root> -B <build-dir> -DPOCO_OSP_ENABLE_TESTS=ON
```

```bash
cmake --build <build-dir> --parallel
```

Build only the parts you work on, and one test runner with the libraries it needs:

```bash
cmake -S <repo-root> -B <build-dir> -DPOCO_OSP_MINIMAL_BUILD=ON -DPOCO_OSP_ENABLE_TESTS=ON -DENABLE_OSP_BUNDLES=ON -DENABLE_OSP_NET=ON -DENABLE_OSP_WEB=ON
```

```bash
cmake --build <build-dir> --parallel --target OSPWeb-testrunner
```

- Use one build directory per branch and per configuration (sanitizer, warnings, release), outside tracked directories.
- Nothing is switched on behind the user's back: a part that needs another part that is off stops the configure with
  the message `OSP options conflict`, which names the options to switch on.
- The configure summary (`POCO OSP <version>`) lists the POCO source and version, the enabled parts and the disabled
  ones; a part that the configure switched off is listed with the reason.
- The default build type is `RelWithDebInfo`, as in POCO.

## Options

The options of the OSP parts are defined in `cmake/PocoOSPOptions.cmake`, with the parts each one needs
(`_poco_osp_part(<option> NEEDS ... POCO ...)`). "ON" below means ON unless `POCO_OSP_MINIMAL_BUILD` is ON.

| Option | Default | Purpose |
|---|---|---|
| `POCO_OSP_MINIMAL_BUILD` | OFF | Build only the core library, the bundle creator and the parts enabled explicitly |
| `POCO_OSP_ENABLE_TESTS` | OFF, or POCO's `ENABLE_TESTS` when a superproject adds POCO | Build the test suites of the core library and of each enabled part, and register them with CTest |
| `POCO_OSP_ENABLE_SAMPLES` | OFF, or POCO's `ENABLE_SAMPLES` when a superproject adds POCO | Build the samples and, with the tests, their test suite |
| `ENABLE_OSP` | ON (also minimal) | The core library `Poco::OSP` |
| `ENABLE_OSP_BUNDLE_CREATOR` | ON (also minimal) | The bundle creator `bundle` |
| `ENABLE_OSP_BUNDLES` | ON | Creating bundles; switched off in a static build |
| `ENABLE_OSP_BUNDLESIGN` | ON | The bundle signing library and the bundle signer `signbundle` |
| `ENABLE_OSP_STRIPBUNDLE` | ON | The bundle stripping tool `stripbundle` |
| `ENABLE_OSP_CODECACHE` | ON | The code cache utility `ccutil` |
| `ENABLE_OSP_CORE` | ON | The `osp.core` bundle |
| `ENABLE_OSP_NET`, `ENABLE_OSP_NETSSL`, `ENABLE_OSP_CRYPTO` | ON | The bundles of POCO's Net, NetSSL and Crypto libraries; NetSSL also enables the SecureWebServer and SecureMail bundles |
| `ENABLE_OSP_DATA`, `ENABLE_OSP_DATA_SQLITE` | ON | The bundles of POCO's Data library and SQLite connector |
| `ENABLE_OSP_DATA_ODBC`, `ENABLE_OSP_DATA_MYSQL`, `ENABLE_OSP_DATA_POSTGRESQL` | OFF | The bundles of the other Data connectors |
| `ENABLE_OSP_ACTIVERECORD` | OFF | The ActiveRecord bundle |
| `ENABLE_OSP_REDIS`, `ENABLE_OSP_JWT`, `ENABLE_OSP_PROMETHEUS` | ON | The bundles of POCO's Redis, JWT and Prometheus libraries |
| `ENABLE_OSP_WEB` | ON | The Web library and the Web, WebServer and WebEvent bundles |
| `ENABLE_OSP_BUNDLEADMIN` | ON | The BundleAdmin web application |
| `ENABLE_OSP_MAIL` | ON | The mail delivery service |
| `ENABLE_OSP_AUTH` | ON | The Auth libraries and bundles: the core, the SQL user database, two-factor authentication, and with the shell and RemotingNG parts the `auth` command and Auth Remoting |
| `ENABLE_OSP_AUTH_LDAP` | OFF | LDAP in OSP Auth |
| `ENABLE_OSP_SIMPLEAUTH` | ON | The SimpleAuth bundle |
| `ENABLE_OSP_SHELL` | ON | The remote shell and its command bundles |
| `ENABLE_OSP_SHELL_SSH` | OFF | SSH in the remote shell |
| `ENABLE_OSP_SHELL_CLIENT` | OFF | The shell client `ospsh` |
| `ENABLE_OSP_REMOTING` | ON | The RemotingNG TCP and REST bundles and OSP Auth Remoting |
| `ENABLE_OSP_UNIT` | OFF | The OSP Unit test runner bundle |

POCO options that also apply to the OSP sources, because the root `CMakeLists.txt` applies them:

| Option | Purpose |
|---|---|
| `BUILD_SHARED_LIBS` | Shared (default) or static libraries; a static build switches off bundles and tests |
| `POCO_SANITIZEFLAGS` | Sanitizer flags for compile and link (see [analysis.md](analysis.md)) |
| `ENABLE_COMPILER_WARNINGS` | POCO's maintainer warnings |
| `ENABLE_TRACE` | Stack traces in exceptions (defines `POCO_ENABLE_TRACE`) |
| `POCO_MT` | MSVC only: static runtime (`/MT`) |
| `CMAKE_EXPORT_COMPILE_COMMANDS` | Write `compile_commands.json` for clang-tidy and cppcheck |

The settings that choose POCO (`POCO_OSP_POCO_REPOSITORY`, `POCO_OSP_POCO_VERSION`, `FETCHCONTENT_SOURCE_DIR_POCO`)
are described in [building.md](building.md#poco).

NOTE: The repository and the POCO build share one CMake cache. An OSP option never reuses the name of a POCO option.

## Source files

- Most component `CMakeLists.txt` files collect `src/*.cpp` and `include/*.h` with `file(GLOB)` and pass them to POCO's
  `POCO_SOURCES_AUTO` and `POCO_HEADERS_AUTO`; others list their sources (`Web/CMakeLists.txt` lists the library sources
  and the bundle activator separately). Add a new source file to the list where there is one.
- The build does not notice a file that is added to or removed from a globbed directory: run the configure step again.
- A file whose name contains `_` and that has a `// Package:` line is treated by `POCO_SOURCES_AUTO` as a platform
  implementation file and is not compiled on its own (see POCO's
  [cmake.md](https://github.com/pocoproject/poco/blob/main/ai-agents/cmake.md#source-files)).

## Bundles

`cmake/PocoOspBundle.cmake` defines the functions that build bundles; their arguments are documented in the file:

- `poco_osp_bundle_module(<target> <source>...)` adds the library of a bundle: a `MODULE` library with the file name
  that POCO OSP loads.
- `poco_osp_bundle(<target> SPEC <bndlspec> DESTINATION <dir> [CODE ...] [DEPENDS ...] [WORKING_DIRECTORY <dir>]
  [SYMLINKS] [INSTALL_COMPONENT <component>])` runs the bundle creator. The bundles of the repository are written to
  `POCO_OSP_BUNDLE_DIR` (`<build-dir>/bundles`) and installed with the component `Bundles`.

`cmake/PocoOspRemoteGen.cmake` defines `poco_remote_gen()`, which runs RemoteGenNG at configure time and writes the
generated files into the build directory. An installed POCO OSP provides all three functions through
`find_package(Poco REQUIRED COMPONENTS OSPBundleCreator)`.

## Rules for CMake files

These are POCO's rules
([cmake.md](https://github.com/pocoproject/poco/blob/main/ai-agents/cmake.md#rules-for-cmake-files)),
with this repository's names.

1. **Targets, not directories.** Use the `target_*` commands. The directory-scope commands (`include_directories`,
   `add_definitions`, `add_compile_definitions`, `add_compile_options`, `add_link_options`, `link_libraries`,
   `link_directories`) are used only in the root `CMakeLists.txt` (rule 5).
2. **Scope every dependency.** Each `target_link_libraries` call names `PUBLIC`, `PRIVATE` or `INTERFACE`; the same
   applies to include directories, definitions and options.
3. **Namespaced targets.** Every library has an alias `add_library(Poco::OSP<Name> ALIAS <target>)` directly after
   `add_library`, and links POCO and OSP libraries as `Poco::<Name>` and external ones through imported targets.
4. **No library type.** `add_library(<target> ${SRCS})` without `STATIC` or `SHARED`; `BUILD_SHARED_LIBS` decides.
   Bundle libraries are `MODULE` libraries, added with `poco_osp_bundle_module()`.
5. **Project-wide options in one place.** The root `CMakeLists.txt` applies POCO's sanitizer flags, MSVC runtime
   library, Release stripping, `POCO_ENABLE_TRACE` and maintainer warnings to the OSP targets, because POCO sets them in
   its own directory scope, which does not reach them. Component files change no global state.
6. **`CMAKE_<LANG>_FLAGS` belongs to the user** (command line, toolchain file, `CXXFLAGS`).
7. **Compiler conditions.** Wrap compiler-specific options in a condition on `CMAKE_CXX_COMPILER_ID` or a generator
   expression.
8. **Language standard.** The root `CMakeLists.txt` sets `CMAKE_CXX_STANDARD` (see
   [building.md](building.md#prerequisites)), and `Poco::OSP` passes `cxx_std_20` to its users as a `PUBLIC` compile
   feature. Components set no standard of their own and never add `-std=` flags.
9. **Paths.** Use `CMAKE_CURRENT_SOURCE_DIR`, `CMAKE_CURRENT_BINARY_DIR` and `PROJECT_SOURCE_DIR`, never
   `CMAKE_SOURCE_DIR`: the repository can be a subdirectory of a superproject. Public include directories are given as
   `$<BUILD_INTERFACE:...>` and `$<INSTALL_INTERFACE:...>`.
10. **Build order comes from linking.** `add_dependencies` is needed only for a target that is used without being
    linked, such as a library or bundle that a test loads.
11. **Variables.** A part option is `ENABLE_OSP_<PART>`; any other cache variable starts with `POCO_OSP_`. In the root
    `CMakeLists.txt` and the modules in `cmake/`, a temporary variable starts with `_poco_osp_` and is unset after use;
    component files keep the variable names of the existing ones (`LIBNAME`, `SRCS_G`).
12. **Platform checks.** Test `WIN32`, `APPLE` or `CMAKE_SYSTEM_NAME`, and test for headers and functions, instead of
    assuming a platform from another one.

## Adding a component

1. `<Component>/include/Poco/OSP/<Component>/`, `src/`, `testsuite/src/`, the bundle specification
   `<Component>.bndlspec` with its `bundle/` directory, and `<Component>/doc/` with a page.
2. `<Component>/CMakeLists.txt`, modelled on `Web/CMakeLists.txt`: sources and headers, `DLLVersion.rc` for MSVC shared
   builds, `add_library` with the `Poco::` alias, `set_target_properties` with
   `VERSION`/`SOVERSION ${SHARED_LIBRARY_VERSION}`, `OUTPUT_NAME Poco<Library>` and `DEFINE_SYMBOL <Library>_EXPORTS`,
   scoped links and include directories, the bundle with `poco_osp_bundle_module()` and `poco_osp_bundle()`,
   `POCO_INSTALL(<Library>)`, `POCO_GENERATE_PACKAGE(<Library>)` and `add_subdirectory(testsuite)` under
   `POCO_OSP_ENABLE_TESTS`.
3. `<Component>/cmake/Poco<Library>Config.cmake`: one `find_dependency(Poco<Dependency>)` per POCO and OSP dependency,
   then `include("${CMAKE_CURRENT_LIST_DIR}/Poco<Library>Targets.cmake")`.
4. `<Component>/testsuite/CMakeLists.txt`, modelled on `Web/testsuite/CMakeLists.txt`:
   `poco_add_executable(OSP<Component>-testrunner ...)` and `add_test(NAME OSP<Component> ...)` with
   `-ignore ${POCO_OSP_CPPIGNORE_FILE}` and `POCO_BASE=${PROJECT_SOURCE_DIR}` in the test environment. An executable
   that links `Poco::Crypto`, `Poco::NetSSL` or `Poco::JWT`, or loads bundles that use them, calls
   `poco_osp_link_applink(<target>)` (cmake/PocoOSPSetup.cmake): without `OPENSSL_Applink`, OpenSSL for Windows x86
   and x64 ends the process when POCO passes it a file opened by the C runtime.
5. `cmake/PocoOSPOptions.cmake`: the option `ENABLE_OSP_<PART>` and its `_poco_osp_part()` line with the parts and POCO
   targets it needs.
6. `cmake/PocoOSPComponents.cmake`: `add_subdirectory(<Component>)` under the option.
7. `CHANGELOG.md` and `doc/99100-OSPReleaseNotes.page`.

## Known pitfalls

**The checkout is full of generated files (`CMakeCache.txt`, `CMakeFiles/`, `bin/`, `lib/`).**
Cause: CMake was configured with the checkout as the build directory (`cmake .`).
What to do: always configure with `-S <repo-root> -B <build-dir>`. To recover, list the changes with
`git status --short --ignored` and delete the CMake-generated files.

NOTE: Do not use `git clean -x` or `git clean -X` for this: they also delete ignored local files such as
`CLAUDE.local.md` and `ai-agents/local/`.

**A reconfigure does not pick up new POCO commits.**
Cause: `release` and `latest` are resolved once, and the pulled POCO is updated only when the repository or the ref
changes.
What to do: `cmake -U POCO_OSP_POCO_RESOLVED_REF <build-dir>`, or set `POCO_OSP_POCO_VERSION` to the ref.

**A part is missing from the build although its option is ON.**
Cause: the configure switched it off, because the pulled POCO does not build a POCO library it needs, because a part it
needs was switched off, or because `BUILD_SHARED_LIBS` is OFF.
What to do: read the warning and the `Disabled:` lines of the configure summary.
