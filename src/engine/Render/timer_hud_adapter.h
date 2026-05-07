#pragma once

#include "timer_hud/time_scope.h"

#if defined(__has_include)
#if __has_include("timer_hud/timer_hud_config.h")
#include "timer_hud/timer_hud_config.h"
#else
#define IDE_TIMER_HUD_LEGACY_SESSION_COMPAT 1
#endif
#else
#define IDE_TIMER_HUD_LEGACY_SESSION_COMPAT 1
#endif

#if IDE_TIMER_HUD_LEGACY_SESSION_COMPAT
typedef struct TimerHUDSession TimerHUDSession;

typedef enum TimerHUDVisualMode {
    TIMER_HUD_VISUAL_MODE_TEXT_COMPACT = 0,
    TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH,
    TIMER_HUD_VISUAL_MODE_HYBRID
} TimerHUDVisualMode;

typedef struct TimerHUDInitConfig {
    const char* program_name;
    const char* output_root;
    const char* settings_path;
    const char* default_settings_path;
    bool seed_settings_if_missing;
} TimerHUDInitConfig;

TimerHUDSession* ts_session_create(void);
void ts_session_destroy(TimerHUDSession* session);
void ts_session_register_backend(TimerHUDSession* session, const TimerHUDBackend* backend);
bool ts_session_apply_init_config(TimerHUDSession* session, const TimerHUDInitConfig* config);
void ts_session_init(TimerHUDSession* session);
void ts_session_shutdown(TimerHUDSession* session);
void ts_session_start_timer(TimerHUDSession* session, const char* name);
void ts_session_stop_timer(TimerHUDSession* session, const char* name);
void ts_session_frame_start(TimerHUDSession* session);
void ts_session_frame_end(TimerHUDSession* session);
void ts_session_render(TimerHUDSession* session);
void ts_session_set_hud_enabled(TimerHUDSession* session, bool enabled);
bool ts_session_is_hud_enabled(const TimerHUDSession* session);
bool ts_session_is_log_enabled(const TimerHUDSession* session);
bool ts_session_is_event_tagging_enabled(const TimerHUDSession* session);
TimerHUDVisualMode ts_visual_mode_from_string(const char* mode);
bool ts_session_set_hud_visual_mode_kind(TimerHUDSession* session, TimerHUDVisualMode mode);
const char* ts_session_get_hud_visual_mode(const TimerHUDSession* session);
const char* ts_session_get_log_filepath(const TimerHUDSession* session);
#endif

TimerHUDSession* timer_hud_session(void);
void timer_hud_register_backend(void);
void timer_hud_shutdown_session(void);
void timer_hud_bind_renderer(void* renderer);
bool timer_hud_session_supports_runtime_work(void);
