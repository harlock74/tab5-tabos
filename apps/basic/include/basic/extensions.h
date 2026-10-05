#ifndef BASIC_EXTENSIONS_H
#define BASIC_EXTENSIONS_H

#include <stdbool.h>
#include <stdint.h>

// Stable TabBASIC token values; never reinterpret existing Commodore tokens.
enum {
    BASIC_GRAPHICS = 0xcc,
    BASIC_TEXT,
    BASIC_CLS,
    BASIC_COLOR,
    BASIC_PSET,
    BASIC_LINE,
    BASIC_RECT,
    BASIC_CIRCLE,
    BASIC_PRESENT,
    BASIC_SPRITE,
    BASIC_SPRITEROW,
    BASIC_SPRITEPOS,
    BASIC_SPRITESHOW,
    BASIC_SPRITEHIDE,
    BASIC_SLEEP,
    BASIC_SOUND,
    BASIC_GTEXT,
    BASIC_KEY,
    BASIC_TOKEN_LAST       = BASIC_KEY,
    BASIC_EXTENSION_RETURN = 0x02f0,
};

unsigned basic_extension_tokenize(unsigned offset);
bool basic_extension_list(unsigned token);
unsigned basic_extension_begin(bool function);
unsigned basic_extension_resume(void);
void basic_extension_reset(void);

#endif
