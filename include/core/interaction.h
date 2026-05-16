#ifndef INTERACTION_H
#define INTERACTION_H
#include "peztold_core.h"
BOOL Interaction_Begin(HWND hWnd, int x, int y, int nButton, int toolId);
BOOL Interaction_BeginEx(HWND hWnd, int x, int y, int nButton, int toolId, BOOL captureMouse);
void Interaction_UpdateLastPoint(int x, int y);
void Interaction_MarkModified(void);
BOOL Interaction_IsActive(void);
BOOL Interaction_IsModified(void);
BOOL Interaction_Commit(const char *label);
void Interaction_Abort(void);
void Interaction_EndQuiet(void);
BOOL Interaction_IsActiveButton(int nButton);
void Interaction_OnCaptureLost(const char *defaultLabel);
int Interaction_GetActiveToolId(void);
int Interaction_GetDrawButton(void);
void Interaction_GetLastPoint(POINT *outPt);
/* Accumulate bitmap dirty rect during a stroke; one Invalidate per flush. */
void Interaction_NoteStrokeSegment(int x1, int y1, int x2, int y2, int padBmp);
void Interaction_FlushStrokeRedraw(void);
#endif
