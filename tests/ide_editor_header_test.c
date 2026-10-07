#include "ide/Panes/Editor/editor_header_controls.h"
#include "ide/Panes/Editor/Input/input_editor.h"
#include "ide/Panes/Editor/Input/editor_input_mouse.h"
#include "core/CommandBus/command_metadata.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static IDECoreState core;
static uint64_t generation=1;
static bool renaming;
static int commands,legacy;
static InputCommand last_command;
IDECoreState* getCoreState(void){return &core;}
uint64_t ide_ipc_workspace_generation(void){return generation;}
bool isRenaming(void){return renaming;}
void enqueueCommand(InputCommandMetadata metadata){++commands;last_command=metadata.cmd;free(metadata.payload);}
void setActiveEditorView(EditorView* view){core.activeEditorView=view;}
EditorView* findLeafUnderCursor(EditorView* root,int x,int y){
 if(!root)return NULL;
 if(root->type==VIEW_SPLIT){EditorView* a=findLeafUnderCursor(root->childA,x,y);return a?a:findLeafUnderCursor(root->childB,x,y);}
 return x>=root->x && x<root->x+root->w && y>=root->y && y<root->y+root->h?root:NULL;
}
bool isEditorDraggingScrollbar(void){return false;}
void endEditorScrollbarDrag(void){}
bool handleEditorScrollbarEvent(UIPane* pane,SDL_Event* event){return false;}
void updateActiveEditorViewFromMouse(int x,int y){core.activeEditorView=findLeafUnderCursor(core.editorPane->editorView,x,y);}
EditorView* getHoveredEditorView(void){return core.activeEditorView;}
void handleEditorMouseClick(UIPane* p,SDL_Event* e,EditorView* v){++legacy;}
void handleEditorMouseDrag(UIPane* p,SDL_Event* e,EditorView* v){++legacy;}
void handleEditorMouseButtonUp(UIPane* p,SDL_Event* e,EditorView* v){++legacy;}
bool handleEditorScrollWheel(UIPane* p,SDL_Event* e){++legacy;return true;}
static void paint(UIPane* pane,EditorView* view,bool enabled){
 ide_editor_headers_begin(pane);
 KitUiInteractionControl controls[]={
 {.id=IDE_EDITOR_HEADER_CLOSE,.bounds={view->x+250,view->y+10,18,18},.enabled=enabled},
 {.id=IDE_EDITOR_HEADER_TAB_BASE,.bounds={view->x+10,view->y+10,70,18},.enabled=1},
 {.id=IDE_EDITOR_HEADER_TAB_BASE+1,.bounds={view->x+90,view->y+10,70,18},.enabled=1}};
 ide_editor_header_publish(view,controls,3);ide_editor_headers_end();
}
static void mouse(UIPane* p,Uint32 type,int x,int y,int button){SDL_Event e={0};e.type=type;e.button.x=x;e.button.y=y;e.button.button=button;handleEditorMouseInput(p,&e);}
int main(void){
 OpenFile a={0},b={0},c={0};OpenFile* files[]={&a,&b};
 EditorView leaf={.type=VIEW_LEAF,.x=0,.y=0,.w=300,.h=200,.fileCount=2,.activeTab=0,.openFiles=files};
 UIPane pane={.x=0,.y=0,.w=600,.h=300,.visible=true,.editorView=&leaf};core.editorPane=&pane;core.focusedPane=&pane;core.activeEditorView=&leaf;
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);assert(!commands);paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==1 && last_command==COMMAND_CLOSE_TAB);
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,95,15,SDL_BUTTON_LEFT);assert(leaf.activeTab==0);mouse(&pane,SDL_MOUSEBUTTONUP,95,15,SDL_BUTTON_LEFT);assert(leaf.activeTab==1 && core.activeEditorView==&leaf);
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);files[1]=&c;mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==1);files[1]=&b;
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);++generation;mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==1);
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);leaf.x++;mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==1);leaf.x--;
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);leaf.tabScrollX++;mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==1);leaf.tabScrollX--;
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);renaming=true;mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==1);renaming=false;
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);core.workspaceAuthoring.active=true;mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==1);core.workspaceAuthoring.active=false;
 paint(&pane,&leaf,false);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==1);
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);mouse(&pane,SDL_MOUSEBUTTONUP,220,15,SDL_BUTTON_LEFT);assert(commands==1);
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_RIGHT);mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_RIGHT);assert(commands==1);
 /* Ending document selection releases still pass through after model invalidation. */
 int before=legacy;paint(&pane,&leaf,true);leaf.activeTab=0;mouse(&pane,SDL_MOUSEBUTTONUP,150,100,SDL_BUTTON_LEFT);assert(legacy==before+1);
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);ide_editor_headers_forget_view(&leaf);mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==1);
 SDL_Event key={0};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_TAB;EditorView* target=NULL;uint32_t id=0;assert(!ide_editor_headers_event(&pane,&key,&target,&id));handleEditorKeyboardInput(&pane,&key);assert(commands==2 && last_command==COMMAND_INSERT_TAB);
 /* A different split leaf cannot inherit a held close. */
 EditorView other=leaf;other.x=300;EditorView root={.type=VIEW_SPLIT,.childA=&leaf,.childB=&other};pane.editorView=&root;
 paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);pane.editorView=&other;mouse(&pane,SDL_MOUSEBUTTONUP,555,15,SDL_BUTTON_LEFT);assert(commands==2);
 /* Retirement/reuse requires repaint, never dereferences a stale view. */
 ide_editor_headers_forget_view(&leaf);ide_editor_headers_forget_view(&other);pane.editorView=&leaf;paint(&pane,&leaf,true);mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==3);
 /* All eight split leaf scopes coexist, including all sixteen tab IDs. */
 ide_editor_headers_forget_view(&leaf);
 EditorView leaves[8]={0},splits[7]={0};OpenFile* all_files[16];
 for(int j=0;j<16;++j)all_files[j]=(j%2)?&a:&b;
 for(int j=0;j<8;++j){leaves[j]=leaf;leaves[j].x=j*300;leaves[j].openFiles=all_files;leaves[j].fileCount=16;leaves[j].activeTab=0;}
 for(int j=6;j>=0;--j){splits[j].type=VIEW_SPLIT;splits[j].childA=&leaves[j];splits[j].childB=(j==6)?&leaves[7]:&splits[j+1];}
 pane.editorView=&splits[0];pane.w=2400;
 ide_editor_headers_begin(&pane);
 for(int j=0;j<8;++j){
  KitUiInteractionControl controls[17]={0};
  controls[0]=(KitUiInteractionControl){.id=IDE_EDITOR_HEADER_CLOSE,.bounds={j*300+250,10,18,18},.enabled=1};
  for(int k=0;k<16;++k)controls[k+1]=(KitUiInteractionControl){.id=IDE_EDITOR_HEADER_TAB_BASE+k,.bounds={j*300+10+k*12,10,10,18},.enabled=1};
  ide_editor_header_publish(&leaves[j],controls,17);
 }
 ide_editor_headers_end();
 mouse(&pane,SDL_MOUSEBUTTONDOWN,2355,15,SDL_BUTTON_LEFT);mouse(&pane,SDL_MOUSEBUTTONUP,2355,15,SDL_BUTTON_LEFT);assert(commands==4 && core.activeEditorView==&leaves[7]);
 mouse(&pane,SDL_MOUSEBUTTONDOWN,1990,15,SDL_BUTTON_LEFT);mouse(&pane,SDL_MOUSEBUTTONUP,1990,15,SDL_BUTTON_LEFT);assert(leaves[6].activeTab==15 && core.activeEditorView==&leaves[6]);
 /* Existing larger sessions retain close and late-index tab activation. */
 for(int j=0;j<8;++j)ide_editor_headers_forget_view(&leaves[j]);
 OpenFile* many[80];for(int j=0;j<80;++j)many[j]=&a;
 leaf.openFiles=many;leaf.fileCount=80;leaf.activeTab=79;pane.editorView=&leaf;pane.w=600;
 ide_editor_headers_begin(&pane);
 KitUiInteractionControl late[]={
 {.id=IDE_EDITOR_HEADER_CLOSE,.bounds={250,10,18,18},.enabled=1},
 {.id=IDE_EDITOR_HEADER_TAB_BASE+70,.bounds={10,10,70,18},.enabled=1}};
 ide_editor_header_publish(&leaf,late,2);ide_editor_headers_end();
 mouse(&pane,SDL_MOUSEBUTTONDOWN,15,15,SDL_BUTTON_LEFT);mouse(&pane,SDL_MOUSEBUTTONUP,15,15,SDL_BUTTON_LEFT);assert(leaf.activeTab==70);
 ide_editor_headers_begin(&pane);ide_editor_header_publish(&leaf,late,2);ide_editor_headers_end();
 mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);many[79]=&b;mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==4);
 ide_editor_headers_begin(&pane);ide_editor_header_publish(&leaf,late,2);ide_editor_headers_end();
 mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);assert(commands==5);
 /* Empty split leaves retain a usable close control; the sole empty root does not. */
 ide_editor_headers_forget_view(&leaf);
 leaf.fileCount=0;leaf.activeTab=-1;leaf.openFiles=NULL;
 other.fileCount=0;other.activeTab=-1;other.openFiles=NULL;
 root.childA=&leaf;root.childB=&other;pane.editorView=&root;
 assert(ide_editor_header_close_enabled(&pane,&leaf));
 paint(&pane,&leaf,ide_editor_header_close_enabled(&pane,&leaf));
 mouse(&pane,SDL_MOUSEBUTTONDOWN,255,15,SDL_BUTTON_LEFT);
 mouse(&pane,SDL_MOUSEBUTTONUP,255,15,SDL_BUTTON_LEFT);
 assert(commands==6 && last_command==COMMAND_CLOSE_TAB && core.activeEditorView==&leaf);
 pane.editorView=&leaf;assert(!ide_editor_header_close_enabled(&pane,&leaf));
 assert(!ide_editor_header_close_enabled(&pane,&other));
 puts("Editor production header routing: release actions, model/geometry/workspace/modal cancellation and document input ownership pass");return 0;
}
