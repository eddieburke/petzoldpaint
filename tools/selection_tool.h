#ifndef SELECTION_TOOL_H
#define SELECTION_TOOL_H

#include "../peztold_core.h"

#include "../helpers.h"
#include "../pixel_ops.h"
#include <windows.h>

/*------------------------------------------------------------------------------
 * Selection Tool Event Handlers
 *----------------------------------------------------------------------------*/

void SelectionToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void SelectionToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void SelectionToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

void SelectionToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY); // Draw handles/frame
void SelectionTool_Deactivate(void);
BOOL SelectionTool_Cancel(void);

/*------------------------------------------------------------------------------
 * State Management
 *----------------------------------------------------------------------------*/

void SelectionClearState(void);
void CommitSelection(void);
void CancelSelection(void);
BOOL IsSelectionActive(void);
BOOL SelectionIsDragging(void);
BOOL IsPointInSelection(int x, int y);

/*------------------------------------------------------------------------------
 * Clipboard / Edit Operations
 *----------------------------------------------------------------------------*/

void SelectionCopy(void);
void SelectionCut(void);
void SelectionPaste(HWND hWnd);
void SelectionInvert(void);
void SelectionInvertColors(void);
void SelectionDelete(void);
void SelectionSelectAll(void);

/*------------------------------------------------------------------------------
 * Transform Operations
 *----------------------------------------------------------------------------*/

void SelectionRotate(int degrees);
void SelectionFlip(BOOL bHorz);
int SelectionGetCursorId(int x, int y);
void SelectionMove(int dx, int dy);

/*------------------------------------------------------------------------------
 * Snapshot System (History)
 *----------------------------------------------------------------------------*/

#include "../poly_store.h"

typedef struct SelectionSnapshot {
    int mode;                  // Captured mode (SEL_NONE, SEL_REGION_ONLY, SEL_FLOATING)
    RECT rcBounds;            // Selection bounds
    HRGN hRegion;            // Copy of region (FreeType)
    HBITMAP hFloatBmp;       // Copy of floating bitmap
    BYTE *pFloatBits;        // Pixel data
    int nFloatW, nFloatH;     // Dimensions
    double fRotationAngle;       // Rotation angle
    POINT ptRotateCenter;      // Rotation center
    PolyStore freeformPts;      // Polygon points (for reconstructing region/editing)
} SelectionSnapshot;

SelectionSnapshot *Selection_CreateSnapshot(void);
void Selection_DestroySnapshot(SelectionSnapshot *snapshot);
void Selection_ApplySnapshot(SelectionSnapshot *snapshot);

#endif
