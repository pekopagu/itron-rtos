/*
 * SPDX-License-Identifier: MIT
 */

#ifndef ITRON_RTOS_ARCH_X86_64_SERIAL_H
#define ITRON_RTOS_ARCH_X86_64_SERIAL_H

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *message);

#endif
