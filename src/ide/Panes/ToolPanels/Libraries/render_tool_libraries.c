#include "render_tool_libraries.h"

#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"

#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_scheduler.h"
#include "core/Analysis/include_graph.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/row_surface.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>

static const char* bucket_label(LibraryBucketKind kind) {
    switch (kind) {
        case LIB_BUCKET_PROJECT:    return "Project headers";
        case LIB_BUCKET_SYSTEM:     return "System headers";
        case LIB_BUCKET_EXTERNAL:   return "External headers";
        case LIB_BUCKET_UNRESOLVED: return "Unresolved headers";
        default:                    return "Headers";
    }
}

static TTF_Font* library_row_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    return font ? font : getActiveFont();
}

static SDL_Color library_row_color(const LibraryFlatRow* row) {
    IDEThemePalette palette = {0};
    SDL_Color primary = {230, 230, 230, 255};
    SDL_Color muted = {170, 170, 170, 255};
    if (ide_shared_theme_resolve_palette(&palette)) {
        primary = palette.text_primary;
        muted = palette.text_muted;
    }
    if (!row) return primary;
    if (row->type == LIB_NODE_USAGE) return muted;
    return primary;
}

static UIRowSurfaceRenderSpec library_row_surface_spec(bool is_selected,
                                                       bool is_primary_selected,
                                                       bool hovered) {
    UIRowSurfaceRenderSpec spec = {0};
    spec.draw_selection_fill = is_selected;
    spec.draw_hover_outline = hovered;
    spec.draw_selection_outline = is_primary_selected && !hovered;
    return spec;
}

static const KitGraphStructNodeLayout* graph_layout_by_id(const KitGraphStructNodeLayout* layouts,
                                                          int count,
                                                          uint32_t node_id) {
    if (!layouts || node_id == 0) return NULL;
    for (int i = 0; i < count; ++i) {
        if (layouts[i].node_id == node_id) return &layouts[i];
    }
    return NULL;
}

static int graph_node_outgoing_count(const LibraryPanelState* st, uint32_t node_id) {
    if (!st || node_id == 0) return 0;
    int count = 0;
    for (int i = 0; i < st->graphEdgeCount; ++i) {
        if (st->graphEdges[i].from_id == node_id) count++;
    }
    return count;
}

static int graph_node_incoming_count(const LibraryPanelState* st, uint32_t node_id) {
    if (!st || node_id == 0) return 0;
    int count = 0;
    for (int i = 0; i < st->graphEdgeCount; ++i) {
        if (st->graphEdges[i].to_id == node_id) count++;
    }
    return count;
}

static int graph_source_order_for_id(const LibraryPanelState* st, uint32_t node_id) {
    if (!st || node_id == 0) return 0;
    int order = 0;
    for (int i = 0; i < st->graphNodeCount; ++i) {
        if (st->graphNodes[i].type != LIB_NODE_DEP_SOURCE) continue;
        if (graph_node_outgoing_count(st, st->graphNodes[i].id) <= 0) continue;
        if (st->graphNodes[i].id == node_id) return order;
        order++;
    }
    return order;
}

static float graph_dep_average_source_order(const LibraryPanelState* st, uint32_t dep_id) {
    if (!st || dep_id == 0) return 0.0f;
    float sum = 0.0f;
    int count = 0;
    for (int i = 0; i < st->graphEdgeCount; ++i) {
        if (st->graphEdges[i].to_id != dep_id) continue;
        sum += (float)graph_source_order_for_id(st, st->graphEdges[i].from_id);
        count++;
    }
    return count > 0 ? sum / (float)count : 0.0f;
}

static const char* graph_node_short_name(const LibraryGraphNodeInfo* node) {
    const char* label = (node && node->label) ? node->label : "(node)";
    const char* slash = strrchr(label, '/');
    return slash && slash[1] ? slash + 1 : label;
}

static LibraryGraphNodeInfo* graph_node_info_by_id(LibraryPanelState* st, uint32_t node_id) {
    if (!st || !st->graphNodes || node_id == 0) return NULL;
    for (int i = 0; i < st->graphNodeCount; ++i) {
        if (st->graphNodes[i].id == node_id) return &st->graphNodes[i];
    }
    return NULL;
}

static const char* graph_node_role_label(const LibraryGraphNodeInfo* node) {
    if (!node) return "Node";
    return node->type == LIB_NODE_DEP_SOURCE ? "Source" : "Dependency";
}

static void draw_graph_hud_line(SDL_Renderer* renderer,
                                SDL_Rect* rect,
                                int* y,
                                const char* textLine,
                                TTF_Font* rowFont,
                                SDL_Color color) {
    (void)renderer;
    if (!rect || !y || !textLine) return;
    int textHeight = rowFont ? TTF_FontHeight(rowFont) : LIBRARY_ROW_HEIGHT;
    if (textHeight < 1) textHeight = LIBRARY_ROW_HEIGHT;
    drawTextUTF8WithFontColorClipped(rect->x, *y, textLine, rowFont, color, false, rect);
    *y += textHeight + 2;
}

static void render_libraries_graph_hud(SDL_Renderer* renderer,
                                       LibraryPanelState* st,
                                       SDL_Rect clip,
                                       TTF_Font* rowFont,
                                       SDL_Color text,
                                       SDL_Color muted,
                                       SDL_Color fill,
                                       SDL_Color outline) {
    if (!renderer || !st) return;

    LibraryGraphNodeInfo* detailNode =
        graph_node_info_by_id(st, st->hoveredGraphNodeId ? st->hoveredGraphNodeId : st->selectedGraphNodeId);
    int textHeight = rowFont ? TTF_FontHeight(rowFont) : LIBRARY_ROW_HEIGHT;
    if (textHeight < 1) textHeight = LIBRARY_ROW_HEIGHT;

    int hudW = clip.w - 20;
    if (hudW > 430) hudW = 430;
    if (hudW < 120) hudW = 120;
    int lineCount = detailNode ? 4 : 2;
    int hudH = 12 + lineCount * (textHeight + 2);
    SDL_Rect hud = {clip.x + 10, clip.y + 10, hudW, hudH};
    if (hud.y + hud.h > clip.y + clip.h - 8) {
        hud.y = clip.y + clip.h - hud.h - 8;
    }
    if (hud.y < clip.y + 8) hud.y = clip.y + 8;

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 232);
    SDL_RenderFillRect(renderer, &hud);
    SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, 210);
    SDL_RenderDrawRect(renderer, &hud);

    SDL_Rect textRect = {hud.x + 8, hud.y + 6, hud.w - 16, hud.h - 12};
    int y = textRect.y;
    char line[512];
    if (clip.w < 520 && st->hiddenGraphNodeCount > 0) {
        snprintf(line,
                 sizeof(line),
                 "Graph %d n / %d e / %d quiet",
                 st->graphNodeCount,
                 st->graphEdgeCount,
                 st->hiddenGraphNodeCount);
    } else if (st->hiddenGraphNodeCount > 0) {
        snprintf(line,
                 sizeof(line),
                 "Graph %d nodes / %d edges / %d quiet",
                 st->graphNodeCount,
                 st->graphEdgeCount,
                 st->hiddenGraphNodeCount);
    } else {
        snprintf(line,
                 sizeof(line),
                 "Graph %d nodes / %d edges",
                 st->graphNodeCount,
                 st->graphEdgeCount);
    }
    draw_graph_hud_line(renderer, &textRect, &y, line, rowFont, muted);

    snprintf(line, sizeof(line), "Zoom %.0f%%", st->graphCamera.zoom * 100.0f);
    draw_graph_hud_line(renderer, &textRect, &y, line, rowFont, muted);

    if (detailNode) {
        int outgoing = graph_node_outgoing_count(st, detailNode->id);
        int incoming = graph_node_incoming_count(st, detailNode->id);
        snprintf(line,
                 sizeof(line),
                 "%s: %s",
                 graph_node_role_label(detailNode),
                 graph_node_short_name(detailNode));
        draw_graph_hud_line(renderer, &textRect, &y, line, rowFont, text);

        snprintf(line,
                 sizeof(line),
                 "Deps %d / Used by %d",
                 outgoing,
                 incoming);
        draw_graph_hud_line(renderer, &textRect, &y, line, rowFont, muted);
    }
}

static void compute_compact_libraries_graph_layout(LibraryPanelState* st, SDL_Rect clip) {
    if (!st || !st->graphLayouts || st->graphNodeCount <= 0) return;

    const float zoom = st->graphCamera.zoom > 0.0f ? st->graphCamera.zoom : 1.0f;
    const float content_pad = 34.0f;
    const float content_lane_gap = 430.0f;
    const float content_node_h = 20.0f;
    const float content_node_w = 34.0f;
    int source_count = 0;
    int dep_count = 0;

    for (int i = 0; i < st->graphNodeCount; ++i) {
        st->graphLayouts[i] = (KitGraphStructNodeLayout){
            .node_id = st->graphNodes[i].id,
            .rect = {
                .x = (float)clip.x - 10000.0f,
                .y = (float)clip.y - 10000.0f,
                .width = 1.0f,
                .height = 1.0f
            },
            .depth = 0
        };
    }

    for (int i = 0; i < st->graphNodeCount; ++i) {
        if (st->graphNodes[i].type == LIB_NODE_DEP_SOURCE) {
            source_count++;
        } else if (st->graphNodes[i].type == LIB_NODE_DEP_TARGET) {
            dep_count++;
        }
    }

    int max_rows = source_count > dep_count ? source_count : dep_count;
    if (max_rows < 1) return;

    float content_h = ((float)clip.h / zoom) - (content_pad * 2.0f);
    float min_content_h = (float)max_rows * (content_node_h + 18.0f);
    if (content_h < min_content_h) content_h = min_content_h;
    if (content_h < 1.0f) content_h = 1.0f;

    float source_x = (float)clip.x + st->graphCamera.pan_x + (content_pad * zoom);
    float dep_x = (float)clip.x + st->graphCamera.pan_x + ((content_pad + content_lane_gap) * zoom);
    float node_w = content_node_w * zoom;
    float node_h = content_node_h * zoom;
    if (node_w < 4.0f) node_w = 4.0f;
    if (node_h < 3.0f) node_h = 3.0f;

    float base_y = (float)clip.y + st->graphCamera.pan_y + (content_pad * zoom);
    float lane_h = content_h * zoom;
    int source_order = 0;
    for (int i = 0; i < st->graphNodeCount; ++i) {
        LibraryGraphNodeInfo* source = &st->graphNodes[i];
        if (source->type != LIB_NODE_DEP_SOURCE) continue;

        float t = source_count > 1 ? (float)source_order / (float)(source_count - 1) : 0.5f;
        float y = base_y + (t * lane_h);
        st->graphLayouts[i] = (KitGraphStructNodeLayout){
            .node_id = source->id,
            .rect = {
                .x = source_x,
                .y = y,
                .width = node_w,
                .height = node_h
            },
            .depth = 0
        };
        source_order++;
    }

    if (dep_count <= 0) return;

    int* dep_indices = malloc((size_t)dep_count * sizeof(int));
    if (!dep_indices) return;
    int fill = 0;
    for (int i = 0; i < st->graphNodeCount; ++i) {
        if (st->graphNodes[i].type == LIB_NODE_DEP_TARGET) {
            dep_indices[fill++] = i;
        }
    }
    for (int i = 1; i < dep_count; ++i) {
        int key = dep_indices[i];
        float key_rank = graph_dep_average_source_order(st, st->graphNodes[key].id);
        int j = i - 1;
        while (j >= 0) {
            float prev_rank = graph_dep_average_source_order(st, st->graphNodes[dep_indices[j]].id);
            const char* prev_label = st->graphNodes[dep_indices[j]].label ? st->graphNodes[dep_indices[j]].label : "";
            const char* key_label = st->graphNodes[key].label ? st->graphNodes[key].label : "";
            if (prev_rank < key_rank) break;
            if (prev_rank == key_rank && strcmp(prev_label, key_label) <= 0) break;
            dep_indices[j + 1] = dep_indices[j];
            j--;
        }
        dep_indices[j + 1] = key;
    }
    for (int i = 0; i < dep_count; ++i) {
        int dep_idx = dep_indices[i];
        float t = dep_count > 1 ? (float)i / (float)(dep_count - 1) : 0.5f;
        float y = base_y + (t * lane_h);
        st->graphLayouts[dep_idx] = (KitGraphStructNodeLayout){
            .node_id = st->graphNodes[dep_idx].id,
            .rect = {
                .x = dep_x,
                .y = y,
                .width = node_w,
                .height = node_h
            },
            .depth = 1
        };
    }
    free(dep_indices);
}

static void render_libraries_graph(SDL_Renderer* renderer,
                                   LibraryPanelState* st,
                                   SDL_Rect clip,
                                   TTF_Font* rowFont) {
    if (!renderer || !st) return;

    IDEThemePalette palette = {0};
    SDL_Color text = {230, 230, 230, 255};
    SDL_Color muted = {150, 150, 150, 255};
    SDL_Color sourceFill = {52, 74, 92, 255};
    SDL_Color depFill = {58, 61, 67, 255};
    SDL_Color outline = {108, 125, 145, 255};
    SDL_Color selected = {74, 145, 220, 255};
    SDL_Color hovered = {185, 205, 225, 255};
    if (ide_shared_theme_resolve_palette(&palette)) {
        text = palette.text_primary;
        muted = palette.text_muted;
        outline = palette.pane_border;
        selected = palette.accent_primary;
        sourceFill = palette.pane_header_fill;
        depFill = palette.pane_body_fill;
    }

    st->graphBounds = clip;
    SDL_SetRenderDrawColor(renderer, muted.r, muted.g, muted.b, 80);

    if (st->graphNodeCount <= 0) {
        const char* empty = "No include dependency graph yet";
        int tx = clip.x + 12;
        int ty = clip.y + 12;
        drawTextUTF8WithFontColorClipped(tx, ty, empty, rowFont, muted, false, &clip);
        return;
    }

    KitGraphStructNode* nodes =
        malloc((size_t)st->graphNodeCount * sizeof(KitGraphStructNode));
    if (!nodes) return;
    for (int i = 0; i < st->graphNodeCount; ++i) {
        nodes[i].id = st->graphNodes[i].id;
        nodes[i].label = st->graphNodes[i].label;
    }

    KitGraphStructLayoutStyle style;
    kit_graph_struct_layout_style_default(&style);
    style.padding = 18.0f;
    style.level_gap = 62.0f;
    style.sibling_gap = 18.0f;
    style.node_height = 26.0f;
    style.node_width = 150.0f;
    style.node_min_width = 112.0f;
    style.node_max_width = 240.0f;
    style.node_padding_x = 12.0f;
    style.label_char_width = 7.0f;

    bool compactGraph = clip.w < 1600;
    if (compactGraph) {
        compute_compact_libraries_graph_layout(st, clip);
    } else {
        if (clip.w < 820) {
            style.padding = 12.0f;
            style.level_gap = 48.0f;
            style.sibling_gap = 8.0f;
            style.node_height = 24.0f;
            style.node_width = 108.0f;
            style.node_min_width = 82.0f;
            style.node_max_width = 140.0f;
            style.node_padding_x = 8.0f;
            style.label_char_width = 6.0f;
        }
        KitRenderRect bounds = {
            .x = (float)clip.x,
            .y = (float)clip.y,
            .width = (float)clip.w,
            .height = (float)clip.h
        };
        KitGraphStructViewport viewport = {
            .pan_x = st->graphCamera.pan_x,
            .pan_y = st->graphCamera.pan_y,
            .zoom = st->graphCamera.zoom
        };
        CoreResult layoutResult =
            kit_graph_struct_compute_layered_dag_layout(nodes,
                                                        (uint32_t)st->graphNodeCount,
                                                        st->graphEdges,
                                                        (uint32_t)st->graphEdgeCount,
                                                        bounds,
                                                        &viewport,
                                                        &style,
                                                        st->graphLayouts);
        if (layoutResult.code != CORE_OK) {
            drawTextUTF8WithFontColorClipped(clip.x + 12,
                                             clip.y + 12,
                                             "Dependency graph layout failed",
                                             rowFont,
                                             muted,
                                             false,
                                             &clip);
            free(nodes);
            return;
        }
    }

    SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, 130);
    for (int i = 0; i < st->graphEdgeCount; ++i) {
        const KitGraphStructNodeLayout* from =
            graph_layout_by_id(st->graphLayouts, st->graphNodeCount, st->graphEdges[i].from_id);
        const KitGraphStructNodeLayout* to =
            graph_layout_by_id(st->graphLayouts, st->graphNodeCount, st->graphEdges[i].to_id);
        if (!from || !to) continue;
        if (compactGraph) {
            int x1 = (int)(from->rect.x + from->rect.width);
            int y1 = (int)(from->rect.y + from->rect.height * 0.5f);
            int x2 = (int)to->rect.x;
            int y2 = (int)(to->rect.y + to->rect.height * 0.5f);
            int laneOffset = (i % 7) - 3;
            int laneX = x1 + ((x2 - x1) / 2) + laneOffset * 5;
            int minLaneX = x1 + 8;
            int maxLaneX = x2 - 8;
            if (maxLaneX < minLaneX) {
                minLaneX = x1 + ((x2 - x1) / 2);
                maxLaneX = minLaneX;
            }
            if (laneX < minLaneX) laneX = minLaneX;
            if (laneX > maxLaneX) laneX = maxLaneX;
            SDL_RenderDrawLine(renderer, x1, y1, laneX, y1);
            SDL_RenderDrawLine(renderer, laneX, y1, laneX, y2);
            SDL_RenderDrawLine(renderer, laneX, y2, x2, y2);
        } else {
            int x1 = (int)(from->rect.x + from->rect.width * 0.5f);
            int y1 = (int)(from->rect.y + from->rect.height);
            int x2 = (int)(to->rect.x + to->rect.width * 0.5f);
            int y2 = (int)to->rect.y;
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
    }

    int textHeight = rowFont ? TTF_FontHeight(rowFont) : LIBRARY_ROW_HEIGHT;
    if (textHeight < 1) textHeight = LIBRARY_ROW_HEIGHT;
    LibraryGraphNodeInfo* detailNode = NULL;
    for (int i = 0; i < st->graphNodeCount; ++i) {
        LibraryGraphNodeInfo* node = &st->graphNodes[i];
        KitRenderRect r = st->graphLayouts[i].rect;
        SDL_Rect rect = {
            (int)r.x,
            (int)r.y,
            (int)r.width,
            (int)r.height
        };
        if (rect.x + rect.w < clip.x || rect.y + rect.h < clip.y ||
            rect.x > clip.x + clip.w || rect.y > clip.y + clip.h) {
            continue;
        }
        bool isSelected = st->selectedGraphNodeId == node->id;
        bool isHovered = st->hoveredGraphNodeId == node->id;
        if (isHovered) detailNode = node;
        else if (!detailNode && isSelected) detailNode = node;
        SDL_Color fill = node->type == LIB_NODE_DEP_SOURCE ? sourceFill : depFill;
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &rect);
        SDL_Color stroke = isSelected ? selected : (isHovered ? hovered : outline);
        SDL_SetRenderDrawColor(renderer, stroke.r, stroke.g, stroke.b, 255);
        SDL_RenderDrawRect(renderer, &rect);

        if (!compactGraph || isSelected || isHovered) {
            SDL_Rect textClip = {
                compactGraph ? rect.x + rect.w + 6 : rect.x + 7,
                rect.y,
                compactGraph ? (clip.x + clip.w - rect.x - rect.w - 10) : rect.w - 14,
                rect.h
            };
            if (textClip.w < 0) textClip.w = 0;
            int textY = rect.y + ((rect.h - textHeight) / 2);
            drawTextUTF8WithFontColorClipped(textClip.x,
                                             textY,
                                             compactGraph ? graph_node_short_name(node)
                                                          : (node->label ? node->label : "(node)"),
                                             rowFont,
                                             text,
                                             false,
                                             &textClip);
        }
    }

    render_libraries_graph_hud(renderer,
                               st,
                               clip,
                               rowFont,
                               text,
                               muted,
                               depFill,
                               detailNode ? selected : outline);

    char countBuf[128];
    if (st->hiddenGraphNodeCount > 0) {
        snprintf(countBuf,
                 sizeof(countBuf),
                 "%d nodes / %d edges / %d quiet / %.0f%%",
                 st->graphNodeCount,
                 st->graphEdgeCount,
                 st->hiddenGraphNodeCount,
                 st->graphCamera.zoom * 100.0f);
    } else {
        snprintf(countBuf,
                 sizeof(countBuf),
                 "%d nodes / %d edges / %.0f%%",
                 st->graphNodeCount,
                 st->graphEdgeCount,
                 st->graphCamera.zoom * 100.0f);
    }
    drawTextUTF8WithFontColorClipped(clip.x + 10,
                                     clip.y + clip.h - textHeight - 8,
                                     countBuf,
                                     rowFont,
                                     muted,
                                     false,
                                     &clip);

    free(nodes);
}

void renderLibrariesPanel(UIPane* pane) {
    LibraryPanelState* st = libraries_panel_state();
    const int rowHeightPx = LIBRARY_ROW_HEIGHT;
    uint64_t published_index_stamp = library_index_published_stamp();
    uint64_t include_graph_stamp = include_graph_combined_stamp();
    if (st->last_published_index_stamp != published_index_stamp ||
        st->last_include_graph_stamp != include_graph_stamp) {
        st->last_published_index_stamp = published_index_stamp;
        st->last_include_graph_stamp = include_graph_stamp;
        rebuildLibraryFlatRows();
    }
    static bool scrollInit = false;
    if (!scrollInit) {
        scroll_state_init(&st->scroll, NULL);
        scrollInit = true;
    }
    st->scroll.line_height_px = (float)rowHeightPx;

    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;

    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    const int trackWidth = 8;
    const int trackGap = 4;
    int contentTop = tool_panel_single_row_content_top(pane);
    ToolPanelSplitLayout split = {0};
    tool_panel_compute_split_layout(pane, contentTop, &split);
    int contentX = split.body_rect.x + (d.pad_left - 1);
    int contentY = split.body_rect.y;
    int contentH = split.body_rect.h;
    int viewBottom = contentY + contentH;

    ToolPanelControlRow row = tool_panel_control_row_at(pane, pane->y + d.controls_top);
    SDL_Rect viewRect = tool_panel_row_take_left(&row, 82);
    UIPanelTaggedRectList* controlHits = libraries_control_hits();
    ui_panel_tagged_rect_list_reset(controlHits);
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = viewRect,
                                       .label = st->viewMode == LIB_PANEL_VIEW_HEADERS
                                                    ? "Headers"
                                                    : (st->viewMode == LIB_PANEL_VIEW_DEPENDENCIES
                                                           ? "Deps"
                                                           : "Graph"),
                                       .active = st->viewMode != LIB_PANEL_VIEW_HEADERS,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits, LIB_TOP_CONTROL_VIEW_MODE, viewRect);

    SDL_Rect toggleRect = tool_panel_row_take_left(&row, 112);
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = toggleRect,
                                       .label = st->includeSystemHeaders ? "System: On" : "System: Off",
                                       .active = st->includeSystemHeaders,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    if (st->viewMode == LIB_PANEL_VIEW_HEADERS) {
        (void)ui_panel_tagged_rect_list_add(controlHits, LIB_TOP_CONTROL_SYSTEM_TOGGLE, toggleRect);
    }

    SDL_Rect logsRect = tool_panel_row_take_left(&row, 98);
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = logsRect,
                                       .label = analysis_frontend_logs_enabled() ? "Logs: On" : "Logs: Off",
                                       .active = analysis_frontend_logs_enabled(),
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits, LIB_TOP_CONTROL_LOGS_TOGGLE, logsRect);

    // Status indicator in header area (not clipped)
    AnalysisStatusSnapshot snap = {0};
    AnalysisSchedulerSnapshot sched = {0};
    int progressCompleted = 0;
    int progressTotal = 0;
    analysis_status_snapshot(&snap);
    analysis_status_get_progress(&progressCompleted, &progressTotal);
    analysis_scheduler_snapshot(&sched);
    char statusBuf[128] = {0};
    if (snap.updating) {
        if (progressTotal > 0) {
            snprintf(statusBuf, sizeof(statusBuf),
                     sched.active_run_id ? "Updating %d/%d (#%llu)" : "Updating %d/%d",
                     progressCompleted,
                     progressTotal,
                     (unsigned long long)sched.active_run_id);
        } else {
            snprintf(statusBuf, sizeof(statusBuf),
                     sched.active_run_id ? "Updating (#%llu)..." : "Updating...",
                     (unsigned long long)sched.active_run_id);
        }
    } else if (snap.last_error[0]) {
        snprintf(statusBuf, sizeof(statusBuf), "Analysis error");
    } else if (snap.has_cache) {
        snprintf(statusBuf, sizeof(statusBuf), "(cached)");
    }
    if (statusBuf[0]) {
        int tw = getTextWidth(statusBuf);
        int tx = pane->x + pane->w - tw - 16;
        int ty = tool_panel_info_line_y(pane, 0);
        drawText(tx, ty, statusBuf);
    }

    tool_panel_render_split_background(renderer, pane, contentTop, d.body_darken);

    st->scrollTrack = (SDL_Rect){
        split.body_rect.x + split.body_rect.w - trackWidth,
        contentY,
        trackWidth,
        contentH
    };
    st->scrollThumb = st->scrollTrack;
    SDL_Rect clip = {
        split.body_rect.x,
        contentY,
        split.body_rect.w - (trackWidth + trackGap),
        contentH
    };
    if (clip.w < 0) clip.w = 0;
    pushClipRect(&clip);

    float totalHeight = (float)(st->flatCount * rowHeightPx);
    scroll_state_set_viewport(&st->scroll, (float)contentH);
    scroll_state_set_content_height(&st->scroll,
                                    scroll_state_top_anchor_content_height(&st->scroll, totalHeight));
    st->scrollThumb = scroll_state_thumb_rect(&st->scroll,
                                              st->scrollTrack.x,
                                              st->scrollTrack.y,
                                              st->scrollTrack.w,
                                              st->scrollTrack.h);

    st->hoveredRow = -1;
    float offset = scroll_state_get_offset(&st->scroll);
    int yStart = contentY - (int)offset;
    TTF_Font* rowFont = library_row_font();
    int textHeight = rowFont ? TTF_FontHeight(rowFont) : rowHeightPx;
    if (textHeight < 1) textHeight = rowHeightPx;

    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);

    if (st->viewMode == LIB_PANEL_VIEW_GRAPH) {
        render_libraries_graph(renderer, st, clip, rowFont);
        popClipRect();
        return;
    }

    for (int i = 0; i < st->flatCount; ++i) {
        LibraryFlatRow* row = &st->flatRows[i];
        int rowHeight = rowHeightPx;
        int rowTop = yStart + i * rowHeight;
        int rowBottom = rowTop + rowHeight;
        if (rowBottom <= contentY) continue;   // entirely above clip
        if (rowTop >= viewBottom) break;       // past viewport

        int drawY = rowTop;
        int indent = row->depth * 20;
        int drawX = contentX + indent;

        const char* prefix = "    ";
        if (row->type == LIB_NODE_BUCKET) {
            bool open = st->bucketExpanded[row->bucketIndex];
            prefix = open ? "[-] " : "[+] ";
        } else if (row->type == LIB_NODE_HEADER) {
            bool open = (row->headerIndex >= 0 &&
                         row->headerIndex < (int)st->headerExpandedCount[row->bucketIndex] &&
                         st->headerExpanded[row->bucketIndex][row->headerIndex]);
            prefix = open ? "[-] " : "[+] ";
        }

        char line[1024];
        if (row->type == LIB_NODE_BUCKET) {
            int count = row->bucketHeaderCount;
            snprintf(line, sizeof(line), "%s%s (%d)", prefix, bucket_label((LibraryBucketKind)row->bucketIndex), count);
        } else if (row->type == LIB_NODE_HEADER) {
            const char* kindGlyph = (row->includeKind == LIB_INCLUDE_KIND_SYSTEM) ? "<>" : "\"\"";
            const char* unresolved = (row->bucketIndex == LIB_BUCKET_UNRESOLVED) ? " [!]" : "";
            snprintf(line, sizeof(line), "%s%s %s%s", prefix, kindGlyph,
                     row->labelPrimary ? row->labelPrimary : "(header)", unresolved);
        } else {
            if (row->type == LIB_NODE_DEP_SOURCE) {
                snprintf(line, sizeof(line), "%s (%d deps)",
                         row->labelPrimary ? row->labelPrimary : "(source)",
                         row->bucketHeaderCount);
            } else if (row->type == LIB_NODE_DEP_TARGET) {
                snprintf(line, sizeof(line), "-> %s",
                         row->labelPrimary ? row->labelPrimary : "(dependency)");
            } else {
                snprintf(line, sizeof(line), "    %s", row->labelPrimary ? row->labelPrimary : "(usage)");
            }
        }

        int textWidth = getTextWidthWithFont(line, rowFont);
        int textY = drawY + ((rowHeightPx - textHeight) / 2);
        SDL_Rect box = {
            drawX - 6,
            textY - 1,
            textWidth + 12,
            textHeight + 2
        };
        UIRowSurfaceLayout rowSurface = ui_row_surface_layout_from_rect(box);
        UIRowSurfaceLayout visibleSurface = {0};
        if (!ui_row_surface_clip(&rowSurface, &clip, &visibleSurface)) {
            continue;
        }

        bool isSel = library_row_is_selected(i);
        bool hovered = ui_row_surface_contains(&visibleSurface, mouseX, mouseY);
        UIRowSurfaceRenderSpec rowSpec = library_row_surface_spec(isSel,
                                                                  st->selectedRow == i,
                                                                  hovered);
        ui_row_surface_render(renderer, &visibleSurface, &rowSpec);

        if (hovered) {
            st->hoveredRow = i;
        }

        drawTextUTF8WithFontColorClipped(drawX,
                                         textY,
                                         line,
                                         rowFont,
                                         library_row_color(row),
                                         false,
                                         &clip);
    }

    // Scrollbar render
    SDL_SetRenderDrawColor(renderer, st->scroll.track_color.r, st->scroll.track_color.g,
                           st->scroll.track_color.b, st->scroll.track_color.a);
    SDL_RenderFillRect(renderer, &st->scrollTrack);
    SDL_SetRenderDrawColor(renderer, st->scroll.thumb_color.r, st->scroll.thumb_color.g,
                           st->scroll.thumb_color.b, st->scroll.thumb_color.a);
    SDL_RenderFillRect(renderer, &st->scrollThumb);
    popClipRect();
}
