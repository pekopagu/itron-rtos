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

## v1.0 Scope

At Chapter 15 Section 15.3, this repository is tagged as `v1.0` for a
learning-oriented μITRON-like RTOS. This does not mean full μITRON compliance,
production readiness, or a complete preemptive RTOS. It means the main learning
path can now be followed from boot to the public API layer, shared error codes,
and the local API reference generation entry.

* x86_64 + QEMU boot path and `kernel_main`
* serial output and the early HAL boundary
* static task table, priority scheduler, and current RUNNING task model
* `RUNNING` / `READY` / `WAITING` / `DORMANT` task states
* task entry execution and task-entry return handling
* task stack storage, register save area, and context switch boundary
* timer tick, interrupt entry/exit responsibilities, and dispatch pending
* higher-priority preemption decision points
* semaphore wait queue and semaphore wakeup paths
* delay queue, timeout semaphore wait, and tick-reached READY return
* μITRON-like task, sleep/wakeup, semaphore, and delay API entries
* common `ER` return type and `E_OK` / `E_ID` / `E_PAR` / `E_CTX` /
  `E_OBJ` / `E_TMOUT` / `E_QOVR` error names

Chapter 14 organized the public μITRON-like API layer: `cre_tsk()` and
`sta_tsk()` separate task creation from task start; `slp_tsk()` and
`wup_tsk()` move sleep waiters between WAITING and READY; `wai_sem()`,
`sig_sem()`, `pol_sem()`, and `twai_sem()` provide the semaphore API layer; and
Chapter 14.4 made API results readable through shared error codes.

Chapter 15 Section 15.1 does not add kernel behavior. It aligns this README,
the articles under `articles/`, the Zenn Articles table, and the roadmap so the
v1.0-before milestone is visible from documentation alone.

Chapter 15 Section 15.2 also does not add kernel behavior. It adds the Doxygen
generation path so readers can use source code, Zenn articles, and generated API
reference pages together while preparing the repository for v1.0 documentation.

Chapter 15 Section 15.3 finalizes this point as `v1.0`. It is a release summary
and documentation boundary only; it does not add scheduler, dispatcher, task,
semaphore, delay queue, timer, interrupt, or API behavior.

---

## v1.0 Non-Goals

The `v1.0` tag intentionally does not claim:

* Full μITRON compatibility
* Production-ready RTOS behavior
* Complete interrupt-return-frame based preemptive context switching
* Full timer interrupt subsystem
* SMP, APIC/IOAPIC/LAPIC support, or production IRQ routing
* Priority inheritance, mutexes, event flags, mailboxes, or full wait-object APIs
* Formal conformance tests against an official μITRON specification
* Public hosting of generated Doxygen HTML

Generated Doxygen output remains local-only. The repository keeps `Doxyfile`
and the source comments, but does not commit or publish `docs/doxygen/`.

---

## v1.0 Validation

The `v1.0` boundary is validated with:

```sh
make
make run
make run VALIDATE_TIMER_IRQ_ENTRY=1
doxygen Doxyfile
```

These checks confirm that the kernel still builds, the QEMU smoke path boots,
the timer IRQ entry validation path reaches the expected observation flow, and
the local Doxygen API reference can be regenerated.

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
| 6       | 6.1     | Semaphore foundation                       | v6.1-semaphore-foundation   | Completed |
| 6       | 6.2     | Timer foundation                           | v6.2-timer-foundation       | Completed |
| 6       | 6.3     | Preemption foundation                      | v6.3-preemption-foundation  | Completed |
| 7       | 7.1     | Interrupt and exception foundation         | v7.1-interrupt-exception-foundation | Completed |
| 7       | 7.2     | PIC interrupt controller foundation        | v7.2-interrupt-controller-foundation | Completed |
| 7       | 7.3     | Timer interrupt entry                      | v7.3-timer-interrupt-entry | Completed |
| 8       | 8.1     | Timer tick from hardware IRQ               | v8.1-timer-tick-from-hardware-irq | Completed |
| 8       | 8.2     | Timer IRQ preemption decision entry        | v8.2-timer-irq-preemption-decision | Completed |
| 8       | 8.3     | Timer IRQ dispatch pending observation     | v8.3-timer-irq-dispatch-pending-observation | Completed |
| 8       | 8.4     | Timer IRQ entry/exit responsibility        | v8.4-timer-irq-entry-exit-responsibility | Completed |
| 9       | 9.1     | Task-to-task context switch smoke          | v9.1-task-to-task-context-switch-smoke | Completed |
| 9       | 9.2     | Dispatcher switch boundary                 | v9.2-dispatcher-switch-boundary | Completed |
| 9       | 9.3     | Dispatcher state transition switch         | v9.3-dispatcher-state-transition-switch | Completed |
| 9       | 9.4     | Task entry return finalization             | v9.4-task-entry-return-finalization | Completed |
| 10      | 10.1    | yield_tsk API foundation                   | v10.1-yield-task-api-foundation | Completed |
| 10      | 10.2    | yield_tsk RUNNING to READY transition      | v10.2-yield-running-to-ready | Completed |
| 10      | 10.3    | yield_tsk next READY task selection        | v10.3-yield-select-next-task | Completed |
| 10      | 10.4    | yield_tsk connect to context switch        | v10.4-yield-connect-context-switch | Completed |
| 11      | 11.1    | Timer IRQ detect higher-priority READY     | v11.1-timer-irq-detect-higher-ready | Completed |
| 11      | 11.2    | Timer IRQ deferred dispatch switch         | v11.2-timer-irq-deferred-dispatch-switch | Completed |
| 11      | 11.3    | Same-priority READY is not a time slice target | v11.3-same-priority-not-timeslice-target | Completed |
| 11      | 11.4    | Preemption event log stabilization         | v11.4-preemption-event-log-stabilization | Completed |
| 12      | 12.1    | wai_sem RUNNING to WAITING transition      | v12.1-wai-sem-running-to-waiting | Completed |
| 12      | 12.2    | sig_sem waiting task to READY transition   | v12.2-sig-sem-waiting-to-ready | Completed |
| 12      | 12.3    | semaphore FIFO wait queue                  | v12.3-semaphore-wait-queue | Completed |
| 12      | 12.4    | semaphore wakeup preemption decision       | v12.4-semaphore-wakeup-preemption | Completed |
| 13      | 13.1    | dly_tsk delay task API foundation          | v13.1-delay-task-api-foundation | Completed |
| 13      | 13.2    | sleep/delay queue                          | v13.2-sleep-delay-queue | Completed |
| 13      | 13.3    | twai_sem timeout semaphore wait            | v13.3-twai-sem-timeout-wait | Completed |
| 13      | 13.4    | tick ready wakeup                          | v13.4-tick-ready-wakeup | Completed |
| 14      | 14.1    | cre_tsk / sta_tsk create-start task API    | v14.1-create-start-task-api | Completed |
| 14      | 14.2    | slp_tsk / wup_tsk sleep-wakeup task API    | v14.2-sleep-wakeup-task-api | Completed |
| 14      | 14.3    | semaphore API layer                        | v14.3-semaphore-api-layer | Completed |
| 14      | 14.4    | μITRON-like common error code system       | v14.4-itron-error-code-system | Completed |
| 15      | 15.1    | README and Zenn milestone summary          | v15.1-readme-zenn-milestone-summary | Completed |
| 15      | 15.2    | Doxygen documentation foundation           | v15.2-doxygen-documentation-foundation | Completed |
| 15      | 15.3    | v1.0 release summary                       | v1.0                       | Completed |

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

Chapter 6 Section 6.1 adds the semaphore foundation:

* `semaphore_t` stores `id`, `name`, `count`, and `max_count`.
* Semaphores are managed in a static semaphore table.
* `sem_create()` initializes a semaphore and logs its initial state.
* `wai_sem()` decrements `count` when a resource is available.
* When `count == 0`, `wai_sem()` moves the target task to `WAITING`.
* The TCB stores `wait_sem_id` so semaphore wait state can be observed.
* `sig_sem()` wakes one waiting task as a minimal boot-time model.
* `sem_dump()` logs semaphore `count` and `max_count`.
* This does not implement timer integration, timeout waits, preemption,
  interrupts, FIFO/priority wait queues, mutexes, or full μITRON compatibility.

Chapter 6 Section 6.2 adds the timer foundation:

* `timer_init()` initializes the RTOS-internal system tick to 0.
* `timer_tick()` advances the system tick by one explicit call.
* `timer_get_ticks()` returns the current system tick without changing state.
* The boot-time timer smoke calls `timer_tick()` explicitly and logs tick
  growth to the QEMU serial log.
* `timer_tick()` is shaped so a future timer interrupt handler can call it,
  but this chapter does not connect it to a hardware timer interrupt.
* This is not preemption. It does not implement PIT/APIC/HPET setup,
  interrupt-handler context switching, time slice, `dly_tsk`, timeout waits,
  sleep/delay queues, ready queues, round-robin, or μITRON-compatible timer APIs.

Chapter 6 Section 6.3 adds the preemption decision foundation:

* `scheduler_select_preemption_candidate()` compares the logical current task
  with the highest-priority READY task.
* A READY task becomes a switch target only when its numeric priority is lower
  than the current task's priority.
* Equal-priority READY tasks are not preemption targets. This chapter does not
  introduce time slicing.
* The scheduler returns a decision value only. It does not commit a new current
  task, update task states, or call the context switch layer.
* The boot-time smoke advances the timer tick first, then asks the scheduler
  for a preemption decision through the kernel verification flow.
* `[preempt]` logs show no-current, switch-target, and no-switch cases.
* This is still not complete interrupt-driven preemption. It does not implement
  interrupt nesting, SMP, priority inheritance, tickless timer, advanced
  interrupt masking, user mode, or ITRON-compatible APIs.

Chapter 7 Section 7.1 adds the interrupt and CPU exception foundation:

* `hal_interrupt_init()` is the kernel-facing HAL boundary for the exception
  foundation; the x86_64 HAL implementation delegates to `arch_interrupt_init()`.
* `arch_interrupt_init()` builds a minimal x86_64 IDT and loads it with `lidt`.
* The IDT uses the existing long-mode GDT code selector prepared by `boot/boot.asm`.
* Representative CPU exception entries are registered for observation.
* The common exception handler logs the vector, exception name, error code,
  RIP, CS, and RFLAGS through the HAL console.
* Normal boot logs IDT initialization and then continues the existing Chapter
  6.3 smoke flow.
* `make run VALIDATE_EXCEPTION=1` explicitly triggers `int3` and confirms
  handler arrival, then halts after the observation log.
* This is an educational observation handler, not a recoverable exception
  subsystem.
* This chapter still does not implement hardware timer interrupts, IRQ routing,
  PIC/APIC setup, scheduler calls, dispatcher calls, context switching from an
  interrupt handler, preemption, nested interrupt control, user mode, SMP, or
  ITRON-compatible interrupt APIs.

Chapter 7 Section 7.2 adds the interrupt controller foundation:

* The first interrupt controller target is the legacy PIC (8259A), because it
  keeps the early x86_64 + QEMU learning path observable through simple port
  I/O and explicit vector remapping.
* APIC, IOAPIC, and LAPIC are documented as future extensions, not implemented
  in this chapter.
* `hal_interrupt_controller_init()` is the kernel-facing HAL boundary for
  interrupt controller initialization; the x86_64 HAL implementation delegates
  to the arch-local PIC module.
* The PIC is remapped so IRQ0 starts at vector 32, leaving CPU exception
  vectors 0-31 reserved for exceptions.
* The PIC remains fully masked after initialization. No IRQ line is unmasked by
  the normal boot path.
* The boot log shows PIC initialization completion before the existing task,
  timer, semaphore, preemption-decision, context-switch, and cooperative smoke
  flows.
* At the Chapter 7.2 point, the project still did not implement PIT timer
  interrupt delivery, `timer_tick()` from an ISR, scheduler or dispatcher calls
  from an interrupt handler, preemption execution, context switching from an
  interrupt handler, nested interrupts, SMP, or ITRON-compatible interrupt APIs.

Chapter 7 Section 7.3 adds the timer interrupt entry boundary:

* IDT vector 32 is registered as the remapped legacy PIC IRQ0 entry.
* `arch/x86_64/interrupt_entry.asm` provides the temporary timer IRQ entry
  stub, and the C handler logs only `[timer-irq] entry reached: vector=32 irq=0`.
* The handler sends the minimum legacy PIC EOI for IRQ0 after the observation
  log. PIC EOI port details remain inside the x86_64 PIC module.
* Normal boot keeps IRQ0 masked, so existing smoke logs continue without timer
  IRQ handler arrival.
* `make run VALIDATE_TIMER_IRQ_ENTRY=1` explicitly unmasks IRQ0 and enables
  maskable interrupts for entry-arrival observation.
* This is not a timer subsystem. It does not program PIT frequency, call
  `timer_tick()` from an interrupt, evaluate preemption, call scheduler or
  dispatcher, switch context, change task state, implement nested interrupts,
  or provide ITRON-compatible interrupt APIs.
* The interrupt-time log is intentionally minimal and exists only as explicit
  validation evidence.

Chapter 7 Section 7.4 defines the interrupt-time log observation model:

* Logs emitted while the CPU is handling an interrupt are constrained. In this
  educational stage they use the same polling serial path as normal boot logs,
  so a timer IRQ observation line can appear in the middle of an existing log
  sequence.
* `[timer-irq] entry reached: vector=32 irq=0` is a validation-only observation
  log, not a normal boot smoke log and not evidence of a general
  interrupt-safe logging subsystem.
* Normal boot keeps IRQ0 masked and does not emit the timer IRQ observation
  log. Only `make run VALIDATE_TIMER_IRQ_ENTRY=1` enables the temporary
  observation path.
* The observation handler still does not call `timer_tick()`, scheduler,
  dispatcher, context switch, preemption logic, or task state transition logic.
* Doxygen comments in `arch/x86_64/interrupt.c` and `arch/x86_64/pic.c`
  describe the intent of the observation path, PIC remap sequence, mask mirror,
  IRQ0 unmask validation gate, and EOI placement.
* This section does not implement nested interrupts, continuous interrupt
  delivery, production interrupt return with `iretq`, PIT programming, APIC
  support, SMP, or ITRON-compatible interrupt APIs.

Chapter 8 Section 8.1 connects the timer IRQ handler to the kernel timer tick:

* The IRQ0/vector 32 timer interrupt handler calls `timer_tick()` once after
  the validation arrival log, then sends IRQ0 EOI.
* `make run VALIDATE_TIMER_IRQ_ENTRY=1` observes handler arrival, the
  interrupt-originated tick update through `[timer] tick: N`, and
  `[timer-irq] eoi sent: irq=0`.
* Following the Chapter 7 Section 7.4 observation model, logs emitted inside
  the timer IRQ handler are validation-only observation logs. They are not part
  of the normal boot log ordering guarantee. Normal boot keeps IRQ0 masked and
  does not emit timer IRQ handler logs.
* Chapter 8 Section 8.1 is the tick-connection step, not the preemption
  implementation step. The handler still does not connect to scheduler,
  dispatcher, context switching, dispatch pending, task state changes,
  sleep or delay queues, timeout handling, time slicing, or μITRON APIs.
* PIT programming, hardware timer period configuration, normal interrupt return
  via `iretq`, nested interrupts, stable continuous interrupt operation,
  APIC/IOAPIC/LAPIC, and SMP remain unimplemented.

Chapter 8 Section 8.2 connects the timer IRQ handler to the preemption
decision entry:

* The IRQ0/vector 32 timer interrupt handler still calls `timer_tick()` once
  after the validation arrival log.
* Immediately after `timer_tick()`, the handler calls the kernel-side
  preemption decision boundary, then sends IRQ0 EOI.
* The arch handler depends only on the kernel public preemption boundary; the
  scheduler and dispatcher details stay inside the kernel side.
* `make run VALIDATE_TIMER_IRQ_ENTRY=1` observes handler arrival, the
  interrupt-originated tick update, `[preempt-irq] decision evaluated: ...`,
  and `[timer-irq] eoi sent: irq=0`.
* The decision result is observation only. Chapter 8 Section 8.2 does not call
  the dispatcher, perform a context switch, change task state, or implement
  dispatch pending.
* Following the Chapter 7 Section 7.4 policy, interrupt-time logs remain
  validation-only and may interleave with normal boot logs.

Chapter 8 Section 8.3 records and observes dispatch pending:

* The IRQ0/vector 32 timer interrupt handler keeps the 8.1 and 8.2 ordering:
  validation arrival log, `timer_tick()`, preemption decision evaluation,
  dispatch pending observation, then IRQ0 EOI.
* The kernel owns the dispatch pending state. The x86_64 handler calls only a
  public dispatch pending observation API and does not depend on scheduler or
  dispatcher internals.
* A `switch-target` preemption decision records dispatch pending as requested
  from IRQ context. `no-switch`, `invalid-current`, and `no-current` decisions
  do not request dispatch pending.
* `make run VALIDATE_TIMER_IRQ_ENTRY=1` observes `[dispatch-pending] ...`
  between `[preempt-irq] decision evaluated: ...` and
  `[timer-irq] eoi sent: irq=0`.
* Dispatch pending is still observation-only. Chapter 8 Section 8.3 does not
  call the dispatcher, commit current, perform a context switch, switch task
  stacks, save or restore registers, change task states, or complete an
  interrupt-return dispatch model.
* Interrupt-time dispatch pending logs follow the Chapter 7 Section 7.4 policy:
  they are validation-only and may interleave with normal boot logs.

Chapter 8 Section 8.4 separates the timer IRQ path into entry, kernel handler,
and exit boundary responsibilities:

* Interrupt entry is the CPU arrival at IRQ0/vector 32 and the transfer from
  `arch_timer_irq_stub` to the C-side timer IRQ handler. The temporary stub
  still does not perform full register save/restore or normal `iretq` return.
* The kernel IRQ handler responsibility is limited to `timer_tick()`,
  `preemption_evaluate_from_irq()`, `dispatch_pending_log_state_from_irq()`,
  interrupt exit boundary observation, and IRQ0 EOI.
* The interrupt exit boundary is now visible in validation logs as
  `[timer-irq] exit boundary: dispatch-pending=... action=...`.
* If dispatch pending is requested, Chapter 8 Section 8.4 still does not
  consume it. The action remains `not-dispatched-yet`, and the kernel does not
  call the dispatcher, commit current, perform a context switch, switch task
  stacks, save or restore live registers, or change task states.
* If dispatch pending is not requested, the exit boundary reports
  `action=defer`. This is still only a future connection point for Chapter 9
  and later.
* Interrupt-time logs remain validation-only observations and may interleave
  with normal boot logs.

Chapter 9 Section 9.1 extends the boot-time context switch smoke from a
boot-to-task check into a minimal task-to-task observation:

* The context smoke prepares initial stack frames for two distinct tasks.
* The smoke first switches from the boot context to the selected task context.
* After the first task entry returns, the smoke marks that task READY again and
  switches once from the first task context to the second task context.
* The second task entry runs through the existing trampoline path, then returns
  to the boot context through the existing switch-back model.
* This is still a boot-time verification model. It is not
  `dispatcher_switch_to()`, interrupt-exit dispatch, dispatch pending
  consumption, yield API behavior, preemption, time slicing, semaphore wakeup
  dispatch, or a complete task lifecycle model.
* The expected observation includes
  `[context] task-to-task switch begin: ...`, followed by the second task entry
  log and the boot resume log.

Chapter 9 Section 9.2 adds a dispatcher-layer switch boundary on top of the
9.1 smoke flow:

* `dispatcher_switch_to(from, to)` is now the visible boundary for starting
  the boot-time task-to-task smoke from the dispatcher layer.
* The kernel context smoke no longer calls `task_context_switch_to_task_pair()`
  directly. It commits the current task, obtains mutable TCBs, and then enters
  the switch path through `dispatcher_switch_to()`.
* `task_context_switch_to_task_pair()` remains as a task-context-layer
  boot-time smoke helper. It is not the public dispatcher boundary.
* The dispatcher boundary logs begin/end events around the task context helper,
  including source and destination task identities and the result code.
* This still does not complete formal RUNNING/READY transition integration,
  consume dispatch pending, connect interrupt exit to dispatch, switch from the
  timer IRQ handler, implement yield, preemption, time slicing, semaphore
  wakeup dispatch, task termination, or a complete μITRON API.
* The expected observation includes
  `[dispatcher] switch boundary begin: ...`,
  `[context] task-to-task switch begin: ...`, and
  `[dispatcher] switch boundary end: result=0`.

Chapter 9 Section 9.3 connects RUNNING/READY state transitions to that
dispatcher switch boundary:

* `dispatcher_switch_to(from, to)` verifies `from` is RUNNING and `to` is READY
  before advancing the switch boundary.
* The dispatcher logs `from` as RUNNING->READY and `to` as READY->RUNNING before
  delegating to the task-context smoke helper.
* The destination task becomes the dispatcher current task at this boundary.
* `task_context_switch_to_task_pair()` remains as the boot-time smoke helper for
  stack/register-context observation and does not own dispatcher current state.
* This still does not finalize entry-return task termination or DORMANT/READY
  lifecycle handling, consume dispatch pending, connect interrupt exit to
  dispatch, switch from the timer IRQ handler, implement yield, preemption, time
  slicing, semaphore wakeup dispatch, or a complete μITRON API.
* The expected observation includes
  `[dispatcher] state transition: from ... RUNNING->READY`,
  `[dispatcher] state transition: to ... READY->RUNNING`,
  the existing task-to-task switch log, and
  `[dispatcher] switch boundary end: result=0`.

Chapter 9 Section 9.4 finalizes the task state after a task entry function
returns in the context-switch smoke path:

* The 9.1 task_b -> task_c task-to-task context switch smoke remains in place.
* The 9.2 `dispatcher_switch_to(from, to)` boundary remains the visible switch
  entry and end boundary.
* The 9.3 dispatcher-owned `from RUNNING->READY` and `to READY->RUNNING`
  transition logs remain unchanged.
* After entry return, `task_context_enter()` treats the returned entry as the
  completion of that activation and finalizes the task to `TASK_STATE_DORMANT`.
* In the current smoke flow, task_b is finalized from READY to DORMANT because
  the dispatcher already moved the switch source from RUNNING to READY; task_c
  is finalized from RUNNING to DORMANT.
* This is limited to entry-return lifecycle finalization in the task_context
  layer. Dispatch pending consumption, interrupt-exit dispatch, timer IRQ
  switching, preemptive context switching, task restart, and ITRON-like
  `sta_tsk`/`ext_tsk`/`exd_tsk` APIs remain unimplemented.

Chapter 10 Section 10.1 adds the first μITRON-like cooperative API entry:

* `yield_tsk()` is now exposed from the kernel common API layer.
* The API logs that it was called and observes the dispatcher current task.
* When the current task is RUNNING, the log includes task id, name, and state,
  then records `switch-not-connected-yet`.
* When no current task exists, or the current task is not RUNNING, the API
  rejects the call as `invalid-current-state` and returns a negative value.
* This is an API-entry foundation only. `yield_tsk()` does not move RUNNING
  tasks back to READY, select the next task, call `dispatcher_switch_to()`,
  call `task_context_switch_to_task_pair()`, consume dispatch pending, or
  connect to interrupt exit or timer IRQ dispatch.
* The 9.1 task_b -> task_c smoke, 9.2 dispatcher switch boundary, 9.3
  RUNNING/READY transition logs, and 9.4 entry return -> DORMANT finalization
  remain in place.

Chapter 10 Section 10.2 lets `yield_tsk()` return the RUNNING current task to
READY:

* `yield_tsk()` now calls the task-management transition boundary for the
  current task only when the current task exists and is `TASK_STATE_RUNNING`.
* A successful call logs `RUNNING->READY`, then records
  `scheduler-not-connected-yet`.
* DORMANT current tasks, including tasks finalized by the 9.4 entry-return
  path, are still rejected as `invalid-current-state` and are not moved back to
  READY.
* This is still not complete cooperative scheduling. `yield_tsk()` does not
  call `scheduler_select_next()`, `dispatcher_switch_to()`, or
  `task_context_switch_to_task_pair()`, and it does not consume dispatch
  pending or connect interrupt exit/timer IRQ paths to dispatch.
* The 9.1 task_b -> task_c smoke, 9.2 dispatcher switch boundary, 9.3
  RUNNING/READY transition logs, and 9.4 entry return -> DORMANT finalization
  remain in place.

Chapter 10 Section 10.3 lets `yield_tsk()` ask the scheduler for the next
READY task candidate after returning the RUNNING current task to READY:

* `yield_tsk()` still accepts only a RUNNING current task and still rejects
  DORMANT current tasks as `invalid-current-state`.
* After the RUNNING->READY transition succeeds, `yield_tsk()` calls
  `scheduler_select_next()` and logs the selected candidate's id, name,
  priority, and state.
* If no READY task is available, `yield_tsk()` logs
  `no-ready-task`; the successful READY transition still returns
  `YIELD_TSK_OK`.
* The boot-time yield smoke keeps multiple READY tasks available and observes
  the scheduler candidate, then stops at
  `dispatcher-switch-not-connected-yet`.
* This is still not complete cooperative scheduling. `yield_tsk()` does not
  call `dispatcher_switch_to()` or `task_context_switch_to_task_pair()`, does
  not commit dispatcher current to the selected candidate, and does not
  consume dispatch pending or connect interrupt exit/timer IRQ paths to
  dispatch.

Chapter 10 Section 10.4 connects `yield_tsk()` to the existing dispatcher and
task_context switch boundaries for the first cooperative API switch path:

* `yield_tsk()` still accepts only a RUNNING current task and still rejects
  DORMANT current tasks as `invalid-current-state`.
* After RUNNING->READY succeeds, `yield_tsk()` calls `scheduler_select_next()`
  and, when a different READY task exists, logs switch begin/end around
  `dispatcher_switch_to()`.
* `dispatcher_switch_to()` remains the visible switch boundary. It advances the
  selected task READY->RUNNING, updates dispatcher current, and delegates the
  switch body to `task_context_switch_to_task_pair()`.
* The task_context layer logs task-to-task switch begin/end and still finalizes
  entry return to `TASK_STATE_DORMANT`; `yield_tsk()` is not entry return and
  does not restart DORMANT tasks.
* If the only READY task is the just-yielded current task, `yield_tsk()` logs
  `no-ready-task` and `deferred: reason=no-next-task` without switching.
* This remains a cooperative API connection only. Timer IRQ, interrupt exit
  dispatch, dispatch pending consumption, preemptive switch, and time slicing
  are still not connected.

Chapter 11 Section 11.1 detects a higher-priority READY task after timer IRQ
preemption evaluation and requests dispatch pending for observation:

* The timer IRQ handler still runs `timer_tick()`, preemption evaluation,
  dispatch pending observation, interrupt exit boundary observation, and IRQ0
  EOI in that order.
* The preemption layer logs the current RUNNING task and, when present, the
  higher-priority READY candidate. A candidate is higher priority only when its
  numeric priority value is smaller than the current task's priority.
* Equal-priority READY tasks are logged as
  `same-priority-not-timeslice-target` and are not preemption targets in this
  chapter.
* When a higher-priority READY task exists, dispatch pending is requested with
  the source and destination task id/name, and the interrupt exit boundary still
  reports `action=not-dispatched-yet`.
* This chapter still does not call `yield_tsk()` or `dispatcher_switch_to()`
  from the timer IRQ handler, consume dispatch pending, connect interrupt exit
  to real dispatch, or perform a preemptive context switch.

Chapter 11 Section 11.2 consumes the dispatch pending requested by 11.1 at a
deferred dispatch boundary before IRQ0 EOI:

* Dispatch pending now keeps the requested from/to task identity and exposes a
  one-shot consume path for the later boundary.
* The timer IRQ handler body still does not call `yield_tsk()` or
  `dispatcher_switch_to()` directly. It delegates to the interrupt exit
  boundary after tick, preemption evaluation, and pending observation.
* When pending is requested, the exit boundary logs
  `action=deferred-dispatch`, consumes the pending from/to, re-acquires mutable
  TCBs by id, validates from RUNNING and to READY, then calls
  `dispatcher_switch_to(from, to)`.
* Valid deferred dispatch reaches the existing task-to-task context switch
  smoke and then clears pending with `reason=dispatch-completed`.
* No-pending and invalid from/to cases do not switch. Invalid pending is
  cleared on the safe side.
* Equal-priority READY tasks remain outside preemption and time slicing.
* This is still not a complete interrupt-return-frame switch. Same-priority
  time slice, semaphore wakeup integration, nested interrupts, sleep/delay
  queues, APIC/IOAPIC/LAPIC, and SMP remain unimplemented.

Chapter 11 Section 11.3 fixes same-priority READY handling as explicitly not a
time slice target:

* The scheduler/preemption decision treats only READY tasks with a numerically
  smaller priority value than current as preemption targets.
* If the only READY candidate is the same priority as current, the reason stays
  `same-priority-not-timeslice-target` and dispatch pending is not requested.
* The interrupt exit boundary therefore logs
  `dispatch-pending=none action=no-dispatch`, and the consume API logs
  `consume skipped: reason=no-pending`.
* The 11.2 higher-priority READY deferred dispatch path remains intact:
  requested pending is consumed once and valid from/to tasks still reach
  `dispatcher_switch_to(from, to)`.
* The 10.4 `yield_tsk()` cooperative context switch path remains separate from
  timer IRQ dispatch.
* Round-robin, tick-count slice management, same-priority task ordering,
  semaphore wakeup integration, nested interrupts, and complete
  interrupt-return-frame switching remain unimplemented.

Chapter 11 Section 11.4 stabilizes the timer IRQ preemption event log sequence:

* Higher-priority READY logs now keep the fixed order: current task,
  higher-ready detection, decision, dispatch pending request, exit boundary,
  pending consume, dispatcher switch, pending clear, and IRQ0 EOI.
* Same-priority READY logs remain no-dispatch and keep
  `same-priority-not-timeslice-target`, followed by not-requested, exit boundary
  `dispatch-pending=none action=no-dispatch`, and
  `consume skipped: reason=no-pending`.
* Dispatch pending request, consume, and clear logs use the same
  `higher-priority-ready` and `dispatch-completed` reason strings, and the
  same pending request is not logged as requested more than once.
* The 10.4 `yield_tsk()` cooperative context switch path, the 11.2
  higher-priority READY deferred dispatch path, and the 11.3 same-priority
  READY no-dispatch path remain intact.
* This is still only an educational boot-time verification model. Same-priority
  time slice, round-robin, tick-count slice management, semaphore wakeup,
  sleep/delay queues, nested interrupts, complete interrupt-return-frame
  switching, APIC/IOAPIC/LAPIC, and SMP remain unimplemented.

Chapter 12 Section 12.1 connects the semaphore wait API to the running task
state transition path:

* `wai_sem()` is now a task-context API that reads the current task from the
  dispatcher and rejects missing or non-RUNNING current tasks.
* If the semaphore count is available, `wai_sem()` decrements the count and
  completes with `action=no-switch`.
* If the count is 0, `wai_sem()` moves the RUNNING current task to WAITING,
  records the waited semaphore id, asks the scheduler for the next READY task,
  and connects to `dispatcher_switch_to()`.
* WAITING tasks remain outside scheduler READY candidates.
* The 10.4 `yield_tsk()` cooperative switch path and 11.4 timer IRQ dispatch
  pending log path remain separate.
* `sig_sem()` wakeup integration, wait queues, timeout waits, wakeup-after
  preemption checks, same-priority time slice, and round-robin remain
  unimplemented.

Chapter 12 Section 12.2 adds the minimal `sig_sem()` wakeup path:

* `sig_sem()` now scans the task table for one task with `state=WAITING` and
  `wait_sem_id` matching the target semaphore.
* If a matching waiting task exists, it is returned to READY, its
  `wait_sem_id` is cleared, and the semaphore count is not incremented.
* If no waiting task exists, `sig_sem()` increments the semaphore count by 1.
* The 10.4 `yield_tsk()` cooperative switch path, 11.4 timer IRQ dispatch
  pending log path, and 12.1 `wai_sem()` RUNNING->WAITING path remain separate.
* wait queues, FIFO/priority wakeup ordering, wakeup-after-preemption checks,
  immediate context switch after `sig_sem()`, timeout waits, same-priority time
  slice, and round-robin remain unimplemented.

Chapter 12 Section 12.3 adds a semaphore-local FIFO wait queue:

* Each `semaphore_t` now owns a fixed-length FIFO wait queue of waiting task IDs.
* When `wai_sem()` cannot acquire the semaphore, the RUNNING task becomes
  WAITING and is enqueued on the target semaphore wait queue.
* `sig_sem()` no longer scans the whole task table for a waiting task. It
  dequeues one task from the target semaphore queue and returns that task to
  READY.
* A dequeued task has `wait_sem_id` cleared to the non-waiting state.
* If a waiting task is woken, the semaphore count is not incremented. If the
  wait queue is empty, `sig_sem()` increments the semaphore count by 1.
* Priority-ordered wait queues, timeout waits, wakeup-after-preemption checks,
  immediate context switch after `sig_sem()`, same-priority time slice, and
  round-robin remain unimplemented.

Chapter 12 Section 12.4 adds a wakeup-after-preemption check to `sig_sem()`:

* After `sig_sem()` dequeues a WAITING task and returns it to READY, it compares
  the current RUNNING task priority with the woken READY task priority.
* A smaller numeric priority value is treated as higher priority.
* If the woken task is higher priority than current, `sig_sem()` proceeds to the
  existing `dispatcher_switch_to(from, to)` boundary and completes with
  `action=wakeup-switch`.
* Same-priority and lower-priority wakeups remain `action=wakeup-no-switch`;
  same-priority tasks are still not time slice targets.
* The 12.3 FIFO wait queue order, `wait_sem_id` clear, and semaphore count rule
  are preserved: waking a waiting task does not increment count, and count is
  incremented only when the wait queue is empty.
* Priority-ordered wait queues, timeout waits, sleep/delay queues,
  same-priority time slicing, and round-robin remain unimplemented.

Chapter 13 Section 13.1 adds a `dly_tsk()`-style delay task API foundation:

* `dly_tsk(uint32_t delay_ticks)` is introduced as a task-context API entry.
* `delay_ticks == 0` is treated as an invalid delay and returns
  `action=invalid-delay` without changing task state.
* When `delay_ticks > 0`, the RUNNING current task transitions to WAITING with
  `wait_reason=delay`, `wait_sem_id=0`, and `delay_ticks_remaining` preserved
  for observation.
* Delay waiting is distinct from semaphore waiting. `wai_sem()` still uses
  `wait_reason=semaphore` and `wait_sem_id`, and `sig_sem()` does not wake
  delay-waiting tasks.
* After delay WAITING, the scheduler selects the next READY task and
  `dly_tsk()` proceeds to the existing `dispatcher_switch_to(from, to)`
  boundary when a READY task exists.
* Sleep/delay queues, tick decrement, tick-reached READY return, timeout
  `twai_sem`, same-priority time slicing, and round-robin remain unimplemented.

Chapter 13 Section 13.2 adds a sleep/delay queue for delay waiting tasks:

* The kernel initializes a fixed-length delay queue at boot.
* `dly_tsk(delay_ticks > 0)` checks that the delay queue can accept the current
  task before changing it to WAITING, so a full or duplicate queue does not
  leave an inconsistent WAITING task behind.
* After delay WAITING is recorded with `wait_reason=delay`, `wait_sem_id=0`,
  and `delay_ticks_remaining`, the task is enqueued into the delay queue.
* Delay queue logs and dumps show task id, name, remaining ticks, wait reason,
  state, and queue count.
* The delay queue is separate from semaphore wait queues. `sig_sem()` still
  wakes only semaphore waiters and does not return delay queue tasks to READY.
* Tick decrement, tick-reached READY return, delay dequeue wakeup, timeout
  `twai_sem`, same-priority time slicing, and round-robin remain unimplemented.

Chapter 13 Section 13.3 adds a `twai_sem()`-style timeout semaphore wait
observation path:

* `twai_sem(int sem_id, uint32_t timeout_ticks)` is introduced as a task-context
  API entry.
* `timeout_ticks == 0` is treated as an invalid timeout and is not used as a
  polling semaphore wait.
* When semaphore count is available, `twai_sem()` acquires it immediately,
  logs `action=acquired`, and does not enqueue or switch tasks.
* When semaphore count is zero and `timeout_ticks > 0`, the RUNNING current
  task transitions to WAITING with `wait_reason=semaphore-timeout`,
  `wait_sem_id` set to the target semaphore, and `delay_ticks_remaining`
  preserved for timeout observation.
* The timeout semaphore waiter is enqueued into the semaphore wait queue for
  `sig_sem()` wakeup observation and into the delay queue for remaining tick
  observation.
* Tick decrement, timeout-reached READY return, timeout-side semaphore wait
  queue removal, `sig_sem()` success-time delay queue removal, polling
  semaphore wait, same-priority time slicing, and round-robin remain
  unimplemented.

Chapter 13 Section 13.4 connects timer tick progress to delay queue wakeup:

* `timer_tick()` now calls `delay_queue_tick()` after incrementing the system
  tick count.
* Each delay queue entry decrements its remaining tick by one and logs the
  before/after value, wait reason, and task state.
* When a delay waiter reaches remaining tick zero, it is removed from the delay
  queue and moved from WAITING back to READY.
* When a timeout semaphore waiter reaches remaining tick zero, it is removed
  from the target semaphore wait queue, removed from the delay queue, and moved
  from WAITING back to READY.
* READY wakeup is visible to the existing scheduler/preemption path. Timer IRQ
  handling still observes dispatch pending and does not call
  `dispatcher_switch_to()` directly.
* `sig_sem()` success-time delay queue removal, timeout return-value delivery,
  polling semaphore wait, same-priority time slicing, and round-robin remain
  unimplemented.

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

Chapter 7 Section 7.1 adds this exception-observation path:

```text
kernel_main -> hal_interrupt_init -> arch_interrupt_init -> IDT load
CPU exception -> arch exception stub -> observation handler -> HAL console
```

Chapter 7 Section 7.2 adds this interrupt-controller preparation path:

```text
kernel_main -> hal_interrupt_controller_init -> arch_pic_init -> PIC remap and all-mask
```

Chapter 7 Section 7.3 adds this timer IRQ entry validation path:

```text
kernel_main -> hal_interrupt_enable_timer_entry_validation
IRQ0/vector 32 -> arch timer IRQ stub -> timer IRQ observation handler -> PIC EOI
```

Chapter 8 Section 8.2 extends the validation path without performing a switch:

```text
IRQ0/vector 32 -> arch timer IRQ handler -> timer_tick
                 -> preemption_evaluate_from_irq -> PIC EOI
```

Chapter 8 Section 8.3 extends that path by observing the held dispatch request:

```text
IRQ0/vector 32 -> arch timer IRQ handler -> timer_tick
                 -> preemption_evaluate_from_irq
                 -> dispatch_pending_log_state_from_irq -> PIC EOI
```

Chapter 8 Section 8.4 names the entry/handler/exit responsibilities without
performing a switch:

```text
interrupt entry:
  IRQ0/vector 32 -> arch_timer_irq_stub -> arch_timer_irq_handle

kernel IRQ handler:
  timer_tick -> preemption_evaluate_from_irq
             -> dispatch_pending_log_state_from_irq
             -> arch_timer_irq_exit_observe_boundary
             -> PIC EOI

interrupt exit boundary:
  dispatch pending is observed, but not consumed or dispatched
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

Chapter 6 Section 6.1 adds a semaphore smoke sequence before the existing
minimal context switch smoke path. The kernel creates `sem_a`, lets `task_b`
consume the initial count, moves `task_c` to `WAITING` on the second
`wai_sem()`, and then wakes `task_c` with `sig_sem()`. The waiting task is
returned to READY before the context switch smoke and cooperative runner
continue, so the existing execution-order checks remain intact.

Chapter 6 Section 6.2 adds a timer smoke sequence near the beginning of
`kernel_main`. The kernel initializes the system tick, explicitly calls
`timer_tick()` three times, and reads the current tick with `timer_get_ticks()`.
This is still a boot-time verification model. The tick does not come from a
hardware timer interrupt and does not drive scheduler selection, dispatcher
commit, context switching, preemption, time slice, `dly_tsk`, or timeout wakeup.

Chapter 6 Section 6.3 adds a preemption decision smoke sequence after task
registration and before the semaphore smoke. The kernel advances the timer tick,
reads the current task through the dispatcher boundary, and asks the scheduler
whether a higher-priority READY task should become the switch target. The
scheduler still only returns a decision. The dispatcher remains responsible for
current-task commit, and the context switch layer remains responsible for
register save/restore.

Chapter 7 Section 7.1 initializes the x86_64 IDT immediately after the HAL
console becomes available. The normal boot path only observes IDT initialization
and load completion. Exception-handler arrival is verified with an explicit
validation build:

```powershell
make run VALIDATE_EXCEPTION=1
```

That validation run triggers `int3`, logs vector 3 as `breakpoint`, and then
halts inside the observation handler. The normal `make run` path does not
trigger this validation exception, so the existing task, semaphore, timer,
preemption-decision, context-switch smoke, and cooperative verification logs
continue to run.

Chapter 7 Section 7.2 initializes the legacy PIC after the IDT foundation and
before the existing smoke flows. The PIC is remapped to use vector base 32 for
the master PIC and 40 for the slave PIC, then all IRQ lines are left masked.
This prepares the IRQ0 routing position for Chapter 7 Section 7.3 without
starting PIT delivery or interrupt-driven task switching.

Chapter 7 Section 7.3 registers vector 32 as the timer IRQ entry and adds a
temporary x86_64 handler that logs entry arrival and sends IRQ0 EOI. Normal
boot still leaves IRQ0 masked. The entry path is enabled only by an explicit
validation build:

```powershell
make run VALIDATE_TIMER_IRQ_ENTRY=1
```

That validation run observes the IRQ0/vector 32 entry point. It does not
program the PIT, does not call `timer_tick()`, and does not connect the
interrupt path to preemption, scheduler, dispatcher, context switching, or task
state changes.

Chapter 7 Section 7.4 narrows how that validation output should be read. The
timer IRQ line is an interrupt-time observation log, so it may interleave with
ordinary serial output. Treat it as handler-arrival evidence for the explicit
validation build only; normal boot must remain free of `[timer-irq]` output.
The related Doxygen comments document why the PIC remap keeps all IRQs masked,
why validation opens only IRQ0, and why the Chapter 7 observation handler sent
EOI without calling timer or scheduler logic.

Chapter 8 Section 8.1 advances that validation path by calling `timer_tick()`
from the IRQ0/vector 32 handler before sending EOI. This confirms the first
hardware-interrupt-originated kernel tick update while still stopping before
preemption, scheduler selection, dispatcher commit, context switching, task
state changes, timeout processing, or a normal `iretq` return model.

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
[interrupt] init begin
[interrupt] idt initialized
[interrupt] idt loaded
[pic] init begin
[pic] init done: master_base=32 slave_base=40 irqs=masked
[kernel] task init
[timer-smoke] begin
[timer] init: tick=0
[timer] tick: 1
[timer] tick: 2
[timer] tick: 3
[timer-smoke] current tick=3
[timer-smoke] end
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
[preempt-smoke] begin
[timer] tick: 4
[preempt] no-current result=no-switch reason=no-current current=none candidate=none
[dispatcher] committed current: id=1 name=task_a prio=5 state=RUNNING
[timer] tick: 5
[preempt] higher-ready result=switch-target reason=higher-priority-ready current id=1 name=task_a prio=5 state=RUNNING candidate id=2 name=task_b prio=1 state=READY
[preempt-smoke] restore current result=0 task_id=1
[dispatcher] committed current: id=2 name=task_b prio=1 state=RUNNING
[timer] tick: 6
[preempt] no-higher-ready result=no-switch reason=candidate-not-higher current id=2 name=task_b prio=1 state=RUNNING candidate id=3 name=task_c prio=1 state=READY
[preempt-smoke] restore current result=0 task_id=2
[preempt-smoke] end
[sem-smoke] begin
[sem] table initialized
[sem] initialized: id=1 name=sem_a count=1 max_count=1
[sem] wai_sem: task id=2 name=task_b sem id=1 name=sem_a count_before=1 count_after=0 result=ok
[sem] wai_sem: task id=3 name=task_c sem id=1 name=sem_a count_before=0 result=waiting
[task] waiting: id=3 name=task_c wait_sem_id=1 state=WAITING
[sem] sig_sem: sem id=1 name=sem_a count_before=0 result=wakeup task id=3 name=task_c
[task] ready: id=3 name=task_c state=READY wait_sem_id=0
[sem] dump start
[sem] id=1 name=sem_a count=0 max_count=1
[sem] dump end
[sem-smoke] end
[scheduler] after_register selected: id=2 name=task_b prio=1 state=READY
[yield-smoke] begin
[dispatcher] committed current: id=1 name=task_a prio=5 state=RUNNING
[yield] called: current id=1 name=task_a state=RUNNING
[yield] state transition: current id=1 name=task_a RUNNING->READY
[yield] next selected: id=2 name=task_b prio=1 state=READY
[yield] deferred: reason=dispatcher-switch-not-connected-yet
[yield-smoke] end
[context-smoke] begin
[dispatcher] committed current: id=2 name=task_b prio=1 state=RUNNING
[dispatcher] switch boundary begin: from id=2 name=task_b to id=3 name=task_c
[context] prepared initial frame: task id=2 name=task_b context.rsp=0x...
[context] prepared initial frame: task id=3 name=task_c context.rsp=0x...
[context] switch begin: from=boot to id=2 name=task_b boot.rsp.before=0x0 to.rsp.restore=0x...
[context] entered task stack: task id=2 name=task_b current.rsp=0x...
[task_b] executed
[context] task entry returned: task id=2 name=task_b
[context] task ready after switch entry: result=0 task id=2 name=task_b
[context] task-to-task switch begin: from id=2 name=task_b to id=3 name=task_c from.rsp.before=0x... to.rsp.restore=0x...
[context] entered task stack: task id=3 name=task_c current.rsp=0x...
[task_c] executed
[context] task entry returned: task id=3 name=task_c
[context] task ready after switch entry: result=0 task id=3 name=task_c
[context] switch back: from id=3 name=task_c to=boot from.rsp.before=0x... boot.rsp.restore=0x...
[context] switch resumed: task=boot boot.rsp.after=0x... from id=3 name=task_c from.rsp.saved=0x...
[dispatcher] switch boundary end: result=0
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

Chapter 6 Section 6.1 adds semaphore logs for `sem_a`. `task_b` consumes the
only available count, `task_c` enters `WAITING` with `wait_sem_id=1`, and
`sig_sem()` returns it to READY. The semaphore smoke is still a boot-time
verification model and does not introduce timer-based blocking, preemption,
interrupt-driven scheduling, or a real wait queue.

Chapter 6 Section 6.2 adds timer logs before task registration. The timer
smoke initializes `tick=0`, advances the tick to 1, 2, and 3 through explicit
`timer_tick()` calls, and confirms the current tick with `timer_get_ticks()`.
This is timer foundation only: it is not interrupt-driven and does not reorder
the existing task, semaphore, context switch, or cooperative verification flow.

Chapter 6 Section 6.3 adds preemption decision logs after task registration.
The `no-current` case confirms that a timer tick without a current task does
not force a switch. The `higher-ready` case confirms that `task_b` becomes a
switch target while lower-priority `task_a` is the logical current task. The
`no-higher-ready` case confirms that equal-priority `task_c` is not selected as
a preemption target while `task_b` is current. These logs show the decision
foundation only; the smoke intentionally restores task state and does not
perform an actual dispatch or context switch.

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
│  ├─ semaphore.c
│  ├─ timer.c
│  ├─ task.c
│  ├─ scheduler.c
│  ├─ preemption.c
│  └─ include/
│     ├─ semaphore.h
│     ├─ timer.h
│     ├─ task.h
│     ├─ scheduler.h
│     ├─ preemption.h
│     └─ hal/
│        ├─ console.h
│        └─ interrupt.h
├─ arch/
│  └─ x86_64/
│     ├─ hal_console.c
│     ├─ hal_interrupt.c
│     ├─ context_switch.asm
│     ├─ context_switch.h
│     ├─ interrupt_entry.asm
│     ├─ interrupt.c
│     ├─ interrupt.h
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
* Semaphore foundation with static semaphore table
* `semaphore_t` with `id`, `name`, `count`, and `max_count`
* `sem_create()`
* `wai_sem()`
* `sig_sem()`
* `sem_dump()`
* TCB `wait_sem_id` for observable semaphore waits
* WAITING to READY semaphore wakeup for one waiting task
* Timer foundation with RTOS-internal system tick
* `timer_init()`
* `timer_tick()`
* `timer_get_ticks()`
* Explicit boot-time timer smoke for tick observation
* Preemption decision foundation
* `scheduler_preempt_reason_t`
* `scheduler_preempt_decision_t`
* `scheduler_select_preemption_candidate()`
* Timer-triggered boot-time preemption decision smoke
* Preemption occurrence and non-occurrence serial logs
* Scheduler / dispatcher / timer / context switch responsibility separation for preemption decisions
* x86_64 IDT initialization
* `lidt` based IDT load
* Representative CPU exception entry stubs
* CPU exception observation handler
* Exception vector/name/error/RIP/CS/RFLAGS serial logs
* Explicit validation exception mode with `VALIDATE_EXCEPTION=1`
* Legacy PIC interrupt controller foundation
* PIC remap with IRQ0 prepared at vector 32
* PIC mask/unmask API in the x86_64 arch boundary
* Fully masked PIC state after boot-time initialization
* PIC initialization observation through QEMU serial log
* Interrupt-time log observation model for timer IRQ validation
* Timer IRQ handler to `timer_tick()` connection for explicit validation
* IRQ-originated tick update observation through QEMU serial log
* IRQ-originated preemption decision entry for explicit validation
* `preemption_evaluate_from_irq()` as a thin kernel boundary around the
  existing scheduler decision helper
* Kernel-owned dispatch pending observation state
* `dispatch_request_from_irq()`
* `dispatch_pending_is_requested()`
* `dispatch_pending_clear_for_test_or_later_boundary()`
* `dispatch_pending_log_state_from_irq()`
* IRQ-originated dispatch pending observation through QEMU serial log
* Timer IRQ interrupt entry / kernel IRQ handler / interrupt exit boundary responsibility split
* Interrupt exit boundary deferred dispatch consumption for requested dispatch pending
* Timer IRQ higher-priority READY detection and dispatch pending request observation
* Dispatch pending from/to identity retention and one-shot deferred consume API
* IRQ-originated pending consume connection to `dispatcher_switch_to(from, to)`
* Same-priority READY exclusion from timer IRQ time slicing with
  `same-priority-not-timeslice-target`
* Stabilized timer IRQ preemption event log ordering, reason strings, and
  duplicate request-log suppression
* Entry return finalization to `TASK_STATE_DORMANT` in the task_context layer
* μITRON-like `yield_tsk()` API entry foundation
* `yield_tsk()` current-task observation and invalid-current-state logging
* `yield_tsk()` RUNNING current task to READY transition
* `yield_tsk()` scheduler candidate selection after READY transition
* `yield_tsk()` cooperative connection to `dispatcher_switch_to()`
* Cooperative API path to `task_context_switch_to_task_pair()`
* `wai_sem()` task-context API entry for RUNNING current tasks
* Semaphore count acquisition path with `action=no-switch`
* RUNNING to WAITING transition when `wai_sem()` cannot acquire a semaphore
* Scheduler selection after semaphore wait entry, excluding WAITING tasks
* `wai_sem()` connection to the existing dispatcher switch boundary
* `sig_sem()` waiting task to READY transition
* `sig_sem()` no-waiting-task semaphore count increment path
* Semaphore-local FIFO wait queue for `wai_sem()` / `sig_sem()`
* `sig_sem()` wakeup-after-preemption check for higher-priority READY tasks
* Tick-based delay queue remaining decrement
* Tick-reached READY return for delay waiting tasks
* Tick-reached READY return for timeout semaphore waiting tasks
* Timeout-side removal from the semaphore wait queue
* Preemption pending observation after tick-driven READY wakeup
* Japanese Doxygen comments for interrupt/PIC observation intent and limits

---

## Not Implemented Yet

The following features are intentionally not implemented yet:

* Full task-to-task execution by context switching
* RUNNING as CPU execution state
* Continuous independent task stack execution
* Full timer interrupt subsystem
* Full interrupt-driven preemption
* Production-grade dispatch pending consumption tied to a complete interrupt return frame
* IRQ routing
* APIC, IOAPIC, and LAPIC support
* PIT timer interrupt delivery
* Timer ISR
* Interrupt return with `iretq`
* Recoverable exception handling
* Time slice
* Delay-queue removal when `sig_sem()` wakes a timeout semaphore waiter
* Dynamic memory allocation
* Timer-driven stack switching
* Polling semaphore wait (`pol_sem`)
* Priority semaphore wait queue
* Timer-integrated semaphore blocking
* Round-robin scheduling
* Tick-based time slice management
* Same-priority task ordering for time slicing
* Mutex
* Event flag
* `TASK_STATE_EXITED`
* Task restart
* Full ITRON-like task lifecycle APIs such as `ext_tsk`, `exd_tsk`, `ter_tsk`, and `del_tsk`
* `tslp_tsk`, `rel_wai`, wakeup request counting, and sleep timeout handling
* Interrupt-driven task management
* Interrupt nesting control
* SMP scheduling
* Priority inheritance
* Tickless timer
* User mode
* Advanced interrupt mask control
* ITRON-compatible interrupt management APIs

---

## Documentation

Source files now include Doxygen-style comments for the current low-level
kernel, serial, HAL, interrupt, PIC, and initial task-management APIs. Recent
interrupt/PIC comments are written in Japanese and focus on execution intent:
why the IDT/PIC setup remains observation-only, why normal boot keeps IRQs
masked, why `VALIDATE_TIMER_IRQ_ENTRY=1` opens only IRQ0, and why the timer IRQ
handler sends EOI without connecting to timer, scheduler, dispatcher,
preemption, or context switching.
For Chapter 8 Section 8.1, comments additionally document that the handler now
calls `timer_tick()` before EOI while still not connecting to scheduler,
dispatcher, context switching, preemption, or task state changes.
For Chapter 8 Section 8.2, comments document that the handler now calls the
preemption decision entry after `timer_tick()` and before EOI, while still not
performing dispatcher commit, context switching, task state changes, or
dispatch pending updates.
For Chapter 8 Section 8.3, comments document that a switch-target decision can
set a kernel-owned dispatch pending observation state, but that the state is not
consumed by a dispatcher and still does not cause context switching or task
state changes.
For Chapter 8 Section 8.4, comments document the interrupt entry, kernel IRQ
handler, and interrupt exit boundary responsibilities. The exit boundary reads
dispatch pending only for validation logging and explicitly does not consume it,
dispatch a task, switch context, or change task states.
For Chapter 9 Section 9.4, comments document that `task_context_enter()`
finalizes entry-returned tasks to `TASK_STATE_DORMANT` while keeping dispatcher
switch-boundary responsibility, dispatch pending consumption, interrupt exit
dispatch, timer IRQ switching, and ITRON-like task lifecycle APIs out of scope.
For Chapter 10 Section 10.1, comments introduced `yield_tsk()` as a
μITRON-like API entry and observation point without connecting it to dispatch.
For Chapter 10 Section 10.2, comments document that `yield_tsk()` now returns
only a RUNNING current task to READY through the task-management boundary. They
continue to keep next-task selection, dispatcher switch connection, context
switch connection, dispatch pending consumption, interrupt-exit dispatch, and
timer IRQ dispatch out of scope.
For Chapter 10 Section 10.3, comments document that `yield_tsk()` now asks the
scheduler for a READY candidate after the READY transition and logs that candidate, while
still leaving dispatcher switch connection, context switch connection,
dispatcher current commit, dispatch pending consumption, interrupt-exit
dispatch, and timer IRQ dispatch out of scope.
For Chapter 10 Section 10.4, comments document that `yield_tsk()` now connects
to `dispatcher_switch_to()` and reaches the task_context task-to-task switch
smoke from a cooperative API call, while still leaving timer IRQ dispatch,
interrupt-exit dispatch, dispatch pending consumption, preemptive switch, and
time slicing out of scope.
For Chapter 11 Section 11.1, comments document that the timer IRQ path now
detects higher-priority READY tasks and requests dispatch pending for
observation, while still leaving real dispatch, pending consumption, interrupt
exit dispatch, and preemptive context switch out of scope.
For Chapter 11 Section 11.2, comments document that interrupt exit boundary
now consumes requested dispatch pending once, validates from RUNNING and to
READY, and connects valid requests to `dispatcher_switch_to(from, to)`. They
also state that the timer IRQ handler body still does not call `yield_tsk()` or
`dispatcher_switch_to()` directly, and that same-priority time slice, semaphore
wakeup integration, nested interrupts, and complete interrupt-return-frame
switching remain out of scope.
For Chapter 11 Section 11.3, comments document that same-priority READY tasks
are explicitly not timer IRQ time slice targets. The comments preserve the
reason `same-priority-not-timeslice-target`, explain that no dispatch pending
is requested in that case, and keep round-robin, tick-based slice management,
semaphore wakeup integration, nested interrupts, and complete
interrupt-return-frame switching out of scope.
For Chapter 11 Section 11.4, comments document that timer IRQ preemption event
logs now have a fixed order, unified reason strings, and one-shot requested
logging for each pending request. They also state that this log stabilization
does not add same-priority time slicing, round-robin, semaphore wakeup, nested
interrupts, or complete interrupt-return-frame switching.
For Chapter 12 Section 12.1, comments document that `wai_sem()` is a
task-context API, that available semaphore count completes without switching,
and that an unavailable semaphore moves the RUNNING current task to WAITING
before selecting the next READY task. They also state that `sig_sem()` wakeup,
wait queues, timeout waits, wakeup-after-preemption checks, same-priority time
slice, and round-robin remain out of scope.
For Chapter 12 Section 12.3, comments document that each semaphore now owns a
fixed-length FIFO wait queue, that `wai_sem()` enqueues only tasks that have
entered WAITING, and that `sig_sem()` dequeues from the target semaphore before
returning the task to READY. They also state that priority ordering, timeout
waits, wakeup-after-preemption checks, same-priority time slice, and
round-robin remain out of scope.
For Chapter 12 Section 12.4, comments document that `sig_sem()` is a
task-context API which compares priority only after a dequeued WAITING task has
returned to READY and cleared `wait_sem_id`. They also state that only a
higher-priority wakeup proceeds to the existing dispatcher switch boundary, while
priority-ordered wait queues, timeout waits, same-priority time slice,
round-robin, timer IRQ handler calls to `sig_sem()`, and complete
interrupt-return-frame switching remain out of scope.
For Chapter 13 Section 13.1, comments document that `dly_tsk()` is a
task-context API entry which moves only a RUNNING current task to delay
WAITING, records `wait_reason=delay` and `delay_ticks_remaining`, and then uses
the existing scheduler/dispatcher switch boundary. They also state that
sleep/delay queues, tick decrement, tick-reached READY return, timeout
`twai_sem`, timer IRQ handler calls to `dly_tsk()`, and complete timer-based
wakeup integration remain out of scope.
For Chapter 13 Section 13.2, comments document that `dly_tsk()` now enqueues
delay WAITING tasks into a dedicated delay queue before continuing to the
existing scheduler/dispatcher switch boundary. They also state that remaining
ticks are observation-only and that tick decrement, tick-reached READY return,
delay dequeue wakeup, timeout `twai_sem`, timer IRQ handler calls to task APIs,
and complete timer-based wakeup integration remain out of scope.
For Chapter 13 Section 13.3, comments document that `twai_sem()` is a
task-context API which immediately acquires an available semaphore count, or
classifies the current task as `semaphore-timeout` WAITING and registers it in
both the semaphore wait queue and delay queue for observation. They also state
that tick decrement, timeout-reached READY return, timeout-side semaphore queue
removal, and `sig_sem()` success-time delay queue removal remain out of scope.
For Chapter 13 Section 13.4, comments document that `timer_tick()` advances the
delay queue, that delay waiters and timeout semaphore waiters return from
WAITING to READY when remaining tick reaches zero, and that timeout semaphore
waiters are removed from the semaphore wait queue on timeout. They also state
that timer IRQ handler bodies still do not directly call task APIs or
`dispatcher_switch_to()`.
For Chapter 14 Section 14.1, comments document that `cre_tsk()` registers a
task as DORMANT, `sta_tsk()` starts only DORMANT tasks by moving them to READY,
and task-start preemption is recorded as dispatch pending instead of switching
directly from the API or timer IRQ handler.
For Chapter 14 Section 14.2, comments document that `slp_tsk()` moves only the
RUNNING current task to WAITING with `wait_reason=sleep`, and that `wup_tsk()`
returns only WAITING(sleep) tasks to READY. They also state that READY wakeup is
connected to the existing preemption pending boundary, while semaphore waits,
delay waits, timeout semaphore waits, wakeup request counting, sleep timeout,
and timer IRQ handler calls to `slp_tsk()` / `wup_tsk()` remain out of scope.
For Chapter 14 Section 14.3, comments document that `wai_sem()` / `sig_sem()` /
`pol_sem()` / `twai_sem()` are organized as the μITRON-like semaphore API layer.
`pol_sem()` performs non-blocking acquisition, `twai_sem()` registers timeout
semaphore waiters in both semaphore and delay queues, and `sig_sem()` restores
normal or timeout semaphore waiters to READY while connecting higher-priority
wakeups to the existing preemption pending boundary.
For Chapter 14 Section 14.4, comments document that the μITRON-like API layer now
uses a common `ER` return type and shared error codes. `E_OK` means success,
`E_ID` means an invalid or missing task/semaphore ID, `E_PAR` means invalid
parameters, `E_CTX` means an invalid task context in the range this learning RTOS
can currently detect, `E_OBJ` means an object state mismatch, `E_TMOUT` means
poll failure or timeout, and `E_QOVR` means a queue/count overflow. Existing
task, semaphore, delay queue, dispatcher, and preemption transitions are kept
unchanged while API completion logs report `result=<error-name>`.

For Chapter 15 Section 15.1, comments and documentation summarize the current
milestone before v1.0. This step does not add a scheduler, dispatcher, task,
semaphore, delay queue, or API behavior change. It aligns the README, the
articles under `articles/`, the Zenn Articles table, and the roadmap so readers
can trace the project from the boot foundation through the μITRON-like API layer
and common error code system.

For Chapter 15 Section 15.2, this repository includes a Doxygen generation path
for API reference pages.

This RTOS is a learning-oriented μITRON-like RTOS that runs on x86_64 + QEMU
and is developed with Codex + cc-sdd. The generated API reference is a support
entry point for reading the source code and Zenn articles.

Generate the API reference with:

```sh
doxygen Doxyfile
```

Output:

```text
docs/doxygen/html/index.html
```

The generated `docs/doxygen/` directory is a local inspection artifact.
This repository does not commit or publish the generated HTML files. Regenerate
them locally with `doxygen Doxyfile` when needed.

The Doxygen input is centered on `kernel/include`, with public API and
kernel-facing header contracts as the first documentation target. Doxygen is an
entry point into design information and API reference reading; it is not a
replacement for the specs under `.kiro/specs/`, the README, or the Zenn
articles. Future documentation steps will add Doxygen comments module by module
without changing kernel behavior or API contracts just for documentation output.

For Chapter 15 Section 15.3, the documentation fixes the `v1.0` boundary as a
learning-oriented μITRON-like RTOS release summary. It records the scope,
non-goals, validation commands, and local-only Doxygen output policy without
changing kernel behavior.

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
* [x] Semaphore foundation
* [x] Timer foundation
* [x] Preemption decision foundation
* [x] Interrupt and exception foundation
* [x] PIC interrupt controller foundation
* [x] Timer interrupt entry
* [x] Interrupt log observation model
* [x] Timer interrupt tick connection
* [x] Timer IRQ preemption decision entry
* [x] wai_sem RUNNING to WAITING transition
* [x] sig_sem waiting task to READY transition
* [x] semaphore FIFO wait queue
* [x] semaphore wakeup preemption decision
* [x] dly_tsk delay task API foundation
* [x] sleep/delay queue
* [x] twai_sem timeout semaphore wait observation
* [x] cre_tsk / sta_tsk create-start task API
* [x] slp_tsk / wup_tsk sleep-wakeup task API
* [x] semaphore API layer
* [x] μITRON-like common error code system
* [x] README and Zenn milestone summary
* [x] Doxygen documentation foundation
* [x] v1.0 release summary
* [ ] Full timer interrupt subsystem

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
| 1       | 1.1     | Project concept and target selection       | -                           | Completed |
| 1       | 1.2     | Development environment setup              | -                           | Completed |
| 1       | 1.3     | Publication policy                         | v1.3-publication-policy     | Completed |
| 2       | 2.1     | Initial QEMU boot and `kernel_main`        | v2.1-qemu-boot              | Completed |
| 2       | 2.2     | Serial console API cleanup                 | v2.2-serial-console         | Completed |
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
| 6       | 6.1     | Semaphore foundation                       | v6.1-semaphore-foundation   | Completed |
| 6       | 6.2     | Timer foundation                           | v6.2-timer-foundation       | Completed |
| 6       | 6.3     | Preemption foundation                      | v6.3-preemption-foundation  | Completed |
| 7       | 7.1     | Interrupt and exception foundation         | v7.1-interrupt-exception-foundation | Completed |
| 7       | 7.2     | PIC interrupt controller foundation        | v7.2-interrupt-controller-foundation | Completed |
| 7       | 7.3     | Timer interrupt entry                      | v7.3-timer-interrupt-entry | Completed |
| 8       | 8.1     | Timer tick from hardware IRQ               | v8.1-timer-tick-from-hardware-irq | Completed |
| 8       | 8.2     | Timer IRQ preemption decision entry        | v8.2-timer-irq-preemption-decision | Completed |
| 8       | 8.3     | Timer IRQ dispatch pending observation     | v8.3-timer-irq-dispatch-pending-observation | Completed |
| 8       | 8.4     | Timer IRQ entry/exit responsibility        | v8.4-timer-irq-entry-exit-responsibility | Completed |
| 9       | 9.1     | Task-to-task context switch smoke          | v9.1-task-to-task-context-switch-smoke | Completed |
| 9       | 9.2     | Dispatcher switch boundary                 | v9.2-dispatcher-switch-boundary | Completed |
| 9       | 9.3     | Dispatcher state transition switch         | v9.3-dispatcher-state-transition-switch | Completed |
| 9       | 9.4     | Task entry return finalization             | v9.4-task-entry-return-finalization | Completed |
| 10      | 10.1    | yield_tsk API foundation                   | v10.1-yield-task-api-foundation | Completed |
| 10      | 10.2    | yield_tsk RUNNING to READY transition      | v10.2-yield-running-to-ready | Completed |
| 10      | 10.3    | yield_tsk next READY task selection        | v10.3-yield-select-next-task | Completed |
| 10      | 10.4    | yield_tsk connect to context switch        | v10.4-yield-connect-context-switch | Completed |
| 11      | 11.1    | Timer IRQ detect higher-priority READY     | v11.1-timer-irq-detect-higher-ready | Completed |
| 11      | 11.2    | Timer IRQ deferred dispatch switch         | v11.2-timer-irq-deferred-dispatch-switch | Completed |
| 11      | 11.3    | Same-priority READY is not a time slice target | v11.3-same-priority-not-timeslice-target | Completed |
| 11      | 11.4    | Preemption event log stabilization         | v11.4-preemption-event-log-stabilization | Completed |
| 12      | 12.1    | wai_sem RUNNING to WAITING transition      | v12.1-wai-sem-running-to-waiting | Completed |
| 12      | 12.2    | sig_sem waiting task to READY transition   | v12.2-sig-sem-waiting-to-ready | Completed |
| 12      | 12.3    | semaphore FIFO wait queue                  | v12.3-semaphore-wait-queue | Completed |
| 12      | 12.4    | semaphore wakeup preemption decision       | v12.4-semaphore-wakeup-preemption | Completed |
| 13      | 13.1    | dly_tsk delay task API foundation          | v13.1-delay-task-api-foundation | Completed |
| 13      | 13.2    | sleep/delay queue                          | v13.2-sleep-delay-queue     | Completed |
| 13      | 13.3    | twai_sem timeout semaphore wait            | v13.3-twai-sem-timeout-wait | Completed |
| 13      | 13.4    | tick ready wakeup                          | v13.4-tick-ready-wakeup | Completed |
| 14      | 14.1    | cre_tsk / sta_tsk create-start task API    | v14.1-create-start-task-api | Completed |
| 14      | 14.2    | slp_tsk / wup_tsk sleep-wakeup task API    | v14.2-sleep-wakeup-task-api | Completed |
| 14      | 14.3    | semaphore API layer                        | v14.3-semaphore-api-layer | Completed |
| 14      | 14.4    | μITRON-like common error code system       | v14.4-itron-error-code-system | Completed |
| 15      | 15.1    | README and Zenn milestone summary          | v15.1-readme-zenn-milestone-summary | Completed |
| 15      | 15.2    | Doxygen documentation foundation           | v15.2-doxygen-documentation-foundation | Completed |
| 15      | 15.3    | v1.0 release summary                       | v1.0                       | Completed |

---

## Author

GitHub: pekopagu
