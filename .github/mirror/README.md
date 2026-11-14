# Safe Genode mirror with Josh filtered history

Upstream: https://codeberg.org/genodelabs/genode.git  
GitHub: https://github.com/QuasarRay/genode

## File ownership and safety

The Dagger + Python workflow polls every five minutes (GitHub schedules can
run late). It uses three Git object trees:

- Previous imported upstream tree: `codeberg/main` before each poll
- Current upstream tree: freshly fetched Codeberg `main`
- Destination tree: the current GitHub `main`

It only updates or deletes a destination file if its Git blob ID **and mode**
still match the previous upstream version. Unrelated destination paths are
never touched. New upstream files are installed only when no destination
file exists at that path. Changes to user-modified paths become **visible
conflicts**, not overwritten data. Git renames are applied as safe deletion
plus addition; path collisions cannot overwrite unrelated subtrees.

All of `.github/` is excluded from upstream writes, so other workflows and
the mirror's own scripts always survive. The original Genode `LICENSE`
(AGPLv3 with linking exception) stays alongside the destination's separately
owned `LICENSE.MD`. The latter does **not** relicense Genode.

New merge commits have previous GitHub `main` as their first parent, so
the pipeline cannot force-replace that branch or drop manual commits.
The exact upstream commit is recorded as the second parent whenever a
safe update commit is made. If upstream history is rewritten, the script
refuses to apply changes to `main`. No-change polls create no commits.

## Reserved ref namespaces

- `codeberg/*` **branches**: exact upstream branch commits (mirror-owned).
- `codeberg/*` **tags**: upstream tags (mirror-owned).
- `josh/*` **branches**: transformed, history-filtered read-only views.

Only these namespaces can be pruned or force-updated. User tags and all
other branches remain untouched. Do not put personal work in reserved refs.

## Josh (Just One Single History)

A separate scheduled workflow publishes version-pinned Josh
`josh-filter` (r26.08.28) projections from the exact `codeberg/main`
history. These are independent history views; none is a main-branch reset:

| Branch | Filter | Contents |
|---|---|---|
| `josh/docs` | `:/doc` | Docs only |
| `josh/base` | `:/repos/base` | Genode base |
| `josh/os` | `:/repos/os` | Genode OS |

Unlike the main mirror, Josh may rewrite commit IDs intentionally to
represent filtered histories. It cannot change your GitHub `main` files.
The Josh worker is daily (03:17 UTC) or manually dispatched, to avoid
requiring a Rust compilation on every five-minute mirror poll.

To customize filters, edit `.github/mirror/josh_views.py` and run the
**Publish Josh filtered history views** workflow. See
https://josh-project.github.io/josh/reference/filters.html

For a local Josh clone (requires the Josh CLI):

```sh
josh clone https://github.com/QuasarRay/genode.git :/repos/base ./genode-base
```

## Safety regression tests

```sh
cd .github/mirror
python -m unittest -v test_sync
```

GitHub Actions runs them before Dagger synchronization. They simulate
collisions, local edits, renames, preserved workflows, custom licenses,
tag and branch ownership, and no-change polls.

Manually trigger either job using the repository's GitHub Actions tab.
