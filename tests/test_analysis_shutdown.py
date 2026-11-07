"""Link the real analysis lifecycle to contained app objects; no source-root outputs."""
import argparse,os,shlex,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--build-root',required=True);p.add_argument('--compiler-source',required=True);p.add_argument('--output',required=True);p.add_argument('--baseline',action='store_true');p.add_argument('--sanitize',action='store_true');p.add_argument('--terminal-pressure',action='store_true');p.add_argument('--result-acceptance',action='store_true');p.add_argument('--ipc-ownership',action='store_true');p.add_argument('--native-acceptance',action='store_true');a=p.parse_args();source=Path(a.source);out=Path(a.output);out.mkdir()
args=['make','-n','-B','CODEWORK_ROOT=/Users/calebsv/Desktop/CodeWork','FISICS_DIR='+a.compiler_source,'TARGET_BUILD_ROOT='+a.build_root,'BUILD_PROFILE=debug','BUILD_TOOLCHAIN=clang','all']
lines=subprocess.check_output(args,cwd=source,text=True).splitlines()
unit='src/core/Ipc/ide_ipc_server.c' if a.ipc_ownership else 'src/app/GlobalInfo/event_loop.c' if a.result_acceptance or a.native_acceptance else 'src/core/Analysis/analysis_job.c'
compile=shlex.split(next(x for x in lines if ' -c '+unit+' ' in x).split('||')[0])
compile[compile.index(unit)]=str(ROOT/'tests'/('l3_ipc_server_shutdown.c' if a.ipc_ownership else 'l3_native_acceptance.c' if a.native_acceptance else 'l3_result_acceptance.c' if a.result_acceptance else ('analysis_terminal_pressure_test.c' if a.terminal_pressure else 'analysis_job_shutdown_test.c')));obj=out/'probe.o';compile[compile.index('-o')+1]=str(obj)
selected=(source if a.baseline else ROOT)/unit;compile+=['-D'+('IPC_SERVER_SOURCE' if a.ipc_ownership else 'EVENT_LOOP_SOURCE' if a.result_acceptance or a.native_acceptance else 'ANALYSIS_JOB_SOURCE')+'="'+str(selected)+'"']
if a.sanitize:compile+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
subprocess.run(compile,cwd=source,check=True)
line=next(x for x in lines if ' -o ' in x and ' -c ' not in x and '/core/Analysis/analysis_job.o' in x and '||' in x)
link=shlex.split(line.split('||')[0]);link=[v for v in link if not v.endswith('/main.o') and not v.endswith('/'+unit[4:-2]+'.o')]
link[link.index('-o')+1]=str(out/'probe');link.append(str(obj));subprocess.run(link,cwd=source,check=True)
r=subprocess.run([str(out/'probe')],cwd=(source if a.native_acceptance else out),timeout=(60 if a.native_acceptance else 15));print('analysis shutdown exit',r.returncode,flush=True)
if not a.baseline:assert r.returncode==0
