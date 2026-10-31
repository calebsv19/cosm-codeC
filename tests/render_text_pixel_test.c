/* GPU readback oracle for the production codeC text path. No installed app used. */
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "kit_render_native_font.h"
#include "ide/UI/panel_text_presentation.h"
#include <SDL2/SDL_vulkan.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef assert
#define assert(x) do {if(!(x)){fprintf(stderr,"IDE pixel proof line%d: %s\n",__LINE__,#x);exit(2);}}while(0)
typedef struct Row { const char* text;TTF_Font* font;SDL_Surface* raster;SDL_Color ink;int x,y;bool bold,ascii,clipped;SDL_Rect clip,parent; } Row;
static SDL_Surface* reference(TTF_Font* font,const char* text,SDL_Color ink,bool ascii,float scale) {
    int points;char path[256];CoreFontTextSizeTier tier=font==getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION)?CORE_FONT_TEXT_SIZE_CAPTION:CORE_FONT_TEXT_SIZE_BASIC;
    bool terminal=font==getTerminalFont();
    assert(ide_shared_font_resolve_role(terminal?CORE_FONT_ROLE_UI_MONO:CORE_FONT_ROLE_UI_REGULAR,tier,path,sizeof(path),&points));
    if (!ascii && kit_render_native_font_needs_fallback(font,text))
        return kit_render_native_font_rasterize(path,points,text,ink,scale);
    TTF_Font* cpu=TTF_OpenFont(path,(int)lroundf(points*scale));assert(cpu);
    TTF_SetFontStyle(cpu,TTF_STYLE_NORMAL);TTF_SetFontHinting(cpu,TTF_HINTING_LIGHT);TTF_SetFontKerning(cpu,!terminal);
    SDL_Surface* result=ascii?TTF_RenderText_Blended(cpu,text,ink):TTF_RenderUTF8_Blended(cpu,text,ink);TTF_CloseFont(cpu);return result;
}
static void composite(unsigned char* rgb,int w,int h,const Row* row,float sx,float sy) {
    SDL_Rect clip={0,0,(int)ceilf(w/sx),(int)ceilf(h/sy)};
    if (row->clipped) assert(SDL_IntersectRect(&row->clip,&row->parent,&clip));
    int x0=(int)floorf(clip.x*sx),y0=(int)floorf(clip.y*sy),x1=(int)ceilf((clip.x+clip.w)*sx),y1=(int)ceilf((clip.y+clip.h)*sy);
    SDL_Surface* s=row->raster;assert(s && SDL_LockSurface(s)==0);
    for(int pass=0;pass<(row->bold?2:1);++pass) for(int y=0;y<s->h;y++)for(int x=0;x<s->w;x++) {
        int dx=(int)lroundf((row->x+pass)*sx)+x,dy=(int)lroundf(row->y*sy)+y;
        if(dx<0||dy<0||dx>=w||dy>=h||dx<x0||dy<y0||dx>=x1||dy>=y1)continue;
        Uint8 r,g,b,a;Uint32 pixel=0;memcpy(&pixel,(char*)s->pixels+y*s->pitch+x*s->format->BytesPerPixel,s->format->BytesPerPixel);SDL_GetRGBA(pixel,s->format,&r,&g,&b,&a);
        int ink[3]={r,g,b};for(int c=0;c<3;c++){unsigned char* p=rgb+((size_t)dy*w+dx)*3+c;*p=(ink[c]*a+*p*(255-a)+127)/255;}
    }
    SDL_UnlockSurface(s);
}
static size_t compare(const char* path,const unsigned char* expected,int width,int height) {
    FILE* f=fopen(path,"rb");assert(f);char line[128];int w,h;
    assert(fgets(line,sizeof(line),f)&&strcmp(line,"P6\n")==0);assert(fgets(line,sizeof(line),f)&&sscanf(line,"%d %d",&w,&h)==2&&w==width&&h==height);assert(fgets(line,sizeof(line),f)&&atoi(line)==255);
    size_t count=(size_t)w*h*3;unsigned char* got=malloc(count);assert(got&&fread(got,1,count,f)==count);fclose(f);
    size_t bad=0;for(size_t p=0;p<count;p++)if(abs(got[p]-expected[p])>1) {
        if(!bad)fprintf(stderr,"IDE pixel mismatch %s xy=%zu,%zu channel=%zu got=%d expected=%d\n",path,(p/3)%w,p/3/w,p%3,got[p],expected[p]);++bad;
    }
    free(got);if(bad){fprintf(stderr,"IDE pixel mismatched channels=%zu\n",bad);exit(3);}return (size_t)w*h;
}
static CoreResult measured(void* font,const char* text,float* width) { *width=getTextWidthUTF8WithFont(text,font);return core_result_ok(); }
static void check_caret(TTF_Font* font,const char* text,const SDL_Surface* cpu,float sx) {
    int width=getTextWidthUTF8WithFont(text,font);assert(width==(int)ceilf(cpu->w/sx));
    KitUiTextPresentation view;SDL_Rect viewport={0,0,800,32};
    assert(ide_panel_text_presentation_build(&view,text,5,viewport,32,true,measured,font));
    char prefix[6];memcpy(prefix,text,5);prefix[5]=0;
    assert(view.caret.x==getTextWidthUTF8WithFont(prefix,font));
    size_t position=0;assert(kit_ui_text_presentation_hit(&view,view.caret.x,8,measured,font,&position).code==CORE_OK && position==5);
}
int main(int argc,char** argv) {
    assert(argc==2);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0&&TTF_Init()==0);
    setenv("IDE_FONT_PRESET","ide",1);setenv("IDE_USE_SHARED_FONT","1",1);assert(initFontSystem());
    SDL_Window* window=SDL_CreateWindow("codeC production text pixel oracle",0,0,940,780,SDL_WINDOW_VULKAN|SDL_WINDOW_ALLOW_HIGHDPI);assert(window);
    VkRenderer renderer;VkRendererConfig cfg;vk_renderer_config_set_defaults(&cfg);cfg.enable_validation=VK_TRUE;
    assert(vk_renderer_init(&renderer,window,&cfg)==VK_SUCCESS);int width,height;SDL_Vulkan_GetDrawableSize(window,&width,&height);
    setRenderContext((SDL_Renderer*)&renderer,window,940,780);
    const float scales[]={2,1,1.25f,1.5f,2,2,2};size_t checked=0;
    char long_text[540],long_native[560];memset(long_text,'i',sizeof(long_text)-1);long_text[sizeof(long_text)-1]=0;snprintf(long_native,sizeof(long_native),"%s中",long_text);
    const char* labels[]={"Rigid result | Il1 012345","Camera / MWmw","Terminal | 1289","CJK 中 / 😀","Clipped label / Wm0123",long_text,long_native,"Bold / 1289","ASCII | 012345"};
    for(int z=0;z<7;z++) {
        float scale=scales[z];vk_renderer_set_logical_size(&renderer,width/scale,height/scale);
        if(z>=5){(void)ide_shared_font_set_zoom_step(z==5?1:0);assert(reloadFontSystem());}
        Row rows[9];memset(rows,0,sizeof(rows));SDL_Color ink={230,240,250,255};IDEThemePalette palette;assert(ide_shared_theme_resolve_palette(&palette));
        for(int i=0;i<9;i++) {
            rows[i]=(Row){.text=labels[i],.font=i==1?getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION):i==2?getTerminalFont():getActiveFont(),.ink=i==8?palette.text_primary:ink,.x=17,.y=10+i*34,.bold=i==7,.ascii=i==8,.clipped=i>=4&&i<=6};
            rows[i].clip=(SDL_Rect){24,rows[i].y+3,100,11};rows[i].parent=(SDL_Rect){19,rows[i].y+1,90,25};
            rows[i].raster=reference(rows[i].font,rows[i].text,rows[i].ink,rows[i].ascii,scale);assert(rows[i].raster);
        }
        check_caret(rows[0].font,rows[0].text,rows[0].raster,scale);
        int limit=getTextWidth(long_text);assert(getTextClampedLength(long_text,limit)==strlen(long_text));
        for(int replay=0;replay<2;replay++) {
            if(replay)for(int n=0;n<120;n++) {float unused;(void)getRasterizedFontForScale(getUIFontByTier((CoreFontTextSizeTier)(n%CORE_FONT_TEXT_SIZE_COUNT)),1+(n%39)*.075f,&unused);}
            char path[4096];snprintf(path,sizeof(path),"%s/phase-%d-%d.ppm",argv[1],z,replay);assert(vk_renderer_request_capture(&renderer,path)==VK_SUCCESS);
            VkCommandBuffer cmd;VkFramebuffer fb;VkExtent2D extent;assert(vk_renderer_begin_frame(&renderer,&cmd,&fb,&extent)==VK_SUCCESS);
            vk_renderer_reset_draw_transform(&renderer);vk_renderer_set_clip_rect(&renderer,NULL);vk_renderer_set_draw_color(&renderer,0,0,0,1);SDL_Rect bg={0,0,(int)ceilf(width/scale),(int)ceilf(height/scale)};vk_renderer_fill_rect(&renderer,&bg);
            unsigned char* expected=calloc((size_t)width*height,3);assert(expected);
            for(int i=0;i<9;i++) {
                Row* row=rows+i;
                if(row->clipped){vk_renderer_set_clip_rect(&renderer,&row->parent);drawTextUTF8WithFontColorClipped(row->x,row->y,row->text,row->font,row->ink,row->bold,&row->clip);SDL_Rect restored;vk_renderer_get_clip_rect(&renderer,&restored);assert(memcmp(&restored,&row->parent,sizeof(restored))==0);vk_renderer_set_clip_rect(&renderer,NULL);}
                else if(row->ascii)drawTextWithFont(row->x,row->y,row->text,row->font);
                else drawTextUTF8WithFontColor(row->x,row->y,row->text,row->font,row->ink,row->bold);
                composite(expected,width,height,row,scale,scale);
            }
            assert(vk_renderer_end_frame(&renderer,cmd)==VK_SUCCESS);checked+=compare(path,expected,width,height);free(expected);
        }
        for(int i=0;i<9;i++)SDL_FreeSurface(rows[i].raster);
    }
    const VkRuntimeCapabilityReport* report=vk_runtime_get_capability_report(&renderer.context.device->runtime);assert(report&&report->validation_enabled&&!report->validation_warning_count&&!report->validation_error_count);
    printf("IDE text oracle PASS pixels=%zu drawable=%dx%d scales=1/1.25/1.5/2 cold/replay/clips/long/native/bold/caret/hit/eviction/reload transitions validation=0/0\n",checked,width,height);
    render_text_cache_shutdown();shutdownFontSystem();vk_renderer_shutdown(&renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
