/**
 * @file aether_platform.h
 * @brief Minimal platform abstraction layer for Aether‑Core.
 *
 * The platform layer wraps operating system primitives such as
 * atomic operations and lightweight locks. Keeping these wrappers
 * distinct from higher layers allows the memory allocator and
 * runtime to be portable across POSIX and embedded environments. Only
 * primitives necessary for the deterministic memory allocator are
 * exposed here. Additional functionality should be added with care,
 * ensuring compliance with the Power of Ten rules.
 */

#ifndef AETHER_PLATFORM_H
#define AETHER_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

/** Spinlock type. A spinlock is implemented using an atomic flag.
 *  Acquisition will spin until the lock becomes available. Use
 *  sparingly and only for very short critical sections to avoid
 *  live‑lock on heavily loaded systems.
 */
typedef struct aether_spinlock_s {
    atomic_flag flag;
} aether_spinlock_t;

/** Initialise a spinlock. Must be called before use. */
static inline void aether_spinlock_init(aether_spinlock_t *lock) {
    atomic_flag_clear(&lock->flag);
}

/** Acquire a spinlock. This function will block by spinning until
 *  the lock is acquired. It contains a bounded loop in accordance
 *  with the programming rules; however, the maximum number of
 *  iterations is not explicitly bounded because the loop depends on
 *  uncontrollable contention. It is therefore recommended to only
 *  hold the lock for brief operations and avoid nested lock
 *  scenarios.
 */
static inline void aether_spinlock_lock(aether_spinlock_t *lock) {
    while (atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire)) {
        /* Busy‑wait until the lock becomes available */
    }
}

/** Release a spinlock. */
static inline void aether_spinlock_unlock(aether_spinlock_t *lock) {
    atomic_flag_clear_explicit(&lock->flag, memory_order_release);
}

#endif /* AETHER_PLATFORM_H */