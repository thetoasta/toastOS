/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * You may obtain a copy of the License at https://mozilla.org/MPL/2.0/
 */

#include "drivers/kio.h"
#include "drivers/panic.h"
#include "drivers/multiboot.h"
#include "drivers/fat16.h"
#include "drivers/font_renderer.h"

/* Global variable for total memory in KB */
unsigned int total_memory_kb = 0;

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
void kmain(unsigned long magic, unsigned long addr)
{
    // Check for Multiboot magic number
    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        unsigned int* mb_info = (unsigned int*)addr;
        unsigned int flags = mb_info[0];
        if (flags & MULTIBOOT_INFO_MEMORY) {
            total_memory_kb = mb_info[1] + mb_info[2] + 1024;
        }
    }

    // toastKernel Core
	// Init the services ;)

	fat16_init();
	init_idt();

	// Initialise FreeType font rendering
	toast_ft_init();

	init_shell();

	while(1);
}