#ifndef AIRBRUSH_TOOL_H
#define AIRBRUSH_TOOL_H

#include "../peztold_core.h"

void AirbrushToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void AirbrushToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void AirbrushToolOnMouseUp(HWND hWnd, int x, int y, int nButton);
void AirbrushToolOnTimerTick(HWND hWnd);

BOOL IsAirbrushDrawing(void);
void AirbrushTool_Deactivate(void);
BOOL CancelAirbrushDrawing(void);

#endif
