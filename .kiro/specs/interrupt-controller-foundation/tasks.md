# Implementation Plan

- [x] 1. PIC arch module foundation
- [x] 1.1 Add the x86_64 PIC initialization and mask-control boundary
  - Provide the legacy PIC initialization capability described in the design, including remap to vector bases 32 and 40.
  - Keep the initial IRQ state fully masked after initialization.
  - Provide mask and unmask operations that update only the requested IRQ line and ignore out-of-range IRQ values.
  - Add Doxygen-style Japanese comments explaining purpose, assumptions, limitations, and that this is not yet timer interrupt execution.
  - Observable completion: the new arch boundary builds as part of the kernel image and exposes PIC initialization plus mask/unmask functions for later integration.
  - _Requirements: 1.1, 1.2, 2.1, 2.2, 3.1, 3.2, 3.3, 3.4, 4.1, 4.3, 5.1, 6.4_
  - _Boundary: x86_64 PIC_

- [x] 2. Kernel integration through the HAL boundary
- [x] 2.1 Add the kernel-facing interrupt controller initialization path
  - Extend the HAL interrupt boundary so kernel code can request interrupt controller initialization without including x86_64 PIC headers directly.
  - Call the new initialization path from `kernel_main` after IDT initialization and before existing smoke flows.
  - Preserve existing IDT/exception initialization behavior and validation exception mode.
  - Observable completion: normal boot logs show PIC initialization before task, timer, semaphore, preemption, context-switch, and cooperative verification logs.
  - _Requirements: 1.3, 4.2, 4.4, 5.2, 5.3, 6.2, 6.3_
  - _Boundary: HAL interrupt, kernel boot integration_
  - _Depends: 1.1_

- [x] 3. Documentation alignment
- [x] 3.1 Update README with Chapter 7.2 PIC foundation status
  - Add Chapter 7.2 to the development progress and Zenn article tables.
  - Explain why legacy PIC is adopted first for the learning-oriented QEMU path.
  - State that APIC, IOAPIC, and LAPIC are future extensions.
  - State that PIT, timer ISR, scheduler/dispatcher/preemption/context-switch integration, and interrupt-driven task switching remain out of scope.
  - Observable completion: README readers can distinguish Chapter 7.2 PIC routing preparation from Chapter 7.3 timer interrupt entry work.
  - _Requirements: 1.4, 2.3, 5.4, 6.3_
  - _Boundary: Documentation_
  - _Depends: 2.1_

- [x] 4. Build and smoke validation
- [x] 4.1 Validate build, QEMU boot, serial log, and boundary integrity
  - Run the canonical build command and confirm the kernel links with the new PIC module.
  - Run the canonical QEMU smoke command and confirm `docs/logs/qemu-serial.log` is generated.
  - Confirm the log includes PIC initialization begin/done lines and existing smoke evidence.
  - Confirm kernel common code does not directly include the x86_64 PIC header and PIC code does not call scheduler, dispatcher, timer tick, or context switch code.
  - Observable completion: validation evidence proves this feature prepares 7.3 without starting hardware interrupt driven task switching.
  - _Requirements: 1.1, 1.2, 1.3, 2.1, 2.2, 3.1, 4.1, 4.2, 5.1, 5.2, 5.3, 6.1, 6.2, 6.3, 6.4_
  - _Boundary: Validation_
  - _Depends: 1.1, 2.1, 3.1_

## Implementation Notes

- `make` と `make run` は成功し、`docs/logs/qemu-serial.log` に `[pic] init begin` と `[pic] init done: master_base=32 slave_base=40 irqs=masked` が既存 smoke flow の前に出ることを確認した。
