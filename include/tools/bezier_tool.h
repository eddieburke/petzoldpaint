#ifndef BEZIER_TOOL_H
#define BEZIER_TOOL_H

#include "peztold_core.h"

#include <windows.h>

typedef struct BezierToolSnapshot {
  int state;
  POINT ptStart;
  POINT ptEnd;
  POINT ptCtrl1;
  POINT ptCtrl2;
  int drawButton;
} BezierToolSnapshot;

void BezierTool_OnMouseDown(HWND hWnd, int x, int y, int nButton);
void BezierTool_OnMouseMove(HWND hWnd, int x, int y, int nButton);
void BezierTool_OnMouseUp(HWND hWnd, int x, int y, int nButton);

void BezierTool_DrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);

BOOL IsCurvePending(void);
BOOL BezierTool_Cancel(void);
void BezierTool_Deactivate(void);
void BezierTool_CommitPending(void);
BezierToolSnapshot *BezierTool_CreateSnapshot(void);
void BezierTool_DestroySnapshot(BezierToolSnapshot *snapshot);
void BezierTool_ApplySnapshot(const BezierToolSnapshot *snapshot);

#endif
