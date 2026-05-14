#ifndef HISTORY_H
#define HISTORY_H
#include <windows.h>
#include "layers.h"

// History entry structure
typedef struct HistoryEntry {
    LayerSnapshot* snapshot;
    int activeLayerIndex;    // Layer active when entry was created
    char* description;
    struct HistoryEntry* next;
    struct HistoryEntry* prev;
} HistoryEntry;

// History system functions
void HistoryInit(void);
void HistoryDestroy(void);
void HistoryPush(const char* description);
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

// Internal: Notify panels of history changes (call after HistoryPush/Undo/Redo/Clear/Jump)
void HistoryNotifyPanels(void);

#endif
