.PHONY: package-desktop-main-edit package-desktop-main-edit-self-test package-desktop-main-edit-refresh

package-desktop-main-edit:
	@test -s "$(PACKAGE_APP_ICON_SRC)" || test -d "$(PACKAGE_APP_ICONSET_SRC)" || (echo "Missing Main Edit icon input: $(PACKAGE_APP_ICON_SRC)"; exit 1)
	@python3 tools/packaging/macos/main_edit_package.py build --source-root "$(CURDIR)" --compiler-root "$(FISICS_DIR)"

package-desktop-main-edit-self-test: package-desktop-main-edit
	@test -s "build/main-edit-dist/codeC Main Edit.app/Contents/Resources/$(PACKAGE_APP_ICON_FILE)" || (echo "Missing bundled Main Edit icon"; exit 1)
	@test "$$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIconFile' "build/main-edit-dist/codeC Main Edit.app/Contents/Info.plist")" = "$(PACKAGE_APP_ICON_NAME)" || (echo "Main Edit icon metadata mismatch"; exit 1)
	@if [ -f "$(PACKAGE_APP_ICON_SRC)" ]; then cmp "$(PACKAGE_APP_ICON_SRC)" "build/main-edit-dist/codeC Main Edit.app/Contents/Resources/$(PACKAGE_APP_ICON_FILE)" || exit 1; fi
	@python3 tools/packaging/macos/main_edit_package.py verify --source-root "$(CURDIR)"

package-desktop-main-edit-refresh: package-desktop-main-edit-self-test
	@python3 tools/packaging/macos/main_edit_package.py refresh --source-root "$(CURDIR)"
