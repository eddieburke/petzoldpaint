#ifndef TOOLS_H
#define TOOLS_H
#include "peztold_core.h"
#include "tools/selection_tool.h"

typedef enum ToolPointerEventType {
  TOOL_POINTER_DOWN = 0,
  TOOL_POINTER_MOVE,
  TOOL_POINTER_UP,
  TOOL_POINTER_DOUBLE_CLICK
} ToolPointerEventType;

typedef enum ToolLifecycleEventType {
  TOOL_LIFECYCLE_CANCEL = 0,
  TOOL_LIFECYCLE_CANCEL_INTERRUPT,
  TOOL_LIFECYCLE_CAPTURE_LOST,
  TOOL_LIFECYCLE_VIEWPORT_CHANGED,
  TOOL_LIFECYCLE_RESET_FOR_NEW_DOCUMENT,
  TOOL_LIFECYCLE_TIMER_TICK
} ToolLifecycleEventType;

void ToolCancel(ToolCancelReason reason, BOOL skipSelectionTools);
void ToolHandlePointerEvent(ToolPointerEventType type, HWND hWnd, int x, int y,
                            int nButton);
void ToolHandleLifecycleEvent(ToolLifecycleEventType type, HWND hWnd);

void ToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);
void SetCurrentTool(int nTool);
int Tool_GetCurrent(void);
void Tool_FinalizeCurrentState(void);
BOOL Tool_IsCurrentBusy(void);
void InitializeTools(void);
void CommitCurrentSelection(void);
void ClearSelection(void);
void ResetToolStateForNewDocument(void);
#endif
