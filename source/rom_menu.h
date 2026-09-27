#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Creates the SD layout and selects a ROM into path. Returns false on exit. */
bool RomMenu_Select(char *path, size_t path_size, char *name, size_t name_size,
                    bool *native_engine);
