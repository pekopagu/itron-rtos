# Requirements Document

## Introduction
この feature は、第6回としてカーネル共通部とアーキテクチャ依存部の境界を整理するための要求を定義する。第4回で x86_64 + QEMU の最小カーネル起動に成功し、第5回で `serial_init`、`serial_putc`、`serial_write` によるシリアル出力 API を整理済みである。

現在は `kernel/kernel.c` が `arch/x86_64/serial.h` を直接 include しており、カーネル共通部が x86_64 依存の serial 実装を直接参照している。この feature では、将来の AArch64 / RISC-V64 / MIPS64 などへの展開に備え、まずコンソール出力だけを HAL 経由にする。

## Boundary Context
- **In scope**: カーネル共通部から x86_64 serial ヘッダへの直接依存をなくすこと、HAL console 共通インターフェースを提供すること、x86_64 側で既存 serial API を使って HAL console を実装すること、起動ログを第5回と同等に維持すること、README へ境界変更を簡潔に記載すること、`make` と QEMU `-serial stdio` で確認すること。
- **Out of scope**: AArch64 / RISC-V64 / MIPS64 実装、タイマ HAL、割り込み HAL、コンテキストスイッチ HAL、メモリ管理 HAL、タスク管理、スケジューラ、μITRON 風 API、`printf`、ログレベル、大規模なディレクトリ再編。
- **Adjacent expectations**: `x86_64-qemu-minimal-boot` と `serial-console-api-cleanup` が完了していることを前提とし、既存の COM1 シリアル出力、最小カーネル起動、QEMU 確認手順を維持する。

## Requirements

### Requirement 1: カーネル共通部からアーキテクチャ依存ヘッダを分離する
**Objective:** As a カーネル開発者, I want カーネル共通部が x86_64 固有の serial ヘッダを直接参照しない, so that 将来のマルチアーキテクチャ対応で共通部を保ちやすくなる

#### Acceptance Criteria
1. The kernel HAL boundary feature shall remove direct inclusion of `arch/x86_64/serial.h` from `kernel/kernel.c`.
2. The kernel HAL boundary feature shall make `kernel/kernel.c` include only the HAL console common header for console output.
3. The kernel HAL boundary feature shall not expose x86_64 port I/O details to `kernel/kernel.c`.
4. The kernel HAL boundary feature shall not expose the COM1 address to `kernel/kernel.c`.
5. The kernel HAL boundary feature shall not add architecture selection conditionals such as `ARCH_X86_64` to `kernel/kernel.c`.

### Requirement 2: HAL console 共通インターフェースを提供する
**Objective:** As a カーネル開発者, I want コンソール出力を HAL の共通 API で呼べる, so that カーネル共通部のログ出力がアーキテクチャ依存実装から分離される

#### Acceptance Criteria
1. The kernel HAL boundary feature shall provide a common HAL console interface for console initialization.
2. The kernel HAL boundary feature shall provide a common HAL console interface for single character output.
3. The kernel HAL boundary feature shall provide a common HAL console interface for string output.
4. The kernel HAL boundary feature shall make `hal_console_init` available to kernel common code.
5. The kernel HAL boundary feature shall make `hal_console_putc` available to kernel common code.
6. The kernel HAL boundary feature shall make `hal_console_write` available to kernel common code.

### Requirement 3: x86_64 側で既存 serial API を HAL 実装として利用する
**Objective:** As a メンテナ, I want x86_64 の HAL console 実装が第5回の serial API を再利用する, so that COM1 シリアル出力の既存動作を維持できる

#### Acceptance Criteria
1. The kernel HAL boundary feature shall implement the x86_64 HAL console using `serial_init`.
2. The kernel HAL boundary feature shall implement the x86_64 HAL console using `serial_putc`.
3. The kernel HAL boundary feature shall implement the x86_64 HAL console using `serial_write`.
4. The kernel HAL boundary feature shall preserve existing COM1 serial output behavior.
5. The kernel HAL boundary feature shall keep changes to the existing serial implementation minimal.

### Requirement 4: 起動ログを HAL 経由で維持する
**Objective:** As a レビューア, I want 第5回と同等の起動ログを HAL 経由で確認できる, so that 境界整理後も最小起動とデバッグ出力が壊れていないことを判断できる

#### Acceptance Criteria
1. When `kernel_main` starts, the kernel HAL boundary feature shall initialize console output through `hal_console_init`.
2. When `kernel_main` emits boot logs, the kernel HAL boundary feature shall output logs through `hal_console_write`.
3. When the kernel boots, the kernel HAL boundary feature shall output `itron-rtos booting...`.
4. When the kernel reaches `kernel_main`, the kernel HAL boundary feature shall output `kernel_main reached`.
5. While the kernel remains in its existing halt loop, the kernel HAL boundary feature shall preserve the existing minimal boot behavior.

### Requirement 5: ビルドと QEMU 確認を維持する
**Objective:** As a 開発者, I want HAL 境界整理後もビルドと QEMU 上のログ確認が成功する, so that 変更が起動経路を壊していないことを確認できる

#### Acceptance Criteria
1. When a developer runs `make`, the kernel HAL boundary feature shall build successfully.
2. When `make` succeeds, the kernel HAL boundary feature shall produce `build/kernel.elf`.
3. When the kernel is run on QEMU with `-serial stdio`, the kernel HAL boundary feature shall make the boot log observable on serial output.
4. When QEMU serial output is captured, the kernel HAL boundary feature shall include `itron-rtos booting...` in the output log.
5. When QEMU serial output is captured, the kernel HAL boundary feature shall include `kernel_main reached` in the output log.
6. The kernel HAL boundary feature shall keep the QEMU log evidence concise and limited to the relevant boot log output.

### Requirement 6: README と変更範囲を最小に保つ
**Objective:** As a メンテナ, I want HAL 境界の変更点と非スコープが明確に保たれる, so that 第6回が過剰な HAL 抽象化や周辺機能実装に広がらない

#### Acceptance Criteria
1. The kernel HAL boundary feature shall document the HAL console boundary overview in README.
2. The kernel HAL boundary feature shall keep HAL scope limited to console output.
3. The kernel HAL boundary feature shall not implement AArch64, RISC-V64, or MIPS64 support.
4. The kernel HAL boundary feature shall not implement timer HAL, interrupt HAL, context switch HAL, or memory management HAL.
5. The kernel HAL boundary feature shall not implement task management, scheduler, or μITRON-style API behavior.
6. The kernel HAL boundary feature shall not implement `printf` or log levels.
7. The kernel HAL boundary feature shall avoid large directory restructuring.

### Requirement 7: 対象外ファイルと既存基盤を保護する
**Objective:** As a レビューア, I want HAL 境界整理の差分が対象範囲に限定される, so that boot、link、公開方針、生成物を誤って変更していないことを確認できる

#### Acceptance Criteria
1. The kernel HAL boundary feature shall not change `boot/boot.asm`.
2. The kernel HAL boundary feature shall not change `linker.ld`.
3. The kernel HAL boundary feature shall not change `LICENSE`.
4. The kernel HAL boundary feature shall not change `.gitignore`.
5. The kernel HAL boundary feature shall not change `.agents/`, `.codex/`, `.kiro/settings/`, `articles/`, `prompts/`, or `checkList/`.
6. The kernel HAL boundary feature shall not treat `build/` as a source change target.

