#include "WatchCore.h"

void plot(int16_t x, int16_t y) {
  if (x < 0 || y < 0 || x >= 128 || y >= 64) return;
  u8g2.drawPixel((u8g2_uint_t)x, (u8g2_uint_t)y);
}

void fillBoxClipped(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x >= 128 || y >= 64) return;
  if (x + w > 128) w = 128 - x;
  if (y + h > 64) h = 64 - y;
  if (w > 0 && h > 0) u8g2.drawBox((u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w, (u8g2_uint_t)h);
}

void hLineSafe(int16_t x, int16_t y, int16_t w) {
  if (y < 0 || y >= 64 || w <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (x >= 128) return;
  if (x + w > 128) w = 128 - x;
  if (w > 0) u8g2.drawHLine((u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w);
}

void vLineSafe(int16_t x, int16_t y, int16_t h) {
  if (x < 0 || x >= 128 || h <= 0) return;
  if (y < 0) { h += y; y = 0; }
  if (y >= 64) return;
  if (y + h > 64) h = 64 - y;
  if (h > 0) u8g2.drawVLine((u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)h);
}

void lineSafe(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  for (;;) {
    plot(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = err << 1;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void discSafe(int16_t cx, int16_t cy, int16_t r) {
  if (r < 0) return;
  if (cx + r < 0 || cy + r < 0 || cx - r >= 128 || cy - r >= 64) return;
  for (int16_t y = -r; y <= r; y++)
    for (int16_t x = -r; x <= r; x++)
      if (x * x + y * y <= r * r) plot(cx + x, cy + y);
}

void circleSafe(int16_t cx, int16_t cy, int16_t r) {
  if (r < 0) return;
  if (cx + r < 0 || cy + r < 0 || cx - r >= 128 || cy - r >= 64) return;
  int16_t x = -r, y = 0, err = 2 - 2 * r;
  do {
    plot(cx - x, cy + y);
    plot(cx - y, cy - x);
    plot(cx + x, cy - y);
    plot(cx + y, cy + x);
    r = err;
    if (r <= y) err += ++y * 2 + 1;
    if (r > x || err > y) err += ++x * 2 + 1;
  } while (x < 0);
}

void triSafe(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  lineSafe(x0, y0, x1, y1);
  lineSafe(x1, y1, x2, y2);
  lineSafe(x2, y2, x0, y0);
}

// Filled triangle via bbox + barycentric (clip-safe)
void fillTriSafe(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  int16_t minX = x0, maxX = x0, minY = y0, maxY = y0;
  if (x1 < minX) minX = x1;
  if (x1 > maxX) maxX = x1;
  if (x2 < minX) minX = x2;
  if (x2 > maxX) maxX = x2;
  if (y1 < minY) minY = y1;
  if (y1 > maxY) maxY = y1;
  if (y2 < minY) minY = y2;
  if (y2 > maxY) maxY = y2;
  if (maxX < 0 || maxY < 0 || minX >= 128 || minY >= 64) return;
  if (minX < 0) minX = 0;
  if (minY < 0) minY = 0;
  if (maxX > 127) maxX = 127;
  if (maxY > 63) maxY = 63;
  int32_t a = (int32_t)(x1 - x0) * (y2 - y0) - (int32_t)(x2 - x0) * (y1 - y0);
  if (a == 0) { triSafe(x0, y0, x1, y1, x2, y2); return; }
  for (int16_t y = minY; y <= maxY; y++) {
    for (int16_t x = minX; x <= maxX; x++) {
      int32_t w0 = (int32_t)(x1 - x) * (y2 - y) - (int32_t)(x2 - x) * (y1 - y);
      int32_t w1 = (int32_t)(x2 - x) * (y0 - y) - (int32_t)(x0 - x) * (y2 - y);
      int32_t w2 = (int32_t)(x0 - x) * (y1 - y) - (int32_t)(x1 - x) * (y0 - y);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
        plot(x, y);
    }
  }
}

void frameSafe(int16_t x, int16_t y, int16_t w, int16_t h) {
  hLineSafe(x, y, w);
  hLineSafe(x, y + h - 1, w);
  vLineSafe(x, y, h);
  vLineSafe(x + w - 1, y, h);
}
