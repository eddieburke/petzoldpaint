#include "peztold_core.h"
#include "pixel_ops.h"
#include "layers.h"

void PixelOps_InvertColors(BYTE *bits, int width, int height) {
  if (!bits || width <= 0 || height <= 0)
    return;
  DWORD *pixels = (DWORD *)bits;
  int count = width * height;

  for (int i = 0; i < count; i++) {
    // XOR with 0x00FFFFFF flips B, G, R, but leaves Alpha intact.
    pixels[i] ^= 0x00FFFFFF;
  }
}


void PixelOps_Fill(BYTE *bits, int width, int height, COLORREF color,
                   BYTE alpha) {
  if (!bits || width <= 0 || height <= 0)
    return;

  BYTE r = GetRValue(color);
  BYTE g = GetGValue(color);
  BYTE b = GetBValue(color);

  // BGRA (little-endian: B, G, R, A)
  DWORD pixel = (DWORD)(b | (g << 8) | (r << 16) | (alpha << 24));

  int count = width * height;
  DWORD *dst = (DWORD *)bits;

  for (int i = 0; i < count; i++) {
    dst[i] = pixel;
  }
}


void PixelOps_FillRect(BYTE *bits, int width, int height, COLORREF color,
                     BYTE alpha, int startX, int startY, int endX, int endY) {
  if (!bits || width <= 0 || height <= 0)
    return;
  if (startX < 0)
    startX = 0;
  if (startY < 0)
    startY = 0;
  if (endX > width)
    endX = width;
  if (endY > height)
    endY = height;
  if (startX >= endX || startY >= endY)
    return;

  BYTE r = GetRValue(color);
  BYTE g = GetGValue(color);
  BYTE b = GetBValue(color);
  DWORD pixel = (DWORD)(b | (g << 8) | (r << 16) | (alpha << 24));

  for (int y = startY; y < endY; y++) {
    DWORD *row = (DWORD *)(bits + y * width * 4);
    for (int x = startX; x < endX; x++)
      row[x] = pixel;
  }
}

void PixelOps_FillCheckerboard(BYTE *bits, int width, int height) {
  PixelOps_FillCheckerboardRect(bits, width, height, 0, 0, width, height);
}

void PixelOps_FillCheckerboardRect(BYTE *bits, int width, int height,
                                   int startX, int startY, int endX, int endY) {
  if (!bits || width <= 0 || height <= 0)
    return;

  // Clamp rect
  if (startX < 0)
    startX = 0;
  if (startY < 0)
    startY = 0;
  if (endX > width)
    endX = width;
  if (endY > height)
    endY = height;

  if (startX >= endX || startY >= endY)
    return;

  COLORREF c1 = RGB(192, 192, 192);
  COLORREF c2 = RGB(255, 255, 255);
  int tile = 8;
  DWORD d1 = (DWORD)(GetBValue(c1) | (GetGValue(c1) << 8) |
                     (GetRValue(c1) << 16) | (255 << 24));
  DWORD d2 = (DWORD)(GetBValue(c2) | (GetGValue(c2) << 8) |
                     (GetRValue(c2) << 16) | (255 << 24));

  for (int y = startY; y < endY; y++) {
    int yTile = (y / tile) & 1;
    DWORD *row = (DWORD *)(bits + y * width * 4);
    for (int x = startX; x < endX; x++) {
      int xTile = (x / tile) & 1;
      row[x] = (xTile ^ yTile) ? d1 : d2;
    }
  }
}


/* (empty — implementations now in pixel_ops.h) */

void PixelOps_Flip(BYTE *bits, int width, int height, BOOL bHorz) {
  DWORD *px = (DWORD *)bits;
  if (!px || width <= 0 || height <= 0)
    return;

  if (bHorz) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width / 2; x++) {
        DWORD tmp = px[y * width + x];
        px[y * width + x] = px[y * width + (width - 1 - x)];
        px[y * width + (width - 1 - x)] = tmp;
      }
    }
  } else {
    for (int y = 0; y < height / 2; y++) {
      for (int x = 0; x < width; x++) {
        DWORD tmp = px[y * width + x];
        px[y * width + x] = px[(height - 1 - y) * width + x];
        px[(height - 1 - y) * width + x] = tmp;
      }
    }
  }
}
