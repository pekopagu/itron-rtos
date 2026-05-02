; Minimal Multiboot entry for qemu-system-x86_64 -kernel.
; QEMU enters this image in 32-bit protected mode and this code
; only sets a stack, calls kernel_main, then halts.

BITS 32

MB_MAGIC equ 0x1BADB002
MB_FLAGS equ 0x00000003
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top
    call kernel_main

.halt:
    hlt
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
