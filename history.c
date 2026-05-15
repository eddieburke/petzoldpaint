/*------------------------------------------------------------
   HISTORY.C -- Undo History (full snapshots + per-layer deltas)
  ------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"
#include "peztold_core.h"
#include "tool_session.h"

extern void InvalidateCanvas(void);

#define MAX_HISTORY_ENTRIES 100

static HistoryEntry *s_historyHead = NULL;
static HistoryEntry *s_historyTail = NULL;
static HistoryEntry *s_currentEntry = NULL;
static int s_historyCount = 0;
static int s_currentPosition = -1;

static BYTE *s_pendingBefore = NULL;
static int s_pendingLayer = -1;
static int s_pendingW = 0;
static int s_pendingH = 0;
static int s_pendingLayerCount = 0;

void History_SnapshotLayer(int layerIndex) {
  int w = Canvas_GetWidth();
  int h = Canvas_GetHeight();
  int lc = LayersGetCount();
  if (layerIndex < 0 || layerIndex >= lc || w <= 0 || h <= 0)
    return;
  if (s_pendingBefore && s_pendingLayer == layerIndex && s_pendingW == w &&
      s_pendingH == h && s_pendingLayerCount == lc)
    return;

  History_AbortPendingLayerWrite();

  BYTE *src = LayersGetLayerColorBits(layerIndex);
  if (!src)
    return;

  size_t nbytes = (size_t)w * (size_t)h * 4u;
  s_pendingBefore = (BYTE *)malloc(nbytes);
  if (!s_pendingBefore)
    return;
  memcpy(s_pendingBefore, src, nbytes);
  s_pendingLayer = layerIndex;
  s_pendingW = w;
  s_pendingH = h;
  s_pendingLayerCount = lc;
}

void History_AbortPendingLayerWrite(void) {
  if (s_pendingBefore && s_pendingLayer >= 0) {
    BYTE *dst = LayersGetLayerColorBits(s_pendingLayer);
    if (dst && s_pendingW == Canvas_GetWidth() &&
        s_pendingH == Canvas_GetHeight() &&
        s_pendingLayerCount == LayersGetCount()) {
      size_t nbytes = (size_t)s_pendingW * (size_t)s_pendingH * 4u;
      memcpy(dst, s_pendingBefore, nbytes);
      LayersMarkDirty();
    }
  }
  free(s_pendingBefore);
  s_pendingBefore = NULL;
  s_pendingLayer = -1;
  s_pendingW = 0;
  s_pendingH = 0;
  s_pendingLayerCount = 0;
}

void History_ClearPendingLayerSnapshot(void) {
  free(s_pendingBefore);
  s_pendingBefore = NULL;
  s_pendingLayer = -1;
  s_pendingW = 0;
  s_pendingH = 0;
  s_pendingLayerCount = 0;
}

static void FreeHistoryEntry(HistoryEntry *entry) {
  if (!entry)
    return;
  if (entry->snapshot)
    LayersDestroySnapshot(entry->snapshot);
  if (entry->toolSession)
    ToolSession_Destroy(entry->toolSession);
  free(entry->deltaBefore);
  free(entry->deltaAfter);
  free(entry->description);
  free(entry);
}

static HistoryEntry *CreateFullEntry(const char *description) {
  HistoryEntry *entry = (HistoryEntry *)calloc(1, sizeof(HistoryEntry));
  if (!entry)
    return NULL;

  entry->kind = HIST_ENTRY_FULL;
  entry->snapshot = LayersCreateSnapshot();
  if (!entry->snapshot) {
    free(entry);
    return NULL;
  }

  entry->activeLayerIndex = LayersGetActiveIndex();

  size_t descLen = strlen(description) + 1;
  entry->description = (char *)malloc(descLen);
  if (!entry->description) {
    FreeHistoryEntry(entry);
    return NULL;
  }
  strncpy_s(entry->description, descLen, description, _TRUNCATE);
  return entry;
}

static HistoryEntry *CreateToolSessionEntry(const char *description) {
  HistoryEntry *entry = (HistoryEntry *)calloc(1, sizeof(HistoryEntry));
  if (!entry)
    return NULL;

  entry->kind = HIST_ENTRY_TOOL_SESSION;
  entry->activeLayerIndex = LayersGetActiveIndex();
  entry->toolSession = ToolSession_CaptureCurrent();
  if (!entry->toolSession) {
    free(entry);
    return NULL;
  }

  size_t descLen = strlen(description) + 1;
  entry->description = (char *)malloc(descLen);
  if (!entry->description) {
    FreeHistoryEntry(entry);
    return NULL;
  }
  strncpy_s(entry->description, descLen, description, _TRUNCATE);
  return entry;
}

static HistoryEntry *HistoryCreateDeltaEntryIfPossible(const char *description) {
  int w = Canvas_GetWidth();
  int h = Canvas_GetHeight();
  int active = LayersGetActiveIndex();
  int lc = LayersGetCount();

  if (!s_pendingBefore || s_pendingLayer < 0 || active != s_pendingLayer ||
      w != s_pendingW || h != s_pendingH || lc != s_pendingLayerCount) {
    return NULL;
  }

  BYTE *curBits = LayersGetLayerColorBits(active);
  if (!curBits)
    return NULL;

  size_t nbytes = (size_t)w * (size_t)h * 4u;

  HistoryEntry *entry = (HistoryEntry *)calloc(1, sizeof(HistoryEntry));
  if (!entry)
    return NULL;

  entry->kind = HIST_ENTRY_LAYER_PIXELS;
  entry->activeLayerIndex = active;
  entry->deltaLayerIndex = active;
  entry->deltaByteCount = nbytes;
  entry->deltaBefore = s_pendingBefore;
  s_pendingBefore = NULL;
  s_pendingLayer = -1;
  s_pendingW = 0;
  s_pendingH = 0;
  s_pendingLayerCount = 0;

  entry->deltaAfter = (BYTE *)malloc(nbytes);
  if (!entry->deltaAfter) {
    free(entry->deltaBefore);
    free(entry);
    return NULL;
  }
  memcpy(entry->deltaAfter, curBits, nbytes);

  size_t descLen = strlen(description) + 1;
  entry->description = (char *)malloc(descLen);
  if (!entry->description) {
    FreeHistoryEntry(entry);
    return NULL;
  }
  strncpy_s(entry->description, descLen, description, _TRUNCATE);
  return entry;
}

static void AppendEntry(HistoryEntry *entry) {
  if (!entry)
    return;

  if (s_historyTail) {
    s_historyTail->next = entry;
    entry->prev = s_historyTail;
  } else {
    s_historyHead = entry;
  }

  s_historyTail = entry;
  s_currentEntry = entry;
  s_historyCount++;
  s_currentPosition = s_historyCount - 1;
}

static void TrimRedoBranch(void) {
  if (!s_currentEntry || !s_currentEntry->next)
    return;

  HistoryEntry *entry = s_currentEntry->next;
  while (entry) {
    HistoryEntry *next = entry->next;
    FreeHistoryEntry(entry);
    s_historyCount--;
    entry = next;
  }
  s_currentEntry->next = NULL;
  s_historyTail = s_currentEntry;
}

static HistoryEntry *FindEntryAtIndex(int index) {
  if (index < 0 || index >= s_historyCount)
    return NULL;

  HistoryEntry *entry = s_historyHead;
  int i = 0;
  while (entry && i < index) {
    entry = entry->next;
    i++;
  }
  return entry;
}

static void PruneHeadIfNeeded(void) {
  while (s_historyCount > MAX_HISTORY_ENTRIES && s_historyHead) {
    HistoryEntry *oldHead = s_historyHead;
    s_historyHead = oldHead->next;
    if (s_historyHead) {
      s_historyHead->prev = NULL;
    } else {
      s_historyTail = NULL;
    }

    if (s_currentEntry == oldHead)
      s_currentEntry = s_historyHead;

    FreeHistoryEntry(oldHead);
    s_historyCount--;
    if (s_currentPosition > 0)
      s_currentPosition--;
  }
}

static BOOL ApplyFullEntry(HistoryEntry *entry) {
  if (!entry || entry->kind != HIST_ENTRY_FULL || !entry->snapshot)
    return FALSE;
  if (!LayersApplySnapshot(entry->snapshot))
    return FALSE;
  return TRUE;
}

static BOOL HistoryRebuildToCurrent(void) {
  if (!s_historyHead || !s_currentEntry)
    return FALSE;

  HistoryEntry *e0 = s_historyHead;
  if (e0->kind != HIST_ENTRY_FULL || !e0->snapshot)
    return FALSE;

  if (!ApplyFullEntry(e0))
    return FALSE;

  for (HistoryEntry *e = e0->next; e; e = e->next) {
    if (e->kind == HIST_ENTRY_FULL) {
      if (!ApplyFullEntry(e))
        return FALSE;
    } else if (e->kind == HIST_ENTRY_LAYER_PIXELS) {
      BYTE *bits = LayersGetLayerColorBits(e->deltaLayerIndex);
      if (!bits || e->deltaByteCount == 0)
        return FALSE;
      memcpy(bits, e->deltaAfter, e->deltaByteCount);
    }
    if (e == s_currentEntry)
      break;
  }

  LayersMarkDirty();
  LayersSetActiveIndex(s_currentEntry->activeLayerIndex);
  if (s_currentEntry->toolSession) {
    ToolSession_Apply(s_currentEntry->toolSession);
  } else {
    ToolSession_ClearAllPending();
  }
  InvalidateCanvas();
  Core_Notify(EV_PIXELS_CHANGED);
  return TRUE;
}

void HistoryInit(void) {
  HistoryDestroy();
  HistoryEntry *e = CreateFullEntry("Initial State");
  if (e)
    AppendEntry(e);
}

void HistoryDestroy(void) {
  HistoryEntry *entry = s_historyHead;
  while (entry) {
    HistoryEntry *next = entry->next;
    FreeHistoryEntry(entry);
    entry = next;
  }
  s_historyHead = NULL;
  s_historyTail = NULL;
  s_currentEntry = NULL;
  s_historyCount = 0;
  s_currentPosition = -1;
  History_ClearPendingLayerSnapshot();
}

void HistoryReportPushFailure(const char *context) {
  const char *label = context ? context : "Action";
  char msg[256];
  snprintf(msg, sizeof(msg),
           "History push failed for '%s' (undo entry not recorded).\n", label);
  msg[sizeof(msg) - 1] = '\0';
  OutputDebugStringA(msg);
}

BOOL HistoryPush(const char *description) {
  if (!description)
    description = "Action";

  History_ClearPendingLayerSnapshot();

  TrimRedoBranch();
  HistoryEntry *entry = CreateFullEntry(description);
  if (!entry) {
    HistoryReportPushFailure(description);
    return FALSE;
  }

  AppendEntry(entry);
  PruneHeadIfNeeded();
  Core_Notify(EV_PIXELS_CHANGED);
  return TRUE;
}

void HistoryPushToolActionForActiveLayer(const char *toolName,
                                         const char *action) {
  if (!toolName || !action)
    return;

  char layerName[64];
  int activeIdx = LayersGetActiveIndex();
  LayersGetName(activeIdx, layerName, sizeof(layerName));
  if (layerName[0] == '\0')
    StringCchPrintf(layerName, sizeof(layerName), "Layer %d", activeIdx + 1);

  char description[256];
  StringCchPrintf(description, sizeof(description), "%s: %s [%s]", toolName,
                  action, layerName);

  TrimRedoBranch();

  HistoryEntry *entry = HistoryCreateDeltaEntryIfPossible(description);
  if (!entry) {
    History_ClearPendingLayerSnapshot();
    entry = CreateFullEntry(description);
  }
  if (!entry) {
    HistoryReportPushFailure(description);
    return;
  }

  AppendEntry(entry);
  PruneHeadIfNeeded();
  Core_Notify(EV_PIXELS_CHANGED);
}

void HistoryPushToolSessionById(int toolId, const char *action) {
  if (!action)
    action = "Adjust";

  const char *toolName = GetToolName(toolId);
  if (!toolName)
    toolName = "Tool";

  char description[256];
  StringCchPrintf(description, sizeof(description), "%s: %s", toolName, action);

  TrimRedoBranch();
  HistoryEntry *entry = CreateToolSessionEntry(description);
  if (!entry) {
    HistoryReportPushFailure(description);
    return;
  }

  AppendEntry(entry);
  PruneHeadIfNeeded();
  Core_Notify(EV_PIXELS_CHANGED);
}

BOOL HistoryUndo(void) {
  if (!s_currentEntry || !s_currentEntry->prev)
    return FALSE;
  s_currentEntry = s_currentEntry->prev;
  s_currentPosition--;
  return HistoryRebuildToCurrent();
}

BOOL HistoryRedo(void) {
  if (!s_currentEntry || !s_currentEntry->next)
    return FALSE;
  s_currentEntry = s_currentEntry->next;
  s_currentPosition++;
  return HistoryRebuildToCurrent();
}

BOOL HistoryCanUndo(void) {
  return s_currentEntry && s_currentEntry->prev != NULL;
}

BOOL HistoryCanRedo(void) {
  return s_currentEntry && s_currentEntry->next != NULL;
}

void HistoryClear(void) {
  HistoryDestroy();
  HistoryInit();
  Core_Notify(EV_DOC_RESET);
}

BOOL HistoryJumpTo(int index) {
  HistoryEntry *entry = FindEntryAtIndex(index);
  if (!entry || entry == s_currentEntry)
    return FALSE;
  s_currentEntry = entry;
  s_currentPosition = index;
  return HistoryRebuildToCurrent();
}

int HistoryGetCount(void) { return s_historyCount; }

const char *HistoryGetDescription(int index) {
  HistoryEntry *entry = FindEntryAtIndex(index);
  return entry ? entry->description : NULL;
}

void HistoryGetDescriptionAt(int index, char *out, int outSize) {
  if (!out || outSize <= 0)
    return;

  const char *desc = HistoryGetDescription(index);
  if (desc) {
    strncpy_s(out, outSize, desc, _TRUNCATE);
  } else {
    out[0] = '\0';
  }
}

int HistoryGetPosition(void) { return s_currentPosition; }
