"""Real MCP stdio discovery and bounded dispatch over a fake bridge session."""

import asyncio
from contextlib import asynccontextmanager
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/mcp"))
from runtime import Runtime
from example_run import run_example


@asynccontextmanager
async def client(bridge: Path, workspace: Path):
    params = StdioServerParameters(command=sys.executable,
        args=[str(ROOT / "tools/mcp/server.py"), "--idebridge", str(bridge),
              "--project-root", str(workspace)], env=os.environ.copy())
    async with stdio_client(params) as (reader, writer):
        async with ClientSession(reader, writer) as session:
            await session.initialize()
            yield session


class IDEMCPTests(unittest.IsolatedAsyncioTestCase):
    async def test_private_session_file_pins_live_ide(self):
        with tempfile.TemporaryDirectory(prefix="ide-mcp-session-") as temporary:
            root = Path(temporary)
            workspace = root / "lesson"
            workspace.mkdir()
            bridge = root / "bridge.py"
            bridge.write_text('''#!/usr/bin/env python3
import json, os
assert os.environ["MYIDE_SOCKET"] == "/tmp/ide-test.sock"
assert os.environ["MYIDE_AUTH_TOKEN"] == "private-test-token"
print(json.dumps({"ok":True,"result":{"project_root":os.environ["MYIDE_PROJECT_ROOT"],"session_id":"test-session"}}))
''')
            bridge.chmod(0o755)
            session_file = root / "agent-session.json"
            session = {"schema": "ide_agent_session_v1", "project_root": str(workspace),
                       "socket_path": "/tmp/ide-test.sock", "auth_token": "private-test-token",
                       "session_id": "test-session"}
            session_file.write_text(json.dumps(session))
            session_file.chmod(0o600)
            runtime = Runtime(bridge, workspace, session_file)
            self.assertEqual(runtime.live()["session_id"], "test-session")
            session["session_id"] = "replacement-session"
            session_file.write_text(json.dumps(session))
            with self.assertRaisesRegex(ValueError, "ide_session_changed"):
                runtime.live()
            session_file.chmod(0o644)
            with self.assertRaisesRegex(ValueError, "insecure_ide_session_file"):
                Runtime(bridge, workspace, session_file).live()

    async def test_discovery_read_open_and_conflict(self):
        with tempfile.TemporaryDirectory(prefix="ide-mcp-test-") as temporary:
            root = Path(temporary)
            workspace = root / "lesson"
            workspace.mkdir()
            source = workspace / "example.c"
            source.write_text("int main(void) { return 0; }\n")
            bridge = root / "fake_bridge.py"
            bridge.write_text('''#!/usr/bin/env python3
import json, os, sys
args=sys.argv[1:]
command=args[0]
assert command in ("ping","open","diag","build","edit","type","present","document-inspect","document-save")
if command == "edit":
    from pathlib import Path
    (Path(__file__).parent / "edit_args.json").write_text(json.dumps(args))
if command == "present":
    from pathlib import Path
    (Path(__file__).parent / "present_args.json").write_text(json.dumps(args))
if command == "type":
    from pathlib import Path
    base=Path(__file__).parent
    (base / "type_args.json").write_text(json.dumps(args))
    target=Path(os.environ["MYIDE_PROJECT_ROOT"]) / args[args.index("--path")+1]
    old=args[args.index("--expected_text")+1]
    new=args[args.index("--replacement_text")+1]
    target.write_text(target.read_text().replace(old,new))
if command in ("document-inspect","document-save"):
    from pathlib import Path
    data=(Path(os.environ["MYIDE_PROJECT_ROOT"]) / "example.c").read_bytes()
    h=1469598103934665603
    for byte in data: h=((h ^ byte)*1099511628211)&0xffffffffffffffff
    document={"open":True,"modified":False,"buffer_fnv64":f"{h:016x}",
              "disk_fnv64":f"{h:016x}","buffer_text":data.decode(),"saved":command=="document-save"}
else:
    document={}
result={"document-inspect":document,"document-save":document,"ping":{"project_root":os.environ["MYIDE_PROJECT_ROOT"],"session_id":"test"},
"open":{"applied":True},"diag":{"items":[]},"build":{"ok":False,"status":"failed","output":"units assignment mismatch: lhs is m/s but rhs is s\\n"},
"edit":{"applied":True},"type":{"typed":True,"mutation_steps":8},"present":{"presented":True}}[command]
print(json.dumps({"ok":True,"result":result}))
''')
            bridge.chmod(0o755)
            async with client(bridge, workspace) as session:
                names = {tool.name for tool in (await session.list_tools()).tools}
                self.assertEqual(names, {"session_inspect", "document_read",
                                         "document_open", "edit_apply", "edit_type",
                                         "diagnostics", "build", "build_present", "example_run",
                                         "example_present", "workspace_open", "workspace_refresh",
                                         "document_buffer_inspect", "document_save"})
                absent_build = await session.call_tool("build_present", {
                    "expected_output_sha256": "0" * 64})
                self.assertTrue(absent_build.isError)
                absent = await session.call_tool("example_present", {
                    "expected_executable_sha256": "0" * 64})
                self.assertTrue(absent.isError)
                result = await session.call_tool("document_read", {"path": "example.c"})
                self.assertFalse(result.isError)
                read = json.loads(result.content[0].text)
                self.assertEqual(read["sha256"], hashlib.sha256(source.read_bytes()).hexdigest())
                inspected = await session.call_tool("document_buffer_inspect", {"path":"example.c"})
                self.assertFalse(inspected.isError)
                buffer = json.loads(inspected.content[0].text)
                saved = await session.call_tool("document_save", {"path":"example.c",
                    "expected_buffer_fnv64":buffer["buffer_fnv64"],
                    "expected_disk_fnv64":buffer["disk_fnv64"]})
                self.assertFalse(saved.isError)
                opened = await session.call_tool("document_open", {"path": "example.c", "line": 1})
                self.assertFalse(opened.isError)
                conflict = await session.call_tool("edit_apply", {
                    "path": "example.c", "expected_sha256": "0" * 64,
                    "unified_diff": "--- a/example.c\n+++ b/example.c\n@@ -1 +1 @@\n-int main(void) { return 0; }\n+int main(void) { return 1; }\n"})
                self.assertTrue(conflict.isError)
                self.assertEqual(source.read_text(), "int main(void) { return 0; }\n")
                accepted = await session.call_tool("edit_apply", {
                    "path": "example.c", "expected_sha256": read["sha256"],
                    "unified_diff": "--- a/example.c\n+++ b/example.c\n@@ -1 +1 @@\n-int main(void) { return 0; }\n+int main(void) { return 1; }\n"})
                self.assertFalse(accepted.isError)
                bridge_args = json.loads((root / "edit_args.json").read_text())
                self.assertIn("--expected_fnv64", bridge_args)
                self.assertEqual(len(bridge_args[bridge_args.index("--expected_fnv64") + 1]), 16)
                typed_read = await session.call_tool("document_read", {"path": "example.c"})
                typed_state = json.loads(typed_read.content[0].text)
                typed = await session.call_tool("edit_type", {
                    "path": "example.c", "expected_sha256": typed_state["sha256"],
                    "expected_text": "return 0", "replacement_text": "return 7",
                    "cadence_ms": 20})
                self.assertFalse(typed.isError)
                self.assertIn("return 7", source.read_text())
                type_args = json.loads((root / "type_args.json").read_text())
                self.assertIn("--expected_after_fnv64", type_args)
                self.assertIn("--cadence_ms", type_args)
                built = await session.call_tool("build", {"profile": "debug"})
                self.assertFalse(built.isError)
                build_result = json.loads(built.content[0].text)
                self.assertFalse(build_result["ok"])
                rejected_build = await session.call_tool("build_present", {
                    "expected_output_sha256": "0" * 64})
                self.assertTrue(rejected_build.isError)
                shown_build = await session.call_tool("build_present", {
                    "expected_output_sha256": build_result["output_sha256"],
                    "focus": "units assignment mismatch"})
                self.assertFalse(shown_build.isError)
                build_present_args = json.loads((root / "present_args.json").read_text())
                self.assertIn("units assignment mismatch", build_present_args[build_present_args.index("--text") + 1])
                bad_focus = await session.call_tool("build_present", {
                    "expected_output_sha256": build_result["output_sha256"],
                    "focus": "imaginary diagnostic"})
                self.assertTrue(bad_focus.isError)
                (workspace / "build").mkdir()
                executable = workspace / "build/app"
                executable.write_text("#!/bin/sh\nprintf 'decimal result: 2.5\\n'\n")
                executable.chmod(0o755)
                executable_sha = hashlib.sha256(executable.read_bytes()).hexdigest()
                run = await session.call_tool("example_run", {
                    "expected_executable_sha256": executable_sha})
                self.assertFalse(run.isError)
                shown = await session.call_tool("example_present", {
                    "expected_executable_sha256": executable_sha})
                self.assertFalse(shown.isError)
                self.assertTrue(json.loads(shown.content[0].text)["presented"])
                present_args = json.loads((root / "present_args.json").read_text())
                self.assertIn("decimal result: 2.5", present_args[present_args.index("--text") + 1])

    async def test_example_run_boundaries(self):
        with tempfile.TemporaryDirectory(prefix="ide-example-test-") as temporary:
            root = Path(temporary)
            (root / "build").mkdir()
            executable = root / "build/app"
            executable.write_text("#!/bin/sh\nprintf 'decimal result: 2.5\\n'\n")
            executable.chmod(0o755)
            digest = hashlib.sha256(executable.read_bytes()).hexdigest()
            self.assertEqual(run_example(root, digest)["output"], "decimal result: 2.5\n")
            executable.write_text("#!/bin/sh\nread value\nprintf 'received: %s\\n' \"$value\"\n")
            input_digest = hashlib.sha256(executable.read_bytes()).hexdigest()
            self.assertEqual(run_example(root, input_digest, stdin_text="20\n")["output"], "received: 20\n")
            self.assertEqual(run_example(root, input_digest)["output"], "received: \n")
            with self.assertRaisesRegex(ValueError, "example_input_out_of_range"):
                run_example(root, input_digest, stdin_text="x" * 4097)
            with self.assertRaisesRegex(ValueError, "example_input_out_of_range"):
                run_example(root, input_digest, stdin_text="x\0")
            executable.write_text("#!/bin/sh\nprintf 'decimal result: 2.5\\n'\n")
            with self.assertRaisesRegex(ValueError, "example_executable_changed"):
                run_example(root, "0" * 64)
            executable.write_text("#!/bin/sh\nsleep 5\n")
            with self.assertRaisesRegex(ValueError, "example_executable_changed"):
                run_example(root, digest)
            sleep_digest = hashlib.sha256(executable.read_bytes()).hexdigest()
            self.assertEqual(run_example(root, sleep_digest, 1)["status"], "timed_out")
            executable.write_text("#!/bin/sh\nwhile :; do printf 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'; done\n")
            loud_digest = hashlib.sha256(executable.read_bytes()).hexdigest()
            self.assertEqual(run_example(root, loud_digest)["status"], "output_limit_exceeded")
            executable.unlink()
            executable.symlink_to("/bin/sh")
            with self.assertRaisesRegex(ValueError, "example_must_be_regular_executable"):
                run_example(root, digest)


if __name__ == "__main__":
    unittest.main()
