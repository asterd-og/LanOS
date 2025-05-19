#include <serial.h>
#include <ports.h>
#include <stdio.h>

void serial_printf(const char *str, ...) {
    va_list va;
    va_start(va, str);
    char buffer[1024];
    vsnprintf(buffer, (size_t)-1, str, va);
    va_end(va);
    int i = 0;
    while (buffer[i])
        outb(0xe9, buffer[i++]);
}
