# Route declared Registry producer attempts before legacy build rules load.
ifneq ($(strip $(PREPARATION_ROOT)),)
ifneq ($(PREPARATION_CHILD),1)
PREPARATION_DISPATCH := 1
endif
endif
ifeq ($(PREPARATION_DISPATCH),1)
PREPARATION_AUTH_GOALS := release-bundle-audit release-verify-signed release-notarize release-artifact
ifneq ($(filter $(PREPARATION_AUTH_GOALS),$(MAKECMDGOALS)),)
ifneq ($(filter-out $(PREPARATION_AUTH_GOALS),$(MAKECMDGOALS)),)
$(error Native authentication phases cannot mix with preparation goals)
endif
SHARED_ROOT := $(abspath $(PREPARATION_ROOT))/authentication-shared
PREPARATION_AUTH_PHASE := 1
include make/application.mk
include make/preparation-auth.mk
else
include make/preparation.mk
endif
else
include make/application.mk
endif
