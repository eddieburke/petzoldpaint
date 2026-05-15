#include "crayon_tool.h"
#include "canvas.h"
#include "draw.h"
#include "gdi_utils.h"
#include "helpers.h"
#include "layers.h"
#include "resource.h"
#include "ui/widgets/colorbox.h"
#include "ui/widgets/toolbar.h"
#include "tool_options/presets.h"
#include "tool_options/tool_options.h"
#include "brush_presets.h"

#include <math.h>
#include <stdlib.h>
#include "interaction.h"

#define LCG_A 1103515245
#define LCG_C 12345
#define LCG_M 0x7fffffff
#define TWO_PI_10K 62832.0f


static void CrayonPreset_GetCurrent(BrushPresetData *out) {
  if (!out)
    return;
  out->crayon.density = nCrayonDensity;
  out->crayon.textureIntensity = nCrayonTextureIntensity;
  out->crayon.sprayAmount = nCrayonSprayAmount;
  out->crayon.colorVariation = nCrayonColorVariation;
  out->crayon.brightnessRange = nCrayonBrightnessRange;
  out->crayon.saturationRange = nCrayonSaturationRange;
  out->crayon.hueShiftRange = nCrayonHueShiftRange;
  out->size = nBrushWidth;
}

static void CrayonPreset_Apply(const void *data, size_t size) {
  if (!data || size != sizeof(BrushPresetData))
    return;

  const BrushPresetData *d = (const BrushPresetData *)data;
  nCrayonDensity = d->crayon.density;
  nCrayonTextureIntensity = d->crayon.textureIntensity;
  nCrayonSprayAmount = d->crayon.sprayAmount;
  nCrayonColorVariation = d->crayon.colorVariation;
  nCrayonBrightnessRange = d->crayon.brightnessRange;
  nCrayonSaturationRange = d->crayon.saturationRange;
  nCrayonHueShiftRange = d->crayon.hueShiftRange;
  nBrushWidth = d->size;

  SetStoredLineWidth(d->size);
  {
    HWND h = GetToolOptionsWindow();
    if (h)
      InvalidateRect(h, NULL, FALSE);
  }
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

static BOOL CrayonPreset_SaveCurrent(void) {
  return BrushPreset_SaveCurrent(PRESET_SLOT_CRAYON, (BrushGetFn)CrayonPreset_GetCurrent);
}

void CrayonTool_RegisterPresets(void) {
  Preset_RegisterSlot(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, CrayonPreset_Apply,
                      CrayonPreset_SaveCurrent);

  BrushPresetData c;

  c.crayon.density = 30;
  c.crayon.textureIntensity = 40;
  c.crayon.sprayAmount = 20;
  c.crayon.colorVariation = 30;
  c.crayon.brightnessRange = 20;
  c.crayon.saturationRange = 20;
  c.crayon.hueShiftRange = 5;
  c.size = 2;
  BrushPreset_Add(PRESET_SLOT_CRAYON, "Soft", &c, TRUE);

  c.crayon.density = 50;
  c.crayon.textureIntensity = 60;
  c.crayon.sprayAmount = 40;
  c.crayon.colorVariation = 50;
  c.crayon.brightnessRange = 35;
  c.crayon.saturationRange = 30;
  c.crayon.hueShiftRange = 10;
  c.size = 3;
  BrushPreset_Add(PRESET_SLOT_CRAYON, "Medium", &c, TRUE);

  c.crayon.density = 80;
  c.crayon.textureIntensity = 85;
  c.crayon.sprayAmount = 60;
  c.crayon.colorVariation = 70;
  c.crayon.brightnessRange = 50;
  c.crayon.saturationRange = 45;
  c.crayon.hueShiftRange = 15;
  c.size = 4;
  BrushPreset_Add(PRESET_SLOT_CRAYON, "Bold", &c, TRUE);

  c.crayon.density = 70;
  c.crayon.textureIntensity = 95;
  c.crayon.sprayAmount = 80;
  c.crayon.colorVariation = 60;
  c.crayon.brightnessRange = 60;
  c.crayon.saturationRange = 50;
  c.crayon.hueShiftRange = 20;
  c.size = 3;
  BrushPreset_Add(PRESET_SLOT_CRAYON, "Rough", &c, TRUE);

  c.crayon.density = 40;
  c.crayon.textureIntensity = 25;
  c.crayon.sprayAmount = 15;
  c.crayon.colorVariation = 35;
  c.crayon.brightnessRange = 25;
  c.crayon.saturationRange = 25;
  c.crayon.hueShiftRange = 8;
  c.size = 2;
  BrushPreset_Add(PRESET_SLOT_CRAYON, "Smooth", &c, TRUE);
}


#define MAX_STROKE_POINTS 64

typedef struct {
  float x, y;
  DWORD time;
  float pressure;
} StrokePoint;

static int s_currentNoiseSeed = 0;
static StrokePoint s_strokePoints[MAX_STROKE_POINTS];
static int s_strokePointCount = 0;


#define NOISE_TEX_SIZE 64
static float s_noiseTex[NOISE_TEX_SIZE * NOISE_TEX_SIZE];
static float s_noiseTexDetail[NOISE_TEX_SIZE * NOISE_TEX_SIZE];
static float s_noiseTexBright[NOISE_TEX_SIZE * NOISE_TEX_SIZE];
static float s_noiseTexHue[NOISE_TEX_SIZE * NOISE_TEX_SIZE];
static float s_noiseTexSat[NOISE_TEX_SIZE * NOISE_TEX_SIZE];
static BOOL s_noiseInit = FALSE;

static float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

static void InitNoiseTextures(void) {
    if (s_noiseInit) return;
    srand(42);
    for (int i = 0; i < NOISE_TEX_SIZE * NOISE_TEX_SIZE; i++) {
        s_noiseTex[i] = (float)rand() / (float)RAND_MAX;
        s_noiseTexDetail[i] = (float)rand() / (float)RAND_MAX;
        s_noiseTexBright[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        s_noiseTexHue[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        s_noiseTexSat[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    }
    float blurred[NOISE_TEX_SIZE * NOISE_TEX_SIZE];
    for (int iter = 0; iter < 2; iter++) {
        for (int y = 0; y < NOISE_TEX_SIZE; y++) {
            for (int x = 0; x < NOISE_TEX_SIZE; x++) {
                float sum = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int tx = (x + dx + NOISE_TEX_SIZE) & (NOISE_TEX_SIZE - 1);
                        int ty = (y + dy + NOISE_TEX_SIZE) & (NOISE_TEX_SIZE - 1);
                        sum += s_noiseTex[ty * NOISE_TEX_SIZE + tx];
                    }
                }
                blurred[y * NOISE_TEX_SIZE + x] = sum / 9.0f;
            }
        }
        memcpy(s_noiseTex, blurred, sizeof(s_noiseTex));
    }
    s_noiseInit = TRUE;
}

static float SampleNoise(float x, float y, const float *tex) {
    float fx = x * 0.25f;
    float fy = y * 0.25f;
    int ix = ((int)floorf(fx)) & (NOISE_TEX_SIZE - 1);
    int iy = ((int)floorf(fy)) & (NOISE_TEX_SIZE - 1);
    float sx = fx - floorf(fx);
    float sy = fy - floorf(fy);
    float n00 = tex[iy * NOISE_TEX_SIZE + ix];
    float n10 = tex[iy * NOISE_TEX_SIZE + ((ix + 1) & (NOISE_TEX_SIZE - 1))];
    float n01 = tex[((iy + 1) & (NOISE_TEX_SIZE - 1)) * NOISE_TEX_SIZE + ix];
    float n11 = tex[((iy + 1) & (NOISE_TEX_SIZE - 1)) * NOISE_TEX_SIZE + ((ix + 1) & (NOISE_TEX_SIZE - 1))];
    float a = n00 + sx * (n10 - n00);
    float b = n01 + sx * (n11 - n01);
    return a + sy * (b - a);
}

static float SampleNoiseFine(float x, float y, const float *tex) {
    float fx = x * 0.5f;
    float fy = y * 0.5f;
    int ix = ((int)floorf(fx)) & (NOISE_TEX_SIZE - 1);
    int iy = ((int)floorf(fy)) & (NOISE_TEX_SIZE - 1);
    float sx = fx - floorf(fx);
    float sy = fy - floorf(fy);
    float n00 = tex[iy * NOISE_TEX_SIZE + ix];
    float n10 = tex[iy * NOISE_TEX_SIZE + ((ix + 1) & (NOISE_TEX_SIZE - 1))];
    float n01 = tex[((iy + 1) & (NOISE_TEX_SIZE - 1)) * NOISE_TEX_SIZE + ix];
    float n11 = tex[((iy + 1) & (NOISE_TEX_SIZE - 1)) * NOISE_TEX_SIZE + ((ix + 1) & (NOISE_TEX_SIZE - 1))];
    float a = n00 + sx * (n10 - n00);
    float b = n01 + sx * (n11 - n01);
    return a + sy * (b - a);
}

static float FastNoise2D(float x, float y) {
    float v = SampleNoise(x, y, s_noiseTex);
    v += SampleNoiseFine(x, y, s_noiseTexDetail) * 0.3f;
    return v;
}

static float GrainNoise(int x, int y, int seed) {
   int n = x * 73856093 + y * 19349663 + seed * 83492791;
   n = (n << 13) ^ n;
   n = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
   return ((float)n / 2147483647.0f);
}

static float DirectionalNoise(float x, float y, float dirX, float dirY,
                               int seed) {
   float baseNoise = FastNoise2D(x, y);

   float dirLen = sqrtf(dirX * dirX + dirY * dirY);
   if (dirLen > 0.001f) {
     float ndx = dirX / dirLen;
     float ndy = dirY / dirLen;

     float perpX = -ndy;
     float perpY = ndx;
     float perpNoise = SampleNoise(
         x * 0.3f + perpX * 3.0f, y * 0.3f + perpY * 3.0f, s_noiseTex);

     baseNoise = baseNoise * 0.5f + perpNoise * 0.5f;
   }

   float value = baseNoise;
   float amplitude = 0.6f;
   float frequency = 0.3f;
   for (int i = 1; i < 6; i++) {
     float noise;
     if (i < 3)
         noise = SampleNoise(x * frequency, y * frequency, s_noiseTex);
     else
         noise = SampleNoiseFine(x * frequency, y * frequency, s_noiseTexDetail);
     value += noise * amplitude;
     amplitude *= 0.55f;
     frequency *= 2.0f;
   }

   int ix = (int)floorf(x);
   int iy = (int)floorf(y);
   float grain = GrainNoise(ix, iy, seed + 20000);
   value += (grain - 0.5f) * 0.2f;

   value = (value + 1.5f) / 3.0f;
   if (value < 0.0f) value = 0.0f;
   if (value > 1.0f) value = 1.0f;
   return value;
}

static void GetColorVariationNoise(float x, float y, int seed,
                                    float *brightnessNoise, float *hueNoise,
                                    float *saturationNoise) {
   *brightnessNoise = SampleNoise(x, y, s_noiseTexBright);
   *brightnessNoise += SampleNoiseFine(x * 2.5f, y * 2.5f, s_noiseTexDetail) * 0.5f;
   *brightnessNoise += GrainNoise((int)floorf(x), (int)floorf(y), seed + 32000) * 0.3f;
   if (*brightnessNoise < -1.0f) *brightnessNoise = -1.0f;
   if (*brightnessNoise > 1.0f) *brightnessNoise = 1.0f;
   *brightnessNoise = (*brightnessNoise + 1.0f) / 2.0f;

   *hueNoise = SampleNoise(x, y, s_noiseTexHue);
   *hueNoise += SampleNoiseFine(x * 2.0f, y * 2.0f, s_noiseTexDetail) * 0.4f;
   if (*hueNoise < -1.0f) *hueNoise = -1.0f;
   if (*hueNoise > 1.0f) *hueNoise = 1.0f;
   *hueNoise = (*hueNoise + 1.0f) / 2.0f;

   *saturationNoise = SampleNoise(x, y, s_noiseTexSat);
   *saturationNoise += GrainNoise((int)floorf(x), (int)floorf(y), seed + 51000) * 0.4f;
   if (*saturationNoise < -1.0f) *saturationNoise = -1.0f;
   if (*saturationNoise > 1.0f) *saturationNoise = 1.0f;
   *saturationNoise = (*saturationNoise + 1.0f) / 2.0f;
}

static COLORREF ApplyCrayonColorVariation(COLORREF baseColor,
                                          float brightnessNoise, float hueNoise,
                                          float saturationNoise) {
  int r = GetRValue(baseColor);
  int g = GetGValue(baseColor);
  int b = GetBValue(baseColor);

  float variationScale = (float)nCrayonColorVariation / 100.0f;
  float brightnessRange = (float)nCrayonBrightnessRange / 100.0f;
  float saturationRange = (float)nCrayonSaturationRange / 100.0f;
  float hueShiftRange = (float)nCrayonHueShiftRange / 100.0f;

  float brightnessMin = 1.0f - brightnessRange * 0.3f;
  float brightnessMax = 1.0f + brightnessRange * 0.4f;
  float brightness =
      brightnessMin + brightnessNoise * (brightnessMax - brightnessMin);
  brightness = 1.0f + (brightness - 1.0f) * variationScale;
  r = (int)(r * brightness);
  g = (int)(g * brightness);
  b = (int)(b * brightness);

  float saturationMin = 1.0f - saturationRange * 0.4f;
  float saturation = saturationMin + saturationNoise * (1.0f - saturationMin);
  saturation = 1.0f + (saturation - 1.0f) * variationScale;
  int gray = (r + g + b) / 3;
  r = (int)(gray + (r - gray) * saturation);
  g = (int)(gray + (g - gray) * saturation);
  b = (int)(gray + (b - gray) * saturation);

  float hueShift = (hueNoise - 0.5f) * hueShiftRange * 0.2f * variationScale;
  if (hueShift > 0) {
    r = (int)(r + (255 - r) * hueShift * 0.3f);
    g = (int)(g + (255 - g) * hueShift * 0.2f);
  } else {
    b = (int)(b + (255 - b) * (-hueShift) * 0.2f);
  }

  if (r < 0)
    r = 0;
  if (r > 255)
    r = 255;
  if (g < 0)
    g = 0;
  if (g > 255)
    g = 255;
  if (b < 0)
    b = 0;
  if (b > 255)
    b = 255;

  return RGB(r, g, b);
}


static void CatmullRomSpline(float t, float p0x, float p0y, float p1x,
                             float p1y, float p2x, float p2y, float p3x,
                             float p3y, float *outX, float *outY) {
  float t2 = t * t;
  float t3 = t2 * t;

  *outX = 0.5f * ((2.0f * p1x) + (-p0x + p2x) * t +
                  (2.0f * p0x - 5.0f * p1x + 4.0f * p2x - p3x) * t2 +
                  (-p0x + 3.0f * p1x - 3.0f * p2x + p3x) * t3);

  *outY = 0.5f * ((2.0f * p1y) + (-p0y + p2y) * t +
                  (2.0f * p0y - 5.0f * p1y + 4.0f * p2y - p3y) * t2 +
                  (-p0y + 3.0f * p1y - 3.0f * p2y + p3y) * t3);
}


static float CalculatePressure(StrokePoint *p1, StrokePoint *p2) {
  if (p1->time == 0 || p2->time == 0)
    return 1.0f;

  float dx = p2->x - p1->x;
  float dy = p2->y - p1->y;
  float distance = sqrtf(dx * dx + dy * dy);

  DWORD timeDelta = p2->time - p1->time;
  if (timeDelta == 0)
    timeDelta = 1;

  float speed = distance / (float)timeDelta;

  if (speed < 0.01f)
    speed = 0.01f;
  if (speed > 2.0f)
    speed = 2.0f;

  float pressure = 0.3f + (2.0f - speed) / 2.0f * 0.7f;
  if (pressure < 0.3f)
    pressure = 0.3f;
  if (pressure > 1.0f)
    pressure = 1.0f;

  return pressure;
}


static int GetCrayonSize(void) {
  int sizes[] = {6, 10, 14, 18, 22};
  int idx = (nBrushWidth - 1);
  if (idx < 0)
    idx = 0;
  if (idx > 4)
    idx = 4;
  return sizes[idx];
}


static void ApplySprayEffect(BYTE *bits, int width, int height, float centerX,
                             float centerY, float radius, COLORREF color,
                             BYTE colorAlpha, float pressure, int seed);

static BYTE CalcCrayonBaseAlpha(void) {
  float densityFactor = (float)nCrayonDensity / 100.0f;
  float textureFactor = (float)nCrayonTextureIntensity / 100.0f;
  return (BYTE)(20 + densityFactor * 100 * textureFactor);
}

static void DrawCrayonSpot(BYTE *bits, int width, int height, float sx,
                           float sy, float radius, COLORREF color,
                           BYTE colorAlpha, float pressure, float dirX, float dirY) {
  int centerX = (int)(sx + 0.5f);
  int centerY = (int)(sy + 0.5f);
  int r = (int)(radius + 1.5f);
  BYTE baseAlpha = CalcCrayonBaseAlpha();

  int minX = max(0, centerX - r);
  int maxX = min(width - 1, centerX + r);
  int minY = max(0, centerY - r);
  int maxY = min(height - 1, centerY + r);

  for (int py = minY; py <= maxY; py++) {
    for (int px = minX; px <= maxX; px++) {
      float pixelX = (float)px + 0.5f;
      float pixelY = (float)py + 0.5f;

      float dx = pixelX - sx;
      float dy = pixelY - sy;
      float distSq = dx * dx + dy * dy;
      float maxDist = radius + 0.5f;

      if (distSq > maxDist * maxDist)
        continue;

      float dist = sqrtf(distSq);

      float noise =
          DirectionalNoise(pixelX, pixelY, dirX, dirY, s_currentNoiseSeed);

      float brightnessNoise, hueNoise, saturationNoise;
      GetColorVariationNoise(pixelX, pixelY, s_currentNoiseSeed,
                             &brightnessNoise, &hueNoise, &saturationNoise);

      COLORREF variedColor = ApplyCrayonColorVariation(
          color, brightnessNoise, hueNoise, saturationNoise);

      float textureFactor = (float)nCrayonTextureIntensity / 100.0f;
      float textureMod =
          noise *
          (0.5f + textureFactor * 1.3f);
      if (textureMod > 1.0f)
        textureMod = 1.0f;
      if (textureMod < 0.0f)
        textureMod = 0.0f;

      float edgeFactor = 1.0f;
      if (dist > radius - 1.0f) {
        edgeFactor = (radius + 0.5f - dist) / 1.5f;
        if (edgeFactor < 0.0f)
          edgeFactor = 0.0f;
        edgeFactor = SmoothStep(edgeFactor);
      }

      BYTE alpha = ComposeOpacity((BYTE)(baseAlpha * pressure * textureMod * edgeFactor),
                                  colorAlpha);

      if (alpha > 0) {
        DrawPixelAlpha(bits, width, height, px, py, variedColor, alpha, LAYER_BLEND_NORMAL);
      }
    }
  }

  ApplySprayEffect(bits, width, height, sx, sy, radius, color, colorAlpha, pressure,
                   s_currentNoiseSeed);
}


static void DrawCrayonStrokeSmooth(BYTE *bits, int width, int height,
                                   StrokePoint *p0, StrokePoint *p1,
                                   StrokePoint *p2, StrokePoint *p3,
                                   COLORREF color, BYTE colorAlpha, int size, float pressure) {
  if (!p1 || !p2)
    return;

  float radius = (size / 2.0f) * pressure;

  float dirX = p2->x - p1->x;
  float dirY = p2->y - p1->y;
  float dirLen = sqrtf(dirX * dirX + dirY * dirY);
  if (dirLen < 0.001f) {
    dirX = 1.0f;
    dirY = 0.0f;
  } else {
    dirX /= dirLen;
    dirY /= dirLen;
  }

  float dist = sqrtf((p2->x - p1->x) * (p2->x - p1->x) +
                     (p2->y - p1->y) * (p2->y - p1->y));
  int steps = (int)(dist * 2.0f) + 1;
  if (steps < 2)
    steps = 2;
  if (steps > 50)
    steps = 50;

  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;

    float sx, sy;
    if (p0 && p3) {
      CatmullRomSpline(t, p0->x, p0->y, p1->x, p1->y, p2->x, p2->y, p3->x,
                       p3->y, &sx, &sy);
    } else if (p0) {
      CatmullRomSpline(t, p0->x, p0->y, p1->x, p1->y, p2->x, p2->y, p2->x,
                       p2->y, &sx, &sy);
    } else if (p3) {
      CatmullRomSpline(t, p1->x, p1->y, p1->x, p1->y, p2->x, p2->y, p3->x,
                       p3->y, &sx, &sy);
    } else {
      sx = p1->x + (p2->x - p1->x) * t;
      sy = p1->y + (p2->y - p1->y) * t;
    }

    DrawCrayonSpot(bits, width, height, sx, sy, radius, color, colorAlpha, pressure, dirX,
                   dirY);
  }
}

static void ApplySprayEffect(BYTE *bits, int width, int height, float centerX,
                             float centerY, float radius, COLORREF color,
                             BYTE colorAlpha, float pressure, int seed) {
  float sprayFactor = (float)nCrayonSprayAmount / 100.0f;
  int sprayCount = (int)(radius * radius * 0.15f * pressure * sprayFactor);
  if (sprayCount < 1)
    sprayCount = 1;
  if (sprayCount > 100)
    sprayCount = 100;

  int localSeed = seed + (int)(centerX * 1000.0f) + (int)(centerY * 1000.0f);

  for (int i = 0; i < sprayCount; i++) {
    localSeed = (localSeed * LCG_A + LCG_C) & LCG_M;
    float angle = ((float)(localSeed % (int)TWO_PI_10K) / 10000.0f);
    localSeed = (localSeed * LCG_A + LCG_C) & LCG_M;
    float dist = ((float)(localSeed % 10000) / 10000.0f);
    dist = dist * dist;
    dist *= radius * 1.2f;

    int px = (int)(centerX + cosf(angle) * dist + 0.5f);
    int py = (int)(centerY + sinf(angle) * dist + 0.5f);

    if (px < 0 || px >= width || py < 0 || py >= height)
      continue;

    float brightnessNoise, hueNoise, saturationNoise;
    GetColorVariationNoise((float)px, (float)py, seed + i * 1000,
                           &brightnessNoise, &hueNoise, &saturationNoise);

    COLORREF variedColor = ApplyCrayonColorVariation(color, brightnessNoise,
                                                     hueNoise, saturationNoise);

    localSeed = (localSeed * LCG_A + LCG_C) & LCG_M;
    BYTE sprayAlpha = ComposeOpacity((BYTE)(80 + (localSeed % 100)), colorAlpha);

    DrawPixelAlpha(bits, width, height, px, py, variedColor, sprayAlpha,
                   LAYER_BLEND_NORMAL);
  }
}


static void AddStrokePoint(int x, int y) {
  DWORD currentTime = GetTickCount();

  if (s_strokePointCount >= MAX_STROKE_POINTS) {
    for (int i = 0; i < MAX_STROKE_POINTS - 1; i++) {
      s_strokePoints[i] = s_strokePoints[i + 1];
    }
    s_strokePointCount = MAX_STROKE_POINTS - 1;
  }

  StrokePoint *pt = &s_strokePoints[s_strokePointCount];
  pt->x = (float)x;
  pt->y = (float)y;
  pt->time = currentTime;

  if (s_strokePointCount > 0) {
    pt->pressure =
        CalculatePressure(&s_strokePoints[s_strokePointCount - 1], pt);
  } else {
    pt->pressure = 1.0f;
  }

  s_strokePointCount++;
}


static void DrawCrayonStroke(BYTE *bits, int width, int height,
                             COLORREF color, BYTE colorAlpha) {
  if (s_strokePointCount < 2)
    return;

  int size = GetCrayonSize();

  for (int i = 0; i < s_strokePointCount - 1; i++) {
    StrokePoint *p1 = &s_strokePoints[i];
    StrokePoint *p2 = &s_strokePoints[i + 1];

    StrokePoint *p0 = (i > 0) ? &s_strokePoints[i - 1] : NULL;
    StrokePoint *p3 =
        (i < s_strokePointCount - 2) ? &s_strokePoints[i + 2] : NULL;

    float pressure = (p1->pressure + p2->pressure) * 0.5f;

    DrawCrayonStrokeSmooth(bits, width, height, p0, p1, p2, p3, color, colorAlpha, size,
                           pressure);
  }
}


void CrayonToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
   InitNoiseTextures();
   Interaction_Begin(hWnd, x, y, nButton, TOOL_CRAYON);

  BYTE *bits = LayersGetActiveColorBits();
  if (!bits) {
    Interaction_Abort();
    return;
  }

  s_currentNoiseSeed = (int)GetTickCount() + (x * 1619) + (y * 31337);

  s_strokePointCount = 0;
  AddStrokePoint(x, y);
  Interaction_UpdateLastPoint(x, y);

  int size = GetCrayonSize();
  float radius = size / 2.0f;
  BYTE colorAlpha = GetOpacityForButton(Interaction_GetDrawButton());
  DrawCrayonSpot(bits, Canvas_GetWidth(), Canvas_GetHeight(), (float)x, (float)y, radius,
                 GetColorForButton(Interaction_GetDrawButton()), colorAlpha, 1.0f, 1.0f, 0.0f);
  LayersMarkDirty();
  Interaction_MarkModified();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void CrayonToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  if (!Interaction_IsActive() || !Interaction_IsActiveButton(nButton))
    return;

  AddStrokePoint(x, y);

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    int startIdx = (s_strokePointCount >= 3) ? s_strokePointCount - 3 : 0;
    int endIdx = s_strokePointCount - 1;

    if (endIdx > startIdx) {
      int size = GetCrayonSize();
      COLORREF color = GetColorForButton(Interaction_GetDrawButton());
      BYTE colorAlpha = GetOpacityForButton(Interaction_GetDrawButton());

      for (int i = startIdx; i < endIdx; i++) {
        StrokePoint *p1 = &s_strokePoints[i];
        StrokePoint *p2 = &s_strokePoints[i + 1];

        StrokePoint *p0 = (i > 0) ? &s_strokePoints[i - 1] : NULL;
        StrokePoint *p3 = (i < endIdx - 1) ? &s_strokePoints[i + 2] : NULL;

        float pressure = (p1->pressure + p2->pressure) * 0.5f;

        DrawCrayonStrokeSmooth(bits, Canvas_GetWidth(), Canvas_GetHeight(), p0, p1, p2, p3,
                               color, colorAlpha, size, pressure);
      }
      LayersMarkDirty();
      Interaction_MarkModified();
    }
  }

  Interaction_UpdateLastPoint(x, y);
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void CrayonToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  (void)nButton;
  if (Interaction_IsActive()) {
    AddStrokePoint(x, y);

    BYTE *bits = LayersGetActiveColorBits();
    if (bits && s_strokePointCount >= 2) {
      int size = GetCrayonSize();
      COLORREF color = GetColorForButton(Interaction_GetDrawButton());
      BYTE colorAlpha = GetOpacityForButton(Interaction_GetDrawButton());

      int startIdx = (s_strokePointCount >= 3) ? s_strokePointCount - 3 : 0;
      int endIdx = s_strokePointCount - 1;

      for (int i = startIdx; i < endIdx; i++) {
        StrokePoint *p1 = &s_strokePoints[i];
        StrokePoint *p2 = &s_strokePoints[i + 1];

        StrokePoint *p0 = (i > 0) ? &s_strokePoints[i - 1] : NULL;
        StrokePoint *p3 = (i < endIdx - 1) ? &s_strokePoints[i + 2] : NULL;

        float pressure = (p1->pressure + p2->pressure) * 0.5f;

        DrawCrayonStrokeSmooth(bits, Canvas_GetWidth(), Canvas_GetHeight(), p0, p1, p2, p3,
                               color, colorAlpha, size, pressure);
      }
      LayersMarkDirty();
      Interaction_MarkModified();
    }
  }

  Interaction_UpdateLastPoint(x, y);
  Interaction_Commit("Draw");
  s_strokePointCount = 0;
}

BOOL IsCrayonDrawing(void) { return Interaction_IsActive(); }

void CrayonTool_Deactivate(void) {
  if (Interaction_IsActive()) {
    Interaction_EndQuiet();
    s_strokePointCount = 0;
  }
}

BOOL CancelCrayonDrawing(void) {
  if (!Interaction_IsActive()) {
    s_strokePointCount = 0;
    return FALSE;
  }
  Interaction_Abort();
  s_strokePointCount = 0;
  return TRUE;
}

