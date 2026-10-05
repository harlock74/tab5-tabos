#include <basic/graphics.h>
#include <basic/runtime.h>
#include <basic/sid.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <tabos/audio.h>
#include <tabos/runtime_time.h>

enum {
    SID_START            = 0xd400,
    SID_END              = 0xd418,
    SID_REGISTER_COUNT   = SID_END - SID_START + 1,
    SID_VOICES           = 3,
    SID_VOICE_REGISTERS  = 7,
    SID_SAMPLE_RATE      = TABOS_AUDIO_SAMPLE_RATE_44100,
    SID_CHUNK_SAMPLES    = 256,
    SID_QUEUE_TARGET     = 4096,
    SID_SERVICE_MS       = 5,
    SID_CLOCK_HZ         = 985248,
    SID_MASTER_REGISTER  = 0x18,
    SID_CONTROL_GATE     = 0x01,
    SID_CONTROL_TEST     = 0x08,
    SID_CONTROL_TRIANGLE = 0x10,
    SID_CONTROL_SAW      = 0x20,
    SID_CONTROL_PULSE    = 0x40,
    SID_CONTROL_NOISE    = 0x80,
    SID_OUTPUT_LEVEL     = 6000
};

typedef enum {
    ENVELOPE_OFF,
    ENVELOPE_ATTACK,
    ENVELOPE_DECAY,
    ENVELOPE_SUSTAIN,
    ENVELOPE_RELEASE
} envelope_stage_t;

typedef struct {
        uint32_t phase;
        uint32_t noise;
        uint32_t envelope;
        envelope_stage_t stage;
        bool gate;
} sid_voice_t;

typedef struct {
        uint8_t registers[SID_REGISTER_COUNT];
        sid_voice_t voices[SID_VOICES];
        tabos_audio_stream_t stream;
        int16_t samples[SID_CHUNK_SAMPLES];
        uint32_t pending_bytes;
        uint32_t pending_offset;
        uint64_t next_service;
        bool stream_open;
        bool audio_failed;
} sid_state_t;

static sid_state_t state;

// Linear compatibility timings in milliseconds. These use familiar SID ADSR
// nominal values but do not model the chip's exponential counter behavior.
static const uint16_t attack_ms[16]        = {2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000};
static const uint16_t decay_release_ms[16] = {6,   24,  48,   72,   114,  168,  204,   240,
                                              300, 750, 1500, 2400, 3000, 9000, 15000, 24000};

uint32_t basic_sid_frequency_millihz(uint16_t value)
{
    return (uint32_t) (((uint64_t) value * SID_CLOCK_HZ * 1000U) >> 24U);
}

static uint32_t phase_increment(uint16_t value)
{
    return (uint32_t) ((uint64_t) value * SID_CLOCK_HZ * 256U / SID_SAMPLE_RATE);
}

static uint32_t envelope_step(uint16_t milliseconds)
{
    const uint32_t samples = (uint32_t) milliseconds * SID_SAMPLE_RATE / 1000U;
    if (samples == 0U) {
        return UINT16_MAX;
    }
    return (UINT16_MAX + samples - 1U) / samples;
}

static bool voice_running(const sid_voice_t* voice)
{
    return voice->stage != ENVELOPE_OFF;
}

static bool any_voice_running(void)
{
    for (unsigned voice = 0U; voice < SID_VOICES; ++voice) {
        if (voice_running(&state.voices[voice])) {
            return true;
        }
    }
    return false;
}

bool basic_sid_active(void)
{
    return state.stream_open || (!state.audio_failed && any_voice_running());
}

static void close_stream(void)
{
    if (state.stream_open) {
        (void) tabos_audio_close(state.stream);
        state.stream_open = false;
    }
    state.pending_bytes  = 0U;
    state.pending_offset = 0U;
}

void basic_sid_close(void)
{
    close_stream();
    for (unsigned voice = 0U; voice < SID_VOICES; ++voice) {
        state.voices[voice].stage                          = ENVELOPE_OFF;
        state.voices[voice].gate                           = false;
        state.voices[voice].envelope                       = 0U;
        state.registers[voice * SID_VOICE_REGISTERS + 4U] &= (uint8_t) ~SID_CONTROL_GATE;
    }
}

void basic_sid_reset(void)
{
    basic_sid_close();
    memset(&state, 0, sizeof(state));
    for (unsigned voice = 0U; voice < SID_VOICES; ++voice) {
        state.voices[voice].noise = 0x7ffff8U ^ (voice * 0x15555U);
    }
}

static void update_gate(unsigned voice_index, uint8_t control)
{
    sid_voice_t* voice = &state.voices[voice_index];
    const bool gate    = (control & SID_CONTROL_GATE) != 0U;
    if (gate && !voice->gate) {
        voice->stage = ENVELOPE_ATTACK;
    } else if (!gate && voice->gate && voice->stage != ENVELOPE_OFF) {
        voice->stage = ENVELOPE_RELEASE;
    }
    voice->gate = gate;
    if ((control & SID_CONTROL_TEST) != 0U) {
        voice->phase = 0U;
    }
}

bool basic_sid_read(uint16_t address, uint8_t* value)
{
    if (address < SID_START || address > SID_END || value == NULL) {
        return false;
    }
    *value = state.registers[address - SID_START];
    return true;
}

bool basic_sid_write(uint16_t address, uint8_t value)
{
    if (address < SID_START || address > SID_END) {
        return false;
    }
    const unsigned index = address - SID_START;
    if ((index % SID_VOICE_REGISTERS) == 3U && index < SID_VOICES * SID_VOICE_REGISTERS) {
        value &= 0x0fU;
    }
    state.registers[index] = value;
    if (index < SID_VOICES * SID_VOICE_REGISTERS && (index % SID_VOICE_REGISTERS) == 4U) {
        update_gate(index / SID_VOICE_REGISTERS, value);
    }
    state.next_service = 0U;
    return true;
}

static void update_envelope(unsigned voice_index, uint32_t attack_step, uint32_t decay_step, uint32_t sustain_target,
                            uint32_t release_step)
{
    sid_voice_t* voice = &state.voices[voice_index];
    if (voice->stage == ENVELOPE_ATTACK) {
        if (UINT16_MAX - voice->envelope <= attack_step) {
            voice->envelope = UINT16_MAX;
            voice->stage    = ENVELOPE_DECAY;
        } else {
            voice->envelope += attack_step;
        }
    } else if (voice->stage == ENVELOPE_DECAY) {
        if (voice->envelope <= sustain_target + decay_step) {
            voice->envelope = sustain_target;
            voice->stage    = ENVELOPE_SUSTAIN;
        } else {
            voice->envelope -= decay_step;
        }
    } else if (voice->stage == ENVELOPE_SUSTAIN) {
        voice->envelope = sustain_target;
    } else if (voice->stage == ENVELOPE_RELEASE) {
        if (voice->envelope <= release_step) {
            voice->envelope = 0U;
            voice->stage    = ENVELOPE_OFF;
        } else {
            voice->envelope -= release_step;
        }
    }
}

static int32_t waveform(unsigned voice_index, uint32_t increment)
{
    sid_voice_t* voice       = &state.voices[voice_index];
    const unsigned base      = voice_index * SID_VOICE_REGISTERS;
    const uint8_t control    = state.registers[base + 4U];
    const uint32_t previous  = voice->phase;
    voice->phase            += increment;
    if ((control & SID_CONTROL_TEST) != 0U) {
        voice->phase = 0U;
        return 0;
    }
    int32_t sample = 0;
    if ((control & SID_CONTROL_NOISE) != 0U) {
        if (voice->phase < previous) {
            const uint32_t feedback = ((voice->noise >> 22U) ^ (voice->noise >> 17U)) & 1U;
            voice->noise            = ((voice->noise << 1U) | feedback) & 0x7fffffU;
            if (voice->noise == 0U) {
                voice->noise = 0x7ffff8U;
            }
        }
        sample = (voice->noise & 0x400000U) != 0U ? INT16_MAX : INT16_MIN;
    } else if ((control & SID_CONTROL_PULSE) != 0U) {
        const uint16_t width =
            (uint16_t) ((uint16_t) state.registers[base + 2U] | (uint16_t) (state.registers[base + 3U] & 15U) << 8U);
        sample = (voice->phase >> 20U) < width ? INT16_MAX : INT16_MIN;
    } else if ((control & SID_CONTROL_SAW) != 0U) {
        sample = (int32_t) (voice->phase >> 16U) - 32768;
    } else if ((control & SID_CONTROL_TRIANGLE) != 0U) {
        const uint32_t position = voice->phase >> 16U;
        if (position < 32768U) {
            sample = (int32_t) position * 2 - 32768;
        } else {
            sample = (int32_t) (65535U - position) * 2 - 32768;
        }
    }
    return sample * (int32_t) voice->envelope / UINT16_MAX;
}

static void generate_chunk(void)
{
    const uint32_t volume = state.registers[SID_MASTER_REGISTER] & 15U;
    uint32_t increments[SID_VOICES];
    uint32_t attack_steps[SID_VOICES];
    uint32_t decay_steps[SID_VOICES];
    uint32_t sustain_targets[SID_VOICES];
    uint32_t release_steps[SID_VOICES];
    for (unsigned voice = 0U; voice < SID_VOICES; ++voice) {
        const unsigned base = voice * SID_VOICE_REGISTERS;
        const uint16_t frequency =
            (uint16_t) ((uint16_t) state.registers[base] | (uint16_t) state.registers[base + 1U] << 8U);
        const uint8_t attack   = state.registers[base + 5U] >> 4U;
        const uint8_t decay    = state.registers[base + 5U] & 15U;
        const uint8_t sustain  = state.registers[base + 6U] >> 4U;
        const uint8_t release  = state.registers[base + 6U] & 15U;
        increments[voice]      = phase_increment(frequency);
        attack_steps[voice]    = envelope_step(attack_ms[attack]);
        decay_steps[voice]     = envelope_step(decay_release_ms[decay]);
        sustain_targets[voice] = (uint32_t) sustain * UINT16_MAX / 15U;
        release_steps[voice]   = envelope_step(decay_release_ms[release]);
    }
    for (unsigned index = 0U; index < SID_CHUNK_SAMPLES; ++index) {
        int32_t mixed = 0;
        for (unsigned voice = 0U; voice < SID_VOICES; ++voice) {
            update_envelope(voice, attack_steps[voice], decay_steps[voice], sustain_targets[voice],
                            release_steps[voice]);
            mixed += waveform(voice, increments[voice]);
        }
        mixed                = mixed * SID_OUTPUT_LEVEL / (INT16_MAX * SID_VOICES);
        mixed                = mixed * (int32_t) volume / 15;
        state.samples[index] = (int16_t) mixed;
    }
    state.pending_bytes  = sizeof(state.samples);
    state.pending_offset = 0U;
}

static bool open_stream(void)
{
    if (state.stream_open) {
        return true;
    }
    if (state.audio_failed) {
        return false;
    }
    basic_sound_close();
    const tabos_audio_config_t config = {.direction   = TABOS_AUDIO_PLAYBACK,
                                         .channels    = 1U,
                                         .route       = TABOS_AUDIO_ROUTE_SPEAKER,
                                         .sample_rate = SID_SAMPLE_RATE};
    state.stream                      = tabos_audio_open(&config);
    if (state.stream == TABOS_AUDIO_STREAM_INVALID) {
        basic_runtime_write("\n?TABOS SID AUDIO UNAVAILABLE\n");
        state.audio_failed = true;
        return false;
    }
    state.stream_open = true;
    (void) tabos_audio_set_volume(state.stream, 700U);
    state.audio_failed = false;
    return true;
}

void basic_sid_service(void)
{
    if (!any_voice_running()) {
        close_stream();
        return;
    }
    const uint64_t now = tabos_monotonic_ms();
    if (state.pending_bytes == 0U && now < state.next_service) {
        return;
    }
    const bool audible = (state.registers[SID_MASTER_REGISTER] & 15U) != 0U;
    if (!audible && state.stream_open) {
        close_stream();
    }
    if (audible && !open_stream()) {
        state.next_service = now + SID_SERVICE_MS;
        return;
    }
    uint32_t buffered_bytes = 0U;
    if (state.stream_open) {
        tabos_audio_status_t status;
        if (tabos_audio_get_status(state.stream, &status) != 0) {
            close_stream();
            state.audio_failed = true;
            return;
        }
        if (state.pending_bytes == 0U && status.buffered_bytes >= SID_QUEUE_TARGET) {
            state.next_service = now + SID_SERVICE_MS;
            return;
        }
        buffered_bytes = status.buffered_bytes;
    }
    if (state.pending_bytes == 0U) {
        generate_chunk();
    }
    if (!audible) {
        state.pending_bytes  = 0U;
        state.pending_offset = 0U;
    } else {
        const int written =
            tabos_audio_write(state.stream, (const uint8_t*) state.samples + state.pending_offset, state.pending_bytes);
        if (written > 0) {
            state.pending_offset += (uint32_t) written;
            state.pending_bytes  -= (uint32_t) written;
            buffered_bytes       += (uint32_t) written;
        } else if (written < 0 && errno != EAGAIN && errno != EINTR) {
            close_stream();
            state.audio_failed = true;
        }
    }
    state.next_service = state.pending_bytes == 0U && buffered_bytes < SID_QUEUE_TARGET ? now : now + SID_SERVICE_MS;
}
