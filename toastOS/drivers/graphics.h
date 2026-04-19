#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "stdint.h"

#define FONT_W 8
#define FONT_H 16

extern const uint8_t font_8x16[95][16];

uint32_t gfx_blend(uint32_t fg, uint32_t bg);

void gfx_init(uint32_t *framebuffer, uint32_t width, uint32_t height, uint32_t pitch);
int gfx_is_ready(void);
uint32_t *gfx_get_backbuffer(void);
void gfx_flush(void);

void gfx_clear(uint32_t color);
void gfx_put_pixel(int x, int y, uint32_t color);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color);
void gfx_draw_rect_alpha(int x, int y, int w, int h, uint32_t color);
void gfx_draw_outline(int x, int y, int w, int h, uint32_t color, int thickness);

void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void gfx_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);
void gfx_draw_string_transparent(int x, int y, const char *str, uint32_t fg);

void gfx_blit(int dx, int dy, int w, int h, const uint32_t *src);

#endif
