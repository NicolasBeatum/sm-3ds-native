#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Captures state into SD dump files. Never writes the ROM itself. */
bool DebugDump_Write(const char *rom_name, uint32_t frame_number);
