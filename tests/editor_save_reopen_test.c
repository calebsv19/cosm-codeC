#include "test_tmp_root.h"
#include "core/CommandBus/command_bus.h"
#include "core/CommandBus/save_queue.h"
#include "ide/Panes/Editor/editor_view.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    char path[] = IDE_TEST_TMPDIR "/editor-reopen-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0 && write(fd, "old\n", 4) == 4 && !close(fd));
    OpenFile* tabs[2] = {0};
    EditorView view = {.type=VIEW_LEAF, .fileCapacity=2, .openFiles=tabs};
    getCoreState()->persistentEditorView = &view;
    OpenFile* file = openFileInView(&view, path);
    assert(file && !strcmp(file->buffer->lines[0], "old"));
    free(file->buffer->lines[0]);
    file->buffer->lines[0] = strdup("updated");
    markFileAsModified(file);
    enqueueSave(file);
    assert(isSaveQueueBusy() && file->isModified && !file->showSavedTag);
    /* A hard link forbids atomic replacement. The last tab must survive. */
    char alias[sizeof(path)+8];
    snprintf(alias, sizeof(alias), "%s.link", path);
    assert(!link(path, alias));
    closeTab(&view, 0);
    assert(view.fileCount == 1 && file->saveFailed && file->isModified);
    assert(!unlink(alias));
    enqueueSave(file);
    closeTab(&view, 0);
    assert(!view.fileCount && !isSaveQueueBusy());
    file = openFileInView(&view, path);
    assert(file && !strcmp(file->buffer->lines[0], "updated"));
    tickSaveQueue();
    assert(!file->isModified && !file->showSavedTag);
    closeTab(&view, 0);
    assert(!unlink(path));
    puts("Native command, refused last-tab close, retry, close and fresh tab readback passed.");
    return 0;
}
