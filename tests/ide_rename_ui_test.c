#include "core/InputManager/UserInput/rename_flow.h"
#include "ide/UI/button_scope.h"
#include "app/GlobalInfo/core_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static IDECoreState core;
static int calls,validations,cancels;
static bool valid=true;
static char completed[MAX_PATH_LENGTH];
IDECoreState* getCoreState(void){return &core;}
void requestFullRedraw(uint32_t reason){}
void ide_button_scopes_cancel_except(UIPane* pane){assert(!pane);++cancels;}
static bool validate(const char* text,void* context){++validations;return valid;}
static void complete(const char* old,const char* text,void* context){++calls;snprintf(completed,sizeof(completed),"%s",text);assert(context==&core);}
int main(void){
 beginRenameWithPrompt("Rename","Invalid","Wié水🙂",complete,validate,&core,false);assert(isRenaming() && core.popupPaneActive && cancels==1);
 assert(core.renameFlow.cursorPosition==11);assert(handleRenameEditingKey(SDLK_LEFT));assert(core.renameFlow.cursorPosition==7);
 assert(handleRenameEditingKey(SDLK_BACKSPACE));assert(!strcmp(core.renameFlow.inputBuffer,"Wié🙂") && core.renameFlow.cursorPosition==4);
 assert(handleRenameUtf8TextInput("水"));assert(!strcmp(core.renameFlow.inputBuffer,"Wié水🙂"));assert(handleRenameEditingKey(SDLK_DELETE));assert(!strcmp(core.renameFlow.inputBuffer,"Wié水"));
 assert(handleRenameEditingKey(SDLK_HOME));assert(handleRenameUtf8TextInput("é"));assert(handleRenameEditingKey(SDLK_END));
 char before[MAX_PATH_LENGTH];strcpy(before,core.renameFlow.inputBuffer);int cursor=core.renameFlow.cursorPosition;
 assert(!handleRenameUtf8TextInput("\xff"));assert(!handleRenameUtf8TextInput("\n"));assert(!strcmp(before,core.renameFlow.inputBuffer) && cursor==core.renameFlow.cursorPosition);
 char huge[MAX_PATH_LENGTH+1];memset(huge,'a',MAX_PATH_LENGTH);huge[MAX_PATH_LENGTH]=0;assert(!handleRenameUtf8TextInput(huge));assert(!strcmp(before,core.renameFlow.inputBuffer));
 valid=false;submitRename();assert(!calls && isRenaming() && renameErrorVisible);valid=true;assert(handleRenameUtf8TextInput("x"));assert(!renameErrorVisible);submitRenameWithMod(KMOD_SHIFT);assert(calls==1 && validations==2 && !isRenaming() && !core.popupPaneActive);
 beginRename("same",complete,validate,&core);submitRename();assert(calls==1 && !isRenaming());
 beginRenameWithPrompt("Rename","Invalid","same",complete,validate,&core,true);submitRename();assert(calls==2 && !strcmp(completed,"same"));
 beginRename("keep",complete,validate,&core);cancelRename();assert(calls==2 && !isRenaming());
 beginRename(huge,complete,validate,&core);assert(!isRenaming() && calls==2);beginRename("\xff",complete,validate,&core);assert(!isRenaming());
 beginRename("é",complete,validate,&core);
 lastCaretBlink=1000;caretVisible=true;assert(!tickRenameAnimation(1000));assert(tickRenameAnimation(1551) && !caretVisible);assert(!tickRenameAnimation(1800));assert(tickRenameAnimation(1802) && caretVisible);handleRenameTextInput('\b');assert(!strcmp(core.renameFlow.inputBuffer,""));handleRenameTextInput('A');assert(!strcmp(core.renameFlow.inputBuffer,"A"));cancelRename();
 beginRename("keep",complete,validate,&core);
 assert(handleRenameEditingKeyWithMod(SDLK_a,KMOD_GUI));
 SDL_Event composition={0};composition.type=SDL_TEXTEDITING;strcpy(composition.edit.text,"é水");composition.edit.start=1;composition.edit.length=1;
 assert(handleRenameTextEvent(&composition));assert(!strcmp(core.renameFlow.inputBuffer,"keep"));
 int before_calls=calls,before_validations=validations;submitRename();
 assert(isRenaming() && calls==before_calls && validations==before_validations);
 SDL_Event commit={0};commit.type=SDL_TEXTINPUT;strcpy(commit.text.text,"renamed");assert(handleRenameTextEvent(&commit));
 assert(!strcmp(core.renameFlow.inputBuffer,"renamed") && !core.renameFlow.retainedEdit.composition[0]);
 submitRename();assert(calls==before_calls+1 && !strcmp(completed,"renamed"));
 beginRename("keep",complete,validate,&core);assert(handleRenameTextEvent(&composition));cancelRename();assert(!core.renameFlow.retainedEdit.composition[0]);
 puts("Global rename shared UTF-8 editing: intact rejection, scalar movement/delete and existing submit/validate/cancel policy pass");return 0;
}
