"""Functional tests of the Git transport, independent of Dagger/cloud services."""
from __future__ import annotations

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from sync import mirror


def git(cwd: Path, *args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=cwd, text=True).strip()


def commit(repo: Path, file: str, content: str, message: str) -> str:
    path = repo / file
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    git(repo, "add", "-A")
    git(repo, "commit", "-m", message)
    return git(repo, "rev-parse", "HEAD")


class MirrorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = Path(self.tmp.name)
        self.upstream = root / "upstream.git"
        self.destination = root / "destination.git"
        self.source = root / "source"
        self.admin = root / "admin"
        git(root, "init", "--bare", str(self.upstream))
        git(root, "init", "--bare", str(self.destination))
        git(root, "init", "-b", "main", str(self.source))
        git(root, "init", "-b", "main", str(self.admin))
        for repo in (self.source, self.admin):
            git(repo, "config", "user.name", "Test")
            git(repo, "config", "user.email", "test@example.com")
        self.initial = commit(self.source, "src/core.cc", "int x = 1;\n", "initial upstream")
        git(self.source, "remote", "add", "origin", str(self.upstream))
        git(self.source, "push", "origin", "main")
        git(self.source, "branch", "staging")
        git(self.source, "push", "origin", "staging")
        git(self.source, "tag", "v1")
        git(self.source, "push", "origin", "v1")

        self.workflow = "name: mirror\n"
        self.program = "# pipeline\n"
        self.sync = "# git sync\n"
        commit(self.admin, ".github/workflows/codeberg-mirror.yml", self.workflow, "bootstrap")
        commit(self.admin, ".github/mirror/pipeline.py", self.program, "python pipeline")
        commit(self.admin, ".github/mirror/sync.py", self.sync, "sync implementation")
        commit(self.admin, ".github/mirror/README.md", "README\n", "readme")
        commit(self.admin, ".github/mirror/test_sync.py", "# tests\n", "tests")
        git(self.admin, "remote", "add", "origin", str(self.destination))
        git(self.admin, "push", "origin", "main")
        self.root = root

    def invoke(self) -> None:
        mirror(self.upstream.as_uri(), self.destination.as_uri())

    def test_source_on_default_branch_and_exact_upstream_refs(self):
        self.invoke()
        git(self.admin, "fetch", "origin", "main", "refs/heads/codeberg/main:refs/remotes/origin/codeberg-main", "refs/heads/codeberg/staging:refs/remotes/origin/codeberg-staging", "refs/tags/v1:refs/tags/v1")
        self.assertEqual(git(self.admin, "show", "origin/main:src/core.cc"), "int x = 1;")
        self.assertEqual(git(self.admin, "show", "origin/main:.github/workflows/codeberg-mirror.yml"), self.workflow.strip())
        self.assertEqual(git(self.admin, "rev-parse", "refs/remotes/origin/codeberg-main"), self.initial)
        self.assertEqual(git(self.admin, "rev-parse", "refs/remotes/origin/codeberg-staging"), self.initial)
        self.assertEqual(git(self.admin, "rev-parse", "v1"), self.initial)
        self.assertEqual(git(self.admin, "merge-base", "--is-ancestor", self.initial, "origin/main"), "")

    def test_idempotence_and_new_upstream_commit(self):
        self.invoke()
        before = git(self.admin, "ls-remote", "origin", "refs/heads/main").split()[0]
        self.invoke()
        self.assertEqual(git(self.admin, "ls-remote", "origin", "refs/heads/main").split()[0], before)
        latest = commit(self.source, "src/new.cc", "int y = 2;\n", "new upstream")
        git(self.source, "push", "origin", "main")
        self.invoke()
        after = git(self.admin, "ls-remote", "origin", "refs/heads/main").split()[0]
        self.assertNotEqual(before, after)
        git(self.admin, "fetch", "origin", "main")
        self.assertEqual(git(self.admin, "show", "origin/main:src/new.cc"), "int y = 2;")
        self.assertEqual(git(self.admin, "merge-base", "--is-ancestor", before, after), "")
        self.assertEqual(git(self.admin, "merge-base", "--is-ancestor", latest, after), "")

    def test_removed_upstream_branches_pruned(self):
        self.invoke()
        git(self.source, "push", "origin", ":staging")
        self.invoke()
        out = git(self.admin, "ls-remote", "--heads", "origin", "refs/heads/codeberg/staging")
        self.assertEqual(out, "")


if __name__ == "__main__":
    unittest.main()
