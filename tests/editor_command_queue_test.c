#include "test_tmp_root.h"
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/Editor/Commands/command_editor.h"
#include "ide/Panes/Editor/Commands/editor_command_payloads.h"
#include "ide/Panes/Editor/Input/input_editor.h"
#include "ide/Panes/Editor/editor_view.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool refuse_node;
static bool refuse_rows;
static void* watched;
void* rows_test_calloc(size_t count, size_t size) {
    return refuse_rows ? NULL : calloc(count, size);
}
void* queue_test_calloc(size_t count, size_t size) {
    return refuse_node ? NULL : calloc(count, size);
}
void queue_test_free(void* pointer) {
    if (watched && pointer == watched) watched = NULL;
    free(pointer);
}

static bool watched_text(UIPane* pane) {
    assert(!watched);
    EditorTextInputPayload* payload = calloc(1, sizeof(*payload));
    assert(payload);
    strcpy(payload->text, "x");
    watched = payload;
    return enqueueCommand((InputCommandMetadata){.cmd=COMMAND_EDITOR_TEXT_INPUT,
                                                .targetPane=pane, .payload=payload});
}

static void type(UIPane* pane, const char* value) {
    SDL_Event event = {.type=SDL_TEXTINPUT};
    SDL_strlcpy(event.text.text, value, sizeof(event.text.text));
    handleEditorTextInput(pane, &event);
}

static void shortcut(UIPane* pane, SDL_Keycode key, SDL_Keymod mod) {
    SDL_Event event = {.type=SDL_KEYDOWN};
    event.key.keysym.sym = key;
    event.key.keysym.mod = mod;
    handleEditorKeyboardInput(pane, &event);
}

static void expect_file(OpenFile* file, const char* text, bool dirty) {
    assert(file->buffer->lineCount == 1 && !strcmp(file->buffer->lines[0], text));
    assert(file->isModified == dirty);
}

int main(void) {
    char a[] = IDE_TEST_TMPDIR "/queue-a-XXXXXX", b[] = IDE_TEST_TMPDIR "/queue-b-XXXXXX";
    int fd = mkstemp(a);
    assert(fd >= 0 && write(fd, "A\n", 2) == 2 && !close(fd));
    fd = mkstemp(b);
    assert(fd >= 0 && write(fd, "B\n", 2) == 2 && !close(fd));
    initCoreState();
    initCommandBus();
    OpenFile* tabs[3] = {0};
    EditorView view = {.type=VIEW_LEAF,.fileCapacity=3,.openFiles=tabs};
    UIPane pane = {.role=PANE_ROLE_EDITOR,.editorView=&view,.handleCommand=handleEditorCommand};
    getCoreState()->persistentEditorView = &view;
    assert(watched_text(&pane));
    OpenFile* first = openFileInView(&view, a);
    tickCommandBus(); assert(!watched);
    expect_file(first, "A", false);
    type(&pane, "u"); type(&pane, "v");
    refuse_node = true;
    assert(!watched_text(&pane) && !watched);
    refuse_node = false;
    tickCommandBus(); tickCommandBus();
    expect_file(first, "uvA", true);
    refuse_rows = true;
    shortcut(&pane, SDLK_MINUS, KMOD_ALT); tickCommandBus();
    expect_file(first, "uvA", true);
    refuse_rows = false;
    shortcut(&pane, SDLK_MINUS, KMOD_ALT); tickCommandBus();
    expect_file(first, "A", false);
    shortcut(&pane, SDLK_BACKSPACE, KMOD_NONE); tickCommandBus();
    expect_file(first, "A", false);
    assert(watched_text(&pane));
    OpenFile* second = openFileInView(&view, b);
    tickCommandBus();
    assert(!watched);
    expect_file(second, "B", false);
    expect_file(first, "A", false);
    openFileInView(&view, a);
    shortcut(&pane, SDLK_EQUALS, KMOD_ALT); tickCommandBus();
    expect_file(first, "uvA", true);
    shortcut(&pane, SDLK_s, KMOD_CTRL); tickCommandBus(); tickCommandBus();
    expect_file(first, "uvA", false);
    reloadOpenFileFromDisk(first);
    shortcut(&pane, SDLK_MINUS, KMOD_ALT); tickCommandBus();
    expect_file(first, "A", true);
    shortcut(&pane, SDLK_EQUALS, KMOD_ALT); tickCommandBus();
    expect_file(first, "uvA", false);
    assert(watched_text(&pane));
    closeTab(&view, view.activeTab);
    first = openFileInView(&view, a);
    tickCommandBus();
    assert(!watched);
    expect_file(first, "uvA", false);
    assert(watched_text(&pane));
    reloadOpenFileFromDisk(first);
    tickCommandBus(); assert(!watched);
    expect_file(first, "uvA", false);
    assert(watched_text(&pane));
    pane.role = PANE_ROLE_MENUBAR;
    tickCommandBus(); assert(!watched);
    pane.role = PANE_ROLE_EDITOR;
    assert(watched_text(&pane));
    pane.handleCommand = NULL;
    tickCommandBus(); assert(!watched);
    pane.handleCommand = handleEditorCommand;
    assert(watched_text(&pane));
    shutdownCommandBus(); assert(!watched && !isCommandBusBusy());
    expect_file(first, "uvA", false);
    closeTab(&view, 1); closeTab(&view, 0);
    shutdownCoreState();
    assert(!unlink(a) && !unlink(b));
    puts("Editor command queue: native text, exact undo/redo and save baseline, OOM, switch, reopen, reload, rejection and shutdown passed.");
    return 0;
}
