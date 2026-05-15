/*------------------------------------------------------------------------------
 * FREEHAND_TOOLS.C - Simplified Freehand Drawing
 *----------------------------------------------------------------------------*/

#include "freehand_tools.h"
#include "../canvas.h"
#include "../geom.h"
#include "../gdi_utils.h"
#include "../helpers.h"
#include "../controller.h"
#include "../palette.h"
#include "../layers.h"
#include "drawing_primitives.h"
#include "tool_options/tool_options.h"
#include "../interaction.h"
#include <stdlib.h>

typedef void (*PointFn)(BYTE*,int,int,int,int,COLORREF,BYTE,int);
typedef void (*LineFn)(BYTE*,int,int,int,int,int,int,COLORREF,BYTE,int);

typedef struct { PointFn pt; LineFn line; int (*sz)(void); COLORREF (*color)(int); BYTE (*alpha)(int); int id; } Policy;

static void PencilPt(BYTE *b,int w,int h,int x,int y,COLORREF c,BYTE a,int s){(void)s;DrawPrim_DrawPencilPoint(b,w,h,x,y,c,a);}
static void PencilLn(BYTE *b,int w,int h,int x1,int y1,int x2,int y2,COLORREF c,BYTE a,int s){(void)s;DrawPrim_DrawPencilLine(b,w,h,x1,y1,x2,y2,c,a);}
static void BrushPt(BYTE *b,int w,int h,int x,int y,COLORREF c,BYTE a,int s){DrawPrim_DrawBrushPoint(b,w,h,x,y,c,a,s);}
static void BrushLn(BYTE *b,int w,int h,int x1,int y1,int x2,int y2,COLORREF c,BYTE a,int s){DrawPrim_DrawBrushLine(b,w,h,x1,y1,x2,y2,c,a,s);}
static void EraserPt(BYTE *b,int w,int h,int x,int y,COLORREF c,BYTE a,int s){(void)a;DrawPrim_DrawEraserPoint(b,w,h,x,y,c,s);}
static void SprayPt(BYTE *b,int w,int h,int x,int y,COLORREF c,BYTE a,int s){DrawPrim_DrawSprayPoint(b,w,h,x,y,c,a,s);}

static int OneSize(void){return 1;}
static int BrushSize(void){return nBrushWidth;}
static int SpraySize(void){return nSprayRadius;}

static COLORREF PrimaryColor(int btn){return GetColorForButton(btn);}
static COLORREF BgColor(int btn){(void)btn;return Palette_GetSecondaryColor();}
static BYTE NormalAlpha(int btn){return GetOpacityForButton(btn);}
static BYTE FullAlpha(int btn){(void)btn;return 255;}

static const Policy policies[] = {
    {PencilPt, PencilLn, OneSize, PrimaryColor, NormalAlpha, TOOL_PENCIL},
    {BrushPt, BrushLn, BrushSize, PrimaryColor, NormalAlpha, TOOL_BRUSH},
    {EraserPt, NULL, BrushSize, BgColor, FullAlpha, TOOL_ERASER},
    {SprayPt, NULL, SpraySize, PrimaryColor, NormalAlpha, TOOL_AIRBRUSH},
};

static const Policy *GetPol(int tool){
    for(int i=0;i<(int)(sizeof(policies)/sizeof(policies[0]));i++)
        if(policies[i].id==tool) return &policies[i];
    return NULL;
}

static void DirtyRect(int x,int y,int r){
    RECT rc={x-r,y-r,x+r,y+r}; LayersMarkDirtyRect(&rc);
    RECT rs; RectBmpToScr(&rc,&rs); InflateRect(&rs,2,2); InvalidateCanvasRect(&rs);
}

static void DrawInterp(BYTE *b,int w,int h,const Policy *p,int x1,int y1,int x2,int y2,COLORREF c,BYTE a){
    int sz=p->sz();
    if(p->line) p->line(b,w,h,x1,y1,x2,y2,c,a,sz);
    else {
        int dx=x2-x1,dy=y2-y1,steps=abs(dx)+abs(dy);
        if(steps==0) p->pt(b,w,h,x2,y2,c,a,sz);
        else for(int i=1;i<=steps;i++) p->pt(b,w,h,x1+(dx*i)/steps,y1+(dy*i)/steps,c,a,sz);
    }
}

void FreehandTool_OnMouseDown(HWND hWnd,int x,int y,int btn,int tool){
    const Policy *p=GetPol(tool); if(!p||!p->pt) return;
    Interaction_Begin(hWnd,x,y,btn,tool);
    BYTE *b=LayersGetActiveColorBits(); if(!b){Interaction_Abort();return;}
    p->pt(b,Canvas_GetWidth(),Canvas_GetHeight(),x,y,p->color(btn),p->alpha(btn),p->sz());
    LayersMarkDirty(); Interaction_MarkModified();
    DirtyRect(x,y,DrawPrim_GetBrushSize(p->sz())+10);
}

void FreehandTool_OnMouseMove(HWND hWnd,int x,int y,int btn,int toolId){
    (void)hWnd;(void)toolId;
    if(!Interaction_IsActive()||!Interaction_IsActiveButton(btn)) return;
    const Policy *p=GetPol(Interaction_GetActiveToolId()); if(!p||!p->pt) return;
    BYTE *b=LayersGetActiveColorBits(); if(!b) return;
    POINT lp; Interaction_GetLastPoint(&lp);
    int cbtn=Interaction_GetDrawButton();
    DrawInterp(b,Canvas_GetWidth(),Canvas_GetHeight(),p,lp.x,lp.y,x,y,p->color(cbtn),p->alpha(cbtn));
    int r=DrawPrim_GetBrushSize(p->sz())+2;
    DirtyRect((min(lp.x,x)+max(lp.x,x))/2,(min(lp.y,y)+max(lp.y,y))/2,(max(lp.x,x)-min(lp.x,x))/2+r);
    Interaction_MarkModified(); Interaction_UpdateLastPoint(x,y);
    InvalidateCanvas();
}

void FreehandTool_OnMouseUp(HWND hWnd,int x,int y,int btn,int toolId){(void)hWnd;(void)x;(void)y;(void)btn;(void)toolId;Interaction_Commit("Draw");}

void AirbrushToolOnMouseDown(HWND hWnd,int x,int y,int btn){
    FreehandTool_OnMouseDown(hWnd,x,y,btn,TOOL_AIRBRUSH);
    if(Interaction_IsActive()&&hWnd) SetTimer(hWnd,TIMER_AIRBRUSH,30,NULL);
}

void AirbrushToolOnMouseMove(HWND hWnd,int x,int y,int btn){FreehandTool_OnMouseMove(hWnd,x,y,btn,TOOL_AIRBRUSH);}
void AirbrushToolOnMouseUp(HWND hWnd,int x,int y,int btn){if(hWnd)KillTimer(hWnd,TIMER_AIRBRUSH);FreehandTool_OnMouseUp(hWnd,x,y,btn,TOOL_AIRBRUSH);}

void FreehandTool_OnTimerTick(void){
    if(!Interaction_IsActive()||Interaction_GetActiveToolId()!=TOOL_AIRBRUSH) return;
    BYTE *b=LayersGetActiveColorBits(); if(!b) return;
    POINT lp; Interaction_GetLastPoint(&lp);
    int r=DrawPrim_GetSprayRadius(nSprayRadius)+2;
    DrawPrim_DrawSprayPoint(b,Canvas_GetWidth(),Canvas_GetHeight(),lp.x,lp.y,
        GetColorForButton(Interaction_GetDrawButton()),GetOpacityForButton(Interaction_GetDrawButton()),nSprayRadius);
    DirtyRect(lp.x,lp.y,r); Interaction_MarkModified();
}

void FreehandTool_Deactivate(void){if(Interaction_IsActive()){if(Interaction_GetActiveToolId()==TOOL_AIRBRUSH)KillTimer(GetCanvasWindow(),TIMER_AIRBRUSH);Interaction_EndQuiet();}}
BOOL CancelFreehandDrawing(void){BOOL d=Interaction_IsActive();if(d){if(Interaction_GetActiveToolId()==TOOL_AIRBRUSH)KillTimer(GetCanvasWindow(),TIMER_AIRBRUSH);Interaction_Abort();}return d;}
int GetActiveFreehandTool(void){return Interaction_GetActiveToolId();}
BOOL IsFreehandDrawing(void){return Interaction_IsActive();}