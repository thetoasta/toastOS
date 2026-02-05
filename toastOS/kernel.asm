bits 32

; Toast Boot Manager v1.7

section .multiboot  ; Dedicated section
    align 4
    dd 0x1BADB002   ; Magic
    dd 0x00         ; Flags
    dd - (0x1BADB002 + 0x00) ; Checksum

section .text

	global start
	global keyboard_handler
    global irq0_handler
	global read_port
	global write_port
	global load_idt

	; ISR stubs for CPU exceptions
	global isr0
	global isr1
	global isr2
	global isr3
	global isr4
	global isr5
	global isr6
	global isr7
	global isr8
	global isr9
	global isr10
	global isr11
	global isr12
	global isr13
	global isr14
	global isr15
	global isr16
	global isr17
	global isr18
	global isr19

	extern kmain 		;this is defined in the c file
	extern keyboard_handler_main
    extern timer_handler
	extern isr_handler  ; Common C handler for exceptions

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

    irq0_handler:
        pusha
        call timer_handler
        popa
        iretd

	keyboard_handler:
		call    keyboard_handler_main
		iretd

; Common ISR stub - saves all registers and calls C handler
isr_common_stub:
    pusha                ; Push all general-purpose registers
    mov ax, ds
    push eax             ; Save data segment

    mov ax, 0x10         ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp             ; Push pointer to stack (registers_t*)
    call isr_handler
    add esp, 4           ; Clean up pushed argument

    pop eax              ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                 ; Restore all general-purpose registers
    add esp, 8           ; Clean up error code and ISR number
    iretd                ; Return from interrupt

; ISR stubs - Exceptions without error code push a dummy error code (0)
; ISR 0: Divide By Zero
isr0:
    cli
    push dword 0         ; Dummy error code
    push dword 0         ; Interrupt number
    jmp isr_common_stub

; ISR 1: Debug
isr1:
    cli
    push dword 0
    push dword 1
    jmp isr_common_stub

; ISR 2: Non-Maskable Interrupt
isr2:
    cli
    push dword 0
    push dword 2
    jmp isr_common_stub

; ISR 3: Breakpoint
isr3:
    cli
    push dword 0
    push dword 3
    jmp isr_common_stub

; ISR 4: Overflow
isr4:
    cli
    push dword 0
    push dword 4
    jmp isr_common_stub

; ISR 5: Bound Range Exceeded
isr5:
    cli
    push dword 0
    push dword 5
    jmp isr_common_stub

; ISR 6: Invalid Opcode
isr6:
    cli
    push dword 0
    push dword 6
    jmp isr_common_stub

; ISR 7: Device Not Available
isr7:
    cli
    push dword 0
    push dword 7
    jmp isr_common_stub

; ISR 8: Double Fault (has error code)
isr8:
    cli
    ; Error code is already pushed by CPU
    push dword 8
    jmp isr_common_stub

; ISR 9: Coprocessor Segment Overrun (legacy)
isr9:
    cli
    push dword 0
    push dword 9
    jmp isr_common_stub

; ISR 10: Invalid TSS (has error code)
isr10:
    cli
    push dword 10
    jmp isr_common_stub

; ISR 11: Segment Not Present (has error code)
isr11:
    cli
    push dword 11
    jmp isr_common_stub

; ISR 12: Stack-Segment Fault (has error code)
isr12:
    cli
    push dword 12
    jmp isr_common_stub

; ISR 13: General Protection Fault (has error code)
isr13:
    cli
    push dword 13
    jmp isr_common_stub

; ISR 14: Page Fault (has error code)
isr14:
    cli
    push dword 14
    jmp isr_common_stub

; ISR 15: Reserved
isr15:
    cli
    push dword 0
    push dword 15
    jmp isr_common_stub

; ISR 16: x87 Floating-Point Exception
isr16:
    cli
    push dword 0
    push dword 16
    jmp isr_common_stub

; ISR 17: Alignment Check (has error code)
isr17:
    cli
    push dword 17
    jmp isr_common_stub

; ISR 18: Machine Check
isr18:
    cli
    push dword 0
    push dword 18
    jmp isr_common_stub

; ISR 19: SIMD Floating-Point Exception
isr19:
    cli
    push dword 0
    push dword 19
    jmp isr_common_stub

	start:
		cli 				;block interrupts
		mov esp, stack_space
		
		; Multiboot info is passed on stack
		; ESP+4: magic number
		; ESP+8: multiboot info address
		push ebx  ; multiboot info
		push eax  ; magic number
		
		call kmain
		
		; Clean up stack (kmain doesn't return)
		add esp, 8
		
		hlt 				;halt the CPU

	section .bss
	resb 8192; 8KB for stack
	stack_space: