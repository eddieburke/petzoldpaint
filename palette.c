#include "palette.h"
#include "gdi_utils.h"
#include <math.h>
#include <stdio.h>

static COLORREF customColors[16];
static COLORREF g_primaryColor = RGB(0, 0, 0);
static COLORREF g_secondaryColor = RGB(255, 255, 255);
static BYTE g_primaryOpacity = 255;
static BYTE g_secondaryOpacity = 255;

void SetCustomColors(const COLORREF* colors) {
    for (int i = 0; i < 16; i++) customColors[i] = colors[i];
}

static RGBA RGBA_FromColorRef(COLORREF c) {
    RGBA rgba;
    rgba.r = GetRValue(c);
    rgba.g = GetGValue(c);
    rgba.b = GetBValue(c);
    rgba.a = 255;
    return rgba;
}

COLORREF* Palette_GetPrimaryColorPtr(void) { return &g_primaryColor; }
COLORREF* Palette_GetSecondaryColorPtr(void) { return &g_secondaryColor; }

COLORREF Palette_GetPrimaryColor(void) { return g_primaryColor; }
void Palette_SetPrimaryColor(COLORREF c) { g_primaryColor = c; }
BYTE Palette_GetPrimaryOpacity(void) { return g_primaryOpacity; }
void Palette_SetPrimaryOpacity(BYTE a) { g_primaryOpacity = a; }

COLORREF Palette_GetSecondaryColor(void) { return g_secondaryColor; }
void Palette_SetSecondaryColor(COLORREF c) { g_secondaryColor = c; }
BYTE Palette_GetSecondaryOpacity(void) { return g_secondaryOpacity; }
void Palette_SetSecondaryOpacity(BYTE a) { g_secondaryOpacity = a; }

/* ---- Custom Color Picker Dialog with Alpha ---- */

#define CPICKER_W 286
#define CPICKER_H 340
#define CPICKER_HUE_W 24
#define CPICKER_MARGIN 10
#define CPICKER_SV 200
#define CPICKER_ALPHA_H 18
#define CPICKER_BTN_W 80
#define CPICKER_BTN_H 24

typedef struct {
    float hue, sat, val;
    BYTE alpha;
    BOOL dragSV, dragHue, dragAlpha;
    HBITMAP svBmp;
    BYTE* svBits;
    BOOL ok;
    BOOL* pModalRunning; /* ends nested modal loop without WM_QUIT */
} CPState;

static void HSVtoRGB(float h, float s, float v, BYTE* r, BYTE* g, BYTE* b) {
    float c = v * s, x = c * (1 - fabsf(fmodf(h / 60, 2) - 1)), m = v - c;
    float r1=0,g1=0,b1=0;
    if (h<60){r1=c;g1=x;} else if(h<120){r1=x;g1=c;} else if(h<180){g1=c;b1=x;}
    else if(h<240){g1=x;b1=c;} else if(h<300){r1=x;b1=c;} else{r1=c;b1=x;}
    *r=(BYTE)((r1+m)*255); *g=(BYTE)((g1+m)*255); *b=(BYTE)((b1+m)*255);
}

static void RGBtoHSV(BYTE r, BYTE g, BYTE b, float* h, float* s, float* v) {
    float rf=r/255.f,gf=g/255.f,bf=b/255.f;
    float mx=rf,mn=rf;
    if(gf>mx)mx=gf; if(bf>mx)mx=bf; if(gf<mn)mn=gf; if(bf<mn)mn=bf;
    float d=mx-mn; *v=mx; *s=(mx>0.001f)?d/mx:0;
    if(d<0.001f){*h=0;return;}
    if(mx==rf)*h=60*fmodf((gf-bf)/d,6); else if(mx==gf)*h=60*((bf-rf)/d+2); else *h=60*((rf-gf)/d+4);
    if(*h<0)*h+=360;
}

static void RegenSV(CPState* cp) {
    if (!cp->svBmp)         cp->svBmp = CreateDibSection32(CPICKER_SV, CPICKER_SV, &cp->svBits);
    if (!cp->svBits) return;
    BYTE cr,cg,cb; HSVtoRGB(cp->hue,1,1,&cr,&cg,&cb);
    for (int y=0;y<CPICKER_SV;y++) for (int x=0;x<CPICKER_SV;x++) {
        float s=x/(float)(CPICKER_SV-1), v=1-y/(float)(CPICKER_SV-1);
        BYTE r,g,b; HSVtoRGB(cp->hue,s,v,&r,&g,&b);
        int i=(y*CPICKER_SV+x)*4; cp->svBits[i]=b; cp->svBits[i+1]=g; cp->svBits[i+2]=r; cp->svBits[i+3]=255;
    }
}

static void DrawChecker(HDC hdc, RECT* rc, int sz) {
    for (int y=rc->top;y<rc->bottom;y+=sz) for (int x=rc->left;x<rc->right;x+=sz) {
        RECT t={x,y,min(x+sz,rc->right),min(y+sz,rc->bottom)};
        BOOL dk=(((x-rc->left)/sz)+((y-rc->top)/sz))&1;
        HBRUSH h=CreateSolidBrush(dk?RGB(180,180,180):RGB(230,230,230)); FillRect(hdc,&t,h); DeleteObject(h);
    }
}

static LRESULT CALLBACK CPWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CPState* cp = (CPState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!cp) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:
        cp = (CPState*)((CREATESTRUCT*)lp)->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cp);
        RegenSV(cp);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        HDC mem = CreateCompatibleDC(hdc); HBITMAP hOld;
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_BTNFACE+1));
        int M = CPICKER_MARGIN, SV = CPICKER_SV, HW = CPICKER_HUE_W;

        hOld = SelectObject(mem, cp->svBmp);
        BitBlt(hdc, M, M, SV, SV, mem, 0, 0, SRCCOPY);
        SelectObject(mem, hOld);

        int cx = M + (int)(cp->sat*(SV-1)), cy = M + (int)((1-cp->val)*(SV-1));
        HPEN pen = CreatePen(PS_SOLID, 2, cp->val>0.5?0:0xFFFFFF);
        HPEN oPen = SelectObject(hdc, pen);
        HBRUSH oBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Ellipse(hdc, cx-5, cy-5, cx+6, cy+6);
        SelectObject(hdc, oPen); SelectObject(hdc, oBr); DeleteObject(pen);

        int hx = M+SV+M;
        for (int i=0;i<SV;i++) {
            BYTE r,g,b; HSVtoRGB(360.f*i/(SV-1),1,1,&r,&g,&b);
            RECT rl={hx,M+i,hx+HW,M+i+1}; HBRUSH hb=CreateSolidBrush(RGB(r,g,b)); FillRect(hdc,&rl,hb); DeleteObject(hb);
        }
        int hY = M + (int)(cp->hue/360*(SV-1));
        RECT hc={hx-2,hY-3,hx+HW+2,hY+4}; DrawEdge(hdc,&hc,BDR_RAISEDINNER,BF_RECT);

        int ay = M+SV+M;
        RECT aR={M,ay,M+SV,ay+CPICKER_ALPHA_H};
        DrawChecker(hdc,&aR,6);
        BYTE pr,pg,pb; HSVtoRGB(cp->hue,cp->sat,cp->val,&pr,&pg,&pb);
        for (int i=0;i<SV;i++) {
            BYTE a=(BYTE)(i*255/(SV-1));
            RECT px={aR.left+i,ay,aR.left+i+1,ay+CPICKER_ALPHA_H};
            HBRUSH hb=CreateSolidBrush(RGB((pr*a+255*(255-a))/255,(pg*a+255*(255-a))/255,(pb*a+255*(255-a))/255));
            FillRect(hdc,&px,hb); DeleteObject(hb);
        }
        DrawEdge(hdc,&aR,BDR_SUNKENOUTER,BF_RECT);
        int aX = aR.left + (int)(cp->alpha*(SV-1)/255);
        RECT ac={aX-2,ay-2,aX+3,ay+CPICKER_ALPHA_H+2}; DrawEdge(hdc,&ac,BDR_RAISEDINNER,BF_RECT);

        int py = ay+CPICKER_ALPHA_H+M;
        RECT pR={M,py,M+50,py+32};
        DrawChecker(hdc,&pR,8);
        for (int y2=pR.top;y2<pR.bottom;y2+=4) for (int x2=pR.left;x2<pR.right;x2+=4) {
            RECT t={x2,y2,min(x2+4,pR.right),min(y2+4,pR.bottom)};
            BOOL dk=(((x2-pR.left)/4)+((y2-pR.top)/4))&1;
            COLORREF bg=dk?RGB(180,180,180):RGB(230,230,230);
            HBRUSH hb=CreateSolidBrush(RGB((pr*cp->alpha+GetRValue(bg)*(255-cp->alpha))/255,
                                            (pg*cp->alpha+GetGValue(bg)*(255-cp->alpha))/255,
                                            (pb*cp->alpha+GetBValue(bg)*(255-cp->alpha))/255));
            FillRect(hdc,&t,hb); DeleteObject(hb);
        }
        DrawEdge(hdc,&pR,BDR_SUNKENOUTER,BF_RECT);

        char lbl[64]; _snprintf(lbl,sizeof(lbl),"R:%d G:%d B:%d A:%d",pr,pg,pb,cp->alpha);
        RECT tR={M+58,py+4,M+58+170,py+28};
        HFONT hf=(HFONT)GetStockObject(DEFAULT_GUI_FONT); HFONT oF=SelectObject(hdc,hf);
        DrawTextA(hdc,lbl,-1,&tR,DT_LEFT|DT_VCENTER|DT_SINGLELINE); SelectObject(hdc,oF);

        RECT bOK={M,py+42,M+CPICKER_BTN_W,py+42+CPICKER_BTN_H};
        RECT bCN={M+CPICKER_BTN_W+8,py+42,M+CPICKER_BTN_W*2+8,py+42+CPICKER_BTN_H};
        DrawFrameControl(hdc,&bOK,DFC_BUTTON,DFCS_BUTTONPUSH);
        DrawFrameControl(hdc,&bCN,DFC_BUTTON,DFCS_BUTTONPUSH);
        DrawTextA(hdc,"OK",-1,&bOK,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        DrawTextA(hdc,"Cancel",-1,&bCN,DT_CENTER|DT_VCENTER|DT_SINGLELINE);

        DeleteDC(mem); EndPaint(hwnd,&ps); return 0;
    }

    case WM_LBUTTONDOWN: {
        int mx=(short)LOWORD(lp), my=(short)HIWORD(lp);
        int M=CPICKER_MARGIN,SV=CPICKER_SV,HW=CPICKER_HUE_W;
        int hx=M+SV+M, ay=M+SV+M, py=ay+CPICKER_ALPHA_H+M;

        if (mx>=hx && mx<hx+HW && my>=M && my<M+SV) {
            cp->dragHue=TRUE; cp->hue=360.f*(my-M)/(SV-1); RegenSV(cp); SetCapture(hwnd);
        } else if (mx>=M && mx<M+SV && my>=M && my<M+SV) {
            cp->dragSV=TRUE; cp->sat=max(0.f,min(1.f,(mx-M)/(float)(SV-1)));
            cp->val=max(0.f,min(1.f,1.f-(my-M)/(float)(SV-1))); SetCapture(hwnd);
        } else if (mx>=M && mx<M+SV && my>=ay && my<ay+CPICKER_ALPHA_H) {
            cp->dragAlpha=TRUE; cp->alpha=(BYTE)max(0,min(255,(mx-M)*255/(SV-1))); SetCapture(hwnd);
        } else if (mx>=M && mx<M+CPICKER_BTN_W && my>=py+42 && my<py+42+CPICKER_BTN_H) {
            cp->ok=TRUE; DestroyWindow(hwnd);
        } else if (mx>=M+CPICKER_BTN_W+8 && mx<M+CPICKER_BTN_W*2+8 && my>=py+42 && my<py+42+CPICKER_BTN_H) {
            cp->ok=FALSE; DestroyWindow(hwnd);
        }
        InvalidateRect(hwnd,NULL,FALSE); return 0;
    }

    case WM_MOUSEMOVE: {
        if (!(cp->dragSV||cp->dragHue||cp->dragAlpha)) return 0;
        int mx=(short)LOWORD(lp), my=(short)HIWORD(lp), M=CPICKER_MARGIN, SV=CPICKER_SV;
        if (cp->dragSV) {
            cp->sat=max(0.f,min(1.f,(mx-M)/(float)(SV-1)));
            cp->val=max(0.f,min(1.f,1.f-(my-M)/(float)(SV-1)));
        } else if (cp->dragHue) {
            cp->hue=max(0.f,min(360.f,360.f*(my-M)/(SV-1))); RegenSV(cp);
        } else if (cp->dragAlpha) {
            cp->alpha=(BYTE)max(0,min(255,(mx-M)*255/(SV-1)));
        }
        InvalidateRect(hwnd,NULL,FALSE); return 0;
    }

    case WM_LBUTTONUP:
        cp->dragSV=FALSE; cp->dragHue=FALSE; cp->dragAlpha=FALSE;
        if (GetCapture()==hwnd) ReleaseCapture(); return 0;

    case WM_CAPTURECHANGED:
        cp->dragSV=FALSE; cp->dragHue=FALSE; cp->dragAlpha=FALSE; return 0;

    case WM_DESTROY:
        if (cp->svBmp) { DeleteObject(cp->svBmp); cp->svBmp=NULL; cp->svBits=NULL; }
        if (cp->pModalRunning)
            *cp->pModalRunning = FALSE;
        return 0;

    case WM_KEYDOWN:
        if (wp==VK_ESCAPE) { cp->ok=FALSE; DestroyWindow(hwnd); return 0; }
        if (wp==VK_RETURN) { cp->ok=TRUE; DestroyWindow(hwnd); return 0; }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

BOOL ChooseColorDialog(HWND hWnd, COLORREF* color) {
    RGBA rgba = RGBA_FromColorRef(*color);
    rgba.a = 255;
    if (Palette_ChooseColorWithAlpha(hWnd, &rgba)) {
        *color = RGB(rgba.r, rgba.g, rgba.b);
        return TRUE;
    }
    return FALSE;
}

BOOL Palette_ChooseColorWithAlpha(HWND hWnd, RGBA* outRgba) {
    CPState cp = {0};
    cp.hue=0; cp.sat=0; cp.val=1; cp.alpha=255; cp.ok=FALSE;
    if (outRgba) {
        RGBtoHSV(outRgba->r, outRgba->g, outRgba->b, &cp.hue, &cp.sat, &cp.val);
        cp.alpha = outRgba->a;
    }
    BOOL modalRunning = TRUE;
    cp.pModalRunning = &modalRunning;

    WNDCLASS wc={0};
    wc.lpfnWndProc=CPWndProc; wc.hInstance=hInst; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); wc.lpszClassName="PeztoldColorPicker";
    if (!GetClassInfo(hInst,"PeztoldColorPicker",&wc)) RegisterClass(&wc);

    RECT pr; GetWindowRect(hWnd,&pr);
    int px=pr.left+(pr.right-pr.left-CPICKER_W)/2, py=pr.top+(pr.bottom-pr.top-CPICKER_H)/2;
    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME|WS_EX_WINDOWEDGE,
        "PeztoldColorPicker","Color Picker",WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        px,py,CPICKER_W,CPICKER_H,hWnd,NULL,hInst,&cp);
    if (!hDlg) return FALSE;

    EnableWindow(hWnd, FALSE);
    MSG msg;
    while (modalRunning && GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(hWnd, TRUE);

    BOOL ok = cp.ok;
    if (ok && outRgba) {
        HSVtoRGB(cp.hue, cp.sat, cp.val, &outRgba->r, &outRgba->g, &outRgba->b);
        outRgba->a = cp.alpha;
    }
    return ok;
}
