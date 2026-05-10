; SPDX-License-Identifier: MIT

; x86_64 SysV ABI:
;   rdi = task_context_t *from
;   rsi = const task_context_t *to
;
; task_context_t のfield offset:
;   rsp 0, rbp 8, rbx 16, r12 24, r13 32, r14 40, r15 48

BITS 64

section .text
global arch_context_switch
global task_context_trampoline
extern task_context_enter

arch_context_switch:
    ; 現在のstack pointerとcallee-saved registerをswitch元contextへ保存する。
    ; caller-saved registerはC ABI上で呼び出し側の責務なので、この最小primitiveでは扱わない。
    mov [rdi + 0], rsp
    mov [rdi + 8], rbp
    mov [rdi + 16], rbx
    mov [rdi + 24], r12
    mov [rdi + 32], r13
    mov [rdi + 40], r14
    mov [rdi + 48], r15

    ; switch先contextからstack pointerとcallee-saved registerを復元する。
    ; rspを先に切り替え、最後のretで復元先stack上のreturn addressへ制御を移す。
    mov rsp, [rsi + 0]
    mov rbp, [rsi + 8]
    mov rbx, [rsi + 16]
    mov r12, [rsi + 24]
    mov r13, [rsi + 32]
    mov r14, [rsi + 40]
    mov r15, [rsi + 48]
    ret

task_context_trampoline:
    ; r12はtask contextから復元される。この学習用の初回entry frameでは、
    ; 復元後のr12をTCB pointerの受け渡しに使う。
    ; C関数の第1引数規約に合わせ、r12のTCB pointerをrdiへ移す。
    mov rdi, r12
    call task_context_enter

.halt:
    ; task_context_enter() はboot contextへswitch backするため、通常ここへは戻らない。
    ; 戻った場合は制御流異常として停止し、QEMU上で観測できる状態にする。
    hlt
    jmp .halt
