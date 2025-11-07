bits 32
section .text
        ;multiboot spec
        align 4
        dd 0x1BADB002              ;magic
        dd 0x00                    ;flags
        dd - (0x1BADB002 + 0x00)   ;checksum. m+f+c should be zero

global start
global keyboard_handler
global read_port
global write_port
global load_idt
global isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
global isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

extern kmain 		;this is defined in the c file
extern keyboard_handler_main
extern isr_handler

; ISR common stub
isr_common_stub:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    call isr_handler
    
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    sti
    iret

; CPU exception ISRs (0-31)
isr0:
    cli
    push byte 0
    push byte 0
    jmp isr_common_stub

isr1:
    cli
    push byte 0
    push byte 1
    jmp isr_common_stub

isr2:
    cli
    push byte 0
    push byte 2
    jmp isr_common_stub

isr3:
    cli
    push byte 0
    push byte 3
    jmp isr_common_stub

isr4:
    cli
    push byte 0
    push byte 4
    jmp isr_common_stub

isr5:
    cli
    push byte 0
    push byte 5
    jmp isr_common_stub

isr6:
    cli
    push byte 0
    push byte 6
    jmp isr_common_stub

isr7:
    cli
    push byte 0
    push byte 7
    jmp isr_common_stub

isr8:
    cli
    push byte 8
    jmp isr_common_stub

isr9:
    cli
    push byte 0
    push byte 9
    jmp isr_common_stub

isr10:
    cli
    push byte 10
    jmp isr_common_stub

isr11:
    cli
    push byte 11
    jmp isr_common_stub

isr12:
    cli
    push byte 12
    jmp isr_common_stub

isr13:
    cli
    push byte 13
    jmp isr_common_stub

isr14:
    cli
    push byte 14
    jmp isr_common_stub

isr15:
    cli
    push byte 0
    push byte 15
    jmp isr_common_stub

isr16:
    cli
    push byte 0
    push byte 16
    jmp isr_common_stub

isr17:
    cli
    push byte 17
    jmp isr_common_stub

isr18:
    cli
    push byte 0
    push byte 18
    jmp isr_common_stub

isr19:
    cli
    push byte 0
    push byte 19
    jmp isr_common_stub

isr20:
    cli
    push byte 0
    push byte 20
    jmp isr_common_stub

isr21:
    cli
    push byte 21
    jmp isr_common_stub

isr22:
    cli
    push byte 0
    push byte 22
    jmp isr_common_stub

isr23:
    cli
    push byte 0
    push byte 23
    jmp isr_common_stub

isr24:
    cli
    push byte 0
    push byte 24
    jmp isr_common_stub

isr25:
    cli
    push byte 0
    push byte 25
    jmp isr_common_stub

isr26:
    cli
    push byte 0
    push byte 26
    jmp isr_common_stub

isr27:
    cli
    push byte 0
    push byte 27
    jmp isr_common_stub

isr28:
    cli
    push byte 0
    push byte 28
    jmp isr_common_stub

isr29:
    cli
    push byte 29
    jmp isr_common_stub

isr30:
    cli
    push byte 30
    jmp isr_common_stub

isr31:
    cli
    push byte 0
    push byte 31
    jmp isr_common_stub

read_port:
    mov edx, [esp + 4]
            ;al is the lower 8 bits of eax
    in al, dx	;dx is the lower 16 bits of edx
    ret

write_port:
    mov   edx, [esp + 4]
    mov   al, [esp + 4 + 4]
    out   dx, al
    ret

load_idt:
    mov edx, [esp + 4]
    lidt [edx]
    sti 				;turn on interrupts
    ret

keyboard_handler:
    call    keyboard_handler_main
    iretd

start:
    cli 				;block interrupts
    mov esp, stack_space
    call kmain
    hlt 				;halt the CPU

section .bss
resb 8192; 8KB for stack
stack_space: