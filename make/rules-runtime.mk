run: run-ide-theme

.PHONY: run-ide-theme
run-ide-theme: $(OUT)
	@MallocNanoZone=0 IDE_USE_SHARED_THEME_FONT=1 IDE_USE_SHARED_THEME=1 IDE_USE_SHARED_FONT=1 IDE_THEME_PRESET=ide_gray IDE_FONT_PRESET=ide IDE_TIMER_HUD=0 ./$(OUT)

.PHONY: run-ide-theme-log
run-ide-theme-log: $(OUT)
	@MallocNanoZone=0 IDE_USE_SHARED_THEME_FONT=1 IDE_USE_SHARED_THEME=1 IDE_USE_SHARED_FONT=1 IDE_THEME_PRESET=ide_gray IDE_FONT_PRESET=ide IDE_TIMER_HUD=1 IDE_TIMER_HUD_OVERLAY=0 ./$(OUT)

.PHONY: run-ide-theme-hud
run-ide-theme-hud: $(OUT)
	@MallocNanoZone=0 IDE_USE_SHARED_THEME_FONT=1 IDE_USE_SHARED_THEME=1 IDE_USE_SHARED_FONT=1 IDE_THEME_PRESET=ide_gray IDE_FONT_PRESET=ide IDE_TIMER_HUD=1 IDE_TIMER_HUD_OVERLAY=1 IDE_TIMER_HUD_VISUAL_MODE=hybrid ./$(OUT)

.PHONY: run-ide-theme-nohud
run-ide-theme-nohud: run-ide-theme-log

.PHONY: run-daw-theme
run-daw-theme: $(OUT)
	@IDE_USE_SHARED_THEME_FONT=1 IDE_USE_SHARED_THEME=1 IDE_USE_SHARED_FONT=1 IDE_THEME_PRESET=daw_default IDE_FONT_PRESET=daw_default ./$(OUT)
