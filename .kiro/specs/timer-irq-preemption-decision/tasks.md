# Implementation Plan

- [x] 1. IRQ起点 preemption decision 境界を追加する
  - timer IRQ handler から呼べる kernel public API を追加する。
  - 既存の scheduler preemption decision helper と dispatcher current 読み取りを再利用する。
  - API は decision を観測するだけに留め、dispatcher commit、context switch、task state変更、dispatch pending 更新を行わない。
  - 完了状態として、`make` 対象に新しい境界が含まれ、`[preempt-irq]` validation log を出せる。
  - _Requirements: 1.3, 2.3, 3.1, 3.2, 3.3, 4.1_

- [x] 2. Timer IRQ handler に decision 入口を接続する
  - IRQ0/vector 32 handler の順序を `timer_tick()`、preemption decision entry、IRQ0 EOI にする。
  - arch/x86_64 側は scheduler / dispatcher 内部へ直接依存せず、kernel public API だけを呼ぶ。
  - EOI送信を維持し、decision結果によって handler の責務を増やさない。
  - 完了状態として、validation run で timer IRQ arrival、tick、preemption decision、EOI が観測できる。
  - _Requirements: 1.1, 1.2, 1.4, 2.1, 2.2, 4.3_

- [x] 3. 8.2の到達点を文書と検証証跡へ反映する
  - README に 8.2 が preemption decision 入口までであり、実切替・dispatch pending は未接続であることを明記する。
  - Zenn Articles 表に `v8.2-timer-irq-preemption-decision` tag候補を追加する。
  - `docs/logs/qemu-serial.log` を `make run VALIDATE_TIMER_IRQ_ENTRY=1` の結果で更新する。
  - 完了状態として、通常bootの既存smoke、validation log、spec 3ファイル構成が確認できる。
  - _Requirements: 2.4, 3.4, 4.2, 4.4, 4.5_
