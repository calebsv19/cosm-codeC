#ifndef UI_STATE_H
#define UI_STATE_H

#include <stdbool.h>

struct UIPane; 

typedef struct {
    bool toolPanelVisible;
    bool controlPanelVisible;
    bool terminalVisible;

    // Persistent pane memory
    struct UIPane* menuBar;    
    struct UIPane* iconBar;    
    struct UIPane* toolPanel;
    struct UIPane* editorPanel;
    struct UIPane* controlPanel;
    struct UIPane* terminalPanel;
    struct UIPane* popup;    

    bool panesInitialized;
} UIState;

UIState* getUIState(void);
void initializeUIPanesIfNeeded(void);



#endif // UI_STATE_H

