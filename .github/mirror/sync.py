"""Git-native Codeberg -> GitHub mirror, run inside a Dagger container.

The default GitHub `main` branch contains Codeberg's main tree plus the
small .github/mirror automation overlay, so scheduled Actions survive.
Exact upstream branch commits are additionally published as codeberg/*.
"""
from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path
from urllib.parse import urlparse

SOURCE = "https://codeberg.org/genodelabs/genode.git"
DESTINATION = "https://github.com/QuasarRay/genode.git"
OVERLAY_FILES = (
    ".github/workflows/codeberg-mirror.yml",
    ".github/mirror/pipeline.py",
    ".github/mirror/sync.py",
    ".github/mirror/README.md",
    ".github/mirror/test_sync.py",
)


def cmd(work: Path, *args: str, check: bool = True, data: bytes | None = None) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        ["git", *args], cwd=work, input=data, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, check=check,
    )


def head_exists(work: Path, rev: str) -> bool:
    return cmd(work, "rev-parse", "--verify", "--quiet", rev, check=False).returncode == 0


def entries(work: Path, ref: str | None) -> dict[bytes, tuple[bytes, bytes]]:
    """Read Git tree entries without materializing files or following symlinks."""
    if ref is None:
        return {}
    result = {}
    for line in cmd(work, "ls-tree", "-r", "-z", ref).stdout.split(b"\0"):
        if not line:
            continue
        info, path = line.split(b"\t", 1)
        mode, _kind, sha = info.split(b" ")
        result[path] = (mode, sha)
    return result


def protected(path: bytes) -> bool:
    return path == b".github" or path.startswith(b".github/")


def updated_tree(work: Path, destination: str, previous: str | None, upstream: str):
    old = entries(work, previous)
    new = entries(work, upstream)
    local = entries(work, destination)
    result = local.copy()
    candidates = {}
    conflicts = []
    for path in sorted(old.keys() | new.keys()):
        before, after = old.get(path), new.get(path)
        if before == after or protected(path):
            continue
        # Own only the files whose Git blob+mode match the previous upstream.
        if local.get(path) != before:
            conflicts.append((path, "destination differs from prior upstream version"))
            continue
        candidates[path] = after

    for path, after in candidates.items():
        if after is None:
            result.pop(path, None)
    for path, after in candidates.items():
        if after is None:
            continue
        ancestors = [path[:i] for i, ch in enumerate(path) if ch == 47]
        if any(a in result for a in ancestors) or any(p.startswith(path + b"/") for p in result):
            conflicts.append((path, "file/directory collision"))
            continue
        result[path] = after

    if result == local:
        return None, conflicts
    cmd(work, "read-tree", destination)
    changes = []
    for path in sorted(local.keys() | result.keys()):
        if local.get(path) == result.get(path):
            continue
        if path not in result:
            changes.append(b"0 " + b"0" * 40 + b"\t" + path + b"\0")
        else:
            mode, sha = result[path]
            changes.append(mode + b" " + sha + b"\t" + path + b"\0")
    cmd(work, "update-index", "-z", "--index-info", data=b"".join(changes))
    tree = cmd(work, "write-tree").stdout.strip().decode("ascii")
    return tree, conflicts


def sync_main(work: Path) -> list[tuple[bytes, str]]:
    source = "refs/remotes/upstream/main"
    previous = "refs/remotes/github/codeberg/main"
    dest = "refs/remotes/github/main"
    if not head_exists(work, source) or not head_exists(work, dest):
        raise RuntimeError("Missing main branch; refusing modifications")
    if head_exists(work, previous) and cmd(
        work, "merge-base", "--is-ancestor", previous, source, check=False
    ).returncode != 0:
        raise RuntimeError("Upstream history rewritten; refusing to update main")

    tree, conflicts = updated_tree(
        work, dest, previous if head_exists(work, previous) else None, source
    )
    if tree:
        # Main's first parent is its previous HEAD: no force push or tree reset.
        sha = cmd(
            work, "commit-tree", tree, "-p", dest, "-p", source,
            data=b"chore(mirror): apply safe upstream changes\n",
        ).stdout.strip().decode("ascii")
        cmd(work, "push", "github", f"{sha}:refs/heads/main")
        print("main: updated only unmodified upstream-owned paths")
    else:
        print("main: no safe file updates")
    return conflicts


def mirror(source: str = SOURCE, destination: str = DESTINATION) -> None:
    if os.environ.get("GH_TOKEN") and urlparse(destination).hostname != "github.com":
        raise ValueError("A GitHub token may only be sent to github.com")

    with tempfile.TemporaryDirectory(prefix="genode-mirror-") as directory:
        work = Path(directory)
        cmd(work, "init", "--initial-branch=mirror-work")
        cmd(work, "config", "user.name", "genode-mirror[bot]")
        cmd(work, "config", "user.email", "genode-mirror[bot]@users.noreply.github.com")
        cmd(work, "config", "gc.auto", "0")
        if os.environ.get("GH_TOKEN"):
            askpass = work / "askpass.sh"
            askpass.write_text(
                '#!/bin/sh\ncase "$1" in\n'
                '  *Username*) printf "%s\\n" "x-access-token" ;;\n'
                '  *Password*) printf "%s\\n" "$GH_TOKEN" ;;\n'
                '  *) exit 1 ;;\n'
                'esac\n'
            )
            askpass.chmod(0o700)
            os.environ["GIT_ASKPASS"] = str(askpass)
            os.environ["GIT_TERMINAL_PROMPT"] = "0"

        cmd(work, "remote", "add", "upstream", source)
        cmd(work, "remote", "add", "github", destination)
        # All branches and tags, including upstream staging and historical tags.
        cmd(work, "fetch", "--no-tags", "upstream",
            "+refs/heads/*:refs/remotes/upstream/*")
        cmd(work, "fetch", "--no-tags", "github",
            "+refs/heads/main:refs/remotes/github/main",
            "+refs/heads/codeberg/*:refs/remotes/github/codeberg/*")

        conflicts = sync_main(work)
        # Publish exact upstream commit IDs, not synthetic copies, and delete
        # stale codeberg/* refs in the destination if upstream removes them.
        cmd(work, "push", "--prune", "github",
            "+refs/remotes/upstream/*:refs/heads/codeberg/*")
        # Never prune the user's tags; own only the codeberg/ tag namespace.
        cmd(work, "fetch", "--force", "--tags", "upstream")
        cmd(work, "push", "--prune", "github",
            "+refs/tags/*:refs/tags/codeberg/*")
        for path, reason in conflicts:
            print(f"CONFLICT: {os.fsdecode(path)}: {reason}")
        if conflicts:
            raise RuntimeError(f"Refusing to overwrite {len(conflicts)} user-modified paths")
        print("branches/tags: synchronized; destination-only files preserved")


if __name__ == "__main__":
    mirror()
