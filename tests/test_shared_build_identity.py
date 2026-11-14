import argparse, json, os, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description='Probe real L4 objects only in an owned disposable contained build.')
p.add_argument('--disposable-root',type=Path,required=True)
p.add_argument('--compiler-source',type=Path,required=True)
p.add_argument('--codework-root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();r=a.disposable_root.resolve();s=r/'source';e=a.output.resolve()
if not str(r).startswith('/private/tmp/') or (s/'.git').exists() or not (r/'inputs.json').is_file():
    p.error('requires an existing contained disposable source/build root')
e.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
for name in ['MAKEFLAGS','MFLAGS','MAKEOVERRIDES','MYIDE_PROJECT_ROOT','IDE_DEFAULT_WORKSPACE','IDE_AGENT_SESSION_FILE','XDG_CACHE_HOME','IDE_PROFILE']:env.pop(name,None)
env.update(HOME=str(r/'home'),TMPDIR=str(r/'tmp'),XDG_CACHE_HOME=str(r/'cache'),IDE_PROFILE='test',PYTHONDONTWRITEBYTECODE='1')
base=['make','CODEWORK_ROOT='+str(a.codework_root.resolve()),'FISICS_DIR='+str(a.compiler_source.resolve()),f'TARGET_BUILD_ROOT={r}/build','BUILD_PROFILE=debug','BUILD_TOOLCHAIN=clang']
obj=r/'build/debug/toolchains/clang/app/ide/UI/panel_button_scope.o';archive=r/'build/debug/shared/libcore_time.a';results=[]
def run(name,target,extra=(),success=True,changed=None):
 before=target.stat().st_mtime_ns if target.exists() else None
 with (e/(name+'.log')).open('w') as log:p=subprocess.run(base+list(extra)+[str(target)],cwd=s,env=env,stdout=log,stderr=subprocess.STDOUT)
 after=target.stat().st_mtime_ns if target.exists() else None
 results.append({'case':name,'returncode':p.returncode,'rebuilt':before!=after})
 (e/'identity-results.json').write_text(json.dumps(results,indent=2)+'\n')
 assert (p.returncode==0)==success,(name,p.returncode)
 if changed is not None:assert (before!=after)==changed,(name,before,after)
 print(results[-1],flush=True)
run('identity-object-baseline',obj)
run('identity-object-noop',obj,changed=False)
run('identity-flags',obj,['PROFILE_CFLAGS=-g -O0 -DIDE_L4_PROBE=1'],changed=True)
run('identity-flags-restore',obj,changed=True)
run('identity-compiler',obj,['HOST_CC=clang'],changed=True)
run('identity-compiler-restore',obj,changed=True)
run('identity-archive-baseline',archive)
run('identity-archive-noop',archive,changed=False)
h=s/'third_party/codework_shared/core/core_time/include/core_time.h';os.utime(h,None)
run('identity-shared-header',archive,changed=True)
run('identity-shared-header-noop',archive,changed=False)
f=s/'make/rules-build.mk';old=f.read_bytes()
try:
 f.write_bytes(old+b'\n# disposable L4 rule probe\n');run('identity-rules',archive,changed=True)
finally:f.write_bytes(old)
run('identity-rules-restore',archive,changed=True)
old=h.read_bytes()
try:
 h.write_bytes(old+b'\n/* pin drift */\n');run('identity-pin-refusal',archive,success=False,changed=False)
finally:h.write_bytes(old)
run('identity-pin-retry',archive,changed=True)
held=h.with_suffix('.held');h.rename(held)
try:run('identity-missing-refusal',archive,success=False,changed=False)
finally:held.rename(h)
run('identity-missing-retry',archive)
run('identity-final-noop',archive,changed=False)
