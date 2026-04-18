/**
 * @file aether_memory.c
 * @brief Implementation of Aether‑Core shared memory allocator.
 *
 * The memory allocator manages a single contiguous memory pool from
 * which fixed‑size segments are carved out. It provides a simple
 * bump‑pointer allocation strategy with basic reuse of freed
 * segments. All allocations occur on top of a pre‑allocated pool
 * created during initialisation. No dynamic memory allocation is
 * performed in the fast path. Concurrency is protected using a
 * spinlock; callers are expected to limit time spent within the
 * allocator to avoid starving other threads.
 */

#include "aether_types.h"
#include "aether_platform.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/** Helper to align a size up to the next multiple of AETHER_ALIGNMENT. */
static inline size_t aether_align_up(size_t size) {
    return (size + (AETHER_ALIGNMENT - 1)) & ~(AETHER_ALIGNMENT - 1);
}

/** Initialise a memory pool.
 *
 * @param total_size The size of the pool in bytes. This value will be
 *        aligned up to AETHER_ALIGNMENT.
 * @return Pointer to an initialised pool on success, NULL on failure.
 */
aether_mem_pool_t *aether_mem_init(size_t total_size) {
    /* Allocate the pool structure */
    aether_mem_pool_t *pool = (aether_mem_pool_t *)malloc(sizeof(aether_mem_pool_t));
    if (pool == NULL) {
        return NULL;
    }
    memset(pool, 0, sizeof(*pool));

    /* Align the requested size */
    size_t aligned_size = aether_align_up(total_size);
    pool->total_size = aligned_size;

    /* Allocate the actual memory region with explicit alignment. */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
    pool->base = (uint8_t *)aligned_alloc(AETHER_ALIGNMENT, aligned_size);
    if (pool->base == NULL) {
        free(pool);
        return NULL;
    }
#else
    /* Fallback for older standards; alignment may be implementation-defined. */
    pool->base = (uint8_t *)malloc(aligned_size);
    if (pool->base == NULL) {
        free(pool);
        return NULL;
    }
#endif

    pool->free_offset = 0;
    /* Initialise segment table */
    for (size_t i = 0; i < AETHER_MAX_SEGMENTS; ++i) {
        pool->segments[i].in_use = 0;
        pool->segments[i].offset = 0;
        pool->segments[i].size = 0;
    }
    /* Initialise spinlock */
    aether_spinlock_init(&pool->lock);
    return pool;
}

/** Destroy a memory pool. All segments must be freed or discarded
 *  prior to calling this function. After destruction the pool
 *  pointer must not be used.
 *
 * @param pool Pointer to the pool to destroy.
 */
void aether_mem_destroy(aether_mem_pool_t *pool) {
    if (pool == NULL) {
        return;
    }
    /* Free the underlying memory */
    if (pool->base != NULL) {
        free(pool->base);
    }
    /* Destroy the pool structure */
    free(pool);
}

/** Internal helper to find a free segment index.
 *  Returns -1 if no free slot exists.
 */
static int find_free_segment(aether_mem_pool_t *pool) {
    for (int i = 0; i < AETHER_MAX_SEGMENTS; ++i) {
        if (!pool->segments[i].in_use) {
            return i;
        }
    }
    return -1;
}

/** Allocate a segment from the pool.
 *
 * The allocator attempts to reuse freed segments if a hole of
 * sufficient size exists. Otherwise it allocates from the bump
 * pointer. All returned segments are aligned to AETHER_ALIGNMENT.
 *
 * @param pool Pointer to an initialised memory pool.
 * @param size Size in bytes of the requested allocation.
 * @return A handle identifying the allocated segment, or -1 on failure.
 */
aether_handle_t aether_mem_alloc(aether_mem_pool_t *pool, size_t size) {
    assert(pool != NULL);
    if (size == 0) {
        return -1;
    }
    /* Align requested size */
    size_t aligned_size = aether_align_up(size);

    /* Acquire lock */
    aether_spinlock_lock(&pool->lock);
    aether_handle_t handle = -1;
    /* First, search for a freed segment that is large enough */
    for (int i = 0; i < AETHER_MAX_SEGMENTS; ++i) {
        if (pool->segments[i].in_use == 0 && pool->segments[i].size >= aligned_size) {
            /* Reuse this segment */
            pool->segments[i].in_use = 1;
            handle = (aether_handle_t)i;
            break;
        }
    }
    if (handle == -1) {
        /* Allocate new segment from the end of the pool */
        size_t offset = aether_align_up(pool->free_offset);
        if (offset + aligned_size > pool->total_size) {
            /* Out of memory */
            aether_spinlock_unlock(&pool->lock);
            return -1;
        }
        int slot = find_free_segment(pool);
        if (slot < 0) {
            /* No free metadata slot */
            aether_spinlock_unlock(&pool->lock);
            return -1;
        }
        pool->segments[slot].in_use = 1;
        pool->segments[slot].offset = offset;
        pool->segments[slot].size = aligned_size;
        pool->free_offset = offset + aligned_size;
        handle = (aether_handle_t)slot;
    }
    /* Release lock */
    aether_spinlock_unlock(&pool->lock);
    return handle;
}

/** Free a previously allocated segment. The segment is returned to
 *  the pool and may be reused for a future allocation. The memory
 *  contents are not cleared.
 *
 * @param pool Pointer to the memory pool.
 * @param handle Handle returned by aether_mem_alloc.
 */
void aether_mem_free(aether_mem_pool_t *pool, aether_handle_t handle) {
    assert(pool != NULL);
    if (handle < 0 || handle >= (aether_handle_t)AETHER_MAX_SEGMENTS) {
        return;
    }
    aether_spinlock_lock(&pool->lock);
    aether_segment_t *seg = &pool->segments[handle];
    if (seg->in_use) {
        seg->in_use = 0;
        /* Optionally shrink free_offset if this segment is at the end */
        if (seg->offset + seg->size == pool->free_offset) {
            /* Move free_offset back to start of this segment */
            pool->free_offset = seg->offset;
            seg->offset = 0;
            seg->size = 0;
        }
    }
    aether_spinlock_unlock(&pool->lock);
}

/** Retrieve a pointer to the memory associated with a handle.
 *  The caller must ensure that the handle refers to a valid allocated
 *  segment. If the handle is invalid or not in use, NULL is
 *  returned.
 *
 * @param pool Pointer to the memory pool.
 * @param handle Handle returned by aether_mem_alloc.
 * @return Pointer to the underlying memory, or NULL on error.
 */
void *aether_mem_get_ptr(aether_mem_pool_t *pool, aether_handle_t handle) {
    assert(pool != NULL);
    if (handle < 0 || handle >= (aether_handle_t)AETHER_MAX_SEGMENTS) {
        return NULL;
    }
    aether_segment_t *seg = &pool->segments[handle];
    if (!seg->in_use) {
        return NULL;
    }
    return pool->base + seg->offset;
}