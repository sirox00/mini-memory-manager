#ifndef MMM_H
#define MMM_H

#include <inttypes.h>
#include <stdlib.h>

// any of the allocation callbacks can be NULL, default one is used in that case
typedef struct {
    void *p_user_data;
    void *(*alloc)(uint64_t size, void *p_user_data);                  // default: malloc
    void *(*realloc)(void *ptr, uint64_t new_size, void *p_user_data); // default: if alloc is non-NULL, then alloc + memcpy, realloc otherwise
    void (*free)(void *ptr, void *p_user_data);                        // default: free
} mmm_allocation_callbacks;

typedef struct mmm_memblock_s {
    struct mmm_memblock_s *prev;
    struct mmm_memblock_s *next;

    uint64_t size;
    int64_t tag;       // tags are entirely user defined and are supposed to be used for data labeling for purging
    void **thisPtrs[]; // values pointed to by this would be updated on mmm_pool_grow, can be added with mmm_register_managed_ptr
} mmm_memblock;

typedef struct {
    mmm_memblock *first_block;

    uint64_t size;
    uint64_t alignment;
    uint64_t managed_ptrs_per_block;
    void *mem;
} mmm_pool;

mmm_pool mmm_pool_init(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, mmm_allocation_callbacks allocation_callbacks);
mmm_pool mmm_pool_init_preallocated(uint64_t size, uint64_t alignment, uint64_t managed_ptrs_per_block, void *mem);
void mmm_pool_grow(mmm_pool *pool, uint64_t new_size, mmm_allocation_callbacks allocation_callbacks);
void mmm_pool_grow_preallocated(mmm_pool *pool, uint64_t new_size, void *new_mem);
void mmm_pool_purge(mmm_pool *pool, uint32_t (*decider_func)(mmm_memblock *p_block, void *p_user_data), void *p_user_data);
// you dont need to uninit the pool, if it was used only with _preallocated versions of mmm_pool_init and mmm_pool_grow
void mmm_pool_uninit(mmm_pool *pool, mmm_allocation_callbacks allocation_callbacks);

void *mmm_pool_alloc(mmm_pool *pool, uint64_t size, uint64_t tag);
void *mmm_pool_realloc(mmm_pool *pool, void *allocation, uint64_t new_size);
void mmm_pool_free(mmm_pool *pool, void *allocation);

void mmm_set_tag(void *allocation, uint64_t new_tag);
void mmm_add_managed_ptr(void *allocation, void **p_ptr);

inline mmm_memblock *mmm_ptr_to_block(void *ptr) { return (mmm_memblock *)(ptr - sizeof(mmm_memblock)); }
inline void *mmm_block_to_ptr(mmm_memblock *p_block) { return (void *)(p_block) + sizeof(mmm_memblock); }

#ifdef MMM_IMPLEMENTATION

#endif
#endif
