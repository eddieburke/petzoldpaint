#ifndef COMMIT_BAR_H
#define COMMIT_BAR_H

#include "overlay.h"
#include <windows.h>

#define COMMIT_BAR_HIT_NONE 0
#define COMMIT_BAR_HIT_COMMIT 1
#define COMMIT_BAR_HIT_CANCEL 2

BOOL CommitBar_IsEnabled(void);
void CommitBar_SetEnabled(BOOL enabled);

void CommitBar_Draw(const OverlayContext *ctx, const RECT *targetBmpRect);
int CommitBar_HitTest(const RECT *targetBmpRect, int xBmp, int yBmp);

#define COMMIT_BAR_HANDLE_CLICK(targetBmpRect, xBmp, yBmp, commitCall, cancelCall) \
  do {                                                                            \
    int _commitBarHit = CommitBar_HitTest((targetBmpRect), (xBmp), (yBmp));       \
    if (_commitBarHit == COMMIT_BAR_HIT_COMMIT) {                                 \
      commitCall;                                                                 \
      return;                                                                     \
    }                                                                             \
    if (_commitBarHit == COMMIT_BAR_HIT_CANCEL) {                                 \
      cancelCall;                                                                 \
      return;                                                                     \
    }                                                                             \
  } while (0)

#endif
