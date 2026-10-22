#include "test_tmp_root.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "core/Diagnostics/diagnostics_engine.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
static double now(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9; }
static size_t count(void) { size_t n=0;build_diagnostics_get(&n);return n; }
static void settle(void) { for(int i=0;i<200;++i) {build_diagnostics_poll();diagnostics_poll();usleep(1000);} }
int main(void) {
    char root[]="" IDE_TEST_TMPDIR "/ide-diagnostic-cache-XXXXXX";assert(mkdtemp(root));
    char dir[256],path[256];snprintf(dir,sizeof(dir),"%s/ide_files",root);assert(!mkdir(dir,0700));
    snprintf(path,sizeof(path),"%s/build_output.json",dir);
    build_diagnostics_feed_chunk("fixture.c:3:2: warning: retained\n",34);assert(count()==1);
    build_diagnostics_save(root);build_diagnostics_load(root);settle();assert(count()==1);
    build_diagnostics_load(root);build_diagnostics_clear();settle();assert(count()==0); /* newer build wins */
    assert(!unlink(path));assert(!mkfifo(path,0600));
    double start=now();build_diagnostics_load(root);assert(now()-start<0.05);settle();assert(count()==0);
    assert(!unlink(path));FILE* f=fopen(path,"w");assert(f);fputs("invalid cache",f);fclose(f);
    build_diagnostics_load(root);settle();assert(count()==0);
    assert(!unlink(path));
    snprintf(path,sizeof(path),"%s/analysis_diagnostics.json",dir);
    addDiagnostic("analysis.c",7,2,"retained",DIAG_SEVERITY_ERROR);
    diagnostics_save(root);diagnostics_load(root);settle();assert(getDiagnosticCount()==1);
    diagnostics_load(root);clearDiagnostics();settle();assert(getDiagnosticCount()==0);
    diagnostics_load(root);addDiagnostic("new.c",1,1,"new result",DIAG_SEVERITY_ERROR);
    settle();assert(getDiagnosticCount()==1);
    clearDiagnostics();assert(!unlink(path));assert(!mkfifo(path,0600));
    start=now();diagnostics_load(root);assert(now()-start<0.05);settle();assert(getDiagnosticCount()==0);
    assert(!unlink(path));assert(!rmdir(dir));assert(!rmdir(root));
    puts("Diagnostics cache: async reload, stale build rejection, FIFO and malformed-cache safety passed");
}
