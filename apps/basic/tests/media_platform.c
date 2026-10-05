// Test-only transport beneath the production public SDK implementation.
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <tabos/graphics.h>
#include <tabos/internal/elf_api.h>
#include <tabos/runtime_time.h>

static bool graphics_owned;
static bool audio_owned;
static uint32_t audio_buffered;
static uint64_t audio_last_ms;

static void drain_audio(void)
{
    const uint64_t now     = tabos_monotonic_ms();
    const uint64_t elapsed = now - audio_last_ms;
    const uint64_t drained = elapsed * 44100U * sizeof(int16_t) / 1000U;
    audio_buffered         = drained >= audio_buffered ? 0U : audio_buffered - (uint32_t) drained;
    audio_last_ms          = now;
}
static void trace(const char* operation, unsigned value)
{
    const char* path = getenv("TABOS_BASIC_MEDIA_TRACE");
    if (path != NULL) {
        FILE* output = fopen(path, "a");
        assert(output != NULL);
        fprintf(output, "%s %u\n", operation, value);
        fclose(output);
    }
}
static int graphics_open(uint32_t* width, uint32_t* height)
{
    assert(!graphics_owned);
    if (getenv("TABOS_BASIC_GRAPHICS_UNAVAILABLE") != NULL) {
        return -ENODEV;
    }
    graphics_owned = true;
    *width         = 1280U;
    *height        = 720U;
    trace("OPEN", 0U);
    return 0;
}
static int graphics_close(void)
{
    assert(graphics_owned);
    graphics_owned = false;
    trace("CLOSE", 0U);
    return 0;
}
static int graphics_clear(uint32_t color)
{
    assert(graphics_owned);
    (void) color;
    return 0;
}
static int graphics_present(void)
{
    assert(graphics_owned);
    return 0;
}
static int graphics_blit_ex(const tabos_graphics_blit_options_t* options)
{
    assert(graphics_owned);
    assert(options->bitmap_width == 320U && options->bitmap_height == 200U);
    assert(options->destination.width == 960U && options->destination.height == 600U);
    uint32_t hash = 2166136261U;
    for (unsigned i = 0U; i < options->bitmap_width * options->bitmap_height; ++i) {
        hash = (hash ^ (options->pixels[i] & 255U)) * 16777619U;
        hash = (hash ^ (options->pixels[i] >> 8U)) * 16777619U;
    }
    trace("FRAME", hash);
    const char* path = getenv("TABOS_BASIC_MEDIA_PIXELS");
    if (path != NULL) {
        FILE* output = fopen(path, "wb");
        assert(output != NULL);
        assert(fwrite(options->pixels, 2U, options->bitmap_width * options->bitmap_height, output) ==
               options->bitmap_width * options->bitmap_height);
        fclose(output);
    }
    return 0;
}
static int audio_open(const tabos_audio_config_t* config)
{
    assert(!audio_owned && config->channels == 1U && config->sample_rate == 44100U);
    if (getenv("TABOS_BASIC_AUDIO_UNAVAILABLE") != NULL) {
        return -ENODEV;
    }
    audio_owned    = true;
    audio_buffered = 0U;
    audio_last_ms  = tabos_monotonic_ms();
    trace("AUDIO_OPEN", 0U);
    return 1;
}
static int audio_close(int stream)
{
    assert(audio_owned && stream == 1);
    audio_owned    = false;
    audio_buffered = 0U;
    trace("AUDIO_CLOSE", 0U);
    return 0;
}
static int audio_write(int stream, const void* pcm, uint32_t bytes)
{
    assert(audio_owned && stream == 1 && pcm != NULL && bytes <= 512U && bytes % 2U == 0U);
    drain_audio();
    const uint32_t accepted = bytes < 32768U - audio_buffered ? bytes : 32768U - audio_buffered;
    const int16_t* samples  = pcm;
    uint32_t hash           = 2166136261U;
    uint32_t peak           = 0U;
    for (unsigned index = 0U; index < accepted / sizeof(*samples); ++index) {
        const uint16_t value        = (uint16_t) samples[index];
        const int32_t signed_sample = samples[index];
        const uint32_t amplitude    = (uint32_t) (signed_sample < 0 ? -signed_sample : signed_sample);
        if (amplitude > peak) {
            peak = amplitude;
        }
        hash = (hash ^ (value & 255U)) * 16777619U;
        hash = (hash ^ (value >> 8U)) * 16777619U;
    }
    audio_buffered += accepted;
    trace("PCM", accepted);
    trace("PCM_HASH", hash);
    trace("PCM_PEAK", peak);
    return (int) accepted;
}
static int audio_status(int stream, tabos_audio_status_t* status)
{
    assert(audio_owned && stream == 1);
    drain_audio();
    *status = (tabos_audio_status_t) {.buffered_bytes = audio_buffered, .buffer_capacity = 32768U};
    return 0;
}
static int audio_volume(int stream, uint32_t volume)
{
    assert(audio_owned && stream == 1 && (volume == 200U || volume == 700U));
    return 0;
}
static const tabos_elf_api_t api         = {.abi_version      = TABOS_ELF_API_VERSION,
                                            .graphics_open    = graphics_open,
                                            .graphics_close   = graphics_close,
                                            .graphics_clear   = graphics_clear,
                                            .graphics_present = graphics_present,
                                            .graphics_blit_ex = graphics_blit_ex,
                                            .audio_open       = audio_open,
                                            .audio_close      = audio_close,
                                            .audio_write      = audio_write,
                                            .audio_status     = audio_status,
                                            .audio_set_volume = audio_volume};
const tabos_elf_api_t* tabos_runtime_api = &api;
