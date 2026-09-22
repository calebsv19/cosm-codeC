#!/usr/bin/env python3
"""Local codeC MCP adapter over the IDE's authenticated idebridge session."""

import argparse
import asyncio
from pathlib import Path

from mcp.server.fastmcp import FastMCP
from runtime import Runtime


def create_server(runtime: Runtime) -> FastMCP:
    server = FastMCP("codeC", instructions=(
        "Connect to the intended live IDE workspace. Read saved files before "
        "applying a single-file diff; pass its exact SHA-256 and check readback. "
        "The IDE rejects unsaved-buffer conflicts. Keep raw recordings separate."))

    @server.tool()
    async def session_inspect() -> dict:
        """Inspect the connected IDE session and workspace identity."""
        return await asyncio.to_thread(runtime.live)

    @server.tool()
    async def document_read(path: str) -> dict:
        """Read up to 32 KiB of saved UTF-8 source and its SHA-256."""
        return await asyncio.to_thread(runtime.read_document, path)

    @server.tool()
    async def document_open(path: str, line: int = 1, column: int = 1) -> dict:
        """Open a workspace document at a visible line and column in codeC."""
        return await asyncio.to_thread(runtime.open_document, path, line, column)

    @server.tool()
    async def edit_apply(path: str, expected_sha256: str, unified_diff: str) -> dict:
        """Apply one checked diff through the live IDE and read back saved bytes."""
        return await asyncio.to_thread(runtime.apply_diff, path, expected_sha256,
                                       unified_diff)

    @server.tool()
    async def edit_type(path: str, expected_sha256: str, expected_text: str,
                        replacement_text: str, cadence_ms: int = 45) -> dict:
        """Visibly type one checked replacement through the live editor and save it."""
        return await asyncio.to_thread(runtime.type_visible, path, expected_sha256,
                                       expected_text, replacement_text, cadence_ms)

    @server.tool()
    async def diagnostics(maximum: int = 40) -> dict:
        """Read bounded current IDE diagnostics."""
        return await asyncio.to_thread(runtime.diagnostics, maximum)

    @server.tool()
    async def build(profile: str = "debug") -> dict:
        """Run the IDE's configured project build and return bounded results."""
        return await asyncio.to_thread(runtime.build, profile)

    @server.tool()
    async def build_present(expected_output_sha256: str, focus: str = "") -> dict:
        """Show exact selected lines from the last matching build result in the IDE."""
        return await asyncio.to_thread(runtime.build_present, expected_output_sha256, focus)

    @server.tool()
    async def example_run(expected_executable_sha256: str,
                          timeout_seconds: int = 3) -> dict:
        """Run only workspace build/app after an exact SHA-256 check; return bounded output."""
        return await asyncio.to_thread(runtime.example_run,
                                       expected_executable_sha256, timeout_seconds)

    @server.tool()
    async def example_present(expected_executable_sha256: str) -> dict:
        """Show the last matching completed example run in the IDE Run terminal."""
        return await asyncio.to_thread(runtime.example_present,
                                       expected_executable_sha256)

    return server


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idebridge", type=Path, required=True)
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--session-file", type=Path,
                        help="Private Main Edit agent-session.json (live socket and token)")
    args = parser.parse_args()
    create_server(Runtime(args.idebridge, args.project_root,
                          args.session_file)).run(transport="stdio")


if __name__ == "__main__":
    main()
