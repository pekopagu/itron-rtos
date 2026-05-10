# Implementation Plan

- [x] 1. Register保存領域の型契約をTCBへ追加する
  - `task_context_t` が `rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, `r15` を保持できる状態にする。
  - 各登録済みtaskがTCB内に独立したcontext保存領域を持つ状態にする。
  - Doxygenコメントで、これは将来の最小context switch用の保存領域であり、現段階では実CPU register値の保存・復元を行わないことを明記する。
  - 完了時には `tcb_t` を参照する既存コードがcontext field追加後もビルド可能な状態になる。
  - _Requirements: 1.1, 1.2, 1.3, 5.1, 5.2, 5.3, 5.4_
  - _Boundary: Task Context Model_

- [x] 2. task登録時にcontext保存領域を初期化する
  - 登録成功時に `context.rsp` が同じTCBの `stack_top` と対応する状態にする。
  - 登録成功時に `rbp`, `rbx`, `r12`, `r13`, `r14`, `r15` がゼロとして観測できる状態にする。
  - invalid registrationでは登録済みtaskとしてcontextが公開されない既存の失敗動作を維持する。
  - 完了時には `context.rsp` をCPUのRSPへロードする処理が存在しないまま、TCB metadataだけが初期化される。
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 5.4_
  - _Boundary: Context Initialization_

- [x] 3. 登録ログとdumpログでcontext保存領域を観測可能にする
  - task登録ログに `context.rsp`, `context.rbp`, `context.rbx`, `context.r12`, `context.r13`, `context.r14`, `context.r15` を追加する。
  - task dumpログにも同じcontext fieldsを追加する。
  - 完了時にはQEMU serial log上で `task_a`, `task_b`, `task_c` それぞれの `stack_top` と `context.rsp` を同じtask行から比較できる。
  - ログ追加はtask登録/dumpに限定し、scheduler、dispatcher、cooperative runnerのログ順序は変更しない。
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 4.1, 4.2, 4.3_
  - _Boundary: Task Context Logging_
  - _Depends: 1, 2_

- [x] 4. buildとQEMU serial logでfeature全体を検証する
  - `make` が成功することを確認する。
  - `make run` が成功し、登録ログとdumpログに各taskのcontext fieldsが表示されることを確認する。
  - `context.rsp` が各taskの `stack_top` と対応し、その他context fieldsが `0x0` として表示されることを確認する。
  - cooperative runnerのREADY選択、dispatcher commit、entry call、entry return、cooperative return、READY再候補化の既存ログ順序が維持されることを確認する。
  - diff上でstack switch、register save/restore、assembler変更、interrupt/timer/preemption追加がないことを確認する。
  - _Requirements: 1.1, 1.2, 1.3, 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 3.3, 3.4, 4.1, 4.2, 4.3, 5.1, 5.2, 5.3, 5.4_
  - _Boundary: Runtime Validation_
  - _Depends: 3_

## Implementation Notes
