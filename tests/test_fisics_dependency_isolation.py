"""Exercise dependency guards and output containment using a real Git fixture."""
import pathlib
import subprocess
import tempfile
import unittest

SOURCE = pathlib.Path(__file__).resolve().parents[1]

class DependencyTests(unittest.TestCase):
    def test_dependency_outputs_and_input_drift(self):
        with tempfile.TemporaryDirectory(prefix="ide-dependency-") as temporary:
            root = pathlib.Path(temporary)
            compiler = root / "compiler"
            compiler.mkdir()
            (compiler / "src").mkdir()
            (compiler / "src/input.c").write_text("original")
            (compiler / "Makefile").write_text("frontend:\n\t@mkdir -p \"$(BUILD_DIR)\"\n\t@echo archive > \"$(LIB_FRONTEND)\"\nclean:\n\t@touch forbidden-clean\n")
            def git(*args):
                return subprocess.check_output(["git", "-C", str(compiler), *args], text=True).strip()
            git("init", "-q")
            git("add", ".")
            git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-qm", "fixture")
            head = git("rev-parse", "HEAD")
            module = (SOURCE / "make/rules-fisics-dependency.mk").read_text()
            (root / "Makefile").write_text("FISICS_DIR := " + str(compiler) + "\nBUILD_DIR := build\nSHARED_BUILD_DIR := build/shared\nFISICS_LIB := build/shared/frontend.a\nLLVM_CONFIG := fixture\n" + module + "\nFORCE:\n$(SHARED_BUILD_DIR):\n\t@mkdir -p $@\n")
            def run():
                return subprocess.run(["make", "FISICS_REQUIRED_SOURCE_HEAD=" + head, "build/shared/frontend.a"], cwd=root, capture_output=True, text=True)
            self.assertEqual(run().returncode, 0)
            self.assertTrue((root / "build/shared/frontend.a").exists())
            self.assertFalse((compiler / "build").exists())
            self.assertFalse((compiler / "forbidden-clean").exists())
            (compiler / "tests").mkdir()
            (compiler / "tests/unrelated.c").write_text("unrelated")
            self.assertEqual(run().returncode, 0)
            (compiler / "src/input.c").write_text("changed")
            self.assertNotEqual(run().returncode, 0)
            (compiler / "src/input.c").write_text("original")
            (compiler / "src/new.c").write_text("new")
            self.assertNotEqual(run().returncode, 0)

if __name__ == "__main__":
    unittest.main()
