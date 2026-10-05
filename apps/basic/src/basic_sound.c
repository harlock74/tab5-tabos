#include <basic/graphics.h>
#include <basic/runtime.h>
#include <basic/sid.h>
#include <errno.h>
#include <stdint.h>
#include <tabos/audio.h>
#include <tabos/runtime_time.h>

static tabos_audio_stream_t stream = TABOS_AUDIO_STREAM_INVALID;
static int16_t samples[256];

void basic_sound_close(void)
{
    if (stream != TABOS_AUDIO_STREAM_INVALID) {
        (void) tabos_audio_close(stream);
        stream = TABOS_AUDIO_STREAM_INVALID;
    }
}

unsigned basic_sound_play(int32_t frequency, int32_t milliseconds)
{
    if (frequency < 20 || frequency > 20000 || milliseconds < 1 || milliseconds > 2000) {
        return 14U;
    }
    basic_sid_close();
    const tabos_audio_config_t config = {.direction   = TABOS_AUDIO_PLAYBACK,
                                         .channels    = 1U,
                                         .route       = TABOS_AUDIO_ROUTE_SPEAKER,
                                         .sample_rate = TABOS_AUDIO_SAMPLE_RATE_44100};
    stream                            = tabos_audio_open(&config);
    if (stream == TABOS_AUDIO_STREAM_INVALID) {
        basic_runtime_write("\n?TABOS AUDIO UNAVAILABLE\n");
        return 14U;
    }
    (void) tabos_audio_set_volume(stream, 200U);
    uint32_t remaining      = (uint32_t) milliseconds * 44100U / 1000U;
    uint32_t phase          = 0U;
    const uint64_t deadline = tabos_monotonic_ms() + (uint32_t) milliseconds + 500U;
    unsigned error          = 0U;
    while (remaining != 0U) {
        const uint32_t count = remaining < 256U ? remaining : 256U;
        for (uint32_t i = 0U; i < count; ++i) {
            samples[i] = phase < 22050U ? 5000 : -5000;
            phase      = (phase + (uint32_t) frequency) % 44100U;
        }
        uint32_t sent = 0U;
        while (sent < count * sizeof(samples[0])) {
            if (!basic_runtime_sleep(0U)) {
                goto done;
            }
            const int written =
                tabos_audio_write(stream, (const unsigned char*) samples + sent, count * sizeof(samples[0]) - sent);
            if (written > 0) {
                sent += (uint32_t) written;
            } else if (written < 0 && errno != EAGAIN && errno != EINTR) {
                basic_runtime_write("\n?TABOS AUDIO WRITE FAILED\n");
                error = 14U;
                goto done;
            } else if (!basic_runtime_sleep(5U)) {
                goto done;
            }
            if (tabos_monotonic_ms() >= deadline) {
                basic_runtime_write("\n?TABOS AUDIO QUEUE TIMED OUT\n");
                error = 14U;
                goto done;
            }
        }
        remaining -= count;
    }
    for (;;) {
        tabos_audio_status_t status;
        if (tabos_audio_get_status(stream, &status) != 0) {
            basic_runtime_write("\n?TABOS AUDIO STATUS FAILED\n");
            error = 14U;
            break;
        }
        if (status.buffered_bytes == 0U) {
            // Public status covers the service ring, not downstream device/DMA
            // queues. Keep ownership briefly so a short tone is not stopped as
            // soon as its last samples are handed to the platform. This is a
            // bounded settling interval, not a claim of hardware drain feedback.
            (void) basic_runtime_sleep(100U);
            break;
        }
        if (!basic_runtime_sleep(5U)) {
            break;
        }
        if (tabos_monotonic_ms() >= deadline) {
            basic_runtime_write("\n?TABOS AUDIO DRAIN TIMED OUT\n");
            error = 14U;
            break;
        }
    }
done:
    basic_sound_close();
    return error;
}
