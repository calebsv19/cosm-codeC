#ifndef IDE_WORKSPACE_AUTHORING_OVERLAY_H
#define IDE_WORKSPACE_AUTHORING_OVERLAY_H

struct IDECoreState;
struct UIPane;

void ide_workspace_authoring_overlay_render(struct IDECoreState *core,
                                            struct UIPane **panes,
                                            int pane_count,
                                            int viewport_width,
                                            int viewport_height);

#endif
