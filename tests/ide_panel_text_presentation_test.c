#include "ide/UI/panel_text_presentation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static CoreResult measure(void* user,const char* text,float* width) {
    (void)user;*width=0;
    const unsigned char* s=(const unsigned char*)text;
    while(*s) {
        if(*s=='W') {*width+=15;++s;}
        else if(*s=='i') {*width+=3;++s;}
        else if(*s==0xc3 && s[1]==0xa9) {*width+=7;s+=2;}
        else if(*s==0xe6 && s[1]==0xb0 && s[2]==0xb4) {*width+=16;s+=3;}
        else if(*s==0xf0 && s[1]==0x9f && s[2]==0x99 && s[3]==0x82) {*width+=18;s+=4;}
        else return (CoreResult){CORE_ERR_INVALID_ARG,"fixture got incomplete scalar"};
    }
    return core_result_ok();
}
static int rects,texts;
static CoreResult rect(void* u,KitRenderRect r,KitRenderColor c) {
    KitUiTextPresentation* p=u;KitRenderRect v=p->options.viewport;
    assert(r.x>=v.x && r.y>=v.y && r.x+r.width<=v.x+v.width && r.y+r.height<=v.y+v.height);
    ++rects;return core_result_ok();
}
static CoreResult text(void* u,KitRenderVec2 at,const char* s) {
    KitUiTextPresentation* p=u;assert(at.x==p->rows[0].origin.x);
    assert(!strcmp(s,"Wié水🙂"));++texts;return core_result_ok();
}
int main(void) {
    KitUiTextPresentation p;SDL_Rect viewport={20,30,30,16};const char* value="Wié水🙂";
    assert(ide_panel_text_presentation_build(&p,value,11,viewport,14,true,measure,NULL));
    assert(p.count==1 && !strcmp(p.display,value) && p.cursor==11);
    assert(fabsf(p.rows[0].width-59)<.001 && fabsf(p.scroll_x-30)<.001);
    assert(p.caret.x==49 && p.caret.x+p.caret.width==50);
    KitUiTextPainter painter={&p,rect,text};KitUiTextPresentationColors colors={0};
    assert(kit_ui_text_presentation_paint(&p,&colors,&painter).code==CORE_OK && rects==1 && texts==1);
    for(int x=20;x<50;++x) {
        size_t hit=999;assert(kit_ui_text_presentation_hit(&p,x,30,measure,NULL,&hit).code==CORE_OK);
        assert(hit==0 || hit==1 || hit==2 || hit==4 || hit==7 || hit==11);
    }
    assert(ide_panel_text_presentation_build(&p,value,3,viewport,14,true,measure,NULL));assert(p.cursor==2);
    assert(ide_panel_text_presentation_build(&p,value,11,viewport,14,false,measure,NULL));assert(p.scroll_x==0);
    viewport.w=80;assert(ide_panel_text_presentation_build(&p,value,99,viewport,14,true,measure,NULL));assert(p.cursor==11 && p.scroll_x==0);
    viewport.w=12;assert(ide_panel_text_presentation_build(&p,value,11,viewport,14,true,measure,NULL));assert(p.caret.x==31);
    KitUiTextPresentation before=p;
    assert(!ide_panel_text_presentation_build(&p,"\x80",0,viewport,14,true,measure,NULL));assert(!memcmp(&p,&before,sizeof(p)));
    viewport.w=0;assert(!ide_panel_text_presentation_build(&p,value,0,viewport,14,true,measure,NULL));
    puts("Measured field presentation: UTF-8 boundaries, proportional widths, resize/caret reveal and clipped paint passed");
}
