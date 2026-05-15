/*------------------------------------------------------------
   HISTORY.C - Simplified Undo History (full snapshots only)
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

#define MAX_HISTORY 100

typedef struct HistNode {
    LayerSnapshot *layers;
    ToolSessionSnapshot *tool;
    int activeIdx;
    char *desc;
    struct HistNode *next, *prev;
} HistNode;

static HistNode *head = NULL, *tail = NULL, *curr = NULL;
static int count = 0, pos = -1;

/* No-op: full snapshots are captured on HistoryPush(), not per-write. */
void History_SnapshotLayer(int idx) { (void)idx; }
void History_AbortPendingLayerWrite(void) {}
void History_ClearPendingLayerSnapshot(void) {}

static void FreeNode(HistNode *n) {
    if (!n) return;
    if (n->layers) LayersDestroySnapshot(n->layers);
    if (n->tool) ToolSession_Destroy(n->tool);
    free(n->desc);
    free(n);
}

/* After dropping the oldest node, logical indices shift; keep pos in range. */
static void EvictHeadIfOverLimit(void) {
    if (count <= MAX_HISTORY || !head)
        return;
    HistNode *old = head;
    head = head->next;
    if (head)
        head->prev = NULL;
    FreeNode(old);
    count--;
    if (pos >= count)
        pos = count - 1;
}

void HistoryInit(void) {
    HistoryDestroy();
    HistNode *n = calloc(1, sizeof(HistNode));
    n->layers = LayersCreateSnapshot();
    n->activeIdx = LayersGetActiveIndex();
    n->desc = _strdup("Initial");
    head = tail = curr = n;
    count = 1; pos = 0;
}

void HistoryDestroy(void) {
    while (head) { HistNode *n = head; head = head->next; FreeNode(n); }
    head = tail = curr = NULL;
    count = 0; pos = -1;
}

void HistoryReportPushFailure(const char *ctx) {
    char msg[256];
    snprintf(msg, sizeof(msg), "History push failed: %s\n", ctx ? ctx : "unknown");
    OutputDebugStringA(msg);
}

static void TrimRedo(void) {
    if (!curr || !curr->next) return;
    HistNode *n = curr->next;
    while (n) { HistNode *next = n->next; FreeNode(n); n = next; }
    curr->next = NULL; tail = curr;
    count = pos + 1;
}

BOOL HistoryPush(const char *desc) {
    if(!curr){HistoryReportPushFailure(desc);return FALSE;}
    TrimRedo();
    HistNode *n=calloc(1,sizeof(HistNode));
    n->layers=LayersCreateSnapshot();
    n->activeIdx=LayersGetActiveIndex();
    n->desc=desc?_strdup(desc):_strdup("Action");
    curr->next=n; n->prev=curr;
    curr=tail=n; count++; pos++;
    EvictHeadIfOverLimit();
    Core_Notify(EV_PIXELS_CHANGED);
    return TRUE;
}

void HistoryPushToolActionForActiveLayer(const char *tool, const char *act) {
    char buf[128];
    char lname[64];
    LayersGetName(LayersGetActiveIndex(), lname, sizeof(lname));
    if (!lname[0])
        snprintf(lname, sizeof(lname), "Layer %d", LayersGetActiveIndex() + 1);
    snprintf(buf, sizeof(buf), "%s: %s [%s]", tool ? tool : "Tool", act ? act : "Action", lname);
    HistoryPush(buf);
}

void HistoryPushToolSessionById(int tid, const char *act) {
    char buf[64];
    const char *tn = GetToolName(tid);
    snprintf(buf, sizeof(buf), "%s: %s", tn ? tn : "Tool", act ? act : "Adjust");
    History_ClearPendingLayerSnapshot();
    TrimRedo();
    HistNode *n = calloc(1, sizeof(HistNode));
    n->tool = ToolSession_CaptureCurrent();
    n->activeIdx = LayersGetActiveIndex();
    n->desc = _strdup(buf);
    if (curr) { curr->next = n; n->prev = curr; curr = n; tail = n; count++; pos++; }
    else { head = tail = curr = n; count = 1; pos = 0; }
    EvictHeadIfOverLimit();
    Core_Notify(EV_PIXELS_CHANGED);
}

static BOOL ApplyNode(HistNode *n) {
    if (!n)
        return FALSE;
    BOOL appliedLayers = FALSE;
    if (n->layers) {
        if (!LayersApplySnapshot(n->layers))
            return FALSE;
        appliedLayers = TRUE;
    } else {
        (void)LayersSetActiveIndex(n->activeIdx);
    }
    if (n->tool)
        ToolSession_Apply(n->tool);
    InvalidateCanvas();
    Core_Notify(EV_PIXELS_CHANGED);
    if (appliedLayers)
        Core_Notify(EV_LAYER_CONFIG);
    return TRUE;
}

BOOL HistoryUndo(void) {
    if (!curr || !curr->prev) return FALSE;
    curr = curr->prev; pos--;
    return ApplyNode(curr);
}

BOOL HistoryRedo(void) {
    if (!curr || !curr->next) return FALSE;
    curr = curr->next; pos++;
    return ApplyNode(curr);
}

void HistoryClear(void) { HistoryDestroy(); HistoryInit(); Core_Notify(EV_DOC_RESET); }

BOOL HistoryJumpTo(int idx) {
    if (idx < 0 || idx >= count) return FALSE;
    HistNode *n = head;
    for (int i = 0; i < idx && n; i++) n = n->next;
    if (!n) return FALSE;
    curr = n; pos = idx;
    return ApplyNode(curr);
}

int HistoryGetPosition(void) { return pos; }
int HistoryGetCount(void) { return count; }

const char *HistoryGetDescription(int idx) {
    HistNode *n = head;
    for (int i = 0; i < idx && n; i++) n = n->next;
    return n ? n->desc : NULL;
}

void HistoryGetDescriptionAt(int idx, char *out, int sz) {
    const char *d = HistoryGetDescription(idx);
    if (d) strncpy_s(out, sz, d, _TRUNCATE);
    else if (sz > 0) out[0] = '\0';
}
