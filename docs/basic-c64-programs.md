# TabBASIC C64 program compatibility corpus

Stage K asks which useful Commodore-era BASIC programs fit TabBASIC's existing
bounded profile. This is a curated dependency study, not a compatibility
percentage. A keyword or successful LOAD is not counted as a pass: the program's
actual language, display, input, sound and machine dependencies must work.

## Result in practical terms

Pure text programs written for Microsoft-style BASIC are the strongest unchanged
case. Programs that use only the documented screen RAM, colour RAM, basic sprites
and selected SID registers can also work. A small source edit from joystick or
keyboard-matrix polling to `KEY()` is acceptable when recorded.

Programs that depend on SYS/USR machine code, raster interrupts, VIC fine scrolling,
CIA timers or keyboard/joystick matrices remain outside the profile. Stage K does
not turn those dependencies into emulator requirements.

## Method and provenance

Every candidate was audited before execution for PEEK/POKE, SYS, USR, WAIT,
OPEN/CLOSE/CMD, VIC/SID/CIA/raster addresses, keyboard or joystick polling, custom
characters, bank switching and machine-code DATA. Repository samples are included
only when their redistribution terms are clear.

`AMAZING` is the one redistributed historical listing. It is Jack Hauber's maze
generator from David H. Ahl's *BASIC Computer Games*, taken unchanged from the
[Unlicense basic-computer-games repository](https://github.com/coding-horror/basic-computer-games/tree/5301155192d91d74d337899cecc59dbda59c4c17/02_Amazing).
Its exact provenance and license are retained beside the source in
`apps/basic/examples/THIRD_PARTY.md` and `UNLICENSE`.

Two historical listings with unclear redistribution terms were entered or
inspected locally and are represented only by metadata: the famous maze one-liner
described by [TIME](https://time.com/69316/basic/) and the first sound example in
the [Commodore 64 Programmer's Reference Guide transcription](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_4/page_185.html).
No source from either is committed.

The dependency-only negative case is the MIT-licensed
[C64 horizontal shoot-'em-up](https://github.com/jaredevans/c64-shmup/tree/63554e26e83fb209a41ccf1def1ede4c5f0c4a2e).
It is useful evidence because its own documentation identifies a BASIC SYS stub,
6502 assembly, raster IRQ chains, fine scrolling, a custom character set and a
sprite multiplexer. It was not executed or imported: those dependencies make the
classification unambiguous and deliberately outside TabBASIC.

All other matrix entries are explicitly project-authored compatibility samples,
not historical programs.

## Compatibility matrix

| Program | Provenance | Category | Machine code | VIC usage | SID usage | Input dependency | Changes | Result |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| AMAZING | Jack Hauber; *BASIC Computer Games*; redistributed under Unlicense | Text/maze generator | None | None | None | Numeric INPUT | None; exact source retained | **UNCHANGED PASS** |
| Classic maze one-liner | Historical metadata only; TIME article | Text/character demo | None | PETSCII 205/206 output | None | None | None | **PARTIAL** — loop runs and breaks, but ASCII/CP437 output lacks the two diagonal PETSCII glyphs |
| Programmer's Guide sound example 1 | Historical metadata only; guide transcription | Sound | None | None | `$D400-$D418` subset | None | None | **UNCHANGED PASS** in native production adapter; nonzero PCM and clean release |
| C64 horizontal shoot-'em-up | jaredevans, MIT; dependency audit only | Combined commercial-style game | Required; BASIC `SYS 2064` starts 6502 assembly | Raster IRQ, fine scroll, custom charset, sprite multiplexer, bank layout | Frame-timed three-voice driver | Keyboard handled by machine code | None | **UNSUPPORTED — OUTSIDE PROFILE** |
| C64SCREEN | Project-authored Stage I demonstration | Screen/colour | None | Screen RAM, colour RAM, border/background | None | None | None | **UNCHANGED PASS** |
| C64SPRITE | Project-authored Stage I demonstration | Sprite | None | Sprite bitmap/pointer/position/enable/colour | None | None | None | **UNCHANGED PASS** |
| SIDTONE | Project-authored Stage J demonstration | Sound | None | None | Frequency, ADSR, gate, saw, volume | None | None | **UNCHANGED PASS** within Tier 3A |
| SID3VOICE | Project-authored Stage J demonstration | Sound | None | None | Three supported voices | None | None | **PARTIAL** physically — functional, quiet/buzzy, occasional perceived interruption |
| C64DODGE | Project-authored Stage K C64-style game | Combined playable game | None | Screen/colour, one sprite | One triangle voice and gate | TabBASIC `KEY(LEFT/RIGHT/B)` | Authored with explicit native-key adaptation; no historical original | **UNCHANGED PASS (SELF-AUTHORED DEMONSTRATION)** |
| PONG | Project-authored Stage H native game | Native comparison, not C64 corpus | None | None; native graphics API | Native SOUND is not used | `KEY()` | None | **PASS WITH LIMITATIONS** — playable but slow |

The corpus contains ten evaluated entries: three historical programs/listings, one
modern real C64 machine-code game used as a negative dependency case, five
self-authored compatibility samples, and one native comparison. It is deliberately
small and selected; no percentage of C64 software compatibility follows from it.

## Playable compatibility target

`C64DODGE` is the playable Stage K target. It is self-authored and intentionally
uses only historical-style screen, colour, sprite and SID POKEs already supported
by TabBASIC. Left/Right moves the yellow sprite, falling red characters are hazards,
B exits, and Ctrl+C remains available. Its keyboard line uses `KEY()` because
TabBASIC does not emulate a C64 keyboard or joystick matrix.

```basic
LOAD "C64DODGE.prg"
RUN
```

This demonstrates the useful boundary without presenting the game as a recovered
historical title. New games should normally use the native API described in
[game development](basic-game-development.md).

## PETSCII finding

The two high-byte values in the classic maze one-liner have different meanings in
PETSCII and CP437. Mapping them globally to ASCII slash characters would silently
change the documented byte-oriented terminal behavior and would help only the one
evaluated sample. A complete C64 character set would require a separate licensed
font and broader display semantics. Stage K therefore makes no production mapping
change. Project-authored `GTEXT` or native line drawing is the clean choice for a
new diagonal pattern; historical PETSCII art remains partial.

## Exact blockers retained

- CIA `$DC00/$DD00`, joystick and keyboard matrices: use a documented source edit
  to `KEY()` for ordinary controls; otherwise unsupported.
- Raster registers, IRQs, cycle timing, fine scroll and sprite multiplexing:
  unsupported, with no simulated substitute.
- SYS, USR, executable DATA and machine-code tails: safely rejected/non-executable.
- Custom character ROM/RAM, banking and arbitrary VIC memory layouts: unsupported.
- IEC channels and OPEN/CLOSE/CMD: unsupported; LOAD/SAVE remain bounded program
  storage rather than device emulation.
- SID filters, exact analogue sound and frame-timed music drivers: unsupported or
  partial as documented in [selected SID compatibility](basic-c64-sid.md).
