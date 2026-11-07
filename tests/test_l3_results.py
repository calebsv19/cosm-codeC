"""Contained normal/sanitized queue pressure and real cache-reader barriers."""
from pathlib import Path
import argparse,subprocess,shlex
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--baseline-source');a=p.parse_args();out=Path(a.output);out.mkdir();source=Path(a.baseline_source) if a.baseline_source else ROOT
flags=['-std=c11','-D_DARWIN_C_SOURCE','-Wall','-Wextra','-Werror','-Wno-unused-function','-I'+str(source/'src'),'-I'+str(source/'include'),'-I'+str(source/'third_party/codework_shared/core/core_queue/include'),*shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl2','json-c'],text=True))]
flags += ['-I'+str(p) for p in (source/'third_party/codework_shared/core').glob('*/include')]
compiler=Path('/Users/calebsv/Desktop/CodeWork/release_dependencies/ide_fisics/cd1673aa05d8a1daf54bbfec37ac440ad7cb7be3');flags+=['-I'+str(compiler/'src'),'-pthread']
for mode in (('baseline',) if a.baseline_source else ('normal','sanitize')):
 extra=['-DBASELINE'] if a.baseline_source else (['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if mode=='sanitize' else [])
 for family,relative in [('queue','src/core/LoopResults/completed_results_queue.c'),('analysis','src/core/Diagnostics/diagnostics_engine.c'),('build','src/core/BuildSystem/build_diagnostics.c')]:
  binary=out/(mode+'-'+family);test='l3_completion_pressure.c' if family=='queue' else 'l3_cache_shutdown.c';define='QUEUE_SOURCE' if family=='queue' else 'CACHE_SOURCE';args=['clang',*flags,*extra,'-D'+define+'="'+str(source/relative)+'"',str(ROOT/'tests'/test),'-o',str(binary)]
  if family=='queue':args += [str(source/'third_party/codework_shared/core/core_queue/src/core_queue.c')]
  if family=='build':args += ['-DBUILD_CACHE']
  subprocess.run(args,check=True);fixture=out/(mode+'-'+family+'-data');fixture.mkdir();(fixture/'ide_files').mkdir();(fixture/'ide_files/analysis_diagnostics.json').write_text('{}');(fixture/'ide_files/build_output.json').write_text('{}')
  r=subprocess.run([str(binary),str(fixture)],timeout=10);print(mode,family,r.returncode,flush=True)
  if not a.baseline_source:assert r.returncode==0
