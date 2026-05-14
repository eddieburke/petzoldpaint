/*------------------------------------------------------------
    geom.h - Math, geometry, polygon, and coordinate conversion
------------------------------------------------------------*/

#ifndef GEOM_H
#define GEOM_H

#include <windows.h>

/*------------------------------------------------------------
    Math and Geometry
------------------------------------------------------------*/

int DistSq(int x1, int y1, int x2, int y2);
double Dist(int x1, int y1, int x2, int y2);

void SnapToAngle(int anchorX, int anchorY, int* curX, int* curY, int angleDeg);
void SnapToSquare(int anchorX, int anchorY, int* curX, int* curY);

void NormalizeRect(RECT* rc);
RECT GetRectFromPoints(int x1, int y1, int x2, int y2);
RECT GetBoundingBox(const POINT* points, int count);
void ClampRectToCanvas(RECT* rc, int cvsW, int cvsH);

BOOL IsShiftDown(void);
BOOL IsCtrlDown(void);
BOOL IsAltDown(void);

/*------------------------------------------------------------
    Coordinate Conversion
------------------------------------------------------------*/

double GetZoomScale(void);

void CoordBmpToScr(int xBmp, int yBmp, int* xScr, int* yScr);
void CoordScrToBmp(int xScr, int yScr, int* xBmp, int* yBmp);
void CoordScrToBmpDouble(int xScr, int yScr, double* xBmp, double* yBmp);
void RectBmpToScr(const RECT* rcBmp, RECT* rcScr);

void CoordBmpToScrEx(int xBmp, int yBmp, int* xScr, int* yScr, double scale, int offX, int offY);
void RectBmpToScrEx(const RECT* rcBmp, RECT* rcScr, double scale, int offX, int offY);
void CoordScrToBmpEx(int xScr, int yScr, int* xBmp, int* yBmp, double scale, int offX, int offY);
void CoordScrToBmpExDouble(int xScr, int yScr, double* xBmp, double* yBmp, double scale, int offX, int offY);

void GetScaledDimensions(int width, int height, int* scaledW, int* scaledH);
void ScreenDeltaToBitmap(int dx, int dy, int* outDx, int* outDy);

#endif /* GEOM_H */
