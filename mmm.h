/* mmm.h - mini memory manager in C that is inspired by the zone allocator from Doom
 * see LICENSE or end of this file for the licensing
 *
 * Add the next line before including mmm.h in *one* of your source files to create the implementation:
 * #define MMM_IMPLEMENTATION
 *
 * see declarations below for documentation
 * see test/mmmtest.c and README.md for examples of the API
 */

#ifndef MMM_H
#define MMM_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <inttypes.h>
#include <malloc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef MMM_DEF
#define MMM_DEF
#endif /* MMM_DEF */

// any of the allocation callbacks can be NULL, default one is used in that case
typedef struct {
    void *p_user_data;
    void *(*alloc)(uint64_t size, uint64_t alignment, void *p_user_data); // default: mmm_aligned_alloc
    void (*free)(void *ptr, void *p_user_data);                           // default: mmm_aligned_free
} mmm_allocation_callbacks;

typedef struct mmm_memblock_s {
    struct mmm_memblock_s *p_prev;
    struct mmm_memblock_s *p_next;

    uint64_t size;
    int64_t tag;           // tags are entirely user defined and are supposed to be used for data labeling for purging, 0 is reserved for a free block
    void **managed_ptrs[]; // values pointed to by this would be updated on mmm_pool_grow, can be added with mmm_register_managed_ptr
} mmm_memblock;

typedef struct {
    mmm_memblock *p_first_block;

    uint64_t size;
    uint64_t alignment;
    uint64_t managed_ptrs_per_block;
    void *mem;
} mmm_pool;

// memory allocation functions used internally by default
MMM_DEF void *mmm_aligned_alloc(uint64_t size, uint64_t alignment);
MMM_DEF void mmm_aligned_free(void *ptr);

MMM_DEF void mmm_pool_init(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, mmm_allocation_callbacks *p_allocation_callbacks, mmm_pool *p_pool);
MMM_DEF void mmm_pool_init_preallocated(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, void *mem, mmm_pool *p_pool);
MMM_DEF void mmm_pool_grow(mmm_pool *p_pool, uint64_t new_size, mmm_allocation_callbacks *p_allocation_callbacks);
MMM_DEF void mmm_pool_grow_preallocated(mmm_pool *p_pool, uint64_t new_size, void *new_mem);
MMM_DEF void mmm_pool_purge(mmm_pool *p_pool, uint32_t (*decider_func)(mmm_memblock *p_block, void *p_user_data), void *p_user_data);
// you dont need to uninit the pool, if it was used only with _preallocated versions of mmm_pool_init and mmm_pool_grow
MMM_DEF void mmm_pool_uninit(mmm_pool *p_pool, mmm_allocation_callbacks *p_allocation_callbacks);

MMM_DEF void *mmm_pool_alloc(mmm_pool *p_pool, uint64_t size, uint64_t tag);
MMM_DEF void mmm_pool_free(mmm_pool *p_pool, void *allocation);

MMM_DEF void mmm_set_tag(mmm_pool *p_pool, void *allocation, uint64_t new_tag);
MMM_DEF int64_t mmm_get_tag(mmm_pool *p_pool, void *allocation);
MMM_DEF void mmm_add_managed_ptr(mmm_pool *p_pool, void *allocation, void **p_ptr);

static inline uint32_t mmm_is_power_of_2(uint64_t n) { return (n != 0) && ((n & (n - 1)) == 0); }
static inline uint32_t mmm_block_is_free(mmm_memblock *p_block) { return p_block->tag == 0; }
static inline uint64_t mmm_align_size(uint64_t size, uint64_t alignment) { return (size + alignment - 1) & ~(alignment - 1); }
static inline void *mmm_align_ptr(void *ptr, uint64_t alignment) { return (void *)((uint64_t)(ptr + alignment - 1) & ~(alignment - 1)); }
static inline uint64_t mmm_aligned_memblock_size(mmm_pool *p_pool) {
    return mmm_align_size(sizeof(mmm_memblock) + p_pool->managed_ptrs_per_block * sizeof(void *), p_pool->alignment);
}
static inline mmm_memblock *mmm_ptr_to_block(mmm_pool *p_pool, void *ptr) { return (mmm_memblock *)(ptr - mmm_aligned_memblock_size(p_pool)); }
static inline void *mmm_block_to_ptr(mmm_pool *p_pool, mmm_memblock *p_block) { return (void *)(p_block) + mmm_aligned_memblock_size(p_pool); }

#ifdef MMM_IMPLEMENTATION

MMM_DEF void *mmm_aligned_alloc(uint64_t size, uint64_t alignment) {
#if defined(_WIN32) || defined(_WIN64)
    // windows being windows
    return _aligned_malloc(size, alignment);
#else
    // there is posix_memalign, but the man page says, that its better to use aligned_alloc,
    // and i doubt, that any major os (other than windows) would not implement it
    //
    // file an issue on github, if you encounter problems with it
    return aligned_alloc(alignment, size);
#endif
}

MMM_DEF void mmm_aligned_free(void *ptr) {
#if defined(_WIN32) || defined(_WIN64)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

MMM_DEF void mmm_pool_init(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, mmm_allocation_callbacks *p_allocation_callbacks, mmm_pool *p_pool) {
    if (!mmm_is_power_of_2(alignment) || size < sizeof(mmm_memblock) + managed_ptrs_per_block * sizeof(void *)) {
        p_pool->mem = NULL;
        return;
    }

    void *mem = NULL;
    if (p_allocation_callbacks != NULL) {
        if (p_allocation_callbacks->alloc != NULL) mem = p_allocation_callbacks->alloc(size, alignment, p_allocation_callbacks->p_user_data);
    }
    if (mem == NULL) mem = mmm_aligned_alloc(size, alignment);

    mmm_pool_init_preallocated(size, alignment, managed_ptrs_per_block, mem, p_pool);
}

MMM_DEF void mmm_pool_init_preallocated(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, void *mem, mmm_pool *p_pool) {
    if (!mmm_is_power_of_2(alignment) || size < sizeof(mmm_memblock) + managed_ptrs_per_block * sizeof(void *) || mem == NULL) {
        p_pool->mem = NULL;
        return;
    }

    p_pool->size = size;
    p_pool->alignment = alignment;
    p_pool->managed_ptrs_per_block = managed_ptrs_per_block;
    p_pool->mem = mem;

    mmm_memblock *p_block = p_pool->mem;
    p_block->p_prev = NULL;
    p_block->p_next = NULL;
    p_block->size = p_pool->size - mmm_aligned_memblock_size(p_pool);
    p_block->tag = 0;
    for (uint64_t i = 0; i < p_pool->managed_ptrs_per_block; i++)
        p_block->managed_ptrs[i] = NULL;

    p_pool->p_first_block = p_block;
}

MMM_DEF void mmm_pool_grow(mmm_pool *p_pool, uint64_t new_size, mmm_allocation_callbacks *p_allocation_callbacks) {
    if (new_size <= p_pool->size) return;

    void *new_mem = NULL;
    if (p_allocation_callbacks != NULL) {
        if (p_allocation_callbacks->alloc != NULL) new_mem = p_allocation_callbacks->alloc(new_size, p_pool->alignment, p_allocation_callbacks->p_user_data);
    }
    if (new_mem == NULL) new_mem = mmm_aligned_alloc(new_size, p_pool->alignment);

    void *old_mem = p_pool->mem;
    mmm_pool_grow_preallocated(p_pool, new_size, new_mem);

    if (p_allocation_callbacks != NULL) {
        if (p_allocation_callbacks->free != NULL) {
            p_allocation_callbacks->free(old_mem, p_allocation_callbacks->p_user_data);
        } else
            mmm_aligned_free(old_mem);
    } else
        mmm_aligned_free(old_mem);
}

MMM_DEF void mmm_pool_grow_preallocated(mmm_pool *p_pool, uint64_t new_size, void *new_mem) {
    if (new_size <= p_pool->size || new_mem == NULL) return;

    memcpy(new_mem, p_pool->mem, p_pool->size);

    uint64_t old_size = p_pool->size;
    void *old_mem = p_pool->mem;

    p_pool->size = new_size;
    p_pool->mem = new_mem;

    mmm_memblock *last_block = p_pool->p_first_block;
    while (last_block->p_next != NULL)
        last_block = last_block->p_next;

    if (mmm_block_is_free(last_block)) {
        last_block->size += new_size - old_size;
    } else {
        mmm_memblock *new_block = p_pool->mem + (uint64_t)mmm_align_ptr(mmm_block_to_ptr(p_pool, last_block) + last_block->size, p_pool->alignment);

        last_block->p_next = new_block;

        new_block->p_prev = last_block;
        new_block->p_next = NULL;
        new_block->size = new_size - old_size;
        new_block->tag = 0;
        for (uint64_t i = 0; i < p_pool->managed_ptrs_per_block; i++)
            new_block->managed_ptrs[i] = NULL;
    }

    p_pool->p_first_block = ((void *)p_pool->p_first_block - old_mem) + p_pool->mem;

    for (mmm_memblock *p_block = p_pool->p_first_block; p_block != NULL; p_block = p_block->p_next) {
        if (p_block->p_prev != NULL) p_block->p_prev = ((void *)p_block->p_prev - old_mem) + p_pool->mem;
        if (p_block->p_next != NULL) p_block->p_next = ((void *)p_block->p_next - old_mem) + p_pool->mem;

        for (uint64_t i = 0; i < p_pool->managed_ptrs_per_block; i++) {
            if (p_block->managed_ptrs[i] != NULL) *(p_block->managed_ptrs[i]) = mmm_block_to_ptr(p_pool, p_block);
        }
    }
}

MMM_DEF void mmm_pool_purge(mmm_pool *p_pool, uint32_t (*decider_func)(mmm_memblock *p_block, void *p_user_data), void *p_user_data) {
    for (mmm_memblock *p_block = p_pool->p_first_block; p_block != NULL; p_block = p_block->p_next) {
        if (mmm_block_is_free(p_block)) continue;
        if (decider_func(p_block, p_user_data)) {
            mmm_pool_free(p_pool, mmm_block_to_ptr(p_pool, p_block));
        }
    }
}

MMM_DEF void mmm_pool_uninit(mmm_pool *p_pool, mmm_allocation_callbacks *p_allocation_callbacks) {
    if (p_allocation_callbacks != NULL) {
        if (p_allocation_callbacks->free != NULL) {
            p_allocation_callbacks->free(p_pool->mem, p_allocation_callbacks->p_user_data);
        } else
            mmm_aligned_free(p_pool->mem);
    } else
        mmm_aligned_free(p_pool->mem);
}

MMM_DEF void *mmm_pool_alloc(mmm_pool *p_pool, uint64_t size, uint64_t tag) {
    if (tag == 0) return NULL;

    uint64_t aligned_size = mmm_align_size(size, p_pool->alignment);

    mmm_memblock *p_found_block = NULL;
    for (mmm_memblock *p_block = p_pool->p_first_block; p_block != NULL; p_block = p_block->p_next) {
        if (!mmm_block_is_free(p_block) || p_block->size < aligned_size) continue;

        p_found_block = p_block;
        break;
    }
    if (p_found_block == NULL) return NULL;

    // verify, that we can fit another block of at least alignment bytes size after the one we found
    if (p_found_block->size >= aligned_size + mmm_aligned_memblock_size(p_pool) + p_pool->alignment) {
        // subdivide the block
        mmm_memblock *p_next_block = p_found_block + aligned_size;

        p_next_block->p_prev = p_found_block;
        p_next_block->size = p_found_block->size - aligned_size - mmm_aligned_memblock_size(p_pool);
        p_next_block->tag = 0;
        for (uint64_t i = 0; i < p_pool->managed_ptrs_per_block; i++)
            p_next_block->managed_ptrs[i] = NULL;

        if (p_found_block->p_next != NULL) {
            p_next_block->p_next = p_found_block->p_next;
            p_found_block->p_next->p_prev = p_next_block;
        } else
            p_next_block->p_next = NULL;

        p_found_block->p_next = p_next_block;
        p_found_block->size = aligned_size;
        p_found_block->tag = tag;
    } else {
        p_found_block->tag = tag;
    }

    return mmm_block_to_ptr(p_pool, p_found_block);
}

MMM_DEF void mmm_pool_free(mmm_pool *p_pool, void *allocation) {
    mmm_memblock *p_block = mmm_ptr_to_block(p_pool, allocation);
    p_block->tag = 0;

    // merge with previous
    if (p_block->p_prev != NULL) {
        if (mmm_block_is_free(p_block->p_prev)) {
            p_block->p_prev->size += mmm_aligned_memblock_size(p_pool) + p_block->size;
            p_block->p_prev->p_next = p_block->p_next;
            if (p_block->p_next != NULL) p_block->p_next->p_prev = p_block->p_prev;

            // set this so that we can merge this merged block with the next one, if its free
            p_block = p_block->p_prev;
        }
    }

    // merge with next
    if (p_block->p_next != NULL) {
        if (mmm_block_is_free(p_block->p_next)) {
            p_block->size += mmm_aligned_memblock_size(p_pool) + p_block->p_next->size;
            if (p_block->p_next->p_next != NULL) p_block->p_next->p_next->p_prev = p_block;
            p_block->p_next = p_block->p_next->p_next;
        }
    }
}

MMM_DEF void mmm_set_tag(mmm_pool *p_pool, void *allocation, uint64_t new_tag) {
    mmm_memblock *p_block = mmm_ptr_to_block(p_pool, allocation);
    p_block->tag = new_tag;
}

MMM_DEF int64_t mmm_get_tag(mmm_pool *p_pool, void *allocation) {
    mmm_memblock *p_block = mmm_ptr_to_block(p_pool, allocation);
    return p_block->tag;
}

MMM_DEF void mmm_add_managed_ptr(mmm_pool *p_pool, void *allocation, void **p_ptr) {
    mmm_memblock *p_block = mmm_ptr_to_block(p_pool, allocation);
    for (uint64_t i = 0; i < p_pool->managed_ptrs_per_block; i++)
        if (p_block->managed_ptrs[i] == NULL) {
            p_block->managed_ptrs[i] = p_ptr;
            break;
        }
}

#endif /* MMM_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* MMM_H */

/*
    This is free and unencumbered software released into the public domain.

    Anyone is free to copy, modify, publish, use, compile, sell, or
    distribute this software, either in source code form or as a compiled
    binary, for any purpose, commercial or non-commercial, and by any
    means.

    In jurisdictions that recognize copyright laws, the author or authors
    of this software dedicate any and all copyright interest in the
    software to the public domain. We make this dedication for the benefit
    of the public at large and to the detriment of our heirs and
    successors. We intend this dedication to be an overt act of
    relinquishment in perpetuity of all present and future rights to this
    software under copyright law.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
    OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.

    For more information, please refer to <https://unlicense.org>
 */
