# Instructions for AI Coding Agents

This file is for AI coding agents and the contributors who work with them on POCO OSP. It states the rules that always
apply and points to the detailed instructions in [ai-agents/](ai-agents/README.md). The C++ coding style is POCO's
[CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md); contribution terms are in
[CONTRIBUTING.md](CONTRIBUTING.md).

Personal or machine-specific settings (tool paths, local build hosts, commit trailers) do not belong here; put them in
an untracked local file such as `CLAUDE.local.md`, or in the directory `ai-agents/local/`, which git ignores (see
[local notes](ai-agents/README.md#local-notes)). MCP server registrations that carry credentials (`.mcp.json`) are never
committed.

## Project

POCO OSP (Open Service Platform) is a C++ framework for modular applications on top of the POCO C++ Libraries: bundles
and their life cycle, the service registry, extension points, and bundles for the web server, authentication, the remote
shell and other services. It is licensed under the Boost Software License (`BSL-1.0`). `main` is the development branch.
The version is in `VERSION`, the version of the shared libraries in `libversion`.

The build pulls POCO 2.0 or newer with CMake's FetchContent and builds it together with POCO OSP in one CMake build.
`POCO_OSP_POCO_VERSION` chooses the POCO: `release` (default), `latest` (POCO `main`) or a tag, branch or commit.
`POCO_OSP_POCO_REPOSITORY` is the POCO git URL or a local directory; a local directory works offline. Issues in POCO
itself belong in the [POCO repository](https://github.com/pocoproject/poco).

## Repository layout

| Path | Content |
|---|---|
| `include/Poco/OSP/`, `src/`, `testsuite/` | The OSP core library (`Poco::OSP`) and its test suite |
| `<Component>/` | One directory per library, bundle or tool: `Web`, `WebServer`, `WebEvent`, `Auth` (with `Auth/Data`, `Auth/TwoFactor`, `Auth/Remoting`, `Auth/AuthCommand`), `Shell` (with `Shell/Commands`, `Shell/Client`), `BundleCreator`, `BundleSign`, `Net`, `Data/SQLite`, ... |
| `<Component>/include/Poco/OSP/<Component>/` | Public headers of a component library, in a directory that follows the namespace (`Auth/Data/include/Poco/OSP/Auth/Data/`) |
| `<Component>/src/` | Implementation files, private headers and the bundle activator |
| `<Component>/*.bndlspec`, `<Component>/bundle/` | Bundle specification and the files the bundle contains (`extensions.xml`, `bundle.properties`) |
| `<Component>/cmake/` | Package configuration of a component library (`Poco<Library>Config.cmake`) |
| `<Component>/testsuite/` | CppUnit test suite of the component |
| `doc/`, `<Component>/doc/` | User documentation pages (`*.page`, PocoDoc markup) and the release notes |
| `samples/` | Sample bundles and programs; their tests are in `samples/testsuite/` |
| `cmake/` | CMake modules: options, pulling POCO, `poco_osp_bundle()`, `poco_remote_gen()`; `cmake/testsuite/` tests the modules and the installed packages |
| `PocoDoc/` | The documentation build `mkdoc`, its configuration (`cfg/`, `resources/`) and the check `checkdoc` |
| `.github/workflows/` | CI, CodeQL and release workflows, and the closing of inactive issues |

## Build and test (quick reference)

Configure and build everything with the tests. The configure pulls POCO; `-DPOCO_OSP_POCO_REPOSITORY=<poco-checkout>`
uses a local POCO instead ([building](ai-agents/building.md#poco)):

```bash
cmake -S . -B <build-dir> -DPOCO_OSP_ENABLE_TESTS=ON
```

```bash
cmake --build <build-dir> --parallel
```

Build only the parts you work on: `-DPOCO_OSP_MINIMAL_BUILD=ON` with `-DENABLE_OSP_<PART>=ON` for each part. A part
that needs another one that is off stops the configure with a message that names the options to switch on. The options
and the parts each one needs are defined in `cmake/PocoOSPOptions.cmake`.

```bash
cmake -S . -B <build-dir> -DPOCO_OSP_MINIMAL_BUILD=ON -DPOCO_OSP_ENABLE_TESTS=ON -DENABLE_OSP_BUNDLES=ON -DENABLE_OSP_NET=ON -DENABLE_OSP_WEB=ON
```

Run the tests with CTest, which sets the environment each test runner needs. The test names are `OSP` for the core
library and `OSP<Component>` for the components (`OSPWeb`, `OSPAuthData`):

```bash
ctest --test-dir <build-dir> --output-on-failure -R "^OSPWeb$"
```

Re-run the configure step after adding source files ([source files](ai-agents/cmake.md#source-files)). Details:
[CMake](ai-agents/cmake.md), [building](ai-agents/building.md), [testing](ai-agents/testing.md).

## Rules that always apply

1. **Coding style.** All C++ code follows POCO's
   [CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md). Its *Rules* are mandatory and a
   review finding on a rule is a must-fix; *Recommendations* are the default. Read the guide and the surrounding code
   before writing; new code looks like the code around it.
2. **ASCII only** in every file, as
   [Rule 86 of CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md#rule-86) states, with its
   check.
3. **OpenSSL** version checks and the minimum version follow
   [Rule 103 of CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md#rule-103).
4. **New files** carry the banner of
   [Example 1 of CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md#example-1); locations
   are in [writing code](ai-agents/writing-code.md#new-files).
5. **Tests.** Every bug fix and every behavior change comes with a CppUnit test, or a CMake script test for the CMake
   modules, that fails without the change.
6. **Smallest diff.** No unrelated refactoring or reformatting; no new third-party dependency without the maintainers'
   agreement.
7. **Definition of done:** before reporting a change as done or opening a pull request, run the checks in
   [reviewing](ai-agents/reviewing.md#definition-of-done) and report any step that was skipped.
8. **Commits and pull requests:** branch names in kebab-case with the issue number first; commit messages and PR titles
   in the form `type(scope): description #issue` with the types `fix`, `feat`, `enh`, `chore`. The scope is the
   component directory (`Web`, `Auth/Data`), `OSP` for the core library, or `cmake`, `ci`, `doc` for changes across the
   repository. See [git and pull requests](ai-agents/git-and-pull-requests.md).
9. **Outward actions need approval:** an action that others can see on GitHub is taken only after the maintainer
   approved that specific action, as listed in [outward writes](ai-agents/git-and-pull-requests.md#outward-writes).
10. **Security.** Do not discuss an unpublished vulnerability in public issues, pull requests or commit messages. See
    [security](ai-agents/security.md).

## Task index

| Task | Read |
|---|---|
| Prerequisites, packages, pulling POCO, platform notes | [ai-agents/building.md](ai-agents/building.md) |
| Configure and build, CMake options, change CMake files, add a component | [ai-agents/cmake.md](ai-agents/cmake.md) |
| Write or change library and bundle code | [ai-agents/writing-code.md](ai-agents/writing-code.md), [CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md) |
| Write and run tests | [ai-agents/testing.md](ai-agents/testing.md) |
| Sanitizers, static analysis | [ai-agents/analysis.md](ai-agents/analysis.md) |
| Branches, commits, pull requests | [ai-agents/git-and-pull-requests.md](ai-agents/git-and-pull-requests.md) |
| Review a pull request or a change | [ai-agents/reviewing.md](ai-agents/reviewing.md) |
| Handle a security report | [ai-agents/security.md](ai-agents/security.md) |
