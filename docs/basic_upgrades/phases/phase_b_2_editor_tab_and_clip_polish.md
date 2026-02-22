# Phase B.2 Plan: Editor Clip Safety + Tab Strip Viewport

## Goal
Finish Phase B by hardening editor leaf rendering/input so:
- selection highlights never draw outside leaf content bounds.
- tabs never draw outside their editor header bounds.
- overflowing tabs can be horizontally scrolled in-header with mouse wheel.
- tab labels are truncated with ellipsis and active tab labels are allowed a larger max width.
- partial visibility is pixel-accurate (clip/scissor), including partial glyphs at edges.

## Why this works in current renderer
- The IDE already uses `pushClipRect` / `popClipRect` in `render_helpers.c`.
- In Vulkan mode, `SDL_RenderSetClipRect` is mapped to Vulkan clip/scissor via `vk_renderer_compat_sdl.h`.
- Result: draw calls are clipped at pixel level, including partial characters.

## Implementation Steps

1. Content clip for editor buffer rendering
- Add clip rect around `renderEditorBuffer(...)` content area.
- Keep highlights/cursor/text draw unchanged, but constrained by clip.
- Clamp text-selection registration widths to visible content width.

2. Tab strip viewport rendering
- Add per-leaf horizontal tab scroll offset state on `EditorView`.
- Define tab header viewport (reserving close button area).
- Render all tabs through that viewport clip rect.
- Record tab hitboxes using visible intersections only.

3. Tab width + label truncation policy
- Add two max widths:
  - inactive tabs: smaller width cap
  - active tab: larger width cap
- Compute display labels with ellipsis (`...`) to fit each tab's text budget.
- Keep active tab visual emphasis.

4. Header wheel routing for tab scroll
- If mouse wheel is over tab header viewport, scroll tabs horizontally.
- Otherwise retain existing vertical editor scroll behavior.
- Keep priority ordering:
  - split divider drag
  - scrollbar drag
  - tab header interactions
  - content interactions

5. Validation
- Open many tabs in one leaf, verify tabs never render outside header.
- Verify clipped partial tab/text rendering at viewport edge.
- Verify wheel scroll in header moves tabs, while wheel in content scrolls text.
- Verify close button remains clickable and aligned.
- Verify selection highlight remains inside content bounds.

## Acceptance Criteria
- No tab or selection visual bleeds outside its editor leaf bounds.
- Tab overflow is navigable with wheel in the tab strip.
- Long names are ellipsized; active tab shows more text than inactive tabs.
- Existing editor interactions remain stable.
