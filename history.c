/*------------------------------------------------------------
   HISTORY.C -- Full Undo History System

   This module implements an unlimited undo history that tracks
   all actions from the beginning of the document.
  ------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>

#include "history.h"
#include "layers.h"
#include "tools/selection_tool.h"
#include "tools.h"
#include "tools/pen_tool.h"
#include "tools/crayon_tool.h"
#include "tools/highlighter_tool.h"
#include "tools/freehand_tools.h"

// Forward declarations for panel sync functions - avoids circular include
// These are defined in the respective panel .c files
extern void LayersPanelSync(void);
extern void HistoryPanelSync(void);
extern void InvalidateCanvas(void);
#include <string.h>

#define MAX_HISTORY_ENTRIES 100

static HistoryEntry *s_historyHead = NULL;
static HistoryEntry *s_historyTail = NULL;
static HistoryEntry *s_currentEntry = NULL;
static int s_historyCount = 0;
static int s_currentPosition = -1;

static void FreeHistoryEntry(HistoryEntry *entry) {
  if (!entry)
    return;
  if (entry->snapshot)
    LayersDestroySnapshot(entry->snapshot);
  free(entry->description);
  free(entry);
}

static HistoryEntry *CreateEntryFromCurrentState(const char *description) {
  HistoryEntry *entry = (HistoryEntry *)calloc(1, sizeof(HistoryEntry));
  if (!entry)
    return NULL;

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

void HistoryInit(void) {
  HistoryDestroy();
  AppendEntry(CreateEntryFromCurrentState("Initial State"));
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
}

void HistoryPush(const char *description) {
  if (!description)
    description = "Action";

  TrimRedoBranch();
  AppendEntry(CreateEntryFromCurrentState(description));
  PruneHeadIfNeeded();

  // Notify panels of history change
  HistoryNotifyPanels();
}

// Helper to apply a snapshot
static void ApplySnapshot(HistoryEntry *entry) {
  LayersApplySnapshot(entry->snapshot);
  LayersMarkDirty();
  LayersSetActiveIndex(entry->activeLayerIndex);
  SelectionClearState();
  InvalidateCanvas();
}

// Notify panels after history changes
void HistoryNotifyPanels(void) {
  LayersPanelSync();
  HistoryPanelSync();
}

BOOL HistoryUndo(void) {
  if (!s_currentEntry || !s_currentEntry->prev) {
    return FALSE;
  }

  s_currentEntry = s_currentEntry->prev;
  s_currentPosition--;
  ApplySnapshot(s_currentEntry);
  HistoryNotifyPanels();
  return TRUE;
}

BOOL HistoryRedo(void) {
  if (!s_currentEntry || !s_currentEntry->next) {
    return FALSE;
  }

  s_currentEntry = s_currentEntry->next;
  s_currentPosition++;
  ApplySnapshot(s_currentEntry);
  HistoryNotifyPanels();
  return TRUE;
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
  HistoryNotifyPanels();
}

void HistoryJumpTo(int index) {
  if (index < 0 || index >= s_historyCount)
    return;

  // Find entry at index
  HistoryEntry *entry = s_historyHead;
  for (int i = 0; i < index && entry; i++) {
    entry = entry->next;
  }

  if (entry && entry != s_currentEntry) {
    s_currentEntry = entry;
    s_currentPosition = index;
    ApplySnapshot(entry);
    HistoryNotifyPanels();
  }
}

int HistoryGetCount(void) { return s_historyCount; }

const char *HistoryGetDescription(int index) {
  if (index < 0 || index >= s_historyCount)
    return NULL;

  HistoryEntry *entry = s_historyHead;
  for (int i = 0; i < index && entry; i++) {
    entry = entry->next;
  }
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
