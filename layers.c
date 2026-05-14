/*------------------------------------------------------------
   LAYERS.C -- Layer and Compositing

   Manages the layer stack, blending, composite cache, and
   layer snapshot for history.
------------------------------------------------------------*/

#include "peztold_core.h"
#include "draw.h"
#include "pixel_ops.h"
#include "layers.h"
#include "canvas.h"
#include <stdlib.h>


#define MAX_LAYERS 32
#define DEFAULT_LAYER_NAME "Layer"

typedef struct {
  HBITMAP colorBmp;
  BYTE *colorBits;
  BYTE opacity;
  int blendMode;
  BOOL visible;
  char name[32];
} Layer;

struct LayerSnapshot {
  int width;
  int height;
  int activeIndex;
  int layerCount;
  Layer layers[MAX_LAYERS];
};

static Layer *s_layers = NULL;
static int s_layerCount = 0;
static int s_activeIndex = 0;
static int s_capacity = 0;

static HBITMAP s_compositeBmp = NULL;
static BYTE *s_compositeBits = NULL;
static int s_compositeStride = 0;
static BOOL s_compositeDirty = TRUE;
static BOOL s_partialDirty = FALSE;
static RECT s_dirtyRect = {0, 0, 0, 0};
static BOOL s_lastCheckerboard = FALSE;
static COLORREF s_lastBgColor = RGB(0, 0, 0);

/*------------------------------------------------------------
   Draft Layer - temporary compositing surface for tool previews.
   Tools draw here; the compositor blends it above the active layer.
  ------------------------------------------------------------*/
static HBITMAP s_draftBmp = NULL;
static BYTE *s_draftBits = NULL;
static int s_draftW = 0;
static int s_draftH = 0;

void LayersMarkDirty(void) {
  s_compositeDirty = TRUE;
  s_partialDirty = FALSE;
}

void LayersMarkDirtyRect(const RECT *pRect) {
  if (s_compositeDirty)
    return;
  // Validate rect
  if (pRect->left >= pRect->right || pRect->top >= pRect->bottom)
    return;

  RECT rc = *pRect;
  // Clamp to canvas
  if (rc.left < 0)
    rc.left = 0;
  if (rc.top < 0)
    rc.top = 0;
  if (rc.right > Canvas_GetWidth())
    rc.right = Canvas_GetWidth();
  if (rc.bottom > Canvas_GetHeight())
    rc.bottom = Canvas_GetHeight();

  if (rc.left >= rc.right || rc.top >= rc.bottom)
    return;

  if (s_partialDirty) {
    // Create union of dirty regions
    int newL = min(s_dirtyRect.left, rc.left);
    int newT = min(s_dirtyRect.top, rc.top);
    int newR = max(s_dirtyRect.right, rc.right);
    int newB = max(s_dirtyRect.bottom, rc.bottom);
    s_dirtyRect.left = newL;
    s_dirtyRect.top = newT;
    s_dirtyRect.right = newR;
    s_dirtyRect.bottom = newB;
  } else {
    s_dirtyRect = rc;
    s_partialDirty = TRUE;
  }
}



static void FillDibSolid(BYTE *bits, int width, int height, COLORREF color) {
  PixelOps_Fill(bits, width, height, color, 255);
}

static void EnsureCompositeBuffer(int width, int height) {
  if (s_compositeBmp && s_compositeBits && s_compositeStride == width * 4) {
    BITMAP bm;
    if (GetObject(s_compositeBmp, sizeof(bm), &bm) == sizeof(bm)) {
      if (bm.bmWidth == width && bm.bmHeight == height) {
        return;
      }
    }
  }

  if (s_compositeBmp) {
    DeleteObject(s_compositeBmp);
    s_compositeBmp = NULL;
    s_compositeBits = NULL;
  }

  s_compositeBmp = CreateDibSection32(width, height, &s_compositeBits);
  s_compositeStride = width * 4;
}

static void CompositeLayersToBufferUnified(BYTE *dstBits, int width, int height,
                                           const RECT *pRect, BOOL checkerboard,
                                           COLORREF bgColor, BOOL outputAlpha) {
  if (!dstBits || width <= 0 || height <= 0)
    return;

  int startX = 0, startY = 0;
  int endX = width, endY = height;

  if (pRect) {
    startX = max(0, pRect->left);
    startY = max(0, pRect->top);
    endX = min(width, pRect->right);
    endY = min(height, pRect->bottom);
  }

  if (startX >= endX || startY >= endY)
    return;

  if (outputAlpha) {
    for (int y = startY; y < endY; y++) {
      ZeroMemory(dstBits + (y * width + startX) * 4, (endX - startX) * 4);
    }
  } else if (checkerboard) {
    if (!pRect ||
        (startX == 0 && startY == 0 && endX == width && endY == height)) {
      PixelOps_FillCheckerboard(dstBits, width, height);
    } else {
      PixelOps_FillCheckerboardRect(dstBits, width, height, startX, startY,
                                    endX, endY);
    }
  } else {
    PixelOps_Fill(dstBits, width, height, bgColor, 255);
  }

  for (int i = 0; i < s_layerCount; i++) {
    Layer *layer = &s_layers[i];
    if (!layer->visible || layer->opacity == 0 || !layer->colorBits) {
      continue;
    }

    for (int y = startY; y < endY; y++) {
      BYTE *dstRow = dstBits + y * width * 4;
      BYTE *srcRow = layer->colorBits + y * width * 4;
      for (int x = startX; x < endX; x++) {
        BYTE *dstPx = dstRow + x * 4;
        BYTE *srcPx = srcRow + x * 4;
        BYTE pixelAlpha = srcPx[3];
        if (pixelAlpha == 0)
          continue;

        int srcA = (int)pixelAlpha * (int)layer->opacity / 255;
        if (srcA <= 0)
          continue;
 
        PixelOps_BlendPixel(srcPx[2], srcPx[1], srcPx[0], srcA, dstPx, layer->blendMode);
      }
    }
 
    /* --- Draft Layer: rendered immediately after the active layer --- */
    if (i == s_activeIndex && s_draftBits && s_draftW == width &&
        s_draftH == height) {
      for (int y = startY; y < endY; y++) {
        BYTE *dstRow2 = dstBits + y * width * 4;
        BYTE *dftRow = s_draftBits + y * width * 4;
        for (int x = startX; x < endX; x++) {
          BYTE *dstPx2 = dstRow2 + x * 4;
          BYTE *dftPx = dftRow + x * 4;
          if (dftPx[3] == 0)
            continue;
          PixelOps_BlendPixel(dftPx[2], dftPx[1], dftPx[0], dftPx[3], dstPx2,
                              LAYER_BLEND_NORMAL);
        }
      }
    }
  }
}

static void CompositeLayersToBuffer(BYTE *dstBits, int width, int height,
                                    BOOL checkerboard, COLORREF bgColor) {
  CompositeLayersToBufferUnified(dstBits, width, height, NULL, checkerboard,
                                 bgColor, FALSE);
}

static void CompositeLayersToBufferWithAlpha(BYTE *dstBits, int width,
                                             int height) {
  CompositeLayersToBufferUnified(dstBits, width, height, NULL, FALSE, Palette_GetSecondaryColor(),
                                 TRUE);
}

static BOOL EnsureLayerCapacity(int desired) {
  if (s_capacity >= desired)
    return TRUE;
  int newCap = s_capacity == 0 ? 4 : s_capacity * 2;
  if (newCap < desired)
    newCap = desired;
  if (newCap > MAX_LAYERS)
    newCap = MAX_LAYERS;
  if (newCap < desired)
    return FALSE;

  Layer *newLayers = (Layer *)calloc(newCap, sizeof(Layer));
  if (!newLayers)
    return FALSE;
  if (s_layers && s_layerCount > 0) {
    memcpy(newLayers, s_layers, sizeof(Layer) * s_layerCount);
  }
  free(s_layers);
  s_layers = newLayers;
  s_capacity = newCap;
  return TRUE;
}

static void ClearLayerBitmaps(Layer *layer) {
  if (!layer)
    return;
  if (layer->colorBmp)
    DeleteObject(layer->colorBmp);
  layer->colorBmp = NULL;
  layer->colorBits = NULL;
}

static BOOL InitLayer(Layer *layer, int width, int height, const char *name,
                      BOOL opaque) {
  if (!layer)
    return FALSE;
  layer->colorBmp = CreateDibSection32(width, height, &layer->colorBits);
  if (!layer->colorBmp || !layer->colorBits) {
    ClearLayerBitmaps(layer);
    return FALSE;
  }

  // Fill with white RGB; alpha is 255 if opaque, 0 if transparent
  PixelOps_Fill(layer->colorBits, width, height, RGB(255, 255, 255),
                opaque ? 255 : 0);

  layer->opacity = 255;
  layer->blendMode = LAYER_BLEND_NORMAL;
  layer->visible = TRUE;
    memset(layer->name, 0, sizeof(layer->name));
    if (name && name[0] != '\0') {
        StringCchCopy(layer->name, sizeof(layer->name), name);
    } else {
        StringCchCopy(layer->name, sizeof(layer->name), DEFAULT_LAYER_NAME);
    }

  return TRUE;
}

BOOL LayersInit(int width, int height) {
  LayersDestroy();
  if (!EnsureLayerCapacity(1))
    return FALSE;

  s_layerCount = 1;
  s_activeIndex = 0;

  if (!InitLayer(&s_layers[0], width, height, "Layer 1", FALSE)) {
    LayersDestroy();
    return FALSE;
  }
  
  s_compositeDirty = TRUE;
  return TRUE;
}

void LayersDestroy(void) {
  if (s_layers) {
    for (int i = 0; i < s_layerCount; i++) {
      ClearLayerBitmaps(&s_layers[i]);
    }
    free(s_layers);
    s_layers = NULL;
  }
  s_layerCount = 0;
  s_activeIndex = 0;
  s_capacity = 0;

  if (s_compositeBmp) {
    DeleteObject(s_compositeBmp);
    s_compositeBmp = NULL;
    s_compositeBits = NULL;
    s_compositeStride = 0;
  }

  /* Destroy draft layer */
  if (s_draftBmp) {
    DeleteObject(s_draftBmp);
    s_draftBmp = NULL;
    s_draftBits = NULL;
  }
  s_draftW = 0;
  s_draftH = 0;

  s_compositeDirty = TRUE;

}

BOOL LayersResize(int newWidth, int newHeight) {
  if (newWidth <= 0 || newHeight <= 0)
    return FALSE;
  if (s_layerCount <= 0)
    return FALSE;

  BYTE **newBits = (BYTE **)calloc((size_t)s_layerCount, sizeof(BYTE *));
  HBITMAP *newBmps = (HBITMAP *)calloc((size_t)s_layerCount, sizeof(HBITMAP));
  if (!newBits || !newBmps) {
    free(newBits);
    free(newBmps);
    return FALSE;
  }

  int srcW = Canvas_GetWidth();
  int srcH = Canvas_GetHeight();
  int copyW = (srcW < newWidth) ? srcW : newWidth;
  int copyH = (srcH < newHeight) ? srcH : newHeight;

  for (int i = 0; i < s_layerCount; i++) {
    Layer *layer = &s_layers[i];
    BYTE *newColorBits = NULL;
    HBITMAP hNewColor = CreateDibSection32(newWidth, newHeight, &newColorBits);
    if (!hNewColor || !newColorBits) {
      if (hNewColor)
        DeleteObject(hNewColor);
      for (int j = 0; j < i; j++) {
        if (newBmps[j])
          DeleteObject(newBmps[j]);
      }
      free(newBits);
      free(newBmps);
      return FALSE;
    }

    // Fill with transparent white (alpha=0)
    PixelOps_Fill(newColorBits, newWidth, newHeight, RGB(255, 255, 255), 0);

    // Copy pixels with resizing (crop/pad)
    // Assume top-left alignment (0,0 to 0,0)
    DWORD *pSrcBits = (DWORD *)layer->colorBits;
    DWORD *pDstBits = (DWORD *)newColorBits;

    // Copy row by row
    for (int y = 0; y < copyH; y++) {
      // Check bounds just in case
      if (y >= srcH || y >= newHeight)
        continue;

      // Source and destination pointers for this row
      DWORD *rowSrc = pSrcBits + y * srcW;
      DWORD *rowDst = pDstBits + y * newWidth;

      // Memcpy the visible portion of the row
      memcpy(rowDst, rowSrc, copyW * 4);
    }

    newBmps[i] = hNewColor;
    newBits[i] = newColorBits;
  }

  for (int i = 0; i < s_layerCount; i++) {
    Layer *layer = &s_layers[i];
    ClearLayerBitmaps(layer);
    layer->colorBmp = newBmps[i];
    layer->colorBits = newBits[i];
  }
  free(newBits);
  free(newBmps);

  /* Invalidate draft layer on resize — tools must re-draw if active */
  if (s_draftBmp) {
    DeleteObject(s_draftBmp);
    s_draftBmp = NULL;
    s_draftBits = NULL;
  }
  s_draftW = 0;
  s_draftH = 0;

  s_compositeDirty = TRUE;
  return TRUE;
}

int LayersGetCount(void) { return s_layerCount; }

int LayersGetActiveIndex(void) { return s_activeIndex; }

BOOL LayersSetActiveIndex(int index) {
  if (index < 0 || index >= s_layerCount)
    return FALSE;
  s_activeIndex = index;
  
  return TRUE;
}

BOOL LayersAddLayer(const char *name) {
  if (s_layerCount >= MAX_LAYERS)
    return FALSE;
  if (!EnsureLayerCapacity(s_layerCount + 1))
    return FALSE;

  if (!InitLayer(&s_layers[s_layerCount], Canvas_GetWidth(), Canvas_GetHeight(), name,
                 FALSE)) {
    return FALSE;
  }
  s_layerCount++;
  s_activeIndex = s_layerCount - 1;
  
  s_compositeDirty = TRUE;
  return TRUE;
}

BOOL LayersDeleteLayer(int index) {
  if (s_layerCount <= 1)
    return FALSE;
  if (index < 0 || index >= s_layerCount)
    return FALSE;

  ClearLayerBitmaps(&s_layers[index]);
  for (int i = index; i < s_layerCount - 1; i++) {
    s_layers[i] = s_layers[i + 1];
  }
  s_layerCount--;
  if (s_activeIndex >= s_layerCount)
    s_activeIndex = s_layerCount - 1;
  
  s_compositeDirty = TRUE;
  return TRUE;
}

BOOL LayersMoveLayer(int fromIndex, int toIndex) {
  if (fromIndex < 0 || fromIndex >= s_layerCount)
    return FALSE;
  if (toIndex < 0 || toIndex >= s_layerCount)
    return FALSE;
  if (fromIndex == toIndex)
    return TRUE;

  Layer temp = s_layers[fromIndex];
  if (fromIndex < toIndex) {
    for (int i = fromIndex; i < toIndex; i++) {
      s_layers[i] = s_layers[i + 1];
    }
  } else {
    for (int i = fromIndex; i > toIndex; i--) {
      s_layers[i] = s_layers[i - 1];
    }
  }
  s_layers[toIndex] = temp;

  if (s_activeIndex == fromIndex) {
    s_activeIndex = toIndex;
  } else if (fromIndex < s_activeIndex && toIndex >= s_activeIndex) {
    s_activeIndex--;
  } else if (fromIndex > s_activeIndex && toIndex <= s_activeIndex) {
    s_activeIndex++;
  }
  
  s_compositeDirty = TRUE;
  return TRUE;
}

BOOL LayersMergeDown(int index) {
  if (index <= 0 || index >= s_layerCount)
    return FALSE;

  Layer *src = &s_layers[index];
  Layer *dst = &s_layers[index - 1];

  if (src->visible && src->opacity > 0 && src->colorBits && dst->colorBits) {
    for (int y = 0; y < Canvas_GetHeight(); y++) {
      BYTE *dstRow = dst->colorBits + y * Canvas_GetWidth() * 4;
      BYTE *srcRow = src->colorBits + y * Canvas_GetWidth() * 4;
      for (int x = 0; x < Canvas_GetWidth(); x++) {
        BYTE *dstPx = dstRow + x * 4;
        BYTE *srcPx = srcRow + x * 4;
        BYTE pixelAlpha = srcPx[3];
        if (pixelAlpha == 0)
          continue;

        int alpha = (int)srcPx[3] * (int)src->opacity / 255;
        if (alpha <= 0)
          continue;

        PixelOps_BlendPixel(srcPx[2], srcPx[1], srcPx[0], alpha, dstPx, src->blendMode);
      }
    }
  }

  ClearLayerBitmaps(src);
  for (int i = index; i < s_layerCount - 1; i++) {
    s_layers[i] = s_layers[i + 1];
  }
  s_layerCount--;
  if (s_activeIndex == index) {
    s_activeIndex = index - 1;
  } else if (s_activeIndex > index) {
    s_activeIndex--;
  }
  
  s_compositeDirty = TRUE;
  return TRUE;
}

BOOL LayersGetVisible(int index) {
  if (index < 0 || index >= s_layerCount)
    return FALSE;
  return s_layers[index].visible;
}

void LayersSetVisible(int index, BOOL visible) {
  if (index < 0 || index >= s_layerCount)
    return;
  s_layers[index].visible = visible;
  s_compositeDirty = TRUE;
}

BYTE LayersGetOpacity(int index) {
  if (index < 0 || index >= s_layerCount)
    return 255;
  return s_layers[index].opacity;
}

void LayersSetOpacity(int index, BYTE opacity) {
  if (index < 0 || index >= s_layerCount)
    return;
  s_layers[index].opacity = opacity;
  s_compositeDirty = TRUE;
}

int LayersGetBlendMode(int index) {
  if (index < 0 || index >= s_layerCount)
    return LAYER_BLEND_NORMAL;
  return s_layers[index].blendMode;
}

void LayersSetBlendMode(int index, int blendMode) {
  if (index < 0 || index >= s_layerCount)
    return;
  if (blendMode < 0 || blendMode >= LAYER_BLEND_COUNT)
    return;
  s_layers[index].blendMode = blendMode;
  s_compositeDirty = TRUE;
}

void LayersGetName(int index, char *outName, int outSize) {
  if (!outName || outSize <= 0)
    return;
  if (index < 0 || index >= s_layerCount) {
    outName[0] = '\0';
    return;
  }
  StringCchCopy(outName, outSize, s_layers[index].name);
}

void LayersSetName(int index, const char *name) {
  if (index < 0 || index >= s_layerCount)
    return;
  if (!name)
    name = DEFAULT_LAYER_NAME;
  memset(s_layers[index].name, 0, sizeof(s_layers[index].name));
  StringCchCopy(s_layers[index].name, sizeof(s_layers[index].name), name);
}

/*------------------------------------------------------------
    Layer history helpers (GetLayerDisplayName, HistoryPushLayer*)
------------------------------------------------------------*/

void GetLayerDisplayName(int layerIndex, char *out, int outSize) {
  if (!out || outSize <= 0)
    return;
  LayersGetName(layerIndex, out, outSize);
  if (out[0] == '\0') {
    StringCchPrintf(out, outSize, "Layer %d", layerIndex + 1);
  }
}

void HistoryPushLayerOpacity(int layerIndex, int oldPercent, int newPercent) {
  char layerName[64];
  GetLayerDisplayName(layerIndex, layerName, sizeof(layerName));
  HistoryPushFormatted("Opacity: %d%% → %d%% (%s)", oldPercent, newPercent,
                       layerName);
}

void HistoryPushLayerVisibility(int layerIndex, BOOL oldVisible,
                                BOOL newVisible) {
  char layerName[64];
  GetLayerDisplayName(layerIndex, layerName, sizeof(layerName));
  HistoryPushFormatted("Visibility: %s → %s (%s)",
                       oldVisible ? "Visible" : "Hidden",
                       newVisible ? "Visible" : "Hidden", layerName);
}


void HistoryPushLayerMove(int fromIndex, int toIndex) {
  char fromName[64];
  GetLayerDisplayName(fromIndex, fromName, sizeof(fromName));
  HistoryPushFormatted(toIndex < fromIndex ? "Move Layer Up: %s"
                                           : "Move Layer Down: %s",
                       fromName);
}

HBITMAP LayersGetActiveColorBitmap(void) {
  if (s_layerCount <= 0 || s_activeIndex < 0 || s_activeIndex >= s_layerCount)
    return NULL;
  return s_layers[s_activeIndex].colorBmp;
}

BYTE *LayersGetActiveColorBits(void) {
  if (s_layerCount <= 0 || s_activeIndex < 0 || s_activeIndex >= s_layerCount)
    return NULL;
  return s_layers[s_activeIndex].colorBits;
}

/* LayersGetActiveColorDC returns an HDC with the active layer's color
   bitmap selected. Layer bitmaps are 32-bit DIB sections
   (CreateDibSection32); GetDibBitsFromHdc and
   GetCurrentObject(OBJ_BITMAP)/GetObject with bm.bmBits work for tool
   drawing operations. */
HDC LayersGetActiveColorDC(HBITMAP *phOld) {
  HBITMAP hBmp = LayersGetActiveColorBitmap();
  if (!hBmp)
    return NULL;
  return GetBitmapDC(hBmp, phOld);
}

HBITMAP LayersGetCompositeBitmap(BOOL bCheckerboard) {
  if (s_layerCount <= 0)
    return NULL;
  EnsureCompositeBuffer(Canvas_GetWidth(), Canvas_GetHeight());
  if (s_compositeDirty || s_partialDirty ||
      s_lastCheckerboard != bCheckerboard ||
      (!bCheckerboard && s_lastBgColor != Palette_GetSecondaryColor())) {

    // If parameters changed, force full rebuild
    if (s_lastCheckerboard != bCheckerboard ||
        (!bCheckerboard && s_lastBgColor != Palette_GetSecondaryColor())) {
      s_compositeDirty = TRUE;
    }

    if (s_compositeDirty) {
      CompositeLayersToBufferUnified(s_compositeBits, Canvas_GetWidth(), Canvas_GetHeight(),
                                     NULL, bCheckerboard, Palette_GetSecondaryColor(), FALSE);
    } else if (s_partialDirty) {
      CompositeLayersToBufferUnified(s_compositeBits, Canvas_GetWidth(), Canvas_GetHeight(),
                                     &s_dirtyRect, bCheckerboard, Palette_GetSecondaryColor(),
                                     FALSE);
    }

    s_compositeDirty = FALSE;
    s_partialDirty = FALSE;
    s_lastCheckerboard = bCheckerboard;
    s_lastBgColor = Palette_GetSecondaryColor();
  }
  return s_compositeBmp;
}

HBITMAP LayersFlattenToBitmap(COLORREF bgColor) {
  if (s_layerCount <= 0)
    return NULL;
  BYTE *bits = NULL;
  HBITMAP hBmp = CreateDibSection32(Canvas_GetWidth(), Canvas_GetHeight(), &bits);
  if (!hBmp || !bits) {
    if (hBmp)
      DeleteObject(hBmp);
    return NULL;
  }
  CompositeLayersToBuffer(bits, Canvas_GetWidth(), Canvas_GetHeight(), FALSE, bgColor);
  return hBmp;
}

HBITMAP LayersFlattenToBitmapWithAlpha(BYTE **outBits) {
  if (s_layerCount <= 0)
    return NULL;
  BYTE *bits = NULL;
  HBITMAP hBmp = CreateDibSection32(Canvas_GetWidth(), Canvas_GetHeight(), &bits);
  if (!hBmp || !bits) {
    if (hBmp)
      DeleteObject(hBmp);
    return NULL;
  }
  CompositeLayersToBufferWithAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight());
  if (outBits)
    *outBits = bits;
  return hBmp;
}

COLORREF LayersSampleCompositeColor(int x, int y, COLORREF bgColor) {
  if (x < 0 || y < 0 || x >= Canvas_GetWidth() || y >= Canvas_GetHeight())
    return CLR_INVALID;
  EnsureCompositeBuffer(Canvas_GetWidth(), Canvas_GetHeight());
  if (s_compositeDirty || s_lastCheckerboard || s_lastBgColor != bgColor) {
    CompositeLayersToBuffer(s_compositeBits, Canvas_GetWidth(), Canvas_GetHeight(), FALSE,
                            bgColor);
    s_compositeDirty = FALSE;
    s_lastCheckerboard = FALSE;
    s_lastBgColor = bgColor;
  }
  BYTE *px = s_compositeBits + (y * Canvas_GetWidth() + x) * 4;
  return RGB(px[2], px[1], px[0]);
}

LayerSnapshot *LayersCreateSnapshot(void) {
  if (s_layerCount <= 0)
    return NULL;
  LayerSnapshot *snapshot = (LayerSnapshot *)calloc(1, sizeof(LayerSnapshot));
  if (!snapshot)
    return NULL;

  snapshot->width = Canvas_GetWidth();
  snapshot->height = Canvas_GetHeight();
  snapshot->activeIndex = s_activeIndex;
  snapshot->layerCount = s_layerCount;

  for (int i = 0; i < s_layerCount; i++) {
    Layer *dst = &snapshot->layers[i];
    Layer *src = &s_layers[i];
    *dst = *src;
    dst->colorBmp =
        CreateDibSection32(Canvas_GetWidth(), Canvas_GetHeight(), &dst->colorBits);
    if (!dst->colorBmp || !dst->colorBits) {
      LayersDestroySnapshot(snapshot);
      return NULL;
    }
    memcpy(dst->colorBits, src->colorBits, Canvas_GetWidth() * Canvas_GetHeight() * 4);
  }

  return snapshot;
}

BOOL LayersApplySnapshot(LayerSnapshot *snapshot) {
  if (!snapshot)
    return FALSE;

  if (snapshot->layerCount < 0 || snapshot->layerCount > MAX_LAYERS)
    return FALSE;

  Layer tempLayers[MAX_LAYERS] = {0};
  for (int i = 0; i < snapshot->layerCount; i++) {
    Layer *dst = &tempLayers[i];
    Layer *src = &snapshot->layers[i];
    *dst = *src;
    dst->colorBmp = CreateDibSection32(snapshot->width, snapshot->height, &dst->colorBits);
    if (!dst->colorBmp || !dst->colorBits) {
      for (int j = 0; j <= i; j++) {
        ClearLayerBitmaps(&tempLayers[j]);
      }
      return FALSE;
    }
    if (src->colorBits) {
      memcpy(dst->colorBits, src->colorBits, snapshot->width * snapshot->height * 4);
    }
  }

  Layer oldLayers[MAX_LAYERS] = {0};
  int oldLayerCount = s_layerCount;
  for (int i = 0; i < oldLayerCount; i++) {
    oldLayers[i] = s_layers[i];
    s_layers[i].colorBmp = NULL;
    s_layers[i].colorBits = NULL;
  }

  s_layerCount = snapshot->layerCount;
  s_activeIndex = snapshot->activeIndex;
  Canvas_SetWidth(snapshot->width);
  Canvas_SetHeight(snapshot->height);

  for (int i = 0; i < s_layerCount; i++) {
    s_layers[i] = tempLayers[i];
    tempLayers[i].colorBmp = NULL;
    tempLayers[i].colorBits = NULL;
  }

  for (int i = 0; i < oldLayerCount; i++) {
    ClearLayerBitmaps(&oldLayers[i]);
  }


  s_compositeDirty = TRUE;
  s_partialDirty = FALSE;
  return TRUE;
}

void LayersDestroySnapshot(LayerSnapshot *snapshot) {
  if (!snapshot)
    return;
  for (int i = 0; i < snapshot->layerCount; i++) {
    if (snapshot->layers[i].colorBmp)
      DeleteObject(snapshot->layers[i].colorBmp);
    snapshot->layers[i].colorBmp = NULL;
  }
  free(snapshot);
}

BOOL LayersLoadFromBitmap(HBITMAP hBmp) {
  if (!hBmp)
    return FALSE;
  BITMAP bm;
  if (GetObject(hBmp, sizeof(bm), &bm) != sizeof(bm))
    return FALSE;

  LayersDestroy();
  if (!EnsureLayerCapacity(1))
    return FALSE;

  s_layerCount = 1;
  s_activeIndex = 0;

  Layer *layer = &s_layers[0];
  if (!InitLayer(layer, bm.bmWidth, bm.bmHeight, "Background", TRUE)) {
    LayersDestroy();
    return FALSE;
  }

  HBITMAP hOldSrc = NULL, hOldDst = NULL;
  HDC hSrcDC = GetBitmapDC(hBmp, &hOldSrc);
  HDC hDstDC = GetBitmapDC(layer->colorBmp, &hOldDst);
  if (hSrcDC && hDstDC) {
    BitBlt(hDstDC, 0, 0, bm.bmWidth, bm.bmHeight, hSrcDC, 0, 0, SRCCOPY);
  }
  if (hDstDC)
    ReleaseBitmapDC(hDstDC, hOldDst);
  if (hSrcDC)
    ReleaseBitmapDC(hSrcDC, hOldSrc);

  // Set all alpha to 255 (fully opaque) since loaded bitmaps have no alpha
  int count = bm.bmWidth * bm.bmHeight;
  BYTE *bits = layer->colorBits;
  for (int i = 0; i < count; i++) {
    bits[i * 4 + 3] = 255;
  }

  Canvas_SetWidth(bm.bmWidth);
  Canvas_SetHeight(bm.bmHeight);
  
  s_compositeDirty = TRUE;
  return TRUE;
}

BOOL LayersLoadFromPixels(int width, int height, const BYTE *bgra, int stride,
                          BOOL useAlpha) {
  if (!bgra || width <= 0 || height <= 0)
    return FALSE;

  LayersDestroy();
  if (!EnsureLayerCapacity(1))
    return FALSE;

  s_layerCount = 1;
  s_activeIndex = 0;

  Layer *layer = &s_layers[0];
  if (!InitLayer(layer, width, height, "Background", FALSE)) {
    LayersDestroy();
    return FALSE;
  }

  // Copy pixels with alpha into colorBits directly
  for (int y = 0; y < height; y++) {
    const BYTE *srcRow = bgra + y * stride;
    BYTE *dstColor = layer->colorBits + y * width * 4;
    for (int x = 0; x < width; x++) {
      const BYTE *srcPx = srcRow + x * 4;
      dstColor[x * 4 + 0] = srcPx[0];                  // B
      dstColor[x * 4 + 1] = srcPx[1];                  // G
      dstColor[x * 4 + 2] = srcPx[2];                  // R
      dstColor[x * 4 + 3] = useAlpha ? srcPx[3] : 255; // A
    }
  }

  Canvas_SetWidth(width);
  Canvas_SetHeight(height);
  
  s_compositeDirty = TRUE;
  return TRUE;
}

/*------------------------------------------------------------
   Draft Layer API
   Tools draw to the draft layer during interactive preview.
   The compositor renders it above the active layer automatically.
  ------------------------------------------------------------*/

void LayersEnsureDraft(void) {
  if (s_draftBits && s_draftW == Canvas_GetWidth() && s_draftH == Canvas_GetHeight())
    return;

  /* Reallocate if canvas size changed */
  if (s_draftBmp) {
    DeleteObject(s_draftBmp);
    s_draftBmp = NULL;
    s_draftBits = NULL;
  }

  s_draftBmp = CreateDibSection32(Canvas_GetWidth(), Canvas_GetHeight(), &s_draftBits);
  if (!s_draftBmp || !s_draftBits) {
    s_draftBmp = NULL;
    s_draftBits = NULL;
    s_draftW = 0;
    s_draftH = 0;
    return;
  }
  s_draftW = Canvas_GetWidth();
  s_draftH = Canvas_GetHeight();

  /* Start clear */
  ZeroMemory(s_draftBits, Canvas_GetWidth() * Canvas_GetHeight() * 4);
}

BYTE *LayersGetDraftBits(void) {
  LayersEnsureDraft();
  return s_draftBits;
}

void LayersClearDraft(void) {
  if (s_draftBits && s_draftW > 0 && s_draftH > 0) {
    ZeroMemory(s_draftBits, s_draftW * s_draftH * 4);
  }
  s_compositeDirty = TRUE;
}

void LayersMergeDraftToActive(void) {
  if (!s_draftBits || s_draftW != Canvas_GetWidth() || s_draftH != Canvas_GetHeight())
    return;

  BYTE *activeBits = LayersGetActiveColorBits();
  if (!activeBits)
    return;

  int total = Canvas_GetWidth() * Canvas_GetHeight();
  for (int i = 0; i < total; i++) {
    BYTE *dft = s_draftBits + i * 4;
    BYTE dftA = dft[3];
    if (dftA == 0)
      continue;
    BYTE *dst = activeBits + i * 4;
    PixelOps_BlendPixel(dft[2], dft[1], dft[0], dftA, dst, LAYER_BLEND_NORMAL);
  }

  /* Clear the draft after merge */
  ZeroMemory(s_draftBits, Canvas_GetWidth() * Canvas_GetHeight() * 4);
  s_compositeDirty = TRUE;
}

BOOL LayersIsDraftDirty(void) {
  if (!s_draftBits || s_draftW <= 0 || s_draftH <= 0)
    return FALSE;

  /* Quick scan — check a few rows for any non-zero alpha */
  int total = s_draftW * s_draftH;
  for (int i = 0; i < total; i++) {
    if (s_draftBits[i * 4 + 3] != 0)
      return TRUE;
  }
  return FALSE;
}

BOOL LayersApplyTransformToAll(int newWidth, int newHeight,
                               BitmapTransformFunc pfnTransform,
                               void *pUserData) {
  if (!pfnTransform || s_layerCount <= 0)
    return FALSE;

  for (int i = 0; i < s_layerCount; i++) {
    Layer *layer = &s_layers[i];
    HBITMAP hNewColor =
        TransformBitmap(layer->colorBmp, Canvas_GetWidth(), Canvas_GetHeight(), newWidth,
                        newHeight, pfnTransform, pUserData);
    if (!hNewColor)
      return FALSE;

    BYTE *newColorBits = NULL;
    HBITMAP hColorDib = CreateDibSection32(newWidth, newHeight, &newColorBits);
    if (!hColorDib || !newColorBits) {
      if (hColorDib)
        DeleteObject(hColorDib);
      DeleteObject(hNewColor);
      return FALSE;
    }

    HDC hdcScreen = GetScreenDC();
    HDC hSrcDC = CreateTempDC(hdcScreen);
    HDC hDstDC = CreateTempDC(hdcScreen);
    if (hSrcDC && hDstDC) {
      HBITMAP hOldSrc = (HBITMAP)SelectObject(hSrcDC, hNewColor);
      HBITMAP hOldDst = (HBITMAP)SelectObject(hDstDC, hColorDib);
      BitBlt(hDstDC, 0, 0, newWidth, newHeight, hSrcDC, 0, 0, SRCCOPY);
      SelectObject(hSrcDC, hOldSrc);
      SelectObject(hDstDC, hOldDst);
    }
    if (hSrcDC)
      DeleteTempDC(hSrcDC);
    if (hDstDC)
      DeleteTempDC(hDstDC);
    ReleaseScreenDC(hdcScreen);

    DeleteObject(hNewColor);

    ClearLayerBitmaps(layer);
    layer->colorBmp = hColorDib;
    layer->colorBits = newColorBits;
  }

  Canvas_SetWidth(newWidth);
  Canvas_SetHeight(newHeight);
  
  s_compositeDirty = TRUE;
  return TRUE;
}

BOOL LayersApplyRawTransformToAll(int newWidth, int newHeight,
                                  RawBitmapTransformFunc pfnTransform,
                                  void *pUserData) {
  if (!pfnTransform || s_layerCount <= 0)
    return FALSE;

  for (int i = 0; i < s_layerCount; i++) {
    Layer *layer = &s_layers[i];

    BYTE *newColorBits = NULL;
    HBITMAP hColorDib = CreateDibSection32(newWidth, newHeight, &newColorBits);
    if (!hColorDib || !newColorBits) {
      if (hColorDib)
        DeleteObject(hColorDib);
      return FALSE;
    }

    // Apply transform directly to raw bits
    pfnTransform(layer->colorBits, Canvas_GetWidth(), Canvas_GetHeight(), newColorBits,
                 newWidth, newHeight, pUserData);

    ClearLayerBitmaps(layer);
    layer->colorBmp = hColorDib;
    layer->colorBits = newColorBits;
  }

  Canvas_SetWidth(newWidth);
  Canvas_SetHeight(newHeight);
  
  s_compositeDirty = TRUE;
  return TRUE;
}
