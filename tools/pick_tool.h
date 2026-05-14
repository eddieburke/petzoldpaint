#ifndef PICK_TOOL_H
#define PICK_TOOL_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Pick Tool Public API
 *----------------------------------------------------------------------------*/

void PickToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void PickToolOnMouseMove(HWND hWnd, int x, int y, int nButton);

#endif
