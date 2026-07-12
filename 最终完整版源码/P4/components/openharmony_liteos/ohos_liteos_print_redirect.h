#pragma once

#include <stdio.h>

int esp_rom_printf(const char *fmt, ...);

#define printf(...) esp_rom_printf(__VA_ARGS__)
#define fprintf(stream, ...) esp_rom_printf(__VA_ARGS__)
#define puts(str) esp_rom_printf("%s\n", str)
