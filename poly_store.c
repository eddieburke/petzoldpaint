/*------------------------------------------------------------
    poly_store.c - Dynamic POINT array for polygon/freeform tools
------------------------------------------------------------*/

#include "poly_store.h"
#include "geom.h"
#include <stdlib.h>
#include <string.h>

void Poly_Init(PolyStore *p) {
  p->points = NULL;
  p->count = 0;
  p->capacity = 0;
}

void Poly_Free(PolyStore *p) {
  if (p->points)
    free(p->points);
  Poly_Init(p);
}

void Poly_Clear(PolyStore *p) { p->count = 0; }

BOOL Poly_Add(PolyStore *p, int x, int y) {
  if (p->count >= p->capacity) {
    int newCap = p->capacity + POLY_BLOCK_SIZE;
    POINT *newPts = (POINT *)realloc(p->points, newCap * sizeof(POINT));
    if (!newPts)
      return FALSE;
    p->points = newPts;
    p->capacity = newCap;
  }
  p->points[p->count].x = x;
  p->points[p->count].y = y;
  p->count++;
  return TRUE;
}

void Poly_SetLast(PolyStore *p, int x, int y) {
  if (p->count > 0) {
    p->points[p->count - 1].x = x;
    p->points[p->count - 1].y = y;
  }
}

POINT Poly_GetLast(PolyStore *p) {
  POINT pt = {0, 0};
  if (p->count > 0)
    pt = p->points[p->count - 1];
  return pt;
}

RECT Poly_GetBounds(PolyStore *p) {
  return GetBoundingBox(p->points, p->count);
}

BOOL Poly_Copy(PolyStore *dst, const PolyStore *src) {
  Poly_Clear(dst);
  if (!src || src->count == 0)
    return TRUE;

  if (dst->capacity < src->count) {
    int newCap = src->count + POLY_BLOCK_SIZE;
    POINT *newPts = (POINT *)realloc(dst->points, newCap * sizeof(POINT));
    if (!newPts)
      return FALSE;
    dst->points = newPts;
    dst->capacity = newCap;
  }

  memcpy(dst->points, src->points, src->count * sizeof(POINT));
  dst->count = src->count;
  return TRUE;
}

HRGN Poly_CreateRegionEx(PolyStore *p, int fillMode) {
  if (!p || p->count < 3)
    return NULL;
  return CreatePolygonRgn(p->points, p->count, fillMode);
}

HRGN Poly_CreateRegion(PolyStore *p) {
  return Poly_CreateRegionEx(p, ALTERNATE);
}
