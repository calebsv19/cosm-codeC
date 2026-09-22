"""Lesson receipts preserve one applied step and reject drift on retry."""

import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/lesson"))
import lesson_control


class FakeRuntime:
    edit_count = 0

    def __init__(self, bridge, workspace, session_file=None):
        self.workspace = workspace
        self.session_file = session_file

    def read_document(self, relative):
        data = (self.workspace / relative).read_bytes()
        return {"sha256": hashlib.sha256(data).hexdigest()}

    def apply_diff(self, relative, expected, diff):
        FakeRuntime.edit_count += 1
        target = self.workspace / relative
        assert self.read_document(relative)["sha256"] == expected
        target.write_text("int x = 2;\n")
        return {"saved": self.read_document(relative),
                "bridge": {"document_revision_after": 2}}

    def open_document(self, relative, line, column):
        return {"applied": True, "path": relative, "line": line}

    def build(self, profile):
        return {"status": "passed"}

    def live(self):
        return {"session_id": "test-session"}


class LessonControlTests(unittest.TestCase):
    def test_prepare_creates_exact_before_source_once(self):
        with tempfile.TemporaryDirectory(prefix="ide-lesson-prepare-") as temporary:
            workspace = Path(temporary) / "take-1"
            before = b"int x = 1;\n"
            manifest = {"lesson_id": "demo", "steps": [{"id": "s1",
                "path": "src/example.c", "before_text": before.decode(),
                "before_sha256": hashlib.sha256(before).hexdigest()}]}
            result = lesson_control.step_prepare(manifest, "s1", workspace)
            self.assertEqual(result["before_sha256"], hashlib.sha256(before).hexdigest())
            self.assertEqual((workspace / "src/example.c").read_bytes(), before)
            with self.assertRaises(FileExistsError):
                lesson_control.step_prepare(manifest, "s1", workspace)
            manifest["steps"][0]["path"] = "../escape.c"
            with self.assertRaisesRegex(ValueError, "unsafe_lesson_source_path"):
                lesson_control.step_prepare(manifest, "s1", Path(temporary) / "take-2")

    def test_one_step_receipt_is_idempotent_and_drift_rejects(self):
        with tempfile.TemporaryDirectory(prefix="ide-lesson-test-") as temporary:
            workspace = Path(temporary)
            source = workspace / "example.c"
            source.write_text("int x = 1;\n")
            before = hashlib.sha256(source.read_bytes()).hexdigest()
            after = hashlib.sha256(b"int x = 2;\n").hexdigest()
            manifest = {"lesson_id": "demo", "steps": [{"id": "s1", "path": "example.c",
                "before_sha256": before, "after_sha256": after,
                "unified_diff": "--- a/example.c\n+++ b/example.c\n@@ -1 +1 @@\n-int x = 1;\n+int x = 2;\n",
                "intent": "change x", "capture_seconds": 8,
                "present": {"line": 1, "column": 1}, "build": True}]}
            receipts = workspace / "receipts"
            FakeRuntime.edit_count = 0
            with patch.object(lesson_control, "Runtime", FakeRuntime):
                first = lesson_control.step_apply(manifest, "s1", workspace,
                                                  workspace, receipts)
                second = lesson_control.step_apply(manifest, "s1", workspace,
                                                   workspace, receipts)
                self.assertEqual(first, second)
                self.assertEqual(FakeRuntime.edit_count, 1)
                self.assertEqual(first["after_sha256"], after)
                source.write_text("int x = 3;\n")
                with self.assertRaisesRegex(ValueError, "existing_receipt_or_source_changed"):
                    lesson_control.step_apply(manifest, "s1", workspace,
                                              workspace, receipts)


if __name__ == "__main__":
    unittest.main()
