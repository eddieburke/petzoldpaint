#ifndef POLYGON_TOOL_H
#define POLYGON_TOOL_H

#include "../peztold_core.h"

#include <windows.h>

/*------------------------------------------------------------------------------
 * Polygon Tool Event Handlers
 *----------------------------------------------------------------------------*/

void PolygonTool_OnMouseDown(HWND hWnd, int x, int y, int nButton);
void PolygonTool_OnMouseMove(HWND hWnd, int x, int y, int nButton);
void PolygonTool_OnMouseUp(HWND hWnd, int x, int y, int nButton);
void PolygonTool_OnDoubleClick(HWND hWnd, int x, int y, int nButton);

/*------------------------------------------------------------------------------
 * Overlay Drawing (screen-space handles only)
 *----------------------------------------------------------------------------*/

void PolygonTool_DrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);

/*------------------------------------------------------------------------------
 * State / Lifecycle
 *----------------------------------------------------------------------------*/

BOOL IsPolygonPending(void);
void CommitPendingPolygon(void);
BOOL PolygonTool_Cancel(void);
void PolygonTool_Deactivate(void);

#endif
