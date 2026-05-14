/*------------------------------------------------------------
   app_commands.h - Menu command and init-menu-popup handlers
------------------------------------------------------------*/

#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H

#include <windows.h>

BOOL AppCommands_OnCommand(HWND hwnd, WPARAM wParam, LPARAM lParam);
void AppCommands_OnInitMenuPopup(HWND hwnd, WPARAM wParam, LPARAM lParam);

// Document Lifecycle
void DocumentNew(HWND hwnd);
void DocumentOpen(HWND hwnd, const char *path);

#endif /* APP_COMMANDS_H */
