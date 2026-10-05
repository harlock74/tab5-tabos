# TabBASIC selected SID compatibility

Stage J adds a bounded virtual register profile for simple Commodore-style BASIC
sound listings. It is a compatibility synthesizer, not MOS 6581/8580 emulation.
There is no analogue chip model, filter fidelity, cycle timing, CIA timer, raster
interrupt, machine-code execution or native-memory mapping.

## Try the supplied examples

The self-authored tokenized examples are installed under `T:/data/basic/`; the
[example catalogue](basic-examples.md) is the authoritative list:

```basic
LOAD "SIDTONE.prg"
RUN
```

`SIDSPRITE` combines an ongoing tone with virtual
C64 sprite movement and runs until Ctrl+C. Their sound is controlled entirely by
SID-style POKEs; none uses TabBASIC's native `SOUND` statement.
`C64DODGE` is the self-authored Stage K playable sample combining one SID voice,
screen RAM and a virtual sprite. Bare `LOAD "C64DODGE.prg"` uses the packaged copy
unless a personal program with the same name exists.

## Virtual registers

Only `$D400-$D418` is accepted. These 25 bytes are fixed application-owned state;
they never alias the interpreter array, native pointers or executable addresses.

| Offset per voice | Meaning |
| ---: | --- |
| 0/1 | 16-bit frequency low/high |
| 2/3 | 12-bit pulse width low/high; the high byte is masked to four bits |
| 4 | Gate and triangle/sawtooth/pulse/noise control bits |
| 5 | Attack/decay nibbles |
| 6 | Sustain/release nibbles |

The voice bases are `$D400`, `$D407` and `$D40E`. `$D415-$D417` are retained for
safe PEEK/POKE round trips but filters are not synthesized. `$D418` is retained;
its low nibble controls master volume. PEEK returns the stored compatibility byte.
Oscillator, envelope and paddle readbacks are not implemented.

Control bit 0 is gate, bit 3 is test/reset, and bits 4–7 select triangle,
sawtooth, pulse and noise. When a listing sets several waveform bits, the bounded
compatibility priority is noise, pulse, sawtooth, then triangle. Sync and ring
modulation bits are stored but ignored. This deterministic rule is not authentic
combined-waveform behavior.

## Frequency, waveform and envelope model

Stage J uses one fixed PAL-style compatibility clock of **985,248 Hz**. There is
no PAL/NTSC selector. A 16-bit register value `F` is converted as:

```text
frequency_hz = F * 985248 / 16777216
```

The implementation uses bounded integer phase accumulators at 44,100 samples per
second. Triangle, sawtooth, pulse and a deterministic 23-bit noise generator are
available for three voices. The mixed mono output is deliberately limited before
it reaches the public TabOS stream.

Attack, decay, sustain and release are linear integer ramps using the conventional
16 nominal SID time choices. They do not reproduce the SID's exponential envelope
counter, delay bug or analogue variation. Gate-on enters attack; gate-off enters
release; a voice becomes inactive when release reaches zero. Test resets phase and
silences that voice while set.

## Cooperative audio and lifecycle

The synthesizer writes signed 16-bit mono PCM only through `<tabos/audio.h>`. It
uses a fixed 256-sample staging buffer, services at most one bounded write per
call, and targets 4,096 queued bytes (about 46 ms) while checking public queue
status. A service schedules the next bounded fill immediately until it reaches
that target, then returns to its 5 ms cadence. Frequency and envelope increments
are calculated once per staging chunk instead of once per sample. BASIC's
normal interpreter checkpoints service audio. While BASIC waits for a command,
the normalized input wait uses a 5 ms timeout only while SID work is active, so
sound continues and input remains responsive without a busy wait.

Register POKEs configure state and do not synchronously play or drain a tone.
Native `SOUND` remains its separate synchronous generator; either path closes the
other path's stream before taking audio ownership. `TEXT`, Ctrl+C, Ctrl+Q, process
exit and a completed release close the SID stream and silence voices. Virtual
registers reset on a new BASIC process and are not saved as device state. BASIC
programs containing the POKEs still LIST/SAVE/LOAD normally.

The fixed SID state adds 632 bytes of application BSS, including the 512-byte PCM
staging buffer. The requested 256 KiB heap and 32 KiB stack are unchanged. The
audio service owns its existing stream queue outside the application image.

If audio cannot be opened, TabBASIC prints `?TABOS SID AUDIO UNAVAILABLE` once for
that session state and preserves the virtual bytes safely. Unsupported addresses,
including `$D419`, retain the ordinary bounded C64-address error. SYS, USR, WAIT,
CIA, raster state and arbitrary memory remain disabled.

## Compatibility classification

Selected register-driven tones are **Tier 3A / PARTIAL**. Core register storage,
four waveforms, three voices, gate, approximate ADSR and volume work within the
model above. Filter-dependent, sync/ring, readback, exact noise, cycle-timed and
CIA-timed listings are unsupported. Audible similarity is not evidence of 6581 or
8580 fidelity.

## Physical Stage J result

On the Tab5, the single-tone and scale examples were audible and completed cleanly;
the output was quiet and the basic waveforms sounded buzzy. The envelope change was
subtle. The corrected combined SID/sprite example produced a continuous tone with
mostly smooth movement and prompt Ctrl+C cleanup. The three-voice example improved
after queue scheduling was corrected, but still had a couple of perceived audio
interruptions. These observations establish functional compatibility within this
profile and do not establish analogue SID fidelity.

## Historical sample

The first sound listing in the Commodore *C64 Programmer's Reference Guide* uses
only `$D400-$D418`, BASIC loops and DATA; it has no SYS/USR, CIA or raster
dependency. It was entered transiently into the production interpreter and ran to
completion with nonzero PCM and clean stream closure. It was not imported because
the online transcription does not establish a redistribution license. See the
[primary guide transcription](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_4/page_185.html).
