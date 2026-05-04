/*
 * SPDX-License-Identifier: MIT
 */

#ifndef ITRON_RTOS_HAL_CONSOLE_H
#define ITRON_RTOS_HAL_CONSOLE_H

void hal_console_init(void);
void hal_console_putc(char c);
void hal_console_write(const char *message);

#endif
