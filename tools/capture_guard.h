#ifndef CAPTURE_GUARD_H
#define CAPTURE_GUARD_H

#include "../peztold_core.h"

typedef enum {
  CAPTURE_END_NORMAL = 0,
  CAPTURE_END_CANCEL,
  CAPTURE_END_DEACTIVATE,
  CAPTURE_END_TOOL_SWITCH,
  CAPTURE_END_CAPTURE_LOST
} CaptureEndReason;

BOOL CaptureBegin(HWND hwnd, int toolId, int buttonState);
BOOL CaptureEnd(HWND hwnd, int toolId, CaptureEndReason reason);
BOOL CaptureOnLost(HWND hwnd, int toolId);
BOOL IsCapturedByTool(int toolId);

#endif
