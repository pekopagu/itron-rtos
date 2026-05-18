# Implementation Plan

- [x] 1. 割り込み中ログ制約の文書化
  - READMEに第7章7.4として、割り込み中のserial logが通常boot logの途中に混ざり得ることを明記する。
  - 通常boot logとvalidation専用timer IRQ観測logの違いを説明する。
  - nested interrupt、連続割り込み、通常の割り込み復帰、interrupt-safe logging基盤が非対象であることを明記する。
  - Observable completion: READMEを読めば `[timer-irq]` logがvalidation専用観測であり、通常ログ順序保証の対象ではないことが分かる。
  - _Requirements: 1.1, 1.3, 1.4, 2.3, 3.4_
  - _Boundary: Documentation_

- [x] 2. timer IRQ handlerの観測コメントを7.4向けに更新する
  - handler周辺のDoxygenで、割り込み中ログはvalidation専用観測ログであり通常ログへ混ざり得ることを説明する。
  - handlerが `timer_tick()`、scheduler、dispatcher、context switch、task state変更を呼ばない非ゴールをコメントで維持する。
  - 既存のIRQ0/vector 32到達ログとPIC EOIの動作は維持し、通常bootで新しい観測出力を増やさない。
  - Observable completion: source comment reviewで7.4の観測モデルと非接続方針が確認でき、handler到達ログ文字列はvalidation時だけ観測できる。
  - _Requirements: 1.2, 2.1, 2.2, 2.4, 3.1, 3.2, 3.3_
  - _Boundary: x86_64 TimerIRQObservationHandler_

- [x] 3. buildとQEMU serial logで観測モデルを検証する
  - `make` が成功することを確認する。
  - `make run` で既存smoke flowが壊れず、通常bootではtimer IRQ観測ログが出ないことを確認する。
  - `make run VALIDATE_TIMER_IRQ_ENTRY=1` でtimer IRQ handler到達をvalidation専用ログとして観測できることを確認する。
  - source searchでkernel commonにPIC/vector/I/O port/entry stub詳細が漏れていないことを確認する。
  - source searchでtimer IRQ handlerがtimer/scheduler/dispatcher/context switch/task state変更を呼んでいないことを確認する。
  - Observable completion: build、通常run、validation run、境界検索の証跡により7.4の観測モデルが成立している。
  - _Requirements: 4.1, 4.2, 4.3, 4.4_
  - _Boundary: Validation_

- [x] 4. spec成果物を要求された3ファイルに限定する
  - `.kiro/specs/interrupt-log-observation-model/` に不要な補助ファイルを残さない。
  - requirements/design/tasksが実装結果と整合し、tasksの完了状態が反映されていることを確認する。
  - Observable completion: spec directory listingで `requirements.md`、`design.md`、`tasks.md` の3ファイルだけが確認できる。
  - _Requirements: 4.5_
  - _Boundary: Specification Artifacts_
