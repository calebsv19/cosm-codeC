# Create-only local input for the existing release authentication stage.
RELEASE_PROGRAM_KEY := ide
RELEASE_ROOT ?=
RELEASE_DISPOSABLE_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(RELEASE_PLATFORM)-$(RELEASE_ARCH)-$(RELEASE_CHANNEL)-disposable
RELEASE_ARTIFACT_ZIP = $(RELEASE_ROOT)/$(RELEASE_DISPOSABLE_BASENAME).zip
RELEASE_ARTIFACT_MANIFEST = $(RELEASE_ROOT)/$(RELEASE_DISPOSABLE_BASENAME).manifest.txt

.PHONY: release-artifact-disposable
release-artifact-disposable:
	@set -eu; \
	root="$$(python3 tools/packaging/prepare_release_root.py --output "$(RELEASE_ROOT)")"; \
	$(MAKE) release-package-self-test \
		DIST_DIR="$$root"; \
	archive="$(RELEASE_ARTIFACT_ZIP)"; \
	manifest="$(RELEASE_ARTIFACT_MANIFEST)"; \
	/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$$root/$(PACKAGE_APP_NAME)" "$$archive"; \
	shasum -a 256 "$$archive" > "$$archive.sha256"; \
	{ \
		echo "product=$(RELEASE_PRODUCT_NAME)"; \
		echo "program=$(RELEASE_PROGRAM_KEY)"; \
		echo "version=$(RELEASE_VERSION)"; \
		echo "platform=$(RELEASE_PLATFORM)"; \
		echo "arch=$(RELEASE_ARCH)"; \
		echo "format=zip"; \
		echo "channel=$(RELEASE_CHANNEL)"; \
		echo "bundle_id=$(RELEASE_BUNDLE_ID)"; \
		echo "source_commit=$$(git rev-parse HEAD)"; \
		echo "compiler_source_commit=$(FISICS_SOURCE_HEAD)"; \
		echo "source_tree=$$(git rev-parse HEAD^{tree})"; \
		echo "disposable=1"; \
		echo "signed=0"; \
		echo "release_signed=0"; \
		echo "notarized=0"; \
		echo "codesign_identity=ad-hoc"; \
		echo "installed_app_replacement=0"; \
		echo "app=$(PACKAGE_APP_NAME)"; \
		echo "artifact=$$(basename "$$archive")"; \
		echo "zip=$$(basename "$$archive")"; \
		echo "sha256=$$(cut -d' ' -f1 "$$archive.sha256")"; \
	} > "$$manifest"; \
	echo "Disposable release artifact complete: $$root"

# Preserve the complete package self-test, including Vulkan presentation proof.
.PHONY: release-package-self-test
release-package-self-test:
	@set -eu; \
	mkdir -p "$(BUILD_DIR)/package-self-test"; \
	runtime="$$(mktemp -d "$(BUILD_DIR)/package-self-test/runtime.XXXXXX")"; \
	trap 'rm -rf "$$runtime"' EXIT HUP INT TERM; \
	IDE_RUNTIME_DIR="$$runtime/runtime" IDE_LOG_DIR="$$runtime/logs" \
	$(MAKE) package-desktop-self-test
