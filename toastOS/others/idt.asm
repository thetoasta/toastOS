global idt_load
extern isr0_handler

idt_load:
    mov eax, [esp+4]    ; pointer to idt_ptr
    lidt [eax]
    ret

; ISR0: Divide by zero handler stub
global isr0
isr0:
    cli
    call isr0_handler
    iretd
