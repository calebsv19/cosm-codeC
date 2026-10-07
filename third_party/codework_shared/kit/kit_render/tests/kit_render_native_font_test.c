#include "kit_render_native_font.h"
#include <assert.h>
#include <stdio.h>
int main(int argc, char** argv) {
    assert(argc == 2 && TTF_Init() == 0);
    TTF_Font* base = TTF_OpenFont(argv[1],18); assert(base);
    assert(!kit_render_native_font_needs_fallback(base,"Wi ASCII"));
    const char* mixed = "Wi 水 日本語 🙂 🌈";
    assert(kit_render_native_font_needs_fallback(base,mixed));
    int width=0,height=0;
#ifdef __APPLE__
    assert(kit_render_native_font_measure(argv[1],18,mixed,&width,&height));
    assert(width>70 && height>=18);
    for(int scale=1;scale<=2;++scale) {
        SDL_Surface* surface=kit_render_native_font_rasterize(argv[1],18,mixed,(SDL_Color){255,255,255,255},scale);
        assert(surface && surface->w==width*scale && surface->h==height*scale);
        int ink=0,colored=0;
        for(int y=0;y<surface->h;++y)for(int x=0;x<surface->w;++x) {
            unsigned char* px=(unsigned char*)surface->pixels+y*surface->pitch+x*4;
            if(px[3])++ink;
            if(px[3]>128 && (px[0]!=px[1] || px[1]!=px[2]))++colored;
        }
        assert(ink>50 && colored>10); /* Native emoji color survives straight-alpha conversion. */
        SDL_FreeSurface(surface);
    }
    assert(!kit_render_native_font_measure("/absent-font",18,mixed,&width,&height));
    assert(!kit_render_native_font_rasterize(argv[1],18,mixed,(SDL_Color){0},0));
#else
    assert(!kit_render_native_font_measure(argv[1],18,mixed,&width,&height));
#endif
    TTF_CloseFont(base);TTF_Quit();
    puts("Native fallback: CJK/color emoji, measurement/raster parity at 1x/2x, invalid inputs passed");
}
