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
#include "drivers/string.h"
#include "drivers/user.h"
#include "drivers/paging.h"
#include "drivers/thread.h"
#include "drivers/syscall.h"
#include "drivers/posix.h"

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

    /* Paging: identity-map first 8MB and enable virtual memory */
    paging_init(total_memory_kb > 0 ? total_memory_kb : 8192);

    /* POSIX file descriptor table (stdin/stdout/stderr) */
    posix_init();

    /* Syscall interface (INT 0x80) */
    syscall_init();

    /* Threading / scheduler */
    thread_init();

	init_timer();

    {
        uint32_t start = get_uptime_seconds();

        while ((get_uptime_seconds() - start) < 20) {
            if (inb(0x60) == 0x53 || inb(0x60) == 0x0E) {
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

    const char* password = reg_get("TOASTOS/SECURITY/PASSWORD");
    if (password) {
        bool active = true;
        int tries = 0;
        while (active) {
            clear_screen();
            kprint("password set. please enter it to continue.");
            kprint_newline();
            kprint("enter password to sign into ");
            const char* name = reg_get("TOASTOS/KERNEL/NAME");
            if (name) {
                kprint(name);
            } else {
                kprint("toastOS");
            }
            kprint(" > ");
            char* input = rec_input();
            if (check_password(input)) {
                active = false;
            } else {
                tries++;
                if (tries >= 3) {
                    kprint("sorry, too many attempts. you must reboot manually.");
                    __asm__ volatile ("cli; hlt");
                }
                kprint("that's incorrect. try again.");
            }
        }
    }

    const char* xlx_flag = reg_get("TOASTOS/KERNEL/NAME");

    const char* flag = reg_get("TOASTOS/KERNEL/SETUPSTATUS");
    if (flag == 0 || strcmp(flag, "0") == 0) {
        if (xlx_flag) {
            clear_screen();
            reg_set("TOASTOS/KERNEL/SETUPSTATUS", "1");
            reg_save();
            clear_screen();
            kprint("hello, your name is already set, so we're automatically skipping setup.");
            kprint_newline();
            kprint("it was probably manually reset to 0 via either the file itself or a command.");
            kprint_newline();
            kprint("if you really would like to run setup, open the file in the editor and delete everything.");
            kprint_newline();
            kprint("click enter to continue to os... ");
            rec_input();
        
        } else {
            setupProgram();
        }
    }

    const char* anotherflag = reg_get("TOASTOS/KERNEL/TBOOTCOL");
    if (anotherflag && strcmp(anotherflag, "WK") == 0) {
        init_shell();
    } else {
        toast_shell_color("tbootcol was set to non wk state... ", RED);
        __asm__ volatile ("cli; hlt");
    }

	while(1);
}