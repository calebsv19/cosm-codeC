VULKAN_ROLLOUT_DIR := $(abspath $(BUILD_DIR)/vulkan-rollout)
VULKAN_ROLLOUT_INITIAL_CAPTURE := $(VULKAN_ROLLOUT_DIR)/ide-initial.bmp
VULKAN_ROLLOUT_RESIZED_CAPTURE := $(VULKAN_ROLLOUT_DIR)/ide-resized.bmp
VULKAN_ROLLOUT_LOG := $(VULKAN_ROLLOUT_DIR)/ide-vulkan.log
CANONICAL_SHARED_ROOT ?= ../shared

vulkan-rollout-contract:
	@python3 tools/verify-vulkan-rollout.py --shared-root "$(SHARED_ROOT)" \
		--canonical-shared-root "$(CANONICAL_SHARED_ROOT)"

vulkan-rollout-self-test: $(OUT) vulkan-rollout-contract
	@mkdir -p "$(VULKAN_ROLLOUT_DIR)"
	@python3 tools/verify-vulkan-rollout.py --shared-root "$(SHARED_ROOT)" \
		--canonical-shared-root "$(CANONICAL_SHARED_ROOT)" \
		--app "$(abspath $(OUT))" \
		--initial-capture "$(VULKAN_ROLLOUT_INITIAL_CAPTURE)" \
		--resized-capture "$(VULKAN_ROLLOUT_RESIZED_CAPTURE)" \
		--log "$(VULKAN_ROLLOUT_LOG)" --minimum-scale 1.5
