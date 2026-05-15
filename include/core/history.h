#ifndef HISTORY_H
#define HISTORY_H
#include <windows.h>
#include "layers.h"

typedef struct ToolSessionSnapshot ToolSessionSnapshot;

#define HISTORY_MAX_ENTRIES 50

void HistoryInit(void);
void HistoryDestroy(void);
BOOL HistoryPush(const char* description);
BOOL HistoryUndo(void);
BOOL HistoryRedo(void);
void HistoryClear(void);
BOOL HistoryJumpTo(int index);
int HistoryGetPosition(void);
int HistoryGetCount(void);
const char* HistoryGetDescription(int index);
void HistoryGetDescriptionAt(int index, char* out, int outSize);

void HistoryPushSession(const char *description);

#endif
