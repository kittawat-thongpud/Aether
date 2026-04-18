# Aether-core Architecture Specifications

## เป้าหมาย
เอกสารนี้กำหนดทิศทางสถาปัตยกรรมและลำดับการพัฒนา **โดยเริ่มจากภาษา C ก่อน** แล้วค่อยแตกชั้นความสามารถเป็น **L1** และ **L2** เพื่อให้ระบบมีแกนหลักที่เล็ก เร็ว และควบคุมได้ง่าย

## หลักการออกแบบ
1. **C-first Core**: ส่วนแกน (runtime, memory, scheduler, protocol parser) พัฒนาเป็น C ก่อน
2. **Layered Capability**: ขยายความสามารถผ่านเลเยอร์ L1 และ L2 หลังจากแกน C เสถียร
3. **Contract-driven**: ทุกเลเยอร์สื่อสารผ่าน API contract ที่ชัดเจน
4. **Testable by design**: กำหนด unit/integration test ตั้งแต่ phase C

## สถาปัตยกรรมระดับสูง

```text
+-------------------------------+
| L2: Intelligent Services      |
| - Advanced orchestration      |
| - Adaptive policy engine      |
+---------------^---------------+
                |
+---------------+---------------+
| L1: Application & Domain APIs |
| - Public API surface          |
| - Domain modules              |
+---------------^---------------+
                |
+---------------+---------------+
| C Core (Foundation)           |
| - Runtime loop                |
| - Memory/resource manager     |
| - Message/event bus           |
| - IO adapters                 |
+-------------------------------+
```

## แผนการพัฒนา (C ก่อน L1, L2)

### Phase C — Foundation (ต้องเสร็จก่อน)
**วัตถุประสงค์**
- สร้างแกนระบบที่เชื่อถือได้และประสิทธิภาพสูง

**ขอบเขตหลัก**
- Core runtime loop
- Memory model และ lifecycle management
- Event/message abstraction
- Error model + logging + metrics hook
- Config loader และ baseline CLI

**ผลลัพธ์ (Deliverables)**
- `core/` library ที่ build ผ่าน CI
- API contract ของ core (header + docs)
- Unit tests ครอบคลุมเส้นทางสำคัญ

**เกณฑ์ผ่าน (Exit Criteria)**
- ผ่าน unit test ≥ 85% สำหรับ core
- Latency baseline เป็นไปตามงบประมาณที่กำหนด
- ไม่มี critical memory leak ใน test suite

---

### Phase L1 — Service Layer
**เริ่มได้เมื่อ Phase C ผ่าน Exit Criteria เท่านั้น**

**วัตถุประสงค์**
- เปิดความสามารถให้โมดูลระดับแอปใช้งาน core ได้สะดวก

**ขอบเขตหลัก**
- Public service APIs
- Domain modules ชุดแรก
- Validation/security guard rails
- Integration tests กับ C core

**ผลลัพธ์**
- SDK/API สำหรับผู้ใช้ภายในทีม
- สคริปต์ deployment ขั้นต้น

---

### Phase L2 — Intelligence & Optimization
**เริ่มหลัง L1 เสถียรและผ่าน SLO ที่กำหนด**

**วัตถุประสงค์**
- เพิ่มความฉลาด/ความยืดหยุ่นและการปรับแต่งเชิงนโยบาย

**ขอบเขตหลัก**
- Adaptive orchestration
- Policy/strategy plugins
- Performance auto-tuning
- Advanced observability dashboard hooks

**ผลลัพธ์**
- ความสามารถขั้นสูงที่ไม่กระทบเสถียรภาพ core
- เอกสาร operational playbook

## Milestones แนะนำ
- **M0 (สัปดาห์ 1-2):** ตั้งโครง C core + CI + coding conventions
- **M1 (สัปดาห์ 3-6):** Runtime, memory, bus, tests
- **M2 (สัปดาห์ 7-8):** Hardening + benchmark + freeze API contract
- **M3 (สัปดาห์ 9-11):** เริ่ม L1 modules + integration path
- **M4 (สัปดาห์ 12+):** L2 capabilities ตามลำดับความสำคัญ

## ความเสี่ยงและการลดความเสี่ยง
- **ความเสี่ยง:** Core API เปลี่ยนบ่อย ทำให้ L1/L2 แตกหัก
  - **Mitigation:** ทำ API freeze ที่ปลาย Phase C
- **ความเสี่ยง:** ประสิทธิภาพต่ำกว่าคาด
  - **Mitigation:** benchmark ตั้งแต่ M1 และตั้ง perf budget ชัดเจน
- **ความเสี่ยง:** Scope creep ใน L2
  - **Mitigation:** ใช้ feature gating และ priority-based rollout

## Definition of Done (รวม)
- เอกสาร API/Architecture อัปเดตครบ
- Test + CI ผ่านตามเป้าหมายแต่ละ phase
- Observability ขั้นพื้นฐานพร้อมใช้งานก่อนขึ้น L1
- รีวิวความปลอดภัยขั้นต่ำ 1 รอบก่อนขึ้น L2
