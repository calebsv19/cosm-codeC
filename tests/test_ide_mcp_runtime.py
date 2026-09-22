"""Transport-independent checks for the codeC MCP runtime."""

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/mcp"))
from runtime import Runtime


class RuntimeTypingTests(unittest.TestCase):
    def test_checked_visible_typing_and_readback(self):
        with tempfile.TemporaryDirectory(prefix="ide-mcp-runtime-") as temporary:
            root = Path(temporary)
            workspace = root / "lesson"
            workspace.mkdir()
            source = workspace / "lesson.c"
            source.write_text("/* TYPE HERE */\n")
            bridge = root / "bridge.py"
            bridge.write_text('''#!/usr/bin/env python3
import json, os, pathlib, sys
args=sys.argv[1:]
command=args[0]
if command == "ping":
    result={"project_root":os.environ["MYIDE_PROJECT_ROOT"],"session_id":"test"}
elif command == "type":
    root=pathlib.Path(os.environ["MYIDE_PROJECT_ROOT"])
    path=root / args[args.index("--path")+1]
    expected=args[args.index("--expected_text")+1]
    replacement=args[args.index("--replacement_text")+1]
    path.write_text(path.read_text().replace(expected,replacement))
    result={"typed":True,"mutation_steps":len(replacement),"saved_readback":True}
else:
    raise SystemExit(2)
print(json.dumps({"ok":True,"result":result}))
''')
            bridge.chmod(0o755)
            runtime = Runtime(bridge, workspace)
            before = source.read_bytes()
            result = runtime.type_visible(
                "lesson.c", hashlib.sha256(before).hexdigest(),
                "/* TYPE HERE */", "int main(void) { return 0; }", 20)
            self.assertTrue(result["bridge"]["typed"])
            self.assertEqual(source.read_text(), "int main(void) { return 0; }\n")
            self.assertFalse(result["intermediate_frames_captured"])

    def test_typing_requires_unique_checked_ascii_range(self):
        with tempfile.TemporaryDirectory(prefix="ide-mcp-runtime-") as temporary:
            root = Path(temporary)
            source = root / "lesson.c"
            source.write_text("TODO TODO\n")
            bridge = root / "bridge.py"
            bridge.write_text("#!/bin/sh\nexit 2\n")
            bridge.chmod(0o755)
            runtime = Runtime(bridge, root)
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            with self.assertRaisesRegex(ValueError, "must_be_unique"):
                runtime.type_visible("lesson.c", digest, "TODO", "done")
            with self.assertRaisesRegex(ValueError, "bounded_ascii"):
                runtime.type_visible("lesson.c", digest, "TODO TODO", "snowman: \u2603")


if __name__ == "__main__":
    unittest.main()
