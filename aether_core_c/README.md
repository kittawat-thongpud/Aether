# Aether C Core (L0–L1)

This directory contains the foundational C implementation for the Aether‑Core
project. It covers the **L0 Platform Foundation** and **L1 Memory Core**
layers as described in the development plan. The design strictly follows
the [Aether Engineering & Programming Rules](../docs/Programming-Rule.md),
ensuring deterministic control flow, pre‑allocated memory, and
assertion‑driven safety.

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
    └── test_memory.c       # Basic unit tests for the allocator
```

## Building

To build the tests on a machine with a C11 compiler and standard
library support:

```sh
cd aether_core_c/tests
gcc -std=c11 -Wall -Wextra -Werror -I../include test_memory.c \
    ../src/aether_memory.c -o test_memory
./test_memory
```

The allocator uses `posix_memalign` where available to ensure
alignment. On platforms without POSIX support, it falls back to
`malloc`.

## Usage

```c
#include "aether_memory.h"

int main(void) {
    // Initialise a memory pool of 16 KiB
    aether_mem_pool_t *pool = aether_mem_init(16 * 1024);
    if (!pool) {
        return 1;
    }
    // Allocate 256 bytes
    aether_handle_t handle = aether_mem_alloc(pool, 256);
    void *ptr = aether_mem_get_ptr(pool, handle);
    // Use ptr...
    // Free segment
    aether_mem_free(pool, handle);
    // Destroy pool when done
    aether_mem_destroy(pool);
    return 0;
}
```

## Notes

- The allocator maintains a fixed number (`AETHER_MAX_SEGMENTS`) of
  metadata slots. This bound allows static analysis of control flow
  and avoids unbounded lists.
- Each allocation and free operation acquires a spinlock. Keep
  operations within the critical section brief to avoid contention.
- The current implementation reuses freed segments only if the freed
  slot has sufficient capacity. Further optimisation (e.g. best‑fit
  search) can be added while respecting fixed loop limits.