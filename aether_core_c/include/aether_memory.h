/**
 * @file aether_memory.h
 * @brief Public API for Aether‑Core shared memory allocator.
 *
 * This header exposes the minimal set of functions required to
 * initialise and interact with the Aether memory pool. The
 * implementation resides in aether_memory.c. All functions adhere
 * strictly to deterministic control flow and avoid dynamic memory
 * allocation in the fast path to comply with the Aether programming
 * rules.
 */

#ifndef AETHER_MEMORY_H
#define AETHER_MEMORY_H

#include "aether_types.h"

/** Initialise a memory pool of the given total size.
 *
 * The total_size is rounded up to the next multiple of
 * AETHER_ALIGNMENT to ensure proper alignment. Memory for the pool
 * and the metadata structure is allocated during this call; no
 * allocations occur in the fast path. On success a pointer to a
 * aether_mem_pool_t is returned. On failure returns NULL.
 *
 * @param total_size The desired size of the memory pool in bytes.
 * @return Pointer to the initialised memory pool, or NULL on error.
 */
aether_mem_pool_t *aether_mem_init(size_t total_size);

/** Destroy a previously initialised memory pool.
 *
 * This releases both the memory pool and the metadata. All segments
 * must have been freed or abandoned prior to calling this function.
 * After destruction the pool pointer must not be used.
 *
 * @param pool Pointer to the memory pool.
 */
void aether_mem_destroy(aether_mem_pool_t *pool);

/** Allocate a segment from the pool.
 *
 * Allocates a segment of at least `size` bytes. The size will be
 * rounded up to the nearest multiple of AETHER_ALIGNMENT. A handle
 * identifying the segment is returned. On failure (due to lack of
 * space or metadata slots) returns -1.
 *
 * @param pool Pointer to the memory pool.
 * @param size Size in bytes of the requested allocation.
 * @return Handle identifying the segment, or -1 on failure.
 */
aether_handle_t aether_mem_alloc(aether_mem_pool_t *pool, size_t size);

/** Free a previously allocated segment.
 *
 * After freeing, the segment may be reused for a future allocation.
 * Freeing a segment that is already free or invalid has no effect.
 *
 * @param pool Pointer to the memory pool.
 * @param handle Handle returned by aether_mem_alloc.
 */
void aether_mem_free(aether_mem_pool_t *pool, aether_handle_t handle);

/** Obtain a pointer to the memory associated with a segment.
 *
 * The caller must ensure that the handle refers to a valid in‑use
 * segment. If the handle is invalid or refers to a free segment,
 * NULL is returned.
 *
 * @param pool Pointer to the memory pool.
 * @param handle Handle returned by aether_mem_alloc.
 * @return Pointer to the memory backing the segment, or NULL.
 */
void *aether_mem_get_ptr(aether_mem_pool_t *pool, aether_handle_t handle);

#endif /* AETHER_MEMORY_H */