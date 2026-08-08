#include "ide/Panes/Editor/editor_cursor_preferences.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    editor_cursor_preferences_reset();

    EditorCursorPreferences defaults = editor_cursor_preferences_get();
    assert(defaults.blink_enabled);
    assert(defaults.visible_ms == 680u);
    assert(defaults.hidden_ms == 530u);
    assert(defaults.width_px == 1);
    assert(defaults.active_line_marker_enabled);
    assert(defaults.active_line_number_tint_enabled);
    assert(defaults.goto_flash_enabled);
    assert(defaults.goto_flash_ms == 520u);
    assert(defaults.active_line_marker_width_px == 2);
    assert(defaults.active_line_marker_alpha == 92u);
    assert(defaults.active_line_marker_flash_alpha == 168u);
    assert(defaults.active_line_number_tint_amount == 28u);

    EditorCursorPreferences prefs = defaults;
    prefs.visible_ms = 1u;
    prefs.hidden_ms = 5000u;
    prefs.width_px = 0;
    prefs.goto_flash_ms = 1u;
    prefs.active_line_marker_width_px = 99;
    editor_cursor_preferences_set(prefs);

    EditorCursorPreferences clamped = editor_cursor_preferences_get();
    assert(clamped.visible_ms == 80u);
    assert(clamped.hidden_ms == 3000u);
    assert(clamped.width_px == 1);
    assert(clamped.goto_flash_ms == 80u);
    assert(clamped.active_line_marker_width_px == 8);

    prefs = defaults;
    prefs.width_px = 99;
    prefs.goto_flash_ms = 9000u;
    prefs.active_line_marker_width_px = -5;
    editor_cursor_preferences_set(prefs);

    clamped = editor_cursor_preferences_get();
    assert(clamped.width_px == 8);
    assert(clamped.goto_flash_ms == 3000u);
    assert(clamped.active_line_marker_width_px == 1);

    assert(editor_cursor_blink_visible_at_ns(defaults, 0));
    assert(editor_cursor_blink_visible_at_ns(defaults, 679000000ULL));
    assert(!editor_cursor_blink_visible_at_ns(defaults, 680000000ULL));

    defaults.blink_enabled = false;
    assert(editor_cursor_blink_visible_at_ns(defaults, 680000000ULL));
    assert(editor_cursor_blink_next_edge_delay_ms(defaults, 680000000ULL) == 0u);

    printf("editor_cursor_preferences_test: ok\n");
    return 0;
}
