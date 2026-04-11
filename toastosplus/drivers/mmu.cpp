/* toastMMU - simple free-list memory manager */

#include "mmu.hpp"
#include "kio.hpp"

namespace toast {
namespace mem {

namespace {  // anonymous namespace for internal linkage

struct block {
    uint32_t size;       /* usable bytes in this block */
    uint8_t  free;
    block *next;
};

constexpr uint32_t BLOCK_HDR = sizeof(block);

block *head = nullptr;

/* find first block that fits */
block *find_free(uint32_t size) {
    block *cur = head;
    while (cur) {
        if (cur->free && cur->size >= size)
            return cur;
        cur = cur->next;
    }
    return nullptr;
}

/* split a block if there's enough leftover for a new block */
void split(block *b, uint32_t size) {
    if (b->size >= size + BLOCK_HDR + 16) {
        block *fresh = reinterpret_cast<block*>(reinterpret_cast<uint8_t*>(b) + BLOCK_HDR + size);
        fresh->size = b->size - size - BLOCK_HDR;
        fresh->free = 1;
        fresh->next = b->next;
        b->size = size;
        b->next = fresh;
    }
}

/* merge adjacent free blocks */
void coalesce() {
    block *cur = head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += BLOCK_HDR + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

} // anonymous namespace

void init() {
    head = reinterpret_cast<block*>(HEAP_START);
    head->size = HEAP_SIZE - BLOCK_HDR;
    head->free = 1;
    head->next = nullptr;
}

void* alloc(uint32_t size) {
    if (size == 0) return nullptr;

    /* align to 4 bytes */
    size = (size + 3) & ~3;

    block *b = find_free(size);
    if (!b) return nullptr;

    split(b, size);
    b->free = 0;
    return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(b) + BLOCK_HDR);
}

void free(void *ptr) {
    if (!ptr) return;

    block *b = reinterpret_cast<block*>(reinterpret_cast<uint8_t*>(ptr) - BLOCK_HDR);

    /* basic sanity: pointer should be within heap */
    if (reinterpret_cast<uint32_t>(b) < HEAP_START || 
        reinterpret_cast<uint32_t>(b) >= HEAP_START + HEAP_SIZE)
        return;

    b->free = 1;
    coalesce();
}

void* realloc(void *ptr, uint32_t new_size) {
    if (!ptr) return alloc(new_size);
    if (new_size == 0) { free(ptr); return nullptr; }

    block *b = reinterpret_cast<block*>(reinterpret_cast<uint8_t*>(ptr) - BLOCK_HDR);
    if (b->size >= new_size)
        return ptr;

    void *fresh = alloc(new_size);
    if (!fresh) return nullptr;

    /* copy old data */
    uint8_t *src = reinterpret_cast<uint8_t*>(ptr);
    uint8_t *dst = reinterpret_cast<uint8_t*>(fresh);
    for (uint32_t i = 0; i < b->size; i++)
        dst[i] = src[i];

    free(ptr);
    return fresh;
}

/* Allocate with a specific alignment (must be power of 2) */
void* alloc_aligned(uint32_t size, uint32_t alignment) {
    if (size == 0 || alignment == 0) return nullptr;

    /* Over-allocate to guarantee alignment */
    uint32_t total = size + alignment + sizeof(uint32_t);
    void *raw = alloc(total);
    if (!raw) return nullptr;

    /* Align the usable address */
    uint32_t addr = (reinterpret_cast<uint32_t>(raw) + sizeof(uint32_t) + alignment - 1) & ~(alignment - 1);

    /* Store the original pointer just before the aligned address */
    *(reinterpret_cast<uint32_t*>(addr) - 1) = reinterpret_cast<uint32_t>(raw);

    return reinterpret_cast<void*>(addr);
}

uint32_t used() {
    uint32_t total = 0;
    block *cur = head;
    while (cur) {
        if (!cur->free)
            total += cur->size;
        cur = cur->next;
    }
    return total;
}

uint32_t available() {
    uint32_t total = 0;
    block *cur = head;
    while (cur) {
        if (cur->free)
            total += cur->size;
        cur = cur->next;
    }
    return total;
}

} // namespace mem
} // namespace toast
