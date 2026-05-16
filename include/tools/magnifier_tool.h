#ifndef MAGNIFIER_TOOL_H
#define MAGNIFIER_TOOL_H
#include <windows.h>
#include "tools.h"
void MagnifierTool_OnPointer(const ToolPointerEvent *ev);
void MagnifierTool_DrawOverlay(const OverlayContext *ctx);
void MagnifierTool_Deactivate(void);
void MagnifierTool_OnCaptureLost(void);
#endif
