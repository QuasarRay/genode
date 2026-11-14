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


def sync_main(work: Path) -> bool:
    source = "refs/remotes/upstream/main"
    dest = "refs/remotes/github/main"
    if not head_exists(work, source):
        raise RuntimeError("Codeberg main branch is missing; refusing to modify destination")
    if not head_exists(work, dest):
        raise RuntimeError("GitHub main branch is missing; bootstrap workflow first")

    if cmd(work, "merge-base", "--is-ancestor", source, dest, check=False).returncode == 0:
        print("main: already contains latest Codeberg main")
        return False

    # Start from the *exact* upstream tree and explicitly restore only our CI
    # files. Never run workflows supplied by the upstream source repository.
    cmd(work, "switch", "--detach", source)
    cmd(work, "rm", "-r", "-f", "--ignore-unmatch", "--", ".github/workflows")
    for relative in OVERLAY_FILES:
        blob = cmd(work, "show", f"{dest}:{relative}").stdout
        target = work / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(blob)

    cmd(work, "add", "--", ".github/workflows", ".github/mirror")
    if cmd(work, "diff", "--cached", "--quiet", check=False).returncode == 1:
        cmd(work, "commit", "-m", "chore(mirror): retain trusted GitHub automation")

    # Preserve the original upstream commit IDs and *also* preserve all
    # previous GitHub main history. The resulting push is fast-forward only.
    cmd(work, "merge", "-s", "ours", "--no-ff", "--allow-unrelated-histories",
        "-m", "chore(mirror): merge Codeberg main", dest)
    cmd(work, "push", "github", "HEAD:refs/heads/main")
    print("main: synchronized Genode source and preserved automation")
    return True


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
        cmd(work, "fetch", "--prune", "--force", "--tags", "upstream",
            "+refs/heads/*:refs/remotes/upstream/*")
        cmd(work, "fetch", "--no-tags", "github",
            "+refs/heads/main:refs/remotes/github/main")

        sync_main(work)
        # Publish exact upstream commit IDs, not synthetic copies, and delete
        # stale codeberg/* refs in the destination if upstream removes them.
        cmd(work, "push", "--prune", "github",
            "+refs/remotes/upstream/*:refs/heads/codeberg/*")
        # The destination is a dedicated mirror; its tags track Codeberg.
        cmd(work, "push", "--prune", "github", "+refs/tags/*:refs/tags/*")
        print("branches/tags: synchronized")


if __name__ == "__main__":
    mirror()
