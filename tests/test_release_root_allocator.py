"""Prove fresh output allocation and preservation of occupied destinations."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("release_root", Path(__file__).resolve().parents[1] / "tools/packaging/prepare_release_root.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DisposableRoots(unittest.TestCase):
    def test_fresh_source_and_data_targets_then_reject_reuse(self):
        with tempfile.TemporaryDirectory() as temp:
            source, data = Path(temp).resolve() / "source", Path(temp).resolve() / "data"
            source.mkdir()
            for path in ("build/release-authenticated/job001", str(data / "ide/build/release-authenticated/job002/targets/macos-arm64")):
                root = MODULE.prepare_root(path, source, data)
                (root / "sentinel").write_text("retain")
                with self.assertRaisesRegex(ValueError, "absent"):
                    MODULE.prepare_root(path, source, data)
                self.assertEqual((root / "sentinel").read_text(), "retain")

    def test_unsafe_paths_and_symlink_ancestors(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp).resolve()
            for path in ("", "/tmp/outside", "../escape", "build/release", "build/release-authenticated/a", "build/release-authenticated/../escape", "build/release-authenticated/job001/extra"):
                with self.subTest(path=path), self.assertRaises(ValueError):
                    MODULE.prepare_root(path, source, source / "data")
            (source / "build").symlink_to(source / "elsewhere", target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "symlink"):
                MODULE.prepare_root("build/release-authenticated/job001", source, source / "data")


if __name__ == "__main__":
    unittest.main()
