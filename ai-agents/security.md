# Security Fixes

How vulnerabilities in POCO OSP are reported and fixed. Pushes and advisory changes need approval (see
[outward writes](git-and-pull-requests.md#outward-writes)).

## Reporting

- Report a vulnerability by e-mail to the security address given in [CONTRIBUTING.md](../CONTRIBUTING.md), or through
  "Report a vulnerability" on the repository's Security tab where private reporting is enabled.
- Do not file a public issue or pull request for an unpublished vulnerability.
- A vulnerability in POCO itself is reported to POCO, as POCO's
  [security.md](https://github.com/pocoproject/poco/blob/main/ai-agents/security.md) describes.

## Developing the fix

- Fixes for unpublished vulnerabilities are developed privately: in the advisory's temporary private fork or on a branch
  that is not pushed to the public repository.
- Until the release is available, public pull request titles, descriptions, branch names and commit messages are
  neutral: no advisory ID, no CVE, no description of the exploit.
- A fix has a regression test that fails without it (see [testing.md](testing.md)), and the touched tests pass under the
  sanitizers (see [analysis.md](analysis.md)).
- Code that handles requests, sessions, credentials, bundle files or other input from outside the process is checked
  with truncated and malformed input as well.

## Advisory content

Write in a factual tone, without speculation about impact beyond what was verified: a one-line summary (component and
defect); the defect, the conditions under which it is reachable and the effect; the affected and patched versions; the
CWE; the CVSS 3.1 vector; the credits; configuration workarounds for users who cannot update.

## Release

- `CHANGELOG.md` and `doc/99100-OSPReleaseNotes.page` list each published advisory of a release under "Security fixes".
- Advisories are published after the release is available.
