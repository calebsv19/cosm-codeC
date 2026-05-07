#pragma once

#include "timer_hud/time_scope.h"

TimerHUDSession* timer_hud_session(void);
void timer_hud_register_backend(void);
void timer_hud_shutdown_session(void);
void timer_hud_bind_renderer(void* renderer);
bool timer_hud_session_supports_runtime_work(void);
