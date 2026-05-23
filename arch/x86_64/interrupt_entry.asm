; SPDX-License-Identifier: MIT

; 第7章7.1 CPU例外entry stub。
;
; このstubは最小観測frameを作ってC側の例外loggerを呼ぶだけである。
; task context保存、scheduler呼び出し、task dispatch、割り込み駆動の
; 実行復帰経路はまだ実装しない。

BITS 64

section .text

extern arch_exception_handle
extern arch_timer_irq_handle

global arch_exception_stub_divide_error
global arch_exception_stub_breakpoint
global arch_exception_stub_invalid_opcode
global arch_exception_stub_general_protection
global arch_exception_stub_page_fault
global arch_timer_irq_stub

%macro EXCEPTION_STUB_NO_ERROR 2
%1:
    ; entry時のCPU stack: RIP, CS, RFLAGS。
    ; error codeを持たない例外にも0を補い、C handlerへ渡すframe形状を揃える。
    sub rsp, 40
    mov qword [rsp + 0], %2
    mov qword [rsp + 8], 0
    ; CPUが積んだ復帰情報を観測用frameへコピーする。iretq復帰には使わない。
    mov rax, [rsp + 40]
    mov [rsp + 16], rax
    mov rax, [rsp + 48]
    mov [rsp + 24], rax
    mov rax, [rsp + 56]
    mov [rsp + 32], rax
    mov rdi, rsp
    call arch_exception_handle
.halt:
    ; C handlerはnoreturnだが、万一戻った場合も実行を進めず停止させる。
    hlt
    jmp .halt
%endmacro

%macro EXCEPTION_STUB_WITH_ERROR 2
%1:
    ; entry時のCPU stack: ERROR, RIP, CS, RFLAGS。
    ; CPUが積んだerror codeはログ観測用に残し、この章では復旧やiretqを試みない。
    sub rsp, 40
    mov qword [rsp + 0], %2
    ; error codeつき例外ではCPU提供値をそのまま観測frameへ移す。
    mov rax, [rsp + 40]
    mov [rsp + 8], rax
    mov rax, [rsp + 48]
    mov [rsp + 16], rax
    mov rax, [rsp + 56]
    mov [rsp + 24], rax
    mov rax, [rsp + 64]
    mov [rsp + 32], rax
    mov rdi, rsp
    call arch_exception_handle
.halt:
    ; C handlerから戻っても例外後の通常実行へ進めない。
    hlt
    jmp .halt
%endmacro

EXCEPTION_STUB_NO_ERROR arch_exception_stub_divide_error, 0
EXCEPTION_STUB_NO_ERROR arch_exception_stub_breakpoint, 3
EXCEPTION_STUB_NO_ERROR arch_exception_stub_invalid_opcode, 6
EXCEPTION_STUB_WITH_ERROR arch_exception_stub_general_protection, 13
EXCEPTION_STUB_WITH_ERROR arch_exception_stub_page_fault, 14

arch_timer_irq_stub:
    ; 第8章8.4のtimer IRQ interrupt entry観測用stub。
    ; CPUがIRQ0/vector 32へ入った後、C側のkernel IRQ handlerへ渡す境界である。
    ; C handler側でtimer_tick()、preemption decision、dispatch pending観測、
    ; interrupt exit boundary観測、IRQ0 EOIまで行う。ただし本格的なinterrupt frame、
    ; register保存/復元、iretq復帰、dispatch pending消費、実task切り替えはまだ行わない。
    call arch_timer_irq_handle
.halt:
    ; interrupt gate entry後にiretqしないため、validation runは到達log後に停止する。
    ; 連続IRQやpreemptionへ進まないことを明示する観測モデルである。
    hlt
    jmp .halt
