#include "tool_session.h"

#include "tools/bezier_tool.h"
#include "tools/polygon_tool.h"
#include "tools/selection_tool.h"
#include "tools/shape_tools.h"
#include "tools/text_tool.h"
#include <stdlib.h>

typedef BOOL (*IsActiveFn)(void);
typedef void *(*CaptureFn)(void);
typedef void (*ApplyFn)(void *);
typedef void (*DestroyFn)(void *);

typedef struct {
  IsActiveFn isActive;
  CaptureFn capture;
  ApplyFn apply;
  DestroyFn destroy;
} SessionOps;

static const SessionOps s_ops[TOOL_SESSION_BEZIER + 1] = {
    [TOOL_SESSION_SELECTION] = {IsSelectionActive,
                                (CaptureFn)Selection_CreateSnapshot,
                                (ApplyFn)Selection_ApplySnapshot,
                                (DestroyFn)Selection_DestroySnapshot},
    [TOOL_SESSION_TEXT] = {IsTextEditing, (CaptureFn)TextTool_CreateSnapshot,
                           (ApplyFn)TextTool_ApplySnapshot,
                           (DestroyFn)TextTool_DestroySnapshot},
    [TOOL_SESSION_SHAPE] = {IsShapePending, (CaptureFn)ShapeTool_CreateSnapshot,
                            (ApplyFn)ShapeTool_ApplySnapshot,
                            (DestroyFn)ShapeTool_DestroySnapshot},
    [TOOL_SESSION_POLYGON] = {IsPolygonPending,
                              (CaptureFn)PolygonTool_CreateSnapshot,
                              (ApplyFn)PolygonTool_ApplySnapshot,
                              (DestroyFn)PolygonTool_DestroySnapshot},
    [TOOL_SESSION_BEZIER] = {IsCurvePending, (CaptureFn)BezierTool_CreateSnapshot,
                             (ApplyFn)BezierTool_ApplySnapshot,
                             (DestroyFn)BezierTool_DestroySnapshot},
};

ToolSessionSnapshot *ToolSession_CaptureCurrent(void) {
  for (int k = TOOL_SESSION_SELECTION; k <= TOOL_SESSION_BEZIER; k++) {
    const SessionOps *op = &s_ops[k];
    if (!op->isActive || !op->isActive())
      continue;

    void *data = op->capture();
    if (!data)
      return NULL;

    ToolSessionSnapshot *snapshot =
        (ToolSessionSnapshot *)calloc(1, sizeof(ToolSessionSnapshot));
    if (!snapshot) {
      op->destroy(data);
      return NULL;
    }

    snapshot->kind = (ToolSessionKind)k;
    snapshot->data = data;
    return snapshot;
  }

  return NULL;
}

void ToolSession_ClearAllPending(void) {
  for (int k = TOOL_SESSION_SELECTION; k <= TOOL_SESSION_BEZIER; k++) {
    if (s_ops[k].apply)
      s_ops[k].apply(NULL);
  }
}

void ToolSession_Apply(const ToolSessionSnapshot *snapshot) {
  ToolSession_ClearAllPending();
  if (!snapshot || snapshot->kind <= TOOL_SESSION_NONE ||
      snapshot->kind > TOOL_SESSION_BEZIER)
    return;

  const SessionOps *op = &s_ops[snapshot->kind];
  if (op->apply)
    op->apply(snapshot->data);
}

void ToolSession_Destroy(ToolSessionSnapshot *snapshot) {
  if (!snapshot)
    return;

  if (snapshot->kind > TOOL_SESSION_NONE && snapshot->kind <= TOOL_SESSION_BEZIER) {
    const SessionOps *op = &s_ops[snapshot->kind];
    if (op->destroy)
      op->destroy(snapshot->data);
  }

  free(snapshot);
}
