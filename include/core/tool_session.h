#ifndef TOOL_SESSION_H
#define TOOL_SESSION_H

typedef enum ToolSessionKind {
  TOOL_SESSION_NONE = 0,
  TOOL_SESSION_SELECTION,
  TOOL_SESSION_TEXT,
  TOOL_SESSION_SHAPE,
  TOOL_SESSION_POLYGON,
  TOOL_SESSION_BEZIER
} ToolSessionKind;

typedef struct ToolSessionSnapshot {
  ToolSessionKind kind;
  void *data;
} ToolSessionSnapshot;

ToolSessionSnapshot *ToolSession_CaptureCurrent(void);
void ToolSession_Apply(const ToolSessionSnapshot *snapshot);
void ToolSession_Destroy(ToolSessionSnapshot *snapshot);
void ToolSession_ClearAllPending(void);

#endif
