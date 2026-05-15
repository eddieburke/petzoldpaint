#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <windows.h>

/*------------------------------------------------------------
   Controller Layer

   Separates application logic from WndProc message handling.
   CanvasWndProc delegates to Controller_* functions, keeping
   canvas.c focused on windowing and rendering.
------------------------------------------------------------*/

#define TIMER_AIRBRUSH 101

/* Resize handle types */
typedef enum {
    RESIZE_HANDLE_NONE = 0,
    RESIZE_HANDLE_RIGHT,
    RESIZE_HANDLE_BOTTOM,
    RESIZE_HANDLE_CORNER
} ResizeHandleType;

/* Window message handlers */
void Controller_HandleSize(HWND hwnd);
void Controller_HandleMouseDown(HWND hwnd, int screenX, int screenY, int btn);
void Controller_HandleMouseMove(HWND hwnd, int screenX, int screenY, int wParam);
void Controller_HandleMouseUp(HWND hwnd, int screenX, int screenY, int btn);
void Controller_HandleDoubleClick(HWND hwnd, int screenX, int screenY, int btn);
void Controller_HandleKey(HWND hwnd, WPARAM wParam, BOOL down);
void Controller_HandleMouseWheel(HWND hwnd, WPARAM wParam, LPARAM lParam);
void Controller_HandleSetCursor(HWND hwnd, int screenX, int screenY);
void Controller_HandleCaptureLost(HWND hwnd);
void Controller_HandleTimer(HWND hwnd, WPARAM id);
void Controller_HandleScroll(HWND hwnd, int nBar, int nScrollCode);

/* Viewport / scroll helpers */
void Controller_UpdateScrollbars(HWND hwnd);

/* Accessors for WM_PAINT (resize preview) */
BOOL Controller_IsResizing(void);
void Controller_GetResizePreview(int *outW, int *outH);

#endif /* CONTROLLER_H */
