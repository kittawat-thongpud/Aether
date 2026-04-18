/**
 * @file aether_types.h
 * @brief Core data types shared across Aether‑Core C modules.
 *
 * This header defines basic, portable types used throughout the core
 * implementation. Keeping a single header for type definitions helps
 * maintain a consistent ABI and makes it easy to evolve the system
 * without scattering fundamental definitions across multiple files.
 */

#ifndef AETHER_TYPES_H
#define AETHER_TYPES_H

#include <stddef.h>
#include <stdint.h>

/** Maximum number of memory segments the allocator can manage.
 *  This bound ensures deterministic behaviour and enforces fixed loop
 *  limits in accordance with the Power of Ten rules. Adjusting this
 *  value requires review of all allocation logic.
 */
#define AETHER_MAX_SEGMENTS 64

/** Alignment (in bytes) enforced for all allocations. The value must
 *  remain a power of two. Using 64 bytes aligns with typical cache
 *  line sizes on modern CPUs, reducing false sharing when crossing
 *  threads or cores.
 */
#define AETHER_ALIGNMENT 64

/** Opaque handle representing a memory allocation within the shared
 *  memory pool. A handle is an index into the segment table; clients
 *  should treat it as a token rather than a direct pointer.
 */
typedef int32_t aether_handle_t;

/** Structure describing a single segment in the memory pool. Members
 *  are marked volatile and guarded by atomic operations to ensure
 *  visibility across threads. Offsets and sizes are measured in
 *  bytes relative to the base of the pool.
 */
/* Forward declare spinlock type from aether_platform.h to avoid
 * circular inclusion. We include the full definition in
 * aether_platform.h, so here we simply declare the struct to
 * reference it without exposing implementation details. */
struct aether_spinlock_s;
typedef struct aether_spinlock_s aether_spinlock_t;

typedef struct {
    uint8_t in_use;   /**< Non‑zero if this segment is allocated */
    size_t offset;    /**< Byte offset from the pool base */
    size_t size;      /**< Length of this segment in bytes */
} aether_segment_t;

/** Structure encapsulating the entire memory pool and allocator state.
 *  Clients initialise this once via aether_mem_init and subsequently
 *  pass the pointer to other API functions. The structure contains
 *  pre‑allocated segment metadata to avoid dynamic allocation in the
 *  fast path.
 */
typedef struct {
    uint8_t *base;                       /**< Base pointer of the memory pool */
    size_t total_size;                   /**< Total size of the memory pool */
    aether_segment_t segments[AETHER_MAX_SEGMENTS]; /**< Segment metadata table */
    size_t free_offset;                  /**< Current free offset into the pool */
    aether_spinlock_t lock;              /**< Spinlock protecting allocator state */
} aether_mem_pool_t;

#endif /* AETHER_TYPES_H */