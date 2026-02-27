
#include <stdbool.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


//	GLOBAL HANDLING
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/system_control.h"
#include "app/GlobalInfo/event_loop.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "timer_hud/time_scope.h"


// 	UI STATE
#include "ide/UI/layout.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/Panes/Editor/editor_view.h"

int STARTING_WIDTH = 1600, STARTING_HEIGHT = 860;

static const bool ENABLE_TIMER_HUD = false;

static bool parse_bool_token(const char* value, bool* out) {
	if (!value || !out) return false;
	if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
	    strcasecmp(value, "on") == 0 || strcasecmp(value, "yes") == 0) {
		*out = true;
		return true;
	}
	if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
	    strcasecmp(value, "off") == 0 || strcasecmp(value, "no") == 0) {
		*out = false;
		return true;
	}
	return false;
}

static bool resolve_timer_hud_enabled(int argc, char* argv[]) {
	bool enabled = ENABLE_TIMER_HUD;

	const char* env = getenv("IDE_TIMER_HUD");
	bool parsedEnv = false;
	if (env) {
		parsedEnv = parse_bool_token(env, &enabled);
		if (!parsedEnv) {
			fprintf(stderr,
			        "[TimerHUD] Ignoring IDE_TIMER_HUD='%s' (expected 0/1/true/false/on/off).\n",
			        env);
		}
	}

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--timer-hud") == 0) {
			enabled = true;
		} else if (strcmp(argv[i], "--no-timer-hud") == 0) {
			enabled = false;
		}
	}

	return enabled;
}



int main(int argc, char* argv[]){

	setTimerHudEnabled(resolve_timer_hud_enabled(argc, argv));
	fprintf(stderr,
	        "[TimerHUD] IDE profiling %s (enable with --timer-hud or IDE_TIMER_HUD=1)\n",
	        isTimerHudEnabled() ? "ENABLED" : "DISABLED");

	// 	====================================
	//		INITS


	if (!initializeSystem()) {
            return 1;
        }

    {
        const char* persistedTheme = loadThemePresetPreference();
        if (persistedTheme && persistedTheme[0]) {
            if (ide_shared_theme_set_preset(persistedTheme)) {
                ide_refresh_live_theme();
            }
        }
    }


	UIPane* panes[MAX_PANES];
	int paneCount = 0;

	ResizeZone resizeZones[5]; // max 3 zones
	int resizeZoneCount = 0;


	int lastW = 0, lastH = 0;

 	bool running = true;
        SDL_Event event;

	Uint64 lastTime = SDL_GetPerformanceCounter();
	Uint64 lastRender = lastTime;
	const float targetFrameTime = 1.0f / 60.0f;  // 60 FPS

	
	IDECoreState* core = getCoreState();
	if (!core->persistentEditorView) {
		core->persistentEditorView = createEditorView();  // fallback when no session restored
	}
	if (!core->activeEditorView) {
		core->activeEditorView = (core->persistentEditorView->type == VIEW_LEAF)
			? core->persistentEditorView
			: findNextLeaf(core->persistentEditorView);
		if (core->activeEditorView) {
			setActiveEditorView(core->activeEditorView);
		}
	}


        layout_static_panes(panes, &paneCount);  // initial layout
		

	FrameContext ctx = {
	    .panes = panes,
	    .paneCount = &paneCount,
	    .resizeZones = resizeZones,
	    .resizeZoneCount = &resizeZoneCount,
	    .running = &running,
	    .event = &event,
	    .lastW = &lastW,
	    .lastH = &lastH,
	    .targetFrameTime = targetFrameTime,
	    .lastRender = &lastRender
	};

        


	//              INITS
        //      ====================================
        //              MAIN LOOP


	while (running) {

		// +++++++++++++++++++++++++++++++++++++++++++++++++++++++
		//              UPDATE LOGIC

		Uint64 now = SDL_GetPerformanceCounter();
		float dt = (now - lastTime) / (float)SDL_GetPerformanceFrequency();
		lastTime = now;
		
		if (isTimerHudEnabled()) ts_frame_start();
		runFrameLoop(&ctx, now, dt);
		if (isTimerHudEnabled()) ts_frame_end();


        }
	
	shutdownSystem(panes, paneCount);
	return 0;
}
