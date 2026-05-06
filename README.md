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

| Chapter | Section | Topic                                      | Tag                         | Status    |
| ------- | ------- | ------------------------------------------ | --------------------------- | --------- |
| 1       | 1.1     | Project concept and target selection       | -                           | Completed |
| 1       | 1.2     | Development environment setup              | -                           | Completed |
| 1       | 1.3     | Publication policy                         | v1.3-publication-policy     | Completed |
| 2       | 2.1     | Minimal x86_64 + QEMU boot path and `kernel_main` | v2.1-qemu-boot             | Completed |
| 2       | 2.2     | Serial console API cleanup for COM1 output | v2.2-serial-console         | Completed |
| 2       | 2.3     | HAL boundary for console output            | v2.3-hal-boundary           | Completed |
| 3       | 3.1     | Initial task management                    | v3.1-task-tcb               | Completed |
| 3       | 3.2     | Simple priority scheduler                  | v3.2-priority-scheduler     | Completed |
| 3       | 3.3     | Current task and RUNNING state             | v3.3-current-running        | Completed |
| 4       | 4.1     | Task entry handling                        | v4.1-task-entry-runner      | Completed |
| 4       | 4.2     | Entry return observation                   | v4.2-task-entry-return      | Planned   |

Chapter 3 Section 3.1 adds the first task-management layer:

* TCB (`Task Control Block`) definition
* `task_state_t` definition
* Static task table
* `task_init()`
* `task_register()`
* `task_dump()`

Chapter 3 Section 3.2 adds a simple priority scheduler:

* `scheduler_init()`
* `scheduler_select_next()`
* READY task selection
* Priority-based selection
* Lower numeric `priority` means higher priority
* Same-priority tasks are selected by task table registration order
* Boot-time scheduler selection logs
* NULL-safe kernel-side scheduler selection log

This is still a selection-only scheduler. Task execution, RUNNING transition,
context switching, stack switching, timer interrupts, and preemption are not
implemented yet.

Chapter 3 Section 3.3 adds the current task commit boundary:

* `scheduler_select_next()` still only selects a READY task
* `dispatcher_commit_current()` commits the selected task as the current task
* `RUNNING` means a logical state adopted as current, not CPU execution
* Task entry functions are still not called in the dispatcher
* Context switching and stack switching are still not performed
* Chapter 4 uses the committed current task as the input for entry handling

Chapter 4 Section 4.1 adds boot-time task entry handling:

* The committed current task entry is called once as a normal C function call
* The entry call target is the current task returned by `dispatcher_get_current()`, not the selected task directly
* Before calling entry, the kernel checks `current != NULL`, `state == TASK_STATE_RUNNING`, and `entry != NULL`
* Entry call, task entry body, and entry return are observable in the QEMU serial log
* After entry returns, the kernel does not create a formal task termination state and proceeds to the halt loop
* This is a boot-time verification model, not a real context switch

Chapter 4 Section 4.2 defines entry return observation in the current
boot-time verification model. At this stage, an entry return is not treated as
formal task termination. The current task is preserved, `TASK_STATE_RUNNING`
remains unchanged, and the scheduler is not re-run. This temporary behavior
will be replaced by a context-switch-based execution model in Chapter 5.

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

Only the above environment is tested. Compatibility with other environments is not guaranteed.

---

## Build & Run

Chapter 2 Section 2.1 provides the first minimal boot path.

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

Chapter 3 Section 3.1 registers sample tasks during boot and dumps the registered task table
to the serial log.

Chapter 3 Section 3.2 initializes the simple scheduler and logs which READY task would be
selected next. The selected task entry function is not called.

Chapter 3 Section 3.3 commits the selected task through the dispatcher boundary.
The committed task becomes the current task and transitions from READY to RUNNING
as a logical state. The dispatcher itself still does not execute the task entry
function, perform a context switch, or switch stacks.

Chapter 4 Section 4.1 directly calls the committed current task entry once.
This is still not a real context switch, and the task does not run on an
independent task stack.

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
[scheduler] before_register selected: none
[task] registered: id=1 name=task_a state=READY prio=5 ...
[kernel] task_register task_a returned 1
[task] registered: id=2 name=task_b state=READY prio=1 ...
[kernel] task_register task_b returned 2
[task] registered: id=3 name=task_c state=READY prio=1 ...
[kernel] task_register task_c returned 3
[task] dump start
[task] id=1 name=task_a prio=5 state=READY ...
[task] id=2 name=task_b prio=1 state=READY ...
[task] id=3 name=task_c prio=1 state=READY ...
[task] dump end
[scheduler] after_register selected: id=2 name=task_b prio=1 state=READY
[dispatcher] committed current: id=2 name=task_b prio=1 state=RUNNING
[task] dump start
[task] id=2 name=task_b prio=1 state=RUNNING ...
[task] dump end
[entry] calling current: id=2 name=task_b prio=1 state=RUNNING
[task_b] executed
[entry] returned current: id=2 name=task_b prio=1 state=RUNNING
```

`task_b` is selected because it has the highest priority under the current rule:
a smaller numeric priority is higher. `task_b` and `task_c` have the same
priority, so the task table registration order selects `task_b` first. The
dispatcher then commits the selected task as current, and Chapter 4 Section 4.1
calls `task_b` entry once as a normal C function. `RUNNING` in this log is still
a logical current-task state. It does not mean CPU context restoration, stack
switching, or preemptive execution.

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
│  ├─ scheduler.c
│  └─ include/
│     ├─ task.h
│     ├─ scheduler.h
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
* Simple priority scheduler
* `scheduler_init()`
* `scheduler_select_next()`
* READY task selection
* Priority-based selection
* Same-priority first-match selection
* Kernel-side scheduler selection log
* NULL-safe scheduler selection log
* Dispatcher current task boundary
* `dispatcher_init()`
* `dispatcher_commit_current()`
* `dispatcher_get_current()`
* Logical READY to RUNNING transition for the committed current task
* Boot-time current task entry execution model
* Current task entry precondition checks
* Entry call / entry return serial logs
* Direct normal C function call of `current->entry()` for Chapter 4.1
* Return-after-entry halt behavior for temporary verification
* Monotonically increasing task IDs
* Stack information storage (`stack_base`, `stack_size`)
* Boot-time task registration, dump, scheduler selection, and current commit confirmation

---

## Not Implemented Yet

The following features are intentionally not implemented yet:

* Real task execution by context switching
* RUNNING as CPU execution state
* Independent task stack execution
* Context switch
* Timer interrupt
* Preemption
* Dynamic memory allocation
* Stack switching
* Stack frame initialization
* Formal task termination state
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
* [x] Scheduler
* [x] Task entry handling
* [ ] Semaphore
* [ ] Timer / interrupt

---

## Repository Status

This repository is currently private and under active development.

The source code will be made public after the initial working kernel is implemented.

---

## Zenn Articles

Development process and design decisions are documented in Zenn articles.

Articles and source code versions are linked by Git tags when tags are created.

| Chapter | Section | Topic                                      | Git Tag                     | Status |
| ------- | ------- | ------------------------------------------ | --------------------------- | ------ |
| 1       | 1.1     | Project concept and target selection       | -                           | Draft  |
| 1       | 1.2     | Development environment setup              | -                           | Draft  |
| 1       | 1.3     | Publication policy                         | v1.3-publication-policy     | Ready  |
| 2       | 2.1     | Initial QEMU boot and `kernel_main`        | v2.1-qemu-boot              | Ready  |
| 2       | 2.2     | Serial console API cleanup                 | v2.2-serial-console         | Ready  |
| 2       | 2.3     | HAL boundary for console output            | v2.3-hal-boundary           | Draft  |
| 3       | 3.1     | Initial task management                    | v3.1-task-tcb               | Draft  |
| 3       | 3.2     | Simple priority scheduler                  | v3.2-priority-scheduler     | Draft  |
| 3       | 3.3     | Current task and RUNNING state             | v3.3-current-running        | Completed |
| 4       | 4.1     | Task entry handling                        | v4.1-task-entry-runner      | Completed |
| 4       | 4.2     | Entry return observation                   | v4.2-task-entry-return      | Planned |

---

## Author

GitHub: pekopagu
