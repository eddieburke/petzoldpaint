#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas.h"
#include "helpers.h"
#include "history.h"
#include "interaction.h"
#include "layers.h"
#include "peztold_core.h"
#include "tool_session.h"
#include "tools.h"
typedef struct HistNode {
	LayerSnapshot *layers;
	ToolSessionSnapshot *tool;
	char *desc;
	struct HistNode *next, *prev;
} HistNode;
static HistNode *head = NULL, *tail = NULL, *curr = NULL;
static int count = 0, pos = -1;
static void FreeNode(HistNode *n) {
	if (!n)
		return;
	if (n->layers)
		LayersDestroySnapshot(n->layers);
	if (n->tool)
		ToolSession_Destroy(n->tool);
	free(n->desc);
	free(n);
}
/* After dropping the oldest node, logical indices shift; keep pos in range. */
static void EvictHeadIfOverLimit(void) {
	if (count <= HISTORY_MAX_ENTRIES || !head)
		return;
	HistNode *old = head;
	head = head->next;
	if (head)
		head->prev = NULL;
	FreeNode(old);
	count--;
	/* Logical indices shift left by one when the oldest entry is removed. */
	if (pos > 0)
		pos--;
	if (pos >= count)
		pos = count - 1;
}
void HistoryInit(void) {
	HistoryDestroy();
	HistNode *n = calloc(1, sizeof(HistNode));
	if (!n)
		return;
	n->layers = LayersCreateSnapshot();
	n->desc = _strdup("Initial");
	if (!n->layers || !n->desc) {
		FreeNode(n);
		return;
	}
	head = tail = curr = n;
	count = 1;
	pos = 0;
}
void HistoryDestroy(void) {
	while (head) {
		HistNode *n = head;
		head = head->next;
		FreeNode(n);
	}
	head = tail = curr = NULL;
	count = 0;
	pos = -1;
}
static void HistoryReportPushFailure(const char *ctx) {
	char msg[256];
	snprintf(msg, sizeof(msg), "History push failed: %s\n", ctx ? ctx : "unknown");
	OutputDebugStringA(msg);
}
static void TrimRedo(void) {
	if (!curr || !curr->next)
		return;
	HistNode *n = curr->next;
	while (n) {
		HistNode *next = n->next;
		FreeNode(n);
		n = next;
	}
	curr->next = NULL;
	tail = curr;
	count = pos + 1;
}
static BOOL PushInternal(const char *desc, BOOL captureTool) {
	if (!curr) {
		HistoryReportPushFailure(desc);
		return FALSE;
	}
	TrimRedo();
	HistNode *n = (HistNode *)calloc(1, sizeof(HistNode));
	if (!n)
		return FALSE;
	n->layers = LayersCreateSnapshot();
	if (captureTool)
		n->tool = ToolSession_CaptureCurrent();
	n->desc = _strdup(desc ? desc : "Action");
	if (!n->layers || !n->desc) {
		FreeNode(n);
		return FALSE;
	}
	if (curr) {
		curr->next = n;
		n->prev = curr;
		curr = n;
		tail = n;
		count++;
		pos++;
	} else {
		head = tail = curr = n;
		count = 1;
		pos = 0;
	}
	EvictHeadIfOverLimit();
	Core_Notify(EV_PIXELS_CHANGED);
	return TRUE;
}
BOOL HistoryPush(const char *desc) {
	return PushInternal(desc, FALSE);
}
void HistoryPushSession(const char *desc) {
	(void)PushInternal(desc, TRUE);
}
static BOOL ApplyNode(HistNode *n) {
	if (!n)
		return FALSE;
	ToolCancel(TOOL_CANCEL_INTERRUPT, FALSE);
	if (!n->layers || !LayersApplySnapshot(n->layers))
		return FALSE;
	if (Interaction_IsActive())
		Interaction_Abort();
	ToolSession_Apply(n->tool);
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	Core_Notify(EV_PIXELS_CHANGED);
	Core_Notify(EV_LAYER_CONFIG);
	return TRUE;
}
BOOL HistoryUndo(void) {
	if (!curr || !curr->prev)
		return FALSE;
	HistNode *oldCurr = curr;
	int oldPos = pos;
	curr = curr->prev;
	pos--;
	if (!ApplyNode(curr)) {
		curr = oldCurr;
		pos = oldPos;
		return FALSE;
	}
	return TRUE;
}
BOOL HistoryRedo(void) {
	if (!curr || !curr->next)
		return FALSE;
	HistNode *oldCurr = curr;
	int oldPos = pos;
	curr = curr->next;
	pos++;
	if (!ApplyNode(curr)) {
		curr = oldCurr;
		pos = oldPos;
		return FALSE;
	}
	return TRUE;
}
void HistoryClear(void) {
	HistoryDestroy();
	HistoryInit();
	Core_Notify(EV_DOC_RESET);
}
BOOL HistoryJumpTo(int idx) {
	if (idx < 0 || idx >= count)
		return FALSE;
	HistNode *n = head;
	for (int i = 0; i < idx && n; i++)
		n = n->next;
	if (!n)
		return FALSE;
	HistNode *oldCurr = curr;
	int oldPos = pos;
	curr = n;
	pos = idx;
	if (!ApplyNode(curr)) {
		curr = oldCurr;
		pos = oldPos;
		return FALSE;
	}
	return TRUE;
}
int HistoryGetPosition(void) {
	return pos;
}
int HistoryGetCount(void) {
	return count;
}
static const char *HistoryGetDescription(int idx) {
	HistNode *n = head;
	for (int i = 0; i < idx && n; i++)
		n = n->next;
	return n ? n->desc : NULL;
}
void HistoryGetDescriptionAt(int idx, char *out, int sz) {
	const char *d = HistoryGetDescription(idx);
	if (d)
		strncpy_s(out, sz, d, _TRUNCATE);
	else if (sz > 0)
		out[0] = '\0';
}
