#ifndef LAYERS_H
#define LAYERS_H
#include <windows.h>
#include "palette.h"

#define MAX_LAYERS 32 /* matches LayerSnapshot.layers[] */

typedef enum { LAYER_BLEND_NORMAL=0, LAYER_BLEND_MULTIPLY, LAYER_BLEND_SCREEN, LAYER_BLEND_OVERLAY, LAYER_BLEND_COUNT } LayerBlendMode;

BOOL LayersInit(int w, int h);
void LayersDestroy(void);
BOOL LayersResize(int w, int h);

int LayersGetCount(void);
int LayersGetActiveIndex(void);
BOOL LayersSetActiveIndex(int i);

BOOL LayersAddLayer(const char *name);
BOOL LayersDeleteLayer(int i);
BOOL LayersMoveLayer(int f, int t);
BOOL LayersMergeDown(int i);

BOOL LayersGetVisible(int i);
void LayersSetVisible(int i, BOOL v);
BYTE LayersGetOpacity(int i);
void LayersSetOpacity(int i, BYTE o);
int LayersGetBlendMode(int i);
void LayersSetBlendMode(int i, int b);
void LayersGetName(int i, char *out, int sz);

void GetLayerDisplayName(int i, char *out, int sz);

HBITMAP LayersGetActiveColorBitmap(void);
BYTE *LayersGetActiveColorBits(void);
BYTE *Layers_BeginWrite(void);

HBITMAP LayersGetCompositeBitmap(BOOL check);
HBITMAP LayersFlattenToBitmap(COLORREF bg);
HBITMAP LayersFlattenToBitmapWithAlpha(BYTE **out);
COLORREF LayersSampleCompositeColor(int x, int y, COLORREF bg);
void LayersMarkDirty(void);

BYTE *LayersGetDraftBits(void);
void LayersClearDraft(void);
void LayersMergeDraftToActive(void);
BOOL LayersIsDraftDirty(void);
void LayersEnsureDraft(void);

typedef struct LayerSnapshot {
    int width, height, layerCount, activeIndex;
    struct { HBITMAP colorBmp; BYTE *colorBits; BYTE opacity; int blendMode; BOOL visible; char name[32]; } layers[32];
} LayerSnapshot;

LayerSnapshot *LayersCreateSnapshot(void);
BOOL LayersApplySnapshot(LayerSnapshot *s);
void LayersDestroySnapshot(LayerSnapshot *s);

BOOL LayersLoadFromBitmap(HBITMAP hBmp);
BOOL LayersLoadFromPixels(int w, int h, const BYTE *bgra, int stride, BOOL useAlpha);

typedef void (*RawBitmapTransformFunc)(BYTE *src, int sw, int sh, BYTE *dst, int dw, int dh, void *pUserData);

BOOL LayersApplyRawTransformToAll(int nw, int nh, RawBitmapTransformFunc fn, void *pUserData);

#endif
