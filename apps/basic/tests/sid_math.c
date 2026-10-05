#include <assert.h>
#include <basic/sid.h>
#include <stdint.h>
#include <tabos/audio.h>

// Link-only boundaries for the pure conversion check. PCM behavior is exercised
// separately through the production public SDK implementation.
void basic_sound_close(void)
{
}
void basic_runtime_write(const char* text)
{
    (void) text;
}
uint64_t tabos_monotonic_ms(void)
{
    return 0U;
}
tabos_audio_stream_t tabos_audio_open(const tabos_audio_config_t* config)
{
    (void) config;
    return TABOS_AUDIO_STREAM_INVALID;
}
int tabos_audio_close(tabos_audio_stream_t stream)
{
    (void) stream;
    return 0;
}
int tabos_audio_write(tabos_audio_stream_t stream, const void* pcm, uint32_t bytes)
{
    (void) stream;
    (void) pcm;
    (void) bytes;
    return -1;
}
int tabos_audio_set_volume(tabos_audio_stream_t stream, uint32_t volume)
{
    (void) stream;
    (void) volume;
    return 0;
}
int tabos_audio_get_status(tabos_audio_stream_t stream, tabos_audio_status_t* status)
{
    (void) stream;
    (void) status;
    return -1;
}

int main(void)
{
    assert(basic_sid_frequency_millihz(0U) == 0U);
    assert(basic_sid_frequency_millihz(1U) == 58U);
    assert(basic_sid_frequency_millihz(4455U) == 261621U);
    assert(basic_sid_frequency_millihz(7382U) == 433510U);
    assert(basic_sid_frequency_millihz(65535U) == 3848566U);
    return 0;
}
