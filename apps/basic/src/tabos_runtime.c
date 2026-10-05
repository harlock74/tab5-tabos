#include <basic/c64.h>
#include <basic/engine.h>
#include <basic/graphics.h>
#include <basic/runtime.h>
#include <basic/sid.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <tabos/input.h>
#include <tabos/runtime_time.h>
#include <tabos/tty.h>
#include <tabos/wait.h>
#include <unistd.h>

enum {
    INPUT_CAPACITY = 256,
    LINE_CAPACITY  = 80,
    INPUT_OVERRUN  = 256
};
static uint16_t pending[INPUT_CAPACITY];
static bool queue_discard;
static size_t pending_start;
static size_t pending_count;
static char line[LINE_CAPACITY + 1];
static size_t line_length;
static size_t line_position;
static bool line_ready;
static bool break_pending;
static uint32_t saved_mode;
static tabos_wait_item_t input_wait;
static uint64_t clock_origin;
static uint64_t last_pause;
static uint32_t clock_offset;
static uint64_t text_offset;
static uint32_t text_columns = 80U;
static bool held_keys[128];

static void consume(const tabos_input_event_t* event)
{
    if ((unsigned) event->key < sizeof(held_keys) / sizeof(held_keys[0]) &&
        (event->type == TABOS_INPUT_KEY_DOWN || event->type == TABOS_INPUT_KEY_UP)) {
        held_keys[event->key] =
            event->type == TABOS_INPUT_KEY_DOWN && (event->modifiers & TABOS_MODIFIER_CONTROL) == 0U;
    }
    if (event->type == TABOS_INPUT_KEY_DOWN && (event->modifiers & TABOS_MODIFIER_CONTROL) != 0U) {
        if (event->key == TABOS_KEY_Q) {
            basic_engine_exit(0);
        }
        if (event->key == TABOS_KEY_C && !event->repeat) {
            break_pending = true;
            // Ctrl+C is a cancellation boundary: retain only later typeahead.
            pending_start = 0U;
            pending_count = 0U;
            queue_discard = false;
        }
    }
    // TabOS supplies Backspace as a physical event (including portable repeats).
    // Enter and printable characters come exclusively from normalized text.
    const bool backspace = event->type == TABOS_INPUT_KEY_DOWN && event->key == TABOS_KEY_BACKSPACE;
    if (event->type != TABOS_INPUT_TEXT && !backspace) {
        return;
    }
    const char* text = backspace ? "\b" : event->text;
    for (size_t i = 0U; i < TABOS_INPUT_TEXT_MAX_BYTES && text[i] != '\0'; ++i) {
        const uint8_t byte = (uint8_t) text[i];
        if (!backspace && (byte == '\b' || byte == 127U)) {
            continue;
        }
        if (queue_discard) {
            if (byte != '\r' && byte != '\n') {
                continue;
            }
            queue_discard = false;
        }
        if (pending_count == INPUT_CAPACITY) {
            // Never turn lost typeahead into a truncated executable command.
            pending_start = 0U;
            pending_count = 1U;
            pending[0]    = INPUT_OVERRUN;
            queue_discard = byte != '\r' && byte != '\n';
            if (queue_discard) {
                continue;
            }
        }
        pending[(pending_start + pending_count) % INPUT_CAPACITY] = byte;
        ++pending_count;
    }
}

static void poll_input(void)
{
    tabos_input_event_t event;
    // Bound one drain even if an input producer remains active.
    for (unsigned i = 0U; i < 64U && tabos_input_poll(&event); ++i) {
        consume(&event);
    }
}

bool basic_runtime_init(void)
{
    if (ioctl(STDIN_FILENO, TABOS_TTY_GET_MODE, &saved_mode) != 0) {
        return false;
    }
    if (ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, saved_mode & ~(uint32_t) TABOS_TTY_MODE_RAW_INPUT) != 0) {
        return false;
    }
    input_wait        = (tabos_wait_item_t) {.events = TABOS_WAIT_READABLE};
    input_wait.source = tabos_input_wait_source();
    if (input_wait.source == TABOS_WAIT_SOURCE_INVALID) {
        (void) ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, saved_mode);
        return false;
    }
    memset(held_keys, 0, sizeof(held_keys));
    pending_start = 0U;
    pending_count = 0U;
    queue_discard = false;
    line_length   = 0U;
    line_position = 0U;
    line_ready    = false;
    break_pending = false;
    clock_origin  = tabos_monotonic_ms();
    last_pause    = clock_origin;
    clock_offset  = 0U;
    text_offset   = 0U;
    (void) basic_runtime_column();
    return true;
}

void basic_runtime_shutdown(void)
{
    basic_sound_close();
    basic_c64_close();
    basic_graphics_close();
    // An unfinished draft or trailing PRINT output belongs to BASIC, not the
    // parent's next prompt. Preserve the echo and end its line before returning.
    if (text_offset != 0U) {
        basic_runtime_text_byte('\n');
    }
    (void) fflush(stdout);
    (void) ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, saved_mode);
}

// Every visible application write uses this boundary, including editor echo
// and adapter errors. Preserve offset across soft wraps, reset on hard newline.
void basic_runtime_text_byte(uint8_t byte)
{
    (void) putchar(byte);
    if (byte == '\n' || byte == '\r') {
        text_offset = 0U;
    } else if (byte == '\b') {
        if (text_offset > 0U) {
            --text_offset;
        }
    } else if (byte >= 32U) {
        ++text_offset;
    }
}

void basic_runtime_write(const char* text)
{
    while (*text != '\0') {
        basic_runtime_text_byte((uint8_t) *text++);
    }
}

uint8_t basic_runtime_column(void)
{
    tabos_tty_size_t size;
    if (ioctl(STDOUT_FILENO, TABOS_TTY_GET_SIZE, &size) == 0 && size.columns != 0U) {
        text_columns = size.columns;
    }
    const uint64_t column = text_offset % text_columns;
    if (column > 255U) {
        return 255U;
    }
    return (uint8_t) column;
}

void basic_runtime_put_byte(uint8_t byte)
{
    if (byte == 13U) {
        basic_runtime_text_byte('\n');
    } else if (byte == 29U) {
        basic_runtime_text_byte(' ');
    } else if (byte >= 32U && byte <= 126U) {
        basic_runtime_text_byte((uint8_t) byte);
    }
    // Other PETSCII controls are outside this proof of concept.
}

static int take_byte(void)
{
    if (pending_count == 0U) {
        return 0;
    }
    const uint16_t byte = pending[pending_start];
    pending_start       = (pending_start + 1U) % INPUT_CAPACITY;
    --pending_count;
    if (byte == '\n') {
        return '\r';
    }
    return byte;
}

uint8_t basic_runtime_get_byte(void)
{
    poll_input();
    const int byte = take_byte();
    if (byte == INPUT_OVERRUN) {
        basic_runtime_write("\n?INPUT OVERRUN - TYPEAHEAD DISCARDED\n");
        return 0U;
    }
    return (uint8_t) byte;
}

static char upper_ascii(char byte)
{
    if (byte >= 'a' && byte <= 'z') {
        return (char) (byte - 'a' + 'A');
    }
    return byte;
}

static bool keyword_at(size_t offset, const char* keyword)
{
    const size_t length = strlen(keyword);
    if (length > line_length - offset) {
        return false;
    }
    for (size_t i = 0U; i < length; ++i) {
        if (upper_ascii(line[offset + i]) != keyword[i]) {
            return false;
        }
    }
    return true;
}

static void normalize_command(void)
{
    bool quoted = false;
    bool data   = false;
    for (size_t i = 0U; i < line_length; ++i) {
        if (line[i] == '"') {
            quoted = !quoted;
        } else if (!quoted) {
            if (data) {
                if (line[i] == ':') {
                    data = false;
                }
            } else if (keyword_at(i, "REM")) {
                memcpy(line + i, "REM", 3U);
                return;
            } else if (keyword_at(i, "DATA")) {
                memcpy(line + i, "DATA", 4U);
                data  = true;
                i    += 3U;
            } else {
                line[i] = upper_ascii(line[i]);
            }
        }
    }
}

int basic_runtime_read_byte(bool command)
{
    basic_c64_service(true);
    bool rejected = false;
    for (;;) {
        (void) fflush(stdout);
        poll_input();
        if (break_pending) {
            break_pending = false;
            basic_sid_close();
            line_length   = 0U;
            line_position = 0U;
            line_ready    = false;
            basic_runtime_text_byte('\n');
            return BASIC_INPUT_BREAK;
        }
        if (line_ready) {
            if (line_position < line_length) {
                return (uint8_t) line[line_position++];
            }
            line_ready    = false;
            line_length   = 0U;
            line_position = 0U;
            return '\r';
        }
        if (pending_count == 0U) {
            const uint32_t timeout = basic_sid_active() ? 5U : TABOS_WAIT_TIMEOUT_INFINITE;
            if (tabos_wait(&input_wait, 1U, timeout) < 0) {
                basic_engine_exit(1);
            }
            basic_sid_service();
            continue;
        }
        const int byte = take_byte();
        if (byte == INPUT_OVERRUN) {
            basic_runtime_write("\n?INPUT OVERRUN - SUBMISSION DISCARDED\n");
            line_length = 0U;
            rejected    = true;
        } else if (byte == '\r') {
            if (rejected) {
                // Wait for a replacement line; INPUT must not receive a partial
                // value or a fabricated empty response after rejection.
                rejected = false;
                basic_runtime_write(command ? "READY.\n" : "? ");
            } else {
                if (command) {
                    normalize_command();
                }
                line_ready = true;
                // BASIC's $AACA echoes the submitted CR exactly once.
            }
        } else if (rejected) {
            continue;
        } else if (byte == '\b' || byte == 127) {
            if (line_length != 0U) {
                --line_length;
                basic_runtime_text_byte('\b');
            }
        } else if (byte >= 32 && byte <= 126) {
            if (line_length < LINE_CAPACITY) {
                line[line_length++] = (char) byte;
                basic_runtime_text_byte((uint8_t) byte);
            } else {
                basic_runtime_write("\n?LINE TOO LONG - SUBMISSION DISCARDED\n");
                line_length = 0U;
                rejected    = true;
            }
        } else if (byte != 0) {
            basic_runtime_write("\n?NON-ASCII/CONTROL INPUT - SUBMISSION DISCARDED\n");
            line_length = 0U;
            rejected    = true;
        }
    }
}

bool basic_runtime_break(void)
{
    basic_c64_service(false);
    poll_input();
    const bool result = break_pending;
    break_pending     = false;
    if (result) {
        basic_sid_close();
    }
    return result;
}

void basic_runtime_checkpoint(void)
{
    basic_c64_service(false);
    const uint64_t now = tabos_monotonic_ms();
    if (now - last_pause >= 10U) {
        poll_input();
        (void) fflush(stdout);
        if (tabos_sleep_ms(1U) != 0) {
            basic_engine_exit(1);
        }
        last_pause = tabos_monotonic_ms();
    }
}

uint32_t basic_runtime_jiffies(void)
{
    return (uint32_t) ((clock_offset + (tabos_monotonic_ms() - clock_origin) * 60U / 1000U) % 5184000U);
}

void basic_runtime_set_jiffies(uint32_t value)
{
    clock_origin = tabos_monotonic_ms();
    clock_offset = value % 5184000U;
}

int basic_runtime_key(const char* name, unsigned length)
{
    static const struct {
            const char* name;
            tabos_key_t key;
    } names[] = {
        { "LEFT",  TABOS_KEY_LEFT},
        {"RIGHT", TABOS_KEY_RIGHT},
        {   "UP",    TABOS_KEY_UP},
        { "DOWN",  TABOS_KEY_DOWN},
        {"SPACE", TABOS_KEY_SPACE},
        {    "A",     TABOS_KEY_A},
        {    "B",     TABOS_KEY_B}
    };
    poll_input();
    for (unsigned index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (strlen(names[index].name) == length) {
            bool match = true;
            for (unsigned ch = 0U; ch < length; ++ch) {
                if (upper_ascii(name[ch]) != names[index].name[ch]) {
                    match = false;
                }
            }
            if (match) {
                return held_keys[names[index].key] ? 1 : 0;
            }
        }
    }
    return -1;
}

bool basic_runtime_sleep(uint32_t milliseconds)
{
    const uint64_t end = tabos_monotonic_ms() + milliseconds;
    for (;;) {
        basic_sid_service();
        poll_input();
        if (break_pending) {
            basic_sid_close();
            return false;
        }
        const uint64_t now = tabos_monotonic_ms();
        if (now >= end) {
            return true;
        }
        const uint32_t remaining = (uint32_t) (end - now);
        if (tabos_sleep_ms(remaining < 10U ? remaining : 10U) != 0) {
            basic_engine_exit(1);
        }
    }
}
