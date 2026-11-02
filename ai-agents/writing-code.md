# Writing Code

Rules for changing library and bundle code. The style itself (layout, naming, formatting, comments, error handling) is
defined in POCO's [CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md); this file does not
repeat it. Where POCO's [writing-code.md](https://github.com/pocoproject/poco/blob/main/ai-agents/writing-code.md) gives
general rules (platform-specific code, deprecation, protocol and parser code, waits and timeouts), they apply here too.

## Before writing

- Read CODING_STYLE.md and the code around the place you change. New code looks like the code next to it.
- Reuse before writing: the C++ standard library first, then POCO classes, then the OSP core library and the helpers of
  the component. Search the repository (`git grep`) before adding a function.
- When the change, or the code it touches, duplicates logic, extract a helper instead of adding a second copy, and place
  it in the component that owns the concept.
- Make the smallest diff that solves the problem. No unrelated refactoring, renaming or reformatting.
- Do not reformat lines whose content does not change, comments included. An unchanged comment keeps its line breaks,
  also when the code around it moves.
- A change that POCO needs belongs in POCO, through a POCO pull request.

## Public API

- Changes to public headers are deliberate, documented in the `///` reference comments and consistent with the existing
  API of the component (names, parameter order, error reporting).
- Visibility or ownership is never widened only to make something compile.
- Bundles of other projects derive from the classes of this repository and implement its service interfaces: adding a
  pure virtual function, changing a signature or removing a protected member breaks them. Such a change is listed under
  "Breaking changes" in `CHANGELOG.md`.
- `final` is used only on classes that are new in this repository, never added to an existing public class.

## New files

| Kind | Location |
|---|---|
| Public header of the core library | `include/Poco/OSP/<Class>.h`, or a subdirectory that follows the namespace (`include/Poco/OSP/Auth/`) |
| Public header of a component | `<Component>/include/Poco/OSP/<Component>/<Class>.h`, in a directory that follows the namespace |
| Source, private header, bundle activator | `<Component>/src/` (`src/` for the core library) |
| Test | `<Component>/testsuite/src/<Class>Test.h`, `.cpp` (see [testing.md](testing.md)) |

- Every file starts with the banner of
  [Example 1 of CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md#example-1), with
  `Library: OSP` for the core library and the component path for a component (`Library: OSP/Web`,
  `Library: OSP/Auth/Data`), the copyright line of the surrounding files and `SPDX-License-Identifier: BSL-1.0`. The
  include guard follows the path below `Poco/`: `OSP_Web_WebSession_INCLUDED` for `Poco/OSP/Web/WebSession.h`.
- Exported classes carry the export macro of their library (`OSP_API`, `OSPWeb_API`, `OSPAuthData_API`), defined in the
  library's main header: `Poco/OSP/OSP.h`, `Poco/OSP/Web/Web.h`, `Poco/OSP/Auth/Data/Data.h`.
- Adding a file needs the configure step again, or an entry in the source list of the component
  ([cmake.md](cmake.md#source-files)).

## Bundles

- A bundle activator derives from `Poco::OSP::BundleActivator` and is exported with
  `POCO_BEGIN_MANIFEST(BundleActivator)` and `POCO_EXPORT_CLASS(<Class>)`; the bundle specification names it in
  `<activator>` with the name it is exported under.
- A bundle reads its settings in `start()` before it registers services, extensions or factories, so that a start that
  fails on a setting leaves nothing registered.
- Settings that the bundle documents are checked for their range at the start; an invalid value fails the start with
  `Poco::InvalidArgumentException`.

## Errors

Use POCO exceptions (`Poco/Exception.h`) and the assertion macros of `Poco/Bugcheck.h` as described in the section
"Error Handling" of CODING_STYLE.md. POCO OSP's own exceptions are in `Poco/OSP/OSPException.h`.

## Documentation

- Reference documentation is written as `///` comments after the declaration, as shown in the section "Appendix:
  Documentation" of CODING_STYLE.md. PocoDoc builds the reference from them.
- User guides are `doc/*.page` and `<Component>/doc/*.page` in PocoDoc markup: first line title, second line category,
  then `!!!` (level 1), `!!` (level 2) and `!` (level 3) headings, `<*italic*>`, `<!bold!>`, `<[code]>`, `[[url text]]`
  links and indented code blocks.
- A change in behavior updates the reference comment, the guide where one describes it, `CHANGELOG.md` and the
  release notes (`doc/99100-OSPReleaseNotes.page`).
- `PocoDoc/mkdoc <build-dir> <output-dir> [<cmake-option>...]` builds PocoDoc from the pulled POCO, generates the
  documentation with `PocoDoc/cfg/mkdoc-poco-osp.xml` and runs `PocoDoc/checkdoc`, which fails when a `.page` file or
  a public header does not reach the output. A new page or public header must pass it; the CMake options are those of
  the configure, for example `-DPOCO_OSP_POCO_REPOSITORY=<poco-checkout>`.
- A message that a guide quotes or that a test matches changes together with them.

## Before handing over

- Every behavior change has a test: [testing.md](testing.md).
- Every changed line is ASCII; run the check of
  [Rule 86 of CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md#rule-86).
- Build with `ENABLE_COMPILER_WARNINGS=ON` and run the checks of [analysis.md](analysis.md) on the changed files.
- Complete the [definition of done](reviewing.md#definition-of-done).
