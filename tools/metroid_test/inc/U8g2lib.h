#pragma once
// Host stub of the U8g2 bits the watch uses. Text is drawn as small blocks (one per glyph)
// so screenshots show where text lands and how wide it is.
#include "Arduino.h"
#include <stdio.h>
#include <string>
typedef uint16_t u8g2_uint_t;
static const uint8_t u8g2_font_6x10_tr[1]={6}, u8g2_font_logisoso24_tn[1]={14}, u8g2_font_5x7_tr[1]={5},
  u8g2_font_4x6_tr[1]={4}, u8g2_font_ncenB18_tr[1]={15}, u8g2_font_ncenB08_tr[1]={7}, u8g2_font_ncenB10_tr[1]={9};
struct U8G2_SH1106_128X64_NONAME_F_HW_I2C {
  uint8_t buf[1024]; uint8_t col = 1; int cx = 0, cy = 0; int fw = 5;
  void setDrawColor(uint8_t c) { col = c; }
  void drawPixel(int x, int y) { x &= 0xffff; y &= 0xffff; if (x >= 128 || y >= 64) return; uint8_t m = 1 << (y & 7); if (col) buf[(y>>3)*128+x] |= m; else buf[(y>>3)*128+x] &= ~m; }
  void drawBox(int x, int y, int w, int h) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) drawPixel(x+i, y+j); }
  void drawFrame(int x, int y, int w, int h) { drawHLine(x,y,w); drawHLine(x,y+h-1,w); drawVLine(x,y,h); drawVLine(x+w-1,y,h); }
  void drawHLine(int x, int y, int w) { for (int i = 0; i < w; i++) drawPixel(x+i, y); }
  void drawVLine(int x, int y, int h) { for (int i = 0; i < h; i++) drawPixel(x, y+i); }
  void drawTriangle(int, int, int, int, int, int) {}
  void drawXBMP(int x, int y, int w, int h, const uint8_t *b) { int rb = (w+7)/8; for (int j=0;j<h;j++) for(int i=0;i<w;i++) if (b[j*rb+i/8] & (1<<(i&7))) drawPixel(x+i,y+j); }
  uint8_t *getBufferPtr() { return buf; }
  int getStrWidth(const char *s) { return fw * (int)strlen(s); }
  void setCursor(int x, int y) { cx = x; cy = y; }
  void setFont(const uint8_t *f) { fw = f[0]; }
  void text(const std::string &s) {
    int h = fw <= 4 ? 5 : fw <= 6 ? 7 : fw + 3;
    for (char c : s) { if (c != ' ') drawBox(cx, cy - h + 1, fw - 1, h - 1 + (c=='_'));
      cx += fw; }
  }
  void print(const char *s) { text(s); }
  void print(const class __FlashStringHelper *s) { text((const char *)s); }
  void print(char c) { text(std::string(1, c)); }
  void print(int v) { text(std::to_string(v)); }
  void print(unsigned v) { text(std::to_string(v)); }
  void print(long v) { text(std::to_string(v)); }
  void print(unsigned long v) { text(std::to_string(v)); }
  void print(uint8_t v) { text(std::to_string((int)v)); }
  void print(uint16_t v) { text(std::to_string((int)v)); }
  void clearBuffer() { memset(buf, 0, sizeof buf); }
};
