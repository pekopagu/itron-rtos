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
| 4       | 4.2     | Entry return observation                   | v4.2-task-entry-return      | Completed |
| 4       | 4.3     | Cooperative execution control              | v4.3-task-cooperative-runner | Completed |
| 5       | 5.1     | Task stack foundation                      | v5.1-task-stack-foundation  | Completed |
| 5       | 5.2     | Register save area                         | v5.2-register-save-area     | Completed |
| 5       | 5.3     | Minimal context switch                     | v5.3-minimal-context-switch | Completed |

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

Chapter 4 Section 4.3 adds cooperative execution control as a boot-time
verification model:

* The kernel repeats a finite cooperative verification loop.
* Each iteration selects a READY task with `scheduler_select_next()`.
* The selected task is committed as current through `dispatcher_commit_current()`.
* The entry call target is the current task returned by `dispatcher_get_current()`.
* The current task entry is still called as a normal C function call.
* Entry return is observed as a cooperative return event, not formal task termination.
* The returned RUNNING task is moved back to READY as cooperative re-candidacy.
* The loop stops when the entry-call limit is reached or no READY task exists.

This is still not a real context switch. It does not switch stacks, save or
restore registers, use interrupts, use timers, or implement preemption.
It also does not add a `yield_tsk` compatible API, `TASK_STATE_EXITED`,
DORMANT transition, or task restart behavior.

Chapter 5 Section 5.1 adds task stack foundation metadata:

* Each sample task has its own static stack region.
* The TCB stores `stack_base`, `stack_size`, and `stack_top`.
* `stack_top` is derived from `stack_base + stack_size` for the current
  downward-growing x86_64 stack assumption.
* Registration logs and task dump logs show each task's stack metadata.
* `stack_top` is only a future initial stack pointer candidate.
* The kernel still does not load `stack_top` into CPU RSP.
* Task entries still run through the existing direct normal C function call
  model from Chapter 4.

Chapter 5 Section 5.2 adds a per-task register save area:

* `task_context_t` defines the prepared x86_64 register save area.
* The TCB stores `context` with `rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, and `r15`.
* `context.rsp` is initialized from `stack_top` as a future restore candidate.
* Other context fields are initialized to zero.
* Registration logs and task dump logs show each task's context metadata.
* This is still a save-area preparation step only.
* The kernel still does not save live CPU registers, restore CPU registers,
  switch stacks, or load `context.rsp` into CPU RSP.

Chapter 5 Section 5.3 adds a minimal explicit context switch smoke path:

* The boot path enters x86_64 long mode before `kernel_main`.
* `arch_context_switch()` saves and restores `rsp`, `rbp`, `rbx`, and
  `r12`-`r15`.
* The kernel prepares one initial task stack frame for the selected current
  task.
* The smoke path switches from the boot context to the task stack, runs the
  task entry once through a trampoline, observes entry return, and switches
  back to the boot context.
* Scheduler responsibility remains READY task selection.
* Dispatcher responsibility remains current commit.
* Timer interrupts, interrupt-handler switching, preemption, wait states,
  semaphores, timers, and a public yield API are still not implemented.

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

Chapter 4 Section 4.3 runs a finite cooperative verification loop. Each
iteration commits a scheduler-selected READY task as current, directly calls
the current task entry as a normal C function, observes the return as a
cooperative return event, and returns the task to READY as cooperative
re-candidacy. This is still not a real context switch, and the task does not
run on an independent task stack.

Chapter 5 Section 5.1 registers stack foundation metadata for each sample
task. The serial log shows `stack_base`, `stack_size`, and `stack_top` for
each task, and the stack ranges are separate. This is still metadata only:
the kernel does not switch stacks or load `stack_top` into CPU RSP.

Chapter 5 Section 5.2 registers a prepared register save area for each task.
The serial log shows `context.rsp`, `context.rbp`, `context.rbx`,
`context.r12`, `context.r13`, `context.r14`, and `context.r15`. `context.rsp`
matches the task's `stack_top`, but it is still metadata only and is not loaded
into CPU RSP.

Chapter 5 Section 5.3 adds a separate minimal context switch smoke path. The
image is linked as an x86_64 kernel and converted to a 32-bit ELF container so
QEMU's Multiboot `-kernel` loader can still load it. The boot stub then enters
long mode before calling `kernel_main`.

The smoke path logs the selected task, dispatcher commit, prepared task stack
frame, the switch from boot context to task context, task entry execution on
the prepared task stack, and the switch back to boot context. `RUNNING` is
still a logical current-adopted state managed by the dispatcher and task module;
this smoke does not introduce preemptive CPU ownership or a complete task
lifecycle.

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
[task] registered: id=1 name=task_a state=READY prio=5 entry=0x... stack_base=0x... stack_size=1024 stack_top=0x... context.rsp=0x... context.rbp=0x0 context.rbx=0x0 context.r12=0x0 context.r13=0x0 context.r14=0x0 context.r15=0x0
[kernel] task_register task_a returned 1
[task] registered: id=2 name=task_b state=READY prio=1 entry=0x... stack_base=0x... stack_size=1024 stack_top=0x... context.rsp=0x... context.rbp=0x0 context.rbx=0x0 context.r12=0x0 context.r13=0x0 context.r14=0x0 context.r15=0x0
[kernel] task_register task_b returned 2
[task] registered: id=3 name=task_c state=READY prio=1 entry=0x... stack_base=0x... stack_size=1024 stack_top=0x... context.rsp=0x... context.rbp=0x0 context.rbx=0x0 context.r12=0x0 context.r13=0x0 context.r14=0x0 context.r15=0x0
[kernel] task_register task_c returned 3
[task] dump start
[task] id=1 name=task_a prio=5 state=READY entry=0x... stack_base=0x... stack_size=1024 stack_top=0x... context.rsp=0x... context.rbp=0x0 context.rbx=0x0 context.r12=0x0 context.r13=0x0 context.r14=0x0 context.r15=0x0
[task] id=2 name=task_b prio=1 state=READY entry=0x... stack_base=0x... stack_size=1024 stack_top=0x... context.rsp=0x... context.rbp=0x0 context.rbx=0x0 context.r12=0x0 context.r13=0x0 context.r14=0x0 context.r15=0x0
[task] id=3 name=task_c prio=1 state=READY entry=0x... stack_base=0x... stack_size=1024 stack_top=0x... context.rsp=0x... context.rbp=0x0 context.rbx=0x0 context.r12=0x0 context.r13=0x0 context.r14=0x0 context.r15=0x0
[task] dump end
[scheduler] after_register selected: id=2 name=task_b prio=1 state=READY
[context-smoke] begin
[dispatcher] committed current: id=2 name=task_b prio=1 state=RUNNING
[context] prepared initial frame: task id=2 name=task_b context.rsp=0x...
[context] switch begin: from=boot to id=2 name=task_b boot.rsp.before=0x0 to.rsp.restore=0x...
[context] entered task stack: task id=2 name=task_b current.rsp=0x...
[task_b] executed
[context] task entry returned: task id=2 name=task_b
[context] task ready after switch entry: result=0 task id=2 name=task_b
[context] switch back: from id=2 name=task_b to=boot from.rsp.before=0x... boot.rsp.restore=0x...
[context] switch resumed: task=boot boot.rsp.after=0x... from id=2 name=task_b from.rsp.saved=0x...
[context-smoke] end
[cooperative] iteration=1 begin
[scheduler] cooperative selected: id=2 name=task_b prio=1 state=READY
[dispatcher] committed current: id=2 name=task_b prio=1 state=RUNNING
[entry] calling current: id=2 name=task_b prio=1 state=RUNNING
[task_b] executed
[entry] returned current: id=2 name=task_b prio=1 state=RUNNING
[cooperative] returned current: id=2 name=task_b prio=1 state=RUNNING
[cooperative] ready again: result=ok id=2 name=task_b state=READY
[cooperative] iteration=2 begin
[scheduler] cooperative selected: id=2 name=task_b prio=1 state=READY
...
[cooperative] stop: reason=limit-reached
```

`task_b` is selected because it has the highest priority under the current rule:
a smaller numeric priority is higher. `task_b` and `task_c` have the same
priority, so the task table registration order selects `task_b` first. In
Chapter 4 Section 4.3, the cooperative runner returns `task_b` from RUNNING to
READY after each cooperative return event. Because the scheduler is still a
simple fixed-priority selector, it can select `task_b` again until the finite
entry-call limit is reached. `RUNNING` in this log is still a logical
current-task state. It does not mean CPU context restoration, stack switching,
or preemptive execution.

The `stack_top` values shown in Chapter 5 Section 5.1 are observable
foundation data only. They are not used as active CPU stack pointers yet.
Chapter 5 Section 5.2 additionally shows `context.rsp` values that correspond
to `stack_top`. These values are prepared register save-area metadata only;
the kernel still does not save or restore live CPU register values.

Chapter 5 Section 5.3 changes that last point for the explicit smoke path only:
the arch switch saves and restores the minimal callee-saved x86_64 context and
loads the prepared task `context.rsp`. The older cooperative runner remains as
a direct C-call verification model for comparison and is still not timer-driven
or preemptive.

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
│     ├─ context_switch.asm
│     ├─ context_switch.h
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
* Direct normal C function call of `current->entry()` for Chapter 4.1-4.3
* Cooperative return event observation for Chapter 4.3
* RUNNING to READY cooperative re-candidacy for temporary verification
* Finite cooperative entry-call limit
* Monotonically increasing task IDs
* Stack information storage (`stack_base`, `stack_size`, `stack_top`)
* Per-task static stack regions for boot-time observation
* Stack metadata logs for registration and dump output
* Per-task register save area (`task_context_t`) in the TCB
* Register save-area initialization from stack metadata
* Register save-area logs for registration and dump output
* x86_64 minimal context switch primitive
* Boot-context to task-context switch smoke
* Prepared initial task stack frame for the smoke path
* Boot-time task registration, dump, scheduler selection, and current commit confirmation

---

## Not Implemented Yet

The following features are intentionally not implemented yet:

* Full task-to-task execution by context switching
* RUNNING as CPU execution state
* Continuous independent task stack execution
* Timer interrupt
* Preemption
* `yield_tsk` compatible API
* Dynamic memory allocation
* Timer-driven stack switching
* Formal task termination state
* `TASK_STATE_EXITED`
* Task restart
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
* [x] Task stack foundation
* [x] Register save area
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
| 4       | 4.2     | Entry return observation                   | v4.2-task-entry-return      | Completed |
| 4       | 4.3     | Cooperative execution control              | v4.3-task-cooperative-runner | Completed |
| 5       | 5.1     | Task stack foundation                      | v5.1-task-stack-foundation  | Completed |
| 5       | 5.2     | Register save area                         | v5.2-register-save-area     | Completed |
| 5       | 5.3     | Minimal context switch                     | v5.3-minimal-context-switch | Completed |

---

## Author

GitHub: pekopagu
