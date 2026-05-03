/*
 * SPDX-License-Identifier: MIT
 */

#include "../arch/x86_64/serial.h"

void kernel_main(void)
{
    serial_init();
    serial_write("itron-rtos booting...\n");
    serial_write("kernel_main reached\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
