#include <assert.h>
#include <basic/sid.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <tabos/audio.h>

enum {
    TEST_STREAM          = 1,
    TEST_BUFFER_CAPACITY = 32768,
    TEST_FRAME_BYTES     = sizeof(int16_t)
};

static uint64_t now_ms;
static uint64_t empty_status_ms = UINT64_MAX;
static uint32_t buffered_bytes;
static uint32_t discarded_bytes;
static uint32_t write_limit = UINT32_MAX;
static unsigned close_calls;
static bool stream_open;
static bool release_started;
static bool partial_write_seen;

void basic_sound_close(void)
{
}

void basic_runtime_write(const char* text)
{
    (void) text;
}

uint64_t tabos_monotonic_ms(void)
{
    return now_ms;
}

tabos_audio_stream_t tabos_audio_open(const tabos_audio_config_t* config)
{
    assert(config != NULL);
    assert(config->direction == TABOS_AUDIO_PLAYBACK);
    assert(config->channels == 1U);
    assert(config->sample_rate == TABOS_AUDIO_SAMPLE_RATE_44100);
    assert(!stream_open);
    stream_open = true;
    return TEST_STREAM;
}

int tabos_audio_close(tabos_audio_stream_t stream)
{
    assert(stream == TEST_STREAM);
    assert(stream_open);
    discarded_bytes = buffered_bytes;
    buffered_bytes  = 0U;
    stream_open     = false;
    ++close_calls;
    return 0;
}

int tabos_audio_write(tabos_audio_stream_t stream, const void* pcm, uint32_t bytes)
{
    assert(stream == TEST_STREAM);
    assert(stream_open);
    assert(pcm != NULL);
    assert(bytes != 0U && bytes % TEST_FRAME_BYTES == 0U);
    uint32_t accepted = TEST_BUFFER_CAPACITY - buffered_bytes;
    if (accepted > write_limit) {
        accepted = write_limit;
    }
    if (accepted > bytes) {
        accepted = bytes;
    }
    accepted -= accepted % TEST_FRAME_BYTES;
    assert(accepted != 0U);
    if (accepted < bytes) {
        partial_write_seen = true;
    }
    buffered_bytes += accepted;
    return (int) accepted;
}

int tabos_audio_set_volume(tabos_audio_stream_t stream, uint32_t volume)
{
    assert(stream == TEST_STREAM);
    assert(volume == 700U);
    return 0;
}

int tabos_audio_get_status(tabos_audio_stream_t stream, tabos_audio_status_t* status)
{
    assert(stream == TEST_STREAM);
    assert(stream_open);
    assert(status != NULL);
    *status = (tabos_audio_status_t) {
        .buffered_bytes  = buffered_bytes,
        .buffer_capacity = TEST_BUFFER_CAPACITY,
    };
    if (release_started && buffered_bytes == 0U && empty_status_ms == UINT64_MAX) {
        empty_status_ms = now_ms;
    }
    return 0;
}

static void advance_playback(uint32_t milliseconds)
{
    const uint32_t consumed = milliseconds * TABOS_AUDIO_SAMPLE_RATE_44100 * TEST_FRAME_BYTES / 1000U;
    if (consumed >= buffered_bytes) {
        buffered_bytes = 0U;
    } else {
        buffered_bytes -= consumed;
        buffered_bytes -= buffered_bytes % TEST_FRAME_BYTES;
    }
    now_ms += milliseconds;
}

static void verify_normal_completion_drains(void)
{
    basic_sid_reset();
    assert(basic_sid_write(0xd418U, 15U));
    assert(basic_sid_write(0xd400U, 214U));
    assert(basic_sid_write(0xd401U, 28U));
    assert(basic_sid_write(0xd405U, 0U));
    assert(basic_sid_write(0xd406U, 240U));
    assert(basic_sid_write(0xd404U, 17U));

    while (buffered_bytes < 4096U) {
        basic_sid_service();
    }
    assert(basic_sid_active());
    assert(basic_sid_write(0xd404U, 16U));
    release_started = true;
    write_limit     = 64U;

    unsigned passes = 0U;
    while (basic_sid_active() && passes < 1000U) {
        basic_sid_service();
        advance_playback(5U);
        ++passes;
    }

    assert(passes < 1000U);
    assert(partial_write_seen);
    assert(close_calls == 1U);
    assert(discarded_bytes == 0U);
    assert(empty_status_ms != UINT64_MAX);
    assert(now_ms >= empty_status_ms + 25U);
}

int main(void)
{
    assert(basic_sid_frequency_millihz(0U) == 0U);
    assert(basic_sid_frequency_millihz(1U) == 58U);
    assert(basic_sid_frequency_millihz(4455U) == 261621U);
    assert(basic_sid_frequency_millihz(7382U) == 433510U);
    assert(basic_sid_frequency_millihz(65535U) == 3848566U);
    verify_normal_completion_drains();
    return 0;
}
