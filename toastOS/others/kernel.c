/*
 * Copyright (C) 2025 thetoasta
 * This file is licensed under the Mozilla Public License, v. 2.0.
 * You may obtain a copy of the License at https://mozilla.org/MPL/2.0/
 */

#include "drivers/kio.h"
#include "drivers/panic.h"
#include "drivers/multiboot.h"
#include "drivers/time.h"
#include "drivers/fat16.h"
#include "drivers/font_renderer.h"
#include "drivers/JBFontData.h"
#include "drivers/registry.h"
#include "drivers/exec.h"
#include "drivers/editor.h"
#include "drivers/funcs.h"
#include "drivers/mmu.h"
#include "services/setup.h"

/* Global variable for total memory in KB */
unsigned int total_memory_kb = 0;
unsigned int recovery_triggered = 0;

#define MIN_RAM_KB 8096

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
void kmain(unsigned long magic, unsigned long addr)
{

    int memory_info_available = 0;

    // Check for Multiboot magic number
    if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
        unsigned int* mb_info = (unsigned int*)addr;
        unsigned int flags = mb_info[0];
        if (flags & MULTIBOOT_INFO_MEMORY) {
            total_memory_kb = mb_info[1] + mb_info[2] + 1024;
            memory_info_available = 1;
        }
    }

    if (memory_info_available && total_memory_kb < MIN_RAM_KB) {
        l3_panic("MEMORY_ERROR // NOT_ENOUGH_MEM");
    }

    // toastKernel Core
	// Init the services ;)

        init_idt();



    mmu_init();

	init_timer();

    {
        uint32_t start = get_uptime_seconds();

        while ((get_uptime_seconds() - start) < 20) {
            if (inb(0x60) == 0x53) {
                recovery_triggered = 1; // Assigned to the global variable
                break;
            }
        }
    }

    if (recovery_triggered == 1) {
            kprint("something has gone wrong. you've entered recovery. check your regsystem and such.");
            kprint_newline();
            fat16_init();
            registry_init();
            kprint("init recov enviro!");
            kprint_newline();
            kprint("options: 'regreset', 'diskreset', 'regvaloverwrite' ALL OPTIONS DONT HAVE UNDO FEATURES !");
            kprint_newline();
            kprint("> ");
            const char* action = rec_input();
            if (strcmp(action, "regreset") == 0) {
                fat16_delete_file("TOASTREG.TXT");
            } else if (strcmp(action, "diskreset") == 0 ) {
                fat16_format();
            } else if (strcmp(action, "regvaloverwrite") == 0) {
                kprint("too lazy to add sry");
            }
        clear_screen();
        kprint("done.");
        __asm__ volatile ("cli; hlt");
    }

	// shell is dependent on idt. idt is not really dependent on shell. some things like auto clear the screen wont do but it mostly works.
    // not having idt started, its like KIO just is a big file of mess. no keyboard input works.
	// Initialise FreeType font rendering
	//toast_ft_init();
	//toast_ft_load_font(fonts_JBMono_ttf, fonts_JBMono_ttf_len);
	//toast_ft_set_size(12);

	// toast_ft_puts("toastOS Font Test", 10, 30, WHITE);

    fat16_init();

    registry_init();

    exec_init();

    editor_init();

    const char* flag = reg_get("TOASTOS/KERNEL/SETUPSTATUS");
    if (flag == 0 || strcmp(flag, "0") == 0) {
        setupProgram();
    }

    const char* anotherflag = reg_get("TOASTOS/KERNEL/TBOOTCOL");
    if (strcmp(anotherflag, "WK") == 0) {
        init_shell();
    } else {
        toast_shell_color("tbootcol was set to non wk state... ", RED);
        __asm__ volatile ("cli; hlt");
    }

    init_shell();

	while(1);
}