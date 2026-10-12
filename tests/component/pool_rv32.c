#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <tabos/application.h>
#include <tabos/internal/input.h>
#include <tabos/internal/display.h>
#include <tabos/internal/console.h>
#include <tabos/internal/runtime.h>
#include <tabos/internal/application.h>
#include <pool/table.h>
#include <pool/render.h>
#include <tabos/platform/storage_backend.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// This optional executable takes the actual SDK-built shell as argv[1].
// Override only the host drive mapping; runtime, interpreter and SDK stay real.
static char storage_root[] = "/tmp/tabos-pool-rv32-XXXXXX";

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "RV32 Pool test failed: %s\n", message);
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

static void pump(void)
{
    for (unsigned int index = 0U; index < 100U; ++index) {
        // Drain real wake notifications without blocking this bounded test pump.
        const platform_runtime_events_t events = platform_runtime_wait_until(platform_time_ms());
        kernel_runtime_update(events);
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

static void new_rack(void)
{
    key(TABOS_KEY_R, 0U);
    key(TABOS_KEY_ENTER, 0U);
}

static void text(const char* value)
{
    while (*value != '\0') {
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

static void boot(void)
{
    check(kernel_runtime_init() && platform_init(getenv("POOL_TEST_WINDOW") == NULL) && kernel_runtime_start(false),
          "runtime startup");
    check(tabos_app_launch_path("T:/shell") == TABOS_APP_RESULT_OK, "launch real RV32 shell");
    pump();
}

static void stop(void)
{
    kernel_runtime_shutdown();
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
static void settle(void)
{
    const uint64_t deadline = platform_time_ms() + 5000U;
    while (platform_time_ms() < deadline) {
        pump();
        if (!kernel_application_system_runnable()) {
            return;
        }
    }
    check(false, "guest reaches input wait");
}

static uint64_t frame_hash(void)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    uint64_t hash                       = 14695981039346656037ULL;
    for (uint32_t y = 0U; y < frame->height; ++y) {
        for (uint32_t x = 0U; x < frame->width; ++x) {
            hash ^= frame->pixels[y * frame->stride_pixels + x];
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

static uint64_t state_hash(void)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    uint64_t hash                       = UINT64_C(14695981039346656037);
    for (unsigned int y = 28U; y < 44U; ++y) {
        for (unsigned int x = 212U; x < 480U; ++x) {
            hash ^= frame->pixels[y * frame->stride_pixels + x];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static uint64_t rack_hash(void)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    uint64_t hash                       = UINT64_C(14695981039346656037);
    for (unsigned int y = 288U; y < 432U; ++y) {
        for (unsigned int x = 890U; x < 1024U; ++x) {
            hash ^= frame->pixels[y * frame->stride_pixels + x];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static bool placement_visible(void)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    const unsigned int x                = 2U * (POOL_TABLE_X + 132U);
    const unsigned int y                = 2U * (POOL_TABLE_Y + 132U);
    return frame->pixels[y * frame->stride_pixels + x] == TABOS_RGB565(219, 183, 105);
}

static void run_for(uint32_t milliseconds)
{
    const uint64_t end = platform_time_ms() + milliseconds;
    while (platform_time_ms() < end) {
        pump();
    }
    /* Held aim/power can render continuously. Only settle after releasing
     * controls or entering an idle state, not while a timed control is active. */
}

static void capture(const char* path)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    FILE* file                          = fopen(path, "wb");
    check(file != NULL, "open screenshot");
    fprintf(file, "P6\n%zu %zu\n255\n", frame->width, frame->height);
    for (uint32_t y = 0U; y < frame->height; ++y) {
        for (uint32_t x = 0U; x < frame->width; ++x) {
            const uint16_t pixel      = frame->pixels[y * frame->stride_pixels + x];
            const unsigned char rgb[] = {(unsigned char) (((pixel >> 11U) & 31U) * 255U / 31U),
                                         (unsigned char) (((pixel >> 5U) & 63U) * 255U / 63U),
                                         (unsigned char) ((pixel & 31U) * 255U / 31U)};
            check(fwrite(rgb, 1U, sizeof(rgb), file) == sizeof(rgb), "write screenshot");
        }
    }
    check(fclose(file) == 0, "close screenshot");
}

static uint64_t motion_hash(void)
{
    const platform_framebuffer_t* frame = display_framebuffer();
    uint64_t hash                       = UINT64_C(14695981039346656037);
    for (unsigned int y = 100U; y < 630U; y += 8U) {
        for (unsigned int x = 115U; x < 1160U; x += 8U) {
            hash = (hash ^ frame->pixels[y * frame->stride_pixels + x]) * UINT64_C(1099511628211);
        }
    }
    return hash;
}

static void shot_latency(bool muted)
{
    new_rack();
    if (muted) {
        key(TABOS_KEY_M, 0U);
    }
    settle();
    const platform_framebuffer_t* frame = display_framebuffer();
    const unsigned int x                = 2U * (POOL_TABLE_X + 132U);
    const unsigned int y                = 2U * (POOL_TABLE_Y + 132U);
    const tabos_color_t cue             = frame->pixels[y * frame->stride_pixels + x];
    tabos_input_event_t fire            = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_SPACE};
    const uint64_t start                = platform_time_ms();
    check(input_submit(&fire), "latency fire down");
    fire.type = TABOS_INPUT_KEY_UP;
    check(input_submit(&fire), "latency fire up");
    while (frame->pixels[y * frame->stride_pixels + x] == cue && platform_time_ms() - start < 5000U) {
        kernel_runtime_update(platform_runtime_wait_until(platform_time_ms()));
    }
    const uint64_t elapsed = platform_time_ms() - start;
    check(elapsed < 5000U, "shot visibly moves within bounded deadline");
    fprintf(stderr, "Pool shot-to-cue-origin-clearance (%s): %llu ms\n", muted ? "muted" : "sound",
            (unsigned long long) elapsed);
    uint64_t last = platform_time_ms(), maximum = 0U, hash = motion_hash();
    const uint64_t end  = last + 1200U;
    unsigned int frames = 0U;
    while (platform_time_ms() < end) {
        kernel_runtime_update(platform_runtime_wait_until(platform_time_ms()));
        const uint64_t next = motion_hash();
        if (next != hash) {
            const uint64_t now = platform_time_ms();
            if (now - last > maximum) {
                maximum = now - last;
            }
            last = now;
            hash = next;
            ++frames;
        }
    }
    fprintf(stderr, "Pool moving frames (%s): %u, largest gap %llu ms\n", muted ? "muted" : "sound", frames,
            (unsigned long long) maximum);
    new_rack();
    if (muted) {
        key(TABOS_KEY_M, 0U);
    }
    settle();
}

int main(int argc, char** argv)
{
    check(argc == 3 || argc == 4, "pass SDK shell, Pool, and optional screenshot path");
    check(mkdtemp(storage_root) != NULL, "temporary storage");
    char shell_path[512], pool_path[512], history_path[512], user_path[512];
    (void) snprintf(shell_path, sizeof(shell_path), "%s/shell", storage_root);
    (void) snprintf(pool_path, sizeof(pool_path), "%s/pool", storage_root);
    (void) snprintf(history_path, sizeof(history_path), "%s/user/history.txt", storage_root);
    (void) snprintf(user_path, sizeof(user_path), "%s/user", storage_root);
    copy(argv[1], shell_path);
    copy(argv[2], pool_path);
    check(setenv("SDL_VIDEODRIVER", "dummy", 1) == 0 && setenv("SDL_AUDIODRIVER", "dummy", 1) == 0,
          "headless environment");
    boot();
    uint64_t baseline = 0U;
    for (unsigned int round = 0U; round < 3U; ++round) {
        fprintf(stderr, "Pool RV32: launch %u\n", round + 1U);
        text("./pool --two-player");
        key(TABOS_KEY_ENTER, 0U);
        settle();
        const uint64_t title_frame = frame_hash();
        key(TABOS_KEY_H, 0U);
        settle();
        check(frame_hash() != title_frame, "title opens instructions");
        key(TABOS_KEY_ESCAPE, 0U);
        settle();
        check(frame_hash() == title_frame, "help returns to title");
        key(TABOS_KEY_LEFT, 0U);
        key(TABOS_KEY_RIGHT, 0U);
        key(TABOS_KEY_ENTER, 0U);
        settle();
        check(tabos_process_count() == 2U, "pool is shell child");
        check(console_next_deadline() == UINT64_MAX, "fullscreen suspends terminal cursor");
        if (round == 0U) {
            shot_latency(false);
            shot_latency(true);
        }
        const uint64_t table        = frame_hash();
        const uint64_t aiming_state = state_hash();
        if (round == 0U) {
            baseline = table;
            if (argc == 4) {
                capture(argv[3]);
            }
        }
        check(table == baseline, "relaunch resets table, aim and power");
        key(TABOS_KEY_H, 0U);
        settle();
        key(TABOS_KEY_H, 0U);
        key(TABOS_KEY_M, 0U);
        key(TABOS_KEY_M, 0U);
        settle();
        run_for(100U);
        check(frame_hash() == table, "static table keeps unchanged pixels");
        key(TABOS_KEY_R, 0U);
        settle();
        check(frame_hash() != table, "restart asks for confirmation");
        key(TABOS_KEY_ESCAPE, 0U);
        settle();
        check(frame_hash() == table && tabos_process_count() == 2U, "Escape cancels restart without quitting");
        key(TABOS_KEY_LEFT, 0U);
        settle();
        check(frame_hash() != table, "quick aim tap and angle wrap");
        key(TABOS_KEY_D, 0U);
        settle();
        check(frame_hash() == table, "opposite aim tap restores rightward aim");
        key(TABOS_KEY_W, 0U);
        settle();
        check(frame_hash() != table, "power tap changes HUD and cue");
        key(TABOS_KEY_DOWN, 0U);
        settle();
        check(frame_hash() == table, "power alias restores selected power");
        tabos_input_event_t held = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_SHIFT};
        check(input_submit(&held), "hold fine adjustment");
        key(TABOS_KEY_D, 0U);
        settle();
        const uint64_t fine = frame_hash();
        held.type           = TABOS_INPUT_KEY_UP;
        check(input_submit(&held), "release fine adjustment");
        new_rack();
        key(TABOS_KEY_D, 0U);
        settle();
        check(frame_hash() != fine, "normal aim differs from fine adjustment");
        new_rack();
        held = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_RIGHT};
        check(input_submit(&held), "hold right");
        run_for(600U);
        const uint64_t rotated = frame_hash();
        check(rotated != table, "held aim responds over ticks and normalized repeats");
        tabos_input_event_t alias = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_D};
        check(input_submit(&alias), "hold second aim alias");
        held.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&held), "release first aim alias");
        run_for(200U);
        alias.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&alias), "release second aim alias");
        settle();
        const uint64_t released = frame_hash();
        check(released != rotated, "second alias maintains aiming");
        run_for(100U);
        check(frame_hash() == released, "aim stops on final release");
        held = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_A};
        check(input_submit(&held), "hold aim across pause");
        key(TABOS_KEY_P, 0U);
        settle();
        const uint64_t paused = frame_hash();
        run_for(150U);
        key(TABOS_KEY_K, 0U);
        settle();
        check(frame_hash() == paused, "pause freezes aim and rejects fire");
        key(TABOS_KEY_P, 0U);
        settle();
        const uint64_t resumed = frame_hash();
        run_for(150U);
        check(frame_hash() == resumed, "resume requires held aim to be released");
        held.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&held), "release paused aim");
        new_rack();
        settle();
        check(frame_hash() == table, "reset restores rack, aim and power");
        key(TABOS_KEY_K, 0U);
        run_for(200U);
        const uint64_t shot = frame_hash();
        check(shot != table && state_hash() != aiming_state, "shot starts motion");
        key(TABOS_KEY_RIGHT, 0U);
        key(TABOS_KEY_UP, 0U);
        key(TABOS_KEY_SPACE, 0U);
        run_for(200U);
        check(frame_hash() != shot, "cue ball continues moving");
        key(TABOS_KEY_P, 0U);
        settle();
        const uint64_t motion_paused = frame_hash();
        run_for(250U);
        key(TABOS_KEY_K, 0U);
        settle();
        check(frame_hash() == motion_paused, "pause freezes moving cue and rejects shot");
        key(TABOS_KEY_P, 0U);
        run_for(200U);
        check(frame_hash() != motion_paused, "resume advances cue");
        const uint64_t deadline = platform_time_ms() + 10000U;
        while (state_hash() != aiming_state && platform_time_ms() < deadline) {
            pump();
            if (placement_visible()) {
                key(TABOS_KEY_ENTER, 0U);
            }
        }
        check(state_hash() == aiming_state, "motion reaches rest and restores aiming");
        settle();
        const uint64_t stopped = frame_hash();
        run_for(250U);
        check(frame_hash() == stopped && stopped != table, "settled turn remains stable");
        key(TABOS_KEY_SPACE, 0U);
        run_for(150U);
        check(state_hash() != aiming_state, "next shot works without reset");
        new_rack();
        settle();
        check(frame_hash() == table, "reset during motion restores initial table");
        held = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_SPACE};
        check(input_submit(&held), "hold fire across reset");
        run_for(150U);
        check(state_hash() != aiming_state, "Space fires like K");
        new_rack();
        key(TABOS_KEY_K, 0U);
        run_for(150U);
        settle();
        check(frame_hash() == table, "held fire alias cannot rearm after reset");
        held.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&held), "release fire");
        key(TABOS_KEY_SPACE, 0U);
        run_for(150U);
        check(state_hash() != aiming_state, "fresh fire press rearms after release");
        new_rack();
        settle();
        held = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_UP};
        check(input_submit(&held), "raise break power");
        run_for(1200U);
        held.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&held), "release break power");
        settle();
        const uint64_t rack = rack_hash();
        key(TABOS_KEY_SPACE, 0U);
        const uint64_t break_deadline = platform_time_ms() + 15000U;
        while (state_hash() != aiming_state && platform_time_ms() < break_deadline) {
            pump();
            if (placement_visible()) {
                key(TABOS_KEY_ENTER, 0U);
            }
        }
        check(state_hash() == aiming_state, "full rack comes to rest");
        settle();
        check(rack_hash() != rack, "break scatters numbered rack");
        if (round == 0U && argc == 4) {
            capture(argv[3]);
        }
        new_rack();
        /* Exactly 45 degrees points the original cue at the bottom side mouth. */
        for (unsigned int tap = 0U; tap < 46U; ++tap) {
            key(TABOS_KEY_RIGHT, 0U);
        }
        held = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_SHIFT};
        check(input_submit(&held), "fine scratch aim");
        for (unsigned int tap = 0U; tap < 6U; ++tap) {
            key(TABOS_KEY_RIGHT, 0U);
        }
        held.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&held), "release fine scratch aim");
        settle();
        /* Queue both before pumping: a Debug pump can outlast the entire shot,
         * so injecting Enter after key() could legitimately confirm placement. */
        tabos_input_event_t scratch_fire = {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_SPACE};
        check(input_submit(&scratch_fire), "scratch fire down");
        scratch_fire.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&scratch_fire), "scratch fire up");
        held = (tabos_input_event_t) {.type = TABOS_INPUT_KEY_DOWN, .key = TABOS_KEY_ENTER};
        check(input_submit(&held), "hold confirm during motion");
        pump();
        const uint64_t scratch_deadline = platform_time_ms() + 10000U;
        while (!placement_visible() && platform_time_ms() < scratch_deadline) {
            pump();
        }
        if (!placement_visible()) {
            capture("/tmp/pool-stage5-scratch-failure.ppm");
        }
        check(placement_visible(), "scratch reaches cue placement after motion stops");
        settle();
        const uint64_t placement = frame_hash();
        run_for(200U);
        check(placement_visible(), "held Enter does not auto-confirm scratch");
        if (round == 0U && argc == 4) {
            capture(argv[3]);
        }
        key(TABOS_KEY_RIGHT, 0U);
        settle();
        check(frame_hash() != placement, "placement arrows move cue preview");
        held.type = TABOS_INPUT_KEY_UP;
        check(input_submit(&held), "release confirm");
        key(TABOS_KEY_ENTER, 0U);
        settle();
        check(state_hash() == aiming_state, "fresh Enter returns cue to play");
        run_for(150U);
        check(state_hash() == aiming_state, "placement confirmation does not shoot");
        key(round == 1U ? TABOS_KEY_ESCAPE : TABOS_KEY_Q, 0U);
        for (unsigned int i = 0U; i < 100U && tabos_process_count() != 1U; ++i) {
            pump();
        }
        int status = -1;
        check(tabos_process_count() == 1U && !tabos_process_system_panicked(), "shell restored");
        check(tabos_app_last_exit_status(&status) && status == 0, "pool exits zero");
        check(console_next_deadline() != UINT64_MAX, "terminal cursor restored");
    }
    /* Default launch runs Player 2 automatically after a deliberate no-contact
     * foul. Pause before thinking can finish, then observe an autonomous break. */
    text("./pool");
    key(TABOS_KEY_ENTER, 0U);
    settle();
    key(TABOS_KEY_ENTER, 0U);
    settle();
    const uint64_t ai_rack     = rack_hash();
    const uint64_t human_state = state_hash();
    for (unsigned int i = 0U; i < 49U; ++i) {
        key(TABOS_KEY_DOWN, 0U);
    }
    key(TABOS_KEY_SPACE, 0U);
    const uint64_t thinking_deadline = platform_time_ms() + 10000U;
    while (state_hash() == human_state && platform_time_ms() < thinking_deadline) {
        pump();
    }
    /* The shot first moves; wait until the gold placement preview appears. */
    while (!placement_visible() && platform_time_ms() < thinking_deadline) {
        pump();
    }
    check(placement_visible(), "computer receives ball in hand");
    key(TABOS_KEY_P, 0U);
    settle();
    const uint64_t paused_ai = frame_hash();
    run_for(300U);
    check(frame_hash() == paused_ai, "pause freezes computer planning");
    key(TABOS_KEY_R, 0U);
    key(TABOS_KEY_ESCAPE, 0U);
    settle();
    check(frame_hash() == paused_ai, "restart cancellation preserves paused computer");
    key(TABOS_KEY_P, 0U);
    const uint64_t ai_deadline = platform_time_ms() + 30000U;
    while (rack_hash() == ai_rack && platform_time_ms() < ai_deadline) {
        pump();
    }
    check(rack_hash() != ai_rack, "computer places and strikes through real RV32 physics");
    if (argc == 4) {
        capture(argv[3]);
    }
    key(TABOS_KEY_R, 0U);
    key(TABOS_KEY_ENTER, 0U);
    settle();
    check(rack_hash() == ai_rack && state_hash() == human_state, "computer mode restarts at human break");
    key(TABOS_KEY_Q, 0U);
    for (unsigned int i = 0U; i < 100U && tabos_process_count() != 1U; ++i) {
        pump();
    }
    check(tabos_process_count() == 1U && !tabos_process_system_panicked(), "computer game restores shell");
    int ai_status = -1;
    check(tabos_app_last_exit_status(&ai_status) && ai_status == 0, "computer game exits zero");
    stop();
    (void) unlink(history_path);
    (void) rmdir(user_path);
    check(unlink(shell_path) == 0 && unlink(pool_path) == 0 && rmdir(storage_root) == 0, "remove fixtures");
    puts("RV32 Pool aiming, motion, pause, rest, turns, confirmed restart, breaks, fouls, placement and computer tests "
         "passed");
    return 0;
}
