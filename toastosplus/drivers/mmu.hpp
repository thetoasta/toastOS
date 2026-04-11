/*
 * toastOS++ Memory Management Unit
 * Namespace: toast::mem
 */

#ifndef MMU_HPP
#define MMU_HPP

#include "stdint.hpp"

/* Heap configuration - lives at 0x400000 - 0x600000 (2MB) */
#define HEAP_START 0x400000
#define HEAP_SIZE  0x200000

namespace toast {
namespace mem {

void init();
void* alloc(uint32_t size);
void* alloc_aligned(uint32_t size, uint32_t alignment);
void free(void* ptr);
void* realloc(void* ptr, uint32_t new_size);
uint32_t used();
uint32_t available();

/* Template allocator for typed allocation */
template<typename T>
inline T* create() { return static_cast<T*>(alloc(sizeof(T))); }

template<typename T>
inline T* create_array(uint32_t count) { return static_cast<T*>(alloc(sizeof(T) * count)); }

template<typename T>
inline void destroy(T* ptr) { free(ptr); }

} // namespace mem
} // namespace toast

/* Legacy C-style aliases for compatibility */
inline void mmu_init() { toast::mem::init(); }
inline void* kmalloc(uint32_t size) { return toast::mem::alloc(size); }
inline void* kmalloc_aligned(uint32_t size, uint32_t align) { return toast::mem::alloc_aligned(size, align); }
inline void kfree(void* ptr) { toast::mem::free(ptr); }
inline void* krealloc(void* ptr, uint32_t size) { return toast::mem::realloc(ptr, size); }
inline uint32_t mmu_used() { return toast::mem::used(); }
inline uint32_t mmu_free() { return toast::mem::available(); }

#endif /* MMU_HPP */
