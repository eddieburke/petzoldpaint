#ifndef HISTORY_PANEL_H
#define HISTORY_PANEL_H
#include <windows.h>
void CreateHistoryPanel(HWND hParent);
HWND GetHistoryPanelWindow(void);
void HistoryPanelSync(void);
#endif
