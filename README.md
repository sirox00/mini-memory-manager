# mini memory manager
stb-like header-only memory management library in C is inspired by zone allocator from doom.

# usage
if you're using cmake, include this project in your main CMakeLists.txt and link with `mmm` library.
on any other build system you can just add this project as an include directory.

Add the next line before including mmm.h in *one* of your source files to create the implementation:
```c
#define MMM_IMPLEMENTATION
```
if you don't want to clutter your source files with library implementations, then create a file named `mmm.c` and paste the following in it:
```c
#define MMM_IMPLEMENTATION
#include <mmm.h>
```

# API examples
creating a pool
```c
#define MMM_IMPLEMENTATION
#include <mmm.h>

#include <inttypes.h>

int32_t main() {
    const uint64_t size = 1 * 1024 * 1024; // 1 megabyte
    // all pointers and block structure pointers would be a multiple of 16, must be a power of 2
    const uint64_t alignment = 16;
    // mmm introduces a concept of a managed pointer to deal with growing the pool
    // basically, you can add a pointer to your varible into the block structure
    // then, when growing the pool, the new address of that block's memory would be put into those variables
    const uint32_t max_managed_ptrs = 1;

    // alignment and max_managed_ptrs CAN NOT be changed after the pool is created

    mmm_pool pool;
    mmm_pool_init(size, alignment, max_managed_ptrs, NULL, &pool);

    // on error, pool.mem is set to NULL
    if (pool.mem == NULL) return -1;

    // dont forget to uninit the pool so that its memory can be released
    mmm_pool_uninit(&pool);
    
    return 0;
}
```

creating a pool with custom allocators
```c

#define MMM_IMPLEMENTATION
#include <mmm.h>

#include <inttypes.h>

int32_t main() {
    mmm_allocation_callbacks alloc_cbs = {};
    alloc_cbs.p_user_data = NULL;
    // note that alloc function must return a pointer that is a multiple of the `alignment` argument
    alloc_cbs.alloc = my_alloc_func; // if NULL, the default function would be used (mmm_aligned_alloc)
    alloc_cbs.free = my_free_func; // if NULL, the default function would be used (mmm_aligned_free)

    mmm_pool pool;
    // alloc_cbs is NOT required to have a lifetime longer than the pool
    // because it is provided as an argument to all functions that need it
    mmm_pool_init(1 * 1024 * 1024, 16, 1, &alloc_cbs, &pool);

    if (pool.mem == NULL) return -1;

    mmm_pool_uninit(&pool);
    
    return 0;
}
```

making a pool in a preallocated buffer
```c

#define MMM_IMPLEMENTATION
#include <mmm.h>

#include <inttypes.h>
#include <stdlib.h>

int32_t main() {
    void *buf = malloc(1 * 1024 * 1024);
    
    mmm_pool pool;
    mmm_pool_init_preallocated(1 * 1024 * 1024, 16, 1, buf, &pool);

    if (pool.mem == NULL) return -1;

    // we dont need to uninit a pool created inside of a preallocated buffer
    // (note that you must use _preallocated grow with such a pool)
    // though dont forget to manage the buffer itself
    free(buf);
    
    return 0;
}
```

allocating memory from the pool
```c
#define MMM_IMPLEMENTATION
#include <mmm.h>

#include <inttypes.h>
#include <stdio.h>

int32_t main() {
    mmm_pool pool;
    mmm_pool_init(1 * 1024 * 1024, 16, 1, NULL, &pool);

    if (pool.mem == NULL) return -1;

    // the last parameter is the tag, 0 reserved labeling a free block
    // tags are useful for data labeling and deciding which allocations to purge
    // you can read the tag with mmm_get_tag or set with mmm_set_tag
    void *p = mmm_pool_alloc(&pool, sizeof(uint64_t) * 100, 1);

    for (uint64_t i = 0; i < 100; i++) p[i] = i+1;

    for (uint64_t i = 0; i < 100; i++) printf("%I64u ", p[i]);
    printf("\n");

    mmm_pool_free(&pool, p);

    mmm_pool_uninit(&pool);

    return 0;
}
```

growing the pool
```c
#define MMM_IMPLEMENTATION
#include <mmm.h>

#include <inttypes.h>

int32_t main() {
    mmm_pool pool;
    mmm_pool_init(1 * 1024 * 1024, 16, 1, NULL, &pool);

    if (pool.mem == NULL) return -1;

    void *p = mmm_pool_alloc(&pool, sizeof(uint64_t) * 100, 1);

    for (uint64_t i = 0; i < 100; i++) p[i] = i+1;

    // prints numbers 1 ... 100
    for (uint64_t i = 0; i < 100; i++) printf("%I64u ", p[i]);
    printf("\n");

    // we add a pointer to variable `p` as a managed pointer to the block structure of that allocation
    // after we grow the pool, all pointers become invalid
    // but managed pointers would have their values updated to new ones by mmm_pool_grow
    mmm_add_managed_ptr(&pool, p, &p);

    mmm_pool_grow(&pool, 2 * 1024 * 1024, NULL);

    if (pool.mem == NULL) return -1;

    // prints numbers 1 ... 100 once again, as p now points to new memory
    for (uint64_t i = 0; i < 100; i++) printf("%I64u ", p[i]);
    printf("\n");

    mmm_pool_free(&pool, p);

    mmm_pool_uninit(&pool);

    return 0;
}
```

purging data
```c

#define MMM_IMPLEMENTATION
#include <mmm.h>

#include <inttypes.h>

uint32_t purge_decider_func(mmm_memblock *p_block, void *p_user_data) {
    (void)p_user_data;

    // returning 0 means not purge, anything else to purge
    return p_block->tag < 2;
}

int32_t main() {
    mmm_pool pool;
    mmm_pool_init(1 * 1024 * 1024, 16, 1, NULL, &pool);

    if (pool.mem == NULL) return -1;

    void *p = mmm_pool_alloc(&pool, sizeof(uint64_t) * 100, 1);
    void *p2 = mmm_pool_alloc(&pool, sizeof(uint64_t) * 100, 2);

    // p is freed here
    mmm_pool_purge(&pool, purge_decider_func, NULL);

    mmm_pool_free(&pool, p2);

    mmm_pool_uninit(&pool);

    return 0;
}
```
