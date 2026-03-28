/*
 * Framebuffer driver for toastOS
 * Copyright (C) 2025 thetoasta
 * Licensed under MPL-2.0
 *
 * Provides pixel-level drawing and an 8×16 bitmap-font text console
 * on top of a linear 32-bit framebuffer supplied by GRUB / multiboot.
 */

#include "fb.h"
#include "font_renderer.h"

/* ------------------------------------------------------------------ */
/*  Global framebuffer descriptor                                      */
/* ------------------------------------------------------------------ */
fb_info_t fb = { 0, 0, 0, 0, 0 };

static int fb_ready = 0;   /* 1 once fb_init succeeds */

/* ------------------------------------------------------------------ */
/*  JBMono glyph cache  (printable ASCII 0x20 – 0x7E = 95 glyphs)     */
/*  Pre-rasterised at boot by FreeType, then used for fast blitting.   */
/* ------------------------------------------------------------------ */
#define GLYPH_MAX_W 16
#define GLYPH_MAX_H 24
#define GLYPH_COUNT 95  /* 0x20 .. 0x7E */

typedef struct {
    uint8_t bitmap[GLYPH_MAX_H][GLYPH_MAX_W]; /* alpha (0–255) */
    int width;
    int height;
    int bearing_x;
    int bearing_y;
    int advance;
} cached_glyph_t;

static cached_glyph_t glyph_cache[GLYPH_COUNT];
static int font_cached = 0;

/* Public cell dimensions – set by fb_init_font() */
int fb_char_w = 8;   /* sensible defaults until font is loaded */
int fb_char_h = 16;

/* ------------------------------------------------------------------ */
/*  VGA 4-bit palette → 32-bit ARGB look-up                           */
/* ------------------------------------------------------------------ */
static const uint32_t vga_palette[16] = {
    0x00000000, /* 0  BLACK        */
    0x000000AA, /* 1  BLUE         */
    0x0000AA00, /* 2  GREEN        */
    0x0000AAAA, /* 3  CYAN         */
    0x00AA0000, /* 4  RED          */
    0x00AA00AA, /* 5  MAGENTA      */
    0x00AA5500, /* 6  BROWN        */
    0x00AAAAAA, /* 7  LIGHT_GREY   */
    0x00555555, /* 8  DARK_GREY    */
    0x005555FF, /* 9  LIGHT_BLUE   */
    0x0055FF55, /* A  LIGHT_GREEN  */
    0x0055FFFF, /* B  LIGHT_CYAN   */
    0x00FF5555, /* C  LIGHT_RED    */
    0x00FF55FF, /* D  LIGHT_MAGENTA*/
    0x00FFFF55, /* E  YELLOW       */
    0x00FFFFFF, /* F  WHITE        */
};

uint32_t fb_vga_to_rgb(uint8_t vga_color) {
    return vga_palette[vga_color & 0x0F];
}

/* ------------------------------------------------------------------ */
/*  Init / query                                                       */
/* ------------------------------------------------------------------ */
void fb_init(uint32_t *addr, uint32_t pitch, uint32_t w, uint32_t h, uint8_t bpp) {
    fb.addr   = addr;
    fb.pitch  = pitch;
    fb.width  = w;
    fb.height = h;
    fb.bpp    = bpp;
    fb_ready  = (addr != 0 && w > 0 && h > 0);
}

int fb_available(void) {
    return fb_ready;
}

/* ------------------------------------------------------------------ */
/*  Pixel / rectangle                                                  */
/* ------------------------------------------------------------------ */
void fb_putpixel(int x, int y, uint32_t color) {
    if (!fb_ready) return;
    if ((unsigned)x >= fb.width || (unsigned)y >= fb.height) return;
    /* pitch is in bytes; each pixel is 4 bytes for 32-bpp */
    uint32_t *row = (uint32_t *)((uint8_t *)fb.addr + y * fb.pitch);
    row[x] = color;
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!fb_ready) return;
    for (int row = y; row < y + h && (unsigned)row < fb.height; row++) {
        uint32_t *line = (uint32_t *)((uint8_t *)fb.addr + row * fb.pitch);
        for (int col = x; col < x + w && (unsigned)col < fb.width; col++) {
            line[col] = color;
        }
    }
}

void fb_clear(uint32_t color) {
    if (!fb_ready) return;
    for (unsigned y = 0; y < fb.height; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)fb.addr + y * fb.pitch);
        for (unsigned x = 0; x < fb.width; x++) {
            row[x] = color;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Font cache initialisation (call after FreeType + JBMono loaded)    */
/* ------------------------------------------------------------------ */
int fb_init_font(void) {
    toast_glyph_t g;
    int max_advance = 0;
    int max_height  = 0;

    for (int i = 0; i < GLYPH_COUNT; i++) {
        char c = (char)(0x20 + i);
        cached_glyph_t *cg = &glyph_cache[i];

        /* Zero out */
        for (int y = 0; y < GLYPH_MAX_H; y++)
            for (int x = 0; x < GLYPH_MAX_W; x++)
                cg->bitmap[y][x] = 0;

        if (toast_ft_render_glyph(c, &g) == 0) {
            cg->width     = g.width;
            cg->height    = g.height;
            cg->bearing_x = g.bearing_x;
            cg->bearing_y = g.bearing_y;
            cg->advance   = g.advance;

            /* Copy the FreeType grayscale bitmap into our cache */
            for (int y = 0; y < g.height && y < GLYPH_MAX_H; y++) {
                for (int x = 0; x < g.width && x < GLYPH_MAX_W; x++) {
                    cg->bitmap[y][x] = g.buffer[y * g.pitch + x];
                }
            }

            if (g.advance > max_advance) max_advance = g.advance;
            if (g.height  > max_height)  max_height  = g.height;
        } else {
            /* Fallback: blank glyph */
            cg->width = 0;
            cg->height = 0;
            cg->bearing_x = 0;
            cg->bearing_y = 0;
            cg->advance = 8;
        }
    }

    /* Set cell dimensions from the font metrics */
    if (max_advance > 0) fb_char_w = max_advance;
    if (max_height  > 0) fb_char_h = max_height + 4; /* add a little line spacing */

    font_cached = 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Text rendering with cached JBMono glyphs                           */
/* ------------------------------------------------------------------ */
static inline uint32_t blend_pixel(uint32_t fg, uint32_t bg, uint8_t alpha) {
    if (alpha == 0)   return bg;
    if (alpha == 255) return fg;
    /* Simple per-channel blend: out = bg + alpha*(fg-bg)/255 */
    uint32_t rb_fg = fg & 0xFF00FF;
    uint32_t g_fg  = fg & 0x00FF00;
    uint32_t rb_bg = bg & 0xFF00FF;
    uint32_t g_bg  = bg & 0x00FF00;
    uint32_t rb = rb_bg + (((rb_fg - rb_bg) * alpha) >> 8) & 0xFF00FF;
    uint32_t gn = g_bg  + (((g_fg  - g_bg)  * alpha) >> 8) & 0x00FF00;
    return rb | gn;
}

void fb_putchar(int col, int row, char c, uint32_t fg, uint32_t bg) {
    if (!fb_ready) return;

    int px = col * fb_char_w;
    int py = row * fb_char_h;

    /* Fill the whole cell with background first */
    fb_fill_rect(px, py, fb_char_w, fb_char_h, bg);

    if (!font_cached) return;

    unsigned char ch = (unsigned char)c;
    if (ch < 0x20 || ch > 0x7E) return; /* just bg for unprintable */

    cached_glyph_t *cg = &glyph_cache[ch - 0x20];
    if (cg->width == 0 || cg->height == 0) return;

    /* Baseline sits near the bottom of the cell */
    int baseline = py + (fb_char_h - 4); /* 4px below baseline reserved */
    int ox = px + cg->bearing_x;
    int oy = baseline - cg->bearing_y;

    for (int gy = 0; gy < cg->height; gy++) {
        int sy = oy + gy;
        if (sy < 0 || (unsigned)sy >= fb.height) continue;
        uint32_t *line = (uint32_t *)((uint8_t *)fb.addr + sy * fb.pitch);
        for (int gx = 0; gx < cg->width; gx++) {
            int sx = ox + gx;
            if (sx < 0 || (unsigned)sx >= fb.width) continue;
            uint8_t alpha = cg->bitmap[gy][gx];
            if (alpha > 0)
                line[sx] = blend_pixel(fg, bg, alpha);
        }
    }
}

void fb_puts(int col, int row, const char *s, uint32_t fg, uint32_t bg) {
    while (*s) {
        if (col * fb_char_w >= (int)fb.width) break;
        fb_putchar(col, row, *s, fg, bg);
        col++;
        s++;
    }
}

/* Scroll text rows [top_row .. bot_row] up by one row; clear bottom */
void fb_scroll_up(int top_row, int bot_row, uint32_t bg) {
    if (!fb_ready) return;
    int src_y = (top_row + 1) * fb_char_h;
    int dst_y = top_row * fb_char_h;
    int rows_px = (bot_row - top_row) * fb_char_h;

    /* Copy scanlines up */
    for (int i = 0; i < rows_px; i++) {
        uint32_t *dst = (uint32_t *)((uint8_t *)fb.addr + (dst_y + i) * fb.pitch);
        uint32_t *src = (uint32_t *)((uint8_t *)fb.addr + (src_y + i) * fb.pitch);
        for (unsigned x = 0; x < fb.width; x++) dst[x] = src[x];
    }

    /* Clear the last text row */
    int clear_y = bot_row * fb_char_h;
    for (int i = 0; i < fb_char_h; i++) {
        uint32_t *line = (uint32_t *)((uint8_t *)fb.addr + (clear_y + i) * fb.pitch);
        for (unsigned x = 0; x < fb.width; x++) line[x] = bg;
    }
}

/* ------------------------------------------------------------------ */
/*  Text grid helpers                                                  */
/* ------------------------------------------------------------------ */
int fb_text_cols(void) {
    if (fb_char_w <= 0) return 80;
    return (int)(fb.width / (unsigned)fb_char_w);
}

int fb_text_rows(void) {
    if (fb_char_h <= 0) return 25;
    return (int)(fb.height / (unsigned)fb_char_h);
}
