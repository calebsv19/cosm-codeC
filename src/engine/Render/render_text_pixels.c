#include "engine/Render/render_text_pixels.h"
#if USE_VULKAN
void ide_text_draw_native_pixels(VkRenderer* renderer, const VkRendererTexture* texture,
                                int x, int y, bool bold, const SDL_Rect* clip) {
    if (!renderer || !texture) return;
    float sx,sy; ide_text_pixel_scale((SDL_Renderer*)renderer,&sx,&sy);
    SDL_Rect previous={0}, intersection;
    SDL_bool clipped=vk_renderer_is_clip_enabled(renderer);
    if (clipped) vk_renderer_get_clip_rect(renderer,&previous);
    bool explicit_clip=clip && clip->w>0 && clip->h>0;
    if (explicit_clip) {
        intersection=*clip;
        if (clipped && !SDL_IntersectRect(&previous,clip,&intersection)) return;
        vk_renderer_set_clip_rect(renderer,&intersection);
    }
    const float uv0[2]={0,0},uv1[2]={1,1},tint[4]={1,1,1,1};
    for (int pass=0;pass<(bold?2:1);++pass) {
        SDL_FRect bounds={roundf((x+pass)*sx)/sx,roundf(y*sy)/sy,
                          texture->width/sx,texture->height/sy};
        (void)vk_renderer_draw_textured_quad(renderer,texture,&bounds,uv0,uv1,tint);
    }
    if (explicit_clip) vk_renderer_set_clip_rect(renderer,clipped?&previous:NULL);
}
#endif
