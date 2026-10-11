#ifndef BASIC_GRAPHICS_H
#define BASIC_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>

// Returns a BASIC error number (zero on success).
unsigned basic_graphics_command(unsigned token, const int32_t* arguments, unsigned count, const char* text,
                                unsigned length);
void basic_graphics_close(void);
void basic_sound_close(void);
bool basic_graphics_active(void);

#endif
