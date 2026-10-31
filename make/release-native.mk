# Native authentication is phased by the Registry owner. No cleanup or publish.
RELEASE_APP_ZIP = $(RELEASE_ROOT)/codeC-$(RELEASE_VERSION)-macOS-arm64-stable.zip
.PHONY: release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact
release-build: preparation-auth-inputs
	@python3 tools/packaging/prepare_release_root.py --output "$(RELEASE_ROOT)"
	@$(MAKE) PREPARATION_CHILD=1 PREPARATION_AUTH_PHASE=0 PREPARATION_DISPATCH=0 SHARED_ROOT="$(SHARED_ROOT)" DIST_DIR="$(RELEASE_ROOT)" package-desktop-self-test
release-bundle-audit: release-build
	@test "$$('/usr/libexec/PlistBuddy' -c 'Print :CFBundleIdentifier' "$(RELEASE_ROOT)/codeC.app/Contents/Info.plist")" = com.cosm.codec
release-sign: release-bundle-audit
	@python3 tools/packaging/release_native_bundle.py sign --root "$(RELEASE_ROOT)" --identity "$(APPLE_SIGN_IDENTITY)"
release-verify: release-sign
	@codesign --verify --deep --strict "$(RELEASE_ROOT)/codeC.app"
release-verify-signed: release-verify
	@echo "release-verify-signed passed"
release-notarize: release-verify-signed
	@test -n "$(APPLE_NOTARY_PROFILE)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(RELEASE_ROOT)/codeC.app" "$(RELEASE_APP_ZIP)"
	@xcrun notarytool submit "$(RELEASE_APP_ZIP)" --keychain-profile "$(APPLE_NOTARY_PROFILE)" --wait --output-format json > "$(RELEASE_ROOT)/notary_submit.json"
	@python3 -c 'import json; assert json.load(open("$(RELEASE_ROOT)/notary_submit.json"))["status"] == "Accepted"'
release-staple: release-notarize
	@xcrun stapler staple "$(RELEASE_ROOT)/codeC.app"
	@xcrun stapler validate "$(RELEASE_ROOT)/codeC.app"
release-verify-notarized: release-staple
	@codesign --verify --deep --strict "$(RELEASE_ROOT)/codeC.app"
	@xcrun stapler validate "$(RELEASE_ROOT)/codeC.app"
	@spctl --assess --type execute --verbose=2 "$(RELEASE_ROOT)/codeC.app"
release-artifact: release-verify-notarized
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(RELEASE_ROOT)/codeC.app" "$(RELEASE_APP_ZIP)"
	@python3 tools/packaging/release_native_bundle.py manifest --root "$(RELEASE_ROOT)"
