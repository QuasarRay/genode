"""Dagger Python SDK orchestration; run via `dagger run python ...`."""
from __future__ import annotations

import asyncio
import os

import dagger
from dagger import dag


async def main() -> None:
    token = os.environ["GITHUB_TOKEN"]
    # The engine handles the token as a Secret, not as a repository URL,
    # shell argument, Dagger cache key or tracked file.
    async with dagger.connection():
        result = (
            dag.container()
            .from_("alpine:3.21")
            .with_exec(["apk", "add", "--no-cache", "git", "python3", "ca-certificates"])
            .with_file("/opt/mirror/sync.py", dag.host().file(".github/mirror/sync.py"))
            .with_secret_variable("GH_TOKEN", dag.set_secret("github-token", token))
            .with_exec(["python3", "/opt/mirror/sync.py"])
        )
        print(await result.stdout())


if __name__ == "__main__":
    asyncio.run(main())
