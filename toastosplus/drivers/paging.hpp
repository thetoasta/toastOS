/*
 * toastOS++ Paging
 * Converted to C++ from toastOS
 */

#ifndef PAGING_HPP
#define PAGING_HPP

#ifdef __cplusplus
extern "C" {
#endif

/* toastOS Paging - Virtual Memory Management */

#include "stdint.hpp"

/* Page size is 4KB */
#define PAGE_SIZE       4096
#define PAGE_SHIFT      12
#define PAGES_PER_TABLE 1024
#define TABLES_PER_DIR  1024

/* Page directory/table entry flags */
#define PG_PRESENT    0x001
#define PG_WRITE      0x002
#define PG_USER       0x004
#define PG_WRITETHROUGH 0x008
#define PG_CACHE_DIS  0x010
#define PG_ACCESSED   0x020
#define PG_DIRTY      0x040
#define PG_4MB        0x080

/* Page frame allocator - manages physical 4KB frames */
#define MAX_PHYS_FRAMES  2048   /* 8MB / 4KB = 2048 frames */

/* Initialise paging: identity-map first 8MB, enable CR0.PG */
void paging_init(uint32_t total_mem_kb);

/* Allocate a physical 4KB frame, returns physical address or 0 on failure */
uint32_t frame_alloc(void);

/* Free a physical frame */
void frame_free(uint32_t phys_addr);

/* Map a virtual address to a physical address in the current page directory */
void paging_map(uint32_t virt, uint32_t phys, uint32_t flags);

/* Unmap a virtual address */
void paging_unmap(uint32_t virt);

/* Get physical address for a virtual address, returns 0 if not mapped */
uint32_t paging_get_phys(uint32_t virt);

/* Flush a single TLB entry */
void paging_flush_tlb(uint32_t virt);

/* Get the current page directory physical address */
uint32_t paging_get_cr3(void);

#ifdef __cplusplus
}
#endif

#endif /* PAGING_HPP */
