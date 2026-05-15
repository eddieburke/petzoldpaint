#ifndef POLY_STORE_H
#define POLY_STORE_H

#include <windows.h>

#define POLY_BLOCK_SIZE 128

typedef struct {
  POINT *points;
  int count;
  int capacity;
} PolyStore;

void Poly_Init(PolyStore *p);
void Poly_Free(PolyStore *p);
void Poly_Clear(PolyStore *p);
BOOL Poly_Add(PolyStore *p, int x, int y);
void Poly_SetLast(PolyStore *p, int x, int y);
POINT Poly_GetLast(PolyStore *p);
HRGN Poly_CreateRegion(PolyStore *p);
BOOL Poly_Copy(PolyStore *dst, const PolyStore *src);

#endif
