/*
 * Font Renderer stub for toastOS (FreeType removed)
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#ifndef FONT_RENDERER_H
#define FONT_RENDERER_H

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

/* ---- Glyph bitmap access (for framebuffer driver) ---- */
typedef struct {
    const unsigned char *buffer; /* grayscale bitmap (8-bit per pixel) */
    int width;                   /* bitmap width in pixels             */
    int height;                  /* bitmap height in pixels            */
    int pitch;                   /* bytes per row in buffer            */
    int bearing_x;               /* left side bearing (pixels)         */
    int bearing_y;               /* top bearing (pixels from baseline) */
    int advance;                 /* horizontal advance (pixels)        */
} toast_glyph_t;

/* Render a single character and fill 'out' with its bitmap info.
   Returns 0 on success, -1 on failure.  The buffer pointer is only
   valid until the next call to this function. */
int toast_ft_render_glyph(char c, toast_glyph_t *out);

#endif /* FONT_RENDERER_H */
