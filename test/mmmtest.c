#define MMM_IMPLEMENTATION
#include <mmm.h>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

void verify_pool(mmm_pool *p_pool) {
    mmm_memblock *p_block = p_pool->p_first_block;
    uint64_t block_count = 0;
    uint64_t total_block_size = 0;

    while (p_block != NULL) {
        total_block_size += p_block->size;
        block_count++;

        if (p_block->p_prev != NULL) assert(p_block->p_prev->p_next == p_block);
        if (p_block->p_next != NULL) assert(p_block->p_next->p_prev == p_block);

        p_block = p_block->p_next;
    }

    assert(total_block_size < p_pool->size);

    printf("Pool: %I64u blocks, total blocks size %I64u (pool size %I64u)\n", block_count, total_block_size, p_pool->size);
}

uint32_t purge_decider(mmm_memblock *p_block, void *p_user_data) {
    (void)p_user_data;

    return p_block->tag < 2;
}

int32_t main() {
    mmm_pool pool;
    mmm_pool_init(64 * 1024, 8, 1, NULL, &pool);
    assert(pool.mem != NULL);

    verify_pool(&pool);

    int64_t *a = mmm_pool_alloc(&pool, sizeof(int64_t), 1);
    int32_t *b = mmm_pool_alloc(&pool, sizeof(int32_t), 1);
    double *c = mmm_pool_alloc(&pool, sizeof(double), 2);

    *a = 1;
    *b = 5;
    *c = 50.0;

    printf("Allocated: a=%I64d, b=%I32d=%p, c=%lf\n", *a, *b, b, *c);
    verify_pool(&pool);

    mmm_add_managed_ptr(&pool, b, (void **)&b);
    mmm_pool_grow(&pool, 128 * 1024, NULL);

    printf("Grew: a=%I64d, b=%I32d, c=%lf\n", *a, *b, *c);
    assert(*b == 5);
    verify_pool(&pool);

    mmm_pool_free(&pool, b);
    printf("freed b\n");
    verify_pool(&pool);

    mmm_pool_purge(&pool, purge_decider, NULL);
    printf("purged a\n");
    verify_pool(&pool);

    mmm_pool_uninit(&pool, NULL);
    return 0;
}
