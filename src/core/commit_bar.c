#include "commit_bar.h"
#include "geom.h"
#include "gdi_utils.h"

static BOOL s_commitBarEnabled = TRUE;

static void CommitBar_GetRects(const RECT *targetBmpRect, RECT *outCommitScr,
                               RECT *outCancelScr) {
  const int btnSize = 16;
  const int gap = 4;
  const int margin = 8;

  int leftScr = 0, topScr = 0;
  int rightScr = 0, bottomScr = 0;
  CoordBmpToScr(targetBmpRect->left, targetBmpRect->top, &leftScr, &topScr);
  CoordBmpToScr(targetBmpRect->right, targetBmpRect->bottom, &rightScr,
                &bottomScr);

  if (rightScr < leftScr) {
    int t = leftScr;
    leftScr = rightScr;
    rightScr = t;
  }
  if (bottomScr < topScr) {
    int t = topScr;
    topScr = bottomScr;
    bottomScr = t;
  }

  int barX = rightScr + margin;
  int barY = topScr;
  if (barY < 0)
    barY = 0;
  if (barY + (btnSize * 2 + gap) > bottomScr + margin) {
    barY = bottomScr + margin;
  }

  SetRect(outCommitScr, barX, barY, barX + btnSize, barY + btnSize);
  SetRect(outCancelScr, barX, barY + btnSize + gap, barX + btnSize,
          barY + btnSize + gap + btnSize);
}

BOOL CommitBar_IsEnabled(void) { return s_commitBarEnabled; }

void CommitBar_SetEnabled(BOOL enabled) {
  s_commitBarEnabled = enabled ? TRUE : FALSE;
}

void CommitBar_Draw(const OverlayContext *ctx, const RECT *targetBmpRect) {
  if (!ctx || !targetBmpRect || !s_commitBarEnabled)
    return;

  RECT rcCommit, rcCancel;
  CommitBar_GetRects(targetBmpRect, &rcCommit, &rcCancel);

  HBRUSH hOldBrush = NULL;
  HPEN hOldPen = NULL;

  HBRUSH hCommitBr =
      CreateBrushAndSelect(ctx->hdc, RGB(228, 248, 228), &hOldBrush);
  HPEN hCommitPen =
      CreatePenAndSelect(ctx->hdc, PS_SOLID, 1, RGB(32, 128, 32), &hOldPen);
  Rectangle(ctx->hdc, rcCommit.left, rcCommit.top, rcCommit.right,
            rcCommit.bottom);
  MoveToEx(ctx->hdc, rcCommit.left + 4, rcCommit.top + 9, NULL);
  LineTo(ctx->hdc, rcCommit.left + 7, rcCommit.top + 12);
  LineTo(ctx->hdc, rcCommit.left + 12, rcCommit.top + 4);
  RestorePen(ctx->hdc, hOldPen);
  SelectObject(ctx->hdc, hOldBrush);
  Gdi_DeleteBrush(hCommitBr);
  Gdi_DeletePen(hCommitPen);

  HBRUSH hCancelBr =
      CreateBrushAndSelect(ctx->hdc, RGB(252, 230, 230), &hOldBrush);
  HPEN hCancelPen =
      CreatePenAndSelect(ctx->hdc, PS_SOLID, 1, RGB(176, 48, 48), &hOldPen);
  Rectangle(ctx->hdc, rcCancel.left, rcCancel.top, rcCancel.right,
            rcCancel.bottom);
  MoveToEx(ctx->hdc, rcCancel.left + 4, rcCancel.top + 4, NULL);
  LineTo(ctx->hdc, rcCancel.left + 12, rcCancel.top + 12);
  MoveToEx(ctx->hdc, rcCancel.left + 12, rcCancel.top + 4, NULL);
  LineTo(ctx->hdc, rcCancel.left + 4, rcCancel.top + 12);
  RestorePen(ctx->hdc, hOldPen);
  SelectObject(ctx->hdc, hOldBrush);
  Gdi_DeleteBrush(hCancelBr);
  Gdi_DeletePen(hCancelPen);
}

int CommitBar_HitTest(const RECT *targetBmpRect, int xBmp, int yBmp) {
  if (!targetBmpRect || !s_commitBarEnabled)
    return COMMIT_BAR_HIT_NONE;

  int xScr = 0, yScr = 0;
  CoordBmpToScr(xBmp, yBmp, &xScr, &yScr);

  RECT rcCommit, rcCancel;
  CommitBar_GetRects(targetBmpRect, &rcCommit, &rcCancel);
  POINT pt = {xScr, yScr};
  if (PtInRect(&rcCommit, pt))
    return COMMIT_BAR_HIT_COMMIT;
  if (PtInRect(&rcCancel, pt))
    return COMMIT_BAR_HIT_CANCEL;
  return COMMIT_BAR_HIT_NONE;
}
