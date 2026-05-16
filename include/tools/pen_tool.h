#ifndef PEN_TOOL_H
#define PEN_TOOL_H
#include "peztold_core.h"
#include "tools.h"
void PenTool_OnPointer(const ToolPointerEvent *ev);
BOOL IsPenDrawing(void);
void PenTool_Deactivate(void);
BOOL PenTool_Cancel(void);
#endif
