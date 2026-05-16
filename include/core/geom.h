#ifndef GEOM_H
#define GEOM_H
#include <windows.h>
int DistSq(int x1, int y1, int x2, int y2);
double Dist(int x1, int y1, int x2, int y2);
void SnapToAngle(int anchorX, int anchorY, int *curX, int *curY, int angleDeg);
void SnapToSquare(int anchorX, int anchorY, int *curX, int *curY);
void NormalizeRect(RECT *rc);
RECT GetRectFromPoints(int x1, int y1, int x2, int y2);
RECT GetBoundingBox(const POINT *points, int count);
void ClampRectToCanvas(RECT *rc, int cvsW, int cvsH);
#define HANDLE_SIZE  6
#define HT_NONE      -1
#define HT_BODY      8
#define HT_TL        0
#define HT_T         1
#define HT_TR        2
#define HT_R         3
#define HT_BR        4
#define HT_B         5
#define HT_BL        6
#define HT_L         7
#define HT_ROTATE_TL 10
#define HT_ROTATE_TR 11
#define HT_ROTATE_BR 12
#define HT_ROTATE_BL 13
BOOL IsShiftDown(void);
BOOL IsCtrlDown(void);
BOOL IsAltDown(void);
typedef struct {
	double scale;
	int offX;
	int offY;
} Viewport;
Viewport GetCurrentViewport(void);
double GetZoomScale(void);
void CoordBmpToScr(int xBmp, int yBmp, int *xScr, int *yScr);
void CoordScrToBmp(int xScr, int yScr, int *xBmp, int *yBmp);
void CoordScrToBmpDouble(int xScr, int yScr, double *xBmp, double *yBmp);
void RectBmpToScr(const RECT *rcBmp, RECT *rcScr);
void Viewport_BmpToScr(const Viewport *vp, int xBmp, int yBmp, int *xScr, int *yScr);
void Viewport_RectBmpToScr(const Viewport *vp, const RECT *rcBmp, RECT *rcScr);
void Viewport_ScrToBmp(const Viewport *vp, int xScr, int yScr, int *xBmp, int *yBmp);
void Viewport_ScrToBmpDouble(const Viewport *vp, int xScr, int yScr, double *xBmp, double *yBmp);
void GetScaledDimensions(int width, int height, int *scaledW, int *scaledH);
void ScreenDeltaToBitmap(int dx, int dy, int *outDx, int *outDy);
int HitTestBoxHandles(const RECT *rc, int x, int y, int tolerance);
#endif
