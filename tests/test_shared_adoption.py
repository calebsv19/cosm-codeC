#!/usr/bin/env python3
"""Exact-input refusal checks use private fixtures and preserve generated output."""
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('adoption', Path(__file__).resolve().parents[1]/'tools/verify_shared_adoption.py')
adoption = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adoption)

class AdoptionTests(unittest.TestCase):
    def test_inputs_and_generated_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = root/'include/example.h'
            header.parent.mkdir()
            header.write_text('int example(void);\n')
            original = header.read_bytes()
            manifest = {'modules':['core/example'], 'files':{'core/example/include/example.h':hashlib.sha256(original).hexdigest()}}
            roots = {'core/example':root}
            self.assertEqual(adoption.verify(manifest, roots), [])
            output = root/'build/retained.bin'
            output.parent.mkdir()
            output.write_bytes(b'retained output')
            self.assertEqual(adoption.verify(manifest, roots), [])
            header.write_bytes(original+b'/* drift */')
            self.assertTrue(adoption.verify(manifest, roots))
            header.unlink()
            self.assertTrue(adoption.verify(manifest, roots))
            header.write_bytes(original)
            extra = root/'include/shadow.h'
            extra.write_text('/* unrecorded input */')
            self.assertTrue(adoption.verify(manifest, roots))
            extra.unlink()
            self.assertEqual(adoption.verify(manifest, roots), [])
            self.assertEqual(output.read_bytes(), b'retained output')
            self.assertTrue(adoption.verify(manifest, {}))

if __name__ == '__main__': unittest.main()
