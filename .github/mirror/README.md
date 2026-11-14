# Codeberg Genode mirror

Upstream: <https://codeberg.org/genodelabs/genode.git>  
Destination: <https://github.com/QuasarRay/genode>

GitHub Actions runs a **Dagger/Python** mirror on a `*/5 * * * *` schedule,
on demand, and whenever these automation files change. GitHub's scheduled
jobs may be delayed or occasionally skipped: five minutes is the requested
schedule, **not a guaranteed synchronization SLA**.

## Git layout

- `main`: the **complete current upstream main source tree**, with just the
  trusted `.github/mirror/` scripts and the mirror workflow kept alongside.
  This branch preserves upstream history through merge ancestry, but its
  HEAD commit is not byte-identical to the upstream HEAD.
- `codeberg/main`: **exact** upstream main commit ID, with no overlay.
- `codeberg/staging` and other `codeberg/*` branches: exact upstream branch
  tips; removed upstream branches are pruned from this namespace.
- Git tags: upstream Git tags, including annotated tags (not releases or LFS
  objects). Tags deleted upstream are pruned from GitHub.

All changes to upstream main lead to a fast-forward merge commit on GitHub
main, preserving the source's original commits and all previous destination
main commits. A no-change poll does not create a new main commit. This avoids
`git push --mirror` deleting the workflow that drives the schedule.

The Dagger container receives the ephemeral job `GITHUB_TOKEN` using a
Dagger `Secret` and a Git askpass helper. No personal access token is needed.
Only repository `contents: write` is requested. The GitHub workflow directory
is cleared from upstream before our single trusted workflow is restored.

## Tests

Run the Git transport tests using Python's standard library:

```sh
cd .github/mirror
python -m unittest -v test_sync
```

The test suite uses temporary bare local repositories to check first import,
upstream ancestry, idempotence, new commits, and deleted-branch pruning.

## Run manually

Use **Actions → Mirror Genode from Codeberg → Run workflow** in the GitHub
repository. The job also starts after the workflow is first committed.
