.PHONY: package-build-lane
package-build-lane:
	@echo "Building package binaries for toolchain $(PACKAGE_TOOLCHAIN)..."
	@$(MAKE) BUILD_PROFILE="$(PACKAGE_BUILD_PROFILE)" FISICS_SANITIZED=0 BUILD_TOOLCHAIN="$(PACKAGE_TOOLCHAIN)" TARGET_OS="$(TARGET_OS)" TARGET_ARCH="$(TARGET_ARCH)" TARGET_VARIANT="$(TARGET_VARIANT)" "$(PACKAGE_BIN)" "$(PACKAGE_IDEBRIDGE_BIN)"

package-desktop: package-build-lane
	@echo "Preparing app bundle layout..."
	@rm -rf $(PACKAGE_APP_DIR)
	@mkdir -p $(PACKAGE_MACOS_DIR) $(PACKAGE_RESOURCES_DIR) $(PACKAGE_FRAMEWORKS_DIR)
	@cp $(PACKAGE_INFO_PLIST_SRC) $(PACKAGE_CONTENTS_DIR)/Info.plist
	@cp $(PACKAGE_BIN) $(PACKAGE_MACOS_DIR)/ide-bin
	@cp $(PACKAGE_IDEBRIDGE_BIN) $(PACKAGE_MACOS_DIR)/idebridge
	@cp $(PACKAGE_LAUNCHER_SRC) $(PACKAGE_MACOS_DIR)/ide-launcher
	@chmod +x $(PACKAGE_MACOS_DIR)/ide-launcher $(PACKAGE_MACOS_DIR)/ide-bin $(PACKAGE_MACOS_DIR)/idebridge
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" $(PACKAGE_DYLIB_BUNDLER) $(PACKAGE_MACOS_DIR)/ide-bin $(PACKAGE_FRAMEWORKS_DIR)
	@PACKAGE_DEP_SEARCH_ROOTS="$(TARGET_DEP_SEARCH_ROOTS)" $(PACKAGE_DYLIB_BUNDLER) $(PACKAGE_MACOS_DIR)/idebridge $(PACKAGE_FRAMEWORKS_DIR)
	@mkdir -p $(PACKAGE_RESOURCES_DIR)/include
	@cp -R include/fonts $(PACKAGE_RESOURCES_DIR)/include/
	@mkdir -p $(PACKAGE_RESOURCES_DIR)/shared/assets
	@cp -R third_party/codework_shared/assets/fonts $(PACKAGE_RESOURCES_DIR)/shared/assets/
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ]; then \
		cp "$(PACKAGE_APP_ICON_SRC)" "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICON_SRC)"; \
	elif [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		iconutil -c icns "$(PACKAGE_APP_ICONSET_SRC)" -o "$(PACKAGE_BUNDLED_ICON_PATH)"; \
		echo "Bundled app icon from $(PACKAGE_APP_ICONSET_SRC)"; \
	else \
		echo "Warning: no app icon input found; continuing without bundled AppIcon.icns"; \
	fi
	@mkdir -p $(PACKAGE_RESOURCES_DIR)/vk_renderer
	@cp -R third_party/codework_shared/vk_renderer/shaders $(PACKAGE_RESOURCES_DIR)/vk_renderer/
	@mkdir -p $(PACKAGE_RESOURCES_DIR)/shaders
	@cp -R third_party/codework_shared/vk_renderer/shaders/. $(PACKAGE_RESOURCES_DIR)/shaders/
	@$(MAKE) package-desktop-sign-adhoc
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-sign-adhoc:
	@echo "Applying ad-hoc signatures to packaged app..."
	@test -d "$(PACKAGE_APP_DIR)" || (echo "Missing app bundle"; exit 1)
	@for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
		[ -f "$$dylib" ] || continue; \
		codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$dylib"; \
	done
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/ide-bin"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/idebridge"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/ide-launcher"
	@codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)"
	@echo "package-desktop-sign-adhoc passed."

package-desktop-copy-desktop: package-desktop
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

# Convenience target for post-edit desktop deployment flow.
package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop app sync complete."

package-desktop-open: package-desktop
	@open $(PACKAGE_APP_DIR)

package-desktop-smoke: package-desktop
	@test -x $(PACKAGE_MACOS_DIR)/ide-launcher || (echo "Missing launcher"; exit 1)
	@test -x $(PACKAGE_MACOS_DIR)/ide-bin || (echo "Missing ide-bin"; exit 1)
	@test -x $(PACKAGE_MACOS_DIR)/idebridge || (echo "Missing idebridge"; exit 1)
	@test -f $(PACKAGE_CONTENTS_DIR)/Info.plist || (echo "Missing Info.plist"; exit 1)
	@test -f $(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib || (echo "Missing bundled libvulkan"; exit 1)
	@test -f $(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib || (echo "Missing bundled libMoltenVK"; exit 1)
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ] || [ -d "$(PACKAGE_APP_ICONSET_SRC)" ]; then \
		test -f "$(PACKAGE_BUNDLED_ICON_PATH)" || (echo "Missing bundled AppIcon.icns"; exit 1); \
	fi
	@test -f $(PACKAGE_RESOURCES_DIR)/include/fonts/Lato/Lato-Regular.ttf || (echo "Missing bundled Lato"; exit 1)
	@test -f $(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv || (echo "Missing bundled shaders"; exit 1)
	@test -f $(PACKAGE_RESOURCES_DIR)/shaders/line.vert.spv || (echo "Missing bundled runtime line shader"; exit 1)
	@test -f $(PACKAGE_RESOURCES_DIR)/shaders/line.frag.spv || (echo "Missing bundled runtime line shader"; exit 1)
	@actual_archs="$$(/usr/bin/lipo -archs "$(PACKAGE_MACOS_DIR)/ide-bin" 2>/dev/null || true)"; \
	printf '%s\n' "$$actual_archs" | /usr/bin/grep -qw "$(TARGET_ARCH)" || (echo "Unexpected ide-bin archs: $$actual_archs"; exit 1)
	@bridge_archs="$$(/usr/bin/lipo -archs "$(PACKAGE_MACOS_DIR)/idebridge" 2>/dev/null || true)"; \
	printf '%s\n' "$$bridge_archs" | /usr/bin/grep -qw "$(TARGET_ARCH)" || (echo "Unexpected idebridge archs: $$bridge_archs"; exit 1)
	@for dylib in "$(PACKAGE_FRAMEWORKS_DIR)"/*.dylib; do \
		[ -f "$$dylib" ] || continue; \
		dylib_archs="$$(/usr/bin/lipo -archs "$$dylib" 2>/dev/null || true)"; \
		printf '%s\n' "$$dylib_archs" | /usr/bin/grep -qw "$(TARGET_ARCH)" || (echo "Unexpected dylib archs for $$dylib: $$dylib_archs"; exit 1); \
	done
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@$(PACKAGE_MACOS_DIR)/ide-launcher --self-test || (echo "package-desktop self-test failed."; exit 1)
	@echo "package-desktop-self-test passed."

test-package-launcher-runtime-dir-hardening:
	@tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/ide_launcher_runtime.XXXXXX")"; \
	mkdir -p "$$tmp/app/Contents/MacOS" "$$tmp/app/Contents/Frameworks" \
		"$$tmp/app/Contents/Resources/include/fonts/Lato" \
		"$$tmp/app/Contents/Resources/shaders" \
		"$$tmp/app/Contents/Resources/vk_renderer/shaders" \
		"$$tmp/tmp"; \
	cp "$(PACKAGE_LAUNCHER_SRC)" "$$tmp/app/Contents/MacOS/ide-launcher"; \
	chmod +x "$$tmp/app/Contents/MacOS/ide-launcher"; \
	touch "$$tmp/app/Contents/MacOS/ide-bin" "$$tmp/app/Contents/MacOS/idebridge" \
		"$$tmp/app/Contents/Frameworks/libvulkan.1.dylib" \
		"$$tmp/app/Contents/Frameworks/libMoltenVK.dylib" \
		"$$tmp/app/Contents/Resources/include/fonts/Lato/Lato-Regular.ttf"; \
	chmod +x "$$tmp/app/Contents/MacOS/ide-bin" "$$tmp/app/Contents/MacOS/idebridge"; \
	for shader in textured.vert.spv textured.frag.spv line.vert.spv line.frag.spv fill.vert.spv fill.frag.spv; do \
		touch "$$tmp/app/Contents/Resources/shaders/$$shader"; \
		touch "$$tmp/app/Contents/Resources/vk_renderer/shaders/$$shader"; \
	done; \
	HOME="$$tmp/home" TMPDIR="$$tmp/tmp" IDE_RUNTIME_DIR="$$tmp/custom-runtime" \
		"$$tmp/app/Contents/MacOS/ide-launcher" --print-config > "$$tmp/safe.out"; \
	grep -F "IDE_RUNTIME_DIR=$$tmp/custom-runtime" "$$tmp/safe.out" >/dev/null; \
	grep -F "IDE_RUNTIME_DIR_SOURCE=env" "$$tmp/safe.out" >/dev/null; \
	test -f "$$tmp/custom-runtime/vk_renderer/shaders/textured.vert.spv"; \
	HOME="$$tmp/home" TMPDIR="$$tmp/tmp" IDE_RUNTIME_DIR="relative-runtime" \
		"$$tmp/app/Contents/MacOS/ide-launcher" --print-config > "$$tmp/relative.out"; \
	grep -F "IDE_RUNTIME_DIR=$$tmp/home/Library/Application Support/IDE/runtime" "$$tmp/relative.out" >/dev/null; \
	grep -F "IDE_RUNTIME_DIR_SOURCE=default_after_invalid_env" "$$tmp/relative.out" >/dev/null; \
	ln -s / "$$tmp/root-link"; \
	HOME="$$tmp/home2" TMPDIR="$$tmp/tmp" IDE_RUNTIME_DIR="$$tmp/root-link" \
		"$$tmp/app/Contents/MacOS/ide-launcher" --print-config > "$$tmp/symlink.out"; \
	grep -F "IDE_RUNTIME_DIR=$$tmp/home2/Library/Application Support/IDE/runtime" "$$tmp/symlink.out" >/dev/null; \
	grep -F "IDE_RUNTIME_DIR_SOURCE=default_after_invalid_env" "$$tmp/symlink.out" >/dev/null; \
	rm -rf "$$tmp"; \
	echo "test-package-launcher-runtime-dir-hardening passed."

package-desktop-remove:
	@rm -rf "$(DESKTOP_APP_DIR)"
	@echo "Removed desktop copy at $(DESKTOP_APP_DIR)"

package-desktop-refresh: package-desktop
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"
