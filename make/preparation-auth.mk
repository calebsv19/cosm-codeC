# Initial native build creates writable shared inputs; the owner retains them.
.PHONY: preparation-auth-inputs
preparation-auth-inputs:
	@test "$(abspath $(TARGET_BUILD_ROOT))" = "$(abspath $(PREPARATION_ROOT))"
	@python3 tools/packaging/prepare_shared_copy.py --source third_party/codework_shared --output "$(SHARED_ROOT)"
