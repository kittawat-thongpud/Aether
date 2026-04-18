# Aether Test Guide (Phase 0 → Phase 3)

คู่มือนี้สรุปวิธีเทสด้วยตัวเองตามสถานะโค้ดปัจจุบันของ repo นี้ โดยเน้น
**Phase 0 ถึง Phase 3** ตามสเปกสถาปัตยกรรม.

## 1) Prerequisites

- OS: Linux/macOS หรือ WSL
- Compiler: GCC ที่รองรับ C11

ตรวจสอบเวอร์ชัน:

```sh
gcc --version
```

Expected: มีข้อมูลเวอร์ชัน GCC และคำสั่งออกด้วย code 0

---

## 2) Phase 0 — Architecture Freeze Checks

จุดประสงค์: ยืนยันว่า contract หลักของ core ยัง compile ได้ครบและ deterministic constraints ยังอยู่.

### Command

```sh
cd aether_core_c/tests
gcc -std=c11 -Wall -Wextra -Werror -I../include \
    test_memory.c ../src/aether_memory.c ../src/aether_platform.c \
    -o test_memory
```

### Expected output

- ไม่มี warning/error
- ได้ binary `test_memory` ถูกสร้าง

---

## 3) Phase 1 — Core Foundation Tests (L0/L1)

จุดประสงค์: ทดสอบ allocator และ platform primitive ตามฟีเจอร์ที่ implement แล้ว.

### Command

```sh
cd aether_core_c/tests
./test_memory
```

### Expected output

```text
All Aether L0/L1 tests passed (allocator + platform primitives).
```

### สิ่งที่ test ครอบคลุม

- pool init และ alignment (`AETHER_ALIGNMENT`)
- allocation ปกติ + pointer ต้องไม่ชนกัน
- free/reuse behavior
- invalid handle behavior (`NULL` / no-op)
- memory exhaustion
- segment slot exhaustion (`AETHER_MAX_SEGMENTS`)
- tail-free แล้ว `free_offset` ลดลง
- spinlock primitive smoke test

---

## 4) Phase 2 — Schema/Runtime Readiness Checks (Current repo scope)

> หมายเหตุ: ใน repo ปัจจุบันยังไม่มี implementation ของ L2 Schema และ L3 Runtime เต็มรูปแบบ

สิ่งที่ตรวจได้ตอนนี้จาก L0/L1 invariant:

- deterministic failure path (`aether_mem_alloc` คืน `-1` เมื่อ exhausted)
- invalid access protection (`aether_mem_get_ptr` คืน `NULL` เมื่อ handle ไม่ถูกต้อง)

### Command (reuse suite)

```sh
cd aether_core_c/tests
./test_memory
```

### Expected output

เหมือน Phase 1 (ต้องผ่านครบ)

---

## 5) Phase 3 — QoS Semantics Readiness (Current repo scope)

> หมายเหตุ: QoS semantics เต็มรูปแบบยังต้องใช้ runtime/transport layer

สิ่งที่ประเมินได้ตอนนี้:

- allocator behavior ควร deterministic และ bounded
- no warnings build policy (`-Werror`) ผ่าน

### Command

```sh
cd aether_core_c/tests
gcc -std=c11 -Wall -Wextra -Werror -I../include \
    test_memory.c ../src/aether_memory.c ../src/aether_platform.c \
    -o test_memory && ./test_memory
```

### Expected output

- build ผ่านไม่มี warning
- runtime test ผ่านพร้อมข้อความ success เดิม

---

## 6) Quick one-shot command

```sh
cd aether_core_c/tests && \
gcc -std=c11 -Wall -Wextra -Werror -I../include \
    test_memory.c ../src/aether_memory.c ../src/aether_platform.c \
    -o test_memory && \
./test_memory
```

Expected final line:

```text
All Aether L0/L1 tests passed (allocator + platform primitives).
```

---

## 7) If test fails

- ถ้า compile fail: ตรวจ path include/source และ GCC C11 support
- ถ้า assertion fail: ดูว่ามีการแก้ `AETHER_MAX_SEGMENTS`, alignment, หรือ allocator logic ล่าสุดหรือไม่
- ถ้ารันใน environment จำกัด: ตรวจสิทธิ์ execute (`chmod +x test_memory` กรณีพิเศษ)
