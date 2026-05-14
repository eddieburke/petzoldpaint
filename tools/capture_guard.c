#include "capture_guard.h"

static struct {
  HWND hwnd;
  int toolId;
  int buttonState;
  BOOL active;
} s_capture = {NULL, -1, 0, FALSE};

BOOL CaptureBegin(HWND hwnd, int toolId, int buttonState) {
  if (!hwnd) return FALSE;
  s_capture.hwnd = hwnd;
  s_capture.toolId = toolId;
  s_capture.buttonState = buttonState;
  s_capture.active = TRUE;
  SetCapture(hwnd);
  return TRUE;
}

BOOL IsCapturedByTool(int toolId) {
  return s_capture.active && s_capture.toolId == toolId &&
         s_capture.hwnd && GetCapture() == s_capture.hwnd;
}

BOOL CaptureEnd(HWND hwnd, int toolId, CaptureEndReason reason) {
  (void)reason;
  if (!s_capture.active || s_capture.toolId != toolId) return FALSE;

  HWND owned = s_capture.hwnd;
  s_capture.active = FALSE;
  s_capture.hwnd = NULL;
  s_capture.toolId = -1;
  s_capture.buttonState = 0;

  if (owned && GetCapture() == owned) {
    ReleaseCapture();
  }
  (void)hwnd;
  return TRUE;
}

BOOL CaptureOnLost(HWND hwnd, int toolId) {
  (void)hwnd;
  if (!s_capture.active || s_capture.toolId != toolId) return FALSE;
  s_capture.active = FALSE;
  s_capture.hwnd = NULL;
  s_capture.toolId = -1;
  s_capture.buttonState = 0;
  return TRUE;
}
