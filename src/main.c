
#include <stdbool.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <unistd.h>


//	GLOBAL HANDLING
#include "GlobalInfo/core_state.h"
#include "GlobalInfo/system_control.h"
#include "GlobalInfo/event_loop.h"


// 	UI STATE
#include "UI/layout.h"

int STARTING_WIDTH = 1600, STARTING_HEIGHT = 860;




int main(int argc, char* argv[]){

	// 	====================================
	//		INITS


	if (!initializeSystem()) {
            return 1;
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
	core->persistentEditorView = createEditorView();  // Only once
	core->activeEditorView = core->persistentEditorView;


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
		
		runFrameLoop(&ctx, now, dt);


        }
	
	shutdownSystem(panes, paneCount);
	return 0;
}

