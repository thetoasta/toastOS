/*
 * toastOS Built-in Text Editor (editor.c)
 *
 * Layout (80x25 VGA):
 *   Row  0        : top bar (maintained by kio)
 *   Rows 1-22     : editable content area
 *   Row 23        : status bar (filename, modified flag, row:col)
 *   Row 24        : keybinding hints / messages
 *
 * Controls:
 *   Arrow keys       : move cursor
 *   Home/End         : start/end of line
 *   PgUp/PgDn        : scroll by visible page
 *   Backspace        : delete char before cursor
 *   Delete           : delete char at cursor
 *   Enter            : insert newline
 *   Tab              : insert 4 spaces
 *   Ctrl+S           : save file
 *   Ctrl+Q           : quit (prompts if unsaved; press twice to force)
 */

#include "editor.h"
#include "fat16.h"
#include "kio.h"
#include "toast_libc.h"
#include "tscript.h"

/* ------------------------------------------------------------------ */
/* VGA helpers (direct hardware access — fixed at 0xb8000)             */
/* ------------------------------------------------------------------ */

static volatile uint8_t *const vga = (volatile uint8_t *)0xb8000;

#define SCREEN_COLS  80
#define SCREEN_ROWS  25

/* Attribute bytes */
#define ATTR_NORMAL   0x07   /* light-grey on black   */
#define ATTR_CURLINE  0x17   /* white on dark-blue    */
#define ATTR_STATUS   0x70   /* black on light-grey   */
#define ATTR_MSG      0x0F   /* bright-white on black */
#define ATTR_HINT     0x08   /* dark-grey on black    */

/* Editor screen regions */
#define EDIT_TOP      1
#define EDIT_BOTTOM   22
#define STATUS_ROW    23
#define MSG_ROW       24
#define EDIT_ROWS     (EDIT_BOTTOM - EDIT_TOP + 1)   /* 22 */

/* ---- Low-level VGA write ---- */
#define LINE_NUM_WIDTH 4   /* 3 digits + 1 space gutter */

static void vga_put(int col, int row, char c, uint8_t attr) {
    int off = (row * SCREEN_COLS + col) * 2;
    vga[off]     = (uint8_t)c;
    vga[off + 1] = attr;
}

static void vga_str(int col, int row, const char *s, uint8_t attr, int width) {
    int i = 0;
    while (s[i] && i < width) { vga_put(col + i, row, s[i], attr); i++; }
    while (i < width)          { vga_put(col + i, row, ' ', attr); i++; }
}

/* ------------------------------------------------------------------ */
/* Editor state                                                         */
/* ------------------------------------------------------------------ */

static char ed_buf[EDITOR_MAX_LINES][EDITOR_MAX_LINELEN];
static int  ed_nlines;
static int  ed_cr;       /* cursor row  (index into ed_buf) */
static int  ed_cc;       /* cursor col  (index into current line) */
static int  ed_scroll;   /* first visible line index */
static int  ed_modified; /* 0=clean, 1=dirty, -1=pending force-quit confirm */
static int  ed_active;
static int  ed_ide_mode; /* 1 = App Engine IDE mode (^B build, ^R run) */
static int  ed_run_req;  /* set to 1 when user presses ^R */
static char ed_filename[16];
static char ed_msg[81];

/* ------------------------------------------------------------------ */
/* Rendering                                                            */
/* ------------------------------------------------------------------ */

extern void update_cursor(int x, int y);   /* defined in kio.c */

static void render_line(int screen_row, int buf_line) {
    uint8_t attr = (buf_line == ed_cr) ? ATTR_CURLINE : ATTR_NORMAL;
    /* Draw line number gutter */
    if (buf_line >= 0 && buf_line < ed_nlines) {
        char num[4];
        int n = buf_line + 1;
        num[0] = (n >= 100) ? ('0' + (n / 100) % 10) : ' ';
        num[1] = (n >= 10)  ? ('0' + (n / 10) % 10)  : ' ';
        num[2] = '0' + (n % 10);
        num[3] = '\0';
        uint8_t gutter_attr = 0x08; /* dark grey */
        for (int g = 0; g < 3; g++)
            vga_put(g, screen_row, num[g], gutter_attr);
        vga_put(3, screen_row, ' ', gutter_attr);
        /* Draw line content */
        const char *line = ed_buf[buf_line];
        int len = (int)strlen(line);
        int i;
        int content_cols = SCREEN_COLS - LINE_NUM_WIDTH;
        for (i = 0; i < content_cols && i < len; i++)
            vga_put(LINE_NUM_WIDTH + i, screen_row, line[i], attr);
        for (; i < content_cols; i++)
            vga_put(LINE_NUM_WIDTH + i, screen_row, ' ', attr);
    } else {
        vga_put(0, screen_row, ' ', ATTR_NORMAL);
        vga_put(1, screen_row, ' ', ATTR_NORMAL);
        vga_put(2, screen_row, ' ', ATTR_NORMAL);
        vga_put(3, screen_row, '~', ATTR_NORMAL);
        for (int i = LINE_NUM_WIDTH; i < SCREEN_COLS; i++)
            vga_put(i, screen_row, ' ', ATTR_NORMAL);
    }
}

/* Tiny int-to-decimal helper (writes into buf, returns length) */
static int itoa_dec(int n, char *buf) {
    if (n <= 0) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    char tmp[12]; int len = 0;
    while (n > 0) { tmp[len++] = '0' + (n % 10); n /= 10; }
    for (int i = 0; i < len; i++) buf[i] = tmp[len - 1 - i];
    buf[len] = '\0';
    return len;
}

static void render_status(void) {
    char bar[81];
    int pos = 0;

    bar[pos++] = ' ';
    const char *fname = ed_filename[0] ? ed_filename : "(new)";
    for (int i = 0; fname[i] && pos < 55; i++) bar[pos++] = fname[i];
    if (ed_modified > 0) {
        const char *m = " [+]";
        for (int i = 0; m[i] && pos < 60; i++) bar[pos++] = m[i];
    }

    /* Right-align row:col */
    char rc[16]; int ri = 0;
    char nb[8];
    itoa_dec(ed_cr + 1, nb);
    for (int i = 0; nb[i]; i++) rc[ri++] = nb[i];
    rc[ri++] = ':';
    itoa_dec(ed_cc + 1, nb);
    for (int i = 0; nb[i]; i++) rc[ri++] = nb[i];
    rc[ri] = '\0';

    while (pos < SCREEN_COLS - ri - 1) bar[pos++] = ' ';
    for (int i = 0; rc[i] && pos < SCREEN_COLS; i++) bar[pos++] = rc[i];
    while (pos < SCREEN_COLS) bar[pos++] = ' ';
    bar[SCREEN_COLS] = '\0';

    vga_str(0, STATUS_ROW, bar, ATTR_STATUS, SCREEN_COLS);
}

static void render_msg(void) {
    if (ed_msg[0])
        vga_str(0, MSG_ROW, ed_msg, ATTR_MSG, SCREEN_COLS);
    else if (ed_ide_mode)
        vga_str(0, MSG_ROW, "  ^S Save  ^B Build  ^R Run  ^Q Quit  | ToastScript IDE",
                ATTR_HINT, SCREEN_COLS);
    else
        vga_str(0, MSG_ROW, "  ^S Save   ^Q Quit   Arrows Navigate   Home/End   PgUp/PgDn",
                ATTR_HINT, SCREEN_COLS);
}

static void render_all(void) {
    for (int sr = EDIT_TOP; sr <= EDIT_BOTTOM; sr++)
        render_line(sr, ed_scroll + (sr - EDIT_TOP));
    render_status();
    render_msg();
    int screen_row = EDIT_TOP + (ed_cr - ed_scroll);
    update_cursor(LINE_NUM_WIDTH + ed_cc, screen_row);
}

static void set_msg(const char *msg) {
    strncpy(ed_msg, msg, 80);
    ed_msg[80] = '\0';
}

/* ------------------------------------------------------------------ */
/* Cursor helpers                                                       */
/* ------------------------------------------------------------------ */

static void clamp_cursor(void) {
    if (ed_nlines == 0) { ed_nlines = 1; ed_buf[0][0] = '\0'; }
    if (ed_cr < 0) ed_cr = 0;
    if (ed_cr >= ed_nlines) ed_cr = ed_nlines - 1;
    int len = (int)strlen(ed_buf[ed_cr]);
    if (ed_cc < 0) ed_cc = 0;
    if (ed_cc > len) ed_cc = len;
    /* Scroll to keep cursor visible */
    if (ed_cr < ed_scroll) ed_scroll = ed_cr;
    if (ed_cr >= ed_scroll + EDIT_ROWS) ed_scroll = ed_cr - EDIT_ROWS + 1;
    if (ed_scroll < 0) ed_scroll = 0;
}

/* ------------------------------------------------------------------ */
/* Buffer mutations                                                     */
/* ------------------------------------------------------------------ */

static void ed_insert_char(char c) {
    char *line = ed_buf[ed_cr];
    int len = (int)strlen(line);
    if (len >= EDITOR_MAX_LINELEN - 1) return;
    /* Shift right */
    for (int i = len; i >= ed_cc; i--) line[i + 1] = line[i];
    line[ed_cc++] = c;
    ed_modified = 1;
}

static void ed_backspace(void) {
    if (ed_cc > 0) {
        char *line = ed_buf[ed_cr];
        int len = (int)strlen(line);
        for (int i = ed_cc - 1; i < len; i++) line[i] = line[i + 1];
        ed_cc--;
        ed_modified = 1;
    } else if (ed_cr > 0) {
        /* Merge with previous line */
        char *prev = ed_buf[ed_cr - 1];
        char *curr = ed_buf[ed_cr];
        int plen = (int)strlen(prev);
        int clen = (int)strlen(curr);
        if (plen + clen < EDITOR_MAX_LINELEN - 1) {
            for (int i = 0; i <= clen; i++) prev[plen + i] = curr[i];
            ed_cc = plen;
            /* Shift lines up */
            for (int i = ed_cr; i < ed_nlines - 1; i++)
                memcpy(ed_buf[i], ed_buf[i + 1], EDITOR_MAX_LINELEN);
            ed_buf[--ed_nlines][0] = '\0';
            ed_cr--;
            ed_modified = 1;
        }
    }
}

static void ed_delete_fwd(void) {
    char *line = ed_buf[ed_cr];
    int len = (int)strlen(line);
    if (ed_cc < len) {
        for (int i = ed_cc; i < len; i++) line[i] = line[i + 1];
        ed_modified = 1;
    } else if (ed_cr < ed_nlines - 1) {
        /* Merge next line into this one */
        char *next = ed_buf[ed_cr + 1];
        int nlen = (int)strlen(next);
        if (len + nlen < EDITOR_MAX_LINELEN - 1) {
            for (int i = 0; i <= nlen; i++) line[len + i] = next[i];
            for (int i = ed_cr + 1; i < ed_nlines - 1; i++)
                memcpy(ed_buf[i], ed_buf[i + 1], EDITOR_MAX_LINELEN);
            ed_buf[--ed_nlines][0] = '\0';
            ed_modified = 1;
        }
    }
}

static void ed_newline(void) {
    if (ed_nlines >= EDITOR_MAX_LINES) return;
    /* Shift all lines after cursor down by one */
    for (int i = ed_nlines; i > ed_cr + 1; i--)
        memcpy(ed_buf[i], ed_buf[i - 1], EDITOR_MAX_LINELEN);
    char *curr = ed_buf[ed_cr];
    char *next = ed_buf[ed_cr + 1];
    int tail = (int)strlen(curr) - ed_cc;
    /* Copy tail to new line */
    for (int i = 0; i <= tail; i++) next[i] = curr[ed_cc + i];
    curr[ed_cc] = '\0';
    ed_nlines++;
    ed_cr++;
    ed_cc = 0;
    ed_modified = 1;
}

/* ------------------------------------------------------------------ */
/* File I/O                                                             */
/* ------------------------------------------------------------------ */

static int ed_load(void) {
    static char fbuf[EDITOR_MAX_FILESIZE];
    int bytes = fat16_read_file(ed_filename, fbuf, EDITOR_MAX_FILESIZE - 1);
    if (bytes < 0) return -1;
    fbuf[bytes] = '\0';

    ed_nlines = 0;
    int li = 0, ci = 0;
    ed_buf[0][0] = '\0';

    for (int i = 0; i < bytes && li < EDITOR_MAX_LINES; i++) {
        char ch = fbuf[i];
        if (ch == '\r') continue;   /* skip CR in CRLF */
        if (ch == '\n') {
            ed_buf[li][ci] = '\0';
            li++;
            ci = 0;
            if (li < EDITOR_MAX_LINES) ed_buf[li][0] = '\0';
        } else if (ci < EDITOR_MAX_LINELEN - 1) {
            ed_buf[li][ci++] = ch;
        }
    }
    ed_buf[li][ci] = '\0';
    ed_nlines = li + 1;
    return 0;
}

static int ed_save(void) {
    static char fbuf[EDITOR_MAX_FILESIZE];
    int pos = 0;
    for (int i = 0; i < ed_nlines && pos < EDITOR_MAX_FILESIZE - 2; i++) {
        int len = (int)strlen(ed_buf[i]);
        if (pos + len + 2 >= EDITOR_MAX_FILESIZE) break;
        for (int j = 0; j < len; j++) fbuf[pos++] = ed_buf[i][j];
        if (i < ed_nlines - 1) fbuf[pos++] = '\n';
    }
    fbuf[pos] = '\0';
    fat16_delete_file(ed_filename);
    return fat16_create_file(ed_filename, fbuf);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void editor_init(void) {
    ed_active   = 0;
    ed_ide_mode = 0;
    ed_run_req  = 0;
    ed_nlines   = 1;
    ed_cr = ed_cc = ed_scroll = 0;
    ed_modified = 0;
    ed_filename[0] = '\0';
    ed_msg[0] = '\0';
    ed_buf[0][0] = '\0';
}

int editor_is_active(void) {
    return ed_active;
}

void editor_open_ide(const char *filename) {
    ed_ide_mode = 1;
    editor_open(filename);
}

int editor_run_requested(void) {
    if (ed_run_req) { ed_run_req = 0; return 1; }
    return 0;
}

const char *editor_get_filename(void) {
    return ed_filename;
}

void editor_open(const char *filename) {
    strncpy(ed_filename, filename, 15);
    ed_filename[15] = '\0';

    ed_cr = ed_cc = ed_scroll = 0;
    ed_modified = 0;
    ed_msg[0] = '\0';

    for (int i = 0; i < EDITOR_MAX_LINES; i++) ed_buf[i][0] = '\0';
    ed_nlines = 1;

    if (ed_load() == 0) {
        set_msg("File loaded. Ctrl+S to save, Ctrl+Q to quit.");
    } else {
        set_msg("New file. Ctrl+S to save, Ctrl+Q to quit.");
    }

    ed_active = 1;
    render_all();
}

void editor_handle_key(uint8_t scancode, char ascii,
                       int shift, int ctrl, int is_e0) {
    (void)shift;   /* used indirectly through ascii */

    /* ---- Ctrl shortcuts ---- */
    if (ctrl) {
        if (scancode == 0x1F) {          /* Ctrl+S */
            if (ed_save() == 0) {
                ed_modified = 0;
                set_msg("Saved.");
            } else {
                set_msg("ERROR: Save failed.");
            }
            render_all();
            return;
        }
        if (scancode == 0x10) {          /* Ctrl+Q */
            if (ed_modified > 0) {
                set_msg("Unsaved changes! Press Ctrl+Q again to force quit.");
                ed_modified = -1;
                render_all();
                return;
            }
            ed_ide_mode = 0;
            goto do_quit;
        }
        /* ---- IDE-only shortcuts ---- */
        if (ed_ide_mode) {
            if (scancode == 0x30) {      /* Ctrl+B  — build (validate) */
                /* Flatten buffer into a string for the validator */
                static char vbuf[EDITOR_MAX_FILESIZE];
                int vpos = 0;
                for (int i = 0; i < ed_nlines && vpos < EDITOR_MAX_FILESIZE - 2; i++) {
                    int len = (int)strlen(ed_buf[i]);
                    for (int j = 0; j < len && vpos < EDITOR_MAX_FILESIZE - 2; j++)
                        vbuf[vpos++] = ed_buf[i][j];
                    if (i < ed_nlines - 1) vbuf[vpos++] = '\n';
                }
                vbuf[vpos] = '\0';
                char errmsg[81];
                errmsg[0] = '\0';
                if (tscript_validate(vbuf, errmsg, sizeof(errmsg)) == 0) {
                    set_msg("Build OK!");
                } else {
                    /* Prefix with "Build Error: " */
                    char full[81] = "Build Error: ";
                    int el = (int)strlen(full);
                    strncpy(full + el, errmsg, 80 - el);
                    full[80] = '\0';
                    set_msg(full);
                }
                render_all();
                return;
            }
            if (scancode == 0x13) {      /* Ctrl+R  — run */
                /* Save first */
                ed_save();
                ed_modified = 0;
                /* Signal kio.c to load the file and run it */
                ed_run_req = 1;
                ed_active  = 0;
                return;
            }
        }
        return;   /* ignore other Ctrl combos */
    }

    /* Reset "confirm quit" sentinel on any non-ctrl key */
    if (ed_modified == -1) ed_modified = 1;

    /* Clear old message */
    ed_msg[0] = '\0';

    /* ---- Extended (E0-prefix) keys ---- */
    if (is_e0) {
        switch (scancode) {
            case 0x48: ed_cr--;               break;  /* Up    */
            case 0x50: ed_cr++;               break;  /* Down  */
            case 0x4B:                                /* Left  */
                if (ed_cc > 0) { ed_cc--; clamp_cursor(); render_all(); return; }
                if (ed_cr > 0) { ed_cr--; ed_cc = (int)strlen(ed_buf[ed_cr]); }
                clamp_cursor(); render_all(); return;
            case 0x4D:                                /* Right */
                { int len = (int)strlen(ed_buf[ed_cr]);
                  if (ed_cc < len) { ed_cc++; clamp_cursor(); render_all(); return; }
                  if (ed_cr < ed_nlines - 1) { ed_cr++; ed_cc = 0; } }
                clamp_cursor(); render_all(); return;
            case 0x47: ed_cc = 0;                          break;  /* Home   */
            case 0x4F: ed_cc = (int)strlen(ed_buf[ed_cr]); break;  /* End    */
            case 0x49: ed_cr -= EDIT_ROWS; break;  /* PgUp  */
            case 0x51: ed_cr += EDIT_ROWS; break;  /* PgDn  */
            case 0x53: ed_delete_fwd(); clamp_cursor(); render_all(); return;
            default:   return;
        }
        clamp_cursor();
        render_all();
        return;
    }

    /* ---- Regular scancodes ---- */
    switch (scancode) {
        case 0x0E:   /* Backspace */
            ed_backspace();
            clamp_cursor();
            render_all();
            return;
        case 0x1C:   /* Enter */
            ed_newline();
            clamp_cursor();
            render_all();
            return;
        case 0x0F:   /* Tab — 4 spaces */
            for (int i = 0; i < 4; i++) ed_insert_char(' ');
            clamp_cursor();
            render_all();
            return;
        default:
            if (ascii >= 0x20 && (unsigned char)ascii < 0x7F) {
                ed_insert_char(ascii);
                clamp_cursor();
                render_all();
            }
            return;
    }

do_quit:
    ed_active   = 0;
    ed_modified = 0;
    /* kio.c redraws the shell prompt after detecting editor exited */
}
