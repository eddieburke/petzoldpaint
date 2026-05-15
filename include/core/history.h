#ifndef HISTORY_H
#define HISTORY_H
#include <windows.h>
#include "layers.h"

typedef struct ToolSessionSnapshot ToolSessionSnapshot;

// History system functions
void HistoryInit(void);
void HistoryDestroy(void);
BOOL HistoryPush(const char* description);
void HistoryReportPushFailure(const char* context);
BOOL HistoryUndo(void);
BOOL HistoryRedo(void);
void HistoryClear(void);
BOOL HistoryJumpTo(int index);
int HistoryGetPosition(void);  // Returns current position (0 = beginning, count-1 = end)
int HistoryGetCount(void);
const char* HistoryGetDescription(int index);
void HistoryGetDescriptionAt(int index, char* out, int outSize);

void HistoryPushSession(const char *description);

#endif
