#ifndef BASIC_RUNTIME_H
#define BASIC_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

int basic_runtime_key(const char* name, unsigned length);
bool basic_runtime_sleep(uint32_t milliseconds);
bool basic_runtime_init(void);
void basic_runtime_shutdown(void);
void basic_runtime_put_byte(uint8_t byte);
void basic_runtime_text_byte(uint8_t byte);
void basic_runtime_write(const char* text);
uint8_t basic_runtime_column(void);
enum {
    BASIC_INPUT_BREAK = -1
};
int basic_runtime_read_byte(bool command);
uint8_t basic_runtime_get_byte(void);
bool basic_runtime_break(void);
void basic_runtime_checkpoint(void);
uint32_t basic_runtime_jiffies(void);
void basic_runtime_set_jiffies(uint32_t value);

#endif
