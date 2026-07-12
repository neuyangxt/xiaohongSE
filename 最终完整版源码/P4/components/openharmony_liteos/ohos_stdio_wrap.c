#include <stdarg.h>
#include <stdio.h>
#include <sys/reent.h>
#include "esp_rom_sys.h"

int __wrap_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = esp_rom_vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

int __wrap_vprintf(const char *fmt, va_list ap)
{
    return esp_rom_vprintf(fmt, ap);
}

int __wrap_fprintf(FILE *stream, const char *fmt, ...)
{
    (void)stream;
    va_list ap;
    va_start(ap, fmt);
    int ret = esp_rom_vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

int __wrap_vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    (void)stream;
    return esp_rom_vprintf(fmt, ap);
}

int __wrap__vfprintf_r(struct _reent *r, FILE *stream, const char *fmt, va_list ap)
{
    (void)r;
    (void)stream;
    return esp_rom_vprintf(fmt, ap);
}

int __wrap_puts(const char *s)
{
    return esp_rom_printf("%s\n", s);
}

int __wrap__puts_r(struct _reent *r, const char *s)
{
    (void)r;
    return esp_rom_printf("%s\n", s);
}
