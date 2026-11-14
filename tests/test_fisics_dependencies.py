#!/usr/bin/env python3
"""Verify dependency publication failures preserve the last usable object pair."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    'fisics_compile', Path(__file__).resolve().parents[1]/'tools/fisics_compile.py')
ADAPTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ADAPTER)


class Dependencies(unittest.TestCase):
    def test_failed_or_missing_graph_preserves_pair_and_retry(self):
        import json
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source, header = root/'source.c', root/'nested header.h'
            source.write_text('int main(void) { return 0; }\n')
            header.write_text('/* included */\n')
            obj, dep = root/'target.o', root/'target.d'
            obj.write_bytes(b'previous object'); dep.write_text('previous dependencies')
            command = ['compiler', '-c', str(source), '-o', str(obj)]

            def compiler(args, mode):
                Path(args[args.index('-o')+1]).write_bytes(b'new object')
                graph = Path(args[args.index('--emit-deps-json')+1])
                if mode == 'malformed': graph.write_text('{}')
                if mode == 'missing-header':
                    graph.write_text(json.dumps({'edges': [{'from': str(source), 'to': str(root/'absent.h')}]}))
                if mode == 'valid':
                    graph.write_text(json.dumps({'edges': [{'from': str(source), 'to': str(header)}]}))
                return 1 if mode == 'compile-failure' else 0

            for mode in ['compile-failure', 'missing', 'malformed', 'missing-header']:
                with self.subTest(mode=mode), patch.object(ADAPTER.subprocess, 'call', lambda args: compiler(args, mode)):
                    if mode == 'compile-failure': self.assertEqual(ADAPTER.compile_command(command), 1)
                    else:
                        with self.assertRaises((OSError, ValueError)): ADAPTER.compile_command(command)
                self.assertEqual(obj.read_bytes(), b'previous object')
                self.assertEqual(dep.read_text(), 'previous dependencies')
                self.assertFalse(list(root.glob('.fisics-*')))
            with patch.object(ADAPTER.subprocess, 'call', lambda args: compiler(args, 'valid')):
                self.assertEqual(ADAPTER.compile_command(command), 0)
            self.assertEqual(obj.read_bytes(), b'new object')
            self.assertIn('nested\\ header.h', dep.read_text())

    def test_make_escaping(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            header = root/'space #$header.h'
            header.write_text('header')
            makefile = root/'Makefile'
            makefile.write_text('all: ' + ADAPTER.make_path(str(header)) + '\n\t@echo accepted\n')
            result = subprocess.run(['make', '-f', str(makefile)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('accepted', result.stdout)
        for path in ['colon:path', 'line\nbreak', 'wild*card', 'target%pattern']:
            with self.assertRaises(ValueError): ADAPTER.make_path(path)

    def test_link_passthrough(self):
        command = ['compiler', '-lSDL2', 'app.o', '-o', 'app']
        with patch.object(ADAPTER.subprocess, 'call', return_value=7) as call:
            self.assertEqual(ADAPTER.compile_command(command), 7)
            call.assert_called_once_with(command)


if __name__ == '__main__':
    unittest.main()
