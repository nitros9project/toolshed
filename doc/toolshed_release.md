# Releasing ToolShed

ToolShed command-line releases use tags such as `v2.7.0`. DiskShed has a
separate release workflow and tags such as `DiskShed-v1.0.0`; see below.
Do not use the old underscore tag format or modify existing release tags.

The examples below use `2.7.0`. Substitute the new version, release date,
branch name, commit IDs, PR numbers, and run IDs throughout.

## 1. Start from current upstream main

Check `git status --short --branch` first. Preserve any existing work; do
not reset or discard it to prepare a release.

```sh
git switch main
git pull --ff-only origin main
git pull --ff-only upstream main
git switch -c release-2.7.0
```

If a pull cannot fast-forward, stop and investigate. Do not reset local
commits without reviewing them. Use a feature branch without slashes and
push preparation commits to the fork (`origin`), not upstream main.

## 2. Update versions and changelog

Update all three version locations:

| File | Value |
| --- | --- |
| `build/unix/rules.mak` | `VERSION = 2.7.0` |
| `README.md` | `Version 2.7.0` |
| `doc/ToolShed.md` | `ToolShed v2.7.0` |

Add the release section to `doc/changelog.md`, with the actual release
date. Review the changes since the previous **ToolShed** tag, not the most
recent DiskShed tag:

```sh
git log v2.6.1..upstream/main --oneline
gh release view v2.6.1 --repo nitros9project/toolshed
git grep -n '2\.6\.1'
```

Check that the changelog includes relevant features, compatibility
changes, fixes, and build updates. Historical version references in the
changelog should remain unchanged.

## 3. Build, test, and submit the release PR

```sh
make -C build/unix -B
git diff --check
build/unix/os9/os9 --version
```

Use `-B` to force rebuilding with the new version macro: an incremental
build may keep binaries compiled with the previous version. Check that
all unit-test suites pass and that the executable reports the new version.

The manual build needs the `markdown` command. Its Makefile currently
ignores a missing-command error, so a successful `make` exit alone does
not prove the HTML manual was generated. Ensure the packaged
`ToolShed.html` is nonempty. Release CI installs `markdown`.

```sh
git add build/unix/rules.mak README.md doc/ToolShed.md doc/changelog.md
git commit -m "Prepare ToolShed v2.7.0 release"
git push -u origin release-2.7.0
gh pr create --repo nitros9project/toolshed \
  --head tlindner:release-2.7.0 --base main \
  --title "Release ToolShed v2.7.0" \
  --body "Update versions and changelog; local build and tests passed."
```

Wait for all PR checks, including Linux GCC, Windows MinGW, and both
macOS architectures. A local Apple clang build does not replace CI.

```sh
gh pr checks <PR> --repo nitros9project/toolshed --watch
```

If no checks are reported immediately after PR creation, wait for GitHub
to register them before retrying. Inspect failed jobs rather than
assuming a compiler failure:

```sh
gh run view <RUN> --repo nitros9project/toolshed --log-failed
```

A macOS ARM64 job can be cancelled without starting because GitHub has
no hosted runner available. Check the job annotations. For that transient
capacity failure, rerun failed jobs rather than changing the code:

```sh
gh run rerun <RUN> --repo nitros9project/toolshed --failed
```

Merge the PR after checks pass. If it edits workflow files, the CLI token
needs `workflow` scope; otherwise GitHub can refuse the merge:

```sh
gh auth refresh -h github.com -s workflow
```

Alternatively, merge in the GitHub web interface. Approval alone is not
a merge; verify the PR's state and obtain its actual merge commit:

```sh
gh pr view <PR> --repo nitros9project/toolshed --json state,mergeCommit
```

## 4. Tag the merged commit on upstream

Confirm the tag does not already exist, fetch the merged commit, and
create an annotated tag on that exact commit, not the fork's PR head:

```sh
git ls-remote --tags upstream v2.7.0
git fetch upstream main
git tag -a v2.7.0 <MERGE_COMMIT> -m "ToolShed v2.7.0"
git push upstream refs/tags/v2.7.0
```

Push only the intended tag, not every local tag. If the release workflow
fails, investigate and rerun the appropriate job; do not move the tag.

## 5. Verify the GitHub release

Pushing `v*` triggers `.github/workflows/release.yml` (**Make Release**).
It builds Linux and Windows packages, publishes the GitHub release, and
attaches `changelog.md`. Do not create a competing release manually while
the workflow is running.

```sh
gh run list --repo nitros9project/toolshed --workflow release.yml --limit 5
gh run watch <RUN> --repo nitros9project/toolshed --exit-status
gh release view v2.7.0 --repo nitros9project/toolshed
```

Expected assets:

- `toolshed-unix64-2.7.0.tgz` (Linux binaries, not a source archive)
- `toolshed-win64-2.7.0.zip`
- `changelog.md`

Download into a fresh directory and inspect the archives. Check that the
core tools, including `dgndos`, and a nonempty HTML manual are present.
Compare downloaded SHA-256 hashes with the asset digests returned by
`gh release view --json assets`.

```sh
gh release download v2.7.0 --repo nitros9project/toolshed \
  --pattern 'toolshed-*' --dir <DOWNLOAD_DIR>
```

From the download directory, create and upload checksums:

```sh
shasum -a 256 toolshed-unix64-2.7.0.tgz toolshed-win64-2.7.0.zip > SHA256SUMS
gh release upload v2.7.0 --repo nitros9project/toolshed SHA256SUMS
```

Set the title to `ToolShed v2.7.0`, add the curated changelog entry ahead
of the generated PR notes, and verify the release is published rather
than a draft or prerelease. Mark it as latest when appropriate. macOS
command-line packages are currently CI artifacts, not assets produced by
the Make Release workflow.

## 6. Update and test Homebrew

Repository: <https://github.com/nitros9project/homebrew-toolshed>

On a feature branch, update **`Formula/Toolshed.rb`** (capital `T`).
The formula builds from source, so use the tagged source archive, never
the `toolshed-unix64-*.tgz` Linux binary package:

```sh
curl --fail --location --retry 3 \
  -o toolshed-v2.7.0.tar.gz \
  https://github.com/nitros9project/toolshed/archive/refs/tags/v2.7.0.tar.gz
shasum -a 256 toolshed-v2.7.0.tar.gz
```

Update `url` and `sha256` together. Keep the `markdown` build dependency.
The formula clears `SOURCE_DATE_EPOCH` before building because bundled
raw-copy tests expect current modification times, while Homebrew normally
sets a fixed timestamp.

Test the candidate formula before merging its PR:

```sh
HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 \
  brew install --build-from-source nitros9project/toolshed/toolshed
brew test nitros9project/toolshed/toolshed
```

Those commands must resolve to the **candidate** formula, not an older
installed tap revision. Recent Homebrew versions reject installation
directly from a local `.rb` path. One way to test a committed candidate
branch without changing an existing tap is:

```sh
brew tap --custom-remote release-test/toolshed <LOCAL_FORMULA_REPO>
brew trust --formula release-test/toolshed/toolshed
HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 \
  brew install --build-from-source release-test/toolshed/toolshed
brew test release-test/toolshed/toolshed
```

The local repository's checked-out branch must contain the committed
candidate. After subsequent commits, fast-forward the test tap before
retesting. Grant trust only to the reviewed formula, not the entire tap.
If ToolShed is already installed, use the appropriate `reinstall` or
`upgrade` operation rather than accepting an "already installed" result
as validation.

Verify the installed `os9` reports `ToolShed 2.7.0`, then merge the
Homebrew PR. Account for any temporary test tap or developer-mode setting
created during validation; do not uninstall or overwrite unrelated tools.

## 7. Verify wiki documentation

The manual belongs at:
<https://github.com/nitros9project/toolshed/wiki>

`.github/workflows/sync-release-to-wiki.yml` listens for a published
release, but do not assume it ran. Releases created with a workflow's
`GITHUB_TOKEN` generally do not trigger other workflows.

Check that wiki `Home.md` matches `doc/ToolShed.md` from the release tag.
If automatic synchronization did not occur, clone
`git@github.com:nitros9project/toolshed.wiki.git`, review the existing
content, copy the tagged manual to `Home.md`, and commit/push the update.
Discover the wiki's default branch instead of assuming it is `main`
(it was `master` for v2.7.0).

## DiskShed releases

DiskShed uses `.github/workflows/diskshed-release.yml` (**Release DiskShed**).
Its tag pattern is `DiskShed-v*`; the parser must strip `DiskShed-v` so
`DiskShed-v1.0.0` produces version `1.0.0`, not `DiskShed-v1.0.0`.

After validating and merging any preparation PR:

```sh
git tag -a DiskShed-v1.0.0 <MERGE_COMMIT> -m "DiskShed v1.0.0"
git push upstream refs/tags/DiskShed-v1.0.0
```

The workflow publishes macOS ARM64/Intel and Windows ZIPs, a Linux
AppImage, and a `.sha256` file for each binary. Verify all four builds,
the checksum verification job, release title, and asset filenames. The
same macOS runner-capacity retry guidance applies. Do not bump the
ToolShed CLI version or Homebrew formula solely for a DiskShed release.

## Final checklist

- [ ] Version strings and dated changelog updated on a feature branch.
- [ ] Forced local rebuild, unit tests, and version check passed.
- [ ] Upstream PR checks passed and PR merged.
- [ ] Annotated tag pushed to upstream on the exact merged commit.
- [ ] Release workflow succeeded; published title, notes, and assets verified.
- [ ] Downloaded package contents and SHA-256 checksums verified.
- [ ] Homebrew candidate installed/tested and formula PR merged.
- [ ] Wiki manual matches the tagged manual.
- [ ] Remaining local changes and temporary test setup reported.
