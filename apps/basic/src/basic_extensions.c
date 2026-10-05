#include <basic/extensions.h>
#include <basic/graphics.h>
#include <basic/runtime.h>
#include <string.h>
#include "../upstream/glue.h"

static const char* const keywords[] = {"GRAPHICS",   "TEXT",       "CLS",     "COLOR",  "PSET",      "LINE",
                                       "RECT",       "CIRCLE",     "PRESENT", "SPRITE", "SPRITEROW", "SPRITEPOS",
                                       "SPRITESHOW", "SPRITEHIDE", "SLEEP",   "SOUND",  "GTEXT",     "KEY"};

typedef struct {
        unsigned token;
        unsigned count;
        bool string_result;
        int32_t numbers[5];
        char text[65];
        unsigned length;
} extension_frame_t;
static extension_frame_t frames[8];
static unsigned depth;

unsigned basic_extension_tokenize(unsigned offset)
{
    if (offset >= 256U) {
        return 0U;
    }
    // Prefer longest match (SPRITEPOS before SPRITE). Called only at the
    // core keyword scanner, after its quoted-string/DATA/REM handling.
    unsigned result  = 0U;
    unsigned longest = 0U;
    for (unsigned index = 0U; index < sizeof(keywords) / sizeof(keywords[0]); ++index) {
        const unsigned length = (unsigned) strlen(keywords[index]);
        if (length > longest && length <= 256U - offset &&
            memcmp(RAM + 0x200U + offset, keywords[index], length) == 0) {
            longest = length;
            result  = ((offset + length - 1U) << 8U) | (BASIC_GRAPHICS + index);
        }
    }
    return result;
}

bool basic_extension_list(unsigned token)
{
    if (token < BASIC_GRAPHICS || token > BASIC_TOKEN_LAST) {
        return false;
    }
    basic_runtime_write(keywords[token - BASIC_GRAPHICS]);
    return true;
}

void basic_extension_reset(void)
{
    depth = 0U;
}

static unsigned error(unsigned code)
{
    X = (uint8_t) code;
    return 0xa437U;
}

static unsigned call(unsigned address)
{
    if (S < 16U) {
        return error(16U); // OUT OF MEMORY: retain the interpreter stack guard.
    }
    RAM[0x100U + S--] = (BASIC_EXTENSION_RETURN - 1U) >> 8U;
    RAM[0x100U + S--] = (BASIC_EXTENSION_RETURN - 1U) & 255U;
    return address;
}

static bool string_argument(const extension_frame_t* frame)
{
    return frame->token == BASIC_KEY ||
           ((frame->token == BASIC_SPRITEROW || frame->token == BASIC_GTEXT) && frame->count == 2U);
}

static unsigned finish(extension_frame_t* frame)
{
    const unsigned code =
        basic_graphics_command(frame->token, frame->numbers, frame->count, frame->text, frame->length);
    --depth;
    if (code != 0U) {
        return error(code);
    }
    CHRGOT();
    if (basic_runtime_break()) {
        C = Z = 1U;
        return 0xa82fU;
    }
    return 0U; // RTS to the ordinary statement/expression continuation.
}

unsigned basic_extension_begin(bool function)
{
    if (depth == sizeof(frames) / sizeof(frames[0])) {
        return error(16U);
    }
    extension_frame_t* frame = &frames[depth++];
    memset(frame, 0, sizeof(*frame));
    frame->token = A;
    CHRGET();
    if (function) {
        if (A != '(') {
            return error(11U);
        }
        CHRGET();
    } else if (Z != 0U) {
        return finish(frame);
    }
    return call(0xad9eU); // Evaluate using the production BASIC expression parser.
}

unsigned basic_extension_resume(void)
{
    if (depth == 0U) {
        return error(11U);
    }
    extension_frame_t* frame = &frames[depth - 1U];
    if (frame->string_result) {
        const unsigned address = (unsigned) X | (unsigned) Y << 8U;
        if (A > sizeof(frame->text) - 1U || address + A > sizeof(RAM)) {
            return error(14U);
        }
        frame->length = A;
        memcpy(frame->text, RAM + address, A);
        frame->text[A]       = '\0';
        frame->string_result = false;
    } else if (string_argument(frame)) {
        if (RAM[0x0d] == 0U) {
            return error(22U); // TYPE MISMATCH
        }
        frame->string_result = true;
        return call(0xb6a3U); // Release temporary descriptor, return A length / XY data.
    } else {
        if (RAM[0x0d] != 0U) {
            return error(22U);
        }
        const unsigned exponent = RAM[0x61];
        if (exponent > 144U) {
            return error(14U);
        }
        uint32_t magnitude = 0U;
        if (exponent >= 129U) {
            const uint32_t mantissa =
                (uint32_t) RAM[0x62] << 24U | (uint32_t) RAM[0x63] << 16U | (uint32_t) RAM[0x64] << 8U | RAM[0x65];
            magnitude = mantissa >> (160U - exponent);
        }
        const bool negative = (RAM[0x66] & 0x80U) != 0U;
        if (magnitude > (negative ? 32768U : 32767U)) {
            return error(14U);
        }
        frame->numbers[frame->count] = negative ? -(int32_t) magnitude : (int32_t) magnitude;
    }
    ++frame->count;
    CHRGOT();
    if (frame->token == BASIC_KEY) {
        if (A != ')') {
            return error(11U);
        }
        const int held = basic_runtime_key(frame->text, frame->length);
        if (held < 0) {
            return error(14U);
        }
        CHRGET();
        RAM[0x0d] = 0U;
        RAM[0x61] = held ? 129U : 0U;
        RAM[0x62] = 0x80U;
        RAM[0x63] = RAM[0x64] = RAM[0x65] = RAM[0x70] = 0U;
        RAM[0x66]                                     = held ? 0xffU : 0U;
        --depth;
        return 0U;
    }
    if (A == ',') {
        if (frame->count == 5U) {
            return error(11U);
        }
        CHRGET();
        return call(0xad9eU);
    }
    if (Z == 0U) {
        return error(11U);
    }
    return finish(frame);
}
