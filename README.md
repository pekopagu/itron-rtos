# ITRON RTOS (itron-rtos)

AI-assisted ITRON-like RTOS for x86_64 + QEMU

---

## Overview

This project is an experimental Real-Time Operating System (RTOS) developed using AI coding agents.

* Target: x86_64 + QEMU
* Purpose: Educational / Experimental
* Style: ITRON-like (not fully compliant)

This project explores how far AI agents can assist in low-level system development.

---

## Purpose

This project is primarily intended to share development results and experiments.

It is **not intended for production use or active adoption**.

---

## Features (Planned)

* Bootloader & kernel startup
* Kernel entry (`kernel_main`)
* Task management (ITRON-like API)
* Scheduler (cooperative → preemptive)
* Synchronization (semaphore)
* Timer management

---

## Development Progress

The current implementation covers the following milestones:

* Part 4: Minimal x86_64 + QEMU boot path and `kernel_main`
* Part 5: Serial console API cleanup for COM1 output
* Part 6: HAL boundary for console output
* Part 7: Initial task management

Part 7 adds the first task-management layer:

* TCB (`Task Control Block`) definition
* `task_state_t` definition
* Static task table
* `task_init()`
* `task_register()`
* `task_dump()`

This is a registration and inspection layer only. Task execution, scheduler,
context switching, timer interrupts, and stack frame initialization are not
implemented yet.

---

## Development Environment

The project is developed using the following tools:

* OS:

  * Windows 11

* Toolchain:

  * LLVM / Clang
  * NASM
  * GNU Make

* Emulator:

  * QEMU (x86_64)

* Development Tools:

  * Git
  * Node.js / npm
  * Codex CLI
  * cc-sdd (Spec-Driven Development toolkit)
  * ChatGPT

This project is developed and tested on Windows 11.

---

## Tested Environment

This project has been tested in the following environment:

* OS:

  * Windows 11

* Architecture:

  * x86_64

* Toolchain:

  * clang / LLVM 22.x
  * NASM 3.x
  * GNU Make 3.81

* Emulator:

  * QEMU 11.x

* Other Tools:

  * Git
  * Node.js / npm
  * Codex CLI
  * cc-sdd
  * ChatGPT

※ Only the above environment is tested. Compatibility with other environments is not guaranteed.

---

## Build & Run

Part 4 provides the first minimal boot path.

The current image is a small Multiboot ELF that boots on `qemu-system-x86_64`
and reaches the freestanding C entry point `kernel_main`.

Console output is routed through a minimal HAL boundary. The common kernel code
includes `hal/console.h` and no longer includes `arch/x86_64/serial.h`
directly; the x86_64 HAL implementation delegates to the existing serial
driver.

The current runtime path is:

```text
kernel -> HAL -> arch(x86_64) -> serial -> COM1
```

Part 7 also registers sample tasks during boot and dumps the registered task
table to the serial log.

### Build

Run from Windows PowerShell:

```powershell
make
```

Expected output:

```text
build/kernel.elf
```

### Run on QEMU

Run from Windows PowerShell:

```powershell
make run
```

The run target starts QEMU briefly, captures serial output, and stores it in:

```text
docs/logs/qemu-serial.log
```

To inspect the serial console directly with QEMU stdio:

```powershell
qemu-system-x86_64 -kernel build/kernel.elf -serial stdio -display none -no-reboot
```

Successful output includes:

```text
itron-rtos booting...
kernel_main reached
[kernel] task init
[task] registered: id=1 name=task_a state=READY prio=1 ...
[kernel] task_register task_a returned 1
[task] registered: id=2 name=task_b state=READY prio=2 ...
[kernel] task_register task_b returned 2
[task] dump start
[task] id=1 name=task_a prio=1 state=READY ...
[task] id=2 name=task_b prio=2 state=READY ...
[task] dump end
```

The `task_a` and `task_b` entry functions are registered but are not executed.

### Clean

```powershell
make clean
```

---

## Directory Structure

```text
itron-rtos/
├─ README.md
├─ LICENSE
├─ boot/
├─ kernel/
│  ├─ kernel.c
│  ├─ task.c
│  └─ include/
│     ├─ task.h
│     └─ hal/
│        └─ console.h
├─ arch/
│  └─ x86_64/
│     ├─ hal_console.c
│     ├─ serial.c
│     └─ serial.h
├─ build/
├─ docs/
├─ .kiro/
├─ linker.ld
└─ Makefile
```

---

## Implemented Features

The current kernel includes:

* Minimal QEMU boot to `kernel_main`
* COM1 serial output for QEMU `-serial stdio`
* HAL console boundary
* TCB definition
* `task_state_t` definition
* Static `task_table` with `MAX_TASKS = 256`
* `task_init()`
* `task_register()`
* `task_dump()`
* Monotonically increasing task IDs
* Stack information storage (`stack_base`, `stack_size`)
* Boot-time task registration and dump confirmation

---

## Not Implemented Yet

The following features are intentionally not implemented yet:

* Task execution
* Scheduler
* Context switch
* Timer interrupt
* Dynamic memory allocation
* Stack frame initialization
* Interrupt-driven task management

---

## Documentation

Source files now include Doxygen-style comments for the current low-level
kernel, serial, HAL, and initial task-management APIs.

Doxygen generation tooling and a `Doxyfile` are not included yet. They are
planned for a future documentation step.

---

## Spec-Driven Development (SDD)

This project uses cc-sdd for Spec-Driven Development.

```text
spec/spec.md   : Requirements
spec/plan.md   : Design
spec/tasks.md  : Implementation tasks
```

Specifications are used as the single source of truth for development.

---

## Development Policy

* AI-assisted development (Codex + cc-sdd)
* Human-reviewed code before acceptance
* Focus on learning and experimentation
* Development process is documented in Zenn articles

---

## Contributing

Contributions are welcome, but this is a personal experimental project.

* Issues and bug reports are accepted
* Pull requests are welcome
* Responses and fixes are not guaranteed

---

## Support

This project is provided without official support.

* No guarantee of functionality
* No guarantee of bug fixes
* No guarantee of compatibility with different environments

Issues and questions may be submitted, but responses are not guaranteed.

---

## Safety Notice

This software is provided for educational and experimental purposes only.

Do not use this software in production or safety-critical systems.

The author is not responsible for any damages caused by this software.

---

## AI Notice

This project includes AI-assisted code generation using tools such as ChatGPT and Codex.

All generated code is reviewed by the maintainer before being included.

---

## Verification Policy

All checks are assisted by AI tools, but final verification and decisions are made by the project maintainer.

AI-generated results are not accepted as-is and are always reviewed and validated manually.

The maintainer is responsible for confirming:

* License compliance
* Absence of unintended external code
* Export control considerations
* Overall safety and correctness

---

## Export Control Notice

This project is intended for educational and experimental purposes only.

This project does not intentionally include:

* Cryptographic functionality
* Military-related features
* Dual-use technologies requiring export control assessment

The project does not perform formal export classification.

This notice does not constitute legal advice.

Users are responsible for complying with applicable laws and regulations in their jurisdiction when using, modifying, or redistributing this software.

---

## Publication Review Status

Publication-related checks are tracked as part of the development process.

The following items are currently prepared:

* [x] License policy defined
* [x] MIT License file added
* [x] Safety notice added
* [x] AI usage notice added
* [x] Verification policy defined
* [x] Export control notice added
* [x] `.gitignore` added
* [x] Development environment documented
* [x] Tested environment documented
* [ ] Source code implemented
* [x] Initial build verified
* [x] QEMU boot verified
* [ ] AI-generated code fully reviewed

Detailed review records will be maintained in `docs/` as development progresses.

---

## License

This project is licensed under the MIT License.

See the LICENSE file for details.

---

## Roadmap

* [x] Define project policy
* [x] Setup development environment
* [x] Define publication policy
* [x] Boot on QEMU
* [x] Kernel entry (`kernel_main`)
* [x] Initial task management
* [ ] Scheduler
* [ ] Semaphore
* [ ] Timer / interrupt

---

## Repository Status

This repository is currently private and under active development.

The source code will be made public after the initial working kernel is implemented (planned in Part 4).

---

## Zenn Articles

Development process and design decisions are documented in Zenn articles.

Articles and source code versions are linked by Git tags when tags are created.

| Part   | Topic                                      | Git Tag        | Status |
| ------ | ------------------------------------------ | -------------- | ------ |
| Part 1 | Project concept and target selection       | -                         | Draft  |
| Part 2 | Development environment setup              | -                         | Draft  |
| Part 3 | Publication policy and preparation         | v0.3.04-policy            | Ready  |
| Part 4 | Initial QEMU boot and `kernel_main`        | v0.4.00-boot              | Ready  |
| Part 5 | SPDX and serial console API cleanup        | v0.5.00-serial            | Ready  |
| Part 6 | HAL boundary for console output            | v0.6.00-hal               | Draft  |
| Part 7 | Initial task management                    | v0.6.00-task_management   | Draft  |

---

## Author

GitHub: pekopagu
