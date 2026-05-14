/*------------------------------------------------------------------------------
 * CRAYON_TOOL.C
 *
 * Crayon Tool Implementation
 *
 * Features a novel stroke system with:
 * - Spline-based smooth curve interpolation
 * - Speed-based pressure simulation
 * - Directional noise texture
 * - Natural texture buildup
 *----------------------------------------------------------------------------*/

#include "crayon_tool.h"
#include "../canvas.h"
#include "../draw.h"
#include "../gdi_utils.h"
#include "../helpers.h"
#include "../layers.h"
#include "../resource.h"
#include "../ui/widgets/colorbox.h"
#include "../ui/widgets/toolbar.h"
#include "tool_options/presets.h"
#include "tool_options/tool_options.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stroke_session.h"

/*------------------------------------------------------------------------------
 * Crayon Presets
 *----------------------------------------------------------------------------*/

typedef struct {
  int density;
  int textureIntensity;
  int sprayAmount;
  int colorVariation;
  int brightnessRange;
  int saturationRange;
  int hueShiftRange;
  int size;
} CrayonPresetData;

static void CrayonPreset_GetCurrent(CrayonPresetData *out) {
  if (!out)
    return;
  out->density = nCrayonDensity;
  out->textureIntensity = nCrayonTextureIntensity;
  out->sprayAmount = nCrayonSprayAmount;
  out->colorVariation = nCrayonColorVariation;
  out->brightnessRange = nCrayonBrightnessRange;
  out->saturationRange = nCrayonSaturationRange;
  out->hueShiftRange = nCrayonHueShiftRange;
  out->size = nBrushWidth;
}

static void CrayonPreset_Apply(const void *data, size_t size) {
  if (!data || size != sizeof(CrayonPresetData))
    return;

  const CrayonPresetData *d = (const CrayonPresetData *)data;
  nCrayonDensity = d->density;
  nCrayonTextureIntensity = d->textureIntensity;
  nCrayonSprayAmount = d->sprayAmount;
  nCrayonColorVariation = d->colorVariation;
  nCrayonBrightnessRange = d->brightnessRange;
  nCrayonSaturationRange = d->saturationRange;
  nCrayonHueShiftRange = d->hueShiftRange;
  nBrushWidth = d->size;

  SetStoredLineWidth(d->size);
  {
    HWND h = GetToolOptionsWindow();
    if (h)
      InvalidateWindow(h);
  }
  InvalidateCanvas();
}

static BOOL CrayonPreset_SaveCurrent(void) {
  static int customCounter = 1;
  char name[MAX_PRESET_NAME];
  CrayonPresetData d;

CrayonPreset_GetCurrent(&d);
   int n = snprintf(name, sizeof(name), "Custom %d", customCounter++);

  return Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, name, &d, sizeof(d),
                    FALSE);
}

void CrayonTool_RegisterPresets(void) {
  Preset_RegisterSlot(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, CrayonPreset_Apply,
                      CrayonPreset_SaveCurrent);

  CrayonPresetData c;

  // Soft Preset
  c.density = 30;
  c.textureIntensity = 40;
  c.sprayAmount = 20;
  c.colorVariation = 30;
  c.brightnessRange = 20;
  c.saturationRange = 20;
  c.hueShiftRange = 5;
  c.size = 2;
  Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, "Soft", &c, sizeof(c), TRUE);

  // Medium Preset
  c.density = 50;
  c.textureIntensity = 60;
  c.sprayAmount = 40;
  c.colorVariation = 50;
  c.brightnessRange = 35;
  c.saturationRange = 30;
  c.hueShiftRange = 10;
  c.size = 3;
  Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, "Medium", &c, sizeof(c),
             TRUE);

  // Bold Preset
  c.density = 80;
  c.textureIntensity = 85;
  c.sprayAmount = 60;
  c.colorVariation = 70;
  c.brightnessRange = 50;
  c.saturationRange = 45;
  c.hueShiftRange = 15;
  c.size = 4;
  Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, "Bold", &c, sizeof(c), TRUE);

  // Rough Preset
  c.density = 70;
  c.textureIntensity = 95;
  c.sprayAmount = 80;
  c.colorVariation = 60;
  c.brightnessRange = 60;
  c.saturationRange = 50;
  c.hueShiftRange = 20;
  c.size = 3;
  Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, "Rough", &c, sizeof(c),
             TRUE);

  // Smooth Preset
  c.density = 40;
  c.textureIntensity = 25;
  c.sprayAmount = 15;
  c.colorVariation = 35;
  c.brightnessRange = 25;
  c.saturationRange = 25;
  c.hueShiftRange = 8;
  c.size = 2;
  Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, "Smooth", &c, sizeof(c),
             TRUE);
}

/*------------------------------------------------------------
   Drawing State with Stroke History
  ------------------------------------------------------------*/

#define MAX_STROKE_POINTS 64

typedef struct {
  float x, y;
  DWORD time;
  float pressure;
} StrokePoint;

static StrokeSession s_session = {0};
static int s_currentNoiseSeed = 0;
static StrokePoint s_strokePoints[MAX_STROKE_POINTS];
static int s_strokePointCount = 0;

/*------------------------------------------------------------
    Improved Noise Implementation
   ------------------------------------------------------------*/

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
    // Apply simple smoothing to make noise less blocky
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

// Rough grain noise - fast hash per pixel
static float GrainNoise(int x, int y, int seed) {
   int n = x * 73856093 + y * 19349663 + seed * 83492791;
   n = (n << 13) ^ n;
   n = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
   return ((float)n / 2147483647.0f);
}

// Directional noise - follows stroke direction with very rough texture
static float DirectionalNoise(float x, float y, float dirX, float dirY,
                               int seed) {
   // High-frequency base noise for rough grain
   float baseNoise = FastNoise2D(x, y);

   // Directional component - creates texture that follows stroke
   float dirLen = sqrtf(dirX * dirX + dirY * dirY);
   if (dirLen > 0.001f) {
     float ndx = dirX / dirLen;
     float ndy = dirY / dirLen;

     // Sample noise along perpendicular direction for stroke-like texture
     float perpX = -ndy;
     float perpY = ndx;
     float perpNoise = SampleNoise(
         x * 0.3f + perpX * 3.0f, y * 0.3f + perpY * 3.0f, s_noiseTex);

     // Combine base and directional noise
     baseNoise = baseNoise * 0.5f + perpNoise * 0.5f;
   }

   // Add multi-octave noise for rough texture
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

   // Add pixel-level grain for extra roughness
   int ix = (int)floorf(x);
   int iy = (int)floorf(y);
   float grain = GrainNoise(ix, iy, seed + 20000);
   value += (grain - 0.5f) * 0.2f;

   // Normalize to 0-1
   value = (value + 1.5f) / 3.0f;
   if (value < 0.0f) value = 0.0f;
   if (value > 1.0f) value = 1.0f;
   return value;
}

// Get separate noise values for color variation (brightness, hue shift)
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

// Modify color with crayon-like texture variation
static COLORREF ApplyCrayonColorVariation(COLORREF baseColor,
                                          float brightnessNoise, float hueNoise,
                                          float saturationNoise) {
  int r = GetRValue(baseColor);
  int g = GetGValue(baseColor);
  int b = GetBValue(baseColor);

  // Use color variation setting to scale effect
  float variationScale = (float)nCrayonColorVariation / 100.0f;
  float brightnessRange = (float)nCrayonBrightnessRange / 100.0f;
  float saturationRange = (float)nCrayonSaturationRange / 100.0f;
  float hueShiftRange = (float)nCrayonHueShiftRange / 100.0f;

  // Brightness variation: scaled by brightness range setting
  float brightnessMin = 1.0f - brightnessRange * 0.3f;
  float brightnessMax = 1.0f + brightnessRange * 0.4f;
  float brightness =
      brightnessMin + brightnessNoise * (brightnessMax - brightnessMin);
  brightness = 1.0f + (brightness - 1.0f) * variationScale;
  r = (int)(r * brightness);
  g = (int)(g * brightness);
  b = (int)(b * brightness);

  // Saturation variation: scaled by saturation range setting
  float saturationMin = 1.0f - saturationRange * 0.4f;
  float saturation = saturationMin + saturationNoise * (1.0f - saturationMin);
  saturation = 1.0f + (saturation - 1.0f) * variationScale;
  int gray = (r + g + b) / 3;
  r = (int)(gray + (r - gray) * saturation);
  g = (int)(gray + (g - gray) * saturation);
  b = (int)(gray + (b - gray) * saturation);

  // Hue shift: scaled by hue shift range setting
  float hueShift = (hueNoise - 0.5f) * hueShiftRange * 0.2f * variationScale;
  if (hueShift > 0) {
    // Shift slightly warmer (more red/yellow)
    r = (int)(r + (255 - r) * hueShift * 0.3f);
    g = (int)(g + (255 - g) * hueShift * 0.2f);
  } else {
    // Shift slightly cooler (more blue)
    b = (int)(b + (255 - b) * (-hueShift) * 0.2f);
  }

  // Clamp values
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

/*------------------------------------------------------------
   Spline Interpolation
  ------------------------------------------------------------*/

// Catmull-Rom spline interpolation for smooth curves
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

/*------------------------------------------------------------
   Pressure Calculation
  ------------------------------------------------------------*/

// Calculate pressure based on stroke speed
static float CalculatePressure(StrokePoint *p1, StrokePoint *p2) {
  if (p1->time == 0 || p2->time == 0)
    return 1.0f;

  float dx = p2->x - p1->x;
  float dy = p2->y - p1->y;
  float distance = sqrtf(dx * dx + dy * dy);

  DWORD timeDelta = p2->time - p1->time;
  if (timeDelta == 0)
    timeDelta = 1;

  // Speed in pixels per millisecond
  float speed = distance / (float)timeDelta;

  // Inverse relationship: slower = higher pressure
  // Clamp speed to reasonable range (0.01 to 2.0 px/ms)
  if (speed < 0.01f)
    speed = 0.01f;
  if (speed > 2.0f)
    speed = 2.0f;

  // Pressure: 0.3 (fast) to 1.0 (slow)
  float pressure = 0.3f + (2.0f - speed) / 2.0f * 0.7f;
  if (pressure < 0.3f)
    pressure = 0.3f;
  if (pressure > 1.0f)
    pressure = 1.0f;

  return pressure;
}

/*------------------------------------------------------------
   Crayon Size
  ------------------------------------------------------------*/

static int GetCrayonSize(void) {
  int sizes[] = {6, 10, 14, 18, 22};
  int idx = (nBrushWidth - 1);
  if (idx < 0)
    idx = 0;
  if (idx > 4)
    idx = 4;
  return sizes[idx];
}

/*------------------------------------------------------------
   Low-level Rendering Helpers
  ------------------------------------------------------------*/

// Forward declaration for renderer
static void ApplySprayEffect(BYTE *bits, int width, int height, float centerX,
                             float centerY, float radius, COLORREF color,
                             float pressure, int seed);

static BYTE CalcCrayonBaseAlpha(void) {
  float densityFactor = (float)nCrayonDensity / 100.0f;
  float textureFactor = (float)nCrayonTextureIntensity / 100.0f;
  // Higher base alpha for more visible rough texture, scaled by texture
  // intensity
  return (BYTE)(20 + densityFactor * 100 * textureFactor);
}

static void DrawCrayonSpot(BYTE *bits, int width, int height, float sx,
                           float sy, float radius, COLORREF color,
                           float pressure, float dirX, float dirY) {
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

      // Get directional noise texture for opacity
      float noise =
          DirectionalNoise(pixelX, pixelY, dirX, dirY, s_currentNoiseSeed);

      // Get color variation noise for realistic crayon texture
      float brightnessNoise, hueNoise, saturationNoise;
      GetColorVariationNoise(pixelX, pixelY, s_currentNoiseSeed,
                             &brightnessNoise, &hueNoise, &saturationNoise);

      // Apply color variation to create textured crayon appearance
      COLORREF variedColor = ApplyCrayonColorVariation(
          color, brightnessNoise, hueNoise, saturationNoise);

      // Noise modulation scaled by texture intensity
      float textureFactor = (float)nCrayonTextureIntensity / 100.0f;
      float textureMod =
          noise *
          (0.5f + textureFactor * 1.3f); // Texture intensity controls range
      if (textureMod > 1.0f)
        textureMod = 1.0f;
      if (textureMod < 0.0f)
        textureMod = 0.0f;

      // Edge falloff with smooth transition
      float edgeFactor = 1.0f;
      if (dist > radius - 1.0f) {
        edgeFactor = (radius + 0.5f - dist) / 1.5f;
        if (edgeFactor < 0.0f)
          edgeFactor = 0.0f;
        edgeFactor = SmoothStep(edgeFactor);
      }

      // Calculate final alpha with pressure, noise, and edge
      BYTE alpha = (BYTE)(baseAlpha * pressure * textureMod * edgeFactor);

      if (alpha > 0) {
        DrawPixelAlpha(bits, width, height, px, py, variedColor, alpha, LAYER_BLEND_NORMAL);
      }
    }
  }

  // Apply aliased spraybrush effect at the end for extra texture
  ApplySprayEffect(bits, width, height, sx, sy, radius, color, pressure,
                   s_currentNoiseSeed);
}

/*------------------------------------------------------------
   Novel Stroke Rendering
  ------------------------------------------------------------*/

// Draw a smooth stroke segment using spline interpolation
static void DrawCrayonStrokeSmooth(BYTE *bits, int width, int height,
                                   StrokePoint *p0, StrokePoint *p1,
                                   StrokePoint *p2, StrokePoint *p3,
                                   COLORREF color, int size, float pressure) {
  if (!p1 || !p2)
    return;

  float radius = (size / 2.0f) * pressure;

  // Calculate direction for directional noise
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

  // Determine number of spline steps based on distance
  float dist = sqrtf((p2->x - p1->x) * (p2->x - p1->x) +
                     (p2->y - p1->y) * (p2->y - p1->y));
  int steps = (int)(dist * 2.0f) + 1;
  if (steps < 2)
    steps = 2;
  if (steps > 50)
    steps = 50;

  // Draw spline curve
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

    DrawCrayonSpot(bits, width, height, sx, sy, radius, color, pressure, dirX,
                   dirY);
  }
}

// Aliased spraybrush effect - adds scattered pixels at the end for texture
static void ApplySprayEffect(BYTE *bits, int width, int height, float centerX,
                             float centerY, float radius, COLORREF color,
                             float pressure, int seed) {
  // Number of spray particles based on area, pressure, and spray amount setting
  float sprayFactor = (float)nCrayonSprayAmount / 100.0f;
  int sprayCount = (int)(radius * radius * 0.15f * pressure * sprayFactor);
  if (sprayCount < 1)
    sprayCount = 1;
  if (sprayCount > 100)
    sprayCount = 100; // Increased max for more spray options

  // Use seed for deterministic randomness
  int localSeed = seed + (int)(centerX * 1000.0f) + (int)(centerY * 1000.0f);

  for (int i = 0; i < sprayCount; i++) {
    // Generate random offset within radius
    // Simple LCG for deterministic randomness
    localSeed = (localSeed * 1103515245 + 12345) & 0x7fffffff;
    float angle = ((float)(localSeed % 62832) / 10000.0f); // 0 to 2*PI
    localSeed = (localSeed * 1103515245 + 12345) & 0x7fffffff;
    float dist = ((float)(localSeed % 10000) / 10000.0f); // 0 to 1
    dist = dist * dist;    // Square for more center-weighted distribution
    dist *= radius * 1.2f; // Extend slightly beyond radius

    int px = (int)(centerX + cosf(angle) * dist + 0.5f);
    int py = (int)(centerY + sinf(angle) * dist + 0.5f);

    if (px < 0 || px >= width || py < 0 || py >= height)
      continue;

    // Get color variation for spray particles
    float brightnessNoise, hueNoise, saturationNoise;
    GetColorVariationNoise((float)px, (float)py, seed + i * 1000,
                           &brightnessNoise, &hueNoise, &saturationNoise);

    COLORREF variedColor = ApplyCrayonColorVariation(color, brightnessNoise,
                                                     hueNoise, saturationNoise);

    // Random alpha for spray particles (aliased - full opacity or low)
    localSeed = (localSeed * 1103515245 + 12345) & 0x7fffffff;
    BYTE sprayAlpha = (BYTE)(80 + (localSeed % 100)); // 80-180 alpha range

    // Draw aliased pixel (no anti-aliasing, hard edge)
    DrawPixelAlpha(bits, width, height, px, py, variedColor, sprayAlpha,
                   LAYER_BLEND_NORMAL);
  }
}

/*------------------------------------------------------------
   Stroke Point Management
  ------------------------------------------------------------*/

static void AddStrokePoint(int x, int y) {
  DWORD currentTime = GetTickCount();

  // Shift points if buffer is full
  if (s_strokePointCount >= MAX_STROKE_POINTS) {
    for (int i = 0; i < MAX_STROKE_POINTS - 1; i++) {
      s_strokePoints[i] = s_strokePoints[i + 1];
    }
    s_strokePointCount = MAX_STROKE_POINTS - 1;
  }

  // Add new point
  StrokePoint *pt = &s_strokePoints[s_strokePointCount];
  pt->x = (float)x;
  pt->y = (float)y;
  pt->time = currentTime;

  // Calculate pressure based on previous point
  if (s_strokePointCount > 0) {
    pt->pressure =
        CalculatePressure(&s_strokePoints[s_strokePointCount - 1], pt);
  } else {
    pt->pressure = 1.0f;
  }

  s_strokePointCount++;
}

/*------------------------------------------------------------
   Main Stroke Drawing
  ------------------------------------------------------------*/

static void DrawCrayonStroke(BYTE *bits, int width, int height,
                             COLORREF color) {
  if (s_strokePointCount < 2)
    return;

  int size = GetCrayonSize();

  // Draw smooth strokes between points
  for (int i = 0; i < s_strokePointCount - 1; i++) {
    StrokePoint *p1 = &s_strokePoints[i];
    StrokePoint *p2 = &s_strokePoints[i + 1];

    // Get control points for spline
    StrokePoint *p0 = (i > 0) ? &s_strokePoints[i - 1] : NULL;
    StrokePoint *p3 =
        (i < s_strokePointCount - 2) ? &s_strokePoints[i + 2] : NULL;

    // Use average pressure
    float pressure = (p1->pressure + p2->pressure) * 0.5f;

    DrawCrayonStrokeSmooth(bits, width, height, p0, p1, p2, p3, color, size,
                           pressure);
  }
}

/*------------------------------------------------------------
   Crayon Tool Public API
  ------------------------------------------------------------*/

void CrayonToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
   InitNoiseTextures();
   StrokeSession_Begin(&s_session, hWnd, x, y, nButton, TOOL_CRAYON);

  // Generate new random seed for this stroke
  s_currentNoiseSeed = (int)GetTickCount() + (x * 1619) + (y * 31337);

  // Reset stroke points
  s_strokePointCount = 0;
  AddStrokePoint(x, y);

  // Draw initial point
  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    int size = GetCrayonSize();
    float radius = size / 2.0f;
    DrawCrayonSpot(bits, Canvas_GetWidth(), Canvas_GetHeight(), (float)x, (float)y, radius,
                   GetColorForButton(s_session.drawButton), 1.0f, 1.0f, 0.0f);
    LayersMarkDirty();
    StrokeSession_MarkPixelsModified(&s_session);
  }
  InvalidateCanvas();
}

void CrayonToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  if (!s_session.isDrawing || !StrokeSession_IsActiveButton(nButton))
    return;

  // Add new point to stroke
  AddStrokePoint(x, y);

  // Draw stroke from last few points (keep it smooth and responsive)
  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    // Only draw the most recent segment to avoid redrawing entire stroke
    int startIdx = (s_strokePointCount >= 3) ? s_strokePointCount - 3 : 0;
    int endIdx = s_strokePointCount - 1;

    if (endIdx > startIdx) {
      int size = GetCrayonSize();
      COLORREF color = GetColorForButton(s_session.drawButton);

      for (int i = startIdx; i < endIdx; i++) {
        StrokePoint *p1 = &s_strokePoints[i];
        StrokePoint *p2 = &s_strokePoints[i + 1];

        StrokePoint *p0 = (i > 0) ? &s_strokePoints[i - 1] : NULL;
        StrokePoint *p3 = (i < endIdx - 1) ? &s_strokePoints[i + 2] : NULL;

        float pressure = (p1->pressure + p2->pressure) * 0.5f;

        DrawCrayonStrokeSmooth(bits, Canvas_GetWidth(), Canvas_GetHeight(), p0, p1, p2, p3,
                               color, size, pressure);
      }
      LayersMarkDirty();
      StrokeSession_MarkPixelsModified(&s_session);
    }
  }

  InvalidateCanvas();
}

void CrayonToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  // Add final point
  if (s_session.isDrawing) {
    AddStrokePoint(x, y);

    // Draw final stroke segment
    BYTE *bits = LayersGetActiveColorBits();
    if (bits && s_strokePointCount >= 2) {
      int size = GetCrayonSize();
      COLORREF color = GetColorForButton(s_session.drawButton);

      int startIdx = (s_strokePointCount >= 3) ? s_strokePointCount - 3 : 0;
      int endIdx = s_strokePointCount - 1;

      for (int i = startIdx; i < endIdx; i++) {
        StrokePoint *p1 = &s_strokePoints[i];
        StrokePoint *p2 = &s_strokePoints[i + 1];

        StrokePoint *p0 = (i > 0) ? &s_strokePoints[i - 1] : NULL;
        StrokePoint *p3 = (i < endIdx - 1) ? &s_strokePoints[i + 2] : NULL;

        float pressure = (p1->pressure + p2->pressure) * 0.5f;

        DrawCrayonStrokeSmooth(bits, Canvas_GetWidth(), Canvas_GetHeight(), p0, p1, p2, p3,
                               color, size, pressure);
      }
      LayersMarkDirty();
    }
  }

  StrokeSession_CommitIfNeeded(&s_session, "Draw");
  StrokeSession_End(&s_session);
  s_strokePointCount = 0;
}

BOOL IsCrayonDrawing(void) { return s_session.isDrawing; }

void CrayonTool_Deactivate(void) {
  if (s_session.isDrawing) {
    StrokeSession_End(&s_session);
    s_strokePointCount = 0;
  }
}

BOOL CancelCrayonDrawing(void) {
  if (!s_session.isDrawing) {
    s_strokePointCount = 0;
    return FALSE;
  }
  StrokeSession_Cancel(&s_session);
  s_strokePointCount = 0;
  return TRUE;
}

void CrayonTool_OnCaptureLost(void) {
  StrokeSession_OnCaptureLost(&s_session, "Draw");
}
