# Contributing

## Bug Reports and Feature Requests

If you think you have found a bug or would like to see a feature in one of the
[upcoming releases](https://github.com/pocoproject/poco-osp/milestones), file an
[issue](https://github.com/pocoproject/poco-osp/issues). Please make sure that your explanation is clear and
coherent, and search for existing issues before posting. Issues in the POCO C++ Libraries themselves belong in the
[POCO repository](https://github.com/pocoproject/poco/issues).

Possible security issues or vulnerabilities can also be reported by email directly to the core team at
security AT pocoproject.org. The core team will respond to security issues within 24 hours.

## Code Contributions

Before writing any code, please read the

* [C++ Coding Style Guide](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md) of the POCO C++ Libraries,
  which applies to this repository, and
* [Instructions for AI coding agents](AGENTS.md), which also describe the build, test and review procedures.

[Pull requests](https://docs.github.com/en/pull-requests) are the channel for code contributions:

1. [Fork](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks/fork-a-repo)
   the repository and keep the fork synchronized with it.
2. Create a branch with a descriptive name off `main`.
3. Write the code changes and make sure they compile. The build pulls the POCO C++ Libraries itself.
4. Write the tests (in most cases there are some to write) and make sure they pass.
5. Test the changes, preferably on Linux, Windows and macOS.
6. Send a pull request against `main`, or against a release branch for a fix to a release. Commit subjects have the
   form `type(Scope): description #issue`, with the types `fix`, `feat`, `enh` and `chore`.

## Contributing Mini-FAQ

**Q:** Is there a contributor license agreement to sign?

**A:** No. All contributions are under the [Boost Software License](https://spdx.org/licenses/BSL-1.0). For
contributions to existing libraries, assign the copyright to "Applied Informatics Software Engineering GmbH and
Contributors" by using the license header of the existing source files.

**Q:** How can I get write access to the repository?

**A:** Direct write access stays with a small group of core contributors; forks and pull requests cover everything
else. To ask for access, contact the maintainers at poco@pocoproject.org.

**Q:** Why does compilation or a test run fail on my platform?

**A:** The core team keeps recent versions of Visual Studio, clang and gcc building, with passing tests, on Windows,
macOS and Linux. Other platforms depend on their users: the fix is usually small, and a pull request is welcome.
