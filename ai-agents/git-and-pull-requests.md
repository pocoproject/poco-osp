# Git and Pull Requests

## Branches

- `main` is the development branch. Pull requests go against `main`, or against a release branch for a fix to a
  release.
- Feature branches use kebab-case with the issue number first when there is one: `12-web-session-timeout`.
- Check `git status` before switching branches; use a separate build directory per branch (see
  [cmake.md](cmake.md#configure-and-build)).
- Changes in the working tree that you did not make belong to someone else: keep them, mention them, and never revert
  them. Ask when they conflict with your change.
- Before a git sequence of several steps (creating branches, cherry-picks, rebases, pushes, creating a pull request),
  show the plan: branch names, the commits involved, the pull request title and body. Execute it after approval; its
  outward writes still need their own approval (see [outward writes](#outward-writes)).

## Commits

- Conventional format: `type(scope): description` with the types `fix`, `feat`, `enh` and `chore`.
- The scope is the component directory relative to the repository root (`Web`, `Auth/Data`, `BundleCreator`), `OSP`
  for the core library, `samples` and `testsuite` for those directories, and `cmake`, `ci` or `doc` for changes across
  the repository. Several scopes are separated by commas.
- The issue number of this repository follows the subject: `fix(Web): reject a path with a control character #12`.
  Issues and pull requests of other repositories are not referenced in commit messages.
- Keep messages short: the subject line alone when it suffices; at most a short body when the reason is not visible in
  the diff. No bullet lists that restate every changed file.
- One logical change per commit; a fix and the test that pins it go in the same commit.

## Writing style for commits, pull requests, issues and documentation

Applies to commit messages, pull request and issue text, GitHub comments and documentation. Comments in code follow the
section [Comments](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md#45-comments) of CODING_STYLE.md.

- Clear standard technical English with plain vocabulary. No marketing tone and no filler adjectives. The same term for
  the same concept throughout a text.
- Documentation describes things as they are: no "now", "new" or "since version X". History belongs in `CHANGELOG.md`
  and `doc/99100-OSPReleaseNotes.page`.
- Constraints go into short NOTE lines, not into long paragraphs.
- ASCII only ([Rule 86 of CODING_STYLE.md](https://github.com/pocoproject/poco/blob/main/CODING_STYLE.md#rule-86)).
- Nothing public contains host names, local paths, user names or IP addresses.
- An issue or pull request comment states the technical problem and cites what its readers can open: tracked paths on
  named branches, issue and pull request numbers.

## Pull requests

- Title in the commit format.
- The description has about 150 words: the link to the issue (`Closes #<N>`), two or three sentences on the problem, one
  bullet per area of change, one line on tests, one line on follow-ups. Details belong in the issue.
- A change that needs a POCO change names the POCO pull request; this repository's pull request is merged after it.

## Merging

- Security fixes keep their commits (merge commit or rebase merge, no squash). Contributor pull requests and small
  housekeeping pull requests may be squashed. When unsure, ask the maintainer and name the method.

Fetch a pull request locally:

```bash
git fetch origin +refs/pull/<N>/head:pr/<N>
```

## History rewrites

- Before rewriting history, create a backup ref:

  ```bash
  git branch backup/<branch> <branch>
  ```

- Force-push only with `git push --force-with-lease`.

## Outward writes

These actions are visible to others. An agent takes each of them only after the maintainer approved that specific
action:

- pushes, including force-pushes and pushes of tags;
- opening, editing, closing and merging pull requests; approving CI runs;
- comments and reviews on issues, pull requests and commits, and dismissing a review;
- creating, editing and closing issues; setting labels and milestones;
- dismissing or reopening code scanning alerts;
- publishing or editing releases and running release workflows;
- creating, editing and publishing security advisories.

Local operations (creating branches, cherry-picks, rebases) need the shown plan of [Branches](#branches), not a separate
approval each. For an outward write:

1. Draft the text or describe the action, and show it.
2. Act only after an explicit go for this write; an earlier approval does not cover the next one.
3. Read the result back (for example `gh pr view`, `gh api`) and report what was written.

## Cleanup after a merge

Delete the feature branch and its build directory only after confirming that the content is on the target branch:

- merge or rebase merge: the branch tip is an ancestor of the target
  (`git merge-base --is-ancestor <branch> origin/main`);
- squash merge: the files of the branch match the target (`git diff <branch> origin/main -- <paths>` is empty).
