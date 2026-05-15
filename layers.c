/*------------------------------------------------------------
   LAYERS.C - Simplified Layer System
------------------------------------------------------------*/

#include "peztold_core.h"
#include "draw.h"
#include "gdi_utils.h"
#include "history.h"
#include "pixel_ops.h"
#include "layers.h"
#include "canvas.h"
#include <stdlib.h>
#include <string.h>

#define MAX_LAYERS 32
#define DEF_NAME "Layer"

typedef struct {
    HBITMAP bmp;
    BYTE *bits;
    BYTE opacity;
    int blend;
    BOOL visible;
    char name[32];
} Layer;

struct LayerSnap {
    int w, h, cnt, act;
    Layer layers[MAX_LAYERS];
};

static Layer *lyr = NULL;
static int lc = 0, act = 0, cap = 0;
static HBITMAP comp = NULL;
static BYTE *compBits = NULL;
static int compW = 0, compH = 0;
static BOOL dirty = TRUE;
static HBITMAP draft = NULL;
static BYTE *draftBits = NULL;
static int draftW = 0, draftH = 0;

void LayersMarkDirty(void) { dirty = TRUE; }
void LayersMarkDirtyRect(const RECT *r) { (void)r; dirty = TRUE; }

static void ClearLayer(Layer *l) { if(l->bmp){DeleteObject(l->bmp);l->bmp=NULL;l->bits=NULL;} }

static HBITMAP CreateLayerBmp(int ww, int hh, BYTE **bits) {
    /* Negative biHeight => top-down DIB. Document/tools use y=0 at top, y
       increasing downward; positive height would be bottom-up and invert Y
       relative to mouse mapping (CoordScrToBmp) and StretchBlt output. */
    BITMAPINFO bi = {sizeof(BITMAPINFOHEADER),ww,-hh,1,32,BI_RGB,0,0,0,0,0};
    void *ptr; HBITMAP h=CreateDIBSection(NULL,&bi,DIB_RGB_COLORS,&ptr,NULL,0);
    *bits=(BYTE*)ptr; return h;
}

BOOL LayersInit(int ww, int hh) {
    LayersDestroy();
    cap=4; lyr=calloc(cap,sizeof(Layer));
    lyr[0].bmp=CreateLayerBmp(ww,hh,&lyr[0].bits);
    memset(lyr[0].bits,0,ww*hh*4);
    lyr[0].opacity=255; lyr[0].blend=0; lyr[0].visible=TRUE;
    strncpy_s(lyr[0].name, sizeof(lyr[0].name), "Layer 1", _TRUNCATE);
    lc=1; act=0; dirty=TRUE;
    return TRUE;
}

void LayersDestroy(void) {
    for(int i=0;i<lc;i++) ClearLayer(&lyr[i]);
    free(lyr); lyr=NULL; lc=cap=act=0;
    if(comp){DeleteObject(comp);comp=NULL;compBits=NULL;}
    if(draft){DeleteObject(draft);draft=NULL;draftBits=NULL;}
    draftW=draftH=0; dirty=TRUE;
}

BOOL LayersResize(int ww, int hh) {
    if(ww<=0||hh<=0||lc<=0) return FALSE;
    for(int i=0;i<lc;i++){
        BYTE *ob=lyr[i].bits; HBITMAP obmp=lyr[i].bmp;
        int ow=Canvas_GetWidth(), oh=Canvas_GetHeight();
        HBITMAP nb=CreateLayerBmp(ww,hh,&lyr[i].bits);
        memset(lyr[i].bits,0,ww*hh*4);
        int cw=min(ow,ww), ch=min(oh,hh);
        for(int y=0;y<ch;y++) memcpy(lyr[i].bits+y*ww*4, ob+y*ow*4, cw*4);
        DeleteObject(obmp); lyr[i].bmp=nb;
    }
    if(draft){DeleteObject(draft);draft=NULL;draftBits=NULL;draftW=draftH=0;}
    dirty=TRUE; return TRUE;
}

int LayersGetCount(void) { return lc; }
int LayersGetActiveIndex(void) { return act; }
BOOL LayersSetActiveIndex(int i) { if(i<0||i>=lc) return FALSE; act=i; return TRUE; }

BOOL LayersAddLayer(const char *name) {
    if(lc>=MAX_LAYERS) return FALSE;
    if(cap<=lc){cap=cap?cap*2:4; if(cap>MAX_LAYERS)cap=MAX_LAYERS; Layer* nl=calloc(cap,sizeof(Layer)); memcpy(nl,lyr,lc*sizeof(Layer)); free(lyr);lyr=nl;}
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    lyr[lc].bmp=CreateLayerBmp(ww,hh,&lyr[lc].bits);
    memset(lyr[lc].bits,0,ww*hh*4);
    lyr[lc].opacity=255; lyr[lc].blend=0; lyr[lc].visible=TRUE;
    strncpy_s(lyr[lc].name, sizeof(lyr[lc].name), name && name[0] ? name : DEF_NAME, _TRUNCATE);
    act=lc++; dirty=TRUE; return TRUE;
}

BOOL LayersDeleteLayer(int i) {
    if(lc<=1 || i<0 || i>=lc) return FALSE;
    ClearLayer(&lyr[i]);
    for(;i<lc-1;i++) lyr[i]=lyr[i+1];
    lc--; if(act>=lc) act=lc-1; dirty=TRUE; return TRUE;
}

BOOL LayersMoveLayer(int f, int t) {
    if(f<0||f>=lc||t<0||t>=lc||f==t) return FALSE;
    Layer tmp=lyr[f];
    if(f<t){ for(int i=f;i<t;i++)lyr[i]=lyr[i+1]; }else{ for(int i=f;i>t;i--)lyr[i]=lyr[i-1]; }
    lyr[t]=tmp;
    if(act==f)act=t; else if(f<act&&t>=act)act--; else if(f>act&&t<=act)act++;
    dirty=TRUE; return TRUE;
}

BOOL LayersMergeDown(int i) {
    if(i<=0||i>=lc) return FALSE;
    Layer *s=&lyr[i], *d=&lyr[i-1];
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    for(int y=0;y<hh;y++) for(int x=0;x<ww;x++){
        BYTE *sp=s->bits+(y*ww+x)*4, *dp=d->bits+(y*ww+x)*4;
        if(!s->visible||s->opacity==0||sp[3]==0) continue;
        int a=sp[3]*s->opacity/255; if(a<=0) continue;
        PixelOps_BlendPixel(sp[2],sp[1],sp[0],a,dp,s->blend);
    }
    ClearLayer(s);
    for(;i<lc-1;i++)lyr[i]=lyr[i+1];
    lc--; if(act>=lc)act=lc-1; dirty=TRUE; return TRUE;
}

BOOL LayersGetVisible(int i){return i>=0&&i<lc?lyr[i].visible:FALSE;}
void LayersSetVisible(int i, BOOL v){if(i>=0&&i<lc){lyr[i].visible=v;dirty=TRUE;}}
BYTE LayersGetOpacity(int i){return i>=0&&i<lc?lyr[i].opacity:255;}
void LayersSetOpacity(int i, BYTE o){if(i>=0&&i<lc){lyr[i].opacity=o;dirty=TRUE;}}
int LayersGetBlendMode(int i){return i>=0&&i<lc?lyr[i].blend:0;}
void LayersSetBlendMode(int i, int b){if(i>=0&&i<lc){lyr[i].blend=b;dirty=TRUE;}}
void LayersGetName(int i, char *out, int sz){if(i>=0&&i<lc&&out&&sz>0)strncpy_s(out,sz,lyr[i].name,_TRUNCATE);}

void GetLayerDisplayName(int i, char *out, int sz) {
    if (!out || sz <= 0)
        return;
    LayersGetName(i, out, sz);
    if (!out[0])
        snprintf(out, (size_t)sz, "Layer %d", i + 1);
}
void HistoryPushLayerOpacity(int i,int o,int n){char nm[64];GetLayerDisplayName(i,nm,64);HistoryPushFormatted("Opacity: %d->%d (%s)",o,n,nm);}
void HistoryPushLayerVisibility(int i,BOOL o,BOOL n){char nm[64];GetLayerDisplayName(i,nm,64);HistoryPushFormatted("Visibility: %s->%s (%s)",o?"Visible":"Hidden",n?"Visible":"Hidden",nm);}
void HistoryPushLayerBlendMode(int i,int o,int n){char nm[64];static const char*bn[]={"Normal","Multiply","Screen","Overlay"};GetLayerDisplayName(i,nm,64);HistoryPushFormatted("Blend: %s->%s (%s)",bn[o<4?o:0],bn[n<4?n:0],nm);}
void HistoryPushLayerMove(int f,int t){char nm[64];GetLayerDisplayName(f,nm,64);HistoryPushFormatted(t<f?"Move Up: %s":"Move Down: %s",nm);}

HBITMAP LayersGetActiveColorBitmap(void){return lc>0&&act>=0&&act<lc?lyr[act].bmp:NULL;}
BYTE *LayersGetActiveColorBits(void){return lc>0&&act>=0&&act<lc?lyr[act].bits:NULL;}
BYTE *Layers_BeginWrite(void){History_SnapshotLayer(act); return LayersGetActiveColorBits();}
HDC LayersGetActiveColorDC(HBITMAP *ph){HBITMAP h=LayersGetActiveColorBitmap(); if(!h)return NULL; return GetBitmapDC(h,ph);}

static void Composite(BYTE *dst, int ww, int hh, BOOL check, COLORREF bg){
    if(check)PixelOps_FillCheckerboard(dst,ww,hh); else PixelOps_Fill(dst,ww,hh,bg,255);
    for(int i=0;i<lc;i++)if(lyr[i].visible && lyr[i].opacity && lyr[i].bits){
        for(int y=0;y<hh;y++) for(int x=0;x<ww;x++){
            BYTE *sp=lyr[i].bits+(y*ww+x)*4, *dp=dst+(y*ww+x)*4;
            if(sp[3]==0)continue;
            int a=sp[3]*lyr[i].opacity/255; if(a<=0)continue;
            PixelOps_BlendPixel(sp[2],sp[1],sp[0],a,dp,lyr[i].blend);
        }
        if(i==act && draftBits && draftW==ww && draftH==hh){
            for(int y=0;y<hh;y++) for(int x=0;x<ww;x++){
                BYTE *dp=dst+(y*ww+x)*4, *fp=draftBits+(y*ww+x)*4;
                if(fp[3]==0)continue;
                PixelOps_BlendPixel(fp[2],fp[1],fp[0],fp[3],dp,0);
            }
        }
    }
}

HBITMAP LayersGetCompositeBitmap(BOOL check){
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    if(!comp||compW!=ww||compH!=hh){if(comp)DeleteObject(comp); comp=CreateLayerBmp(ww,hh,&compBits); compW=ww; compH=hh;}
    if(dirty){Composite(compBits,ww,hh,check,Palette_GetSecondaryColor()); dirty=FALSE;}
    return comp;
}

HBITMAP LayersFlattenToBitmap(COLORREF bg){
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    BYTE *bits; HBITMAP bmp=CreateLayerBmp(ww,hh,&bits);
    if(bmp) Composite(bits,ww,hh,FALSE,bg);
    return bmp;
}

HBITMAP LayersFlattenToBitmapWithAlpha(BYTE **out){
    /* Intentionally omits the tool draft overlay: Composite() blends the draft for
       on-screen preview, while flatten/export represents committed layer pixels only. */
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    BYTE *bits; HBITMAP bmp=CreateLayerBmp(ww,hh,&bits);
    if(bmp){
        memset(bits,0,ww*hh*4);
        for(int i=0;i<lc;i++)if(lyr[i].visible&&lyr[i].opacity)
            for(int y=0;y<hh;y++) for(int x=0;x<ww;x++){
                BYTE *sp=lyr[i].bits+(y*ww+x)*4,*dp=bits+(y*ww+x)*4;
                if(sp[3]) PixelOps_BlendPixel(sp[2],sp[1],sp[0],sp[3]*lyr[i].opacity/255,dp,0);
            }
        if(out)*out=bits;
    }
    return bmp;
}

COLORREF LayersSampleCompositeColor(int x,int y,COLORREF bg){
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    if(x<0||y<0||x>=ww||y>=hh) return CLR_INVALID;
    BYTE *tmp; HBITMAP bm=CreateLayerBmp(ww,hh,&tmp);
    Composite(tmp,ww,hh,FALSE,bg);
    BYTE *p=tmp+(y*ww+x)*4;
    COLORREF result=RGB(p[2],p[1],p[0]);
    DeleteObject(bm);
    return result;
}

LayerSnapshot *LayersCreateSnapshot(void){
    if(lc<=0) return NULL;
    LayerSnapshot *s=calloc(1,sizeof(LayerSnapshot));
    if(!s) return NULL;
    s->width=Canvas_GetWidth(); s->height=Canvas_GetHeight();
    s->layerCount=lc; s->activeIndex=act;
    for(int i=0;i<lc;i++){
        s->layers[i].colorBmp=CreateLayerBmp(s->width,s->height,&s->layers[i].colorBits);
        memcpy(s->layers[i].colorBits,lyr[i].bits,s->width*s->height*4);
        s->layers[i].opacity=lyr[i].opacity; s->layers[i].blendMode=lyr[i].blend;
        s->layers[i].visible=lyr[i].visible; strncpy_s(s->layers[i].name,sizeof(s->layers[i].name),lyr[i].name,_TRUNCATE);
    }
    return s;
}

BOOL LayersApplySnapshot(LayerSnapshot *s){
    if(!s||s->layerCount<0||s->layerCount>MAX_LAYERS) return FALSE;
    for(int i=0;i<lc;i++)ClearLayer(&lyr[i]);
    if(cap<s->layerCount){cap=s->layerCount; free(lyr);lyr=calloc(cap,sizeof(Layer));}
    lc=s->layerCount; act=s->activeIndex; if(act>=lc) act=lc-1; if(act<0) act=0;
    for(int i=0;i<lc;i++){
        lyr[i].bmp=CreateLayerBmp(s->width,s->height,&lyr[i].bits);
        memcpy(lyr[i].bits,s->layers[i].colorBits,s->width*s->height*4);
        lyr[i].opacity=s->layers[i].opacity; lyr[i].blend=s->layers[i].blendMode;
        lyr[i].visible=s->layers[i].visible; strncpy_s(lyr[i].name,sizeof(lyr[i].name),s->layers[i].name,_TRUNCATE);
    }
    Canvas_SetWidth(s->width); Canvas_SetHeight(s->height);
    if(draft){DeleteObject(draft);draft=NULL;draftBits=NULL;draftW=draftH=0;}
    dirty=TRUE; return TRUE;
}

void LayersDestroySnapshot(LayerSnapshot *s){
    if(!s) return;
    for(int i=0;i<s->layerCount;i++) if(s->layers[i].colorBmp) DeleteObject(s->layers[i].colorBmp);
    free(s);
}

BOOL LayersLoadFromBitmap(HBITMAP hBmp){
    if(!hBmp) return FALSE;
    BITMAP bm; GetObject(hBmp,sizeof(bm),&bm);
    LayersDestroy(); cap=4; lyr=calloc(cap,sizeof(Layer));
    lyr[0].bmp=CreateLayerBmp(bm.bmWidth,bm.bmHeight,&lyr[0].bits);
    HDC srcDC=CreateCompatibleDC(NULL), dstDC=CreateCompatibleDC(NULL);
    HBITMAP os=SelectObject(srcDC,hBmp), od=SelectObject(dstDC,lyr[0].bmp);
    BitBlt(dstDC,0,0,bm.bmWidth,bm.bmHeight,srcDC,0,0,SRCCOPY);
    SelectObject(srcDC,os); SelectObject(dstDC,od); DeleteDC(srcDC); DeleteDC(dstDC);
    int cnt=bm.bmWidth*bm.bmHeight;
    for(int i=0;i<cnt;i++)lyr[0].bits[i*4+3]=255;
    lc=1; act=0; Canvas_SetWidth(bm.bmWidth); Canvas_SetHeight(bm.bmHeight); dirty=TRUE;
    return TRUE;
}

BOOL LayersLoadFromPixels(int ww,int hh,const BYTE *bgra,int stride,BOOL useAlpha){
    if(!bgra||ww<=0||hh<=0) return FALSE;
    LayersDestroy(); cap=1; lyr=calloc(1,sizeof(Layer));
    lyr[0].bmp=CreateLayerBmp(ww,hh,&lyr[0].bits);
    for(int y=0;y<hh;y++){const BYTE *sr=bgra+y*stride; BYTE *ds=lyr[0].bits+y*ww*4;
        for(int x=0;x<ww;x++){ds[x*4]=sr[x*4]; ds[x*4+1]=sr[x*4+1]; ds[x*4+2]=sr[x*4+2]; ds[x*4+3]=useAlpha?sr[x*4+3]:255;}}
    lc=1; act=0; Canvas_SetWidth(ww); Canvas_SetHeight(hh); dirty=TRUE;
    return TRUE;
}

BOOL LayersApplyRawTransformToAll(int nw,int nh,RawBitmapTransformFunc fn,void *pUserData){
    if(!fn||lc<=0) return FALSE;
    int ow=Canvas_GetWidth(), oh=Canvas_GetHeight();
    for(int i=0;i<lc;i++){
        BYTE *obits=lyr[i].bits; HBITMAP obmp=lyr[i].bmp;
        HBITMAP nb=CreateLayerBmp(nw,nh,&lyr[i].bits);
        fn(obits,ow,oh,lyr[i].bits,nw,nh,pUserData);
        BYTE *nbits=lyr[i].bits; DeleteObject(obmp); lyr[i].bmp=nb; lyr[i].bits=nbits;
    }
    Canvas_SetWidth(nw); Canvas_SetHeight(nh);
    if(draft){DeleteObject(draft);draft=NULL;draftBits=NULL;draftW=draftH=0;}
    dirty=TRUE; return TRUE;
}

/* Draft Layer API */
void LayersEnsureDraft(void){
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    if(draft && draftW==ww && draftH==hh) return;
    if(draft){DeleteObject(draft);draft=NULL;draftBits=NULL;}
    draft=CreateLayerBmp(ww,hh,&draftBits); draftW=ww; draftH=hh;
    if(draftBits) memset(draftBits,0,ww*hh*4);
}
BYTE *LayersGetDraftBits(void){LayersEnsureDraft(); return draftBits;}
void LayersClearDraft(void){if(draftBits && draftW && draftH){memset(draftBits,0,draftW*draftH*4); dirty=TRUE;}}
BOOL LayersIsDraftDirty(void){
    if(!draftBits||!draftW||!draftH) return FALSE;
    int n=draftW*draftH; for(int i=0;i<n;i++)if(draftBits[i*4+3])return TRUE;
    return FALSE;
}
void LayersMergeDraftToActive(void){
    if(!draftBits||draftW!=Canvas_GetWidth()||draftH!=Canvas_GetHeight()) return;
    BYTE *ab=LayersGetActiveColorBits(); if(!ab) return;
    for(int i=0;i<draftW*draftH;i++){
        BYTE *d=draftBits+i*4, *a=ab+i*4;
        if(d[3]==0)continue; PixelOps_BlendPixel(d[2],d[1],d[0],d[3],a,0);
    }
    memset(draftBits,0,draftW*draftH*4); dirty=TRUE;
}
