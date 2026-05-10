; SPDX-License-Identifier: MIT

; qemu-system-x86_64 -kernel 用の最小 Multiboot エントリ。
; QEMU は32-bit protected modeでこのイメージへ入るため、boot stub側で
; long modeを有効化してからfreestanding C kernelを呼び出す。これにより
; 第5章5.3でx86_64のcallee-saved registerを保存・復元できるようにする。

BITS 32

MB_MAGIC equ 0x1BADB002
MB_FLAGS equ 0x00000003
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

CR0_PG equ 0x80000000
CR4_PAE equ 0x00000020
EFER_MSR equ 0xC0000080
EFER_LME equ 0x00000100

CODE64_SEL equ gdt64_code - gdt64_start
DATA64_SEL equ gdt64_data - gdt64_start

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
    ; long mode移行前の処理用に、まず32-bit stackを設定する。
    mov esp, boot_stack_top

    ; 64-bit code/data descriptorを含むGDTを読み込む。
    lgdt [gdt64_pointer]

    ; long modeではPAE pagingが前提になるため、CR4.PAEを有効にする。
    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    ; 先頭1GiBをidentity mapする最小page tableをCR3へ設定する。
    mov eax, pml4_template
    mov cr3, eax

    ; EFER.LMEを立て、次のpaging有効化でlong modeへ入れる状態にする。
    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LME
    wrmsr

    ; pagingを有効化する。CR0.PG有効後、far jumpで64-bit code segmentへ移る。
    mov eax, cr0
    or eax, CR0_PG
    mov cr0, eax

    jmp CODE64_SEL:long_mode_start

BITS 64

long_mode_start:
    ; data segment selectorを64-bit用へ揃える。long modeではbase/limitはほぼ使わないが、
    ; stack segmentなどのselector整合性を保つため明示しておく。
    mov ax, DATA64_SEL
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; C ABIに合わせて16-byte alignmentしたboot stackからkernel_mainへ入る。
    mov rsp, boot_stack_top
    and rsp, -16
    call kernel_main

.halt:
    ; kernel_mainが戻った場合も、再起動や未定義実行へ進まず停止する。
    hlt
    jmp .halt

section .rodata
align 8
gdt64_start:
gdt64_null:
    dq 0
gdt64_code:
    dq 0x00209A0000000000
gdt64_data:
    dq 0x0000920000000000
gdt64_end:

gdt64_pointer:
    dw gdt64_end - gdt64_start - 1
    dd gdt64_start

section .bss
align 16
boot_stack_bottom:
    resb 16384
boot_stack_top:

section .data
align 4096
pml4_template:
    dq pdpt_template + 0x003
    times 511 dq 0
pdpt_template:
    dq pd_template + 0x003
    times 511 dq 0
pd_template:
%assign page_index 0
%rep 512
    dq (page_index * 0x200000) + 0x083
%assign page_index page_index + 1
%endrep
