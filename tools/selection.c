/*------------------------------------------------------------------------------
 * SELECTION.C
 *
 * Consolidated Selection Subsystem
 *
 * This module integrates tool logic, buffer management, transforms, and
 * clipboard I/O into a single, cohesive unit. This eliminates redundant
 * abstractions like SelectionBuffer and keeps selection state flat and direct.
 *----------------------------------------------------------------------------*/

#include "selection_tool.h"
#include "../canvas.h"
#include "../peztold_core.h"
#include "../draw.h"
#include "../geom.h"
#include "../helpers.h"
#include "../history.h"
#include "../layers.h"
#include "../overlay.h"
#include "../commit_bar.h"
#include "../poly_store.h"
#include "../file_io.h"
#include "tool_options/tool_options.h"
#include "../palette.h"
#include "tools.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wincodec.h>
#include <objbase.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Selection State
// ============================================================================

typedef enum {
    SEL_NONE,
    SEL_REGION_ONLY,
    SEL_FLOATING
} SelectionMode;

typedef struct {
    HBITMAP hFloatBmp;
    BYTE* pFloatBits;
    int nWidth, nHeight;
    
    // Backup for cancel
    HBITMAP hBackupBmp;
    BYTE* pBackupBits;
    HRGN hBackupRegion;
    RECT rcLiftOrigin;
} SelectionPixels;

typedef struct {
    double fAngle;
    POINT ptCenter;
    double fStartAngle;
    double fLastAngle;
} SelectionRotation;

typedef struct {
    SelectionMode mode;
    HRGN hRegion;
    RECT rcBounds;
    PolyStore freeformPts;
    
    SelectionPixels pixels;
    SelectionRotation rot;
    
    int nDragMode;
    POINT ptDragStart;
    POINT ptDragLast;
    RECT rcDragOrig;
} SelectionState;

static SelectionState s_sel = {0};
static SelectionMode s_modeAtDragStart = SEL_NONE;
static BOOL s_dragMoved = FALSE;

// Forward declarations
static void UpdateSelectionDraftLayer(void);
static void SelectionHelpers_RestoreCanvas(void);
static void SelectionHelpers_SampleRotated(BYTE* pSrcBits, int srcW, int srcH,
                                          BYTE* pDstBits, int dstW, int dstH,
                                          double angleDegrees, double centerX, double centerY,
                                          int dstStartX, int dstStartY, int dstEndX, int dstEndY,
                                          int canvasWidth, int canvasHeight);
static void Selection_ComputeRotatedSampleExtents(const RECT *rcBounds,
                                                  double angleDeg, int *pDstW,
                                                  int *pDstH, double *pCx,
                                                  double *pCy, int *pSx, int *pSy,
                                                  int *pEx, int *pEy);
static void Selection_GetOverlayFrameRect(RECT *out);

// ============================================================================
// State Management
// ============================================================================

void SelectionClearState(void) {
    if (s_sel.hRegion) DeleteObject(s_sel.hRegion);
    if (s_sel.pixels.hFloatBmp) DeleteObject(s_sel.pixels.hFloatBmp);
    if (s_sel.pixels.hBackupBmp) DeleteObject(s_sel.pixels.hBackupBmp);
    if (s_sel.pixels.hBackupRegion) DeleteObject(s_sel.pixels.hBackupRegion);
    
    Poly_Free(&s_sel.freeformPts);

    memset(&s_sel, 0, sizeof(SelectionState));
    s_sel.mode = SEL_NONE;
    s_sel.nDragMode = HT_NONE;
}

BOOL IsSelectionActive(void) { return s_sel.mode != SEL_NONE; }

BOOL SelectionIsDragging(void) { return s_sel.nDragMode != HT_NONE; }

// ============================================================================
// Lifting Pixels
// ============================================================================

static void SelectionHelpers_BackupCanvas(void) {
    if (s_sel.pixels.hBackupBmp) DeleteObject(s_sel.pixels.hBackupBmp);
    s_sel.pixels.hBackupBmp = CopyBitmapToDib32(LayersGetActiveColorBitmap(), 
                                               Canvas_GetWidth(), Canvas_GetHeight(), 
                                               &s_sel.pixels.pBackupBits);
    
    if (s_sel.pixels.hBackupRegion) DeleteObject(s_sel.pixels.hBackupRegion);
    s_sel.pixels.hBackupRegion = NULL;
    if (s_sel.hRegion) {
        s_sel.pixels.hBackupRegion = CreateRectRgn(0, 0, 0, 0);
        CombineRgn(s_sel.pixels.hBackupRegion, s_sel.hRegion, NULL, RGN_COPY);
    }
    s_sel.pixels.rcLiftOrigin = s_sel.rcBounds;
}

static void LiftSelectionPixels(void) {
    if (s_sel.mode != SEL_REGION_ONLY) return;

    int w = s_sel.rcBounds.right - s_sel.rcBounds.left;
    int h = s_sel.rcBounds.bottom - s_sel.rcBounds.top;
    if (w <= 0 || h <= 0) return;

    SelectionHelpers_BackupCanvas();
    if (!s_sel.pixels.hBackupBmp) return;

    s_sel.pixels.hFloatBmp = CreateDibSection32(w, h, &s_sel.pixels.pFloatBits);
    if (!s_sel.pixels.hFloatBmp) return;
    s_sel.pixels.nWidth = w;
    s_sel.pixels.nHeight = h;

    HBITMAP hOldCanvas;
    HDC hCanvas = GetCanvasBitmapDC(&hOldCanvas);
    if (!hCanvas)
      return;

    HDC hdcScreen = GetScreenDC();
    HDC hFloatDC = CreateTempDC(hdcScreen);
    HBITMAP hOldFloat = (HBITMAP)SelectObject(hFloatDC, s_sel.pixels.hFloatBmp);

    // Initialize as fully transparent (never key transparency off secondary color)
    if (s_sel.pixels.pFloatBits) {
        memset(s_sel.pixels.pFloatBits, 0, (size_t)w * (size_t)h * 4u);
    }

    // Copy clipped region or rectangle
    if (s_sel.hRegion) {
        HRGN hClipRgn = CreateRectRgn(0, 0, 0, 0);
        CombineRgn(hClipRgn, s_sel.hRegion, NULL, RGN_COPY);
        OffsetRgn(hClipRgn, -s_sel.rcBounds.left, -s_sel.rcBounds.top);
        SelectClipRgn(hFloatDC, hClipRgn);
        BitBlt(hFloatDC, 0, 0, w, h, hCanvas, s_sel.rcBounds.left, s_sel.rcBounds.top, SRCCOPY);
        SelectClipRgn(hFloatDC, NULL);
        DeleteObject(hClipRgn);
    } else {
        BitBlt(hFloatDC, 0, 0, w, h, hCanvas, s_sel.rcBounds.left, s_sel.rcBounds.top, SRCCOPY);
    }

    SelectObject(hFloatDC, hOldFloat);
    DeleteTempDC(hFloatDC);
    ReleaseScreenDC(hdcScreen);
    ReleaseCanvasBitmapDC(hCanvas, hOldCanvas);

    Layers_BeginWrite();
    BYTE *canvasBits = LayersGetActiveColorBits();

    // Transfer alpha and clear source
    if (canvasBits) {
        int cw = Canvas_GetWidth();
        for (int y = 0; y < h; y++) {
            int cy = s_sel.rcBounds.top + y;
            if (cy < 0 || cy >= Canvas_GetHeight()) continue;
            for (int x = 0; x < w; x++) {
                int cx = s_sel.rcBounds.left + x;
                if (cx < 0 || cx >= cw) continue;

                if (s_sel.hRegion && !PtInRegion(s_sel.hRegion, cx, cy)) continue;

                int cIdx = (cy * cw + cx) * 4;
                int fIdx = (y * w + x) * 4;
                s_sel.pixels.pFloatBits[fIdx + 3] = canvasBits[cIdx + 3];

                // Clear from canvas
                *(DWORD*)(canvasBits + cIdx) = 0;
            }
        }
    }

    s_sel.rot.fAngle = 0.0;
    s_sel.mode = SEL_FLOATING;
    UpdateCanvasAfterModification();
    UpdateSelectionDraftLayer();
}

// ============================================================================
// Commitment & Cancellation
// ============================================================================

static void SelectionHelpers_RestoreCanvas(void) {
    if (!s_sel.pixels.hBackupBmp || !s_sel.pixels.pBackupBits) return;
    BYTE* colorBits = LayersGetActiveColorBits();
    if (!colorBits) return;

    int cw = Canvas_GetWidth();
    int ch = Canvas_GetHeight();
    RECT rc = s_sel.pixels.rcLiftOrigin;

    for (int y = rc.top; y < rc.bottom; y++) {
        if (y < 0 || y >= ch) continue;
        for (int x = rc.left; x < rc.right; x++) {
            if (x < 0 || x >= cw) continue;
            int idx = (y * cw + x) * 4;
            *(DWORD*)(colorBits + idx) = *(DWORD*)(s_sel.pixels.pBackupBits + idx);
        }
    }
}

static void Selection_ComputeRotatedSampleExtents(const RECT *rcBounds,
                                                  double angleDeg, int *pDstW,
                                                  int *pDstH, double *pCx,
                                                  double *pCy, int *pSx, int *pSy,
                                                  int *pEx, int *pEy) {
    int dstW = rcBounds->right - rcBounds->left;
    int dstH = rcBounds->bottom - rcBounds->top;
    *pDstW = dstW;
    *pDstH = dstH;
    if (dstW <= 0 || dstH <= 0) {
        *pCx = *pCy = 0.0;
        *pSx = *pSy = *pEx = *pEy = 0;
        return;
    }
    double cx = rcBounds->left + dstW / 2.0;
    double cy = rcBounds->top + dstH / 2.0;
    *pCx = cx;
    *pCy = cy;
    double angleRad = angleDeg * M_PI / 180.0;
    double cosA = cos(angleRad), sinA = sin(angleRad);
    double hw = dstW / 2.0, hh = dstH / 2.0;
    double corners[4][2] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    for (int i = 0; i < 4; i++) {
        double rx = corners[i][0] * cosA - corners[i][1] * sinA;
        double ry = corners[i][0] * sinA + corners[i][1] * cosA;
        if (i == 0 || rx < minX) minX = rx;
        if (i == 0 || rx > maxX) maxX = rx;
        if (i == 0 || ry < minY) minY = ry;
        if (i == 0 || ry > maxY) maxY = ry;
    }
    *pSx = (int)floor(cx + minX);
    *pEx = (int)ceil(cx + maxX);
    *pSy = (int)floor(cy + minY);
    *pEy = (int)ceil(cy + maxY);
}

static void Selection_GetOverlayFrameRect(RECT *out) {
    *out = s_sel.rcBounds;
    if (s_sel.mode != SEL_FLOATING || fabs(s_sel.rot.fAngle) <= 0.01)
        return;
    int dstW, dstH, sx, sy, ex, ey;
    double cx, cy;
    Selection_ComputeRotatedSampleExtents(&s_sel.rcBounds, s_sel.rot.fAngle, &dstW,
                                          &dstH, &cx, &cy, &sx, &sy, &ex, &ey);
    SetRect(out, sx, sy, ex, ey);
}

void CommitSelection(void) {
    if (s_sel.mode == SEL_NONE) return;
    if (s_sel.mode == SEL_FLOATING && s_sel.pixels.pFloatBits) {
        Layers_BeginWrite();
        BYTE* pBits = LayersGetActiveColorBits();
        if (pBits) {
            int dstW, dstH, sx, sy, ex, ey;
            double cx, cy;
            Selection_ComputeRotatedSampleExtents(&s_sel.rcBounds, s_sel.rot.fAngle,
                                                  &dstW, &dstH, &cx, &cy, &sx, &sy,
                                                  &ex, &ey);
            if (dstW > 0 && dstH > 0) {
                SelectionHelpers_SampleRotated(s_sel.pixels.pFloatBits, s_sel.pixels.nWidth, s_sel.pixels.nHeight,
                                              pBits, dstW, dstH, s_sel.rot.fAngle, cx, cy,
                                              sx, sy, ex, ey, Canvas_GetWidth(), Canvas_GetHeight());
            }
        }
        UpdateCanvasAfterModification();
        SetDocumentDirty();
        SelectionClearState();
        HistoryPushToolActionById(TOOL_SELECT, "Commit Selection");
    } else {
        SelectionClearState();
        InvalidateCanvas();
    }
}

void CancelSelection(void) {
    if (s_sel.mode == SEL_NONE && s_sel.nDragMode == HT_NONE) return;
    if (s_sel.mode == SEL_FLOATING && s_sel.pixels.hBackupBmp) {
        SelectionHelpers_RestoreCanvas();
        HRGN hOld = s_sel.hRegion;
        s_sel.hRegion = s_sel.pixels.hBackupRegion;
        s_sel.pixels.hBackupRegion = NULL;
        if (hOld) DeleteObject(hOld);
        s_sel.rcBounds = s_sel.pixels.rcLiftOrigin;
        s_sel.mode = SEL_REGION_ONLY;
        memset(&s_sel.rot, 0, sizeof(s_sel.rot));
        if (s_sel.pixels.hFloatBmp) {
            DeleteObject(s_sel.pixels.hFloatBmp);
            s_sel.pixels.hFloatBmp = NULL;
        }
        if (s_sel.pixels.hBackupBmp) {
            DeleteObject(s_sel.pixels.hBackupBmp);
            s_sel.pixels.hBackupBmp = NULL;
        }
        s_sel.pixels.pFloatBits = NULL;
        s_sel.pixels.pBackupBits = NULL;
        LayersClearDraft();
        UpdateCanvasAfterModification();
        return;
    }
    SelectionClearState();
    LayersClearDraft();
    InvalidateCanvas();
}

// ============================================================================
// Snapshot Support
// ============================================================================

SelectionSnapshot* Selection_CreateSnapshot(void) {
    SelectionSnapshot* s = (SelectionSnapshot*)calloc(1, sizeof(SelectionSnapshot));
    if (!s) return NULL;
    s->mode = (int)s_sel.mode;
    s->rcBounds = s_sel.rcBounds;
    if (s_sel.hRegion) {
        s->hRegion = CreateRectRgn(0, 0, 0, 0);
        CombineRgn(s->hRegion, s_sel.hRegion, NULL, RGN_COPY);
    }
    if (s_sel.pixels.hFloatBmp) {
        s->hFloatBmp = CopyBitmapToDib32(s_sel.pixels.hFloatBmp, s_sel.pixels.nWidth, s_sel.pixels.nHeight, &s->pFloatBits);
        s->nFloatW = s_sel.pixels.nWidth;
        s->nFloatH = s_sel.pixels.nHeight;
    }
    s->fRotationAngle = s_sel.rot.fAngle;
    s->ptRotateCenter = s_sel.rot.ptCenter;
    Poly_Copy(&s->freeformPts, &s_sel.freeformPts);
    return s;
}

void Selection_DestroySnapshot(SelectionSnapshot* s) {
    if (!s) return;
    if (s->hRegion) DeleteObject(s->hRegion);
    if (s->hFloatBmp) DeleteObject(s->hFloatBmp);
    Poly_Free(&s->freeformPts);
    free(s);
}

void Selection_ApplySnapshot(SelectionSnapshot* s) {
    SelectionClearState();
    if (!s) { InvalidateCanvas(); return; }
    s_sel.mode = (SelectionMode)s->mode;
    s_sel.rcBounds = s->rcBounds;
    if (s->hRegion) {
        s_sel.hRegion = CreateRectRgn(0, 0, 0, 0);
        CombineRgn(s_sel.hRegion, s->hRegion, NULL, RGN_COPY);
    }
    if (s->hFloatBmp) {
        s_sel.pixels.hFloatBmp = CopyBitmapToDib32(s->hFloatBmp, s->nFloatW, s->nFloatH, &s_sel.pixels.pFloatBits);
        s_sel.pixels.nWidth = s->nFloatW;
        s_sel.pixels.nHeight = s->nFloatH;
    }
    s_sel.rot.fAngle = s->fRotationAngle;
    s_sel.rot.ptCenter = s->ptRotateCenter;
    Poly_Copy(&s_sel.freeformPts, &s->freeformPts);
    UpdateSelectionDraftLayer();
    InvalidateCanvas();
}

// ============================================================================
// Transforms & Sampling
// ============================================================================

static void SelectionHelpers_SampleRotated(BYTE* pSrcBits, int srcW, int srcH,
                                          BYTE* pDstBits, int dstW, int dstH,
                                          double angleDegrees, double centerX, double centerY,
                                          int dstStartX, int dstStartY, int dstEndX, int dstEndY,
                                          int canvasWidth, int canvasHeight) {
    if (!pSrcBits || !pDstBits || srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return;
    double angleRad = angleDegrees * M_PI / 180.0;
    double cosA = cos(angleRad), sinA = sin(angleRad);
    double hwSrc = srcW / 2.0, hhSrc = srcH / 2.0;
    
    if (dstStartX < 0) dstStartX = 0; if (dstStartY < 0) dstStartY = 0;
    if (dstEndX > canvasWidth) dstEndX = canvasWidth; if (dstEndY > canvasHeight) dstEndY = canvasHeight;
    
    for (int y = dstStartY; y < dstEndY; y++) {
        double dy = (y + 0.5) - centerY;
        for (int x = dstStartX; x < dstEndX; x++) {
            double dx = (x + 0.5) - centerX;
            double unRotX = dx * cosA + dy * sinA;
            double unRotY = -dx * sinA + dy * cosA;
            double srcX = unRotX * (srcW / (double)dstW) + hwSrc;
            double srcY = unRotY * (srcH / (double)dstH) + hhSrc;
            int sx = (int)floor(srcX), sy = (int)floor(srcY);
            if (sx >= 0 && sx < srcW && sy >= 0 && sy < srcH) {
                int srcIdx = (sy * srcW + sx) * 4;
                BYTE sb = pSrcBits[srcIdx + 0], sg = pSrcBits[srcIdx + 1], sr = pSrcBits[srcIdx + 2], sa = pSrcBits[srcIdx + 3];
                if (sa == 0) continue;
                int canvasIdx = (y * canvasWidth + x) * 4;
                PixelOps_BlendPixel((int)sr, (int)sg, (int)sb, (int)sa, pDstBits + canvasIdx, 0);
            }
        }
    }
}

static void SelectionFlipInternal(BOOL bHorz) {
    if (s_sel.mode != SEL_FLOATING || !s_sel.pixels.pFloatBits) return;
    Transform_Flip(s_sel.pixels.pFloatBits, s_sel.pixels.nWidth, s_sel.pixels.nHeight, bHorz);
    UpdateSelectionDraftLayer();
}

void SelectionRotate(int degrees) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    if (s_sel.mode != SEL_FLOATING) return;
    s_sel.rot.fAngle += degrees;
    UpdateSelectionDraftLayer();
    HistoryPushToolSessionById(TOOL_SELECT, "Adjust Selection");
}

void SelectionFlip(BOOL bHorz) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    if (s_sel.mode != SEL_FLOATING) return;
    SelectionFlipInternal(bHorz);
    HistoryPushToolSessionById(TOOL_SELECT, "Adjust Selection");
}

static void UpdateSelectionDraftLayer(void) {
    if (s_sel.mode != SEL_FLOATING || !s_sel.pixels.pFloatBits) { LayersClearDraft(); return; }
    BYTE *draftBits = LayersGetDraftBits();
    if (!draftBits) return;
    LayersClearDraft();
    int dstW, dstH, sx, sy, ex, ey;
    double cx, cy;
    Selection_ComputeRotatedSampleExtents(&s_sel.rcBounds, s_sel.rot.fAngle, &dstW, &dstH,
                                        &cx, &cy, &sx, &sy, &ex, &ey);
    if (dstW > 0 && dstH > 0) {
        SelectionHelpers_SampleRotated(s_sel.pixels.pFloatBits, s_sel.pixels.nWidth, s_sel.pixels.nHeight,
                                      draftBits, dstW, dstH, s_sel.rot.fAngle, cx, cy,
                                      sx, sy, ex, ey, Canvas_GetWidth(), Canvas_GetHeight());
    }
    LayersMarkDirty();
}

// ============================================================================
// Clipboard Operations
// ============================================================================

static UINT uPngClipboardFormat = 0;

static BOOL SelectionHelpers_EnsureWic(void) {
    return FileIO_GetWicFactory() != NULL;
}

static HGLOBAL SelectionHelpers_ExportPng(void) {
    if (!s_sel.pixels.pFloatBits || !SelectionHelpers_EnsureWic()) return NULL;
    IWICImagingFactory* factory = FileIO_GetWicFactory();
    IStream* pStream = NULL; IWICBitmapEncoder* pEncoder = NULL; IWICBitmapFrameEncode* pFrame = NULL;
    HGLOBAL hMem = NULL;
    if (!factory || FAILED(CreateStreamOnHGlobal(NULL, FALSE, &pStream))) return NULL;
    if (SUCCEEDED(factory->lpVtbl->CreateEncoder(factory, &GUID_ContainerFormatPng, NULL, &pEncoder))) {
        if (SUCCEEDED(pEncoder->lpVtbl->Initialize(pEncoder, pStream, WICBitmapEncoderNoCache))) {
            if (SUCCEEDED(pEncoder->lpVtbl->CreateNewFrame(pEncoder, &pFrame, NULL))) {
                pFrame->lpVtbl->Initialize(pFrame, NULL);
                pFrame->lpVtbl->SetSize(pFrame, s_sel.pixels.nWidth, s_sel.pixels.nHeight);
                WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
                pFrame->lpVtbl->SetPixelFormat(pFrame, &format);
                pFrame->lpVtbl->WritePixels(pFrame, s_sel.pixels.nHeight, s_sel.pixels.nWidth * 4, s_sel.pixels.nWidth * s_sel.pixels.nHeight * 4, s_sel.pixels.pFloatBits);
                pFrame->lpVtbl->Commit(pFrame);
            }
            pEncoder->lpVtbl->Commit(pEncoder);
        }
    }
    GetHGlobalFromStream(pStream, &hMem);
    if (pFrame) pFrame->lpVtbl->Release(pFrame);
    if (pEncoder) pEncoder->lpVtbl->Release(pEncoder);
    if (pStream) pStream->lpVtbl->Release(pStream);
    return hMem;
}

static HGLOBAL SelectionHelpers_ExportDibV5(void) {
    if (!s_sel.pixels.pFloatBits) return NULL;
    int w = s_sel.pixels.nWidth, h = s_sel.pixels.nHeight;
    DWORD imgSize = w * h * 4;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(BITMAPV5HEADER) + imgSize);
    if (!hMem) return NULL;
    BYTE* pData = (BYTE*)GlobalLock(hMem);
    BITMAPV5HEADER* v5 = (BITMAPV5HEADER*)pData;
    v5->bV5Size = sizeof(BITMAPV5HEADER); v5->bV5Width = w; v5->bV5Height = -h; v5->bV5Planes = 1; v5->bV5BitCount = 32;
    v5->bV5Compression = BI_BITFIELDS; v5->bV5RedMask = 0x00FF0000; v5->bV5GreenMask = 0x0000FF00; v5->bV5BlueMask = 0x000000FF; v5->bV5AlphaMask = 0xFF000000;
    v5->bV5CSType = LCS_sRGB; v5->bV5Intent = LCS_GM_IMAGES; v5->bV5SizeImage = imgSize;
    memcpy(pData + sizeof(BITMAPV5HEADER), s_sel.pixels.pFloatBits, imgSize);
    GlobalUnlock(hMem);
    return hMem;
}

void SelectionCopy(void) {
    if (s_sel.mode == SEL_REGION_ONLY) LiftSelectionPixels();
    if (s_sel.mode != SEL_FLOATING) return;
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hPng = SelectionHelpers_ExportPng(); if (hPng) SetClipboardData(RegisterClipboardFormat("PNG"), hPng);
        HGLOBAL hDib = SelectionHelpers_ExportDibV5(); if (hDib) SetClipboardData(CF_DIBV5, hDib);
        CloseClipboard();
    }
}

void SelectionCut(void) { 
    SelectionCopy(); 
    SelectionDelete(); 
    if (!HistoryPush("Cut")) {
        /* Change applied, but undo entry could not be recorded. */
    }
}

void SelectionPaste(HWND hWnd) {
    SelectionClearState();
    if (!OpenClipboard(hWnd)) return;
    HGLOBAL hDat = NULL; int w = 0, h = 0; BYTE* bits = NULL;
    UINT uPng = RegisterClipboardFormat("PNG");
    IWICImagingFactory* factory = FileIO_GetWicFactory();
    if ((hDat = GetClipboardData(uPng)) && factory) {
        IStream* pStream = NULL; IWICBitmapDecoder* pDec = NULL; IWICBitmapFrameDecode* pFr = NULL; IWICFormatConverter* pConv = NULL;
        if (SUCCEEDED(CreateStreamOnHGlobal(hDat, FALSE, &pStream))) {
            if (SUCCEEDED(factory->lpVtbl->CreateDecoderFromStream(factory, pStream, NULL, WICDecodeMetadataCacheOnLoad, &pDec))) {
                if (SUCCEEDED(pDec->lpVtbl->GetFrame(pDec, 0, &pFr))) {
                    factory->lpVtbl->CreateFormatConverter(factory, &pConv);
                    pConv->lpVtbl->Initialize(pConv, (IWICBitmapSource*)pFr, &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
                    UINT uw, uh; pConv->lpVtbl->GetSize(pConv, &uw, &uh);
                    s_sel.pixels.hFloatBmp = CreateDibSection32(uw, uh, &s_sel.pixels.pFloatBits);
                    if (s_sel.pixels.hFloatBmp) {
                        pConv->lpVtbl->CopyPixels(pConv, NULL, uw * 4, uw * uh * 4, s_sel.pixels.pFloatBits);
                        w = uw; h = uh;
                    }
                }
            }
        }
        if (pConv) pConv->lpVtbl->Release(pConv); if (pFr) pFr->lpVtbl->Release(pFr); if (pDec) pDec->lpVtbl->Release(pDec); if (pStream) pStream->lpVtbl->Release(pStream);
    } else if (hDat = GetClipboardData(CF_DIBV5)) {
        BYTE* p = (BYTE*)GlobalLock(hDat); BITMAPV5HEADER* v5 = (BITMAPV5HEADER*)p;
        w = v5->bV5Width; h = abs(v5->bV5Height);
        s_sel.pixels.hFloatBmp = CreateDibSection32(w, h, &s_sel.pixels.pFloatBits);
        if (s_sel.pixels.hFloatBmp) {
            memcpy(s_sel.pixels.pFloatBits, p + v5->bV5Size, w * h * 4);
            if (v5->bV5AlphaMask == 0) {
                int pixelCount = w * h;
                for (int i = 0; i < pixelCount; i++) {
                    s_sel.pixels.pFloatBits[i * 4 + 3] = 255;
                }
            }
        }
        GlobalUnlock(hDat);
    }
    CloseClipboard();
    if (s_sel.pixels.hFloatBmp) {
        s_sel.pixels.nWidth = w; s_sel.pixels.nHeight = h;
        s_sel.mode = SEL_FLOATING; s_sel.rcBounds = (RECT){0, 0, w, h};
        UpdateSelectionDraftLayer(); InvalidateCanvas();
    }
}

void SelectionDelete(void) {
    if (s_sel.mode == SEL_FLOATING) { SelectionClearState(); InvalidateCanvas(); }
    else if (s_sel.mode == SEL_REGION_ONLY) {
        Layers_BeginWrite();
        BYTE* p = LayersGetActiveColorBits(); int cw = Canvas_GetWidth(), ch = Canvas_GetHeight();
        for (int y = s_sel.rcBounds.top; y < s_sel.rcBounds.bottom; y++) {
            if (y < 0 || y >= ch) continue;
            for (int x = s_sel.rcBounds.left; x < s_sel.rcBounds.right; x++) {
                if (x < 0 || x >= cw) continue;
                if (!s_sel.hRegion || PtInRegion(s_sel.hRegion, x, y)) *(DWORD*)(p + (y * cw + x) * 4) = 0;
            }
        }
        UpdateCanvasAfterModification(); SelectionClearState();
        if (!HistoryPush("Delete Selection")) {
            /* Change applied, but undo entry could not be recorded. */
        }
    }
}

// ============================================================================
// Event Handlers & Hit Testing
// ============================================================================

static int HitTestRotationHandles(RECT *rc, int x, int y) {
    double scale = GetZoomScale();
    int tol = (int)(6.0 / scale); if (tol < 1) tol = 1;
    int rotTol = (int)(20.0 / scale);
    int L = rc->left, T = rc->top;
    int R = rc->right - 1, B = rc->bottom - 1;
    if (rc->right <= rc->left + 1)
        R = L;
    else if (R < L)
        R = L;
    if (rc->bottom <= rc->top + 1)
        B = T;
    else if (B < T)
        B = T;
    POINT c[4] = {{L, T}, {R, T}, {R, B}, {L, B}};
    int h[4] = {HT_ROTATE_TL, HT_ROTATE_TR, HT_ROTATE_BR, HT_ROTATE_BL};
    for (int i = 0; i < 4; i++) {
        int dx = x - c[i].x, dy = y - c[i].y;
        int d2 = dx*dx + dy*dy;
        if (d2 > tol*tol && d2 <= rotTol*rotTol) {
            if ((i==0 && (x<L || y<T)) || (i==1 && (x>R || y<T)) ||
                (i==2 && (x>R || y>B)) || (i==3 && (x<L || y>B))) return h[i];
        }
    }
    return HT_NONE;
}

void SelectionToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
    if (nButton == MK_RBUTTON) return;
    s_modeAtDragStart = s_sel.mode;
    s_dragMoved = FALSE;
    if (s_sel.mode != SEL_NONE) {
        RECT rcCommitBar;
        Selection_GetOverlayFrameRect(&rcCommitBar);
        COMMIT_BAR_HANDLE_CLICK(&rcCommitBar, x, y, CommitSelection(), CancelSelection());

        RECT rcFrame;
        Selection_GetOverlayFrameRect(&rcFrame);
        int rotH = HitTestRotationHandles(&rcFrame, x, y);
        if (rotH != HT_NONE) {
            if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
            s_sel.nDragMode = rotH;
            s_sel.rot.ptCenter.x = (s_sel.rcBounds.left+s_sel.rcBounds.right)/2;
            s_sel.rot.ptCenter.y = (s_sel.rcBounds.top+s_sel.rcBounds.bottom)/2;
            s_sel.rot.fStartAngle = atan2(y - s_sel.rot.ptCenter.y, x - s_sel.rot.ptCenter.x) * 180.0 / M_PI;
            s_sel.rot.fLastAngle = s_sel.rot.fAngle;
            SetCapture(hWnd); return;
        }
        int handle = Overlay_HitTestBoxHandles(&s_sel.rcBounds, x, y);
        if (handle != HT_NONE) {
            if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
            s_sel.nDragMode = handle; s_sel.ptDragStart = (POINT){x, y}; s_sel.rcDragOrig = s_sel.rcBounds;
            SetCapture(hWnd); return;
        }
        if (IsPointInSelection(x, y)) {
            if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
            s_sel.nDragMode = HT_BODY; s_sel.ptDragStart = (POINT){x, y}; s_sel.rcDragOrig = s_sel.rcBounds;
            SetCapture(hWnd); return;
        }
        CommitSelection();
    }
    s_sel.nDragMode = HT_BODY;
    s_sel.ptDragStart = (POINT){x, y}; s_sel.rcBounds = (RECT){x, y, x, y};
    if (Tool_GetCurrent() == TOOL_FREEFORM) {
        Poly_Clear(&s_sel.freeformPts);
        Poly_Add(&s_sel.freeformPts, x, y);
    }
    SetCapture(hWnd);
}

void SelectionToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
    if (GetCapture() != hWnd) return;
    if (s_sel.nDragMode >= HT_ROTATE_TL && s_sel.nDragMode <= HT_ROTATE_BL) {
        s_dragMoved = TRUE;
        double cur = atan2(y - s_sel.rot.ptCenter.y, x - s_sel.rot.ptCenter.x) * 180.0 / M_PI;
        s_sel.rot.fAngle = s_sel.rot.fLastAngle + (cur - s_sel.rot.fStartAngle);
    } else if (s_sel.nDragMode == HT_BODY && s_modeAtDragStart == SEL_NONE) {
        s_dragMoved = TRUE;
        if (Tool_GetCurrent() == TOOL_FREEFORM) {
            Poly_Add(&s_sel.freeformPts, x, y);
            s_sel.rcBounds = Poly_GetBounds(&s_sel.freeformPts);
        } else {
            s_sel.rcBounds.left = min(s_sel.ptDragStart.x, x); s_sel.rcBounds.top = min(s_sel.ptDragStart.y, y);
            s_sel.rcBounds.right = max(s_sel.ptDragStart.x, x) + 1; s_sel.rcBounds.bottom = max(s_sel.ptDragStart.y, y) + 1;
        }
        s_sel.mode = SEL_REGION_ONLY;
    } else if (s_sel.nDragMode == HT_BODY) {
        s_dragMoved = TRUE;
        int dx = x - s_sel.ptDragStart.x, dy = y - s_sel.ptDragStart.y;
        s_sel.rcBounds = s_sel.rcDragOrig; OffsetRect(&s_sel.rcBounds, dx, dy);
} else if (s_sel.nDragMode != HT_NONE) {
         s_dragMoved = TRUE;
         s_sel.rcBounds = s_sel.rcDragOrig;
         ResizeRect(&s_sel.rcBounds, s_sel.nDragMode, x - s_sel.ptDragStart.x, y - s_sel.ptDragStart.y, 2, IsShiftDown());
        NormalizeRect(&s_sel.rcBounds);
    }
    if (s_sel.mode == SEL_FLOATING) UpdateSelectionDraftLayer();
    InvalidateCanvas();
}

void SelectionToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
    (void)x; (void)y; (void)nButton;
    if (GetCapture() == hWnd) ReleaseCapture();

    if (s_modeAtDragStart == SEL_NONE && !s_dragMoved) {
        SelectionClearState();
        LayersClearDraft();
        InvalidateCanvas();
    } else if (Tool_GetCurrent() == TOOL_FREEFORM && s_sel.freeformPts.count > 2) {
        s_sel.hRegion = Poly_CreateRegion(&s_sel.freeformPts);
    }

    if (s_dragMoved && s_sel.mode != SEL_NONE) {
        HistoryPushToolSessionById(TOOL_SELECT, "Adjust Selection");
    }

    s_sel.nDragMode = HT_NONE;
    s_dragMoved = FALSE;
}

void SelectionTool_OnCaptureLost(void) {
    s_sel.nDragMode = HT_NONE;
    s_dragMoved = FALSE;
    if (s_modeAtDragStart == SEL_NONE && s_sel.mode == SEL_REGION_ONLY) {
        SelectionClearState();
        LayersClearDraft();
        InvalidateCanvas();
    }
}

int SelectionGetCursorId(int x, int y) {
    if (s_sel.mode == SEL_NONE) return HT_NONE;
    RECT rcFrame;
    Selection_GetOverlayFrameRect(&rcFrame);
    int rot = HitTestRotationHandles(&rcFrame, x, y);
    if (rot != HT_NONE) return rot;
    return Overlay_HitTestBoxHandles(&s_sel.rcBounds, x, y);
}

BOOL IsPointInSelection(int x, int y) {
    if (s_sel.mode == SEL_NONE) return FALSE;
    double fx = x, fy = y;
    if (fabs(s_sel.rot.fAngle) > 0.01) {
        double rad = -s_sel.rot.fAngle * M_PI / 180.0;
        double cx = (s_sel.rcBounds.left+s_sel.rcBounds.right)/2.0, cy = (s_sel.rcBounds.top+s_sel.rcBounds.bottom)/2.0;
        double dx = x - cx, dy = y - cy;
        fx = cx + dx*cos(rad) - dy*sin(rad); fy = cy + dx*sin(rad) + dy*cos(rad);
    }
    int ix = (int)floor(fx+0.5), iy = (int)floor(fy+0.5);
    if (s_sel.hRegion) return PtInRegion(s_sel.hRegion, ix, iy);
    return PtInRect(&s_sel.rcBounds, (POINT){ix, iy});
}

// ============================================================================
// Drawing & Overlay
// ============================================================================

void SelectionToolDrawOverlay(HDC hdc, double scale, int dx, int dy) {
    if (s_sel.mode == SEL_NONE) return;
    OverlayContext ctx; Overlay_Init(&ctx, hdc, scale, dx, dy);
    RECT rcFrame;
    Selection_GetOverlayFrameRect(&rcFrame);
    Overlay_DrawSelectionFrame(&ctx, &rcFrame, (s_sel.mode == SEL_FLOATING));
    if (s_sel.mode == SEL_REGION_ONLY) {
        if (s_sel.freeformPts.count > 1) {
            Overlay_DrawPolyline(&ctx, s_sel.freeformPts.points, s_sel.freeformPts.count, RGB(0, 0, 0), PS_DOT);
        }
    }
    CommitBar_Draw(&ctx, &rcFrame);
}

void SelectionTool_Deactivate(void) { if (s_sel.mode != SEL_NONE) CommitSelection(); }
BOOL SelectionTool_Cancel(void) { if (s_sel.mode != SEL_NONE) { CancelSelection(); return TRUE; } return FALSE; }

void SelectionInvert(void) { /* Implementation here if needed */ }
void SelectionInvertColors(void) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    if (s_sel.mode != SEL_FLOATING) return;
    int w = s_sel.pixels.nWidth, h = s_sel.pixels.nHeight;
    for (int i=0; i<w*h; i++) {
        BYTE* p = s_sel.pixels.pFloatBits + i*4;
        p[0] = 255-p[0]; p[1] = 255-p[1]; p[2] = 255-p[2];
    }
    UpdateSelectionDraftLayer();
}

void SelectionSelectAll(void) {
     SelectionClearState();
     s_sel.rcBounds = (RECT){0, 0, Canvas_GetWidth(), Canvas_GetHeight()};
     s_sel.mode = SEL_REGION_ONLY;
     InvalidateCanvas();
}

void SelectionMove(int dx, int dy) {
     if (s_sel.mode != SEL_FLOATING) {
         LiftSelectionPixels();
     }
     if (s_sel.mode == SEL_FLOATING) {
         OffsetRect(&s_sel.rcBounds, dx, dy);
         UpdateSelectionDraftLayer();
         InvalidateCanvas();
         HistoryPushToolSessionById(TOOL_SELECT, "Adjust Selection");
     }
}
