/*------------------------------------------------------------
   IMAGE_TRANSFORMS.C -- Image Transformation Operations

   This module provides image transformation functions including
   flip, rotate, resize, skew, invert colors, and attributes.
  ------------------------------------------------------------*/

#include "peztold_core.h"
#include "image_transforms.h"
#include "canvas.h"
#include "history.h"
#include "layers.h"
#include "pixel_ops.h"
#include "tools/selection_tool.h"
#include "ui/panels/layers_panel.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*------------------------------------------------------------
   Flip/Rotate Dialog Procedure
  ------------------------------------------------------------*/

static BOOL CALLBACK FlipRotateDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG:
    CheckRadioButton(hDlg, IDC_FLIP_HORZ, IDC_ROTATE_90, IDC_FLIP_HORZ);
    EnableWindow(GetDlgItem(hDlg, IDC_ROTATE_90), FALSE);
    return TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_FLIP_HORZ || LOWORD(wParam) == IDC_FLIP_VERT) {
      EnableWindow(GetDlgItem(hDlg, IDC_ROTATE_180), FALSE);
      EnableWindow(GetDlgItem(hDlg, IDC_ROTATE_270), FALSE);
    } else if (LOWORD(wParam) == IDC_ROTATE_90) {
      EnableWindow(GetDlgItem(hDlg, IDC_ROTATE_180), TRUE);
      EnableWindow(GetDlgItem(hDlg, IDC_ROTATE_270), TRUE);
    }

    switch (LOWORD(wParam)) {
    case IDOK:
      if (IsDlgButtonChecked(hDlg, IDC_FLIP_HORZ))
        EndDialog(hDlg, 1);
      else if (IsDlgButtonChecked(hDlg, IDC_FLIP_VERT))
        EndDialog(hDlg, 2);
      else if (IsDlgButtonChecked(hDlg, IDC_ROTATE_90))
        EndDialog(hDlg, 3);
      else if (IsDlgButtonChecked(hDlg, IDC_ROTATE_180))
        EndDialog(hDlg, 4);
      else if (IsDlgButtonChecked(hDlg, IDC_ROTATE_270))
        EndDialog(hDlg, 5);
      return TRUE;

    case IDCANCEL:
      EndDialog(hDlg, 0);
      return TRUE;
    }
    break;
  }
  return FALSE;
}

/*------------------------------------------------------------
   Flip Transform Function
  ------------------------------------------------------------*/

static void FlipRawTransform(BYTE *pSrc, int srcW, int srcH, BYTE *pDst,
                             int dstW, int dstH, void *pUserData) {
  BOOL bHorz = (BOOL)(INT_PTR)pUserData;
  // For flip, dst bit set is initially empty or needs a copy if we flip in
  // place. LayersApplyRawTransformToAll gives us a fresh pDst.
  memcpy(pDst, pSrc, srcW * srcH * 4);
  PixelOps_Flip(pDst, srcW, srcH, bHorz);
}

static void DoFlip(BOOL bHorz) {
  if (!LayersGetActiveColorBitmap())
    return;

  char desc[64];
  StringCchPrintf(desc, sizeof(desc), "Flip %s", bHorz ? "Horizontal" : "Vertical");
  if (LayersApplyRawTransformToAll(Canvas_GetWidth(), Canvas_GetHeight(), FlipRawTransform,
                                   (void *)(INT_PTR)bHorz)) {
    HistoryPush(desc);
    SetDocumentDirty();
    LayersPanelSync();
    InvalidateCanvas();
  }
}

/*------------------------------------------------------------
   Rotate Transform Function
  ------------------------------------------------------------*/

static void RotateRawTransform(BYTE *pSrc, int srcW, int srcH, BYTE *pDst,
                               int dstW, int dstH, void *pUserData) {
  int degrees = (int)(INT_PTR)pUserData;
  DWORD *src = (DWORD *)pSrc;
  DWORD *dst = (DWORD *)pDst;

  for (int y = 0; y < dstH; y++) {
    for (int x = 0; x < dstW; x++) {
      int sx, sy;
      if (degrees == 90) {
        sx = y;
        sy = srcH - 1 - x;
      } else if (degrees == 180) {
        sx = srcW - 1 - x;
        sy = srcH - 1 - y;
      } else if (degrees == 270) {
        sx = srcW - 1 - y;
        sy = x;
      } else {
        sx = x;
        sy = y;
      }
      if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH) {
        dst[y * dstW + x] = src[sy * srcW + sx];
      }
    }
  }
}

static void DoRotate(int degrees) {
  if (!LayersGetActiveColorBitmap())
    return;

  int nNewW = (degrees == 90 || degrees == 270) ? Canvas_GetHeight() : Canvas_GetWidth();
  int nNewH = (degrees == 90 || degrees == 270) ? Canvas_GetWidth() : Canvas_GetHeight();

  char desc[64];
  StringCchPrintf(desc, sizeof(desc), "Rotate %d Degrees", degrees);
  if (LayersApplyRawTransformToAll(nNewW, nNewH, RotateRawTransform,
                                   (void *)(INT_PTR)degrees)) {
    HistoryPush(desc);
    SetDocumentDirty();
    LayersPanelSync();
    SendMessage(hMainWnd, WM_SIZE, 0, 0);
    InvalidateCanvas();
  }
}

/*------------------------------------------------------------
   ImageFlipRotate

   Displays the flip/rotate dialog and applies the transformation.
  ------------------------------------------------------------*/

void ImageFlipRotate(HWND hWnd) {
  INT_PTR result = DialogBox(hInst, MAKEINTRESOURCE(IDD_FLIPROTATE), hWnd,
                             (DLGPROC)FlipRotateDlgProc);
  if (result <= 0)
    return;

  if (IsSelectionActive()) {
    if (result == 1)
      SelectionFlip(TRUE);
    else if (result == 2)
      SelectionFlip(FALSE);
    else if (result == 3)
      SelectionRotate(90);
    else if (result == 4)
      SelectionRotate(180);
    else if (result == 5)
      SelectionRotate(270);
  } else {
    if (result == 1)
      DoFlip(TRUE);
    else if (result == 2)
      DoFlip(FALSE);
    else if (result == 3)
      DoRotate(90);
    else if (result == 4)
      DoRotate(180);
    else if (result == 5)
      DoRotate(270);
  }
}

/*------------------------------------------------------------
   Resize/Skew Transform Structures
  ------------------------------------------------------------*/

typedef struct {
  int resH, resV;
  int skewH, skewV;
} TRANSFORM_PARAMS;

typedef struct {
  int nNewW, nNewH;
  int offX, offY;
} SkewParams;

/*------------------------------------------------------------
   Skew Transform Function
  ------------------------------------------------------------*/

static void SkewRawTransform(BYTE *pSrc, int srcW, int srcH, BYTE *pDst,
                             int dstW, int dstH, void *pUserData) {
  SkewParams *p = (SkewParams *)pUserData;
  int nNewW = p->nNewW;
  int nNewH = p->nNewH;
  int offX = p->offX;
  int offY = p->offY;

  DWORD *src = (DWORD *)pSrc;
  DWORD *dst = (DWORD *)pDst;

  int startX = (offX < 0) ? -offX : 0;
  int startY = (offY < 0) ? -offY : 0;

  double Ax = startX + offX;
  double Ay = startY;
  double Bx = startX + offX + nNewW;
  double By = startY + offY;
  double Cx = startX;
  double Cy = startY + nNewH;

  double det = (Bx - Ax) * (Cy - Ay) - (By - Ay) * (Cx - Ax);
  if (fabs(det) < 0.0001)
    return;

  for (int y = 0; y < dstH; y++) {
    for (int x = 0; x < dstW; x++) {
      double dx = (double)x - Ax;
      double dy = (double)y - Ay;

      double u = (dx * (Cy - Ay) - dy * (Cx - Ax)) / det;
      double v = ((Bx - Ax) * dy - (By - Ay) * dx) / det;

      int sx = (int)round(u * srcW);
      int sy = (int)round(v * srcH);

      if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH) {
        dst[y * dstW + x] = src[sy * srcW + sx];
      }
    }
  }
}

/*------------------------------------------------------------
   Resize/Skew Dialog Procedure
  ------------------------------------------------------------*/

static BOOL CALLBACK ResizeSkewDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
  static TRANSFORM_PARAMS *pParams;

  switch (message) {
  case WM_INITDIALOG:
    pParams = (TRANSFORM_PARAMS *)lParam;
    SetDlgItemInt(hDlg, IDC_RESIZE_HORZ, 100, FALSE);
    SetDlgItemInt(hDlg, IDC_RESIZE_VERT, 100, FALSE);
    SetDlgItemInt(hDlg, IDC_SKEW_HORZ, 0, TRUE);
    SetDlgItemInt(hDlg, IDC_SKEW_VERT, 0, TRUE);
    return TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      pParams->resH = GetDlgItemInt(hDlg, IDC_RESIZE_HORZ, NULL, FALSE);
      pParams->resV = GetDlgItemInt(hDlg, IDC_RESIZE_VERT, NULL, FALSE);
      pParams->skewH = GetDlgItemInt(hDlg, IDC_SKEW_HORZ, NULL, TRUE);
      pParams->skewV = GetDlgItemInt(hDlg, IDC_SKEW_VERT, NULL, TRUE);
      EndDialog(hDlg, 1);
      return TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, 0);
      return TRUE;
    }
    break;
  }
  return FALSE;
}

/*------------------------------------------------------------
   ImageResizeSkew

   Displays the resize/skew dialog and applies the transformation.
  ------------------------------------------------------------*/

void ImageResizeSkew(HWND hWnd) {
  TRANSFORM_PARAMS params;
  if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_RESIZESKEW), hWnd,
                     (DLGPROC)ResizeSkewDlgProc, (LPARAM)&params)) {
    int nNewW = (int)(Canvas_GetWidth() * params.resH / 100.0);
    int nNewH = (int)(Canvas_GetHeight() * params.resV / 100.0);
    if (nNewW < 1)
      nNewW = 1;
    if (nNewH < 1)
      nNewH = 1;

    double radH = params.skewH * M_PI / 180.0;
    double radV = params.skewV * M_PI / 180.0;
    int offX = (int)(nNewH * tan(radH));
    int offY = (int)(nNewW * tan(radV));
    int finalW = nNewW + abs(offX);
    int finalH = nNewH + abs(offY);

    SkewParams skewParams = {nNewW, nNewH, offX, offY};

    char desc[64];
    StringCchPrintf(desc, sizeof(desc), "Resize & Skew: %dx%d → %dx%d", Canvas_GetWidth(), Canvas_GetHeight(),
            finalW, finalH);
    if (LayersApplyRawTransformToAll(finalW, finalH, SkewRawTransform,
                                     &skewParams)) {
      HistoryPush(desc);
      SetDocumentDirty();
      LayersPanelSync();
      SendMessage(hMainWnd, WM_SIZE, 0, 0);
      InvalidateCanvas();
    }
  }
}

/*------------------------------------------------------------
   Attributes Dialog Procedure
  ------------------------------------------------------------*/

static BOOL CALLBACK AttributesDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG:
    SetDlgItemInt(hDlg, IDC_WIDTH, Canvas_GetWidth(), FALSE);
    SetDlgItemInt(hDlg, IDC_HEIGHT, Canvas_GetHeight(), FALSE);
    CheckRadioButton(hDlg, IDC_UNITS_INCHES, IDC_UNITS_PIXELS,
                     IDC_UNITS_PIXELS);
    CheckRadioButton(hDlg, IDC_COLORS_BW, IDC_COLORS_COLOR, IDC_COLORS_COLOR);
    return TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      int w = GetDlgItemInt(hDlg, IDC_WIDTH, NULL, FALSE);
      int h = GetDlgItemInt(hDlg, IDC_HEIGHT, NULL, FALSE);
      if (w > 0 && h > 0) {
        ResizeCanvas(w, h);
        EndDialog(hDlg, 1);
      }
      return TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, 0);
      return TRUE;
    } else if (LOWORD(wParam) == 1025) {
      SetDlgItemInt(hDlg, IDC_WIDTH, 800, FALSE);
      SetDlgItemInt(hDlg, IDC_HEIGHT, 600, FALSE);
      return TRUE;
    }
    break;
  }
  return FALSE;
}

/*------------------------------------------------------------
   ImageAttributes

   Displays the image attributes dialog and resizes the canvas.
  ------------------------------------------------------------*/

void ImageAttributes(HWND hWnd) {
  int oldW = Canvas_GetWidth();
  int oldH = Canvas_GetHeight();
  if (DialogBox(hInst, MAKEINTRESOURCE(IDD_ATTRIBUTES), hWnd,
                (DLGPROC)AttributesDlgProc)) {
    if (Canvas_GetWidth() != oldW || Canvas_GetHeight() != oldH) {
      char desc[64];
      StringCchPrintf(desc, sizeof(desc), "Resize Canvas: %dx%d → %dx%d", oldW, oldH, Canvas_GetWidth(),
              Canvas_GetHeight());

      SetDocumentDirty();
      extern void LayersPanelSync(void);
      LayersPanelSync();

      // Push undo state AFTER resize is complete
      HistoryPush(desc);
    }
    InvalidateCanvas();
    SendMessage(hMainWnd, WM_SIZE, 0, 0);
  }
}

/*------------------------------------------------------------
   ImageInvertColors

   Inverts the colors of the image or selection.
  ------------------------------------------------------------*/

void ImageInvertColors(HWND hWnd) {
  if (IsSelectionActive()) {
    SelectionInvertColors();
    return;
  }

  if (!LayersGetActiveColorBitmap())
    return;

  BYTE *pBits = LayersGetActiveColorBits();
  if (!pBits)
    return;

  PixelOps_InvertColors(pBits, Canvas_GetWidth(), Canvas_GetHeight());

  LayersMarkDirty();

  HistoryPush("Invert Colors");
  SetDocumentDirty();
  InvalidateCanvas();
}

/*------------------------------------------------------------
   ImageClear

   Clears the entire canvas to the background color.
  ------------------------------------------------------------*/

void ImageClear(HWND hWnd) {
  ClearCanvas(Palette_GetSecondaryColor());
  HistoryPush("Clear Image");
  SetDocumentDirty();
  InvalidateCanvas();
}
