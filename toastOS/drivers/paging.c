/* toastOS Paging - Virtual Memory Management */

#include "paging.h"
#include "mmu.h"
#include "kio.h"

/* ---- Physical frame bitmap ---- */
static uint32_t frame_bitmap[MAX_PHYS_FRAMES / 32];  /* 1 bit per frame */
static uint32_t total_frames = 0;

/* ---- Page directory and tables (must be 4KB-aligned) ---- */
static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_table_0[1024]   __attribute__((aligned(4096)));  /* 0x000000 - 0x3FFFFF */
static uint32_t page_table_1[1024]   __attribute__((aligned(4096)));  /* 0x400000 - 0x7FFFFF */

/* ---- Frame bitmap helpers ---- */
static void frame_set(uint32_t frame_idx) {
    frame_bitmap[frame_idx / 32] |= (1U << (frame_idx % 32));
}

static void frame_clear(uint32_t frame_idx) {
    frame_bitmap[frame_idx / 32] &= ~(1U << (frame_idx % 32));
}

static int frame_test(uint32_t frame_idx) {
    return (frame_bitmap[frame_idx / 32] >> (frame_idx % 32)) & 1;
}

/* Allocate a free physical frame */
uint32_t frame_alloc(void) {
    for (uint32_t i = 0; i < total_frames; i++) {
        if (!frame_test(i)) {
            frame_set(i);
            return i * PAGE_SIZE;
        }
    }
    return 0; /* out of frames */
}

/* Free a physical frame */
void frame_free(uint32_t phys_addr) {
    uint32_t idx = phys_addr / PAGE_SIZE;
    if (idx < total_frames) {
        frame_clear(idx);
    }
}

/* Flush a single TLB entry */
void paging_flush_tlb(uint32_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/* Get CR3 */
uint32_t paging_get_cr3(void) {
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

/* Map a virtual address to a physical address */
void paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    /* Check if page table exists for this directory entry */
    if (!(page_directory[pd_idx] & PG_PRESENT)) {
        /* We only support the two statically allocated page tables for now */
        return;
    }

    /* Get the page table */
    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & 0xFFFFF000);
    pt[pt_idx] = (phys & 0xFFFFF000) | (flags & 0xFFF) | PG_PRESENT;
    paging_flush_tlb(virt);
}

/* Unmap a virtual address */
void paging_unmap(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    if (!(page_directory[pd_idx] & PG_PRESENT))
        return;

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & 0xFFFFF000);
    pt[pt_idx] = 0;
    paging_flush_tlb(virt);
}

/* Get physical address for a virtual address */
uint32_t paging_get_phys(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    if (!(page_directory[pd_idx] & PG_PRESENT))
        return 0;

    uint32_t *pt = (uint32_t *)(page_directory[pd_idx] & 0xFFFFF000);
    if (!(pt[pt_idx] & PG_PRESENT))
        return 0;

    return (pt[pt_idx] & 0xFFFFF000) | (virt & 0xFFF);
}

/* ---- Initialise paging ---- */
void paging_init(uint32_t total_mem_kb) {
    /*
     * Identity-map the first 8MB so all existing kernel code/data
     * continues to work at the same addresses after paging is enabled.
     *
     * Memory layout:
     *   0x000000 - 0x0FFFFF : BIOS, VGA (0xB8000), low memory
     *   0x100000 - 0x3FFFFF : Kernel code/data/bss/stack
     *   0x400000 - 0x5FFFFF : Kernel heap (mmu.c)
     *   0x600000 - 0x7FFFFF : Reserved / future use
     */

    total_frames = (total_mem_kb * 1024) / PAGE_SIZE;
    if (total_frames > MAX_PHYS_FRAMES)
        total_frames = MAX_PHYS_FRAMES;

    /* Clear frame bitmap */
    for (uint32_t i = 0; i < MAX_PHYS_FRAMES / 32; i++)
        frame_bitmap[i] = 0;

    /* Mark frames 0 - 0x7FFFFF (first 8MB) as used by kernel */
    for (uint32_t i = 0; i < 2048 && i < total_frames; i++)
        frame_set(i);

    /* Clear page directory */
    for (int i = 0; i < 1024; i++)
        page_directory[i] = 0;

    /* Fill page table 0: identity-map 0x000000 - 0x3FFFFF */
    for (int i = 0; i < 1024; i++)
        page_table_0[i] = (i * PAGE_SIZE) | PG_PRESENT | PG_WRITE;

    /* Fill page table 1: identity-map 0x400000 - 0x7FFFFF */
    for (int i = 0; i < 1024; i++)
        page_table_1[i] = ((1024 + i) * PAGE_SIZE) | PG_PRESENT | PG_WRITE;

    /* Install page tables into page directory */
    page_directory[0] = ((uint32_t)page_table_0) | PG_PRESENT | PG_WRITE;
    page_directory[1] = ((uint32_t)page_table_1) | PG_PRESENT | PG_WRITE;

    /* Load page directory into CR3 and enable paging in CR0 */
    __asm__ volatile(
        "mov %0, %%cr3\n\t"
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"
        "mov %%eax, %%cr0\n\t"
        :
        : "r"((uint32_t)page_directory)
        : "eax", "memory"
    );
}
