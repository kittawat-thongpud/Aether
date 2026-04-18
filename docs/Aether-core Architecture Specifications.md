# Aether-Core

## High-Performance Cross-Hardware & Data-Oriented Middleware

## 1. Introduction

Aether-Core is a next-generation middleware architecture designed to remove communication bottlenecks in robotics, AI, and high-frequency control systems. Its primary goal is to minimize cross-language and cross-device latency by abandoning traditional serialization-heavy approaches such as JSON and Protobuf for the hot path.

Instead, Aether-Core is built around:

* Zero-copy shared memory
* Data-Oriented Design (DOD)
* Distributed schema awareness
* Lightweight network transport using UUID-tagged raw payloads

The system is intended to serve as a high-performance backbone for robotics platforms, AI-enabled control systems, custom engines, and other latency-sensitive distributed applications.

---

## 2. Core Architectural Vision

Aether-Core is designed as a middleware and SDK core that enables:

* Cross-language interoperability without repeated serialization/deserialization
* Cross-host data exchange with minimal metadata overhead
* Direct memory-oriented access for control loops, AI pipelines, and sensor fusion systems
* Decentralized operation without a single point of failure

The architecture treats memory layout, schema identity, and transport behavior as first-class design concerns.

---

## 3. Architectural Pillars

### 3.1 Zero-Copy Data Plane

Aether-Core uses shared memory as the primary local data plane.

#### Principles

* Data is laid out in a columnar or Struct-of-Arrays form inspired by Apache Arrow.
* Clients do not copy payloads into local objects when unnecessary.
* Language bindings wrap memory directly using offsets, descriptors, and schema contracts.

#### Benefits

* O(1) handoff cost for large payloads such as images, tensors, and point clouds
* Improved cache locality due to contiguous data layout
* Reduced pressure on allocators and GC-managed runtimes

### 3.2 Distributed Shared Memory for Metadata

Each host maintains a local schema registry inside shared memory.

#### Principles

* Topic schema identity is tracked with UUID rather than iteration number.
* Local lookups happen in shared memory with nanosecond-scale access characteristics.
* Network metadata sync is separated from local data access.

#### Benefits

* No dependence on a central registry
* Low-latency schema verification before runtime transactions
* Safer version management in decentralized systems

### 3.3 Zenoh as a Transparent Transport Gateway

Zenoh is used as the default network bridge between hosts.

#### Principles

* The network path remains transport-oriented, not object-oriented.
* Packets carry only:

  * UUID
  * raw bytes
* Schema is synchronized independently through the metadata plane.

#### Benefits

* Small wire overhead
* Transport independence
* Suitable for LAN and Wi-Fi environments without specialized network hardware

---

## 4. Data Model and Contracts

### 4.1 Node

A Node is an application or client identity, such as:

* `vision_processor`
* `motor_controller`
* `planner`

Node identity is used for:

* access control
* diagnostics
* tracing and monitoring

### 4.2 Topic

A Topic is a named data contract representing a stream or logical data endpoint.

Examples:

* `/camera/front/image`
* `/motor/state`
* `/robot/pose`

A Topic maps to:

* a schema UUID
* a memory descriptor
* a publication or subscription role

### 4.3 Transaction

A Transaction is the actual runtime read or write performed after a Node has bound to a Topic successfully.

---

## 5. Initialization and Lifecycle Management

### 5.1 Read-Before-Register Rule

A Node must always query the local data plane before attempting to register a Topic.

#### Discovery logic

When a Node starts:

1. Query local shared memory for the Topic name.
2. If found, inspect schema UUID.
3. Decide one of the following actions:

   * UUID matches → bind immediately
   * UUID mismatches → enter alias resolution
   * Topic missing → attempt atomic registration

This avoids redundant registrations and accelerates boot behavior.

### 5.2 Memory Binding

Once schema compatibility is confirmed, the data plane returns a memory descriptor.

The descriptor may include:

* segment ID
* memory offset
* size
* stride or column metadata
* schema UUID
* access mode

The Node maps the memory once during initialization. Runtime transactions then execute without repeated lookup.

### 5.3 Schema Pinning

After a Node binds to a Topic version, it continues using that schema version until:

* its lifecycle ends
* a watchdog requests re-initialization
* the application explicitly rebinds

This prevents unexpected runtime crashes caused by mid-stream schema mutation.

### 5.4 Lazy Sync

Cross-host schema synchronization occurs only when a host actually needs a Topic. This reduces unnecessary metadata chatter and saves bandwidth.

---

## 6. Concurrency and Atomic Registration

### 6.1 Registration Lock

Each local data plane exposes an atomic registration state.

When a Node begins schema registration:

* the Topic state becomes `PENDING_REGISTRATION`
* other Nodes attempting the same operation receive `RETRY_LATER`
* retrying Nodes use exponential backoff with jitter

### 6.2 Double-Check Locking

Before final commit, the registering Node rechecks the registry to verify that another schema UUID has not been committed moments earlier.

If a newer valid entry already exists:

* cancel current registration
* restart discovery

### 6.3 Transactional Commitment

Schema metadata updates must be all-or-nothing.

If the system crashes mid-update:

* old metadata remains authoritative
* old UUID stays valid
* incomplete new metadata is never exposed as committed state

This ensures registry integrity and protects zero-copy readers from partially written schemas.

---

## 7. Conflict Resolution and Schema Safety

### 7.1 UUID as Source of Truth

Topic names are only lookup coordinates. The actual identity of a schema is its UUID.

Two Topics with the same path but different structure must never be treated as interchangeable.

Example:

* `/sensor/temp` with `float32`
* `/sensor/temp` with `float64`

These are different contracts and require distinct identities.

### 7.2 Aliasing Strategy

If a Topic name already exists but the UUID differs, the data plane isolates the new schema through aliasing.

#### Example

* Existing: `/motor/state`
* Conflicting schema: `/motor/state@abcd`

This prevents:

* unsafe memory interpretation
* accidental schema merging
* segmentation faults from invalid layout assumptions

### 7.3 Global Negotiation Model

The host data plane acts as the local negotiator.

Flow:

1. Check local shared memory.
2. Ask the network which schema is already associated with the name.
3. Resolve outcome:

   * none exists → claim primary name
   * same schema exists → join existing pool
   * different schema exists → force alias

### 7.4 Client-Side Verification

Before mapping data, the client checks:

* Topic name
* current UUID
* local schema registry compatibility

Result:

* Match → safe zero-copy access
* Mismatch → drop data immediately and raise incompatible-schema signal

### 7.5 Watchdog Diagnostics

Aether-Core should include a watchdog and diagnostic UI that exposes:

* active Topic table
* UUID mapping table
* alias indicators such as `@abcd`
* schema diffs between conflicting versions
* repeated mismatch events between Nodes

---

## 8. Communication Lanes and QoS Policies

To preserve determinism and reduce jitter, Aether-Core separates data flows by intent.

### 8.1 State Data Lane

Use for:

* sensor streams
* localization
* robot state
* perception outputs

#### Transport

* Zenoh pub/sub
* latency-first behavior

#### Policy

* `Drop if Unknown`
* if schema is not yet synchronized locally, discard the frame
* prioritize freshness over completeness

### 8.2 Command/Event Lane

Use for:

* actuator commands
* mission events
* control requests
* system-critical events

#### Transport

* reliable lane over Zenoh query/reliable mechanism
* acknowledgment-oriented behavior where required

#### Policy

* `Buffer & Wait`
* retain commands until schema is ready
* do not silently lose control messages

---

## 9. Embedded and Host Proxy Architecture

### 9.1 Host-Proxy Model

Resource-constrained MCUs should not carry heavy middleware logic.

Instead:

* MCU streams raw bytes prefixed with UUID
* Host data plane receives and validates payloads
* Host inserts data into shared memory
* Host republishes through the network transport as needed

### 9.2 MCU Responsibilities

An MCU such as STM32 should focus on:

* sensor acquisition
* DMA transfer
* deterministic timing
* minimal framing

The MCU does not need to:

* run a network discovery stack
* manage full schema registry behavior
* participate in complex control-plane negotiation

### 9.3 Host Responsibilities

The host-side bridge performs:

* DMA or serial ingress
* packet framing validation
* UUID lookup
* shared memory placement
* network publish
* drop policy enforcement for unknown schemas

---

## 10. Proof-of-Concept Strategy

### 10.1 Recommended Implementation Languages

| Role                     | Recommended Language | Reason                                          |
| ------------------------ | -------------------- | ----------------------------------------------- |
| Core data plane          | Rust                 | memory safety, systems performance, concurrency |
| High-performance clients | Rust / C++           | tight latency and deterministic access          |
| AI/ML clients            | Python               | rapid integration with PyArrow, NumPy, PyTorch  |
| Embedded producer        | C                    | low overhead and DMA-friendly                   |

### 10.2 PoC Phase 1

#### A. Local SHM Zero-Copy

* Producer in Rust writes columnar data into shared memory
* Consumer in Python maps memory directly using Arrow-compatible layout
* Measure local latency and copy avoidance

#### B. MCU to Host Bridge

* MCU sends `[UUID][payload]` through UART or SPI using DMA
* Host receives frame and places payload into shared memory
* Measure ingress latency and host bridge throughput

#### C. Host-to-Host Sync via Zenoh

* Host publishes UUID-tagged raw payloads
* Receiving host resolves schema from local registry
* Measure end-to-end latency and jitter

### 10.3 Performance Metrics

Track at minimum:

* MCU-to-host latency
* host-to-host end-to-end latency
* jitter distribution
* bridge throughput
* dropped-frame rate
* schema sync delay
* CPU usage on MCU
* CPU usage on host
* memory bandwidth pressure

---

## 11. Runtime Invariants

Aether-Core should preserve the following invariants:

1. A Node must read before attempting registration.
2. A Topic name is not trusted without UUID verification.
3. A bound Node does not silently switch schema version mid-lifecycle.
4. Partially registered metadata must never become visible as valid.
5. Unknown state data may be dropped, but unknown command data must not be silently discarded.
6. Zero-copy access is allowed only after schema compatibility is proven.
7. Schema conflicts are isolated, not auto-merged.

---

## 12. Key Advantages

### 12.1 Lowest-Latency Local Data Access

The hot path is reduced to memory mapping and direct access rather than repeated object parsing.

### 12.2 Decentralized Robustness

Every host owns its local data plane and schema registry, avoiding a single point of failure.

### 12.3 Cross-Language Efficiency

Rust, Python, C, and C++ can share a contract based on memory layout rather than expensive per-language serialization.

### 12.4 Network Efficiency

Only UUID and raw payload move on the network fast path, minimizing metadata overhead.

### 12.5 Embedded Scalability

MCUs remain lightweight while hosts absorb coordination and transport responsibilities.

---

## 13. Best-Fit Use Cases

Aether-Core is especially suitable for:

* robotics and autonomous systems
* AI-assisted control loops
* perception-to-control pipelines
* high-frequency sensor meshes
* local distributed compute clusters
* custom game or simulation engines
* super apps with a native shared-runtime core

---

## 14. Recommended Next Specification Sections

To move from concept to implementable architecture, the next documents should define:

1. Shared memory segment format
2. Schema descriptor binary layout
3. Topic namespace rules
4. UUID generation and compatibility rules
5. Access control and permission model
6. Watchdog event taxonomy
7. Failure recovery and restart semantics
8. Buffer management and reclamation
9. Multi-writer policy per Topic
10. Benchmark methodology and target hardware matrix

---

## 15. Condensed Logic Tables

### 15.1 Initialization Outcomes

| SHM State                  | Node Action       | Result                     |
| -------------------------- | ----------------- | -------------------------- |
| Topic found, UUID match    | No register       | Bind immediately           |
| Topic found, UUID mismatch | Request alias     | Create isolated path       |
| Topic not found            | Atomic register   | Reserve and commit schema  |
| Topic pending registration | Backoff and retry | Wait for current operation |

### 15.2 Update Outcomes

| Situation                            | System Behavior           |
| ------------------------------------ | ------------------------- |
| New name, new schema                 | Register and publish UUID |
| Existing name, same schema           | Join existing pool        |
| Existing name, new schema            | Alias and alert watchdog  |
| Client requests incompatible version | Drop and mark mismatch    |

---

## 16. Summary

Aether-Core is a memory-first, transport-light middleware architecture for high-performance distributed systems. It replaces serialization-heavy communication with zero-copy local access, UUID-based schema identity, and lightweight raw-byte transport across hosts.

Its core idea is simple:

* memory is the real interface
* schema identity must be explicit
* transport should stay thin
* conflicts must be isolated safely
* hot paths must avoid parsing

With a careful implementation of shared memory layout, schema registry semantics, watchdog tooling, and benchmark-driven validation, Aether-Core can become a strong foundation for robotics, AI, and other latency-sensitive systems.

---

## 17. Dependency Layer Model for Development Planning

To keep Aether-Core scalable, verifiable, and easy to extend with plugins, the dependency graph should be constrained into strict layers. Dependencies must flow only downward. Upper layers may depend on lower layers, but lower layers must never import upper layers.

### 17.1 Layer 0 — Platform Foundation

**Purpose**

* OS abstraction
* process primitives
* synchronization primitives
* atomics
* time source
* memory mapping
* file descriptors
* shared memory handles

**Examples**

* mmap / shm_open wrappers
* mutex and lock-free atomics
* clock and monotonic timer API
* CPU affinity and cache-line helpers

**Rules**

* no topic logic
* no network logic
* no schema logic
* no plugin awareness

### 17.2 Layer 1 — Memory Core

**Purpose**

* shared memory allocator
* segment manager
* buffer lifecycle
* memory descriptor table
* alignment and ownership model

**Examples**

* segment header
* slab or arena allocator
* ring buffer primitives
* columnar block layout

**Rules**

* depends only on Layer 0
* no transport awareness
* no CLI
* no plugin API

### 17.3 Layer 2 — Schema and Registry Core

**Purpose**

* UUID identity
* schema descriptor format
* registry table
* compatibility checking
* alias generation

**Examples**

* UUID → schema map
* topic → UUID resolution
* schema hash and compatibility rules
* registration state machine

**Rules**

* depends on Layers 0–1
* no transport implementation details
* no diagnostics UI
* no application-specific logic

### 17.4 Layer 3 — Data Plane Runtime

**Purpose**

* binding nodes to topics
* publish/subscribe memory contracts
* transactional registration
* state transitions
* watchdog hooks

**Examples**

* read-before-register flow
* pending registration handling
* schema pinning
* topic ownership and access modes

**Rules**

* depends on Layers 0–2
* still no network-specific code in core logic
* runtime policy should be interface-based

### 17.5 Layer 4 — QoS and Messaging Semantics

**Purpose**

* lane model
* freshness vs reliability policy
* buffering rules
* retry, timeout, drop policy

**Examples**

* state lane: drop if unknown
* command lane: buffer and wait
* history depth
* durability mode
* priority classes

**Rules**

* depends on Layers 0–3
* must define policy as contracts, not transport-specific implementation
* should be reusable by CLI, bridge, and plugins

### 17.6 Layer 5 — Transport Adapters and Bridge Modules

**Purpose**

* network and device integration
* host-to-host sync
* MCU proxy bridge
* transport bindings

**Examples**

* Zenoh adapter
* UART/SPI DMA bridge
* local IPC bridge
* future RDMA or shared-fabric adapters

**Rules**

* depends on Layers 0–4
* each adapter isolated behind a common interface
* transport adapters must not own schema rules
* bridge modules request registry services from data plane, not reimplement them

### 17.7 Layer 6 — SDK and CLI Surface

**Purpose**

* user-facing API
* language bindings
* CLI tools
* QoS-configurable readers/writers
* developer workflows

**Examples**

* Rust SDK
* Python zero-copy wrapper
* CLI subscribe/read/write/inspect tools
* admin commands for topic and schema diagnostics

**Rules**

* depends on Layers 0–5
* no direct access to raw transport internals unless explicitly exposed through stable interfaces
* CLI should call the same public runtime API as SDKs

### 17.8 Layer 7 — Observability, Tooling, and Control Plane Extensions

**Purpose**

* diagnostics
* watchdog UI
* tracing
* metrics
* schema inspector
* benchmark harness

**Examples**

* conflict dashboard
* topic table monitor
* latency profiler
* registry visualizer
* replay and fault injection tools

**Rules**

* depends on public APIs from Layers 3–6
* must not bypass runtime invariants
* should remain removable without affecting hot-path correctness

### 17.9 Layer 8 — Plugin Modules

**Purpose**

* optional feature expansion
* domain-specific integrations
* non-core protocols
* special storage or compute modules

**Examples**

* ROS-like bridge plugin
* persistent log plugin
* tensor exporter plugin
* custom auth plugin
* external schema translator

**Rules**

* depends only on stable extension APIs
* must never depend on private internals of Layers 1–3
* unloadable or disableable without breaking core runtime

---

## 18. Core vs Plugin Boundary

A clean future-proof split is:

### Must stay in Core Center

* shared memory engine
* allocator and segment manager
* schema registry
* UUID compatibility rules
* topic lifecycle
* binding and transaction model
* QoS policy engine
* stable SDK interfaces

### Should be Plugin or Adapter

* Zenoh bridge implementation
* ROS-style CLI extensions
* protocol translators
* persistence sinks
* cloud sync
* visualization panels
* experimental transports

Rule of thumb:
If removal of the module should not break the local memory runtime, it should be a plugin.

---

## 19. Dependency Direction Rule

The entire codebase should follow this shape:

**Platform → Memory → Schema → Runtime → QoS → Bridge/Transport → SDK/CLI → Tooling/Plugins**

Never the reverse.

Forbidden examples:

* Memory core importing Zenoh adapter
* Schema registry importing CLI code
* Runtime importing UI dashboard code
* Plugin reaching into private allocator internals

---

## 20. Recommended Development Levels

To make implementation manageable, split development into module maturity levels.

### Level A — Critical Core

Must be correct before scale-up.

* Layer 0 Platform Foundation
* Layer 1 Memory Core
* Layer 2 Schema and Registry Core
* Layer 3 Data Plane Runtime

**Goal**

* correctness
* determinism
* recovery safety
* benchmarkable hot path

### Level B — Operational Core

Required for real multi-node operation.

* Layer 4 QoS and Messaging Semantics
* Layer 5 Transport Adapters and Bridge Modules

**Goal**

* networked behavior
* device integration
* predictable runtime policy

### Level C — Developer Surface

Needed for adoption and daily engineering use.

* Layer 6 SDK and CLI Surface
* Layer 7 Observability and Tooling

**Goal**

* usability
* debugging
* maintainability
* operator visibility

### Level D — Ecosystem Expansion

Optional but important for long-term scale.

* Layer 8 Plugin Modules

**Goal**

* extensibility
* domain adaptation
* backward-compatible expansion

---

## 21. NASA/JPL Power-of-Ten Adaptation for Aether-Core Modules

To align with the JPL “Power of Ten” philosophy for safety-critical code, the core layers should adopt these practical module rules:

1. Core control flow must remain simple and explicit.
2. All loops in hot-path core code must have clear upper bounds.
3. No dynamic allocation in hot paths after runtime initialization.
4. Functions in Layers 0–3 should remain small and single-purpose.
5. Assertions must protect invariants such as UUID match, bounds, ownership, and state transitions.
6. Scope of mutable state must be minimized.
7. Every non-void return path must be checked in core and bridge code.
8. Preprocessor or macro metaprogramming should be minimal in critical layers.
9. Pointer usage must remain tightly controlled, especially in zero-copy boundaries.
10. Core code must compile cleanly with maximum warnings and pass static analysis continuously.

---

## 22. Suggested Repository Structure

A practical repository split could be:

* `aether-platform/` → Layer 0
* `aether-memory/` → Layer 1
* `aether-schema/` → Layer 2
* `aether-runtime/` → Layer 3
* `aether-qos/` → Layer 4
* `aether-bridge/` → Layer 5
* `aether-sdk-rust/` → Layer 6
* `aether-sdk-python/` → Layer 6
* `aether-cli/` → Layer 6
* `aether-observe/` → Layer 7
* `plugins/*` → Layer 8

This keeps the dependency graph visible in the package tree and makes violations easier to detect during review.

---

## 23. Development Sequence Recommendation

The safest order to build is:

1. Platform foundation
2. Memory core
3. Schema registry
4. Data plane runtime
5. QoS engine
6. MCU bridge and one network adapter
7. SDK and CLI
8. Watchdog and metrics
9. Plugin SDK
10. Optional ecosystem plugins

This order ensures that extension work starts only after the core memory contract is stable.

---

## 24. Development Roadmap — Block Diagram

```text
┌──────────────────────────────────────────────────────────────────────┐
│                           AETHER-CORE STACK                         │
├──────────────────────────────────────────────────────────────────────┤
│ Layer 8  Plugins / Extensions                                       │
│          ROS-like bridge, persistence, tensor export, auth, cloud   │
├──────────────────────────────────────────────────────────────────────┤
│ Layer 7  Observability / Tooling / Control Extensions               │
│          Watchdog, metrics, tracing, schema inspector, benchmarks   │
├──────────────────────────────────────────────────────────────────────┤
│ Layer 6  SDK / CLI Surface                                          │
│          Rust SDK, Python SDK, C API, QoS CLI, admin tools          │
├──────────────────────────────────────────────────────────────────────┤
│ Layer 5  Bridge / Transport Adapters                                │
│          Zenoh adapter, UART/SPI bridge, IPC bridge, MCU proxy      │
├──────────────────────────────────────────────────────────────────────┤
│ Layer 4  QoS / Messaging Semantics                                  │
│          state lane, command lane, buffering, retry, freshness      │
├──────────────────────────────────────────────────────────────────────┤
│ Layer 3  Data Plane Runtime                                         │
│          bind, register, lifecycle, transaction, watchdog hooks     │
├──────────────────────────────────────────────────────────────────────┤
│ Layer 2  Schema / Registry Core                                     │
│          UUID, topic mapping, compatibility, alias, registry state  │
├──────────────────────────────────────────────────────────────────────┤
│ Layer 1  Memory Core                                                │
│          SHM allocator, segment manager, descriptors, ring buffers  │
├──────────────────────────────────────────────────────────────────────┤
│ Layer 0  Platform Foundation                                        │
│          atomics, clock, mmap, sync, fd, affinity, cache helpers    │
└──────────────────────────────────────────────────────────────────────┘

Dependency Direction:
Layer 8 → 7 → 6 → 5 → 4 → 3 → 2 → 1 → 0
Allowed direction is downward only.
```

### 24.1 Core vs Expansion Diagram

```text
                      ┌─────────────────────────────┐
                      │      CORE CENTER            │
                      │    Layers 0 → 3             │
                      │ Platform / Memory /         │
                      │ Schema / Runtime            │
                      └─────────────┬───────────────┘
                                    │
                     stable public   │   stable policy/runtime API
                     low-level API   │
                                    ▼
                      ┌─────────────────────────────┐
                      │     OPERATIONAL CORE        │
                      │      Layers 4 → 5           │
                      │ QoS / Bridge / Transport    │
                      └─────────────┬───────────────┘
                                    │
                               public SDK API
                                    │
                                    ▼
                      ┌─────────────────────────────┐
                      │    DEVELOPER SURFACE        │
                      │      Layers 6 → 7           │
                      │ SDK / CLI / Observe         │
                      └─────────────┬───────────────┘
                                    │
                              extension API
                                    │
                                    ▼
                      ┌─────────────────────────────┐
                      │   ECOSYSTEM EXPANSION       │
                      │        Layer 8              │
                      │ Plugins / Domain Modules    │
                      └─────────────────────────────┘
```

---

## 25. Dependency Matrix

Legend:

* `Y` = allowed dependency
* `N` = forbidden dependency
* Rows = module using/importing
* Columns = module being used/imported

| From \ To       | L0 Platform | L1 Memory | L2 Schema | L3 Runtime | L4 QoS | L5 Bridge | L6 SDK/CLI | L7 Observe | L8 Plugin |
| --------------- | ----------: | --------: | --------: | ---------: | -----: | --------: | ---------: | ---------: | --------: |
| **L0 Platform** |           Y |         N |         N |          N |      N |         N |          N |          N |         N |
| **L1 Memory**   |           Y |         Y |         N |          N |      N |         N |          N |          N |         N |
| **L2 Schema**   |           Y |         Y |         Y |          N |      N |         N |          N |          N |         N |
| **L3 Runtime**  |           Y |         Y |         Y |          Y |      N |         N |          N |          N |         N |
| **L4 QoS**      |           Y |         Y |         Y |          Y |      Y |         N |          N |          N |         N |
| **L5 Bridge**   |           Y |         Y |         Y |          Y |      Y |         Y |          N |          N |         N |
| **L6 SDK/CLI**  |           Y |         Y |         Y |          Y |      Y |         Y |          Y |          N |         N |
| **L7 Observe**  |           N |         N |         Y |          Y |      Y |         Y |          Y |          Y |         N |
| **L8 Plugin**   |           N |         N |         N |          Y |      Y |         Y |          Y |          Y |         Y |

### 25.1 Reading the Matrix

* Layer 1 may use Layer 0 only.
* Layer 3 may use Layers 0–2, but must not import QoS, bridge, SDK, or tooling.
* Layer 5 may use core runtime and QoS contracts, but must not be depended on by the core.
* Layer 7 should observe through public APIs, not by reaching into allocator internals.
* Layer 8 plugins should depend on stable extension surfaces only, preferably Layer 6–7 APIs and selected Layer 3–5 contracts that are explicitly public.

### 25.2 Forbidden Shortcuts

The following shortcuts should be treated as architecture violations:

* Layer 1 importing Zenoh or transport code
* Layer 2 importing CLI parsing code
* Layer 3 importing dashboard or tracing UI
* Layer 8 importing private shared-memory allocator internals
* Layer 7 mutating runtime state through non-public hooks

---

## 26. Module Breakdown by Development Workstream

| Workstream           | Primary Layers | Main Deliverables                                             |
| -------------------- | -------------- | ------------------------------------------------------------- |
| Foundation           | L0             | platform abstraction, atomics, timer, shm primitives          |
| Memory Engine        | L1             | allocator, segment manager, descriptor table, ring buffer     |
| Schema System        | L2             | UUID rules, descriptor format, compatibility engine, aliasing |
| Runtime Engine       | L3             | bind/register lifecycle, transaction flow, ownership model    |
| Messaging Policy     | L4             | QoS contracts, lane types, retry/drop/buffer rules            |
| Bridge Integration   | L5             | Zenoh adapter, MCU host proxy, local IPC bridge               |
| Developer Interface  | L6             | SDKs, CLI, admin commands, config surface                     |
| Operations & Insight | L7             | watchdog, metrics, tracing, benchmark tools                   |
| Extension Ecosystem  | L8             | plugins, translators, storage sinks, domain modules           |

---

## 27. Development Roadmap by Phase

### Phase 0 — Architecture Freeze

**Goal**
Freeze the core contracts before coding spreads.

**Focus**

* shared memory binary layout
* descriptor format
* schema UUID policy
* topic naming and alias rules
* ownership and lifecycle rules
* dependency policy and repo boundaries

**Exit Criteria**

* architecture spec reviewed
* dependency matrix approved
* no unresolved core contract ambiguity

### Phase 1 — Core Foundation

**Layers**

* L0 Platform
* L1 Memory

**Build**

* platform abstraction crate/module
* shared memory segment model
* allocator strategy
* descriptor headers
* bounded queues/ring buffers
* alignment and cache rules

**Exit Criteria**

* SHM create/open/attach works
* bounded allocation works without runtime leaks
* hot-path allocation policy defined
* local microbenchmarks pass target

### Phase 2 — Schema and Runtime Kernel

**Layers**

* L2 Schema
* L3 Runtime

**Build**

* UUID descriptor registry
* topic resolution table
* compatibility checker
* alias generator
* registration lock state machine
* bind/read/write transaction model

**Exit Criteria**

* read-before-register implemented
* pending registration and retry work
* schema pinning works
* conflict isolation works without crash

### Phase 3 — QoS Semantics

**Layers**

* L4 QoS

**Build**

* lane model
* policy descriptors
* freshness rules
* reliable buffer rules
* timeout/retry behavior
* policy config schema

**Exit Criteria**

* state lane and command lane work as policy contracts
* no QoS logic duplicated in bridge or CLI
* policies testable without transport dependency

### Phase 4 — Bridge and Multi-Host Operation

**Layers**

* L5 Bridge

**Build**

* Zenoh adapter
* MCU UART/SPI DMA bridge
* host proxy loop
* sync handshake between hosts
* registry request/response flow

**Exit Criteria**

* host-to-host topic sync works
* MCU-to-host bridge works
* unknown-schema state frames drop correctly
* command/event path buffers correctly

### Phase 5 — SDK and CLI Enablement

**Layers**

* L6 SDK/CLI

**Build**

* Rust SDK
* Python zero-copy SDK
* CLI publish/subscribe/read/write/inspect
* QoS-configurable commands
* config loading and validation

**Exit Criteria**

* developers can publish and subscribe without internal APIs
* CLI covers common admin/debug workflows
* Python binding can read zero-copy-compatible memory safely

### Phase 6 — Observability and Diagnostics

**Layers**

* L7 Observe

**Build**

* watchdog event bus
* metrics and tracing
* schema inspector
* conflict table
* latency profiler
* benchmark and fault-injection tools

**Exit Criteria**

* alias conflicts visible
* schema mismatch visible
* latency path measurable end-to-end
* operators can diagnose runtime failures quickly

### Phase 7 — Plugin System and Ecosystem

**Layers**

* L8 Plugin

**Build**

* plugin API contract
* extension loading model
* version compatibility rules
* sample plugins
* integration test harness

**Exit Criteria**

* plugins load without touching core internals
* disabling a plugin does not affect core stability
* at least one reference plugin works end-to-end

---

## 28. Milestone View

```text
[Phase 0]
Architecture Freeze
   ↓
[Phase 1]
Platform + Memory Core
   ↓
[Phase 2]
Schema + Runtime Kernel
   ↓
[Phase 3]
QoS Policy Engine
   ↓
[Phase 4]
Bridge + Multi-Host Sync
   ↓
[Phase 5]
SDK + CLI Surface
   ↓
[Phase 6]
Observability + Diagnostics
   ↓
[Phase 7]
Plugin System + Ecosystem
```

### 28.1 Critical Path

The critical path for technical correctness is:

**Phase 0 → Phase 1 → Phase 2 → Phase 3 → Phase 4**

Until this path is stable, avoid deep investment in plugins or UI-heavy tooling.

---

## 29. Delivery Strategy by Team or Solo Development

### If built solo

Recommended execution order:

1. L0–L1
2. L2–L3
3. L4
4. one bridge only
5. one SDK only
6. CLI
7. observability
8. plugin API

### If built by multiple teams

Recommended split:

* Team A: L0–L1
* Team B: L2–L3
* Team C: L4–L5
* Team D: L6–L7
* Team E: L8 after API freeze

Constraint:
No team may bypass the approved dependency matrix.

---

## 30. Definition of Done by Layer

| Layer | Done Means                                                     |
| ----- | -------------------------------------------------------------- |
| L0    | stable OS abstraction, timing, sync, shm primitives validated  |
| L1    | deterministic SHM management and bounded memory behavior       |
| L2    | schema identity, compatibility, aliasing, registry correctness |
| L3    | reliable bind/register lifecycle and transaction invariants    |
| L4    | reusable QoS policy engine independent of transport            |
| L5    | working adapters without contaminating core dependencies       |
| L6    | usable SDK/CLI built only on public APIs                       |
| L7    | measurable, inspectable, diagnosable runtime behavior          |
| L8    | removable, versioned, future-proof extensions                  |

---

## 31. Architecture Governance Rules

To keep the roadmap healthy over time:

* every package declares allowed dependency targets
* CI checks for dependency rule violations
* core modules require stricter static analysis and review gates
* plugin API changes require compatibility review
* no feature may enter bridge or CLI by bypassing missing core abstractions

This prevents accidental inversion of control and keeps Aether-Core scalable as the system grows.
