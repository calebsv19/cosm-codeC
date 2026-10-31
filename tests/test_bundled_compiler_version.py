"""Real recursive Make must not stamp the IDE version into its pinned compiler."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[1]


class BundledCompilerVersion(unittest.TestCase):
    def test_parent_release_version_does_not_relabel_frontend_or_cli(self):
        with tempfile.TemporaryDirectory(prefix='codec-compiler-version-') as folder:
            root = Path(folder)
            compiler = root / 'compiler'
            (compiler / 'src').mkdir(parents=True)
            (compiler / 'VERSION').write_text('0.7.0\n')
            (compiler / 'src/frontend.c').write_text('const char *frontend_version(void) { return FISICS_VERSION; }\n')
            (compiler / 'src/main.c').write_text('#include <stdio.h>\nconst char *frontend_version(void);\nint main(void) { printf("main=%s frontend=%s\\n", FISICS_VERSION, frontend_version()); }\n')
            (compiler / 'Makefile').write_text('''RELEASE_VERSION ?= $(shell cat VERSION)
frontend:
	@mkdir -p "$(BUILD_DIR)"
	@$(CC) -DFISICS_VERSION='"$(RELEASE_VERSION)"' -c src/frontend.c -o "$(BUILD_DIR)/frontend.o"
	@ar rcs "$(LIB_FRONTEND)" "$(BUILD_DIR)/frontend.o"
$(BIN): frontend
	@$(CC) -DFISICS_VERSION='"$(RELEASE_VERSION)"' src/main.c "$(LIB_FRONTEND)" -o "$(BIN)"
''')
            subprocess.run(['git', 'init', '-q', str(compiler)], check=True)
            subprocess.run(['git', '-C', str(compiler), 'add', '.'], check=True)
            subprocess.run(['git', '-C', str(compiler), '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'fixture'], check=True)
            pin = subprocess.check_output(['git', '-C', str(compiler), 'rev-parse', 'HEAD'], text=True).strip()
            (root / 'make').mkdir()
            (root / 'make/fisics-source-pin.txt').write_text(pin + '\n')
            files = {str(p.relative_to(compiler)): {'sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'mode': p.stat().st_mode & 511} for p in [compiler/'Makefile', compiler/'VERSION', *sorted((compiler/'src').glob('*.c'))]}
            (root / 'make/fisics-snapshot-inputs.json').write_text(json.dumps({'commit': pin, 'files': files}))
            helper = root / 'tools/packaging'
            helper.mkdir(parents=True)
            (helper / 'verify_compiler_snapshot.py').write_bytes((SOURCE/'tools/packaging/verify_compiler_snapshot.py').read_bytes())
            module = (SOURCE/'make/rules-fisics-dependency.mk').read_text()
            (root / 'Makefile').write_text(f'FISICS_DIR := {compiler}\nHOST_CC := cc\nBUILD_DIR ?= build\nSHARED_BUILD_DIR := $(BUILD_DIR)/shared\nFISICS_LIB := $(SHARED_BUILD_DIR)/frontend.a\nLLVM_CONFIG := fixture\n' + module + '\nFORCE:\n$(SHARED_BUILD_DIR):\n\t@mkdir -p "$@"\n')
            for ordinal, parent in enumerate(['0.6.0', '9.9.9']):
                with self.subTest(parent_release_version=parent):
                    build = root / f'build-{ordinal}'
                    result = subprocess.run(['make', f'RELEASE_VERSION={parent}', f'BUILD_DIR={build}', 'fisics-package-compiler'], cwd=root, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    binary = build / 'fisics-dependency' / pin / '0.7.0' / 'fisics'
                    self.assertEqual(subprocess.check_output([str(binary)], text=True).strip(), 'main=0.7.0 frontend=0.7.0')
                    self.assertEqual(subprocess.check_output(['git', '-C', str(compiler), 'status', '--porcelain'], text=True).strip(), '')
                    self.assertFalse((compiler/'build').exists())


if __name__ == '__main__':
    unittest.main()
