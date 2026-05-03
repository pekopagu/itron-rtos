/*
 * SPDX-License-Identifier: MIT
 */

#include "../arch/x86_64/serial.h"

void kernel_main(void)
{
    serial_write_string("kernel_main reached\r\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
