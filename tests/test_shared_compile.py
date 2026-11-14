"""Different consumer outputs must retain independent actual include graphs."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ADAPTER = Path(__file__).resolve().parents[1]/'tools/shared_compile.py'

class SharedCompile(unittest.TestCase):
    def test_profile_graphs_do_not_overwrite_each_other(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder); (root/'build').mkdir()
            (root/'a.h').write_text('#define VALUE 1\n')
            (root/'b.h').write_text('#define VALUE 2\n')
            source=root/'sample.c'
            source.write_text('#if SELECT_A\n#include "a.h"\n#else\n#include "b.h"\n#endif\nint sample(void) { return VALUE; }\n')
            targets=[root/'debug/libsample.a',root/'perf/libsample.a']
            for target,select in zip(targets,[1,0]):
                subprocess.run([sys.executable,str(ADAPTER),'--target',str(target),'--','cc',f'-DSELECT_A={select}','-c',str(source),'-o',str(root/'build/sample.o')],cwd=root,check=True)
            for target,wanted,other in zip(targets,['a.h','b.h'],['b.h','a.h']):
                deps=list(Path(str(target)+'.deps').glob('*.d'))
                self.assertEqual(len(deps),1)
                text=deps[0].read_text()
                self.assertIn(str(target)+':',text)
                self.assertIn(str(root/wanted),text)
                self.assertNotIn(str(root/other),text)

if __name__=='__main__':unittest.main()
