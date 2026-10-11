#ifndef BASIC_SID_H
#define BASIC_SID_H

#include <stdbool.h>
#include <stdint.h>

// Bounded virtual SID register and synthesis state. No pointer or generated-core
// memory is reachable through this interface.
void basic_sid_reset(void);
void basic_sid_close(void);
void basic_sid_service(void);
bool basic_sid_active(void);
bool basic_sid_read(uint16_t address, uint8_t* value);
bool basic_sid_write(uint16_t address, uint8_t value);
uint32_t basic_sid_frequency_millihz(uint16_t value);

#endif
