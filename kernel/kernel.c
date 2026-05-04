/*
 * SPDX-License-Identifier: MIT
 */

#include "hal/console.h"

void kernel_main(void)
{
    hal_console_init();
    hal_console_write("itron-rtos booting...\n");
    hal_console_write("kernel_main reached\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
