/*
 * Framebuffer driver for toastOS
 * Copyright (C) 2025 thetoasta
 * Licensed under MPL-2.0
 */

#ifndef FB_H
#define FB_H

#include "stdint.h"

/* ---- Framebuffer info (filled by kernel at boot from multiboot) ---- */
typedef struct {
    uint32_t *addr;       /* pointer to pixel memory                 */
    uint32_t  pitch;      /* bytes per scanline                      */
    uint32_t  width;      /* pixels per row                          */
    uint32_t  height;     /* rows                                    */
    uint8_t   bpp;        /* bits per pixel (expect 32)              */
} fb_info_t;

extern fb_info_t fb;      /* the one global framebuffer descriptor   */

/* ---- Colours (32-bit ARGB) ---- */
#define FB_BLACK        0x00000000
#define FB_WHITE        0x00FFFFFF
#define FB_RED          0x00FF0000
#define FB_GREEN        0x0000FF00
#define FB_BLUE         0x000000FF
#define FB_CYAN         0x0000FFFF
#define FB_MAGENTA      0x00FF00FF
#define FB_YELLOW       0x00FFFF00
#define FB_LIGHT_GREY   0x00C0C0C0
#define FB_DARK_GREY    0x00808080
#define FB_LIGHT_RED    0x00FF6666
#define FB_LIGHT_GREEN  0x0066FF66
#define FB_LIGHT_BLUE   0x006666FF
#define FB_LIGHT_CYAN   0x0066FFFF
#define FB_ORANGE       0x00FF8800
#define FB_BROWN        0x00885500
#define FB_TOASTOS_BLUE 0x000044AA

/* ---- Initialisation ---- */
/* Call once after multiboot has given you the framebuffer pointer.
   If addr is NULL (no framebuffer) it falls back to VGA text mode. */
void fb_init(uint32_t *addr, uint32_t pitch, uint32_t w, uint32_t h, uint8_t bpp);

/* Returns 1 when framebuffer is active, 0 when using VGA text fallback */
int  fb_available(void);

/* ---- Primitive drawing ---- */
void fb_putpixel(int x, int y, uint32_t color);
void fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void fb_clear(uint32_t color);

/* ---- Text (JetBrains Mono via FreeType, pre-cached at boot) ---- */

/* Call AFTER FreeType + JBMono font have been loaded.
   Pre-rasterises printable ASCII into a fast bitmap cache. */
int  fb_init_font(void);

void fb_putchar(int col, int row, char c, uint32_t fg, uint32_t bg);
void fb_puts(int col, int row, const char *s, uint32_t fg, uint32_t bg);
void fb_scroll_up(int top_row, int bot_row, uint32_t bg);

/* ---- Helpers ---- */
/* Map the 4-bit VGA colour index (0x0–0xF) to a 32-bit ARGB colour */
uint32_t fb_vga_to_rgb(uint8_t vga_color);

/* Character cell dimensions (set after fb_init_font) */
extern int fb_char_w;
extern int fb_char_h;

/* Text grid helpers (columns / rows that fit on screen) */
int fb_text_cols(void);
int fb_text_rows(void);

#endif /* FB_H */
