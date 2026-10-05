// Test-only SDK boundary: pipe bytes become normalized events. Production BASIC
// sources, including its input adapter, remain identical to the RV32 build.
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>
#include <tabos/tty.h>
#include <tabos/wait.h>
#include <time.h>
#include <unistd.h>

static uint32_t tty_mode;

int ioctl(int descriptor, unsigned long request, ...)
{
    (void) descriptor;
    va_list args;
    va_start(args, request);
    if (request == TABOS_TTY_GET_MODE) {
        *va_arg(args, uint32_t*) = tty_mode;
    } else if (request == TABOS_TTY_SET_MODE) {
        tty_mode = va_arg(args, uint32_t);
    } else if (request == TABOS_TTY_GET_SIZE) {
        *va_arg(args, tabos_tty_size_t*) = (tabos_tty_size_t) {.rows = 24U, .columns = 80U};
    } else {
        abort();
    }
    va_end(args);
    return 0;
}

bool tabos_input_poll(tabos_input_event_t* event)
{
    static bool initialized;
    if (!initialized) {
        if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) < 0) {
            abort();
        }
        initialized = true;
    }
    unsigned char byte;
    if (read(STDIN_FILENO, &byte, 1U) != 1) {
        return false;
    }
    *event = (tabos_input_event_t) {
        .type = TABOS_INPUT_TEXT, .text = {(char) byte, '\0'}
    };
    if (byte == 8U || byte == 127U) {
        event->type = TABOS_INPUT_KEY_DOWN;
        event->key  = TABOS_KEY_BACKSPACE;
    }
    if (byte == 3U || byte == 17U) {
        event->type      = TABOS_INPUT_KEY_DOWN;
        event->key       = byte == 3U ? TABOS_KEY_C : TABOS_KEY_Q;
        event->modifiers = TABOS_MODIFIER_CONTROL;
    }
    if (getenv("TABOS_BASIC_TEST_KEYS") != NULL && byte >= 0x80U && byte < 0x8eU) {
        const tabos_key_t keys[] = {TABOS_KEY_LEFT,  TABOS_KEY_RIGHT, TABOS_KEY_UP, TABOS_KEY_DOWN,
                                    TABOS_KEY_SPACE, TABOS_KEY_A,     TABOS_KEY_B};
        event->type              = (byte & 1U) == 0U ? TABOS_INPUT_KEY_DOWN : TABOS_INPUT_KEY_UP;
        event->key               = keys[(byte - 0x80U) / 2U];
    }
    return true;
}

tabos_wait_source_t tabos_input_wait_source(void)
{
    return 1;
}

int tabos_wait(tabos_wait_item_t* items, uint32_t count, uint32_t timeout_ms)
{
    (void) items;
    (void) count;
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(STDIN_FILENO, &readable);
    struct timeval timeout;
    struct timeval* timeout_pointer = NULL;
    if (timeout_ms != TABOS_WAIT_TIMEOUT_INFINITE) {
        timeout.tv_sec  = timeout_ms / 1000U;
        timeout.tv_usec = (int) (timeout_ms % 1000U) * 1000;
        timeout_pointer = &timeout;
    }
    return select(STDIN_FILENO + 1, &readable, NULL, NULL, timeout_pointer);
}

uint64_t tabos_monotonic_ms(void)
{
    // Optional deterministic clock owned by policy.py, never compiled into BASIC.
    // The driver atomically replaces the file between commands. Ordinary native
    // and sanitizer sessions continue to use the host monotonic clock.
    const char* clock_path = getenv("TABOS_BASIC_TEST_CLOCK");
    if (clock_path != NULL) {
        FILE* file = fopen(clock_path, "r");
        uint64_t milliseconds;
        if (file == NULL || fscanf(file, "%" SCNu64, &milliseconds) != 1) {
            abort();
        }
        if (fclose(file) != 0) {
            abort();
        }
        return milliseconds;
    }
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        abort();
    }
    return (uint64_t) now.tv_sec * 1000U + (uint64_t) now.tv_nsec / 1000000U;
}

int tabos_sleep_ms(uint32_t milliseconds)
{
    struct timespec delay = {.tv_sec  = (time_t) (milliseconds / 1000U),
                             .tv_nsec = (long) (milliseconds % 1000U) * 1000000L};
    return nanosleep(&delay, NULL);
}
