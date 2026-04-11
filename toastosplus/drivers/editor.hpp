/*
 * toastOS++ Editor
 * Converted to C++ from toastOS
 */

#ifndef EDITOR_HPP
#define EDITOR_HPP

#ifdef __cplusplus
extern "C" {
#endif

/*
 * toastOS Built-in Text Editor
 * A simple nano-style editor with FAT16 read/write support.
 * Now with rich text formatting for document writing!
 */

#include "stdint.hpp"

#define EDITOR_MAX_LINES    500
#define EDITOR_MAX_LINELEN  160
#define EDITOR_MAX_FILESIZE 16384

/* Rich text formatting modes */
#define EDITOR_MODE_PLAIN  0   /* Plain text editor (code, etc.) */
#define EDITOR_MODE_DOC    1   /* Document mode with rich text rendering */

/* Call once after all drivers are ready */
void editor_init(void);

/* Open a file by FAT16 filename. Creates a new file if not found. */
void editor_open(const char *filename);

/* Open in document mode with rich text formatting */
void editor_open_doc(const char *filename);

/*
 * Feed one raw keyboard event to the editor.
 * Called by keyboard_handler_main when editor_is_active() returns 1.
 *   scancode : PS/2 make-code
 *   ascii    : translated character (0 if non-printable)
 *   shift    : 1 if shift held
 *   ctrl     : 1 if ctrl held
 *   is_e0    : 1 if the scancode was preceded by 0xE0
 */
void editor_handle_key(uint8_t scancode, char ascii, int shift, int ctrl, int is_e0);

/* Returns 1 if the editor currently owns the keyboard */
int editor_is_active(void);

/* Open in IDE mode (adds Ctrl+B build, Ctrl+R run) */
void editor_open_ide(const char *filename);

/* Returns 1 (and clears the flag) if user pressed Ctrl+R to run */
int editor_run_requested(void);

/* Returns the current filename being edited */
const char *editor_get_filename(void);

/*
 * Rich Text Formatting Syntax (for Document Mode):
 *
 *   **text**     Bold text
 *   *text*       Italic text
 *   ~~text~~     Strikethrough
 *   ^text        Center this line (^ at start)
 *   {red}text    Color text (colors: red, green, blue, cyan, yellow, magenta, white, grey)
 *   {/}          End color
 *
 * Toggle modes:
 *   Ctrl+D       Toggle document/plain mode
 *   Ctrl+1       Insert bold markers
 *   Ctrl+2       Insert italic markers
 *   Ctrl+3       Insert strikethrough markers
 *   Ctrl+4       Insert center marker
 */

#ifdef __cplusplus
}
#endif

#endif /* EDITOR_HPP */
