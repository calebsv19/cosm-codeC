#include "test_tmp_root.h"
#include "core/CommandBus/save_queue.h"
#include "core/Analysis/analysis_scheduler.h"
#include "core/Analysis/analysis_status.h"
#include "core/LoopEvents/event_queue.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fault, writes, refreshes;
char projectPath[1024];

ssize_t save_test_write(int fd, const void* bytes, size_t len) {
    if (fault == 1 && writes++ > 0) { errno = ENOSPC; return -1; }
    return write(fd, bytes, fault == 1 ? 1 : len);
}
void* save_test_calloc(size_t count, size_t size) {
    return fault == 3 ? NULL : calloc(count, size);
}
int save_test_close(int fd) {
    int result = close(fd);
    if (fault == 2) { errno = EIO; return -1; }
    return result;
}
void analysis_status_set(AnalysisStatus status) { assert(status == ANALYSIS_STATUS_STALE_LOADING); }
bool loop_events_emit_analysis_status_updated(const char* root, uint64_t run, uint64_t stamp) {
    (void)root; (void)run; (void)stamp; return true;
}
void analysis_scheduler_request_file(const char* path, AnalysisRefreshReason reason, bool force) {
    assert(path && reason == ANALYSIS_REASON_EDITOR_SAVE && !force); refreshes++;
}
void pollGitStatusWatcher(void) {}
static void expect_bytes(const char* path, const char* expected) {
    char data[128];
    FILE* f = fopen(path, "rb"); assert(f);
    size_t n = fread(data, 1, sizeof(data), f);
    assert(!ferror(f) && !fclose(f) && n == strlen(expected) && !memcmp(data, expected, n));
}
int main(void) {
    char root[] = IDE_TEST_TMPDIR "/editor-save-XXXXXX";
    assert(mkdtemp(root));
    char path[sizeof(root)+12]; snprintf(path, sizeof(path), "%s/source.c", root);
    FILE* f = fopen(path, "wb"); assert(f && fputs("old", f) >= 0 && !fclose(f));
    assert(!chmod(path, 0750));
    char* line = "first";
    EditorBuffer buffer = {.lineCount=1, .lines=&line};
    OpenFile doc = {.buffer=&buffer, .filePath=path, .isModified=true};
    initSaveQueue();
    for (fault=1; fault<=2; fault++) {
        assert(queueSaveFromOpenFile(&doc) && !doc.showSavedTag && doc.isModified);
        assert(!finishPendingFileSaves(&doc) && doc.saveFailed && doc.isModified && !doc.showSavedTag);
        assert(!isSaveQueueBusy() && !refreshes);
        expect_bytes(path, "old");
    }
    fault=3;
    assert(!queueSaveFromOpenFile(&doc) && doc.saveFailed && doc.isModified && !isSaveQueueBusy());
    expect_bytes(path, "old");
    fault=0;
    assert(queueSaveFromOpenFile(&doc));
    line="second"; doc.documentRevision++;
    assert(queueSaveFromOpenFile(&doc));
    tickSaveQueue();
    expect_bytes(path, "first\n");
    assert(doc.isModified && !doc.showSavedTag && isSaveQueueBusy());
    assert(finishPendingFileSaves(&doc)); /* final tab close drains its snapshot */
    expect_bytes(path, "second\n");
    assert(!doc.isModified && doc.showSavedTag && !doc.saveFailed && !isSaveQueueBusy());
    OpenFile reopened = {.buffer=&buffer, .filePath=path, .isModified=true};
    tickSaveQueue();
    assert(reopened.isModified && !reopened.showSavedTag && refreshes == 2);
    struct stat st; assert(!stat(path,&st) && (st.st_mode & 0777) == 0750);
    shutdownSaveQueue();
    assert(!unlink(path) && !rmdir(root));
    puts("Editor queue partial write / ENOSPC, close, dirty retry, FIFO, edit and close/reopen passed.");
    return 0;
}
