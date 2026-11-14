"""Publish Josh-filtered history views under josh/*, never touching main.

Requires josh-filter from Josh r26.08.28, installed by the Josh workflow.
All projections read the exact Codeberg source history (codeberg/main).
"""
from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path
from sync import DESTINATION, cmd

VIEWS = {
    "docs": ":/doc",
    "base": ":/repos/base",
    "os": ":/repos/os",
}


def publish_views(destination: str = DESTINATION) -> dict[str, str]:
    if os.environ.get("GH_TOKEN") and destination != DESTINATION:
        raise ValueError("GitHub token cannot be sent to another destination")
    with tempfile.TemporaryDirectory(prefix="genode-josh-") as dirname:
        repo = Path(dirname)
        cmd(repo, "init")
        if os.environ.get("GH_TOKEN"):
            askpass = repo / "askpass.sh"
            askpass.write_text(
                '#!/bin/sh\ncase "$1" in\n'
                ' *Username*) printf "%s\\n" "x-access-token";;\n'
                ' *Password*) printf "%s\\n" "$GH_TOKEN";;\n'
                ' *) exit 1;;\nesac\n'
            )
            askpass.chmod(0o700)
            os.environ["GIT_ASKPASS"] = str(askpass)
            os.environ["GIT_TERMINAL_PROMPT"] = "0"
        cmd(repo, "remote", "add", "github", destination)
        cmd(repo, "fetch", "--no-tags", "github",
            "+refs/heads/codeberg/main:refs/remotes/github/codeberg/main")
        result = {}
        for name, spec in VIEWS.items():
            ref = f"refs/heads/josh/{name}"
            output = subprocess.check_output([
                "josh-filter", spec, "refs/remotes/github/codeberg/main",
                "--update", ref,
            ], cwd=repo, text=True)
            sha = output.strip().splitlines()[-1].strip()
            if len(sha) != 40 or any(char not in "0123456789abcdef" for char in sha):
                raise RuntimeError(f"Josh returned invalid object ID for {name}: {sha}")
            # This job owns only josh/* ref names, never main or user branches.
            cmd(repo, "push", "--force", "github", f"{sha}:{ref}")
            result[name] = sha
            print(f"Josh {name}: {sha}")
        return result


if __name__ == "__main__":
    publish_views()
