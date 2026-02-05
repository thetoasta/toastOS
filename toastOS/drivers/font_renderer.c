/*
 * FreeType Font Renderer for toastOS
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#include "font_renderer.h"
#include "kio.h"
#include "toast_libc.h"

/* ---- internal state ---- */
static FT_Library  ft_lib;
static FT_Face     ft_face;
static int         ft_ready = 0;

/* ---- public API ---- */

int toast_ft_init(void) {
    FT_Error err = FT_Init_FreeType(&ft_lib);
    if (err) {
        kprint("[FreeType] init failed\n");
        return -1;
    }
    ft_ready = 1;
    kprint("[FreeType] library initialised\n");
    return 0;
}

int toast_ft_load_font(const unsigned char *data, unsigned int size) {
    if (!ft_ready) return -1;

    FT_Error err = FT_New_Memory_Face(ft_lib, data, (FT_Long)size, 0, &ft_face);
    if (err) {
        kprint("[FreeType] failed to load font\n");
        return -1;
    }
    kprint("[FreeType] font loaded\n");
    return 0;
}

int toast_ft_set_size(unsigned int pixel_height) {
    if (!ft_face) return -1;
    FT_Error err = FT_Set_Pixel_Sizes(ft_face, 0, pixel_height);
    if (err) {
        kprint("[FreeType] set size failed\n");
        return -1;
    }
    return 0;
}

/*
 * For now the renderer just proves FreeType works by printing a
 * confirmation via kprint.  To actually draw anti-aliased glyphs you
 * would blit ft_face->glyph->bitmap into a linear framebuffer.
 *
 * When you switch to VESA / framebuffer mode you can replace the body
 * of this function with a real pixel blitter.
 */
int toast_ft_putchar(char c, int x, int y, unsigned char color) {
    if (!ft_face) return 0;

    FT_Error err = FT_Load_Char(ft_face, (FT_ULong)c, FT_LOAD_RENDER);
    if (err) return 0;

    /* advance is in 26.6 fixed-point */
    return (int)(ft_face->glyph->advance.x >> 6);
}

void toast_ft_puts(const char *text, int x, int y, unsigned char color) {
    if (!text || !ft_face) return;
    int pen_x = x;
    while (*text) {
        pen_x += toast_ft_putchar(*text, pen_x, y, color);
        text++;
    }
}

void toast_ft_cleanup(void) {
    if (ft_face)  { FT_Done_Face(ft_face);       ft_face = NULL; }
    if (ft_lib)   { FT_Done_FreeType(ft_lib);     ft_lib  = NULL; }
    ft_ready = 0;
}
