#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/Editor/editor_view.h"
#include "engine/Render/render_font.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int getUIFontPointSizeByTier(CoreFontTextSizeTier tier) {
    (void)tier;
    return 11;
}

bool isEditorDraggingScrollbar(void) {
    return false;
}

static bool near_float(float actual, float expected) {
    return fabsf(actual - expected) < 0.01f;
}

int main(void) {
    EditorState state;
    resetEditorState(&state);

    const int lineHeight = EDITOR_LINE_HEIGHT;
    const int padding = editor_vertical_padding_px(&state);
    assert(lineHeight == 15);
    assert(padding == 16);

    editorStateFrameLineInUpperBand(&state, 80, 300, 200);
    assert(near_float(state.scrollOffsetPx, 1141.0f));
    assert(near_float(state.scrollTargetPx, state.scrollOffsetPx));
    assert(state.viewTopRow == 75);
    float targetY = (float)padding + (80.0f * (float)lineHeight) - state.scrollOffsetPx;
    assert(near_float(targetY, 75.0f));

    editorStateFrameLineInUpperBand(&state, 2, 300, 200);
    assert(near_float(state.scrollOffsetPx, 0.0f));
    assert(state.viewTopRow == 0);

    editorStateFrameLineInUpperBand(&state, 198, 300, 200);
    float maxOffset = editor_max_scroll_offset_px(&state, 200, 300);
    assert(near_float(state.scrollOffsetPx, maxOffset));
    assert(state.viewTopRow == editor_first_visible_row(&state));

    editorStateFrameLineInUpperBand(&state, 40, 0, 200);
    assert(state.viewTopRow == 36);
    assert(near_float(state.scrollOffsetPx, 540.0f));

    printf("editor_goto_framing_test: ok\n");
    return 0;
}
