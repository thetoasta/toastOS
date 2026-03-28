/*
 * toastOS Built-in Text Editor
 * A simple nano-style editor with FAT16 read/write support.
 */
#ifndef EDITOR_H
#define EDITOR_H

#include "stdint.h"

#define EDITOR_MAX_LINES    500
#define EDITOR_MAX_LINELEN  160
#define EDITOR_MAX_FILESIZE 16384

/* Call once after all drivers are ready */
void editor_init(void);

/* Open a file by FAT16 filename. Creates a new file if not found. */
void editor_open(const char *filename);

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

#endif /* EDITOR_H */
