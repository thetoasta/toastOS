/* toastOS ELF32 definitions */
#ifndef ELF_H
#define ELF_H

#include "stdint.h"

/* ---- ELF identification indices ---- */
#define EI_MAG0       0
#define EI_MAG1       1
#define EI_MAG2       2
#define EI_MAG3       3
#define EI_CLASS      4
#define EI_DATA       5
#define EI_VERSION    6
#define EI_OSABI      7
#define EI_NIDENT     16

/* EI_CLASS values */
#define ELFCLASS32    1

/* EI_DATA values */
#define ELFDATA2LSB   1   /* little-endian */

/* e_type values */
#define ET_EXEC       2

/* e_machine values */
#define EM_386        3

/* p_type values */
#define PT_LOAD       1

/* p_flags bitmask */
#define PF_X          0x1   /* Executable */
#define PF_W          0x2   /* Writable */
#define PF_R          0x4   /* Readable */

/* Special section index */
#define SHN_UNDEF     0

/* ---- ELF32 types ---- */
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;

/* ELF32 file header */
typedef struct {
    uint8_t    e_ident[EI_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off  e_phoff;      /* program header offset */
    Elf32_Off  e_shoff;      /* section header offset */
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;      /* number of program headers */
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;      /* number of section headers */
    Elf32_Half e_shstrndx;   /* section name string table index */
} __attribute__((packed)) Elf32_Ehdr;

/* ELF32 program header */
typedef struct {
    Elf32_Word p_type;
    Elf32_Off  p_offset;     /* offset in file */
    Elf32_Addr p_vaddr;      /* virtual address to load at */
    Elf32_Addr p_paddr;      /* physical address (unused) */
    Elf32_Word p_filesz;     /* bytes in file */
    Elf32_Word p_memsz;      /* bytes in memory (>= filesz, gap is zeroed) */
    Elf32_Word p_flags;
    Elf32_Word p_align;
} __attribute__((packed)) Elf32_Phdr;

/* ELF32 section header */
typedef struct {
    Elf32_Word sh_name;      /* index into section string table */
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off  sh_offset;    /* offset in file */
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
} __attribute__((packed)) Elf32_Shdr;

#endif /* ELF_H */
