/*
 * Font Renderer stub for toastOS (FreeType removed)
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 */

#include "font_renderer.hpp"

int toast_ft_init(void)                                                    { return 0; }
int toast_ft_load_font(const unsigned char *data, unsigned int size)       { return 0; }
int toast_ft_set_size(unsigned int pixel_height)                           { return 0; }
int toast_ft_putchar(char c, int x, int y, unsigned char color)            { return 0; }
void toast_ft_puts(const char *text, int x, int y, unsigned char color)    {}
void toast_ft_cleanup(void)                                                {}
int toast_ft_render_glyph(char c, toast_glyph_t *out)                     { return -1; }
