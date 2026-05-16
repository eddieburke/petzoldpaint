#ifndef FREEHAND_TOOLS_H
#define FREEHAND_TOOLS_H
#include "peztold_core.h"
#include "tools.h"
void FreehandTool_OnPointer(const ToolPointerEvent *ev);
void AirbrushTool_OnPointer(const ToolPointerEvent *ev);
void FreehandTool_OnTimerTick(void);
BOOL IsFreehandDrawing(void);
void FreehandTool_Deactivate(void);
BOOL FreehandTool_Cancel(void);
#endif
