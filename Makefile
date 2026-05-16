# SPDX-License-Identifier: MIT

NASM ?= $(USERPROFILE)/AppData/Local/bin/NASM/nasm.exe
CLANG ?= clang
LD_LLD ?= ld.lld
OBJCOPY ?= llvm-objcopy
QEMU ?= qemu-system-x86_64

BUILD_DIR := build
LOG_DIR := docs/logs
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_ELF64 := $(BUILD_DIR)/kernel64.elf
QEMU_LOG := $(LOG_DIR)/qemu-serial.log

BOOT_OBJ := $(BUILD_DIR)/boot.o
KERNEL_OBJ := $(BUILD_DIR)/kernel.o
TASK_OBJ := $(BUILD_DIR)/task.o
SEMAPHORE_OBJ := $(BUILD_DIR)/semaphore.o
TIMER_OBJ := $(BUILD_DIR)/timer.o
SCHEDULER_OBJ := $(BUILD_DIR)/scheduler.o
DISPATCHER_OBJ := $(BUILD_DIR)/dispatcher.o
TASK_CONTEXT_OBJ := $(BUILD_DIR)/task_context.o
ARCH_CONTEXT_SWITCH_OBJ := $(BUILD_DIR)/arch/x86_64/context_switch.o
HAL_CONSOLE_OBJ := $(BUILD_DIR)/arch/x86_64/hal_console.o
SERIAL_OBJ := $(BUILD_DIR)/arch/x86_64/serial.o
OBJECTS := $(BOOT_OBJ) $(KERNEL_OBJ) $(TASK_OBJ) $(SEMAPHORE_OBJ) $(TIMER_OBJ) $(SCHEDULER_OBJ) $(DISPATCHER_OBJ) $(TASK_CONTEXT_OBJ) $(ARCH_CONTEXT_SWITCH_OBJ) $(HAL_CONSOLE_OBJ) $(SERIAL_OBJ)

CFLAGS := -target x86_64-elf -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -Wall -Wextra -I. -Ikernel/include -Iarch/x86_64
LDFLAGS := -nostdlib -T linker.ld

.PHONY: all run clean dirs

all: $(KERNEL_ELF)

dirs:
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	@if not exist $(BUILD_DIR)\arch\x86_64 mkdir $(BUILD_DIR)\arch\x86_64
	@if not exist $(LOG_DIR) mkdir $(LOG_DIR)

$(BOOT_OBJ): boot/boot.asm | dirs
	$(NASM) -f elf64 boot/boot.asm -o $(BOOT_OBJ)

$(KERNEL_OBJ): kernel/kernel.c kernel/include/hal/console.h kernel/include/task.h kernel/include/scheduler.h kernel/include/dispatcher.h kernel/include/task_context.h kernel/include/semaphore.h kernel/include/timer.h | dirs
	$(CLANG) $(CFLAGS) -c kernel/kernel.c -o $(KERNEL_OBJ)

$(TASK_OBJ): kernel/task.c kernel/include/task.h kernel/include/hal/console.h | dirs
	$(CLANG) $(CFLAGS) -c kernel/task.c -o $(TASK_OBJ)

$(SEMAPHORE_OBJ): kernel/semaphore.c kernel/include/semaphore.h kernel/include/task.h kernel/include/hal/console.h | dirs
	$(CLANG) $(CFLAGS) -c kernel/semaphore.c -o $(SEMAPHORE_OBJ)

$(TIMER_OBJ): kernel/timer.c kernel/include/timer.h kernel/include/hal/console.h | dirs
	$(CLANG) $(CFLAGS) -c kernel/timer.c -o $(TIMER_OBJ)

$(SCHEDULER_OBJ): kernel/scheduler.c kernel/include/scheduler.h kernel/include/task.h | dirs
	$(CLANG) $(CFLAGS) -c kernel/scheduler.c -o $(SCHEDULER_OBJ)

$(DISPATCHER_OBJ): kernel/dispatcher.c kernel/include/dispatcher.h kernel/include/task.h | dirs
	$(CLANG) $(CFLAGS) -c kernel/dispatcher.c -o $(DISPATCHER_OBJ)

$(TASK_CONTEXT_OBJ): kernel/task_context.c kernel/include/task_context.h kernel/include/task.h kernel/include/hal/console.h arch/x86_64/context_switch.h | dirs
	$(CLANG) $(CFLAGS) -c kernel/task_context.c -o $(TASK_CONTEXT_OBJ)

$(ARCH_CONTEXT_SWITCH_OBJ): arch/x86_64/context_switch.asm arch/x86_64/context_switch.h kernel/include/task.h | dirs
	$(NASM) -f elf64 arch/x86_64/context_switch.asm -o $(ARCH_CONTEXT_SWITCH_OBJ)

$(HAL_CONSOLE_OBJ): arch/x86_64/hal_console.c kernel/include/hal/console.h arch/x86_64/serial.h | dirs
	$(CLANG) $(CFLAGS) -c arch/x86_64/hal_console.c -o $(HAL_CONSOLE_OBJ)

$(SERIAL_OBJ): arch/x86_64/serial.c arch/x86_64/serial.h | dirs
	$(CLANG) $(CFLAGS) -c arch/x86_64/serial.c -o $(SERIAL_OBJ)

$(KERNEL_ELF): $(KERNEL_ELF64)
	$(OBJCOPY) -O elf32-i386 $(KERNEL_ELF64) $(KERNEL_ELF)

$(KERNEL_ELF64): $(OBJECTS) linker.ld
	$(LD_LLD) $(LDFLAGS) -o $(KERNEL_ELF64) $(OBJECTS)

run: $(KERNEL_ELF) | dirs
	powershell -NoProfile -ExecutionPolicy Bypass -Command "Remove-Item '$(QEMU_LOG)' -ErrorAction SilentlyContinue; $$p = Start-Process -FilePath '$(QEMU)' -ArgumentList @('-kernel','$(KERNEL_ELF)','-serial','file:$(QEMU_LOG)','-display','none','-no-reboot') -PassThru; Start-Sleep -Seconds 2; if (-not $$p.HasExited) { Stop-Process -Id $$p.Id -Force }; if (Test-Path '$(QEMU_LOG)') { Get-Content '$(QEMU_LOG)' }"

clean:
	powershell -NoProfile -ExecutionPolicy Bypass -Command "Remove-Item -Recurse -Force '$(BUILD_DIR)' -ErrorAction SilentlyContinue"
