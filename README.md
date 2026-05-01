# ITRON RTOS (itron-rtos)

AI-assisted ITRON-like RTOS for x86_64 + QEMU

---

## Overview

This project is an experimental Real-Time Operating System (RTOS) developed using AI coding agents.

* Target: x86_64 + QEMU
* Purpose: Educational / Experimental
* Style: ITRON-like (not fully compliant)

This project explores how far AI agents can assist in low-level system development.

---

## Purpose

This project is primarily intended to share development results and experiments.

It is not intended for production use or active adoption.

---

## Features (Planned)

* Bootloader & kernel startup
* Kernel entry (`kernel_main`)
* Task management (ITRON-like API)
* Scheduler (cooperative → preemptive)
* Synchronization (semaphore)
* Timer management

---

## Development Environment

The project is developed using the following tools:

* OS:

  * Windows 11

* Toolchain:

  * LLVM / Clang
  * NASM
  * GNU Make

* Emulator:

  * QEMU (x86_64)

* Development Tools:

  * Git
  * Node.js / npm
  * Codex CLI
  * ChatGPT

This project is developed and tested on Windows 11.

---

## Tested Environment

This project has been tested in the following environment:

* OS:

  * Windows 11

* Architecture:

  * x86_64

* Toolchain:

  * clang / LLVM 22.x
  * NASM 3.x
  * GNU Make 3.81

* Emulator:

  * QEMU 11.x

* Other Tools:

  * Git
  * Node.js / npm
  * Codex CLI
  * ChatGPT

※ Only the above environment is tested. Compatibility with other environments is not guaranteed.

---

## Build & Run

Not implemented yet (planned in Part 4).

---

## Directory Structure

```text
itron-rtos/
├─ README.md
├─ LICENSE
├─ kernel/
├─ arch/
├─ include/
├─ build/
├─ docs/
└─ Makefile
```

---

## Development Policy

* AI-assisted development (Codex + cc-sdd)
* Human-reviewed code before acceptance
* Focus on learning and experimentation
* Development process is documented in Zenn articles

---

## Contributing

Contributions are welcome, but this is a personal experimental project.

* Issues and bug reports are accepted
* Pull requests are welcome
* Responses and fixes are not guaranteed

---

## Support

This project is provided without official support.

* No guarantee of functionality
* No guarantee of bug fixes
* No guarantee of compatibility with different environments

Issues and questions may be submitted, but responses are not guaranteed.

---

## Safety Notice

This software is provided for educational and experimental purposes only.

Do not use this software in production or safety-critical systems.

The author is not responsible for any damages caused by this software.

---

## AI Notice

This project includes AI-assisted code generation using tools such as ChatGPT and Codex.

All generated code is reviewed by the maintainer before being included.

---

## Verification Policy

All checks are assisted by AI tools, but final verification and decisions are made by the project maintainer.

AI-generated results are not accepted as-is and are always reviewed and validated manually.

The maintainer is responsible for confirming:

* License compliance
* Absence of unintended external code
* Export control considerations
* Overall safety and correctness

---

## Export Control Notice

This project is intended for educational and experimental purposes only.

This project does not intentionally include encryption, military, or dual-use controlled technology.

Users are responsible for complying with applicable laws and regulations in their jurisdiction.

---

## Publication Review Status

Publication-related checks are tracked as part of the development process.

The following items are currently prepared:

* [x] License policy defined
* [x] MIT License file added
* [x] Safety notice added
* [x] AI usage notice added
* [x] Verification policy defined
* [x] Export control notice added
* [x] `.gitignore` added
* [x] Development environment documented
* [x] Tested environment documented
* [ ] Source code implemented
* [ ] Initial build verified
* [ ] QEMU boot verified
* [ ] AI-generated code fully reviewed

Detailed review records will be maintained in `docs/` as development progresses.

---

## License

This project is licensed under the MIT License.

See the LICENSE file for details.

---

## Roadmap

* [x] Define project policy
* [x] Setup development environment
* [x] Define publication policy
* [ ] Boot on QEMU
* [ ] Kernel entry (`kernel_main`)
* [ ] Task management
* [ ] Scheduler
* [ ] Semaphore
* [ ] Timer / interrupt

---

## Repository Status

This repository is currently private and under active development.

The source code will be made public after the initial working kernel is implemented (planned in Part 4).

---

## Zenn Articles

Development process and design decisions are documented in Zenn articles.

Articles and source code versions are linked by Git tags.

| Part | Topic | Git Tag | Status |
|---|---|---|---|
| Part 1 | Project concept and target selection | - | Draft |
| Part 2 | Development environment setup | - | Draft |
| Part 3 | Publication policy and preparation | `v0.3.00-policy` | Draft |
| Part 4 | Initial QEMU boot and `kernel_main` | `v0.4.00-boot`  | Planned |

Note:
- The `main` branch may change over time.
- Article-specific code will be linked using Git tags.
- Links will be added after the repository and articles are published.
---

## Author

GitHub: pekopagu
