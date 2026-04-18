/**
 * @file test_memory.c
 * @brief Unit tests for the Aether shared memory allocator.
 *
 * The suite validates L0/L1 baseline behavior and key invariants from the
 * architecture spec (deterministic, bounded behavior and alignment safety).
 *
 * Build:
 * gcc -std=c11 -Wall -Wextra -Werror -I../include test_memory.c \
 *     ../src/aether_memory.c ../src/aether_platform.c -o test_memory
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "aether_memory.h"
#include "aether_platform.h"

static void test_init_alignment_and_layout(void) {
    aether_mem_pool_t *pool = aether_mem_init(1000);
    assert(pool != NULL);
    assert(pool->total_size % AETHER_ALIGNMENT == 0);
    assert(((uintptr_t)pool->base % AETHER_ALIGNMENT) == 0);
    assert(pool->free_offset == 0);

    for (size_t i = 0; i < AETHER_MAX_SEGMENTS; ++i) {
        assert(pool->segments[i].in_use == 0);
        assert(pool->segments[i].offset == 0);
        assert(pool->segments[i].size == 0);
    }

    aether_mem_destroy(pool);
}

static void test_basic_alloc_and_distinct_ptrs(void) {
    aether_mem_pool_t *pool = aether_mem_init(1024);
    assert(pool != NULL);

    aether_handle_t h1 = aether_mem_alloc(pool, 128);
    aether_handle_t h2 = aether_mem_alloc(pool, 64);
    assert(h1 >= 0);
    assert(h2 >= 0);
    assert(h1 != h2);

    void *p1 = aether_mem_get_ptr(pool, h1);
    void *p2 = aether_mem_get_ptr(pool, h2);
    assert(p1 != NULL);
    assert(p2 != NULL);
    assert(p1 != p2);

    assert((pool->segments[h1].size % AETHER_ALIGNMENT) == 0);
    assert((pool->segments[h2].size % AETHER_ALIGNMENT) == 0);

    aether_mem_destroy(pool);
}

static void test_reuse_freed_segment(void) {
    aether_mem_pool_t *pool = aether_mem_init(1024);
    assert(pool != NULL);

    aether_handle_t h1 = aether_mem_alloc(pool, 128);
    assert(h1 >= 0);
    void *p1 = aether_mem_get_ptr(pool, h1);
    assert(p1 != NULL);

    aether_mem_free(pool, h1);
    assert(aether_mem_get_ptr(pool, h1) == NULL);

    aether_handle_t h2 = aether_mem_alloc(pool, 32);
    assert(h2 == h1);
    void *p2 = aether_mem_get_ptr(pool, h2);
    assert(p2 == p1);

    aether_mem_destroy(pool);
}

static void test_zero_size_and_out_of_bounds_handling(void) {
    aether_mem_pool_t *pool = aether_mem_init(256);
    assert(pool != NULL);

    assert(aether_mem_alloc(pool, 0) == -1);
    assert(aether_mem_get_ptr(pool, -1) == NULL);
    assert(aether_mem_get_ptr(pool, (aether_handle_t)AETHER_MAX_SEGMENTS) == NULL);

    aether_mem_free(pool, -1);
    aether_mem_free(pool, (aether_handle_t)AETHER_MAX_SEGMENTS);

    aether_mem_destroy(pool);
}

static void test_memory_exhaustion(void) {
    aether_mem_pool_t *pool = aether_mem_init(256);
    assert(pool != NULL);

    aether_handle_t h1 = aether_mem_alloc(pool, 128);
    aether_handle_t h2 = aether_mem_alloc(pool, 128);
    aether_handle_t h3 = aether_mem_alloc(pool, 64);

    assert(h1 >= 0);
    assert(h2 >= 0);
    assert(h3 == -1);

    aether_mem_destroy(pool);
}

static void test_segment_metadata_slot_exhaustion(void) {
    /* Large pool so memory is not the limiting factor, slot count is. */
    aether_mem_pool_t *pool = aether_mem_init(65536);
    assert(pool != NULL);

    for (size_t i = 0; i < AETHER_MAX_SEGMENTS; ++i) {
        aether_handle_t h = aether_mem_alloc(pool, 64);
        assert(h >= 0);
    }

    assert(aether_mem_alloc(pool, 64) == -1);

    aether_mem_destroy(pool);
}

static void test_tail_free_shrinks_free_offset(void) {
    aether_mem_pool_t *pool = aether_mem_init(1024);
    assert(pool != NULL);

    aether_handle_t h1 = aether_mem_alloc(pool, 128);
    aether_handle_t h2 = aether_mem_alloc(pool, 128);
    assert(h1 >= 0);
    assert(h2 >= 0);

    size_t old_free_offset = pool->free_offset;
    assert(old_free_offset > 0);

    aether_mem_free(pool, h2);
    assert(pool->free_offset < old_free_offset);

    aether_mem_destroy(pool);
}

static void test_platform_spinlock_basic(void) {
    aether_spinlock_t lock;
    aether_spinlock_init(&lock);
    aether_spinlock_lock(&lock);
    aether_spinlock_unlock(&lock);
}

int main(void) {
    test_init_alignment_and_layout();
    test_basic_alloc_and_distinct_ptrs();
    test_reuse_freed_segment();
    test_zero_size_and_out_of_bounds_handling();
    test_memory_exhaustion();
    test_segment_metadata_slot_exhaustion();
    test_tail_free_shrinks_free_offset();
    test_platform_spinlock_basic();

    printf("All Aether L0/L1 tests passed (allocator + platform primitives).\n");
    return 0;
}
