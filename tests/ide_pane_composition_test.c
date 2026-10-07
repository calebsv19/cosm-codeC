#include "ide/UI/pane_composition.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    UIPane left={.x=0,.y=40,.w=60,.h=600,.visible=true,.role=PANE_ROLE_ICONBAR};
    UIPane right={.x=60,.y=40,.w=100,.h=600,.visible=true,.role=PANE_ROLE_TOOLPANEL};
    UIPane popup={.x=55,.y=50,.w=20,.h=30,.visible=true,.role=PANE_ROLE_POPUP};
    UIPane* panes[]={&left,&right,&popup};KitPaneComposition view={0};SDL_Rect clip;
    assert(ide_pane_composition_build(&view,panes,3,150,500));
    assert(ide_pane_composition_hit(&view,panes,3,59,40)==&left);
    assert(ide_pane_composition_hit(&view,panes,3,60,40)==&right);
    assert(ide_pane_composition_hit(&view,panes,3,60,55)==&popup);
    assert(!ide_pane_composition_hit(&view,panes,3,150,100));
    assert(ide_pane_composition_clip(&view,&right,&clip));assert(clip.x==60 && clip.w==90 && clip.h==460);
    CorePaneId id=view.entries[0].id;
    UIPane* reordered[]={&right,&left,&popup};assert(ide_pane_composition_build(&view,reordered,3,150,500));
    assert(kit_pane_composition_find(&view,id)->shell.x==0);
    popup.visible=false;right.visible=false;assert(ide_pane_composition_build(&view,panes,3,150,500));
    assert(!ide_pane_composition_hit(&view,panes,3,60,55));
    right.visible=true;right.role=left.role;KitPaneComposition before=view;
    assert(!ide_pane_composition_build(&view,panes,3,150,500));assert(view.count==before.count);
    right.role=PANE_ROLE_TOOLPANEL;right.w=-2;assert(!ide_pane_composition_build(&view,panes,3,150,500));
    puts("fixed-pane shared composition: stable identities, half-open edges, topmost popup, visibility, viewport clipping and failure preservation pass");
}
