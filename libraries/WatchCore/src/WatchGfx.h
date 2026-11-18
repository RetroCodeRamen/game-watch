#pragma once
// Clip-safe drawing on the shared u8g2 buffer.
// U8g2 coords are unsigned: a negative x/y wraps to a huge value and paints garbage
// scanlines on the SH1106. Always draw scrolling content through these helpers.

#include <Arduino.h>

void plot(int16_t x, int16_t y);
void hLineSafe(int16_t x, int16_t y, int16_t w);
void vLineSafe(int16_t x, int16_t y, int16_t h);
void lineSafe(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void fillBoxClipped(int16_t x, int16_t y, int16_t w, int16_t h);
void frameSafe(int16_t x, int16_t y, int16_t w, int16_t h);
void discSafe(int16_t cx, int16_t cy, int16_t r);
void circleSafe(int16_t cx, int16_t cy, int16_t r);
void triSafe(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
void fillTriSafe(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
