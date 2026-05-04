/*
 * SPDX-License-Identifier: MIT
 */

#include "hal/console.h"
#include "serial.h"

void hal_console_init(void)
{
    serial_init();
}

void hal_console_putc(char c)
{
    serial_putc(c);
}

void hal_console_write(const char *message)
{
    serial_write(message);
}
