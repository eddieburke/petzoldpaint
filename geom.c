/*------------------------------------------------------------
    geom.c - Math, geometry, polygon, and coordinate conversion
------------------------------------------------------------*/

#define _USE_MATH_DEFINES
#include "peztold_core.h"
#include "canvas.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*------------------------------------------------------------
    Math and Geometry
------------------------------------------------------------*/

int DistSq(int x1, int y1, int x2, int y2)
{
    int dx = x1 - x2;
    int dy = y1 - y2;
    return dx * dx + dy * dy;
}

double Dist(int x1, int y1, int x2, int y2)
{
    return sqrt((double)DistSq(x1, y1, x2, y2));
}

void SnapToAngle(int anchorX, int anchorY, int* curX, int* curY, int angleDeg)
{
    int dx = *curX - anchorX;
    int dy = *curY - anchorY;
    if (dx == 0 && dy == 0)
        return;

    double angleRad = atan2((double)dy, (double)dx);
    double deg = angleRad * 180.0 / M_PI;
    if (deg < 0)
        deg += 360.0;

    double snap = (double)angleDeg;
    double snapped = floor((deg + snap / 2.0) / snap) * snap;
    double rad = snapped * M_PI / 180.0;
    double dist = sqrt((double)(dx * dx + dy * dy));

    *curX = anchorX + (int)(dist * cos(rad));
    *curY = anchorY + (int)(dist * sin(rad));
}

void SnapToSquare(int anchorX, int anchorY, int* curX, int* curY)
{
    int dx = *curX - anchorX;
    int dy = *curY - anchorY;
    int absDx = abs(dx);
    int absDy = abs(dy);
    int maxD = (absDx > absDy) ? absDx : absDy;

    *curX = anchorX + ((dx >= 0) ? maxD : -maxD);
    *curY = anchorY + ((dy >= 0) ? maxD : -maxD);
}

void NormalizeRect(RECT* rc)
{
    if (rc->left > rc->right) {
        int t = rc->left;
        rc->left = rc->right;
        rc->right = t;
    }
    if (rc->top > rc->bottom) {
        int t = rc->top;
        rc->top = rc->bottom;
        rc->bottom = t;
    }
}

RECT GetRectFromPoints(int x1, int y1, int x2, int y2)
{
    RECT rc = {x1, y1, x2, y2};
    NormalizeRect(&rc);
    return rc;
}

RECT GetBoundingBox(const POINT* points, int count)
{
    RECT rc = {0};
    if (count <= 0)
        return rc;

    rc.left = rc.right = points[0].x;
    rc.top = rc.bottom = points[0].y;

    for (int i = 1; i < count; i++) {
        if (points[i].x < rc.left)
            rc.left = points[i].x;
        if (points[i].x > rc.right)
            rc.right = points[i].x;
        if (points[i].y < rc.top)
            rc.top = points[i].y;
        if (points[i].y > rc.bottom)
            rc.bottom = points[i].y;
    }
    return rc;
}

void ClampRectToCanvas(RECT* rc, int cvsW, int cvsH)
{
    if (rc->left < 0)
        rc->left = 0;
    if (rc->top < 0)
        rc->top = 0;
    if (rc->right > cvsW)
        rc->right = cvsW;
    if (rc->bottom > cvsH)
        rc->bottom = cvsH;
}

BOOL IsShiftDown(void)
{
    return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
}

BOOL IsCtrlDown(void)
{
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

BOOL IsAltDown(void)
{
    return (GetKeyState(VK_MENU) & 0x8000) != 0;
}

/*------------------------------------------------------------
    Coordinate Conversion
------------------------------------------------------------*/

double GetZoomScale(void)
{
    return Canvas_GetZoom() / 100.0;
}

void CoordBmpToScrEx(int xBmp, int yBmp, int* xScr, int* yScr,
                    double scale, int offX, int offY)
{
    *xScr = (int)floor(xBmp * scale + 0.5) + offX;
    *yScr = (int)floor(yBmp * scale + 0.5) + offY;
}

void RectBmpToScrEx(const RECT* rcBmp, RECT* rcScr,
                   double scale, int offX, int offY)
{
    rcScr->left   = (LONG)((int)floor(rcBmp->left   * scale) + offX);
    rcScr->top    = (LONG)((int)floor(rcBmp->top    * scale) + offY);
    rcScr->right  = (LONG)((int)ceil (rcBmp->right  * scale) + offX);
    rcScr->bottom = (LONG)((int)ceil (rcBmp->bottom * scale) + offY);

    // Ensure non-empty bitmap rects have at least a 1x1 screen footprint
    if (rcBmp->right > rcBmp->left && rcScr->right <= rcScr->left) rcScr->right = rcScr->left + 1;
    if (rcBmp->bottom > rcBmp->top && rcScr->bottom <= rcScr->top) rcScr->bottom = rcScr->top + 1;
}

void CoordBmpToScr(int xBmp, int yBmp, int* xScr, int* yScr)
{
    int nDestX, nDestY;
    GetCanvasViewportOrigin(&nDestX, &nDestY);
    CoordBmpToScrEx(xBmp, yBmp, xScr, yScr, GetZoomScale(), nDestX, nDestY);
}

void CoordScrToBmpEx(int xScr, int yScr, int* xBmp, int* yBmp,
                    double scale, int offX, int offY)
{
    *xBmp = (int)floor(((double)xScr - offX) / scale);
    *yBmp = (int)floor(((double)yScr - offY) / scale);
}

void CoordScrToBmp(int xScr, int yScr, int* xBmp, int* yBmp)
{
    int nDestX, nDestY;
    GetCanvasViewportOrigin(&nDestX, &nDestY);
    CoordScrToBmpEx(xScr, yScr, xBmp, yBmp, GetZoomScale(), nDestX, nDestY);
}

void RectBmpToScr(const RECT* rcBmp, RECT* rcScr)
{
    int nDestX, nDestY;
    GetCanvasViewportOrigin(&nDestX, &nDestY);
    RectBmpToScrEx(rcBmp, rcScr, GetZoomScale(), nDestX, nDestY);
}

void GetScaledDimensions(int width, int height, int* scaledW, int* scaledH)
{
    double dScale = GetZoomScale();
    *scaledW = (int)(width * dScale);
    *scaledH = (int)(height * dScale);
}

void ScreenDeltaToBitmap(int dx, int dy, int* outDx, int* outDy)
{
    double scale = GetZoomScale();
    if (outDx)
        *outDx = (int)(dx / scale);
    if (outDy)
        *outDy = (int)(dy / scale);
}
