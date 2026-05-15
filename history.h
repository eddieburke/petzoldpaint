#ifndef HISTORY_H
#define HISTORY_H
#include <windows.h>
#include "layers.h"

typedef struct ToolSessionSnapshot ToolSessionSnapshot;

typedef enum HistoryEntryKind {
  HIST_ENTRY_FULL,
  HIST_ENTRY_LAYER_PIXELS,
  HIST_ENTRY_TOOL_SESSION
} HistoryEntryKind;

// History entry structure
typedef struct HistoryEntry {
  HistoryEntryKind kind;
  LayerSnapshot *snapshot;
  int activeLayerIndex;
  int deltaLayerIndex;
  size_t deltaByteCount;
  BYTE *deltaBefore;
  BYTE *deltaAfter;
  ToolSessionSnapshot *toolSession;
  char *description;
  struct HistoryEntry *next;
  struct HistoryEntry *prev;
} HistoryEntry;

// History system functions
void HistoryInit(void);
void HistoryDestroy(void);
BOOL HistoryPush(const char* description);
void HistoryReportPushFailure(const char* context);
BOOL HistoryUndo(void);
BOOL HistoryRedo(void);
BOOL HistoryCanUndo(void);
BOOL HistoryCanRedo(void);
void HistoryClear(void);
BOOL HistoryJumpTo(int index);
int HistoryGetPosition(void);  // Returns current position (0 = beginning, count-1 = end)
int HistoryGetCount(void);
const char* HistoryGetDescription(int index);
void HistoryGetDescriptionAt(int index, char* out, int outSize);

void History_SnapshotLayer(int layerIndex);
void History_AbortPendingLayerWrite(void);
void History_ClearPendingLayerSnapshot(void);
void HistoryPushToolActionForActiveLayer(const char *toolName,
                                         const char *action);
void HistoryPushToolSessionById(int toolId, const char *action);

#endif
