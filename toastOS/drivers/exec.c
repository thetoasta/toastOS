/*
 * toastOS External Application Loader (exec.c)
 *
 * Loads ELF32 .tapp binaries from FAT16 and executes them safely:
 *   - Validates ELF header, program headers, and section headers
 *   - Enforces load address range (apps must live at 0x600000-0x7FFFFF)
 *   - Scans executable segments for privileged port-I/O opcodes
 *   - Requires .tapp_meta section with app identity and requested permissions
 *   - Presents a user-facing security prompt for high-privilege requests
 *   - Captures an IDT checksum before execution and verifies it after,
 *     triggering a kernel panic if any IDT entry was modified (hook detection)
 */

#include "exec.h"
#include "elf.h"
#include "panic.h"
#include "fat16.h"
#include "kio.h"
#include "toast_libc.h"
#include "../services/tapplayer.h"

/* ------------------------------------------------------------------ */
/* Configuration                                                        */
/* ------------------------------------------------------------------ */

/*
 * Virtual address range that apps are permitted to occupy.
 * Guaranteed to be within the minimum 8 MB RAM requirement and well
 * above the kernel image (which starts at 0x100000).
 */
#define TAPP_LOAD_BASE   0x600000UL
#define TAPP_LOAD_LIMIT  0x800000UL   /* 2 MB window for app code/data */

/* Maximum ELF file size we will read from FAT16 (256 KB). */
#define TAPP_MAX_SIZE    (256 * 1024)

/* ------------------------------------------------------------------ */
/* Private state                                                        */
/* ------------------------------------------------------------------ */

/* Read buffer for the raw ELF bytes.  Allocated in BSS — no malloc needed. */
static uint8_t g_elf_buf[TAPP_MAX_SIZE];

/* Syscall table handed to every app at entry. Filled by exec_init(). */
static ToastSyscallTable g_syscall_table;

/* ------------------------------------------------------------------ */
/* IDT integrity helpers                                                */
/* ------------------------------------------------------------------ */

/*
 * Read the IDT descriptor register and return an XOR-fold checksum over
 * all IDT bytes.  Used to detect any hook that patches IDT entries while
 * an app is running.
 */
static uint32_t idt_checksum(void) {
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) idtr;

    __asm__ volatile ("sidt %0" : "=m"(idtr));

    uint32_t num_bytes = (uint32_t)(idtr.limit) + 1u;
    const uint8_t *p   = (const uint8_t *)(uint32_t)idtr.base;
    uint32_t csum      = 0;

    for (uint32_t i = 0; i < num_bytes; i++) {
        /* Rotate the accumulator left by 1 bit before XOR-ing to make the
           checksum sensitive to the position of each byte. */
        csum = (csum << 1) | (csum >> 31);
        csum ^= (uint32_t)p[i];
    }
    return csum;
}

/* ------------------------------------------------------------------ */
/* ELF validation                                                       */
/* ------------------------------------------------------------------ */

/*
 * Basic ELF32 header checks.
 * Returns 0 on success, negative on failure.
 */
static int validate_elf_header(const uint8_t *buf, uint32_t size) {
    if (size < sizeof(Elf32_Ehdr))
        return -1;

    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)buf;

    /* Magic */
    if (ehdr->e_ident[EI_MAG0] != 0x7F || ehdr->e_ident[EI_MAG1] != 'E' ||
        ehdr->e_ident[EI_MAG2] != 'L'  || ehdr->e_ident[EI_MAG3] != 'F')
        return -2;

    /* Must be 32-bit, little-endian, i386 executable */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)  return -3;
    if (ehdr->e_ident[EI_DATA]  != ELFDATA2LSB) return -4;
    if (ehdr->e_type            != ET_EXEC)     return -5;
    if (ehdr->e_machine         != EM_386)      return -6;

    /* Sanity cap: reject absurdly large header tables */
    if (ehdr->e_phnum > 32)  return -7;
    if (ehdr->e_shnum > 256) return -8;

    /* Program header table must fit inside the buffer (overflow-safe) */
    if (ehdr->e_phnum > 0) {
        uint32_t ph_end = ehdr->e_phoff + (uint32_t)ehdr->e_phnum * sizeof(Elf32_Phdr);
        if (ph_end < ehdr->e_phoff || ph_end > size)
            return -9;
    }

    /* Section header table must fit inside the buffer (overflow-safe) */
    if (ehdr->e_shnum > 0) {
        uint32_t sh_end = ehdr->e_shoff + (uint32_t)ehdr->e_shnum * sizeof(Elf32_Shdr);
        if (sh_end < ehdr->e_shoff || sh_end > size)
            return -10;
    }

    return 0;
}

/*
 * Validate every PT_LOAD segment for security:
 *   - No integer overflow in (p_offset + p_filesz) or (p_vaddr + p_memsz)
 *   - File data is entirely within the buffer (prevents OOB reads)
 *   - Load address is fully within [TAPP_LOAD_BASE, TAPP_LOAD_LIMIT)
 *   - Entry point lands inside at least one loaded segment
 *
 * Returns 0 on success, negative on violation.
 */
static int validate_segments(const uint8_t *buf, uint32_t size) {
    const Elf32_Ehdr *ehdr  = (const Elf32_Ehdr *)buf;
    const Elf32_Phdr *phdrs = (const Elf32_Phdr *)(buf + ehdr->e_phoff);

    int entry_seen = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;

        uint32_t p_off   = phdrs[i].p_offset;
        uint32_t p_fsz   = phdrs[i].p_filesz;
        uint32_t p_msz   = phdrs[i].p_memsz;
        uint32_t p_vaddr = phdrs[i].p_vaddr;

        /* Integer overflow checks */
        if (p_fsz > 0 && p_off + p_fsz < p_off)     return -1;
        if (p_msz > 0 && p_vaddr + p_msz < p_vaddr) return -2;

        /* memsz must not be less than filesz */
        if (p_msz < p_fsz) return -3;

        /* File data must lie within the ELF buffer */
        if (p_off + p_fsz > size) return -4;

        /* Load VMA: must start at or above TAPP_LOAD_BASE,
           and end before TAPP_LOAD_LIMIT — no overlap with kernel space */
        if (p_vaddr < TAPP_LOAD_BASE)              return -5;
        if (p_vaddr + p_msz > TAPP_LOAD_LIMIT)    return -6;

        /* Track whether entry point falls within a loaded segment */
        if (ehdr->e_entry >= p_vaddr && ehdr->e_entry < p_vaddr + p_msz)
            entry_seen = 1;
    }

    /* Entry point must land inside a loaded segment */
    if (ehdr->e_phnum > 0 && !entry_seen)
        return -7;

    /* Entry point must also be in app space */
    if (ehdr->e_entry < TAPP_LOAD_BASE || ehdr->e_entry >= TAPP_LOAD_LIMIT)
        return -8;

    return 0;
}

/* ------------------------------------------------------------------ */
/* Port I/O opcode scanner                                              */
/* ------------------------------------------------------------------ */

/*
 * Scan executable ELF segments for privileged port-I/O opcodes.
 * Returns 1 if any suspicious opcodes are found, 0 otherwise.
 *
 * Detected instructions:
 *   E4-E7  : IN/OUT with immediate port address
 *   EC-EF  : IN/OUT with DX register as port address
 *   6C-6F  : INSB/INSW/OUTSB/OUTSW string port instructions
 *   0F 30  : WRMSR (write model-specific register)
 *   0F 32  : RDMSR (read model-specific register)
 */
static int scan_portio_opcodes(const uint8_t *buf, uint32_t size) {
    const Elf32_Ehdr *ehdr  = (const Elf32_Ehdr *)buf;
    const Elf32_Phdr *phdrs = (const Elf32_Phdr *)(buf + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;
        if (!(phdrs[i].p_flags & PF_X))  continue;  /* only executable */

        uint32_t off = phdrs[i].p_offset;
        uint32_t fsz = phdrs[i].p_filesz;

        /* Already validated, but guard defensively */
        if (off + fsz > size) continue;

        for (uint32_t j = 0; j < fsz; j++) {
            uint8_t op = buf[off + j];

            /* IN/OUT fixed port (E4,E5,E6,E7) and variable port (EC,ED,EE,EF) */
            if ((op >= 0xE4 && op <= 0xE7) || (op >= 0xEC && op <= 0xEF))
                return 1;

            /* String port I/O: INSB/INSW/OUTSB/OUTSW (6C-6F) */
            if (op >= 0x6C && op <= 0x6F)
                return 1;

            /* 2-byte privileged instructions: WRMSR (0F 30) / RDMSR (0F 32) */
            if (op == 0x0F && j + 1 < fsz) {
                uint8_t nxt = buf[off + j + 1];
                if (nxt == 0x30 || nxt == 0x32)
                    return 1;
            }
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* .tapp_meta section extractor                                         */
/* ------------------------------------------------------------------ */

/*
 * Locate and parse the .tapp_meta ELF section.
 * Returns 0 on success (and fills *out), -1 if not found or invalid.
 */
static int find_tapp_meta(const uint8_t *buf, uint32_t size, TAppMeta *out) {
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)buf;

    if (ehdr->e_shnum == 0)                      return -1;
    if (ehdr->e_shstrndx == SHN_UNDEF)           return -1;
    if (ehdr->e_shstrndx >= ehdr->e_shnum)       return -1;

    const Elf32_Shdr *shdrs    = (const Elf32_Shdr *)(buf + ehdr->e_shoff);
    const Elf32_Shdr *strtab_s = &shdrs[ehdr->e_shstrndx];
    uint32_t str_off           = strtab_s->sh_offset;
    uint32_t str_sz            = strtab_s->sh_size;

    /* String table itself must be within the buffer */
    if (str_off + str_sz < str_off || str_off + str_sz > size)
        return -1;

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        uint32_t name_idx = shdrs[i].sh_name;
        if (name_idx >= str_sz) continue;

        const char *name      = (const char *)(buf + str_off + name_idx);
        uint32_t max_cmp      = str_sz - name_idx;

        /* ".tapp_meta" = 10 chars + NUL = 11 bytes */
        if (max_cmp < 11 || strncmp(name, ".tapp_meta", 11) != 0)
            continue;

        /* Found the section — validate its data */
        uint32_t d_off = shdrs[i].sh_offset;
        uint32_t d_sz  = shdrs[i].sh_size;

        if (d_off + d_sz < d_off || d_off + d_sz > size)
            return -1;

        if (d_sz < sizeof(TAppMeta))
            return -1;

        const TAppMeta *m = (const TAppMeta *)(buf + d_off);

        /* Verify the 4-byte magic tag */
        if (memcmp(m->magic, TAPP_META_MAGIC, 4) != 0)
            return -1;

        memcpy(out, m, sizeof(TAppMeta));

        /* Force null-termination of all strings, regardless of file content */
        out->name[TAPP_NAME_LEN - 1]      = '\0';
        out->developer[TAPP_DEV_LEN - 1]  = '\0';
        out->version[TAPP_VER_LEN - 1]    = '\0';

        return 0;
    }

    return -1;  /* .tapp_meta section not found */
}

/* ------------------------------------------------------------------ */
/* Security prompt                                                      */
/* ------------------------------------------------------------------ */

/*
 * Display a user-facing security prompt based on the permissions the app
 * has declared and whether raw port I/O was detected in its binary.
 *
 * Returns 1 = user allowed execution, 0 = user denied.
 */
static int security_prompt(const TAppMeta *meta, int portio_detected) {
    int needs_low = (meta->permissions & (PERM_PANIC | PERM_DEVICE | PERM_ALL)) != 0;
    int needs_fs  = (meta->permissions & PERM_FS) != 0;

    if (!needs_low && !needs_fs && !portio_detected)
        return 1;  /* Low-risk — no prompt needed */

    kprint_newline();

    if (needs_low || portio_detected) {
        kprint("[toastSecurity] A app is trying to access low-level device features.");
    } else {
        kprint("[toastSecurity] A app is requesting file system access.");
    }

    kprint_newline();
    kprint("  App:       ");
    kprint(meta->name);
    kprint_newline();
    kprint("  Developer: ");
    kprint(meta->developer);
    kprint_newline();
    kprint("  Version:   ");
    kprint(meta->version);
    kprint_newline();

    if (portio_detected && !(meta->permissions & PERM_DEVICE)) {
        kprint("  WARNING: Hardware port I/O detected but PERM_DEVICE not declared.");
        kprint_newline();
    }

    kprint("Allow? (yes/no) > ");
    char *ans = rec_input();

    if (strcmp(ans, "yes") == 0) {
        kprint("[toastSecurity] Access granted.");
        kprint_newline();
        return 1;
    }

    kprint("[toastSecurity] Blocked. App was not launched.");
    kprint_newline();
    return 0;
}

/* ------------------------------------------------------------------ */
/* Segment loader                                                       */
/* ------------------------------------------------------------------ */

/*
 * Copy all PT_LOAD segments into their target VMAs.
 * The entire app window is zeroed first so BSS gaps are zero-initialised.
 * Caller must have already called validate_segments() successfully.
 */
static void load_segments(const uint8_t *buf) {
    const Elf32_Ehdr *ehdr  = (const Elf32_Ehdr *)buf;
    const Elf32_Phdr *phdrs = (const Elf32_Phdr *)(buf + ehdr->e_phoff);

    /* Zero-out the entire app region (handles .bss) */
    memset((void *)TAPP_LOAD_BASE, 0, TAPP_LOAD_LIMIT - TAPP_LOAD_BASE);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD || phdrs[i].p_filesz == 0) continue;

        void       *dst = (void *)(uint32_t)phdrs[i].p_vaddr;
        const void *src = buf + phdrs[i].p_offset;
        memcpy(dst, src, phdrs[i].p_filesz);
        /* The gap between filesz and memsz is already zeroed above */
    }
}

/* ------------------------------------------------------------------ */
/* Permission get helper (exposed via syscall table)                    */
/* ------------------------------------------------------------------ */

static int syscall_get_permissions(void) {
    AppContext *ctx = get_app_context();
    return ctx ? ctx->permissions : 0;
}

/* ------------------------------------------------------------------ */
/* tapplayer panic wrapper with const-correct signature                 */
/* ------------------------------------------------------------------ */

/* tapplayer declares panic(char*, int) — bridge for the const pointer. */
static void syscall_panic(const char *reason, int severe) {
    panic((char *)reason, severe);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void exec_init(void) {
    g_syscall_table.print           = kprint;
    g_syscall_table.rec_input       = rec_input;
    g_syscall_table.exitapp         = exitapp;
    g_syscall_table.app_panic       = syscall_panic;
    g_syscall_table.get_permissions = syscall_get_permissions;
}

int exec_run(const char *filename) {
    /* --- 1. Read ELF from FAT16 --- */
    int bytes = fat16_read_file(filename, (char *)g_elf_buf, TAPP_MAX_SIZE);
    if (bytes <= 0) {
        kprint("[exec] File not found: ");
        kprint(filename);
        kprint_newline();
        return EXEC_ERR_NOTFOUND;
    }
    uint32_t elf_size = (uint32_t)bytes;

    /* --- 2. Validate ELF header --- */
    if (validate_elf_header(g_elf_buf, elf_size) != 0) {
        kprint("[exec] Invalid ELF binary.");
        kprint_newline();
        return EXEC_ERR_ELF;
    }

    /* --- 3. Validate program segments (security: address range, overflow) --- */
    if (validate_segments(g_elf_buf, elf_size) != 0) {
        kprint("[exec] SECURITY: Segment violates permitted memory range.");
        kprint_newline();
        return EXEC_ERR_SEGSEC;
    }

    /* --- 4. Extract app metadata --- */
    TAppMeta meta;
    if (find_tapp_meta(g_elf_buf, elf_size, &meta) != 0) {
        kprint("[exec] Missing or invalid .tapp_meta — binary is not a .tapp.");
        kprint_newline();
        return EXEC_ERR_NOMETA;
    }

    /* --- 5. Scan for privileged port I/O opcodes --- */
    int portio = scan_portio_opcodes(g_elf_buf, elf_size);

    /* If port I/O is present but PERM_DEVICE not declared, add it so   */
    /* the security prompt explicitly warns the user.                    */
    if (portio && !(meta.permissions & PERM_DEVICE))
        meta.permissions |= PERM_DEVICE;

    /* --- 6. Security prompt (may block execution) --- */
    if (!security_prompt(&meta, portio)) {
        return EXEC_ERR_DENIED;
    }

    /* --- 7. Load segments into app memory window --- */
    load_segments(g_elf_buf);

    /* --- 8. Snapshot IDT before handing control to app --- */
    uint32_t idt_before = idt_checksum();

    /* --- 9. Register app context (sets up tapplayer permission checks) --- */
    int app_id = register_app(meta.name, (int)meta.permissions);

    /* --- 10. Execute the app --- */
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)g_elf_buf;
    tapp_entry_fn entry    = (tapp_entry_fn)(uint32_t)ehdr->e_entry;
    entry(&g_syscall_table, app_id);

    /* --- 11. App has returned — verify IDT was not tampered with --- */
    uint32_t idt_after = idt_checksum();
    if (idt_before != idt_after) {
        /* Hard panic: an app modified the IDT (driver-level hook attempt) */
        l3_panic("SEC_VIOLATION: IDT was modified by an app. Possible hook detected.");
    }

    return EXEC_OK;
}
