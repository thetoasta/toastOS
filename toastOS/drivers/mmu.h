#ifndef TOAST_MMU_H
#define TOAST_MMU_H

#include "stdint.h"

/* heap lives at 0x400000 - 0x600000 (2mb) */
#define HEAP_START 0x400000
#define HEAP_SIZE  0x200000

void mmu_init(void);
void *kmalloc(uint32_t size);
void *kmalloc_aligned(uint32_t size, uint32_t alignment);
void kfree(void *ptr);
void *krealloc(void *ptr, uint32_t new_size);

/* stats */
uint32_t mmu_used(void);
uint32_t mmu_free(void);

#endif
