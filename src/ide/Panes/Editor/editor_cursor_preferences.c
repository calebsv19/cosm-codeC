#include "ide/Panes/Editor/editor_cursor_preferences.h"

enum {
    EDITOR_CURSOR_MIN_BLINK_MS = 80,
    EDITOR_CURSOR_MAX_BLINK_MS = 3000,
    EDITOR_CURSOR_MIN_WIDTH_PX = 1,
    EDITOR_CURSOR_MAX_WIDTH_PX = 8,
    EDITOR_CURSOR_MIN_GOTO_FLASH_MS = 80,
    EDITOR_CURSOR_MAX_GOTO_FLASH_MS = 3000,
    EDITOR_CURSOR_MIN_MARKER_WIDTH_PX = 1,
    EDITOR_CURSOR_MAX_MARKER_WIDTH_PX = 8,
    EDITOR_CURSOR_DEFAULT_VISIBLE_MS = 680,
    EDITOR_CURSOR_DEFAULT_HIDDEN_MS = 530,
    EDITOR_CURSOR_DEFAULT_WIDTH_PX = 1,
    EDITOR_CURSOR_DEFAULT_GOTO_FLASH_MS = 520,
    EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_MARKER_WIDTH_PX = 2,
    EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_MARKER_ALPHA = 92,
    EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_MARKER_FLASH_ALPHA = 168,
    EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_NUMBER_TINT_AMOUNT = 28
};

static EditorCursorPreferences s_editor_cursor_preferences = {
    .blink_enabled = true,
    .visible_ms = EDITOR_CURSOR_DEFAULT_VISIBLE_MS,
    .hidden_ms = EDITOR_CURSOR_DEFAULT_HIDDEN_MS,
    .width_px = EDITOR_CURSOR_DEFAULT_WIDTH_PX,
    .active_line_marker_enabled = true,
    .active_line_number_tint_enabled = true,
    .goto_flash_enabled = true,
    .goto_flash_ms = EDITOR_CURSOR_DEFAULT_GOTO_FLASH_MS,
    .active_line_marker_width_px = EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_MARKER_WIDTH_PX,
    .active_line_marker_alpha = EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_MARKER_ALPHA,
    .active_line_marker_flash_alpha = EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_MARKER_FLASH_ALPHA,
    .active_line_number_tint_amount = EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_NUMBER_TINT_AMOUNT
};

static uint32_t clamp_blink_ms(uint32_t value) {
    if (value < EDITOR_CURSOR_MIN_BLINK_MS) return EDITOR_CURSOR_MIN_BLINK_MS;
    if (value > EDITOR_CURSOR_MAX_BLINK_MS) return EDITOR_CURSOR_MAX_BLINK_MS;
    return value;
}

static int clamp_width_px(int value) {
    if (value < EDITOR_CURSOR_MIN_WIDTH_PX) return EDITOR_CURSOR_MIN_WIDTH_PX;
    if (value > EDITOR_CURSOR_MAX_WIDTH_PX) return EDITOR_CURSOR_MAX_WIDTH_PX;
    return value;
}

static uint32_t clamp_goto_flash_ms(uint32_t value) {
    if (value < EDITOR_CURSOR_MIN_GOTO_FLASH_MS) return EDITOR_CURSOR_MIN_GOTO_FLASH_MS;
    if (value > EDITOR_CURSOR_MAX_GOTO_FLASH_MS) return EDITOR_CURSOR_MAX_GOTO_FLASH_MS;
    return value;
}

static int clamp_marker_width_px(int value) {
    if (value < EDITOR_CURSOR_MIN_MARKER_WIDTH_PX) return EDITOR_CURSOR_MIN_MARKER_WIDTH_PX;
    if (value > EDITOR_CURSOR_MAX_MARKER_WIDTH_PX) return EDITOR_CURSOR_MAX_MARKER_WIDTH_PX;
    return value;
}

static EditorCursorPreferences sanitize_preferences(EditorCursorPreferences prefs) {
    prefs.visible_ms = clamp_blink_ms(prefs.visible_ms);
    prefs.hidden_ms = clamp_blink_ms(prefs.hidden_ms);
    prefs.width_px = clamp_width_px(prefs.width_px);
    prefs.goto_flash_ms = clamp_goto_flash_ms(prefs.goto_flash_ms);
    prefs.active_line_marker_width_px = clamp_marker_width_px(prefs.active_line_marker_width_px);
    return prefs;
}

EditorCursorPreferences editor_cursor_preferences_default(void) {
    EditorCursorPreferences prefs = {
        .blink_enabled = true,
        .visible_ms = EDITOR_CURSOR_DEFAULT_VISIBLE_MS,
        .hidden_ms = EDITOR_CURSOR_DEFAULT_HIDDEN_MS,
        .width_px = EDITOR_CURSOR_DEFAULT_WIDTH_PX,
        .active_line_marker_enabled = true,
        .active_line_number_tint_enabled = true,
        .goto_flash_enabled = true,
        .goto_flash_ms = EDITOR_CURSOR_DEFAULT_GOTO_FLASH_MS,
        .active_line_marker_width_px = EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_MARKER_WIDTH_PX,
        .active_line_marker_alpha = EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_MARKER_ALPHA,
        .active_line_marker_flash_alpha = EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_MARKER_FLASH_ALPHA,
        .active_line_number_tint_amount = EDITOR_CURSOR_DEFAULT_ACTIVE_LINE_NUMBER_TINT_AMOUNT
    };
    return prefs;
}

EditorCursorPreferences editor_cursor_preferences_get(void) {
    return s_editor_cursor_preferences;
}

void editor_cursor_preferences_set(EditorCursorPreferences prefs) {
    s_editor_cursor_preferences = sanitize_preferences(prefs);
}

void editor_cursor_preferences_reset(void) {
    s_editor_cursor_preferences = editor_cursor_preferences_default();
}

uint32_t editor_cursor_blink_period_ms(EditorCursorPreferences prefs) {
    prefs = sanitize_preferences(prefs);
    return prefs.visible_ms + prefs.hidden_ms;
}

uint32_t editor_cursor_blink_phase_ms(EditorCursorPreferences prefs, uint64_t now_ns) {
    uint64_t now_ms = now_ns / 1000000ULL;
    return (uint32_t)(now_ms % (uint64_t)editor_cursor_blink_period_ms(prefs));
}

bool editor_cursor_blink_visible_at_ns(EditorCursorPreferences prefs, uint64_t now_ns) {
    if (!prefs.blink_enabled) return true;
    prefs = sanitize_preferences(prefs);
    return editor_cursor_blink_phase_ms(prefs, now_ns) < prefs.visible_ms;
}

uint32_t editor_cursor_blink_next_edge_delay_ms(EditorCursorPreferences prefs, uint64_t now_ns) {
    if (!prefs.blink_enabled) return 0u;
    prefs = sanitize_preferences(prefs);
    uint32_t phase_ms = editor_cursor_blink_phase_ms(prefs, now_ns);
    uint32_t period_ms = editor_cursor_blink_period_ms(prefs);
    uint32_t edge_ms = (phase_ms < prefs.visible_ms) ? prefs.visible_ms : period_ms;
    uint32_t delay_ms = edge_ms - phase_ms;
    return delay_ms > 0u ? delay_ms : 1u;
}
