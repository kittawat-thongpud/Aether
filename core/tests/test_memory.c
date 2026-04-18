/**
 * @file test_memory.c
 * @brief Unit tests for the Aether shared memory allocator.
 *
 * These tests validate basic allocation and free semantics. They do
 * not exercise concurrency but serve as a baseline harness for
 * verifying behaviour with a native C compiler. Run with `gcc
 * -std=c11 -Wall -Wextra -Werror -I../include test_memory.c ../src/aether_memory.c ../src/aether_platform.c -o test_memory`.
 */

#include <assert.h>
#include <stdio.h>

#include "aether_memory.h"

/* Test allocation and pointer retrieval. */
static void test_basic_alloc(void) {
    aether_mem_pool_t *pool = aether_mem_init(1024);
    assert(pool != NULL);
    /* Allocate 128 bytes */
    aether_handle_t h1 = aether_mem_alloc(pool, 128);
    assert(h1 >= 0);
    void *ptr1 = aether_mem_get_ptr(pool, h1);
    assert(ptr1 != NULL);
    /* Allocate another 64 bytes */
    aether_handle_t h2 = aether_mem_alloc(pool, 64);
    assert(h2 >= 0);
    void *ptr2 = aether_mem_get_ptr(pool, h2);
    assert(ptr2 != NULL);
    /* Ensure pointers are distinct */
    assert(ptr1 != ptr2);
    /* Free the first segment and allocate again; should reuse freed slot or bump */
    aether_mem_free(pool, h1);
    aether_handle_t h3 = aether_mem_alloc(pool, 32);
    assert(h3 >= 0);
    void *ptr3 = aether_mem_get_ptr(pool, h3);
    assert(ptr3 != NULL);
    /* Clean up */
    aether_mem_destroy(pool);
}

/* Test invalid handle cases */
static void test_invalid_handle(void) {
    aether_mem_pool_t *pool = aether_mem_init(256);
    assert(pool != NULL);
    /* Invalid handle should yield NULL */
    assert(aether_mem_get_ptr(pool, -1) == NULL);
    assert(aether_mem_get_ptr(pool, (aether_handle_t)AETHER_MAX_SEGMENTS) == NULL);
    /* Free invalid handle should do nothing */
    aether_mem_free(pool, -1);
    aether_mem_free(pool, (aether_handle_t)AETHER_MAX_SEGMENTS);
    aether_mem_destroy(pool);
}

int main(void) {
    test_basic_alloc();
    test_invalid_handle();
    printf("All memory allocator tests passed.\n");
    return 0;
}