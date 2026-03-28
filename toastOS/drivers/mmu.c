/* toastMMU - simple free-list memory manager */

#include "mmu.h"
#include "kio.h"

typedef struct block {
    uint32_t size;       /* usable bytes in this block */
    uint8_t  free;
    struct block *next;
} block_t;

#define BLOCK_HDR sizeof(block_t)

static block_t *head = (block_t *)0;

void mmu_init(void) {
    head = (block_t *)HEAP_START;
    head->size = HEAP_SIZE - BLOCK_HDR;
    head->free = 1;
    head->next = (block_t *)0;
}

/* find first block that fits */
static block_t *find_free(uint32_t size) {
    block_t *cur = head;
    while (cur) {
        if (cur->free && cur->size >= size)
            return cur;
        cur = cur->next;
    }
    return (block_t *)0;
}

/* split a block if there's enough leftover for a new block */
static void split(block_t *b, uint32_t size) {
    if (b->size >= size + BLOCK_HDR + 16) {
        block_t *fresh = (block_t *)((uint8_t *)b + BLOCK_HDR + size);
        fresh->size = b->size - size - BLOCK_HDR;
        fresh->free = 1;
        fresh->next = b->next;
        b->size = size;
        b->next = fresh;
    }
}

void *kmalloc(uint32_t size) {
    if (size == 0) return (void *)0;

    /* align to 4 bytes */
    size = (size + 3) & ~3;

    block_t *b = find_free(size);
    if (!b) return (void *)0;

    split(b, size);
    b->free = 0;
    return (void *)((uint8_t *)b + BLOCK_HDR);
}

/* merge adjacent free blocks */
static void coalesce(void) {
    block_t *cur = head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += BLOCK_HDR + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_t *b = (block_t *)((uint8_t *)ptr - BLOCK_HDR);

    /* basic sanity: pointer should be within heap */
    if ((uint32_t)b < HEAP_START || (uint32_t)b >= HEAP_START + HEAP_SIZE)
        return;

    b->free = 1;
    coalesce();
}

void *krealloc(void *ptr, uint32_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return (void *)0; }

    block_t *b = (block_t *)((uint8_t *)ptr - BLOCK_HDR);
    if (b->size >= new_size)
        return ptr;

    void *fresh = kmalloc(new_size);
    if (!fresh) return (void *)0;

    /* copy old data */
    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)fresh;
    for (uint32_t i = 0; i < b->size; i++)
        dst[i] = src[i];

    kfree(ptr);
    return fresh;
}

uint32_t mmu_used(void) {
    uint32_t total = 0;
    block_t *cur = head;
    while (cur) {
        if (!cur->free)
            total += cur->size;
        cur = cur->next;
    }
    return total;
}

uint32_t mmu_free(void) {
    uint32_t total = 0;
    block_t *cur = head;
    while (cur) {
        if (cur->free)
            total += cur->size;
        cur = cur->next;
    }
    return total;
}
