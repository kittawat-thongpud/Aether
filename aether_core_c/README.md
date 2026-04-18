# Aether C Core (L0–L1)

This directory contains the foundational C implementation for the Aether‑Core
project. It currently implements and validates:

- **L0 Platform Foundation** (spinlock + atomic wrapper primitives)
- **L1 Memory Core** (deterministic shared-memory allocator)

The design follows the [Aether Engineering & Programming Rules](../docs/Programming-Rule.md),
with deterministic control flow, bounded metadata (`AETHER_MAX_SEGMENTS`), and
assertion-driven safety checks.

## Structure

```text
aether_core_c/
├── include/
│   ├── aether_types.h      # Fundamental type definitions and constants
│   ├── aether_platform.h   # Platform abstraction (spinlocks, atomics)
│   └── aether_memory.h     # Public API for the shared memory allocator
├── src/
│   ├── aether_memory.c     # Implementation of the memory allocator
│   └── aether_platform.c   # Stub for future platform‑specific code
└── tests/
    └── test_memory.c       # Unit/invariant tests for L0 + L1
```

## Build and Run Tests

```sh
cd aether_core_c/tests
gcc -std=c11 -Wall -Wextra -Werror -I../include test_memory.c \
    ../src/aether_memory.c ../src/aether_platform.c -o test_memory
./test_memory
```

Expected output:

```text
All Aether L0/L1 tests passed (allocator + platform primitives).
```

## Phase 0 → Phase 3 Test Matrix

The architecture roadmap defines incremental phases; the current C core can
verify Phase 0/1 fully and Phase 2/3 readiness partially via invariants.

### Phase 0 — Architecture Freeze (contract-level checks)
- Header/API compile checks (`aether_types.h`, `aether_memory.h`, `aether_platform.h`)
- Deterministic allocator constraints (`AETHER_MAX_SEGMENTS`, alignment)

### Phase 1 — Core Foundation (L0 + L1)
- Spinlock primitive smoke test
- Allocator alignment, allocation/free, reuse behavior, memory exhaustion
- Metadata slot bound exhaustion behavior (`AETHER_MAX_SEGMENTS` limit)

### Phase 2 — Schema and Runtime Kernel (L2 + L3 prep)
- Precondition checks already testable:
  - invalid handle rejection
  - deterministic failure behavior (`-1`/`NULL` on invalid or exhausted state)
- Full L2/L3 functional tests require runtime/schema modules (not yet present in this directory)

### Phase 3 — QoS Semantics (prep)
- Baseline determinism checks in allocator form foundation for future QoS timing tests
- Full QoS tests are pending until runtime + transport layers exist

## Usage

```c
#include "aether_memory.h"

int main(void) {
    aether_mem_pool_t *pool = aether_mem_init(16 * 1024);
    if (!pool) {
        return 1;
    }

    aether_handle_t handle = aether_mem_alloc(pool, 256);
    void *ptr = aether_mem_get_ptr(pool, handle);

    (void)ptr; /* Use pointer safely under agreed schema contract */

    aether_mem_free(pool, handle);
    aether_mem_destroy(pool);
    return 0;
}
```

## Notes

- Allocator metadata is bounded by `AETHER_MAX_SEGMENTS` for deterministic loops.
- Allocation and free operations are protected by a spinlock.
- Freed segments are reusable if capacity fits; tail free can shrink `free_offset`.
- L2/L3 modules (schema/runtime) should add dedicated tests under `tests/` once implemented.
