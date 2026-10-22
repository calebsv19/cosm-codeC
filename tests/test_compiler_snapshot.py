import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('compiler_snapshot', Path(__file__).resolve().parents[1] / 'tools/packaging/verify_compiler_snapshot.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class CompilerSnapshot(unittest.TestCase):
    def test_metadata_free_copy_and_input_drift(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); (root/'src').mkdir(); p=root/'src/compiler.c';p.write_text('pinned input')
            manifest=root/'manifest.json';pin='a'*40
            manifest.write_text(json.dumps({'commit':pin,'files':{'src/compiler.c':{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'mode':p.stat().st_mode&0o777}}}))
            self.assertEqual(module.verify(root,manifest,pin),pin)
            (root/'src/.DS_Store').write_bytes(b'finder metadata')
            self.assertEqual(module.verify(root,manifest,pin),pin)
            p.write_text('mutated')
            with self.assertRaisesRegex(ValueError,'input drift'):module.verify(root,manifest,pin)
            p.write_text('pinned input');(root/'src/injected.c').write_text('extra')
            with self.assertRaisesRegex(ValueError,'input set'):module.verify(root,manifest,pin)
            (root/'src/injected.c').unlink();p.unlink();p.symlink_to(manifest)
            with self.assertRaisesRegex(ValueError,'symlink'):module.verify(root,manifest,pin)
            with self.assertRaisesRegex(ValueError,'revision'):module.verify(root,manifest,'b'*40)

    def test_checkout_identity_cannot_be_forged_by_matching_file_manifest(self):
        import subprocess
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);subprocess.run(['git','init','-q',str(root)],check=True)
            p=root/'Makefile';p.write_text('original')
            subprocess.run(['git','-C',str(root),'add','Makefile'],check=True)
            subprocess.run(['git','-C',str(root),'-c','user.name=Fixture','-c','user.email=fixture@example.invalid','commit','-qm','fixture'],check=True)
            pin=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
            manifest=root/'manifest.json'
            def write():manifest.write_text(json.dumps({'commit':pin,'files':{'Makefile':{'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'mode':p.stat().st_mode&511}}}))
            write();self.assertEqual(module.verify(root,manifest,pin),pin)
            p.write_text('changed');write()
            with self.assertRaisesRegex(ValueError,'committed source'):module.verify(root,manifest,pin)

if __name__=='__main__':unittest.main()
