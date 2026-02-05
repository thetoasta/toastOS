/*
 * FreeType Font Renderer for toastOS
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#ifndef FONT_RENDERER_H
#define FONT_RENDERER_H

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftbitmap.h>

/* Initialise FreeType – call once at boot */
int  toast_ft_init(void);

/* Load a TrueType font from a raw memory buffer (e.g. embedded in kernel) */
int  toast_ft_load_font(const unsigned char *data, unsigned int size);

/* Set pixel height for the currently loaded face */
int  toast_ft_set_size(unsigned int pixel_height);

/* Draw a single character at (x,y) on the VGA text-mode screen.
   Returns the horizontal advance in pixels. */
int  toast_ft_putchar(char c, int x, int y, unsigned char color);

/* Draw a NUL-terminated string starting at (x,y). */
void toast_ft_puts(const char *text, int x, int y, unsigned char color);

/* Shut down FreeType */
void toast_ft_cleanup(void);

#endif /* FONT_RENDERER_H */
