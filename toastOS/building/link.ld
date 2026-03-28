ENTRY(start)

SECTIONS
{
    . = 0x100000;
    .text : {
        KEEP(*(.multiboot))  /* Force the header to the front */
        *(.text)
    }
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }
    .data : {
        *(.data)
    }
    .bss : {
        *(COMMON)
        *(.bss)
    }
}