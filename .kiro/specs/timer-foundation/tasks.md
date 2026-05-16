# Implementation Plan

- [x] 1. Timer foundation API と内部 tick 管理
- [x] 1.1 system tick を初期化・明示増加・取得できる timer foundation を追加する
  - timer foundation の公開契約を追加し、起動後 tick を 0 から扱えるようにする。
  - 明示的な tick 増加ごとに tick が 1 ずつ進み、現在値を読み取れるようにする。
  - Doxygen コメントで、将来の timer interrupt handler から呼ばれる想定だが今回は割り込み未接続であることを明記する。
  - 完了時には timer module 単体が scheduler、dispatcher、task、semaphore、arch interrupt に依存しないことが確認できる。
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 2.1, 2.2, 3.1, 3.2, 3.3_
  - _Boundary: Timer Module_

- [x] 2. Boot-time timer smoke と build 統合
- [x] 2.1 timer foundation を既存 boot verification flow に明示呼び出しとして統合する
  - 既存の task、scheduler、dispatcher、context switch、semaphore の責務を変更せず、timer init と複数回の explicit tick を起動時検証へ追加する。
  - timer object を既存 build に含め、`make` で kernel image が生成できる状態にする。
  - 完了時には QEMU serial log 上で timer init が tick 増加より前に出力され、tick 増加が scheduler/dispatcher/context switch を起動しないことが確認できる。
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 4.3, 4.4_
  - _Boundary: Kernel Boot Verification_
  - _Depends: 1.1_

- [x] 3. Documentation と smoke evidence
- [x] 3.1 README と QEMU serial log に timer foundation の範囲と検証結果を反映する
  - README に Chapter 6.2 timer foundation を追加し、preemption ではなく explicit-call based な timer foundation であることを記録する。
  - README に timer interrupt、preemption、time slice、`dly_tsk`、timeout が future work のままであることを明記する。
  - `make run` の結果で `docs/logs/qemu-serial.log` を更新し、timer init、tick 増加、既存 task/semaphore/context smoke の継続を証跡として残す。
  - 完了時には README と qemu serial log の両方から、今回の機能範囲と非範囲を確認できる。
  - _Requirements: 3.4, 4.1, 4.2, 4.3, 4.4_
  - _Boundary: Documentation Evidence_
  - _Depends: 2.1_

## Implementation Notes
- `timer.c` は `hal/console.h` と `timer.h` のみに依存し、tick増加からscheduler/dispatcher/context switchを呼ばない。
