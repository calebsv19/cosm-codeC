.PHONY: package-desktop-main-edit package-desktop-main-edit-self-test package-desktop-main-edit-refresh

package-desktop-main-edit:
	@python3 tools/packaging/macos/main_edit_package.py build --source-root "$(CURDIR)" --compiler-root "$(FISICS_DIR)"

package-desktop-main-edit-self-test: package-desktop-main-edit
	@python3 tools/packaging/macos/main_edit_package.py verify --source-root "$(CURDIR)"

package-desktop-main-edit-refresh: package-desktop-main-edit-self-test
	@python3 tools/packaging/macos/main_edit_package.py refresh --source-root "$(CURDIR)"
