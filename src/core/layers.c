#include "peztold_core.h"
#include "pixel_ops.h"
#include "layers.h"
#include "canvas.h"
#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEF_NAME "Layer"

typedef struct {
    HBITMAP bmp;
    BYTE *bits;
    BYTE opacity;
    int blend;
    BOOL visible;
    char name[32];
} Layer;

static Layer *lyr = NULL;
static int lc = 0, act = 0, cap = 0;
static HBITMAP comp = NULL;
static BYTE *compBits = NULL;
static int compW = 0, compH = 0;
static BOOL dirty = TRUE;
static HBITMAP draft = NULL;
static BYTE *draftBits = NULL;
static int draftW = 0, draftH = 0;
static BOOL draftPixels = FALSE;
static BOOL fullCompositeDirty = TRUE;
static BOOL dirtyRectValid = FALSE;
static RECT dirtyRect;

/* Structural change (visibility, opacity, add/remove/etc.) — must force full
   recompose so any previous partial dirty rect doesn't leave stale pixels. */
static void MarkFullDirty(void) {
    dirty = TRUE;
    fullCompositeDirty = TRUE;
    dirtyRectValid = FALSE;
}

static void UnionDirtyRect(int left, int top, int right, int bottom) {
    if (left >= right || top >= bottom)
        return;
    if (!dirtyRectValid) {
        dirtyRect.left = left;
        dirtyRect.top = top;
        dirtyRect.right = right;
        dirtyRect.bottom = bottom;
        dirtyRectValid = TRUE;
        return;
    }
    if (left < dirtyRect.left)
        dirtyRect.left = left;
    if (top < dirtyRect.top)
        dirtyRect.top = top;
    if (right > dirtyRect.right)
        dirtyRect.right = right;
    if (bottom > dirtyRect.bottom)
        dirtyRect.bottom = bottom;
}

void LayersMarkDirty(void) {
    dirty = TRUE;
    fullCompositeDirty = TRUE;
    dirtyRectValid = FALSE;
}

void LayersMarkDirtyRect(int left, int top, int right, int bottom) {
    dirty = TRUE;
    if (fullCompositeDirty)
        return;
    UnionDirtyRect(left, top, right, bottom);
}

void LayersMarkDraftDirty(void) { draftPixels = TRUE; }

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
    if (!lyr[0].bmp) {
        free(lyr);
        lyr = NULL;
        cap = 0;
        return FALSE;
    }
    memset(lyr[0].bits,0,ww*hh*4);
    lyr[0].opacity=255; lyr[0].blend=0; lyr[0].visible=TRUE;
    strncpy_s(lyr[0].name, sizeof(lyr[0].name), "Layer 1", _TRUNCATE);
    lc=1; act=0; MarkFullDirty();
    return TRUE;
}

void LayersDestroy(void) {
    for(int i=0;i<lc;i++) ClearLayer(&lyr[i]);
    free(lyr); lyr=NULL; lc=cap=act=0;
    if(comp){DeleteObject(comp);comp=NULL;compBits=NULL;}
    if(draft){DeleteObject(draft);draft=NULL;draftBits=NULL;}
    draftW=draftH=0; MarkFullDirty();
}

BOOL LayersResize(int ww, int hh) {
    if(ww<=0||hh<=0||lc<=0) return FALSE;

    if ((size_t)ww > (SIZE_MAX / 4 / (size_t)hh)) {
        OutputDebugStringA("LayersResize: dimensions would overflow size_t\n");
        return FALSE;
    }

    int ow = Canvas_GetWidth(), oh = Canvas_GetHeight();
    HBITMAP *oldBmp = calloc(lc, sizeof(HBITMAP));
    BYTE **oldBits = calloc(lc, sizeof(BYTE *));
    HBITMAP *newBmp = calloc(lc, sizeof(HBITMAP));
    BYTE **newBits = calloc(lc, sizeof(BYTE *));
    if (!oldBmp || !oldBits || !newBmp || !newBits) {
        free(oldBmp); free(oldBits); free(newBmp); free(newBits);
        return FALSE;
    }

    BOOL ok = TRUE;
    for (int i = 0; i < lc; i++) {
        oldBmp[i] = lyr[i].bmp;
        oldBits[i] = lyr[i].bits;
        newBmp[i] = CreateLayerBmp(ww, hh, &newBits[i]);
        if (!newBmp[i]) { ok = FALSE; break; }
    }

    if (!ok) {
        for (int i = 0; i < lc; i++)
            if (newBmp[i]) DeleteObject(newBmp[i]);
        free(oldBmp); free(oldBits); free(newBmp); free(newBits);
        return FALSE;
    }

    int cw = min(ow, ww), ch = min(oh, hh);
    for (int i = 0; i < lc; i++) {
        memset(newBits[i], 0, (size_t)ww * (size_t)hh * 4);
        for (int y = 0; y < ch; y++)
            memcpy(newBits[i] + (size_t)y * ww * 4,
                   oldBits[i] + (size_t)y * ow * 4, (size_t)cw * 4);
        DeleteObject(oldBmp[i]);
        lyr[i].bmp = newBmp[i];
        lyr[i].bits = newBits[i];
    }
    free(oldBmp); free(oldBits); free(newBmp); free(newBits);

    if(draft){DeleteObject(draft);draft=NULL;draftBits=NULL;draftW=draftH=0;}
    MarkFullDirty();
    return TRUE;
}

int LayersGetCount(void) { return lc; }
int LayersGetActiveIndex(void) { return act; }
BOOL LayersSetActiveIndex(int i) { if(i<0||i>=lc) return FALSE; act=i; return TRUE; }

BOOL LayersAddLayer(const char *name) {
    if (lc >= MAX_LAYERS)
        return FALSE;
    if (cap <= lc) {
        int ncap = cap ? cap * 2 : 4;
        Layer *nl = calloc(ncap, sizeof(Layer));
        if (!nl) return FALSE;
        if (lyr && lc > 0) memcpy(nl, lyr, lc * sizeof(Layer));
        free(lyr);
        lyr = nl;
        cap = ncap;
    }
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    lyr[lc].bmp=CreateLayerBmp(ww,hh,&lyr[lc].bits);
    if (!lyr[lc].bmp) return FALSE;
    memset(lyr[lc].bits,0,(size_t)ww*hh*4);
    lyr[lc].opacity=255; lyr[lc].blend=0; lyr[lc].visible=TRUE;
    strncpy_s(lyr[lc].name, sizeof(lyr[lc].name), name && name[0] ? name : DEF_NAME, _TRUNCATE);
    act=lc++; MarkFullDirty(); return TRUE;
}

BOOL LayersDeleteLayer(int i) {
    if(lc<=1 || i<0 || i>=lc) return FALSE;
    ClearLayer(&lyr[i]);
    for(;i<lc-1;i++) lyr[i]=lyr[i+1];
    lc--; if(act>=lc) act=lc-1; MarkFullDirty(); return TRUE;
}

BOOL LayersMoveLayer(int f, int t) {
    if(f<0||f>=lc||t<0||t>=lc||f==t) return FALSE;
    Layer tmp=lyr[f];
    if(f<t){ for(int i=f;i<t;i++)lyr[i]=lyr[i+1]; }else{ for(int i=f;i>t;i--)lyr[i]=lyr[i-1]; }
    lyr[t]=tmp;
    if(act==f)act=t; else if(f<act&&t>=act)act--; else if(f>act&&t<=act)act++;
    MarkFullDirty(); return TRUE;
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
    lc--; if(act>=lc)act=lc-1; MarkFullDirty(); return TRUE;
}

BOOL LayersGetVisible(int i){return i>=0&&i<lc?lyr[i].visible:FALSE;}
BOOL LayersSetVisible(int i, BOOL v){if(i<0||i>=lc||lyr[i].visible==v)return FALSE;lyr[i].visible=v;MarkFullDirty();return TRUE;}
BYTE LayersGetOpacity(int i){return i>=0&&i<lc?lyr[i].opacity:255;}
BOOL LayersSetOpacity(int i, BYTE o){if(i<0||i>=lc||lyr[i].opacity==o)return FALSE;lyr[i].opacity=o;MarkFullDirty();return TRUE;}
int LayersGetBlendMode(int i){return i>=0&&i<lc?lyr[i].blend:0;}
BOOL LayersSetBlendMode(int i, int b){if(i<0||i>=lc||lyr[i].blend==b)return FALSE;lyr[i].blend=b;MarkFullDirty();return TRUE;}
void LayersGetName(int i, char *out, int sz){if(i>=0&&i<lc&&out&&sz>0)strncpy_s(out,sz,lyr[i].name,_TRUNCATE);}

static void LogLastError(const char* msg) {
    char buf[256];
    snprintf(buf, sizeof(buf), "PetzoldPaint Error: %s (Error %lu)\n", msg, GetLastError());
    OutputDebugStringA(buf);
}

HBITMAP LayersGetActiveColorBitmap(void){return lc>0&&act>=0&&act<lc?lyr[act].bmp:NULL;}
BYTE *LayersGetActiveColorBits(void){return lc>0&&act>=0&&act<lc?lyr[act].bits:NULL;}
BYTE *Layers_BeginWrite(void) { return LayersGetActiveColorBits(); }

static void CompositeFillBackground(BYTE *dst, int ww, int hh, int x0, int y0,
                                    int x1, int y1, BOOL check, COLORREF bg) {
    if (check)
        PixelOps_FillCheckerboardRect(dst, ww, hh, x0, y0, x1, y1);
    else
        PixelOps_FillRect(dst, ww, hh, bg, 255, x0, y0, x1, y1);
}

static void CompositePixelAt(BYTE *dst, int ww, int hh, int x, int y,
                             BOOL blendDraft) {
    BYTE *dp = dst + (y * ww + x) * 4;
    int i;

    for (i = 0; i < lc; i++) {
        if (!lyr[i].visible || !lyr[i].opacity || !lyr[i].bits)
            continue;
        BYTE *sp = lyr[i].bits + (y * ww + x) * 4;
        if (sp[3] == 0)
            continue;
        int a = sp[3] * lyr[i].opacity / 255;
        if (a <= 0)
            continue;
        PixelOps_BlendPixel(sp[2], sp[1], sp[0], a, dp, lyr[i].blend);
    }
    if (blendDraft && draftBits && draftW == ww && draftH == hh) {
        BYTE *fp = draftBits + (y * ww + x) * 4;
        if (fp[3])
            PixelOps_BlendPixel(fp[2], fp[1], fp[0], fp[3], dp, 0);
    }
}

static const Layer *GetSingleOpaqueNormalLayer(void) {
    const Layer *single = NULL;
    int i;
    for (i = 0; i < lc; i++) {
        if (!lyr[i].visible || !lyr[i].opacity || !lyr[i].bits)
            continue;
        if (lyr[i].opacity != 255 || lyr[i].blend != LAYER_BLEND_NORMAL)
            return NULL;
        if (single)
            return NULL;
        single = &lyr[i];
    }
    return single;
}

static void CompositeRegionSingleLayer(BYTE *dst, int ww, int hh, int x0, int y0,
                                       int x1, int y1, const Layer *layer,
                                       BOOL blendDraft) {
    BOOL useDraft = blendDraft && draftBits && draftW == ww && draftH == hh;
    int x, y;
    for (y = y0; y < y1; y++) {
        BYTE *dp = dst + ((size_t)y * (size_t)ww + (size_t)x0) * 4;
        BYTE *sp = layer->bits + ((size_t)y * (size_t)ww + (size_t)x0) * 4;
        BYTE *fp = useDraft
                       ? (draftBits + ((size_t)y * (size_t)ww + (size_t)x0) * 4)
                       : NULL;
        for (x = x0; x < x1; x++) {
            BYTE sa = sp[3];
            if (sa == 255) {
                dp[0] = sp[0];
                dp[1] = sp[1];
                dp[2] = sp[2];
                dp[3] = 255;
            } else if (sa != 0) {
                PixelOps_BlendPixel(sp[2], sp[1], sp[0], sa, dp, LAYER_BLEND_NORMAL);
            }
            if (fp && fp[3]) {
                PixelOps_BlendPixel(fp[2], fp[1], fp[0], fp[3], dp,
                                    LAYER_BLEND_NORMAL);
            }
            dp += 4;
            sp += 4;
            if (fp)
                fp += 4;
        }
    }
}

static void CompositeRegion(BYTE *dst, int ww, int hh, int x0, int y0, int x1,
                            int y1, BOOL check, COLORREF bg) {
    const Layer *fastLayer;
    int y, x;

    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > ww)
        x1 = ww;
    if (y1 > hh)
        y1 = hh;
    if (x0 >= x1 || y0 >= y1)
        return;

    CompositeFillBackground(dst, ww, hh, x0, y0, x1, y1, check, bg);
    fastLayer = GetSingleOpaqueNormalLayer();
    if (fastLayer) {
        CompositeRegionSingleLayer(dst, ww, hh, x0, y0, x1, y1, fastLayer, TRUE);
        return;
    }
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            CompositePixelAt(dst, ww, hh, x, y, TRUE);
}

static void Composite(BYTE *dst, int ww, int hh, BOOL check, COLORREF bg) {
    CompositeRegion(dst, ww, hh, 0, 0, ww, hh, check, bg);
}

HBITMAP LayersGetCompositeBitmap(void) {
    const BOOL check = TRUE;
    int ww = Canvas_GetWidth(), hh = Canvas_GetHeight();
    COLORREF bg = Palette_GetSecondaryColor();
    if (!comp || compW != ww || compH != hh) {
        if (comp)
            DeleteObject(comp);
        comp = CreateLayerBmp(ww, hh, &compBits);
        if (!comp || !compBits) {
            comp = NULL;
            compBits = NULL;
            compW = compH = 0;
            return NULL;
        }
        compW = ww;
        compH = hh;
        fullCompositeDirty = TRUE;
    }
    if (dirty) {
        if (fullCompositeDirty || !dirtyRectValid || !compBits) {
            Composite(compBits, ww, hh, check, bg);
        } else {
            CompositeRegion(compBits, ww, hh, dirtyRect.left, dirtyRect.top,
                            dirtyRect.right, dirtyRect.bottom, check, bg);
        }
        dirty = FALSE;
        fullCompositeDirty = FALSE;
        dirtyRectValid = FALSE;
    }
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

COLORREF LayersSampleCompositeColor(int x,int y){
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    if(x<0||y<0||x>=ww||y>=hh) return CLR_INVALID;
    BYTE px[4]={0,0,0,0};
    for(int i=0;i<lc;i++){
        if(!lyr[i].visible||!lyr[i].opacity||!lyr[i].bits) continue;
        BYTE *sp=lyr[i].bits+(y*ww+x)*4;
        if(!sp[3]) continue;
        int a=sp[3]*lyr[i].opacity/255;
        if(a<=0) continue;
        PixelOps_BlendPixel(sp[2],sp[1],sp[0],a,px,lyr[i].blend);
    }
    if (px[3] == 0)
        return CLR_INVALID;
    return RGB(px[2],px[1],px[0]);
}

LayerSnapshot *LayersCreateSnapshot(void) {
    if (lc <= 0 || lc > MAX_LAYERS)
        return NULL;
    LayerSnapshot *s = calloc(1, sizeof(LayerSnapshot));
    if (!s)
        return NULL;
    s->width = Canvas_GetWidth();
    s->height = Canvas_GetHeight();
    s->layerCount = lc;
    s->activeIndex = act;
    for (int i = 0; i < lc; i++) {
        s->layers[i].colorBmp =
            CreateLayerBmp(s->width, s->height, &s->layers[i].colorBits);
        if (!s->layers[i].colorBmp || !s->layers[i].colorBits) {
            s->layerCount = i;
            LayersDestroySnapshot(s);
            return NULL;
        }
        memcpy(s->layers[i].colorBits, lyr[i].bits,
               (size_t)s->width * (size_t)s->height * 4);
        s->layers[i].opacity = lyr[i].opacity;
        s->layers[i].blendMode = lyr[i].blend;
        s->layers[i].visible = lyr[i].visible;
        strncpy_s(s->layers[i].name, sizeof(s->layers[i].name), lyr[i].name,
                  _TRUNCATE);
    }
    return s;
}

BOOL LayersApplySnapshot(LayerSnapshot *s) {
    if (!s || s->layerCount <= 0 || s->layerCount > MAX_LAYERS)
        return FALSE;
    for (int i = 0; i < lc; i++)
        ClearLayer(&lyr[i]);
    if (cap < s->layerCount) {
        cap = s->layerCount;
        free(lyr);
        lyr = calloc(cap, sizeof(Layer));
        if (!lyr)
            return FALSE;
    }
    lc = s->layerCount;
    act = s->activeIndex;
    if (act >= lc)
        act = lc - 1;
    if (act < 0)
        act = 0;
    for (int i = 0; i < lc; i++) {
        lyr[i].bmp = CreateLayerBmp(s->width, s->height, &lyr[i].bits);
        if (!lyr[i].bmp || !lyr[i].bits) {
            for (int j = 0; j < i; j++)
                ClearLayer(&lyr[j]);
            lc = 0;
            return FALSE;
        }
        memcpy(lyr[i].bits, s->layers[i].colorBits,
               (size_t)s->width * (size_t)s->height * 4);
        lyr[i].opacity = s->layers[i].opacity;
        lyr[i].blend = s->layers[i].blendMode;
        lyr[i].visible = s->layers[i].visible;
        strncpy_s(lyr[i].name, sizeof(lyr[i].name), s->layers[i].name, _TRUNCATE);
    }
    Canvas_SetWidth(s->width);
    Canvas_SetHeight(s->height);
    if (draft) {
        DeleteObject(draft);
        draft = NULL;
        draftBits = NULL;
        draftW = draftH = 0;
    }
    draftPixels = FALSE;
    LayersMarkDirty();
    return TRUE;
}

void LayersDestroySnapshot(LayerSnapshot *s){
    if(!s) return;
    for(int i=0;i<s->layerCount;i++) if(s->layers[i].colorBmp) DeleteObject(s->layers[i].colorBmp);
    free(s);
}

BOOL LayersLoadFromBitmap(HBITMAP hBmp){
    if(!hBmp) {
        LogLastError("LayersLoadFromBitmap failed - hBmp is NULL");
        return FALSE;
    }
    BITMAP bm; 
    if (GetObject(hBmp,sizeof(bm),&bm) == 0) {
        LogLastError("LayersLoadFromBitmap failed - GetObject returned 0");
        return FALSE;
    }
    
    // Overflow check for pixel count
    if (bm.bmWidth > 0 && bm.bmHeight > 0) {
        if (bm.bmWidth > 32767 || bm.bmHeight > 32767) { // Heuristic sanity check
            OutputDebugStringA("LayersLoadFromBitmap: dimensions too large\n");
            return FALSE;
        }
    }

    LayersDestroy(); cap=4; lyr=calloc(cap,sizeof(Layer));
    if (!lyr) return FALSE;
    
    lyr[0].bmp=CreateLayerBmp(bm.bmWidth,bm.bmHeight,&lyr[0].bits);
    if (!lyr[0].bmp) {
        LogLastError("LayersLoadFromBitmap failed - CreateLayerBmp returned NULL");
        return FALSE;
    }

    HDC srcDC=CreateCompatibleDC(NULL), dstDC=CreateCompatibleDC(NULL);
    HBITMAP os=SelectObject(srcDC,hBmp), od=SelectObject(dstDC,lyr[0].bmp);
    BitBlt(dstDC,0,0,bm.bmWidth,bm.bmHeight,srcDC,0,0,SRCCOPY);
    SelectObject(srcDC,os); SelectObject(dstDC,od); DeleteDC(srcDC); DeleteDC(dstDC);
    
    size_t cnt = (size_t)bm.bmWidth * (size_t)bm.bmHeight;
    for(size_t i=0;i<cnt;i++)lyr[0].bits[i*4+3]=255;
    lc=1; act=0; Canvas_SetWidth(bm.bmWidth); Canvas_SetHeight(bm.bmHeight); MarkFullDirty();
    return TRUE;
}

BOOL LayersLoadFromPixels(int ww,int hh,const BYTE *bgra,int stride,BOOL useAlpha){
    if(!bgra||ww<=0||hh<=0) return FALSE;
    LayersDestroy(); cap=1; lyr=calloc(1,sizeof(Layer));
    if (!lyr) return FALSE;
    lyr[0].bmp=CreateLayerBmp(ww,hh,&lyr[0].bits);
    if (!lyr[0].bmp) { free(lyr); lyr=NULL; return FALSE; }
    for(int y=0;y<hh;y++){const BYTE *sr=bgra+y*stride; BYTE *ds=lyr[0].bits+y*ww*4;
        for(int x=0;x<ww;x++){ds[x*4]=sr[x*4]; ds[x*4+1]=sr[x*4+1]; ds[x*4+2]=sr[x*4+2]; ds[x*4+3]=useAlpha?sr[x*4+3]:255;}}
    lc=1; act=0; Canvas_SetWidth(ww); Canvas_SetHeight(hh); MarkFullDirty();
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
    MarkFullDirty(); return TRUE;
}

/* Draft Layer API */
void LayersEnsureDraft(void){
    int ww=Canvas_GetWidth(), hh=Canvas_GetHeight();
    if(draft && draftW==ww && draftH==hh) return;
    if(draft){DeleteObject(draft);draft=NULL;draftBits=NULL;}
    draft=CreateLayerBmp(ww,hh,&draftBits); draftW=ww; draftH=hh;
    if(draftBits) memset(draftBits,0,ww*hh*4);
}
void LayersBeginStroke(void) {
    LayersEnsureDraft();
    if (draftBits && draftW && draftH)
        memset(draftBits, 0, (size_t)draftW * (size_t)draftH * 4);
    draftPixels = FALSE;
}

BYTE *LayersGetStrokeBits(void) {
    LayersEnsureDraft();
    return draftBits;
}

BYTE *LayersGetDraftBits(void) {
    LayersEnsureDraft();
    return draftBits;
}

void LayersClearDraft(void) {
    if (draftBits && draftW && draftH) {
        memset(draftBits, 0, (size_t)draftW * (size_t)draftH * 4);
        LayersMarkDirty();
    }
    draftPixels = FALSE;
}

BOOL LayersIsDraftDirty(void) { return draftPixels; }

void LayersMergeDraftRect(const RECT *bmpBounds) {
    int ww, hh, x0, y0, x1, y1, x, y;

    if (!draftBits || draftW != Canvas_GetWidth() ||
        draftH != Canvas_GetHeight())
        return;
    BYTE *ab = LayersGetActiveColorBits();
    if (!ab)
        return;

    ww = draftW;
    hh = draftH;
    if (bmpBounds) {
        x0 = bmpBounds->left;
        y0 = bmpBounds->top;
        x1 = bmpBounds->right;
        y1 = bmpBounds->bottom;
        if (x0 < 0)
            x0 = 0;
        if (y0 < 0)
            y0 = 0;
        if (x1 > ww)
            x1 = ww;
        if (y1 > hh)
            y1 = hh;
    } else {
        x0 = 0;
        y0 = 0;
        x1 = ww;
        y1 = hh;
    }

    if (x0 < x1 && y0 < y1) {
        for (y = y0; y < y1; y++) {
            for (x = x0; x < x1; x++) {
                BYTE *d = draftBits + (y * ww + x) * 4;
                BYTE *a = ab + (y * ww + x) * 4;
                if (d[3] == 0)
                    continue;
                PixelOps_BlendPixel(d[2], d[1], d[0], d[3], a, 0);
            }
        }
        LayersMarkDirtyRect(x0, y0, x1, y1);
    } else {
        LayersMarkDirty();
    }

    memset(draftBits, 0, (size_t)draftW * (size_t)draftH * 4);
    draftPixels = FALSE;
}

void LayersMergeDraftToActive(void) {
    LayersMergeDraftRect(NULL);
}
