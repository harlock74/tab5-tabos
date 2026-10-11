/* CHRGET/CHRGOT and KERNAL conventions adapted from mist64/cbmbasic runtime.c.
 * Copyright (c) 2009 Michael Steil, James Abbatiello.
 * Redistribution conditions and disclaimer: ../LICENSE.
 */
#include <basic/engine.h>
#include <basic/runtime.h>
#include <basic/storage.h>
#include <stdint.h>
#include <stdio.h>
#include "../upstream/glue.h"

static uint32_t random_state;

static void character_fetch(bool advance)
{
    uint16_t pointer = (uint16_t) ((uint16_t) RAM[0x7a] | (uint16_t) RAM[0x7b] << 8U);
    if (advance) {
        ++pointer;
    }
    while (RAM[pointer] == ' ') {
        ++pointer;
    }
    RAM[0x7a] = (uint8_t) pointer;
    RAM[0x7b] = (uint8_t) (pointer >> 8U);
    A         = RAM[pointer];
    if (A >= ':') {
        C = 1U;
        Z = (A == ':');
        N = (uint8_t) (((uint8_t) (A - ':')) >> 7U);
    } else {
        const uint8_t intermediate = (uint8_t) (A - '0');
        const uint8_t result       = (uint8_t) (intermediate - 0xd0U);
        V                          = ((intermediate ^ 0xd0U) & (intermediate ^ result) & 0x80U) != 0U;
        C                          = (intermediate >= 0xd0U);
        Z                          = (result == 0U);
        N                          = result >> 7U;
        A                          = result;
    }
}

// Exact binary-mode CHRGET tail at $0080: CMP #$20 / BEQ CHRGET /
// SEC / SBC #$30 / SEC / SBC #$D0 / RTS. Unlike CHRGOT this starts
// with the character already in A and must not fetch another byte first.
static void character_tail(void)
{
    if (A == ' ') {
        character_fetch(true);
        return;
    }
    const uint8_t intermediate = (uint8_t) (A - 0x30U);
    const uint8_t result       = (uint8_t) (intermediate - 0xd0U);
    V                          = ((intermediate ^ 0xd0U) & (intermediate ^ result) & 0x80U) != 0U;
    A                          = result;
    C                          = intermediate >= 0xd0U;
    Z                          = result == 0U;
    N                          = result >> 7U;
}

void CHRGET(void)
{
    character_fetch(true);
}

void CHRGOT(void)
{
    character_fetch(false);
}

int init_os(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    random_state = 1U;
    basic_storage_init();
    return 0xe394;
}

static void illegal_quantity(void)
{
    // Dispatcher returns by popping a language return address. Route that return
    // to BASIC's error vector, with ILLEGAL QUANTITY (14), instead of native exit.
    X                 = 14U;
    RAM[0x100U + S--] = 0xa4U;
    RAM[0x100U + S--] = 0x36U;
}

static void unsupported(void)
{
    basic_unsupported();
    illegal_quantity();
}

static bool valid_time_string(void)
{
    const unsigned address = (unsigned) RAM[0x22] | (unsigned) RAM[0x23] << 8U;
    if (address > sizeof(RAM) - 6U) {
        return false;
    }
    const unsigned char* value = RAM + address;
    for (unsigned index = 0U; index < 6U; ++index) {
        if (value[index] < '0' || value[index] > '9') {
            return false;
        }
    }
    return (value[0] - '0') * 10 + value[1] - '0' < 24 && value[2] < '6' && value[4] < '6';
}

unsigned int kernal_dispatch(void)
{
    basic_runtime_checkpoint();
    switch (PC) {
        case 0x0073: CHRGET(); break;
        case 0x0079: CHRGOT(); break;
        case 0x0080:
            // TI$'s $AA1F call returns at $AA22. Validate its six-byte string
            // once, before conversion, as the explicitly requested HHMMSS
            // restriction. Other helper uses retain exact character semantics.
            if (Y == 0U && RAM[0x100U + (uint8_t) (S + 1U)] == 0x21U && RAM[0x100U + (uint8_t) (S + 2U)] == 0xaaU &&
                !valid_time_string()) {
                illegal_quantity();
            } else {
                character_tail();
            }
            break;
        case 0xff90: A = 0U; break; // SETMSG
        case 0xff99:
            X = 0U;
            Y = 0xa0U;
            break; // MEMTOP
        case 0xff9c:
            X = 0U;
            Y = 8U;
            break;                                  // MEMBOT
        case 0xffb7: A = 0U; break;                 // READST, no open channels
        case 0xffba: basic_storage_device(); break; // SETLFS
        case 0xffbd: basic_storage_name(); break;   // SETNAM
        case 0xffc0:                                // OPEN
        case 0xffc3:                                // CLOSE
        case 0xffc6:                                // CHKIN
        case 0xffc9:                                // CHKOUT/CMD
            unsupported();
            break;
        case 0xffd5:
        case 0xffd8: {
            if (PC == 0xffd5 && A != 0U) {
                unsupported(); // VERIFY remains disabled.
                break;
            }
            const char* error = PC == 0xffd5 ? basic_storage_load() : basic_storage_save();
            if (error != NULL) {
                basic_runtime_write("\n?");
                basic_runtime_write(error);
                basic_runtime_write("\n");
                X                 = 14U;
                RAM[0x100U + S--] = 0xa4U;
                RAM[0x100U + S--] = 0x36U;
            } else {
                A = 0U;
                C = 0U;
            }
            break;
        }
        case 0xffcc: // CLRCHN
        case 0xffe7: // CLALL
            break;
        case 0xffcf: {
            // CHRIN is called through $E115 and $A565. The next return
            // is $A486 for the direct reader; INPUT uses a different caller.
            // Current-line metadata still describes the previous command here.
            const bool command = RAM[0x100U + (uint8_t) (S + 5U)] == 0x85U && RAM[0x100U + (uint8_t) (S + 6U)] == 0xa4U;
            const int byte     = basic_runtime_read_byte(command);
            if (byte == BASIC_INPUT_BREAK) {
                // Re-enter BASIC's own STOP/BREAK path. The dispatch epilogue
                // pops this synthetic return, then $A82F unwinds to READY.
                // No C longjmp, core restart, or native pointer is involved.
                if (command) {
                    RAM[0x3a] = 0xffU; // Do not report the previous program line at READY.
                }
                RAM[0x100U + S--] = 0xa8U;
                RAM[0x100U + S--] = 0x2eU;
                Z                 = 1U;
                C                 = 1U;
            } else {
                A = (uint8_t) byte;
                C = 0U;
            }
            break;
        }
        case 0xffd2:
            basic_runtime_put_byte(A);
            C = 0U;
            break;
        case 0xffdb: basic_runtime_set_jiffies((uint32_t) A | (uint32_t) X << 8U | (uint32_t) Y << 16U); break;
        case 0xffde: {
            const uint32_t ticks = basic_runtime_jiffies();
            A                    = (uint8_t) ticks;
            X                    = (uint8_t) (ticks >> 8U);
            Y                    = (uint8_t) (ticks >> 16U);
            break;
        }
        case 0xffe1:
            Z = basic_runtime_break();
            if (Z) {
                // BASIC's $A82F distinguishes BREAK from END using carry.
                C = 1U;
            }
            break;
        case 0xffe4:
            A = basic_runtime_get_byte();
            C = 0U;
            break;
        case 0xfff0:
            if (!C) {
                unsupported();
            } else {
                X = 0U;
                Y = basic_runtime_column();
            }
            break;
        case 0xfff3:
            // Session-local pseudo timer bytes for RND. No hardware mapping.
            for (unsigned i = 4U; i < 10U; ++i) {
                random_state     = random_state * 1664525U + 1013904223U;
                RAM[0xdc00U + i] = (uint8_t) (random_state >> 24U);
            }
            X = 0U;
            Y = 0xdcU;
            break;
        default: fprintf(stderr, "basic: unsupported engine dispatch $%04X\n", (unsigned) PC); basic_engine_exit(1);
    }
    return 1U;
}
