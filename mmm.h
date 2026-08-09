#ifndef MMM_H
#define MMM_H

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef MMM_DEF
#define MMM_DEF
#endif

// any of the allocation callbacks can be NULL, default one is used in that case
typedef struct {
    void *p_user_data;
    void *(*alloc)(uint64_t size, void *p_user_data); // default: malloc
    void (*free)(void *ptr, void *p_user_data);       // default: free
} mmm_allocation_callbacks;

typedef struct mmm_memblock_s {
    struct mmm_memblock_s *prev;
    struct mmm_memblock_s *next;

    uint64_t size;
    int64_t tag;        // tags are entirely user defined and are supposed to be used for data labeling for purging, 0 is reserved for a free block
    void **this_ptrs[]; // values pointed to by this would be updated on mmm_pool_grow, can be added with mmm_register_managed_ptr
} mmm_memblock;

typedef struct {
    mmm_memblock *first_block;

    uint64_t size;
    uint64_t alignment;
    uint64_t managed_ptrs_per_block;
    void *mem;
} mmm_pool;

MMM_DEF mmm_pool mmm_pool_init(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, mmm_allocation_callbacks allocation_callbacks);
MMM_DEF mmm_pool mmm_pool_init_preallocated(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, void *mem);
MMM_DEF void mmm_pool_grow(mmm_pool *pool, uint64_t new_size, mmm_allocation_callbacks allocation_callbacks);
MMM_DEF void mmm_pool_grow_preallocated(mmm_pool *pool, uint64_t new_size, void *new_mem);
MMM_DEF void mmm_pool_purge(mmm_pool *pool, uint32_t (*decider_func)(mmm_memblock *p_block, void *p_user_data), void *p_user_data);
// you dont need to uninit the pool, if it was used only with _preallocated versions of mmm_pool_init and mmm_pool_grow
MMM_DEF void mmm_pool_uninit(mmm_pool *pool, mmm_allocation_callbacks allocation_callbacks);

MMM_DEF void *mmm_pool_alloc(mmm_pool *pool, uint64_t size, uint64_t tag);
MMM_DEF void mmm_pool_free(mmm_pool *pool, void *allocation);

MMM_DEF void mmm_set_tag(void *allocation, uint64_t new_tag);
MMM_DEF void mmm_add_managed_ptr(void *allocation, void **p_ptr);

static inline uint32_t mmm_block_is_free(mmm_memblock *p_Block) { return p_Block->tag == 0; }
static inline mmm_memblock *mmm_ptr_to_block(void *ptr) { return (mmm_memblock *)(ptr - sizeof(mmm_memblock)); }
static inline void *mmm_block_to_ptr(mmm_memblock *p_block) { return (void *)(p_block) + sizeof(mmm_memblock); }

#define MMM_IMPLEMENTATION
#ifdef MMM_IMPLEMENTATION

MMM_DEF mmm_pool mmm_pool_init(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, mmm_allocation_callbacks allocation_callbacks) {
    return mmm_pool_init_preallocated(size, alignment, managed_ptrs_per_block,
                                      allocation_callbacks.alloc == NULL ? malloc(size) : allocation_callbacks.alloc(size, allocation_callbacks.p_user_data));
}

MMM_DEF mmm_pool mmm_pool_init_preallocated(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, void *mem) {
    mmm_pool pool = {};

    pool.size = size;
    pool.alignment = alignment;
    pool.managed_ptrs_per_block = managed_ptrs_per_block;
    pool.mem = mem;

    mmm_memblock *p_block = pool.mem;
    p_block->prev = NULL;
    p_block->next = NULL;
    p_block->size = pool.size;
    p_block->tag = 0;
    for (uint64_t i = 0; i < pool.managed_ptrs_per_block; i++)
        p_block->this_ptrs[i] = NULL;

    pool.first_block = p_block;

    return pool;
}

MMM_DEF void mmm_pool_grow(mmm_pool *pool, uint64_t new_size, mmm_allocation_callbacks allocation_callbacks) {
    void *old_mem = pool->mem;
    mmm_pool_grow_preallocated(pool, new_size,
                               allocation_callbacks.alloc == NULL ? malloc(new_size) : allocation_callbacks.alloc(new_size, allocation_callbacks.p_user_data));
    if (allocation_callbacks.free != NULL)
        allocation_callbacks.free(old_mem, allocation_callbacks.p_user_data);
    else
        free(old_mem);
}

MMM_DEF void mmm_pool_grow_preallocated(mmm_pool *pool, uint64_t new_size, void *new_mem) {
    memcpy(new_mem, pool->mem, pool->size);

    uint64_t old_size = pool->size;
    void *old_mem = pool->mem;

    pool->size = new_size;
    pool->mem = new_mem;

    mmm_memblock *last_block = pool->first_block;
    while (last_block->next != NULL)
        last_block = last_block->next;

    if (mmm_block_is_free(last_block)) {
        last_block->size += new_size - old_size;
    } else {
        mmm_memblock *new_block = pool->mem + old_size;

        new_block->prev = last_block;
        new_block->next = NULL;
        new_block->size = new_size - old_size;
        new_block->tag = 0;
        for (uint64_t i = 0; i < pool->managed_ptrs_per_block; i++)
            new_block->this_ptrs[i] = NULL;
    }

    pool->first_block = ((void *)pool->first_block - old_mem) + pool->mem;

    for (mmm_memblock *p_block = pool->first_block; p_block != NULL; p_block = p_block->next) {
        for (uint64_t i = 0; i < pool->managed_ptrs_per_block; i++) {
            if (p_block->prev != NULL) p_block->prev = ((void *)p_block->prev - old_mem) + pool->mem;
            if (p_block->next != NULL) p_block->next = ((void *)p_block->prev - old_mem) + pool->mem;

            if (p_block->this_ptrs[i] != NULL) *(p_block->this_ptrs[i]) = p_block;
        }
    }
}

MMM_DEF void mmm_pool_uninit(mmm_pool *pool, mmm_allocation_callbacks allocation_callbacks) {
    if (allocation_callbacks.free != NULL)
        allocation_callbacks.free(pool->mem, allocation_callbacks.p_user_data);
    else
        free(pool->mem);
}

#endif
#endif
