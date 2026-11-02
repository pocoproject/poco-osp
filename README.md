# POCO OSP

[![poco-osp-ci](https://github.com/pocoproject/poco-osp/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/pocoproject/poco-osp/actions/workflows/ci.yml)

### POCO OSP (Open Service Platform) is:

- A C++ framework for modular applications, built on the [POCO C++ Libraries](https://github.com/pocoproject/poco).
- Bundles: libraries and resources, packed into one file or directory, that are installed, resolved, started, stopped
  and uninstalled at run time, with versioned dependencies between them.
- A service registry, in which bundles register services and find the services of other bundles, by name or by query,
  and are notified when services come and go.
- Extension points, through which a bundle extends another one with an `extensions.xml` file.
- A web server whose request handlers and filters come from bundles, with sessions, resources served from bundles and
  events over WebSocket.
- Authentication and authorization: users, roles and permissions in the SQL user database or from LDAP, two-factor
  authentication with time-based one-time passwords, and permission checks for web paths and shell commands.
- A remote shell, over TCP or SSH, whose commands come from bundles, with the client `ospsh`.
- Open Source, licensed under the [Boost Software License](https://spdx.org/licenses/BSL-1.0).

## Components

| Directory | Content |
|---|---|
| `include/`, `src/` | The core library `Poco::OSP`: bundles and their life cycle, the bundle loader and repository, the code cache, the service registry, extension points, preferences, and the bundle installer service |
| `Core` | The `osp.core` bundle, a bundle without code that other bundles require for the core framework |
| `BundleCreator` | The bundle creator `bundle`; its CMake package provides `poco_osp_bundle()` and `poco_remote_gen()` from `cmake/` |
| `BundleSign` | Bundle signatures and their verification, and the bundle signer `signbundle` |
| `StripBundle` | `stripbundle`, which removes the shared libraries from a bundle |
| `CodeCacheUtility` | `ccutil`, which fills a code cache with the shared libraries of a bundle repository |
| `Web`, `WebServer`, `SecureWebServer` | The web server: request handlers and filters from bundles, sessions, HTTP and HTTPS |
| `WebEvent` | Events from the server to web clients over WebSocket |
| `BundleAdmin` | A web application that lists, installs, starts, stops and removes bundles |
| `Auth` | Authentication and authorization services: SQL user database (`Auth/Data`), LDAP, two-factor authentication (`Auth/TwoFactor`), RemotingNG authentication (`Auth/Remoting`), the shell commands `auth` and `passwd` (`Auth/AuthCommand`) |
| `SimpleAuth` | An authentication service for two users from the configuration |
| `Shell` | The remote shell, its commands (`Shell/Commands`, `Shell/LogLevelCommand`) and the client `ospsh` (`Shell/Client`) |
| `Mail`, `SecureMail` | A mail delivery service over SMTP, with TLS |
| `RemotingNG` | Bundles of POCO RemotingNG and its TCP and REST transports |
| `Net`, `NetSSL_OpenSSL`, `Crypto`, `Data` (`SQLite`, `ODBC`, `MySQL`, `PostgreSQL`), `ActiveRecord`, `JWT`, `Prometheus`, `Redis` | Bundles that make POCO libraries available to other bundles |
| `Unit` | A test runner for CppUnit tests in bundles |
| `samples` | Sample bundles and programs |

## Quick Start (with CMake)

### Prerequisites

- CMake 3.26 or newer. Ubuntu 22.04 ships 3.22 and Debian 12 ships 3.25; a newer one is in
  [Kitware's APT repository](https://apt.kitware.com) (Ubuntu) or in bookworm-backports (Debian 12), or can be
  installed with `pipx install cmake`.
- A C++20 compiler; the configure stops without one.
- git: the configure pulls POCO.
- OpenSSL headers and libraries.
- Optional: libssh (SSH in the shell), OpenLDAP (LDAP in Auth), libedit (line editing in `ospsh`), and the unixODBC,
  MySQL and PostgreSQL client libraries (the Data connector bundles).

### Installing the Dependencies

#### Debian Linux (including Ubuntu)

```
$ sudo apt-get -y update && sudo apt-get -y install git g++ cmake libssl-dev libssh-dev libldap-dev pkg-config libedit-dev unixodbc-dev default-libmysqlclient-dev libpq-dev
```

#### RedHat Linux

```
$ sudo dnf install -y git gcc-c++ cmake openssl-devel libssh-devel openldap-devel pkgconf-pkg-config libedit-devel unixODBC-devel mysql-devel libpq-devel
```

On Rocky Linux 9, `unixODBC-devel` and `mysql-devel` are in the CRB repository, which is not enabled by default.

#### macOS (with Homebrew)

```
$ brew install cmake openssl@3 libssh unixodbc mysql-client libpq
```

LDAP in Auth is not supported on macOS; libedit is part of the SDK.

#### Windows (with WinGet and vcpkg)

```
$ winget install -e --id Git.Git
$ winget install -e --id Kitware.CMake
$ winget install -e --id ShiningLight.OpenSSL.Dev
$ vcpkg install libssh:x64-windows
```

OpenSSL for x86 and x64 ends a process whose executable does not provide `OPENSSL_Applink` when POCO Crypto or
NetSSL passes it a file opened by the C runtime. An application that loads OSP bundles using Crypto or NetSSL therefore
links `OpenSSL::applink` from `find_package(OpenSSL)`, as POCO's executables do.

### Building

```
$ git clone https://github.com/pocoproject/poco-osp.git
$ cd poco-osp
$ cmake -S . -B cmake-build
$ cmake --build cmake-build --config Release
```

The configure pulls POCO and builds it in the same build. POCO OSP needs POCO 2.0 or newer.

- `-DPOCO_OSP_POCO_VERSION=<version>` chooses the POCO: `release` (default) is the newest POCO 2.x release, or POCO
  `main` while there is none; `latest` is POCO `main`; anything else is a POCO tag, branch or commit. `release` and
  `latest` are resolved once, at the first configure.
- `-DPOCO_OSP_POCO_REPOSITORY=<directory>` uses a local POCO git repository or source tree instead of the GitHub
  repository, which also works offline.

On macOS, tell CMake where OpenSSL is, for example with Homebrew:

```
$ cmake -S . -B cmake-build -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3
```

#### Options

| Option | Default | Purpose |
|---|---|---|
| `POCO_OSP_MINIMAL_BUILD` | OFF | Build only the core library, the bundle creator and the parts enabled explicitly |
| `ENABLE_OSP_<PART>` | ON for most parts | Enable one part, for example `ENABLE_OSP_WEB`, `ENABLE_OSP_AUTH`; the list is in `cmake/PocoOSPOptions.cmake` |
| `ENABLE_OSP_AUTH_LDAP`, `ENABLE_OSP_SHELL_SSH`, `ENABLE_OSP_SHELL_CLIENT` | OFF | LDAP in Auth, SSH in the shell, the shell client `ospsh` |
| `ENABLE_OSP_DATA_ODBC`, `ENABLE_OSP_DATA_MYSQL`, `ENABLE_OSP_DATA_POSTGRESQL`, `ENABLE_OSP_ACTIVERECORD`, `ENABLE_OSP_UNIT` | OFF | The other Data connectors, ActiveRecord, the OSP Unit test runner |
| `POCO_OSP_ENABLE_TESTS` | OFF, or POCO's `ENABLE_TESTS` when a superproject adds POCO | Build the test suites and register them with CTest |
| `POCO_OSP_ENABLE_SAMPLES` | OFF, or POCO's `ENABLE_SAMPLES` when a superproject adds POCO | Build the samples |
| `BUILD_SHARED_LIBS` | ON | Shared or static libraries; bundles need shared libraries |

A part that needs another part whose option is OFF stops the configure with a message that names the options; a part
that the configure switches off also switches off the parts that need it. The configure summary lists the enabled and
the disabled parts.

#### Installing

```
$ cmake --install cmake-build --prefix <prefix>
```

POCO and POCO OSP are installed into one prefix: the libraries, the headers, the tools, the CMake packages in the
POCO package directory, and the bundles into `<prefix>/bundles`.

#### Using POCO OSP from a CMake Project

```cmake
find_package(Poco REQUIRED COMPONENTS OSP OSPWeb OSPBundleCreator)

poco_osp_bundle_module(com.example.hello src/HelloBundleActivator.cpp)
target_link_libraries(com.example.hello PRIVATE Poco::OSPWeb Poco::OSP Poco::Foundation)

poco_osp_bundle(com.example.hello-bundle
	SPEC ${CMAKE_CURRENT_SOURCE_DIR}/Hello.bndlspec
	DEPENDS com.example.hello
	CODE $<TARGET_FILE:com.example.hello>
	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bundles
)
```

Configure the project with `-DCMAKE_PREFIX_PATH=<prefix>`. The component `OSPBundleCreator` provides the bundle creator
and the functions `poco_osp_bundle_module()`, `poco_osp_bundle()` and `poco_remote_gen()`, which runs RemoteGenNG at
configure time and writes the generated sources into the build directory.

## Documentation

The documentation pages are in the repository, in PocoDoc format: `doc/` has the overview, getting started, the
tutorial, and the pages on bundles, modules, the bundle creator, the service registry, the standard services,
authentication and the release notes; the `doc/` directories of the components describe the web server, web events, the
shell, BundleAdmin, signed bundles, the mail service, JWT, Prometheus and the RemotingNG TCP transport. `PocoDoc/mkdoc`
generates the HTML documentation, with the reference of the public headers, and checks that every page and header is in
it.

## Community and Contributing

POCO OSP is part of the [POCO project](https://pocoproject.org). Please see [CONTRIBUTING](CONTRIBUTING.md) for
submitting contributions, bug reports, feature requests or security issues, and [AGENTS.md](AGENTS.md) for the build,
test and review procedures. Issues in the POCO C++ Libraries themselves belong in the
[POCO repository](https://github.com/pocoproject/poco/issues).

## License

POCO OSP is licensed under the [Boost Software License 1.0](LICENSE).
