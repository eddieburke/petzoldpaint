#include "tool_session.h"

#include "tools/bezier_tool.h"
#include "tools/polygon_tool.h"
#include "tools/selection_tool.h"
#include "tools/shape_tools.h"
#include "tools/text_tool.h"
#include <stdlib.h>

typedef BOOL (*ToolSessionIsActiveFn)(void);
typedef void *(*ToolSessionCaptureFn)(void);
typedef void (*ToolSessionApplyFn)(void *);
typedef void (*ToolSessionDestroyFn)(void *);

typedef struct ToolSessionHandler {
  ToolSessionKind kind;
  ToolSessionIsActiveFn isActive;
  ToolSessionCaptureFn capture;
  ToolSessionApplyFn apply;
  ToolSessionDestroyFn destroy;
} ToolSessionHandler;

static const ToolSessionHandler s_handlers[] = {
    {TOOL_SESSION_SELECTION, IsSelectionActive,
     (ToolSessionCaptureFn)Selection_CreateSnapshot,
     (ToolSessionApplyFn)Selection_ApplySnapshot,
     (ToolSessionDestroyFn)Selection_DestroySnapshot},
    {TOOL_SESSION_TEXT, IsTextEditing, (ToolSessionCaptureFn)TextTool_CreateSnapshot,
     (ToolSessionApplyFn)TextTool_ApplySnapshot,
     (ToolSessionDestroyFn)TextTool_DestroySnapshot},
    {TOOL_SESSION_SHAPE, IsShapePending,
     (ToolSessionCaptureFn)ShapeTool_CreateSnapshot,
     (ToolSessionApplyFn)ShapeTool_ApplySnapshot,
     (ToolSessionDestroyFn)ShapeTool_DestroySnapshot},
    {TOOL_SESSION_POLYGON, IsPolygonPending,
     (ToolSessionCaptureFn)PolygonTool_CreateSnapshot,
     (ToolSessionApplyFn)PolygonTool_ApplySnapshot,
     (ToolSessionDestroyFn)PolygonTool_DestroySnapshot},
    {TOOL_SESSION_BEZIER, IsCurvePending,
     (ToolSessionCaptureFn)BezierTool_CreateSnapshot,
     (ToolSessionApplyFn)BezierTool_ApplySnapshot,
     (ToolSessionDestroyFn)BezierTool_DestroySnapshot},
};

static const ToolSessionHandler *ToolSession_FindHandler(ToolSessionKind kind) {
  for (int i = 0; i < (int)(sizeof(s_handlers) / sizeof(s_handlers[0])); i++) {
    if (s_handlers[i].kind == kind)
      return &s_handlers[i];
  }
  return NULL;
}

ToolSessionSnapshot *ToolSession_CaptureCurrent(void) {
  for (int i = 0; i < (int)(sizeof(s_handlers) / sizeof(s_handlers[0])); i++) {
    const ToolSessionHandler *handler = &s_handlers[i];
    if (!handler->isActive())
      continue;

    void *data = handler->capture();
    if (!data)
      return NULL;

    ToolSessionSnapshot *snapshot =
        (ToolSessionSnapshot *)calloc(1, sizeof(ToolSessionSnapshot));
    if (!snapshot) {
      handler->destroy(data);
      return NULL;
    }

    snapshot->kind = handler->kind;
    snapshot->data = data;
    return snapshot;
  }

  return NULL;
}

void ToolSession_ClearAllPending(void) {
  for (int i = 0; i < (int)(sizeof(s_handlers) / sizeof(s_handlers[0])); i++) {
    s_handlers[i].apply(NULL);
  }
}

void ToolSession_Apply(const ToolSessionSnapshot *snapshot) {
  ToolSession_ClearAllPending();
  if (!snapshot)
    return;

  const ToolSessionHandler *handler = ToolSession_FindHandler(snapshot->kind);
  if (handler)
    handler->apply(snapshot->data);
}

void ToolSession_Destroy(ToolSessionSnapshot *snapshot) {
  if (!snapshot)
    return;

  const ToolSessionHandler *handler = ToolSession_FindHandler(snapshot->kind);
  if (handler)
    handler->destroy(snapshot->data);

  free(snapshot);
}
