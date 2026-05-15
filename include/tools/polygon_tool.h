#ifndef POLYGON_TOOL_H
#define POLYGON_TOOL_H

#include "peztold_core.h"
#include "poly_store.h"

#include <windows.h>

typedef struct PolygonToolSnapshot {
  BOOL bPolygonPending;
  BOOL bDragging;
  POINT ptRubberBand;
  int nDrawButton;
  int nDragIndex;
  PolyStore polygon;
} PolygonToolSnapshot;


void PolygonTool_OnMouseDown(HWND hWnd, int x, int y, int nButton);
void PolygonTool_OnMouseMove(HWND hWnd, int x, int y, int nButton);
void PolygonTool_OnMouseUp(HWND hWnd, int x, int y, int nButton);
void PolygonTool_OnDoubleClick(HWND hWnd, int x, int y, int nButton);


void PolygonTool_DrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);


BOOL IsPolygonPending(void);
BOOL PolygonTool_Cancel(void);
void PolygonTool_Deactivate(void);
void PolygonTool_CommitPending(void);
PolygonToolSnapshot *PolygonTool_CreateSnapshot(void);
void PolygonTool_DestroySnapshot(PolygonToolSnapshot *snapshot);
void PolygonTool_ApplySnapshot(const PolygonToolSnapshot *snapshot);

#endif
