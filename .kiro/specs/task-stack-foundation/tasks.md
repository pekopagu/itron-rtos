# Implementation Plan

- [x] 1. TCB stack metadata foundation
- [x] 1.1 Add observable `stack_top` metadata to registered tasks
  - Extend the task metadata so each valid registered task keeps `stack_base`, `stack_size`, and `stack_top` together.
  - Ensure `stack_top` is derived from the task's stack region as the top address for a downward-growing x86_64 stack.
  - Keep existing id, name, entry, priority, and READY-state registration behavior unchanged.
  - Completed state is visible when a registered task's TCB exposes non-null stack metadata and invalid stack inputs are still rejected.
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 5.1, 5.2_
  - _Boundary: Task Metadata, Task Registration_

- [x] 2. Stack metadata logging
- [x] 2.1 Include stack top in registration and dump logs
  - Add `stack_top` to successful task registration logs next to `stack_base` and `stack_size`.
  - Add `stack_top` to task dump logs for each registered task.
  - Ensure the log format allows the reader to compare `stack_base`, `stack_size`, and `stack_top` on the same task entry.
  - Completed state is visible when QEMU serial output includes `stack_top` for `task_a`, `task_b`, and `task_c` in both registration and dump paths.
  - _Requirements: 2.1, 2.2, 2.3, 2.4_
  - _Boundary: Task Logging_

- [x] 3. Boot-time stack foundation integration
- [x] 3.1 Preserve separate static sample stacks without changing execution semantics
  - Keep `task_a`, `task_b`, and `task_c` registered with separate static stack regions.
  - Clarify in comments that these stack regions are metadata for this phase and are not used to run entry functions.
  - Preserve the existing cooperative runner sequence: READY selection, current commit, direct entry call, return observation, and READY recandidacy.
  - Completed state is visible when `make run` shows non-overlapping stack ranges and the cooperative runner log order remains unchanged.
  - _Requirements: 3.1, 3.2, 3.3, 4.1, 4.2, 4.3, 5.3_
  - _Boundary: Kernel Boot Sample, Cooperative Runner_

- [x] 4. Validation
- [x] 4.1 Verify build and QEMU serial observations
  - Run the repository build command and confirm it succeeds.
  - Run the QEMU smoke command and inspect serial output for `stack_base`, `stack_size`, and `stack_top`.
  - Confirm each sample task has a distinct stack range and that `stack_top` equals `stack_base + stack_size`.
  - Confirm the entry calls are still ordinary C function calls and no stack switch, register save/restore, assembler dispatch, interrupt, timer, or preemption path was added.
  - Completed state is visible when build and smoke validation results are recorded and no regression in cooperative runner ordering is found.
  - _Requirements: 1.1, 1.2, 2.1, 2.2, 2.3, 2.4, 3.2, 3.3, 4.1, 4.2, 4.3, 5.1, 5.2, 5.3_
  - _Boundary: Runtime Validation_

## Implementation Notes
- `make` and `make run` passed; QEMU serial log shows task_a/task_b/task_c stack ranges as adjacent non-overlapping 1024-byte regions with `stack_top = stack_base + stack_size`.
