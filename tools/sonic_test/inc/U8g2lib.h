#pragma once
#include "Arduino.h"
#include <stdio.h>
typedef uint16_t u8g2_uint_t;
static const uint8_t u8g2_font_6x10_tr[1]={0}, u8g2_font_logisoso24_tn[1]={0}, u8g2_font_5x7_tr[1]={0}, u8g2_font_4x6_tr[1]={0}, u8g2_font_ncenB18_tr[1]={0}, u8g2_font_ncenB08_tr[1]={0}, u8g2_font_ncenB10_tr[1]={0};
struct U8G2_SH1106_128X64_NONAME_F_HW_I2C {
  uint8_t buf[1024]; uint8_t col = 1; int cx = 0, cy = 0;
  void setDrawColor(uint8_t c) { col = c; }
  void drawPixel(int x, int y) { x &= 0xffff; y &= 0xffff; if (x >= 128 || y >= 64) return; uint8_t m = 1 << (y & 7); if (col) buf[(y>>3)*128+x] |= m; else buf[(y>>3)*128+x] &= ~m; }
  void drawBox(int x, int y, int w, int h) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) drawPixel(x+i, y+j); }
  void drawFrame(int x, int y, int w, int h) { drawHLine(x,y,w); drawHLine(x,y+h-1,w); drawVLine(x,y,h); drawVLine(x+w-1,y,h); }
  void drawHLine(int x, int y, int w) { for (int i = 0; i < w; i++) drawPixel(x+i, y); }
  void drawVLine(int x, int y, int h) { for (int i = 0; i < h; i++) drawPixel(x, y+i); }
  void drawTriangle(int, int, int, int, int, int) {}
  void drawXBMP(int x, int y, int w, int h, const uint8_t *b) { int rb = (w+7)/8; for (int j=0;j<h;j++) for(int i=0;i<w;i++) if (b[j*rb+i/8] & (1<<(i&7))) drawPixel(x+i,y+j); }
  uint8_t *getBufferPtr() { return buf; }
  int getStrWidth(const char *s) { return 5 * (int)strlen(s); }
  void setCursor(int x, int y) { cx = x; cy = y; }
  void setFont(const uint8_t *) {}
  template<class T> void print(T) {}
  void clearBuffer() { memset(buf, 0, sizeof buf); }
};
