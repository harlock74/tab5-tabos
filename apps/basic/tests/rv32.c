#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <tabos/application.h>
#include <tabos/internal/console.h>
#include <tabos/internal/audio.h>
#include <tabos/internal/display.h>
#include <tabos/internal/input.h>
#include <tabos/internal/runtime.h>
#include <tabos/internal/application.h>
#include <tabos/platform/storage_backend.h>

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// This optional executable takes the actual SDK-built shell as argv[1].
// Override only the host drive mapping; runtime, interpreter and SDK stay real.
static char storage_root[] = "/tmp/tabos-basic-rv32-XXXXXX";

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "RV32 BASIC test failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

size_t storage_backend_drive_count(void)
{
    return 1U;
}

bool storage_backend_mount(size_t index, char* letter, char* root, size_t root_size, bool* removable, const char** name)
{
    if (index != 0U || strlen(storage_root) >= root_size) {
        return false;
    }
    strcpy(root, storage_root);
    *letter    = 'T';
    *removable = true;
    *name      = "Shell test";
    return true;
}

void storage_backend_unmount(char letter)
{
    (void) letter;
}

bool storage_backend_info(char letter, uint64_t* total_bytes, uint64_t* free_bytes)
{
    (void) letter;
    *total_bytes = 1024U * 1024U;
    *free_bytes  = 512U * 1024U;
    return true;
}

static bool simulate_audio;
static uint64_t audio_last_ms;
static uint64_t audio_nonzero_samples;
static void pump(void)
{
    for (unsigned int index = 0U; index < 100U; ++index) {
        // Drain real wake notifications without blocking this bounded test pump.
        const platform_runtime_events_t events = platform_runtime_wait_until(platform_time_ms());
        kernel_runtime_update(events);
        const uint64_t now = platform_time_ms();
        if (simulate_audio && now > audio_last_ms) {
            // Exercise the real audio service with a timed test consumer so the
            // harness can inspect PCM content; do not claim audibility.
            uint64_t frames = (now - audio_last_ms) * 44100U / 1000U;
            if (frames > 4410U) {
                frames = 4410U;
            }
            int16_t pcm[512];
            while (frames != 0U) {
                const size_t count = frames < 256U ? (size_t) frames : 256U;
                audio_service_render(pcm, count);
                for (size_t sample = 0U; sample < count * 2U; ++sample) {
                    if (pcm[sample] != 0) {
                        ++audio_nonzero_samples;
                    }
                }
                frames -= count;
            }
        }
        audio_last_ms = now;
    }
}

static void key(tabos_key_t code, uint8_t modifiers)
{
    tabos_input_event_t event = {.type = TABOS_INPUT_KEY_DOWN, .key = code, .modifiers = modifiers};
    check(input_submit(&event), "key down");
    event.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&event), "key up");
    if (code == TABOS_KEY_ENTER) {
        const tabos_input_event_t newline = {.type = TABOS_INPUT_TEXT, .text = "\n"};
        check(input_submit(&newline), "normalized enter text");
    }
    pump();
}

static void text(const char* value)
{
    while (*value != '\0') {
        if (*value == '\b') {
            key(TABOS_KEY_BACKSPACE, 0U);
            ++value;
            continue;
        }
        const tabos_input_event_t down = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_A};
        check(input_submit(&down), "text physical key");
        const tabos_input_event_t event = {
            .type = TABOS_INPUT_TEXT, .text = {*value++, '\0'}
        };
        check(input_submit(&event), "text input");
        const tabos_input_event_t up = {.type = TABOS_INPUT_KEY_UP, .key = TABOS_KEY_A};
        check(input_submit(&up), "text key release");
        pump();
    }
}

static terminal_t transcript;

static void boot(void)
{
    check(kernel_runtime_init() && platform_init(true) && kernel_runtime_start(false), "runtime startup");
    check(terminal_init(&transcript, display_framebuffer(), 2U), "transcript terminal");
    console_rebind(&transcript);
    check(tabos_app_launch_path("T:/shell") == TABOS_APP_RESULT_OK, "launch real RV32 shell");
    pump();
}

static void stop(void)
{
    kernel_runtime_shutdown();
    terminal_shutdown(&transcript);
    platform_shutdown();
}


static void copy(const char* src, const char* dst)
{
    FILE* source      = fopen(src, "rb");
    FILE* destination = fopen(dst, "wb");
    check(source != NULL && destination != NULL, "open artifact");
    char bytes[4096];
    size_t size;
    while ((size = fread(bytes, 1U, sizeof(bytes), source)) > 0U) {
        check(fwrite(bytes, 1U, size, destination) == size, "copy artifact");
    }
    check(!ferror(source) && fclose(source) == 0 && fclose(destination) == 0, "close artifact");
}
static void parent_status(int expected_status)
{
    for (size_t i = 0U; i < 100U && tabos_process_count() != 1U; ++i) {
        pump();
    }
    int status = -1;
    check(tabos_process_count() == 1U && !tabos_process_system_panicked(), "parent shell resumed");
    check(tabos_app_last_exit_status(&status) && status == expected_status, "BASIC expected exit status");
}

static void parent(void)
{
    parent_status(0);
}

static char output[65536];

static const char* capture(void)
{
    size_t used = 0U;
    for (uint64_t row = transcript.first_line; row <= transcript.current_line; ++row) {
        for (size_t col = 0U; col < transcript.line_lengths[row % transcript.line_capacity]; ++col) {
            check(used + 2U < sizeof(output), "transcript capacity");
            char byte      = transcript.cells[(row % transcript.line_capacity) * transcript.columns + col].character;
            output[used++] = byte == '\0' ? ' ' : byte;
        }
        if (transcript.hard_breaks[row % transcript.line_capacity] || row == transcript.current_line) {
            output[used++] = '\n';
        }
    }
    output[used] = '\0';
    return output;
}

static void expect(const char* value)
{
    const uint64_t deadline = platform_time_ms() + 10000U;
    while (strstr(capture(), value) == NULL && platform_time_ms() < deadline) {
        pump();
    }
    if (strstr(capture(), value) == NULL) {
        fprintf(stderr, "Expected: %s\nTranscript:\n%s\n", value, output);
        check(false, "expected BASIC output");
    }
    check(tabos_process_count() == 2U && !tabos_process_system_panicked(), "BASIC remains shell child");
}

static void command(const char* value)
{
    text(value);
    key(TABOS_KEY_ENTER, 0U);
}

static void ready_command(const char* value, const char* expected)
{
    terminal_clear(&transcript);
    command(value);
    expect(expected);
    expect("READY.");
    if (strcmp(expected, "READY.") == 0) {
        check(strstr(capture(), " ERROR") == NULL, "successful command has no BASIC error");
    }
    if (strcmp(expected, "READY.") == 0 && (strncmp(value, "SAVE ", 5U) == 0 || strncmp(value, "LOAD ", 5U) == 0)) {
        check(strchr(capture(), '?') == NULL, "successful storage command has no BASIC error");
    }
}

static void break_program(const char* program)
{
    ready_command("NEW", "READY.");
    command(program);
    terminal_clear(&transcript);
    command("RUN");
    const uint64_t running_until = platform_time_ms() + 100U;
    while (platform_time_ms() < running_until) {
        pump();
    }
    check(strstr(capture(), "READY.") == NULL, "program is still running");
    const uint64_t started = platform_time_ms();
    key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
    expect("BREAK IN 10");
    expect("READY.");
    printf("Break latency: %llu ms (%s)\n", (unsigned long long) (platform_time_ms() - started), program);
    ready_command("LIST", program);
    ready_command("PRINT 2+2", "\n 4 ");
}

static void interaction(void)
{
    ready_command("print \"Hello TabOS\"", "\nHello TabOS\n");
    // Physical Enter plus normalized text must execute a side effect once.
    ready_command("a=0", "READY.");
    ready_command("a=a+1:print a", "\n 1 ");
    check(strstr(strstr(capture(), "READY.") + 6U, "READY.") == NULL, "one READY after Enter");
    terminal_clear(&transcript);
    command("");
    check(strcmp(capture(), "\n\n") == 0, "empty line submits once");
    ready_command("\b\b\bPRON\b\bINT 2+2", "\n 4 ");
    ready_command("print \"HELLO\"", "\nHELLO\n");
    ready_command("print \"A B C\"", "\nA B C\n");
    ready_command("print \"\"", "\n\n");
    ready_command("print \"0123456789 !#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\"", "0123456789");
    ready_command("NEW", "READY.");
    command("10 print \"hello\"");
    command("20 rem Mixed CASE:print 0");
    command("30 data Hello TabOS,\"MiXeD:Value\":read a$,b$:print a$;\":\";b$");
    ready_command("list", "20 REM Mixed CASE:print 0");
    ready_command("run", "\nHello TabOS:MiXeD:Value\n");
    ready_command("NEW", "READY.");
    command("10 input a$");
    command("20 print a$");
    for (unsigned attempt = 0U; attempt < 3U; ++attempt) {
        terminal_clear(&transcript);
        command("run");
        expect("? ");
        if (attempt != 0U) {
            text("unsubmitted Mixed");
        }
        key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
        expect("BREAK IN 10");
        expect("READY.");
        ready_command("list", "10 INPUT A$");
    }
    terminal_clear(&transcript);
    command("run");
    expect("? ");
    command("MiXeD input rem data");
    expect("\nMiXeD input rem data\n");
    expect("READY.");
    char exact[81];
    memcpy(exact, "print \"", 7U);
    memset(exact + 7U, 'X', 72U);
    exact[79] = '"';
    exact[80] = '\0';
    ready_command(exact, "\nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
    // Delete across a visual wrap, then restore the closing quote.
    terminal_clear(&transcript);
    text(exact);
    command("\b\bY\"");
    expect("XY\n");
    expect("READY.");
    terminal_clear(&transcript);
    text(exact);
    command(" ");
    expect("?LINE TOO LONG - SUBMISSION DISCARDED");
    expect("READY.");
    check(strstr(capture(), "\nXXXXXXXXXXXXXXXX") == NULL, "oversize command not executed");
    ready_command("print 7", "\n 7 ");
    // Pending ordinary input survives STOP polling during execution, for GET.
    ready_command("NEW", "READY.");
    command("10 FOR I=1 TO 100:NEXT I");
    command("20 GET A$:IF A$=\"\" THEN 20");
    command("30 GET B$:IF B$=\"\" THEN 30");
    command("40 PRINT A$;B$:END");
    terminal_clear(&transcript);
    command("run");
    text("aZ");
    expect("\naZ\n");
    expect("READY.");
    ready_command("NEW", "READY.");
}

static void input_and_scroll(void)
{
    ready_command("NEW", "READY.");
    command("10 INPUT A");
    command("20 PRINT A*2");
    command("30 END");
    terminal_clear(&transcript);
    command("RUN");
    expect("? ");
    command("oops");
    expect("?REDO FROM START");
    command("12X\b");
    expect("\n 24 ");
    expect("READY.");
    terminal_clear(&transcript);
    command("RUN");
    expect("? ");
    command("");
    expect("\n 0 ");
    expect("READY.");
    ready_command("NEW", "READY.");
    command("10 INPUT A,B");
    command("20 PRINT A;B:END");
    terminal_clear(&transcript);
    command("RUN");
    expect("? ");
    command("2,3");
    expect("\n 2  3 ");
    expect("READY.");
    ready_command("NEW", "READY.");
    command("10 INPUT A$");
    command("20 PRINT A$:END");
    terminal_clear(&transcript);
    command("RUN");
    expect("? ");
    command("MistakX\be");
    expect("\nMistake\n");
    expect("READY.");
    terminal_clear(&transcript);
    command("RUN");
    expect("? ");
    command("");
    expect("READY.");
    check(strstr(capture(), "Mistake") == NULL, "empty INPUT leaves a fresh string empty");
    ready_command("NEW", "READY.");
    command("10 FOR I=1 TO 300:PRINT I:NEXT I");
    terminal_clear(&transcript);
    command("RUN");
    expect("\n 300 ");
    expect("READY.");
    check(transcript.first_line > 0U && transcript.viewport_top > 0U, "output scrolls beyond history capacity");
    // Keep the scrolled terminal intact while entering and correcting a command.
    command("print 2+3\b4");
    expect("\n 6 ");
    check(terminal_is_at_end(&transcript), "editor follows the live terminal after scrolling");
}

static void storage_roundtrip(void)
{
    ready_command("NEW", "READY.");
    command("10 PRINT \"HELLO TABOS\"");
    command("20 FOR I=1 TO 3");
    command("30 PRINT I");
    command("40 NEXT I");
    command("50 END");
    ready_command("SAVE \"HELLO\"", "READY.");
    char filename[512];
    (void) snprintf(filename, sizeof(filename), "%s/basic/HELLO", storage_root);
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "SAVE transcript:\n%s\n", capture());
    }
    check(file != NULL, "default directory and file auto-created");
    unsigned char original[512];
    const size_t size = fread(original, 1U, sizeof(original), file);
    check(!ferror(file) && fclose(file) == 0 && size > 4U && size < sizeof(original), "read saved program");
    check(original[0] == 1U && original[1] == 8U, "PRG address header");
    ready_command("NEW", "READY.");
    ready_command("LIST", "READY.");
    check(strstr(capture(), "10 PRINT") == NULL, "NEW removes stored program");
    ready_command("LOAD \"HELLO\"", "READY.");
    ready_command("LIST", "50 END");
    expect("10 PRINT \"HELLO TABOS\"");
    ready_command("RUN", "\nHELLO TABOS\n 1 \n 2 \n 3 ");
    ready_command("SAVE \"T:/basic/RECOVERED.BAS\"", "READY.");
    (void) snprintf(filename, sizeof(filename), "%s/basic/RECOVERED.BAS", storage_root);
    file = fopen(filename, "rb");
    check(file != NULL, "explicit SAVE path");
    unsigned char recovered[512];
    const size_t recovered_size = fread(recovered, 1U, sizeof(recovered), file);
    check(!ferror(file) && fclose(file) == 0 && size == recovered_size && memcmp(original, recovered, size) == 0,
          "byte-exact program recovery");
    command("10 PRINT \"OVERWRITE\"");
    ready_command("SAVE \"HELLO\"", "READY.");
    ready_command("NEW", "READY.");
    ready_command("LOAD \"HELLO\"", "READY.");
    ready_command("RUN", "\nOVERWRITE\n");
    // Restore HELLO for persistence checks after subsequent process relaunches.
    ready_command("LOAD \"T:/basic/RECOVERED.BAS\"", "READY.");
    ready_command("SAVE \"HELLO\"", "READY.");
    ready_command("LOAD \"MISSING\"", "?FILE NOT FOUND");
    ready_command("LIST", "10 PRINT \"HELLO TABOS\"");
    ready_command("SAVE \"\"", "?INVALID FILENAME");
    ready_command("LOAD \"\"", "?INVALID FILENAME");
    (void) snprintf(filename, sizeof(filename), "%s/basic/BAD", storage_root);
    file = fopen(filename, "wb");
    check(file != NULL, "open malformed fixture");
    original[1] = 0xc0U;
    check(fwrite(original, 1U, size, file) == size && fclose(file) == 0, "write malformed fixture");
    ready_command("LOAD \"BAD\"", "?INVALID TOKENIZED PROGRAM");
    ready_command("LIST", "10 PRINT \"HELLO TABOS\"");
    ready_command("RUN", "\nHELLO TABOS\n");
}

// The Python semantic suite exports the same expectations for the real SDK ELF.
// Outer whitespace differs with terminal capture; internal spacing stays exact.
static char* trimmed(char* value)
{
    while (isspace((unsigned char) *value)) {
        ++value;
    }
    size_t length = strlen(value);
    while (length > 0U && isspace((unsigned char) value[length - 1U])) {
        value[--length] = '\0';
    }
    return value;
}

static void compatibility(const char* path)
{
    FILE* fixture = fopen(path, "r");
    check(fixture != NULL, "open compatibility corpus");
    char row[2048];
    unsigned int commands = 0U;
    while (fgets(row, sizeof(row), fixture) != NULL) {
        check(strchr(row, '\n') != NULL, "complete compatibility corpus row");
        char* command_text = strchr(row, '\t');
        check(command_text != NULL, "corpus case separator");
        *command_text++ = '\0';
        char* expected  = strchr(command_text, '\t');
        check(expected != NULL, "corpus expectation separator");
        *expected++                         = '\0';
        expected[strcspn(expected, "\r\n")] = '\0';
        char* write                         = expected;
        for (char* read = expected; *read != '\0'; ++read) {
            if (read[0] == '\\' && read[1] == 'n') {
                *write++ = '\n';
                ++read;
            } else {
                *write++ = *read;
            }
        }
        *write = '\0';
        terminal_clear(&transcript);
        command(command_text);
        expect(command_text);
        (void) capture();
        check(strncmp(output, command_text, strlen(command_text)) == 0 && output[strlen(command_text)] == '\n',
              "compatibility exact command echo");
        if (strcmp(expected, "@LINE@") != 0) {
            expect("READY.");
            (void) capture();
            char* result = strchr(output, '\n');
            check(result != NULL, "compatibility command echo");
            ++result;
            char* ready = strstr(result, "READY.");
            check(ready != NULL, "compatibility READY delimiter");
            *ready = '\0';
            if (strcmp(trimmed(result), expected) != 0) {
                fprintf(stderr, "Case %s, command %s\nExpected [%s]\nActual [%s]\n", row, command_text, expected,
                        trimmed(result));
                check(false, "compatibility exact output");
            }
        }
        ++commands;
    }
    check(commands > 0U, "nonempty compatibility corpus");
    check(!ferror(fixture) && fclose(fixture) == 0, "close compatibility corpus");
    printf("RV32 compatibility: %u command expectations passed\n", commands);
    fflush(stdout);
}

static uint16_t logical_pixel(unsigned x, unsigned y)
{
    const platform_framebuffer_t* framebuffer = display_framebuffer();
    check(framebuffer != NULL && framebuffer->width == 1280U, "physical graphics geometry");
    return framebuffer->pixels[(60U + y * 3U) * framebuffer->stride_pixels + 160U + x * 3U];
}

static uint16_t c64_pixel(unsigned x, unsigned y)
{
    return logical_pixel(x, y);
}

static void c64_benchmarks(void)
{
    const char* names[]  = {"C64_SCREEN_1000", "C64_COLOR_1000", "C64_BACKGROUND_1000", "C64_SPRITE_MOVE_1000",
                            "C64_MIXED_2000"};
    const char* bodies[] = {"POKE 1024+I,65", "POKE 55296+I,I AND 15", "POKE 53281,I AND 15", "POKE 53248,I AND 255",
                            "POKE 1024+I,65:POKE 55296+I,I AND 15"};
    for (unsigned index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        ready_command("NEW", "READY.");
        char statement[128];
        (void) snprintf(statement, sizeof(statement), "10 FOR I=0 TO 999:%s:NEXT", bodies[index]);
        command(statement);
        command("20 PRINT 987:END");
        terminal_clear(&transcript);
        text("RUN");
        const uint64_t start = platform_time_ms();
        key(TABOS_KEY_ENTER, 0U);
        expect("\n 987 ");
        expect("READY.");
        printf("BENCH %s %llu ms\n", names[index], (unsigned long long) (platform_time_ms() - start));
        fflush(stdout);
    }
    const uint64_t redraw_start = platform_time_ms();
    ready_command("POKE 53280,0", "READY.");
    printf("BENCH C64_FULL_REDRAW %llu ms\n", (unsigned long long) (platform_time_ms() - redraw_start));
    fflush(stdout);
    ready_command("TEXT", "READY.");
}

static void graphics_benchmarks(void)
{
    ready_command("GRAPHICS:SPRITE 0,16,16", "READY.");
    for (unsigned row = 0U; row < 16U; ++row) {
        char statement[80];
        (void) snprintf(statement, sizeof(statement), "SPRITEROW 0,%u,\"123456789ABCDEF1\"", row);
        ready_command(statement, "READY.");
    }
    ready_command("SPRITESHOW 0", "READY.");
    FILE* fixture = fopen(TABOS_BASIC_EXAMPLES_DIR "/../tests/benchmarks.tsv", "r");
    check(fixture != NULL, "open benchmark corpus");
    char row[256];
    while (fgets(row, sizeof(row), fixture) != NULL) {
        char* name  = strtok(row, "\t");
        char* count = strtok(NULL, "\t");
        char* body  = strtok(NULL, "\n");
        check(name != NULL && count != NULL && body != NULL, "benchmark row");
        ready_command("NEW", "READY.");
        char statement[100];
        (void) snprintf(statement, sizeof(statement), "10 FOR I=1 TO %s:%s:NEXT", count, body);
        command(statement);
        command("20 PRINT 987:END");
        terminal_clear(&transcript);
        text("RUN");
        const uint64_t start = platform_time_ms();
        key(TABOS_KEY_ENTER, 0U);
        expect("\n 987 ");
        expect("READY.");
        printf("BENCH %s %s %llu ms\n", name, count, (unsigned long long) (platform_time_ms() - start));
        fflush(stdout);
    }
    check(fclose(fixture) == 0, "close benchmark corpus");
    ready_command("TEXT", "READY.");
}

static void graphics_acceptance(void)
{
    char data_directory[512];
    char example_directory[512];
    char example_path[512];
    (void) snprintf(data_directory, sizeof(data_directory), "%s/data", storage_root);
    (void) snprintf(example_directory, sizeof(example_directory), "%s/data/basic", storage_root);
    check(mkdir(data_directory, 0700) == 0 && mkdir(example_directory, 0700) == 0, "packaged example directories");
    (void) snprintf(example_path, sizeof(example_path), "%s/AMAZING.prg", example_directory);
    copy(TABOS_BASIC_EXAMPLES_DIR "/AMAZING.prg", example_path);
    (void) snprintf(example_path, sizeof(example_path), "%s/C64DODGE.prg", example_directory);
    copy(TABOS_BASIC_EXAMPLES_DIR "/C64DODGE.prg", example_path);
    terminal_clear(&transcript);
    command("bin/basic");
    expect("READY.");
    ready_command("LOAD \"AMAZING.prg\"", "READY.");
    terminal_clear(&transcript);
    command("RUN");
    expect("WHAT ARE YOUR WIDTH AND LENGTH? ");
    command("4,3");
    expect(".--");
    expect(":--");
    expect("READY.");
    ready_command("LOAD \"C64DODGE.prg\"", "READY.");
    command("240 SLEEP 16:GOTO 110");
    terminal_clear(&transcript);
    command("RUN");
    expect("PRESS ENTER TO START? ");
    key(TABOS_KEY_ENTER, 0U);
    const uint64_t dodge_until = platform_time_ms() + 80U;
    while (platform_time_ms() < dodge_until) {
        pump();
    }
    check(console_graphics_active() && audio_service_power_inhibited(), "packaged C64DODGE graphics and SID active");
    tabos_input_event_t dodge_right = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_RIGHT};
    check(input_submit(&dodge_right), "C64DODGE held right");
    pump();
    dodge_right.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&dodge_right), "C64DODGE right release");
    key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
    expect("BREAK IN");
    expect("READY.");
    check(!console_graphics_active() && !audio_service_power_inhibited(), "packaged C64DODGE break cleanup");
    simulate_audio = false;
    ready_command("SOUND 440,20", "READY.");
    check(!audio_service_power_inhibited(), "headless audio advance closes stream");
    ready_command("PRINT 123", "\n 123 ");
    simulate_audio = true;
    ready_command("SOUND 440,100", "READY.");
    check(audio_nonzero_samples > 0U, "non-silent PCM reached timed consumer");

    const uint64_t sid_samples = audio_nonzero_samples;
    ready_command("POKE 54296,15:POKE 54277,0:POKE 54278,240", "READY.");
    ready_command("POKE 54272,214:POKE 54273,28:POKE 54276,33", "READY.");
    const uint64_t sid_until = platform_time_ms() + 80U;
    while (platform_time_ms() < sid_until) {
        pump();
    }
    check(audio_service_power_inhibited(), "SID stream remains active cooperatively");
    check(audio_nonzero_samples > sid_samples, "non-silent SID PCM reached timed consumer");
    ready_command("PRINT PEEK(54272);PEEK(54273);PEEK(54276);PEEK(54296)", "\n 214  28  33  15 ");
    ready_command("POKE 54276,32", "READY.");
    const uint64_t release_until = platform_time_ms() + 40U;
    while (platform_time_ms() < release_until) {
        pump();
    }
    check(!audio_service_power_inhibited(), "SID release closes stream");
    ready_command("SOUND 440,20", "READY.");

    ready_command("NEW", "READY.");
    command("10 POKE 54296,15:POKE 54272,214:POKE 54273,28");
    command("20 POKE 54276,33:POKE 53280,0:POKE 53269,1");
    command("30 POKE 53248,I:I=(I+2)AND255:GOTO 30");
    ready_command("SAVE \"SIDGFX\"", "READY.");
    ready_command("NEW", "READY.");
    ready_command("LOAD \"SIDGFX\"", "READY.");
    terminal_clear(&transcript);
    command("RUN");
    const uint64_t combined_until = platform_time_ms() + 80U;
    while (platform_time_ms() < combined_until) {
        pump();
    }
    check(console_graphics_active() && audio_service_power_inhibited(), "combined SID and virtual sprite active");
    key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
    expect("BREAK IN 30");
    expect("READY.");
    check(!console_graphics_active() && !audio_service_power_inhibited(), "combined Ctrl+C cleanup");
    ready_command("LIST", "10 POKE 54296,15:POKE 54272,214:POKE 54273,28");
    ready_command("PRINT 1+PEEK(54297)", "?UNSUPPORTED C64 ADDRESS $D419");
    ready_command("PRINT 123", "\n 123 ");

    ready_command("GRAPHICS", "READY.");
    check(console_graphics_active(), "graphics acquired");
    ready_command("CLS:COLOR 1:PSET 10,20:PRESENT", "READY.");
    check(logical_pixel(10U, 20U) == 0xffffU && logical_pixel(11U, 20U) == 0U, "pixel and scale");
    ready_command("CLS:LINE -5,0,5,0:RECT 10,10,5,4,1:CIRCLE 30,30,3:PRESENT", "READY.");
    check(logical_pixel(0U, 0U) == 0xffffU && logical_pixel(5U, 0U) == 0xffffU, "clipped line endpoints");
    check(logical_pixel(14U, 13U) == 0xffffU && logical_pixel(15U, 13U) == 0U, "filled rectangle extent");
    check(logical_pixel(30U, 27U) == 0xffffU && logical_pixel(30U, 30U) == 0U, "circle radius");
    ready_command("CLS:SPRITE 0,2,1:SPRITEROW 0,0,\".1\"", "READY.");
    ready_command("SPRITEPOS 0,10,20:SPRITESHOW 0:PRESENT", "READY.");
    check(logical_pixel(10U, 20U) == 0U && logical_pixel(11U, 20U) == 0xffffU, "transparent sprite");
    ready_command("SPRITEPOS 0,20,20:PRESENT", "READY.");
    check(logical_pixel(11U, 20U) == 0U && logical_pixel(21U, 20U) == 0xffffU, "sprite move restores background");
    ready_command("TEXT", "READY.");
    check(!console_graphics_active(), "TEXT restores terminal");
    ready_command("POKE 1024,65:POKE 55296,2:POKE 53280,0:POKE 53281,6", "READY.");
    check(console_graphics_active(), "virtual C64 graphics acquired");
    check(c64_pixel(0U, 0U) == 0U && c64_pixel(20U, 12U) == 0x0015U && c64_pixel(22U, 12U) == 0x8800U,
          "virtual C64 border, background and glyph color");
    ready_command("PRINT PEEK(1024);PEEK(55296);PEEK(53280);PEEK(53281)", "\n 65  2  0  6 ");
    ready_command("FOR I=832 TO 894:POKE I,0:NEXT:POKE 832,255:POKE 2040,13", "READY.");
    ready_command("POKE 53248,100:POKE 53249,80:POKE 53287,5:POKE 53269,1", "READY.");
    check(c64_pixel(100U, 80U) == 0x066aU && c64_pixel(108U, 80U) != 0x066aU,
          "virtual C64 sprite pointer and transparency");
    ready_command("GRAPHICS", "READY.");
    check(console_graphics_active(), "native graphics takes C64 canvas ownership");
    ready_command("TEXT", "READY.");
    check(!console_graphics_active(), "TEXT restores terminal after C64/native transition");
    ready_command("NEW", "READY.");
    command("10 FOR I=0 TO 999:POKE 1024+I,65:NEXT:GOTO 10");
    terminal_clear(&transcript);
    command("RUN");
    check(console_graphics_active(), "C64 POKE loop graphics active");
    const uint64_t c64_break_start = platform_time_ms();
    key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
    expect("BREAK IN 10");
    expect("READY.");
    printf("C64 graphics break: %llu ms\n", (unsigned long long) (platform_time_ms() - c64_break_start));
    check(!console_graphics_active(), "C64 break releases graphics");
    ready_command("LIST", "10 FOR I=0 TO 999:POKE 1024+I,65:NEXT:GOTO 10");
    ready_command("POKE 53280,2", "READY.");
    key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
    parent();
    check(!console_graphics_active(), "C64 exit releases graphics");
    terminal_clear(&transcript);
    command("pwd");
    check(strstr(capture(), "T:/") != NULL, "C64 exit shell usable");
    command("bin/basic");
    expect("READY.");
    const tabos_key_t keys[] = {TABOS_KEY_LEFT,  TABOS_KEY_RIGHT, TABOS_KEY_UP, TABOS_KEY_DOWN,
                                TABOS_KEY_SPACE, TABOS_KEY_A,     TABOS_KEY_B};
    const char* names[]      = {"LEFT", "RIGHT", "UP", "DOWN", "SPACE", "A", "B"};
    for (unsigned i = 0U; i < 7U; ++i) {
        ready_command("NEW", "READY.");
        char statement[80];
        (void) snprintf(statement, sizeof(statement), "10 IF KEY(\"%s\")=0 THEN 10", names[i]);
        command(statement);
        command("20 PRINT 456");
        terminal_clear(&transcript);
        command("RUN");
        tabos_input_event_t event = {.type = TABOS_INPUT_KEY_DOWN, .key = keys[i]};
        check(input_submit(&event), "held key down");
        pump();
        expect("\n 456 ");
        expect("READY.");
        event.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&event), "held key up");
        pump();
        (void) snprintf(statement, sizeof(statement), "PRINT KEY(\"%s\")", names[i]);
        ready_command(statement, "\n 0 ");
    }
    ready_command("NEW", "READY.");
    command("10 GRAPHICS:CLS:COLOR 7:RECT 1,1,10,10,1:PRESENT");
    command("20 TEXT:PRINT 789");
    ready_command("SAVE \"GFX\"", "READY.");
    ready_command("NEW", "READY.");
    ready_command("LOAD \"GFX\"", "READY.");
    ready_command("LIST", "10 GRAPHICS:CLS:COLOR 7:RECT 1,1,10,10,1:PRESENT");
    ready_command("RUN", "\n 789 ");
    for (unsigned cycle = 0U; cycle < 5U; ++cycle) {
        ready_command("NEW", "READY.");
        command("10 GRAPHICS:COLOR 5:CIRCLE 160,100,512,1:SLEEP 5000:GOTO 10");
        terminal_clear(&transcript);
        command("RUN");
        check(console_graphics_active(), "running graphics");
        const uint64_t start = platform_time_ms();
        key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
        expect("BREAK IN 10");
        expect("READY.");
        printf("Graphics break: %llu ms\n", (unsigned long long) (platform_time_ms() - start));
        check(!console_graphics_active(), "break releases graphics");
        ready_command("LIST", "10 GRAPHICS");
        ready_command("GRAPHICS", "READY.");
        ready_command("TEXT", "READY.");
        ready_command("GRAPHICS", "READY.");
        key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
        parent();
        check(!console_graphics_active(), "exit releases graphics");
        terminal_clear(&transcript);
        command("pwd");
        check(strstr(capture(), "T:/") != NULL, "graphics exit shell usable");
        command("bin/basic");
        expect("READY.");
    }
    (void) snprintf(example_path, sizeof(example_path), "%s/basic/PONG", storage_root);
    copy(TABOS_BASIC_EXAMPLES_DIR "/PONG.prg", example_path);
    ready_command("LOAD \"PONG\"", "READY.");
    terminal_clear(&transcript);
    command("RUN");
    const uint64_t game_start = platform_time_ms();
    while (platform_time_ms() - game_start < 500U) {
        pump();
    }
    check(console_graphics_active(), "Pong graphics active");
    tabos_input_event_t paddle = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_DOWN};
    check(input_submit(&paddle), "Pong paddle down");
    const uint64_t held_start = platform_time_ms();
    while (platform_time_ms() - held_start < 500U) {
        pump();
    }
    paddle.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&paddle), "Pong paddle release");
    paddle.type = TABOS_INPUT_KEY_DOWN;
    paddle.key  = TABOS_KEY_B;
    check(input_submit(&paddle), "Pong TEXT exit");
    expect("READY.");
    paddle.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&paddle), "Pong exit release");
    check(!console_graphics_active(), "Pong restores terminal");
    ready_command("PRINT PY>90", "\n-1 ");
    check(unlink(example_path) == 0, "remove Pong fixture");
    ready_command("NEW", "READY.");
    command("10 GRAPHICS:IF KEY(\"B\")=0 THEN 10");
    command("20 TEXT:END");
    terminal_clear(&transcript);
    command("RUN");
    tabos_input_event_t exit_key        = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_B};
    const tabos_input_event_t exit_text = {.type = TABOS_INPUT_TEXT, .text = "b"};
    check(input_submit(&exit_key) && input_submit(&exit_text), "physical B and cooked text");
    expect("READY.");
    expect("\nb");
    exit_key.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&exit_key), "B release after game exit");
    key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
    parent();
    check(strstr(capture(), "\nb\nT:/>") != NULL, "shell prompt starts after BASIC draft line");
    key(TABOS_KEY_BACKSPACE, 0U);
    command("pwd");
    check(strstr(capture(), "\nT:/\n") != NULL, "shell has no leftover editable B");
    command("bin/basic");
    expect("READY.");

    ready_command("NEW", "READY.");
    command("10 GRAPHICS:SOUND 440,2000:GOTO 10");
    terminal_clear(&transcript);
    command("RUN");
    key(TABOS_KEY_C, TABOS_MODIFIER_CONTROL);
    expect("BREAK IN 10");
    expect("READY.");
    check(!audio_service_power_inhibited() && !console_graphics_active(), "tone break cleanup");
    command("RUN");
    key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
    parent();
    check(!audio_service_power_inhibited() && !console_graphics_active(), "tone exit cleanup");
    terminal_clear(&transcript);
    command("bin/basic");
    expect("READY.");
    graphics_benchmarks();
    ready_command("SOUND 440,20", "READY.");
    c64_benchmarks();
    ready_command("GRAPHICS", "READY.");
    ready_command("COLOR -1", "?ILLEGAL QUANTITY");
    check(!console_graphics_active(), "error restores terminal");
    ready_command("PRINT 123", "\n 123 ");
    key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
    parent();
    char file[512];
    (void) snprintf(file, sizeof(file), "%s/basic/GFX", storage_root);
    check(unlink(file) == 0, "remove graphical program fixture");
    (void) snprintf(file, sizeof(file), "%s/basic/SIDGFX", storage_root);
    check(unlink(file) == 0, "remove SID graphics program fixture");
    (void) snprintf(file, sizeof(file), "%s/AMAZING.prg", example_directory);
    check(unlink(file) == 0, "remove packaged Amazing fixture");
    (void) snprintf(file, sizeof(file), "%s/C64DODGE.prg", example_directory);
    check(unlink(file) == 0, "remove packaged C64DODGE fixture");
    check(rmdir(example_directory) == 0 && rmdir(data_directory) == 0, "remove packaged example directories");
    puts("Stage H actual RV32 graphics, input, sound, storage and five lifecycle cycles passed");
}

int main(int argc, char** argv)
{
    check(argc == 3 || argc == 4 || (argc == 5 && strcmp(argv[4], "--policy") == 0),
          "pass SDK shell and BASIC artifacts, optional compatibility corpus and --policy");
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    char shell_path[512], basic_path[512], history_path[512], user_path[512];
    (void) snprintf(shell_path, sizeof(shell_path), "%s/shell", storage_root);
    char bin_path[512];
    (void) snprintf(bin_path, sizeof(bin_path), "%s/bin", storage_root);
    check(mkdir(bin_path, 0700) == 0, "application bin directory");
    (void) snprintf(basic_path, sizeof(basic_path), "%s/bin/basic", storage_root);
    (void) snprintf(history_path, sizeof(history_path), "%s/user/history.txt", storage_root);
    (void) snprintf(user_path, sizeof(user_path), "%s/user", storage_root);
    copy(argv[1], shell_path);
    copy(argv[2], basic_path);
    check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0 && setenv("SDL_AUDIODRIVER", "dummy", 1) == 0,
          "headless environment");
    boot();
    const bool graphics_only = argc == 4 && strcmp(argv[3], "--graphics") == 0;
    if (!graphics_only) {
        for (unsigned round = 0U; round < 4U; ++round) {
            terminal_clear(&transcript);
            command("bin/basic");
            expect("READY.");
            expect("38911 BASIC BYTES FREE");
            if (round == 0U) {
                if (argc >= 4) {
                    compatibility(argv[3]);
                    char compatibility_file[512];
                    (void) snprintf(compatibility_file, sizeof(compatibility_file), "%s/basic/COMPAT", storage_root);
                    check(unlink(compatibility_file) == 0, "remove compatibility save");
                }
                storage_roundtrip();
            } else {
                ready_command("LOAD \"HELLO\"", "READY.");
                ready_command("RUN", "\nHELLO TABOS\n 1 \n 2 \n 3 ");
            }
            interaction();
            ready_command("PRINT 2+2", "\n 4 ");
            command("10 PRINT \"HELLO TABOS\"");
            command("20 END");
            ready_command("RUN", "\nHELLO TABOS\n");
            ready_command("LIST", "10 PRINT \"HELLO TABOS\"");
            ready_command("NEW", "READY.");
            command("10 FOR I=1 TO 5");
            command("20 PRINT I");
            command("30 NEXT I");
            command("40 END");
            ready_command("RUN", "\n 1 \n 2 \n 3 \n 4 \n 5 ");
            ready_command("PRINT 2+3\b4", "\n 6 ");
            break_program("10 GOTO 10");
            break_program("10 PRINT \"LOOP\":GOTO 10");
            if (round == 0U) {
                break_program("10 INPUT A$:GOTO 10");
                break_program("10 GET A$:IF A$=\"\" THEN 10");
                break_program("10 A=SQR(12345)+SIN(1)+COS(2)+LOG(3)+EXP(1):GOTO 10");
                input_and_scroll();
            }
            ready_command("PRINT PEEK(0)", "\n 0 ");
            ready_command("POKE 0,0", "READY.");
            const char* denied[] = {
                "SYS 1",     "SYS 40960", "PRINT USR(0)",   "WAIT 1,1", "VERIFY \"T:/test\"", "OPEN 255,8,255,\"X\"",
                "CLOSE 255", "CMD 255",   "IF 1 THEN SYS 1"};
            for (size_t i = 0U; i < sizeof(denied) / sizeof(denied[0]); ++i) {
                ready_command(denied[i], "?UNSUPPORTED IN TABOS STAGE B");
                ready_command("PRINT 2+2", "\n 4 ");
            }
            ready_command("PRINT 1+PEEK(65535)", "?UNSUPPORTED C64 ADDRESS $FFFF");
            ready_command("PRINT 2+2", "\n 4 ");
            ready_command("NEW", "READY.");
            command("10 SYS 1");
            ready_command("RUN", "?UNSUPPORTED IN TABOS STAGE B");
            ready_command("NEW", "READY.");
            command("10 INPUT A$");
            command("20 PRINT A$");
            command("30 END");
            terminal_clear(&transcript);
            command("RUN");
            expect("?");
            command("PRESERVED");
            expect("\nPRESERVED\n");
            expect("READY.");
            ready_command("NEW", "READY.");
            command("10 GET A$:IF A$=\"\" THEN 10");
            command("20 PRINT A$:END");
            terminal_clear(&transcript);
            command("RUN");
            text("Z");
            expect("\nZ\n");
            expect("READY.");
            if (round == 1U) {
                ready_command("NEW", "READY.");
                command("10 INPUT A$");
                terminal_clear(&transcript);
                command("RUN");
                expect("? ");
            }
            if (round == 2U) {
                ready_command("NEW", "READY.");
                command("10 GOTO 10");
                command("RUN");
            }
            if (round == 3U) {
                text("print \"unsubmitted draft");
            }
            key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
            parent();
            terminal_clear(&transcript);
            command("pwd");
            check(strstr(capture(), "T:/") != NULL, "shell usable after BASIC exit");
        }
        // Explicitly verify Ctrl+Q remains reserved while GET is waiting for a key.
        terminal_clear(&transcript);
        command("bin/basic");
        expect("READY.");
        command("10 GET A$:IF A$=\"\" THEN 10");
        command("RUN");
        key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
        parent();
        terminal_clear(&transcript);
        command("pwd");
        check(strstr(capture(), "T:/") != NULL, "shell usable after GET exit");
        if (argc == 5) {
            terminal_clear(&transcript);
            command("bin/basic");
            expect("READY.");
            ready_command("PRINT (TI>=0 AND TI<5184000);LEN(TI$)", "\n-1  6 ");
            ready_command("TI=100", "?SYNTAX  ERROR");
            ready_command("TI$=\"12345\"", "?ILLEGAL QUANTITY  ERROR");
            ready_command("PRINT \"ABC\";POS(0)", "\nABC 3 ");
            ready_command("PRINT ST", "\n 0 ");
            ready_command("TI$=\"000000\":PRINT TI$", "\n000000\n");
            ready_command("TI$=\"123456\":PRINT TI$", "\n123456\n");
            ready_command("TI$=\"235959\":PRINT TI$", "\n235959\n");
            const char* invalid_times[] = {"240000", "126000", "12345", "1234567", "ABCDEF"};
            for (size_t index = 0U; index < sizeof(invalid_times) / sizeof(invalid_times[0]); ++index) {
                char statement[40];
                (void) snprintf(statement, sizeof(statement), "TI$=\"%s\"", invalid_times[index]);
                ready_command(statement, "?ILLEGAL QUANTITY  ERROR");
            }
            ready_command("PRINT 2+2", "\n 4 ");
            key(TABOS_KEY_Q, TABOS_MODIFIER_CONTROL);
            parent();
            terminal_clear(&transcript);
            command("pwd");
            check(strstr(capture(), "T:/") != NULL, "shell survives clock/cursor checks");
            puts("Stage H0: TI$ assignment/errors, POS and shell recovery passed");
        }
    }
    graphics_acceptance();
    stop();
    check(unlink(history_path) == 0 && unlink(shell_path) == 0 && unlink(basic_path) == 0, "clean files");
    const char* saved_names[] = {"HELLO", "RECOVERED.BAS", "BAD"};
    for (size_t i = 0U; !graphics_only && i < sizeof(saved_names) / sizeof(saved_names[0]); ++i) {
        char path[512];
        (void) snprintf(path, sizeof(path), "%s/basic/%s", storage_root, saved_names[i]);
        check(unlink(path) == 0, "remove persistence fixture");
    }
    char basic_directory[512];
    (void) snprintf(basic_directory, sizeof(basic_directory), "%s/basic", storage_root);
    check(rmdir(basic_directory) == 0, "no unexpected BASIC files or temporary leftovers");
    check(rmdir(bin_path) == 0, "remove application bin directory");
    check(rmdir(user_path) == 0 && rmdir(storage_root) == 0, "clean isolated storage");
    if (graphics_only) {
        puts("BASIC actual RV32 graphics acceptance passed");
    } else {
        puts("BASIC actual RV32 acceptance passed (prompt, INPUT, loop, draft, GET and graphics)");
    }
    return 0;
}
