#include "freehand_tools.h"
#include "canvas.h"
#include "geom.h"
#include "gdi_utils.h"
#include "helpers.h"
#include "controller.h"
#include "palette.h"
#include "layers.h"
#include "drawing_primitives.h"
#include "tool_options/tool_options.h"
#include "interaction.h"
#include <stdlib.h>

typedef void (*PointFn)(BYTE*,int,int,int,int,COLORREF,BYTE,int);
typedef void (*LineFn)(BYTE*,int,int,int,int,int,int,COLORREF,BYTE,int);

typedef struct {
    int id;
    PointFn pt;
    LineFn line;
    int (*size)(void);
    COLORREF (*color)(int btn);
    BYTE (*alpha)(int btn);
    int (*pad)(void);
} Policy;

static void PencilPt(BYTE *b,int w,int h,int x,int y,COLORREF c,BYTE a,int s){(void)s;DrawPrim_DrawPencilPoint(b,w,h,x,y,c,a);}
static void PencilLn(BYTE *b,int w,int h,int x1,int y1,int x2,int y2,COLORREF c,BYTE a,int s){(void)s;DrawPrim_DrawPencilLine(b,w,h,x1,y1,x2,y2,c,a);}
static void BrushPt (BYTE *b,int w,int h,int x,int y,COLORREF c,BYTE a,int s){DrawPrim_DrawBrushPoint(b,w,h,x,y,c,a,s);}
static void BrushLn (BYTE *b,int w,int h,int x1,int y1,int x2,int y2,COLORREF c,BYTE a,int s){DrawPrim_DrawBrushLine(b,w,h,x1,y1,x2,y2,c,a,s);}
static void EraserPt(BYTE *b,int w,int h,int x,int y,COLORREF c,BYTE a,int s){(void)c;(void)a;DrawPrim_DrawEraserPoint(b,w,h,x,y,0,s);}
static void SprayPt (BYTE *b,int w,int h,int x,int y,COLORREF c,BYTE a,int s){DrawPrim_DrawSprayPoint(b,w,h,x,y,c,a,s);}

static int OneSize(void)   { return 1; }
static int BrushSize(void) { return nBrushWidth; }
static int SpraySize(void) { return nSprayRadius; }

static COLORREF PrimaryColor(int btn){ return GetColorForButton(btn); }
static COLORREF IgnoredColor(int btn){ (void)btn; return 0; }
static BYTE NormalAlpha (int btn){ return GetOpacityForButton(btn); }
static BYTE IgnoredAlpha(int btn){ (void)btn; return 0; }

static int PencilPad(void){ return 2; }
static int BrushPad (void){ return DrawPrim_GetBrushSize(nBrushWidth) + 2; }
static int EraserPad(void){
    int sizes[] = {4, 6, 8, 10};
    int i = nBrushWidth < 1 ? 0 : (nBrushWidth - 1) % 4;
    return sizes[i] / 2 + 2;
}
static int SprayPad (void){ return DrawPrim_GetSprayRadius(nSprayRadius) + 2; }

static const Policy policies[] = {
    {TOOL_PENCIL,   PencilPt, PencilLn, OneSize,   PrimaryColor, NormalAlpha,  PencilPad},
    {TOOL_BRUSH,    BrushPt,  BrushLn,  BrushSize, PrimaryColor, NormalAlpha,  BrushPad},
    {TOOL_ERASER,   EraserPt, NULL,     BrushSize, IgnoredColor, IgnoredAlpha, EraserPad},
    {TOOL_AIRBRUSH, SprayPt,  NULL,     SpraySize, PrimaryColor, NormalAlpha,  SprayPad},
};

static const Policy *GetPol(int tool){
    for (int i = 0; i < (int)(sizeof(policies)/sizeof(policies[0])); i++)
        if (policies[i].id == tool) return &policies[i];
    return NULL;
}

static const Policy *ActivePolicy(void){
    return Interaction_IsActive() ? GetPol(Interaction_GetActiveToolId()) : NULL;
}

static void DrawInterp(BYTE *b, int w, int h, const Policy *p, int x1, int y1,
                       int x2, int y2, COLORREF c, BYTE a) {
    int sz = p->size();
    if (p->line) {
        p->line(b, w, h, x1, y1, x2, y2, c, a, sz);
        return;
    }
    int dx = x2 - x1, dy = y2 - y1, steps = abs(dx) + abs(dy);
    if (steps == 0) p->pt(b, w, h, x2, y2, c, a, sz);
    else for (int i = 1; i <= steps; i++)
        p->pt(b, w, h, x1 + (dx*i)/steps, y1 + (dy*i)/steps, c, a, sz);
}

void FreehandTool_OnMouseDown(HWND hWnd, int x, int y, int btn, int tool) {
    const Policy *p = GetPol(tool);
    if (!p || !p->pt) return;
    if (Interaction_IsActive()) {
        if (!Interaction_IsActiveButton(btn)) FreehandTool_Cancel();
        return;
    }
    Interaction_Begin(hWnd, x, y, btn, tool);
    BYTE *b = LayersGetStrokeBits();
    if (!b) { Interaction_Abort(); return; }
    p->pt(b, Canvas_GetWidth(), Canvas_GetHeight(), x, y, p->color(btn), p->alpha(btn), p->size());
    Interaction_MarkModified();
    Interaction_NoteStrokeSegment(x, y, x, y, p->pad());
    Interaction_FlushStrokeRedraw();
}

void FreehandTool_OnMouseMove(HWND hWnd, int x, int y, int btn, int toolId) {
    (void)hWnd; (void)toolId;
    if (!Interaction_IsActive() || !Interaction_IsActiveButton(btn)) return;
    const Policy *p = ActivePolicy();
    if (!p || !p->pt) return;
    BYTE *b = LayersGetStrokeBits();
    if (!b) return;
    POINT lp; Interaction_GetLastPoint(&lp);
    int cbtn = Interaction_GetDrawButton();
    DrawInterp(b, Canvas_GetWidth(), Canvas_GetHeight(), p, lp.x, lp.y, x, y, p->color(cbtn), p->alpha(cbtn));
    Interaction_MarkModified();
    Interaction_NoteStrokeSegment(lp.x, lp.y, x, y, p->pad());
    Interaction_UpdateLastPoint(x, y);
}

void FreehandTool_OnMouseUp(HWND hWnd, int x, int y, int btn, int toolId) {
    (void)hWnd; (void)x; (void)y; (void)btn; (void)toolId;
    Interaction_Commit("Draw");
}

void AirbrushTool_OnMouseDown(HWND hWnd, int x, int y, int btn) {
    FreehandTool_OnMouseDown(hWnd, x, y, btn, TOOL_AIRBRUSH);
    if (Interaction_IsActive() && hWnd) SetTimer(hWnd, TIMER_AIRBRUSH, 30, NULL);
}

void AirbrushTool_OnMouseMove(HWND hWnd, int x, int y, int btn) {
    FreehandTool_OnMouseMove(hWnd, x, y, btn, TOOL_AIRBRUSH);
}

void AirbrushTool_OnMouseUp(HWND hWnd, int x, int y, int btn) {
    if (hWnd) KillTimer(hWnd, TIMER_AIRBRUSH);
    FreehandTool_OnMouseUp(hWnd, x, y, btn, TOOL_AIRBRUSH);
}

void FreehandTool_OnTimerTick(void) {
    const Policy *p = ActivePolicy();
    if (!p || Interaction_GetActiveToolId() != TOOL_AIRBRUSH) return;
    BYTE *b = LayersGetStrokeBits();
    if (!b) return;
    POINT lp; Interaction_GetLastPoint(&lp);
    int btn = Interaction_GetDrawButton();
    p->pt(b, Canvas_GetWidth(), Canvas_GetHeight(), lp.x, lp.y, p->color(btn), p->alpha(btn), p->size());
    Interaction_MarkModified();
    Interaction_NoteStrokeSegment(lp.x, lp.y, lp.x, lp.y, p->pad());
    Interaction_FlushStrokeRedraw();
}

static void EndAirbrushTimer(void) {
    if (Interaction_GetActiveToolId() == TOOL_AIRBRUSH)
        KillTimer(GetCanvasWindow(), TIMER_AIRBRUSH);
}

void FreehandTool_Deactivate(void) {
    if (!ActivePolicy()) return;
    EndAirbrushTimer();
    Interaction_EndQuiet();
}

BOOL FreehandTool_Cancel(void) {
    if (!ActivePolicy()) return FALSE;
    EndAirbrushTimer();
    Interaction_Abort();
    return TRUE;
}

int GetActiveFreehandTool(void) {
    const Policy *p = ActivePolicy();
    return p ? p->id : -1;
}

BOOL IsFreehandDrawing(void) { return ActivePolicy() != NULL; }
