/* toastOS Bootloader Implementation */
#include "bootloader.h"
#include "kio.h"
#include "string.h"

/* MBR (Master Boot Record) - Stage 1 Bootloader
 * This code is loaded by BIOS at 0x7C00 and loads Stage 2
 */
void create_mbr_bootloader(uint8_t* buffer) {
    /* Clear buffer */
    for (int i = 0; i < 512; i++) {
        buffer[i] = 0;
    }
    
    /* MBR Code - loads Stage 2 from sector 1 to 0x7E00 */
    uint8_t mbr_code[] = {
        /* Offset 0x00: Entry point */
        0xFA,                           /* CLI - disable interrupts */
        0x31, 0xC0,                     /* XOR AX, AX */
        0x8E, 0xD0,                     /* MOV SS, AX */
        0x8E, 0xC0,                     /* MOV ES, AX */
        0x8E, 0xD8,                     /* MOV DS, AX */
        0xBC, 0x00, 0x7C,               /* MOV SP, 0x7C00 */
        0xFB,                           /* STI - enable interrupts */
        
        /* Display "L" for loading */
        0xB4, 0x0E,                     /* MOV AH, 0x0E (teletype) */
        0xB0, 0x4C,                     /* MOV AL, 'L' */
        0xCD, 0x10,                     /* INT 0x10 */
        
        /* Load Stage 2 using BIOS INT 13h */
        0xB8, 0x01, 0x02,               /* MOV AX, 0x0201 (read 1 sector) */
        0xBB, 0x00, 0x7E,               /* MOV BX, 0x7E00 (destination) */
        0xB9, 0x02, 0x00,               /* MOV CX, 0x0002 (cylinder 0, sector 2) */
        0xBA, 0x80, 0x00,               /* MOV DX, 0x0080 (head 0, drive 0x80) */
        0xCD, 0x13,                     /* INT 0x13 (disk read) */
        
        /* Check for error */
        0x72, 0x07,                     /* JC error (if carry flag set) */
        
        /* Jump to Stage 2 */
        0xEA, 0x00, 0x7E, 0x00, 0x00,   /* JMP 0x0000:0x7E00 */
        
        /* Error handler */
        0xB0, 0x45,                     /* MOV AL, 'E' */
        0xB4, 0x0E,                     /* MOV AH, 0x0E */
        0xCD, 0x10,                     /* INT 0x10 */
        0xF4,                           /* HLT */
        0xEB, 0xFD,                     /* JMP $ (infinite loop) */
    };
    
    /* Copy MBR code to buffer */
    for (unsigned int i = 0; i < sizeof(mbr_code); i++) {
        buffer[i] = mbr_code[i];
    }
    
    /* Boot signature at offset 510-511 */
    buffer[510] = 0x55;
    buffer[511] = 0xAA;
}

/* Stage 2 Bootloader - Switches to Protected Mode and loads kernel
 * This is loaded at 0x7E00 by MBR
 */
void create_stage2_bootloader(uint8_t* buffer) {
    /* Clear buffer */
    for (int i = 0; i < 512; i++) {
        buffer[i] = 0;
    }
    
    uint8_t stage2_code[] = {
        /* Offset 0x00: Stage 2 Entry (still in Real Mode) */
        
        /* Display "2" */
        0xB4, 0x0E,                     /* MOV AH, 0x0E */
        0xB0, 0x32,                     /* MOV AL, '2' */
        0xCD, 0x10,                     /* INT 0x10 */
        
        /* Enable A20 line (Fast A20 method) */
        0xE4, 0x92,                     /* IN AL, 0x92 */
        0x0C, 0x02,                     /* OR AL, 2 */
        0xE6, 0x92,                     /* OUT 0x92, AL */
        
        /* Load kernel using BIOS (load 128 sectors = 64KB to 0x10000) */
        /* We'll load in chunks because BIOS has limits */
        
        /* Load first chunk: 64 sectors to 0x1000:0x0000 */
        0xB8, 0x00, 0x10,               /* MOV AX, 0x1000 (segment) */
        0x8E, 0xC0,                     /* MOV ES, AX */
        0x31, 0xDB,                     /* XOR BX, BX (offset 0) */
        
        0xB8, 0x40, 0x02,               /* MOV AX, 0x0240 (read 64 sectors) */
        0xB9, 0x41, 0x00,               /* MOV CX, 0x0041 (cylinder 0, sector 65) */
        0xBA, 0x80, 0x00,               /* MOV DX, 0x0080 (head 0, drive 0x80) */
        0xCD, 0x13,                     /* INT 0x13 */
        0x72, 0x50,                     /* JC error */
        
        /* Load second chunk: 64 sectors to 0x1800:0x0000 */
        0xB8, 0x00, 0x18,               /* MOV AX, 0x1800 (segment) */
        0x8E, 0xC0,                     /* MOV ES, AX */
        0x31, 0xDB,                     /* XOR BX, BX */
        
        0xB8, 0x40, 0x02,               /* MOV AX, 0x0240 (read 64 sectors) */
        0xB9, 0x01, 0x01,               /* MOV CX, 0x0101 (cylinder 0, sector 129) */
        0xBA, 0x80, 0x00,               /* MOV DX, 0x0080 */
        0xCD, 0x13,                     /* INT 0x13 */
        0x72, 0x3D,                     /* JC error */
        
        /* Display "P" for protected mode */
        0xB4, 0x0E,                     /* MOV AH, 0x0E */
        0xB0, 0x50,                     /* MOV AL, 'P' */
        0xCD, 0x10,                     /* INT 0x10 */
        
        /* Disable interrupts before switching */
        0xFA,                           /* CLI */
        
        /* Load GDT */
        0x0F, 0x01, 0x16, 0x80, 0x7E,   /* LGDT [0x7E80] */
        
        /* Switch to Protected Mode */
        0x0F, 0x20, 0xC0,               /* MOV EAX, CR0 */
        0x0C, 0x01,                     /* OR AL, 1 */
        0x0F, 0x22, 0xC0,               /* MOV CR0, EAX */
        
        /* Far jump to flush pipeline and enter 32-bit code */
        0xEA, 0x00, 0x00, 0x10, 0x00,   /* JMP 0x0008:0x100000 (kernel entry) */
        0x08, 0x00,
        
        /* Error handler (Real mode) */
        /* offset ~0x58 */
        0xB0, 0x45,                     /* MOV AL, 'E' */
        0xB4, 0x0E,                     /* MOV AH, 0x0E */
        0xCD, 0x10,                     /* INT 0x10 */
        0xF4,                           /* HLT */
        0xEB, 0xFD,                     /* JMP $ */
        
        /* Padding to reach GDT location at 0x7E80 (offset 0x80 in this sector) */
    };
    
    /* Copy Stage 2 code */
    for (unsigned int i = 0; i < sizeof(stage2_code); i++) {
        buffer[i] = stage2_code[i];
    }
    
    /* GDT at offset 0x80 (will be at 0x7E80 when loaded) */
    /* GDT Descriptor (6 bytes) */
    buffer[0x80] = 0x17;                /* Limit (23 bytes = 3 entries * 8 - 1) */
    buffer[0x81] = 0x00;
    buffer[0x82] = 0x86;                /* Base low */
    buffer[0x83] = 0x7E;                /* Base mid */
    buffer[0x84] = 0x00;                /* Base high */
    buffer[0x85] = 0x00;
    
    /* GDT Entries start at 0x86 */
    /* Entry 0: Null Descriptor */
    for (int i = 0; i < 8; i++) {
        buffer[0x86 + i] = 0x00;
    }
    
    /* Entry 1: Code Segment (0x08) */
    buffer[0x8E] = 0xFF;                /* Limit low */
    buffer[0x8F] = 0xFF;                /* Limit mid */
    buffer[0x90] = 0x00;                /* Base low */
    buffer[0x91] = 0x00;                /* Base mid */
    buffer[0x92] = 0x00;                /* Base high */
    buffer[0x93] = 0x9A;                /* Access: Present, Ring 0, Code, Readable */
    buffer[0x94] = 0xCF;                /* Flags + Limit high: 4KB granularity, 32-bit */
    buffer[0x95] = 0x00;                /* Base highest */
    
    /* Entry 2: Data Segment (0x10) */
    buffer[0x96] = 0xFF;                /* Limit low */
    buffer[0x97] = 0xFF;                /* Limit mid */
    buffer[0x98] = 0x00;                /* Base low */
    buffer[0x99] = 0x00;                /* Base mid */
    buffer[0x9A] = 0x00;                /* Base high */
    buffer[0x9B] = 0x92;                /* Access: Present, Ring 0, Data, Writable */
    buffer[0x9C] = 0xCF;                /* Flags + Limit high */
    buffer[0x9D] = 0x00;                /* Base highest */
}

/* Calculate simple checksum */
uint32_t calculate_checksum(const uint8_t* data, uint32_t size) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < size; i++) {
        sum += data[i];
    }
    return sum;
}
