#ifndef EDITOR_CURSOR_PREFERENCES_H
#define EDITOR_CURSOR_PREFERENCES_H

#include <stdbool.h>
#include <stdint.h>

typedef struct EditorCursorPreferences {
    bool blink_enabled;
    uint32_t visible_ms;
    uint32_t hidden_ms;
    int width_px;
    bool active_line_marker_enabled;
    bool active_line_number_tint_enabled;
    bool goto_flash_enabled;
    uint32_t goto_flash_ms;
    int active_line_marker_width_px;
    uint8_t active_line_marker_alpha;
    uint8_t active_line_marker_flash_alpha;
    uint8_t active_line_number_tint_amount;
} EditorCursorPreferences;

EditorCursorPreferences editor_cursor_preferences_default(void);
EditorCursorPreferences editor_cursor_preferences_get(void);
void editor_cursor_preferences_set(EditorCursorPreferences prefs);
void editor_cursor_preferences_reset(void);

uint32_t editor_cursor_blink_period_ms(EditorCursorPreferences prefs);
uint32_t editor_cursor_blink_phase_ms(EditorCursorPreferences prefs, uint64_t now_ns);
bool editor_cursor_blink_visible_at_ns(EditorCursorPreferences prefs, uint64_t now_ns);
uint32_t editor_cursor_blink_next_edge_delay_ms(EditorCursorPreferences prefs, uint64_t now_ns);

#endif
