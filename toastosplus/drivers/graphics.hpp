/*
 * toastOS++ Graphics
 * Namespace: toast::gfx
 */

#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include "stdint.hpp"

#define FONT_W 8
#define FONT_H 16

extern const uint8_t font_8x16[95][16];

namespace toast {
namespace gfx {

uint32_t blend(uint32_t fg, uint32_t bg);

void init(uint32_t *framebuffer, uint32_t width, uint32_t height, uint32_t pitch);
bool ready();
uint32_t* backbuffer();
void flush();

void clear(uint32_t color);
void pixel(int x, int y, uint32_t color);
void rect(int x, int y, int w, int h, uint32_t color);
void rect_alpha(int x, int y, int w, int h, uint32_t color);
void outline(int x, int y, int w, int h, uint32_t color, int thickness = 1);

void draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void text(int x, int y, const char *str, uint32_t fg, uint32_t bg);
void text(int x, int y, const char *str, uint32_t fg);  // transparent bg

void blit(int dx, int dy, int w, int h, const uint32_t *src);

} // namespace gfx
} // namespace toast

/* Legacy C-style aliases */
inline uint32_t gfx_blend(uint32_t fg, uint32_t bg) { return toast::gfx::blend(fg, bg); }
inline void gfx_init(uint32_t *fb, uint32_t w, uint32_t h, uint32_t p) { toast::gfx::init(fb, w, h, p); }
inline int gfx_is_ready() { return toast::gfx::ready() ? 1 : 0; }
inline uint32_t* gfx_get_backbuffer() { return toast::gfx::backbuffer(); }
inline void gfx_flush() { toast::gfx::flush(); }
inline void gfx_clear(uint32_t c) { toast::gfx::clear(c); }
inline void gfx_put_pixel(int x, int y, uint32_t c) { toast::gfx::pixel(x, y, c); }
inline void gfx_draw_rect(int x, int y, int w, int h, uint32_t c) { toast::gfx::rect(x, y, w, h, c); }
inline void gfx_draw_rect_alpha(int x, int y, int w, int h, uint32_t c) { toast::gfx::rect_alpha(x, y, w, h, c); }
inline void gfx_draw_outline(int x, int y, int w, int h, uint32_t c, int t) { toast::gfx::outline(x, y, w, h, c, t); }
inline void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) { toast::gfx::draw_char(x, y, c, fg, bg); }
inline void gfx_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg) { toast::gfx::text(x, y, s, fg, bg); }
inline void gfx_draw_string_transparent(int x, int y, const char *s, uint32_t fg) { toast::gfx::text(x, y, s, fg); }
inline void gfx_blit(int dx, int dy, int w, int h, const uint32_t *src) { toast::gfx::blit(dx, dy, w, h, src); }

#endif /* GRAPHICS_HPP */
