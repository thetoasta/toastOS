/*
 * app_link.ld — toastOS App Linker Script
 *
 * Apps are statically linked at TAPP_LOAD_BASE (0x600000).
 * The .tapp_meta section must be present for the kernel loader to
 * identify and authorise the binary.
 *
 * Usage:
 *   i686-elf-ld -m elf_i386 -T app_link.ld -o myapp.elf myapp.o tapp_start.o
 */

ENTRY(__tapp_start)

TAPP_LOAD_BASE = 0x600000;

SECTIONS
{
    . = TAPP_LOAD_BASE;

    /* Executable code */
    .text : {
        *(.text)
        *(.text.*)
    }

    /* Read-only data (string literals, const globals) */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* App identity metadata — MUST be present */
    .tapp_meta : {
        KEEP(*(.tapp_meta))
    }

    /* Initialised data */
    .data : {
        *(.data)
        *(.data.*)
    }

    /* Zero-initialised data (kernel zeroes the entire app window on load) */
    .bss : {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
}
