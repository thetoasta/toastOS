; setjmp / longjmp for i386  —  toastOS
; Copyright (C) 2025 thetoasta
; Licensed under the Mozilla Public License, v. 2.0.

[bits 32]
section .text

; int setjmp(jmp_buf env)
;   env is at [esp+4]
global setjmp
setjmp:
    mov  eax, [esp+4]      ; eax = &env
    mov  [eax+0],  ebx
    mov  [eax+4],  esi
    mov  [eax+8],  edi
    mov  [eax+12], ebp
    lea  ecx, [esp+4]      ; caller's stack pointer (after return addr)
    mov  [eax+16], ecx
    mov  ecx, [esp]         ; return address
    mov  [eax+20], ecx
    xor  eax, eax           ; return 0 on direct call
    ret

; void longjmp(jmp_buf env, int val)
;   env at [esp+4], val at [esp+8]
global longjmp
longjmp:
    mov  edx, [esp+4]      ; edx = &env
    mov  eax, [esp+8]      ; eax = val
    test eax, eax
    jnz  .ok
    inc  eax                ; if val==0, return 1
.ok:
    mov  ebx, [edx+0]
    mov  esi, [edx+4]
    mov  edi, [edx+8]
    mov  ebp, [edx+12]
    mov  esp, [edx+16]
    jmp  [edx+20]           ; jump to saved return address
