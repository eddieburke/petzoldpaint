#ifndef LAYERS_H
#define LAYERS_H
#include "gdi_utils.h"
#include "helpers.h"
#include <windows.h>
#include "palette.h"

typedef enum {
  LAYER_BLEND_NORMAL = 0,
  LAYER_BLEND_MULTIPLY,
  LAYER_BLEND_SCREEN,
  LAYER_BLEND_OVERLAY,
  LAYER_BLEND_COUNT
} LayerBlendMode;

BOOL LayersInit(int width, int height);
void LayersDestroy(void);
BOOL LayersResize(int newWidth, int newHeight);

int LayersGetCount(void);
int LayersGetActiveIndex(void);
BOOL LayersSetActiveIndex(int index);

BOOL LayersAddLayer(const char *name);
BOOL LayersDeleteLayer(int index);
BOOL LayersMoveLayer(int fromIndex, int toIndex);
BOOL LayersMergeDown(int index);

BOOL LayersGetVisible(int index);
void LayersSetVisible(int index, BOOL visible);

BYTE LayersGetOpacity(int index);
void LayersSetOpacity(int index, BYTE opacity);

int LayersGetBlendMode(int index);
void LayersSetBlendMode(int index, int blendMode);

void LayersGetName(int index, char *outName, int outSize);
void LayersSetName(int index, const char *name);

void GetLayerDisplayName(int layerIndex, char *out, int outSize);
void HistoryPushLayerOpacity(int layerIndex, int oldPercent, int newPercent);
void HistoryPushLayerVisibility(int layerIndex, BOOL oldVisible,
                                BOOL newVisible);
void HistoryPushLayerBlendMode(int layerIndex, int oldMode, int newMode);
void HistoryPushLayerMove(int fromIndex, int toIndex);

HBITMAP LayersGetActiveColorBitmap(void);
BYTE *LayersGetActiveColorBits(void);
HDC LayersGetActiveColorDC(HBITMAP *phOld);

HBITMAP LayersGetCompositeBitmap(BOOL bCheckerboard);
HBITMAP LayersFlattenToBitmap(COLORREF bgColor);
HBITMAP LayersFlattenToBitmapWithAlpha(BYTE **outBits);
COLORREF LayersSampleCompositeColor(int x, int y, COLORREF bgColor);
void LayersMarkDirty(void);
void LayersMarkDirtyRect(const RECT *pRect);

/* Draft Layer API — tools draw previews here; compositor renders above active
 */
BYTE *LayersGetDraftBits(void);
void LayersClearDraft(void);
void LayersMergeDraftToActive(void);
BOOL LayersIsDraftDirty(void);
void LayersEnsureDraft(void);

typedef struct LayerSnapshot LayerSnapshot;
LayerSnapshot *LayersCreateSnapshot(void);
void LayersApplySnapshot(LayerSnapshot *snapshot);
void LayersDestroySnapshot(LayerSnapshot *snapshot);

BOOL LayersLoadFromBitmap(HBITMAP hBmp);
BOOL LayersLoadFromPixels(int width, int height, const BYTE *bgra, int stride,
                          BOOL useAlpha);
typedef void (*RawBitmapTransformFunc)(BYTE *pSrc, int srcW, int srcH,
                                       BYTE *pDst, int dstW, int dstH,
                                       void *pUserData);
BOOL LayersApplyTransformToAll(int newWidth, int newHeight,
                               BitmapTransformFunc pfnTransform,
                               void *pUserData);
BOOL LayersApplyRawTransformToAll(int newWidth, int newHeight,
                                  RawBitmapTransformFunc pfnTransform,
                                  void *pUserData);

#endif
