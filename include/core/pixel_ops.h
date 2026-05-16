#ifndef PIXEL_OPS_H
#define PIXEL_OPS_H

#include <windows.h>


void PixelOps_InvertColors(BYTE *bits, int width, int height);


void PixelOps_Fill(BYTE *bits, int width, int height, COLORREF color,
                   BYTE alpha);
void PixelOps_FillRect(BYTE *bits, int width, int height, COLORREF color,
                       BYTE alpha, int startX, int startY, int endX, int endY);


static inline BYTE PixelOps_ApplyBlendMultiply(BYTE s, BYTE d) {
  return (BYTE)((s * d + 127) / 255);
}

static inline BYTE PixelOps_ApplyBlendScreen(BYTE s, BYTE d) {
  return (BYTE)(255 - ((255 - s) * (255 - d) + 127) / 255);
}

static inline BYTE PixelOps_ApplyBlendOverlay(BYTE s, BYTE d) {
  if (d < 128)
    return (BYTE)((2 * s * d + 127) / 255);
  return (BYTE)(255 - (2 * (255 - s) * (255 - d) + 127) / 255);
}

static inline BYTE PixelOps_ApplyBlendMode(BYTE s, BYTE d, int mode) {
  switch (mode) {
  case 1 /* LAYER_BLEND_MULTIPLY */:
    return PixelOps_ApplyBlendMultiply(s, d);
  case 2 /* LAYER_BLEND_SCREEN */:
    return PixelOps_ApplyBlendScreen(s, d);
  case 3 /* LAYER_BLEND_OVERLAY */:
    return PixelOps_ApplyBlendOverlay(s, d);
  case 0 /* LAYER_BLEND_NORMAL */:
  default:
    return s;
  }
}

/* Unified composite blending for all modes (Normal, Multiply, Screen, Overlay).
   Porter-Duff 'over' operator with specialized channel blending. */
static inline void PixelOps_BlendPixel(int sr, int sg, int sb, int sa,
                                       BYTE *px, int mode) {
  if (!px || sa == 0)
    return;

  if (mode == 0 /* LAYER_BLEND_NORMAL */ && sa == 255) {
    px[0] = (BYTE)sb;
    px[1] = (BYTE)sg;
    px[2] = (BYTE)sr;
    px[3] = 255;
    return;
  }

  int db = px[0];
  int dg = px[1];
  int dr = px[2];
  int da = px[3];

  int invSa = 255 - sa;
  int dstTerm = (da * invSa + 127) / 255;
  int outA = sa + dstTerm;

  if (outA == 0) {
    *(DWORD *)px = 0;
    return;
  }

  int srcR = sr, srcG = sg, srcB = sb;
  if (mode != 0 /* LAYER_BLEND_NORMAL */) {
    srcR = PixelOps_ApplyBlendMode((BYTE)sr, (BYTE)dr, mode);
    srcG = PixelOps_ApplyBlendMode((BYTE)sg, (BYTE)dg, mode);
    srcB = PixelOps_ApplyBlendMode((BYTE)sb, (BYTE)db, mode);
  }

  int outR = (srcR * sa + dr * dstTerm + outA / 2) / outA;
  int outG = (srcG * sa + dg * dstTerm + outA / 2) / outA;
  int outB = (srcB * sa + db * dstTerm + outA / 2) / outA;

  if (outR > 255) outR = 255;
  if (outG > 255) outG = 255;
  if (outB > 255) outB = 255;
  if (outA > 255) outA = 255;

  px[0] = (BYTE)outB;
  px[1] = (BYTE)outG;
  px[2] = (BYTE)outR;
  px[3] = (BYTE)outA;
}


void PixelOps_FillCheckerboard(BYTE *bits, int width, int height);
void PixelOps_FillCheckerboardRect(BYTE *bits, int width, int height,
                                   int startX, int startY, int endX, int endY);

void PixelOps_Flip(BYTE *bits, int width, int height, BOOL bHorz);

#endif
