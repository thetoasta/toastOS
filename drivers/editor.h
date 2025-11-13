/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * You may obtain a copy of the License at https://mozilla.org/MPL/2.0/
 */

#ifndef EDITOR_H
#define EDITOR_H

#include "stdint.h"

#define MAX_FILE_SIZE 2048
#define MAX_FILENAME 32

// Start editor with a new file
void editor_new(void);

// Open existing file for editing
void editor_open(const char* filename);

// Editor autosave callback (called by timer)
void editor_autosave_callback(void);

// Check if editor is active
int editor_is_active(void);

#endif // EDITOR_H
