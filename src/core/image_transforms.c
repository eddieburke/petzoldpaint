#include "peztold_core.h"
#include "canvas.h"
#include "history.h"
#include "layers.h"
#include "pixel_ops.h"
#include "tools/selection_tool.h"
#include "tools.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static BOOL CALLBACK FlipRotateDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
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
static void FlipRawTransform(BYTE *pSrc, int srcW, int srcH, BYTE *pDst, int dstW, int dstH, void *pUserData) {
	BOOL bHorz = (BOOL)(INT_PTR)pUserData;
	memcpy(pDst, pSrc, srcW * srcH * 4);
	PixelOps_Flip(pDst, srcW, srcH, bHorz);
}
static void DoFlip(BOOL bHorz) {
	if (!LayersGetActiveColorBitmap())
		return;
	char desc[64];
	StringCchPrintf(desc, sizeof(desc), "Flip %s", bHorz ? "Horizontal" : "Vertical");
	if (LayersApplyRawTransformToAll(Canvas_GetWidth(), Canvas_GetHeight(), FlipRawTransform, (void *)(INT_PTR)bHorz)) {
		(void)HistoryPush(desc);
		SetDocumentDirty();
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
}
static void RotateRawTransform(BYTE *pSrc, int srcW, int srcH, BYTE *pDst, int dstW, int dstH, void *pUserData) {
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
	if (LayersApplyRawTransformToAll(nNewW, nNewH, RotateRawTransform, (void *)(INT_PTR)degrees)) {
		(void)HistoryPush(desc);
		SetDocumentDirty();
		SendMessage(hMainWnd, WM_SIZE, 0, 0);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
}
void ImageFlipRotate(HWND hWnd) {
	BOOL hadSelection = IsSelectionActive();
	if (!hadSelection)
		Tool_FinalizeCurrentState();
	INT_PTR result = DialogBox(hInst, MAKEINTRESOURCE(IDD_FLIPROTATE), hWnd, (DLGPROC)FlipRotateDlgProc);
	if (result <= 0)
		return;
	if (hadSelection) {
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
typedef struct {
	int resH, resV;
	int skewH, skewV;
} TRANSFORM_PARAMS;
typedef struct {
	int nNewW, nNewH;
	int offX, offY;
} SkewParams;
static void SkewRawTransform(BYTE *pSrc, int srcW, int srcH, BYTE *pDst, int dstW, int dstH, void *pUserData) {
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
static BOOL CALLBACK ResizeSkewDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
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
void ImageResizeSkew(HWND hWnd) {
	Tool_FinalizeCurrentState();
	TRANSFORM_PARAMS
	params;
	if (DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_RESIZESKEW), hWnd, (DLGPROC)ResizeSkewDlgProc, (LPARAM)&params)) {
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
		if (!Canvas_IsValidSize(finalW, finalH))
			return;
		SkewParams skewParams = {nNewW, nNewH, offX, offY};
		char desc[64];
		StringCchPrintf(desc, sizeof(desc), "Resize & Skew: %dx%d → %dx%d", Canvas_GetWidth(), Canvas_GetHeight(), finalW, finalH);
		if (LayersApplyRawTransformToAll(finalW, finalH, SkewRawTransform, &skewParams)) {
			(void)HistoryPush(desc);
			SetDocumentDirty();
			SendMessage(hMainWnd, WM_SIZE, 0, 0);
			InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		}
	}
}
static BOOL CALLBACK AttributesDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_INITDIALOG:
		SetDlgItemInt(hDlg, IDC_WIDTH, Canvas_GetWidth(), FALSE);
		SetDlgItemInt(hDlg, IDC_HEIGHT, Canvas_GetHeight(), FALSE);
		CheckRadioButton(hDlg, IDC_UNITS_INCHES, IDC_UNITS_PIXELS, IDC_UNITS_PIXELS);
		CheckRadioButton(hDlg, IDC_COLORS_BW, IDC_COLORS_COLOR, IDC_COLORS_COLOR);
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			int w = GetDlgItemInt(hDlg, IDC_WIDTH, NULL, FALSE);
			int h = GetDlgItemInt(hDlg, IDC_HEIGHT, NULL, FALSE);
			if (w > 0 && h > 0) {
				if (ResizeCanvas(w, h)) {
					EndDialog(hDlg, 1);
				}
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
void ImageAttributes(HWND hWnd) {
	Tool_FinalizeCurrentState();
	int oldW = Canvas_GetWidth();
	int oldH = Canvas_GetHeight();
	if (DialogBox(hInst, MAKEINTRESOURCE(IDD_ATTRIBUTES), hWnd, (DLGPROC)AttributesDlgProc)) {
		if (Canvas_GetWidth() != oldW || Canvas_GetHeight() != oldH) {
			char desc[64];
			StringCchPrintf(desc, sizeof(desc), "Resize Canvas: %dx%d → %dx%d", oldW, oldH, Canvas_GetWidth(), Canvas_GetHeight());
			SetDocumentDirty();
			Core_Notify(EV_LAYER_CONFIG);
			(void)HistoryPush(desc);
		}
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		SendMessage(hMainWnd, WM_SIZE, 0, 0);
	}
}
void ImageInvertColors(HWND hWnd) {
	if (IsSelectionActive()) {
		SelectionInvertColors();
		return;
	}
	Tool_FinalizeCurrentState();
	if (!LayersGetActiveColorBitmap())
		return;
	Layers_BeginWrite();
	BYTE *pBits = LayersGetActiveColorBits();
	if (!pBits)
		return;
	PixelOps_InvertColors(pBits, Canvas_GetWidth(), Canvas_GetHeight());
	LayersMarkDirty();
	(void)HistoryPush("Invert Colors");
	SetDocumentDirty();
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
void ImageClear(HWND hWnd) {
	(void)hWnd;
	Tool_FinalizeCurrentState();
	/* Clear to fully transparent — secondary palette color is a drawing color,
	   not a canvas background. The checkerboard composite shows the result. */
	BYTE *bits = Layers_BeginWrite();
	if (!bits)
		return;
	int ww = Canvas_GetWidth(), hh = Canvas_GetHeight();
	memset(bits, 0, (size_t)ww * (size_t)hh * 4);
	LayersMarkDirty();
	(void)HistoryPush("Clear Image");
	SetDocumentDirty();
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
