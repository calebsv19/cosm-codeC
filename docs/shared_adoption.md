# codeC shared build inputs

codeC consumes the immutable shared revision in `make/shared-source-pin.txt`.
`config/shared_adoption.json` records exact selected module bytes and versions,
including transitive headers and the directly compiled Timer HUD, SDL adapters
and kit_pane composition source. The compiler frontend remains a separate input,
verified against `make/fisics-source-pin.txt` and its snapshot manifest.

`make shared-input-check` rejects missing, modified and unexpected selected source
inputs before consumer compilation. Generated module output is neither imported
nor deleted by this check. An alternate SHARED_ROOT must contain identical inputs.
A dependency update requires managed import, refreshed manifests and consumer
qualification; changing the manifest alone is not an accepted shared adoption.

The local build identity binds compiler commands and binaries, effective flags,
architecture, selected module paths and application make rules. Actual shared
compiler reads produce absolute archive dependencies through `tools/shared_compile.py`.
A stale archive rebuilds its complete member set; an unchanged invocation reuses
objects and archives. Compiler frontend caches are separated by host compiler,
architecture, LLVM selection and build profile, while source parity remains checked.

The standard application route uses Clang. `BUILD_TOOLCHAIN=fisics` is a separate
explicit application route; its compiler is not the embedded frontend pin and
requires its own dependency and full consumer qualification. No default compiler
switch is implied by the shared input manifest.

Use the workspace's contained verification helper with the accepted compiler
snapshot explicitly selected. Builds must use a fresh owned source/output root;
module makefiles write intermediate files beneath the selected shared source.
Do not run broad cleanup in retained source or package trees.

At shutdown the consumer waits for submitted Vulkan work before freeing pane and
text-cache resources. Native qualification checks validation output through the
end of teardown; a successful window-stage summary alone is insufficient.
