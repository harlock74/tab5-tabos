#ifndef BASIC_C64_H
#define BASIC_C64_H

#include <stdbool.h>
#include <stdint.h>

// Application-owned virtual C64 compatibility state. BASIC numeric values are
// validated by the interpreter before reaching this byte-oriented boundary.
void basic_c64_reset(void);
void basic_c64_close(void);
void basic_c64_service(bool force);
void basic_c64_set_profile(bool enabled);
bool basic_c64_read(uint16_t address, uint8_t* value);
bool basic_c64_write(uint16_t address, uint8_t value);
bool basic_c64_active(void);

#endif
