#ifndef TOOLS_H
#define TOOLS_H
#include "peztold_core.h"
#include "overlay.h"
typedef enum ToolPointerEventType { TOOL_POINTER_DOWN = 0, TOOL_POINTER_MOVE, TOOL_POINTER_UP, TOOL_POINTER_DOUBLE_CLICK } ToolPointerEventType;
typedef struct ToolPointerEvent {
	ToolPointerEventType type;
	HWND hwnd;
	POINT
	bmp;
	UINT buttons;
} ToolPointerEvent;
typedef enum ToolLifecycleEventType { TOOL_LIFECYCLE_CANCEL = 0, TOOL_LIFECYCLE_CANCEL_INTERRUPT, TOOL_LIFECYCLE_CAPTURE_LOST, TOOL_LIFECYCLE_VIEWPORT_CHANGED, TOOL_LIFECYCLE_RESET_FOR_NEW_DOCUMENT, TOOL_LIFECYCLE_TIMER_TICK } ToolLifecycleEventType;
void ToolCancel(ToolCancelReason reason, BOOL skipSelectionTools);
void ToolHandlePointerEvent(const ToolPointerEvent *ev);
BOOL ToolHandleOverlayPointerEvent(HWND hWnd, int screenX, int screenY, int nButton);
void ToolHandleLifecycleEvent(ToolLifecycleEventType type, HWND hWnd);
void ToolDrawOverlay(const OverlayContext *ctx);
void SetCurrentTool(int nTool);
int Tool_GetCurrent(void);
BOOL Tool_IsValidId(int nTool);
void Tool_FinalizeCurrentState(void);
BOOL Tool_IsCurrentBusy(void);
void InitializeTools(void);
void ResetToolStateForNewDocument(void);
#endif
