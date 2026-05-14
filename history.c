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

void HistoryInit(void) {
  HistoryDestroy();

  HistoryEntry *initial = (HistoryEntry *)calloc(1, sizeof(HistoryEntry));
  if (!initial)
    return;

  initial->snapshot = LayersCreateSnapshot();
  if (!initial->snapshot) {
    free(initial);
    return;
  }

  initial->activeLayerIndex = LayersGetActiveIndex();

  size_t descLen = strlen("Initial State") + 1;
  char *desc = (char *)malloc(descLen);
  if (!desc) {
    LayersDestroySnapshot(initial->snapshot);
    free(initial);
    return;
  }
  strncpy_s(desc, descLen, "Initial State", _TRUNCATE);
  initial->description = desc;

  s_historyHead = initial;
  s_historyTail = initial;
  s_currentEntry = initial;
  s_historyCount = 1;
  s_currentPosition = 0;
}

void HistoryDestroy(void) {
  HistoryEntry *entry = s_historyHead;
  while (entry) {
    HistoryEntry *next = entry->next;
    if (entry->snapshot)
      LayersDestroySnapshot(entry->snapshot);
    if (entry->description)
      free(entry->description);
    free(entry);
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

  // Remove all entries after current if we're not at the end
  if (s_currentEntry && s_currentEntry->next) {
    int deletedCount = 0;
    HistoryEntry *entry = s_currentEntry->next;
    while (entry) {
      HistoryEntry *next = entry->next;
      if (entry->snapshot)
        LayersDestroySnapshot(entry->snapshot);
      if (entry->description)
        free(entry->description);
      free(entry);
      deletedCount++;
      entry = next;
    }
    s_currentEntry->next = NULL;
    s_historyTail = s_currentEntry;
    s_historyCount -= deletedCount;
  }

  // Create new entry
  HistoryEntry *newEntry = (HistoryEntry *)calloc(1, sizeof(HistoryEntry));
  if (!newEntry)
    return;

  newEntry->snapshot = LayersCreateSnapshot();
  if (!newEntry->snapshot) {
    free(newEntry);
    return;
  }

  newEntry->activeLayerIndex = LayersGetActiveIndex();

  size_t descLen = strlen(description) + 1;
  newEntry->description = (char *)malloc(descLen);
  if (!newEntry->description) {
    LayersDestroySnapshot(newEntry->snapshot);
    free(newEntry);
    return;
  }
  strncpy_s(newEntry->description, descLen, description, _TRUNCATE);

  // Link into list
  if (s_historyTail) {
    s_historyTail->next = newEntry;
    newEntry->prev = s_historyTail;
    s_historyTail = newEntry;
  } else {
    s_historyHead = newEntry;
    s_historyTail = newEntry;
  }

  s_currentEntry = newEntry;
  s_historyCount++;

    // Prune history if it exceeds the maximum allowed entries
    int prunedCount = 0;
    while (s_historyCount > MAX_HISTORY_ENTRIES && s_historyHead) {
        HistoryEntry *oldStart = s_historyHead;
        s_historyHead = oldStart->next;
        if (s_historyHead) {
            s_historyHead->prev = NULL;
        }
        
        if (oldStart->snapshot)
            LayersDestroySnapshot(oldStart->snapshot);
        if (oldStart->description)
            free(oldStart->description);
        free(oldStart);
        
        s_historyCount--;
        prunedCount++;
    }
    
    // Adjust current position based on how many entries were pruned from the head
    if (s_currentPosition >= prunedCount) {
        s_currentPosition -= prunedCount;
    } else {
        // If we've pruned past the current position, reset to beginning
        s_currentPosition = 0;
    }
    
    // Ensure current position is valid
    if (s_currentPosition >= s_historyCount) {
        s_currentPosition = s_historyCount - 1;
    }
    if (s_currentPosition < 0) {
        s_currentPosition = 0;
    }

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
