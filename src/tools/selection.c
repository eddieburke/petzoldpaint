#include "selection_tool.h"
#include "canvas.h"
#include "gdi_utils.h"
#include "peztold_core.h"
#include "draw.h"
#include "geom.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"
#include "overlay.h"
#include "commit_bar.h"
#include "poly_store.h"
#include "file_io.h"
#include "palette.h"
#include "tools.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wincodec.h>
#include <objbase.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define HANDLE_SZ 6

typedef enum { SEL_NONE, SEL_REGION_ONLY, SEL_FLOATING } SelectionMode;

typedef struct {
    SelectionMode mode;
    HRGN hRegion;
    RECT rcBounds;
    PolyStore freeformPts;
    HBITMAP hFloatBmp, hBackupBmp;
    BYTE *pFloatBits, *pBackupBits;
    HRGN hBackupRegion;
    RECT rcLiftOrigin;
    int nFloatW, nFloatH;
    double fAngle;
    POINT ptCenter;
    double fAngleBase;
    double fMouseAngleStart;
    int nDragMode;
    POINT ptDragStart, ptDragLast;
    RECT rcDragOrig;
} SelectionState;

static SelectionState s_sel = {0};
static SelectionMode s_modeAtDragStart = SEL_NONE;
static BOOL s_dragMoved = FALSE;

static void UpdateDraft(void);
static void RestoreCanvas(void);
static void SampleRotated(const BYTE *src, int sw, int sh, BYTE *dst, double angle, double cx, double cy, int sx, int sy, int ex, int ey, int cw, int ch);
static void DeleteInternal(BOOL pushHist, const char *label);
static void LiftSelectionPixels(void);

static void StartRotationDrag(HWND hWnd, int x, int y, int rot);
static void StartResizeDrag(HWND hWnd, int x, int y, int handle);
static void StartMoveDrag(HWND hWnd, int x, int y);
static void StartNewSelection(HWND hWnd, int x, int y);

void SelectionClearState(void) {
    if (s_sel.hRegion) DeleteObject(s_sel.hRegion);
    if (s_sel.hFloatBmp) DeleteObject(s_sel.hFloatBmp);
    if (s_sel.hBackupBmp) DeleteObject(s_sel.hBackupBmp);
    if (s_sel.hBackupRegion) DeleteObject(s_sel.hBackupRegion);
    Poly_Free(&s_sel.freeformPts);
    memset(&s_sel, 0, sizeof(SelectionState));
    s_sel.mode = SEL_NONE;
    s_sel.nDragMode = HT_NONE;
}

BOOL IsSelectionActive(void) { return s_sel.mode != SEL_NONE; }
BOOL SelectionIsDragging(void) { return s_sel.nDragMode != HT_NONE; }

static void BackupCanvas(void) {
    if (s_sel.hBackupBmp) DeleteObject(s_sel.hBackupBmp);
    s_sel.hBackupBmp = CopyBitmapToDib32(LayersGetActiveColorBitmap(),
                                         Canvas_GetWidth(),
                                         Canvas_GetHeight(),
                                         &s_sel.pBackupBits);
    if (s_sel.hBackupRegion) DeleteObject(s_sel.hBackupRegion);
    s_sel.hBackupRegion = s_sel.hRegion ? CreateRectRgn(0, 0, 0, 0) : NULL;
    if (s_sel.hRegion) CombineRgn(s_sel.hBackupRegion, s_sel.hRegion, NULL, RGN_COPY);
    s_sel.rcLiftOrigin = s_sel.rcBounds;
}

static void LiftSelectionPixels(void) {
    if (s_sel.mode != SEL_REGION_ONLY) return;
    int w = s_sel.rcBounds.right - s_sel.rcBounds.left;
    int h = s_sel.rcBounds.bottom - s_sel.rcBounds.top;
    if (w <= 0 || h <= 0) return;
    BackupCanvas();
    s_sel.hFloatBmp = CreateDibSection32(w, h, &s_sel.pFloatBits);
    if (!s_sel.hFloatBmp) return;
    s_sel.nFloatW = w;
    s_sel.nFloatH = h;
    memset(s_sel.pFloatBits, 0, (size_t)w * h * 4);
    HBITMAP hOld;
    HDC hDC = GetCanvasBitmapDC(&hOld);
    if (hDC) {
        HDC fDC = CreateCompatibleDC(hDC);
        HBITMAP hOldF = SelectObject(fDC, s_sel.hFloatBmp);
        if (s_sel.hRegion) {
            HRGN clip = CreateRectRgn(0, 0, 0, 0);
            CombineRgn(clip, s_sel.hRegion, NULL, RGN_COPY);
            OffsetRgn(clip, -s_sel.rcBounds.left, -s_sel.rcBounds.top);
            SelectClipRgn(fDC, clip);
            BitBlt(fDC, 0, 0, w, h, hDC, s_sel.rcBounds.left, s_sel.rcBounds.top, SRCCOPY);
            DeleteObject(clip);
        } else {
            BitBlt(fDC, 0, 0, w, h, hDC, s_sel.rcBounds.left, s_sel.rcBounds.top, SRCCOPY);
        }
        SelectObject(fDC, hOldF);
        DeleteDC(fDC);
        ReleaseCanvasBitmapDC(hDC, hOld);
    }
    Layers_BeginWrite();
    BYTE *cb = LayersGetActiveColorBits();
    if (cb) {
        int cw = Canvas_GetWidth();
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
            int cx = s_sel.rcBounds.left + x, cy = s_sel.rcBounds.top + y;
            if (cx < 0 || cy < 0 || cx >= cw || cy >= Canvas_GetHeight()) continue;
            if (s_sel.hRegion && !PtInRegion(s_sel.hRegion, cx, cy)) continue;
            int ci = (cy * cw + cx) * 4, fi = (y * w + x) * 4;
            s_sel.pFloatBits[fi + 3] = cb[ci + 3];
            *(DWORD*)(cb + ci) = 0;
            }
        }
    }
    s_sel.fAngle = 0;
    s_sel.mode = SEL_FLOATING;
    UpdateDraft();
    UpdateCanvasAfterModification();
}

static void RestoreCanvas(void) {
    if (!s_sel.hBackupBmp || !s_sel.pBackupBits) return;
    BYTE *cb = LayersGetActiveColorBits();
    if (!cb) return;
    int cw = Canvas_GetWidth();
    int ch = Canvas_GetHeight();
    RECT *rc = &s_sel.rcLiftOrigin;
    for (int y = rc->top; y < rc->bottom; y++) {
        for (int x = rc->left; x < rc->right; x++) {
        if (x >= 0 && y >= 0 && x < cw && y < ch)
            *(DWORD*)(cb + (y * cw + x) * 4) = *(DWORD*)(s_sel.pBackupBits + (y * cw + x) * 4);
        }
    }
}

static void GetRotatedExtents(int *dw, int *dh, double *cx, double *cy, int *sx, int *sy, int *ex, int *ey) {
    int w = s_sel.rcBounds.right - s_sel.rcBounds.left;
    int h = s_sel.rcBounds.bottom - s_sel.rcBounds.top;
    *dw = w;
    *dh = h;
    double cxLocal = s_sel.rcBounds.left + w / 2.0;
    double cyLocal = s_sel.rcBounds.top + h / 2.0;
    if (cx) *cx = cxLocal;
    if (cy) *cy = cyLocal;
    if (w <= 0 || h <= 0) {
        if (sx) *sx = 0;
        if (sy) *sy = 0;
        if (ex) *ex = 0;
        if (ey) *ey = 0;
        return;
    }

    double rad = s_sel.fAngle * M_PI / 180.0;
    double c = cos(rad);
    double s = sin(rad);
    double hw = w / 2.0;
    double hh = h / 2.0;
    double pts[4][2] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
    double mnX = 0, mxX = 0, mnY = 0, mxY = 0;
    for (int i = 0; i < 4; i++) {
        double rx = pts[i][0] * c - pts[i][1] * s;
        double ry = pts[i][0] * s + pts[i][1] * c;
        if (i == 0 || rx < mnX) mnX = rx;
        if (i == 0 || rx > mxX) mxX = rx;
        if (i == 0 || ry < mnY) mnY = ry;
        if (i == 0 || ry > mxY) mxY = ry;
    }
    if (sx) *sx = (int)floor(cxLocal + mnX);
    if (ex) *ex = (int)ceil(cxLocal + mxX);
    if (sy) *sy = (int)floor(cyLocal + mnY);
    if (ey) *ey = (int)ceil(cyLocal + mxY);
}

void CommitSelection(void) {
    if (s_sel.mode == SEL_NONE) return;
    if (s_sel.mode == SEL_FLOATING && s_sel.pFloatBits) {
        Layers_BeginWrite();
        BYTE *bits = LayersGetActiveColorBits();
        if (bits) {
            int dw, dh, sx, sy, ex, ey;
            double cx, cy;
            GetRotatedExtents(&dw, &dh, &cx, &cy, &sx, &sy, &ex, &ey);
            if (dw > 0 && dh > 0) {
                SampleRotated(s_sel.pFloatBits, s_sel.nFloatW, s_sel.nFloatH, bits,
                              s_sel.fAngle, cx, cy, sx, sy, ex, ey,
                              Canvas_GetWidth(), Canvas_GetHeight());
            }
        }
        UpdateCanvasAfterModification();
        SetDocumentDirty();
        SelectionClearState();
        HistoryPush("Commit Selection");
    } else {
        SelectionClearState();
        InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    }
}

void CancelSelection(void) {
    if (s_sel.mode == SEL_NONE && s_sel.nDragMode == HT_NONE) return;
    if (s_sel.mode == SEL_FLOATING && s_sel.hBackupBmp) {
        RestoreCanvas();
        HRGN old = s_sel.hRegion;
        s_sel.hRegion = s_sel.hBackupRegion;
        s_sel.hBackupRegion = NULL;
        if (old) DeleteObject(old);
        s_sel.rcBounds = s_sel.rcLiftOrigin;
        s_sel.mode = SEL_REGION_ONLY;
        s_sel.fAngle = 0;
        if (s_sel.hFloatBmp) {
            DeleteObject(s_sel.hFloatBmp);
            s_sel.hFloatBmp = NULL;
            s_sel.pFloatBits = NULL;
        }
        if (s_sel.hBackupBmp) {
            DeleteObject(s_sel.hBackupBmp);
            s_sel.hBackupBmp = NULL;
            s_sel.pBackupBits = NULL;
        }
        LayersClearDraft();
        UpdateCanvasAfterModification();
    } else {
        SelectionClearState();
        LayersClearDraft();
        InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    }
}

SelectionSnapshot* Selection_CreateSnapshot(void) {
    SelectionSnapshot* s = calloc(1, sizeof(SelectionSnapshot));
    if (!s) return NULL;
    s->mode = s_sel.mode;
    s->rcBounds = s_sel.rcBounds;
    if (s_sel.hRegion) {
        s->hRegion = CreateRectRgn(0, 0, 0, 0);
        CombineRgn(s->hRegion, s_sel.hRegion, NULL, RGN_COPY);
    }
    if (s_sel.hFloatBmp) {
        s->hFloatBmp = CopyBitmapToDib32(s_sel.hFloatBmp, s_sel.nFloatW, s_sel.nFloatH,
                                         &s->pFloatBits);
        s->nFloatW = s_sel.nFloatW;
        s->nFloatH = s_sel.nFloatH;
    }
    if (s_sel.hBackupBmp) {
        s->hBackupBmp = CopyBitmapToDib32(s_sel.hBackupBmp, Canvas_GetWidth(),
                                          Canvas_GetHeight(), &s->pBackupBits);
    }
    if (s_sel.hBackupRegion) {
        s->hBackupRegion = CreateRectRgn(0, 0, 0, 0);
        CombineRgn(s->hBackupRegion, s_sel.hBackupRegion, NULL, RGN_COPY);
    }
    s->rcLiftOrigin = s_sel.rcLiftOrigin;
    s->fRotationAngle = s_sel.fAngle;
    s->ptRotateCenter = s_sel.ptCenter;
    Poly_Copy(&s->freeformPts, &s_sel.freeformPts);
    return s;
}

void Selection_DestroySnapshot(SelectionSnapshot* s) {
    if (!s) return;
    if (s->hRegion) DeleteObject(s->hRegion);
    if (s->hFloatBmp) DeleteObject(s->hFloatBmp);
    if (s->hBackupBmp) DeleteObject(s->hBackupBmp);
    if (s->hBackupRegion) DeleteObject(s->hBackupRegion);
    Poly_Free(&s->freeformPts);
    free(s);
}

void Selection_ApplySnapshot(SelectionSnapshot* s) {
    SelectionClearState();
    if (!s) {
        InvalidateRect(GetCanvasWindow(), NULL, FALSE);
        return;
    }
    s_sel.mode = s->mode;
    s_sel.rcBounds = s->rcBounds;
    if (s->hRegion) {
        s_sel.hRegion = CreateRectRgn(0, 0, 0, 0);
        CombineRgn(s_sel.hRegion, s->hRegion, NULL, RGN_COPY);
    }
    if (s->hFloatBmp) {
        s_sel.hFloatBmp = CopyBitmapToDib32(s->hFloatBmp, s->nFloatW, s->nFloatH,
                                            &s_sel.pFloatBits);
        s_sel.nFloatW = s->nFloatW;
        s_sel.nFloatH = s->nFloatH;
    }
    if (s->hBackupBmp) {
        s_sel.hBackupBmp = CopyBitmapToDib32(s->hBackupBmp, Canvas_GetWidth(),
                                             Canvas_GetHeight(), &s_sel.pBackupBits);
    }
    if (s->hBackupRegion) {
        s_sel.hBackupRegion = CreateRectRgn(0, 0, 0, 0);
        CombineRgn(s_sel.hBackupRegion, s->hBackupRegion, NULL, RGN_COPY);
    }
    s_sel.rcLiftOrigin = s->rcLiftOrigin;
    s_sel.fAngle = s->fRotationAngle;
    s_sel.ptCenter = s->ptRotateCenter;
    Poly_Copy(&s_sel.freeformPts, &s->freeformPts);
    s_sel.nDragMode = HT_NONE;
    if (s_sel.mode == SEL_FLOATING && !s_sel.pFloatBits) s_sel.mode = SEL_REGION_ONLY;
    UpdateDraft();
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

static void SampleRotated(const BYTE *src, int sw, int sh, BYTE *dst, double angle, double cx, double cy, int sx, int sy, int ex, int ey, int cw, int ch) {
    if (!src || !dst || sw <= 0 || sh <= 0) return;
    if (ex == sx || ey == sy) return;
    double rad = angle * M_PI / 180.0, c = cos(rad), s = sin(rad), hw = sw/2.0, hh = sh/2.0;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    if (ex > cw) ex = cw;
    if (ey > ch) ey = ch;
    for (int y = sy; y < ey; y++) {
        for (int x = sx; x < ex; x++) {
        double dx = (x + 0.5) - cx, dy = (y + 0.5) - cy;
        double ux = dx*c + dy*s, uy = -dx*s + dy*c;
        double srcX = ux + hw, srcY = uy + hh;
        int si = (int)floor(srcX), sj = (int)floor(srcY);
        if (si >= 0 && si < sw && sj >= 0 && sj < sh) {
            int si2 = (sj * sw + si) * 4, ci = (y * cw + x) * 4;
            const BYTE *sp = src + si2;
            if (sp[3] == 0) continue;
            PixelOps_BlendPixel(sp[2], sp[1], sp[0], sp[3], dst + ci, 0);
        }
        }
    }
}

void SelectionFlip(BOOL horz) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    if (s_sel.mode != SEL_FLOATING || !s_sel.pFloatBits) return;
    Transform_Flip(s_sel.pFloatBits, s_sel.nFloatW, s_sel.nFloatH, horz);
    UpdateDraft();
    HistoryPushSession("Adjust Selection");
}

void SelectionRotate(int deg) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    if (s_sel.mode != SEL_FLOATING) return;
    s_sel.fAngle += deg;
    UpdateDraft();
    HistoryPushSession("Adjust Selection");
}

static void UpdateDraft(void) {
    if (s_sel.mode != SEL_FLOATING || !s_sel.pFloatBits) {
        LayersClearDraft();
        return;
    }
    BYTE *draft = LayersGetDraftBits();
    if (!draft) return;
    LayersClearDraft();
    int dw, dh, sx, sy, ex, ey;
    double cx, cy;
    GetRotatedExtents(&dw, &dh, &cx, &cy, &sx, &sy, &ex, &ey);
    if (dw > 0 && dh > 0) {
        SampleRotated(s_sel.pFloatBits, s_sel.nFloatW, s_sel.nFloatH, draft,
                      s_sel.fAngle, cx, cy, sx, sy, ex, ey, Canvas_GetWidth(),
                      Canvas_GetHeight());
    }
    LayersMarkDirty();
}

static HGLOBAL ExportPNG(void) {
    if (!s_sel.pFloatBits || !FileIO_GetWicFactory()) return NULL;
    IWICImagingFactory *f = FileIO_GetWicFactory();
    IStream *stream = NULL;
    IWICBitmapEncoder *enc = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    HGLOBAL hMem = NULL;
    if (FAILED(CreateStreamOnHGlobal(NULL, FALSE, &stream))) return NULL;
    if (SUCCEEDED(f->lpVtbl->CreateEncoder(f, &GUID_ContainerFormatPng, NULL, &enc))) {
        if (SUCCEEDED(enc->lpVtbl->Initialize(enc, stream, WICBitmapEncoderNoCache))) {
            if (SUCCEEDED(enc->lpVtbl->CreateNewFrame(enc, &frame, NULL))) {
                frame->lpVtbl->Initialize(frame, NULL);
                frame->lpVtbl->SetSize(frame, s_sel.nFloatW, s_sel.nFloatH);
                WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
                frame->lpVtbl->SetPixelFormat(frame, &fmt);
                frame->lpVtbl->WritePixels(frame, s_sel.nFloatH, s_sel.nFloatW * 4, s_sel.nFloatW * s_sel.nFloatH * 4, s_sel.pFloatBits);
                frame->lpVtbl->Commit(frame);
            }
            enc->lpVtbl->Commit(enc);
        }
    }
    if (stream) GetHGlobalFromStream(stream, &hMem);
    if (frame) frame->lpVtbl->Release(frame);
    if (enc) enc->lpVtbl->Release(enc);
    if (stream) stream->lpVtbl->Release(stream);
    return hMem;
}

static HGLOBAL ExportDIBV5(void) {
    if (!s_sel.pFloatBits) return NULL;
    int w = s_sel.nFloatW, h = s_sel.nFloatH;
    DWORD imgSize = w * h * 4;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(BITMAPV5HEADER) + imgSize);
    if (!hMem) return NULL;
    BYTE *p = GlobalLock(hMem);
    BITMAPV5HEADER *v5 = (BITMAPV5HEADER*)p;
    v5->bV5Size = sizeof(BITMAPV5HEADER);
    v5->bV5Width = w;
    v5->bV5Height = -h;
    v5->bV5Planes = 1;
    v5->bV5BitCount = 32;
    v5->bV5Compression = BI_BITFIELDS;
    v5->bV5RedMask = 0x00FF0000;
    v5->bV5GreenMask = 0x0000FF00;
    v5->bV5BlueMask = 0x000000FF;
    v5->bV5AlphaMask = 0xFF000000;
    v5->bV5CSType = LCS_sRGB;
    v5->bV5Intent = LCS_GM_IMAGES;
    v5->bV5SizeImage = imgSize;
    memcpy(p + sizeof(BITMAPV5HEADER), s_sel.pFloatBits, imgSize);
    GlobalUnlock(hMem);
    return hMem;
}

void SelectionCopy(void) {
    if (s_sel.mode == SEL_REGION_ONLY) LiftSelectionPixels();
    if (s_sel.mode != SEL_FLOATING) return;
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hPng = ExportPNG();
        if (hPng) SetClipboardData(RegisterClipboardFormat("PNG"), hPng);
        HGLOBAL hDib = ExportDIBV5();
        if (hDib) SetClipboardData(CF_DIBV5, hDib);
        CloseClipboard();
    }
}

void SelectionCut(void) {
    SelectionCopy();
    DeleteInternal(FALSE, "Cut");
    (void)HistoryPush("Cut");
}

void SelectionPaste(HWND hWnd) {
    SelectionClearState();
    if (!OpenClipboard(hWnd)) return;
    HGLOBAL hDat = NULL;
    int w = 0;
    int h = 0;
    UINT uPng = RegisterClipboardFormat("PNG");
    IWICImagingFactory *f = FileIO_GetWicFactory();
    if ((hDat = GetClipboardData(uPng)) && f) {
        IStream *stream = NULL;
        IWICBitmapDecoder *dec = NULL;
        IWICBitmapFrameDecode *fr = NULL;
        IWICFormatConverter *conv = NULL;
        if (SUCCEEDED(CreateStreamOnHGlobal(hDat, FALSE, &stream))) {
            if (SUCCEEDED(f->lpVtbl->CreateDecoderFromStream(f, stream, NULL, WICDecodeMetadataCacheOnLoad, &dec))) {
                if (SUCCEEDED(dec->lpVtbl->GetFrame(dec, 0, &fr))) {
                    if (SUCCEEDED(f->lpVtbl->CreateFormatConverter(f, &conv))) {
                        if (SUCCEEDED(conv->lpVtbl->Initialize(conv, (IWICBitmapSource*)fr, &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom))) {
                            UINT uw, uh;
                            conv->lpVtbl->GetSize(conv, &uw, &uh);
                            s_sel.hFloatBmp = CreateDibSection32(uw, uh, &s_sel.pFloatBits);
                            if (s_sel.hFloatBmp) {
                                conv->lpVtbl->CopyPixels(conv, NULL, uw * 4, uw * uh * 4,
                                                         s_sel.pFloatBits);
                                w = uw;
                                h = uh;
                            }
                        }
                    }
                }
            }
        }
        if (conv) conv->lpVtbl->Release(conv);
        if (fr) fr->lpVtbl->Release(fr);
        if (dec) dec->lpVtbl->Release(dec);
        if (stream) stream->lpVtbl->Release(stream);
    } else if ((hDat = GetClipboardData(CF_DIBV5))) {
        BYTE *p = (BYTE*)GlobalLock(hDat);
        if (p) {
            BITMAPV5HEADER *v5 = (BITMAPV5HEADER*)p;
            w = v5->bV5Width;
            h = abs(v5->bV5Height);
            if (v5->bV5BitCount == 32 && v5->bV5SizeImage == (DWORD)(w * h * 4)) {
                s_sel.hFloatBmp = CreateDibSection32(w, h, &s_sel.pFloatBits);
                if (s_sel.hFloatBmp) {
                    memcpy(s_sel.pFloatBits, p + v5->bV5Size, w * h * 4);
                    if (v5->bV5AlphaMask == 0) {
                        for (int i = 0; i < w * h; i++) {
                            s_sel.pFloatBits[i * 4 + 3] = 255;
                        }
                    }
                }
            }
            GlobalUnlock(hDat);
        }
    }
    CloseClipboard();
    if (s_sel.hFloatBmp) {
        s_sel.nFloatW = w;
        s_sel.nFloatH = h;
        s_sel.mode = SEL_FLOATING;
        s_sel.rcBounds = (RECT){0, 0, w, h};
        UpdateDraft();
        InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    }
}

static void DeleteInternal(BOOL pushHist, const char *label) {
    const char *lbl = label ? label : "Delete Selection";
    if (s_sel.mode == SEL_FLOATING) {
        BOOL hadBackup = (s_sel.hBackupBmp != NULL);
        SelectionClearState();
        LayersClearDraft();
        UpdateCanvasAfterModification();
        if (pushHist && hadBackup) {
            (void)HistoryPush(lbl);
        }
    } else if (s_sel.mode == SEL_REGION_ONLY) {
        Layers_BeginWrite();
        BYTE *p = LayersGetActiveColorBits();
        int cw = Canvas_GetWidth();
        int ch = Canvas_GetHeight();
        for (int y = s_sel.rcBounds.top; y < s_sel.rcBounds.bottom; y++) {
            for (int x = s_sel.rcBounds.left; x < s_sel.rcBounds.right; x++) {
            if (x >= 0 && y >= 0 && x < cw && y < ch && (!s_sel.hRegion || PtInRegion(s_sel.hRegion, x, y)))
                *(DWORD*)(p + (y * cw + x) * 4) = 0;
            }
        }
        UpdateCanvasAfterModification();
        SelectionClearState();
        if (pushHist) {
            (void)HistoryPush(lbl);
        }
    }
}

void SelectionDelete(void) { DeleteInternal(TRUE, "Delete Selection"); }

static int HitTestRotationHandles(const RECT *rc, int x, int y) {
    double scale = GetZoomScale();
    int tol = max(1, (int)(6.0 / scale));
    int rotTol = (int)(20.0 / scale);
    int L = rc->left, T = rc->top, R = rc->right > rc->left ? rc->right - 1 : L, B = rc->bottom > rc->top ? rc->bottom - 1 : T;
    POINT c[4] = {{L,T},{R,T},{R,B},{L,B}};
    int h[4] = {HT_ROTATE_TL, HT_ROTATE_TR, HT_ROTATE_BR, HT_ROTATE_BL};
    for (int i = 0; i < 4; i++) {
        int dx = x - c[i].x;
        int dy = y - c[i].y;
        int d2 = dx*dx + dy*dy;
        if (d2 > tol*tol && d2 <= rotTol*rotTol) {
            if ((i==0 && (x<L || y<T)) || (i==1 && (x>R || y<T)) || (i==2 && (x>R || y>B)) || (i==3 && (x<L || y>B))) return h[i];
        }
    }
    return HT_NONE;
}

static void StartRotationDrag(HWND hWnd, int x, int y, int rot) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    s_sel.nDragMode = rot;
    s_sel.ptCenter.x = (s_sel.rcBounds.left + s_sel.rcBounds.right) / 2;
    s_sel.ptCenter.y = (s_sel.rcBounds.top + s_sel.rcBounds.bottom) / 2;
    s_sel.fAngleBase = s_sel.fAngle;
    s_sel.fMouseAngleStart = atan2(y - s_sel.ptCenter.y, x - s_sel.ptCenter.x) * 180.0 / M_PI;
    SetCapture(hWnd);
}

static void StartResizeDrag(HWND hWnd, int x, int y, int handle) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    s_sel.nDragMode = handle;
    s_sel.ptDragStart = (POINT){x, y};
    s_sel.rcDragOrig = s_sel.rcBounds;
    SetCapture(hWnd);
}

static void StartMoveDrag(HWND hWnd, int x, int y) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    s_sel.nDragMode = HT_BODY;
    s_sel.ptDragStart = (POINT){x, y};
    s_sel.rcDragOrig = s_sel.rcBounds;
    SetCapture(hWnd);
}

static void StartNewSelection(HWND hWnd, int x, int y) {
    s_sel.nDragMode = HT_BODY;
    s_sel.ptDragStart = (POINT){x, y};
    s_sel.rcBounds = (RECT){x, y, x, y};
    if (Tool_GetCurrent() == TOOL_FREEFORM) {
        Poly_Clear(&s_sel.freeformPts);
        Poly_Add(&s_sel.freeformPts, x, y);
    }
    SetCapture(hWnd);
}

void SelectionToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
    if (nButton == MK_RBUTTON) return;
    s_modeAtDragStart = s_sel.mode;
    s_dragMoved = FALSE;

    if (s_sel.mode != SEL_NONE) {
        RECT rcBar;
        GetRotatedExtents(&rcBar.right, &rcBar.bottom, NULL, NULL, &rcBar.left, &rcBar.top, &rcBar.right, &rcBar.bottom);
        COMMIT_BAR_HANDLE_CLICK(&rcBar, x, y, CommitSelection(), CancelSelection());

        RECT rcFrame;
        GetRotatedExtents(&rcFrame.right, &rcFrame.bottom, NULL, NULL, &rcFrame.left, &rcFrame.top, &rcFrame.right, &rcFrame.bottom);
        int rot = HitTestRotationHandles(&rcFrame, x, y);
        if (rot != HT_NONE) {
            StartRotationDrag(hWnd, x, y, rot);
            return;
        }

        int handle = Overlay_HitTestBoxHandles(&s_sel.rcBounds, x, y);
        if (handle != HT_NONE) {
            StartResizeDrag(hWnd, x, y, handle);
            return;
        }

        if (IsPointInSelection(x, y)) {
            StartMoveDrag(hWnd, x, y);
            return;
        }

        CommitSelection();
    }

    StartNewSelection(hWnd, x, y);
}

void SelectionToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
    if (GetCapture() != hWnd) return;
    if (s_sel.nDragMode >= HT_ROTATE_TL && s_sel.nDragMode <= HT_ROTATE_BL) {
        s_dragMoved = TRUE;
        double cur = atan2(y - s_sel.ptCenter.y, x - s_sel.ptCenter.x) * 180.0 / M_PI;
        s_sel.fAngle = s_sel.fAngleBase + (cur - s_sel.fMouseAngleStart);
    } else if (s_sel.nDragMode == HT_BODY && s_modeAtDragStart == SEL_NONE) {
        s_dragMoved = TRUE;
        if (Tool_GetCurrent() == TOOL_FREEFORM) {
            Poly_Add(&s_sel.freeformPts, x, y);
            s_sel.rcBounds =
                GetBoundingBox(s_sel.freeformPts.points, s_sel.freeformPts.count);
        } else {
            s_sel.rcBounds.left = min(s_sel.ptDragStart.x, x);
            s_sel.rcBounds.top = min(s_sel.ptDragStart.y, y);
            s_sel.rcBounds.right = max(s_sel.ptDragStart.x, x) + 1;
            s_sel.rcBounds.bottom = max(s_sel.ptDragStart.y, y) + 1;
        }
        s_sel.mode = SEL_REGION_ONLY;
    } else if (s_sel.nDragMode == HT_BODY) {
        s_dragMoved = TRUE;
        int dx = x - s_sel.ptDragStart.x;
        int dy = y - s_sel.ptDragStart.y;
        s_sel.rcBounds = s_sel.rcDragOrig;
        OffsetRect(&s_sel.rcBounds, dx, dy);
    } else if (s_sel.nDragMode != HT_NONE) {
        s_dragMoved = TRUE;
        s_sel.rcBounds = s_sel.rcDragOrig;
        ResizeRect(&s_sel.rcBounds, s_sel.nDragMode, x - s_sel.ptDragStart.x, y - s_sel.ptDragStart.y, 2, IsShiftDown());
        NormalizeRect(&s_sel.rcBounds);
    }
    if (s_sel.mode == SEL_FLOATING) UpdateDraft();
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void SelectionToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
    (void)x;
    (void)y;
    (void)nButton;
    if (GetCapture() == hWnd) ReleaseCapture();
    if (s_modeAtDragStart == SEL_NONE && !s_dragMoved) {
        SelectionClearState();
        LayersClearDraft();
        InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    } else if (Tool_GetCurrent() == TOOL_FREEFORM && s_sel.freeformPts.count > 2) {
        s_sel.hRegion = Poly_CreateRegion(&s_sel.freeformPts);
    }
    if (s_dragMoved && s_sel.mode != SEL_NONE) {
        HistoryPushSession(s_modeAtDragStart == SEL_NONE ? "Create Selection"
                                                         : "Adjust Selection");
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
        InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    }
}

int SelectionGetCursorId(int x, int y) {
    if (s_sel.mode == SEL_NONE) return HT_NONE;
    RECT rcFrame;
    GetRotatedExtents(&rcFrame.right, &rcFrame.bottom, NULL, NULL, &rcFrame.left,
                      &rcFrame.top, &rcFrame.right, &rcFrame.bottom);
    int rot = HitTestRotationHandles(&rcFrame, x, y);
    if (rot != HT_NONE) return rot;
    return Overlay_HitTestBoxHandles(&s_sel.rcBounds, x, y);
}

BOOL IsPointInSelection(int x, int y) {
    if (s_sel.mode == SEL_NONE) return FALSE;
    double fx = x, fy = y;
    if (fabs(s_sel.fAngle) > 0.01) {
        double rad = -s_sel.fAngle * M_PI / 180.0, c = cos(rad), s = sin(rad);
        double cx = (s_sel.rcBounds.left + s_sel.rcBounds.right) / 2.0, cy = (s_sel.rcBounds.top + s_sel.rcBounds.bottom) / 2.0;
        double dx = x - cx, dy = y - cy;
        fx = cx + dx * c - dy * s;
        fy = cy + dx * s + dy * c;
    }
    if (s_sel.hRegion) return PtInRegion(s_sel.hRegion, (int)floor(fx + 0.5), (int)floor(fy + 0.5));
    return PtInRect(&s_sel.rcBounds, (POINT){(int)floor(fx + 0.5), (int)floor(fy + 0.5)});
}

void SelectionToolDrawOverlay(HDC hdc, double scale, int dx, int dy) {
    if (s_sel.mode == SEL_NONE) return;
    OverlayContext ctx;
    Overlay_Init(&ctx, hdc, scale, dx, dy);
    RECT rcFrame;
    GetRotatedExtents(&rcFrame.right, &rcFrame.bottom, NULL, NULL, &rcFrame.left,
                      &rcFrame.top, &rcFrame.right, &rcFrame.bottom);
    Overlay_DrawSelectionFrame(&ctx, &rcFrame, s_sel.mode == SEL_FLOATING);
    if (s_sel.mode == SEL_REGION_ONLY && s_sel.freeformPts.count > 1)
        Overlay_DrawPolyline(&ctx, s_sel.freeformPts.points, s_sel.freeformPts.count, RGB(0,0,0), PS_DOT);
    CommitBar_Draw(&ctx, &rcFrame);
}

void SelectionTool_Deactivate(void) { if (s_sel.mode != SEL_NONE) CommitSelection(); }
BOOL SelectionTool_Cancel(ToolCancelReason reason) {
    if (s_sel.mode == SEL_NONE && s_sel.nDragMode == HT_NONE) return FALSE;

    if (reason == TOOL_CANCEL_INTERRUPT && SelectionIsDragging()) {
        SelectionToolOnMouseUp(GetCanvasWindow(), 0, 0, 0);
        return TRUE;
    }

    CancelSelection();
    return TRUE;
}

void SelectionInvert(void) {
    int cw = Canvas_GetWidth(), ch = Canvas_GetHeight();
    if (cw <= 0 || ch <= 0)
        return;
    if (s_sel.mode == SEL_FLOATING)
        return;

    if (s_sel.mode == SEL_NONE) {
        SelectionSelectAll();
        HistoryPushSession("Invert Selection");
        return;
    }

    HRGN full = CreateRectRgn(0, 0, cw, ch);
    if (!full)
        return;

    HRGN sel = CreateRectRgn(0, 0, 0, 0);
    if (!sel) {
        DeleteObject(full);
        return;
    }
    if (s_sel.hRegion) {
        if (CombineRgn(sel, s_sel.hRegion, NULL, RGN_COPY) == ERROR) {
            DeleteObject(sel);
            DeleteObject(full);
            return;
        }
    } else {
        HRGN r = CreateRectRgnIndirect(&s_sel.rcBounds);
        if (!r) {
            DeleteObject(sel);
            DeleteObject(full);
            return;
        }
        DeleteObject(sel);
        sel = r;
    }

    HRGN inv = CreateRectRgn(0, 0, 0, 0);
    if (!inv || CombineRgn(inv, full, sel, RGN_DIFF) == ERROR) {
        DeleteObject(inv);
        DeleteObject(sel);
        DeleteObject(full);
        return;
    }
    DeleteObject(full);
    DeleteObject(sel);

    Poly_Clear(&s_sel.freeformPts);
    if (s_sel.hRegion)
        DeleteObject(s_sel.hRegion);
    s_sel.hRegion = inv;

    RECT br;
    int rb = GetRgnBox(s_sel.hRegion, &br);
    if (rb == NULLREGION || rb == ERROR) {
        SelectionClearState();
        LayersClearDraft();
        InvalidateRect(GetCanvasWindow(), NULL, FALSE);
        HistoryPushSession("Invert Selection");
        return;
    }
    s_sel.rcBounds = br;
    s_sel.mode = SEL_REGION_ONLY;
    UpdateDraft();
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    HistoryPushSession("Invert Selection");
}
void SelectionInvertColors(void) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    if (s_sel.mode != SEL_FLOATING) return;
    for (int i = 0; i < s_sel.nFloatW * s_sel.nFloatH; i++) {
        BYTE *p = s_sel.pFloatBits + i * 4;
        p[0] = 255 - p[0];
        p[1] = 255 - p[1];
        p[2] = 255 - p[2];
    }
    UpdateDraft();
}

void SelectionSelectAll(void) {
    SelectionClearState();
    s_sel.rcBounds = (RECT){0, 0, Canvas_GetWidth(), Canvas_GetHeight()};
    s_sel.mode = SEL_REGION_ONLY;
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void SelectionMove(int dx, int dy) {
    if (s_sel.mode != SEL_FLOATING) LiftSelectionPixels();
    if (s_sel.mode != SEL_FLOATING) return;
    OffsetRect(&s_sel.rcBounds, dx, dy);
    UpdateDraft();
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    HistoryPushSession("Adjust Selection");
}
