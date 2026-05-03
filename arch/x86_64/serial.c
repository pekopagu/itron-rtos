/*
 * SPDX-License-Identifier: MIT
 */

#include "serial.h"

#define COM1_PORT 0x3F8

static void outb(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static unsigned char inb(unsigned short port)
{
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void serial_init(void)
{
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
}

static int serial_can_transmit(void)
{
    return (inb(COM1_PORT + 5) & 0x20) != 0;
}

static void serial_write_char(char value)
{
    while (!serial_can_transmit()) {
    }
    outb(COM1_PORT, (unsigned char)value);
}

void serial_write_string(const char *message)
{
    static int initialized;

    if (!initialized) {
        serial_init();
        initialized = 1;
    }

    while (*message != '\0') {
        serial_write_char(*message);
        message++;
    }
}
