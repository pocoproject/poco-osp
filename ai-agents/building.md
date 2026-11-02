# Building POCO OSP

Prerequisites, the POCO the build uses, and platform notes. The configure and build commands, the CMake options and the
rules for CMake files are in [cmake.md](cmake.md). Placeholders: `<repo-root>` is the checkout, `<build-dir>` a build
directory.

## Prerequisites

- CMake 3.26 or newer (`cmake_minimum_required(VERSION 3.26)` in `CMakeLists.txt`).
- A C++20 compiler: the root `CMakeLists.txt` stops the configure when `cxx_std_20` is not in
  `CMAKE_CXX_COMPILE_FEATURES` or the standard library has no `std::map::contains()`. The CI job `linux-gcc11-cmake`
  builds with GCC 11 on Ubuntu 22.04.
- git, which the configure runs to pull POCO.
- OpenSSL development files: the default build enables the parts that need POCO's Crypto and NetSSL libraries.

## POCO

The configure pulls POCO with CMake's FetchContent (`cmake/PocoOSPFetchPoco.cmake`) and builds the POCO libraries that
the enabled OSP parts need, with `POCO_MINIMAL_BUILD=ON`; POCO's own test suites are not built. POCO OSP needs POCO
2.0 or newer: a POCO without RemotingNG stops the configure.

| Setting | Meaning |
|---|---|
| `POCO_OSP_POCO_REPOSITORY` | POCO git URL (default `https://github.com/pocoproject/poco.git`) or a local directory, relative to `<repo-root>`. A local git repository is cloned like a remote one; a plain source tree is used as it is, and `POCO_OSP_POCO_VERSION` does not apply to it |
| `POCO_OSP_POCO_VERSION` | `release` (default): the newest POCO 2.x release (tag `poco-X.Y.Z-release` or `poco-X.Y.ZpN-release`), or POCO `main` while no such release exists; `latest`: POCO `main`; otherwise a tag, branch or commit |
| `FETCHCONTENT_SOURCE_DIR_POCO` | Absolute path of a POCO source tree, used as it is; the two settings above do not apply |

- `release` and `latest` are resolved to a tag or a commit at the first configure and kept in the cache. A reconfigure
  contacts the repository again only when the repository or the ref changes, so it works offline. To resolve again:
  `cmake -U POCO_OSP_POCO_RESOLVED_REF <build-dir>`.
- A pulled POCO is cloned into `<build-dir>/_deps/poco-src`.
- A superproject can add POCO itself (`add_subdirectory` or FetchContent) and then POCO OSP with `add_subdirectory`;
  POCO OSP then uses that POCO and does not pull one. An installed POCO (`find_package`) is refused.

## Optional parts and their packages

| OSP option | POCO option | Package (Debian/Ubuntu) |
|---|---|---|
| `ENABLE_OSP_CRYPTO`, `ENABLE_OSP_NETSSL`, `ENABLE_OSP_JWT` and the parts that need them | `ENABLE_CRYPTO`, `ENABLE_NETSSL`, `ENABLE_JWT` | `libssl-dev` |
| `ENABLE_OSP_SHELL_SSH` | `ENABLE_SSH` | `libssh-dev` |
| `ENABLE_OSP_AUTH_LDAP` | | `libldap-dev` and `pkg-config` (OpenLDAP); WinLDAP on Windows; not supported on macOS |
| `ENABLE_OSP_SHELL_CLIENT` | | `libedit-dev` (optional: without it, `ospsh` has no line editing; on macOS libedit is part of the SDK) |
| `ENABLE_OSP_DATA_ODBC` | `ENABLE_DATA_ODBC` | `unixodbc-dev` |
| `ENABLE_OSP_DATA_MYSQL` | `ENABLE_DATA_MYSQL` | `default-libmysqlclient-dev` |
| `ENABLE_OSP_DATA_POSTGRESQL` | `ENABLE_DATA_POSTGRESQL` | `libpq-dev` |

- The configure switches on the POCO options that the enabled OSP parts need. POCO requires the package of an enabled
  library and stops the configure without it, except for SSH, which POCO switches off with a status message.
- An OSP part whose POCO library the pulled POCO does not build is switched off with a warning. The configure summary
  (`POCO OSP <version>`) lists the enabled parts, and the disabled ones with the reason a part was switched off.
- In a superproject, a missing POCO library is a configure error that names the POCO option to switch on.

All packages:

```bash
sudo apt install cmake ninja-build git g++ libssl-dev libssh-dev libldap-dev pkg-config libedit-dev unixodbc-dev default-libmysqlclient-dev libpq-dev
```

On macOS, point CMake at a non-system OpenSSL with `-DOPENSSL_ROOT_DIR=<openssl-prefix>`.

## Static builds

With `BUILD_SHARED_LIBS=OFF`, `ENABLE_OSP_BUNDLES` and `POCO_OSP_ENABLE_TESTS` are switched off, and with them every
part that needs bundles: a bundle library loaded at run time would have its own copy of the static libraries.

## Windows

- Compiler: MSVC with the x64 developer environment, with Ninja (`-G Ninja -DCMAKE_BUILD_TYPE=Release`).
- OpenSSL and libssh can come from vcpkg (`x64-windows` triplet); pass `-DOPENSSL_ROOT_DIR=<openssl-prefix>` and
  `-DCMAKE_PREFIX_PATH=<libssh-prefix>`.
- `POCO_MT=ON` selects the static runtime (`/MT`), as in POCO.
- When the source tree is copied to a separate build machine, copy an export of the commit rather than a working tree
  with converted line endings:

  ```bash
  git archive --format=zip -o poco-osp-src.zip HEAD
  ```

## Building for Linux from another operating system

Linux-only code is built and tested in a Linux virtual machine or container with the packages above. Use a separate
build directory for it; do not share a build directory between operating systems.

## Next steps

- Configure, build and CMake options: [cmake.md](cmake.md)
- Running tests: [testing.md](testing.md)
- Sanitizers, warnings and static analysis: [analysis.md](analysis.md)
