# TabBASIC native graphics and games

TabBASIC provides a 320×200 RGB565 canvas through the public TabOS graphics SDK.
Game rules live in BASIC. These commands remain independent of the optional
[virtual C64 graphics profile](basic-c64-graphics.md). SYS, USR and WAIT remain
disabled; selected PEEK/POKE addresses use separate virtual C64 state.

## Try the supplied programs

Build/install applications using the ordinary SDK workflow. The BASIC runtime
assets are installed under `T:/data/basic/` alongside the application in `T:/bin`.
At the TabOS shell, run `basic`, then enter:

```basic
LOAD "PONG.prg"
RUN
```

Move with Up/Down (A also moves up). Space restarts the score and ball; B returns
to text. The right paddle is controlled by BASIC code. Ctrl+C interrupts and
preserves the program; Ctrl+Q exits BASIC to the shell.

For the palette, shapes and sprite demo:

```basic
LOAD "GRAPHICS.prg"
RUN
```

Arrows move the sprite; Space returns to text. Source programs are
[PONG.bas](../apps/basic/examples/PONG.bas) and
[GRAPHICS.bas](../apps/basic/examples/GRAPHICS.bas). The `.prg` assets are generated
with the production interpreter and checked against their source by the tests.
The [example catalogue](basic-examples.md) is the authoritative installed list.

## Drawing and presentation

`GRAPHICS` acquires fullscreen ownership, allocates a 128000-byte (125 KiB) logical
canvas, clears it to black, resets sprites and selects white. Calling it again
while already in graphics preserves the canvas/state and presents it. On the
1280×720 display the SDK selects 3× scaling: a centered 960×600 image with black
letterboxing. The application does not access the physical framebuffer.

Drawing updates the logical backbuffer. `PRESENT` displays the current background
and visible sprites. There is no automatic presentation per drawing primitive.
Text output and READY are retained in the terminal while graphics owns the display;
they become visible on `TEXT`, Ctrl+C or an error. For interactive direct-mode
experiments, type the next command even while the terminal is hidden. Ctrl+C
provides a quick visible return to READY. A graphical program can deliberately
leave its final picture displayed until TEXT; END alone does not close graphics.

```basic
GRAPHICS
CLS
COLOR 5
RECT 20,20,80,60,1
COLOR 1
CIRCLE 160,100,30
PRESENT
TEXT
```

All commands work in direct mode and stored programs, with colon-separated
statements, BASIC expressions, IF/THEN, LIST, SAVE, LOAD and RUN.

| Command | Meaning and limits |
| --- | --- |
| `GRAPHICS` | Enter the fixed logical graphics mode; no arguments. |
| `TEXT` | Release graphics and restore the retained terminal; no arguments. |
| `CLS [color]` | Clear the background, default black (0). Does not delete/hide sprites. |
| `COLOR color` | Select drawing color 0–15; valid in text mode too. Entering a new graphics session resets it to white. |
| `PSET x,y` | One pixel. |
| `LINE x1,y1,x2,y2` | Line with both endpoints included. |
| `RECT x,y,width,height[,filled]` | Width/height 0–1024; zero area draws nothing. Final pixel is x+width−1, y+height−1. `filled` is 0 (outline/default) or 1. |
| `CIRCLE x,y,radius[,filled]` | Midpoint rasterization, radius 0–512; radius zero is one pixel. Filled flag 0/1 as above. |
| `GTEXT x,y,string$` | Up to 64 bytes in a small transparent 5×7 font, six-pixel character advance. Digits, Latin letters, space, colon and hyphen; lowercase glyphs render uppercase and other characters render `?`. String bytes themselves are preserved. |
| `PRESENT` | Present background and sprites; no arguments. |

Drawing coordinates are −1024 through 1024, clipped safely to the canvas. Valid
wholly off-screen shapes have no visible effect. Numeric arguments use BASIC
expressions, are truncated toward zero, and must fit signed 16-bit values before
command-specific bounds apply. Shapes never write outside the SDK canvas. Invalid
types/counts/ranges produce native BASIC errors. Errors close graphics and restore
a usable READY prompt without destroying the program. Device acquisition/presentation
failure also reports ILLEGAL QUANTITY; it does not terminate BASIC.

## Palette

RGB values are quantized by the SDK's RGB565 conversion. Names are descriptive;
these are native palette indices, not emulated hardware registers.

| Index | Name | RGB |
| --- | --- | --- |
| 0 | Black | 0,0,0 |
| 1 | White | 255,255,255 |
| 2 | Red | 220,60,60 |
| 3 | Cyan | 60,220,220 |
| 4 | Purple | 180,70,210 |
| 5 | Green | 70,190,80 |
| 6 | Blue | 60,90,220 |
| 7 | Yellow | 240,220,70 |
| 8 | Orange | 240,140,50 |
| 9 | Brown | 140,85,45 |
| 10 | Light red | 250,150,150 |
| 11 | Dark gray | 65,65,65 |
| 12 | Gray | 120,120,120 |
| 13 | Light green | 160,240,160 |
| 14 | Light blue | 150,180,255 |
| 15 | Light gray | 195,195,195 |

## Sprites

There are 16 software sprites, IDs 0–15. Each has its own image, up to 16×16 pixels,
position and visibility. They are application objects, not hardware sprites.

```basic
SPRITE 0,3,2
SPRITEROW 0,0,".1."
SPRITEROW 0,1,"121"
SPRITEPOS 0,160,100
SPRITESHOW 0
PRESENT
```

| Command | Meaning |
| --- | --- |
| `SPRITE id,width,height` | Define/reset a sprite; dimensions 1–16. Initially transparent, hidden, at 0,0. |
| `SPRITEROW id,row,string$` | Set one row, 0 through height−1. String length must equal width. Hex digits 0–F (either case) select colors; `.` is transparent. |
| `SPRITEPOS id,x,y` | Move a defined sprite; signed 16-bit coordinates, safely clipped on presentation. |
| `SPRITESHOW id` | Show a defined sprite. |
| `SPRITEHIDE id` | Hide a defined sprite. |

A row can come from DATA/READ, a string variable or a string expression. No POKE
or asset-memory addresses are exposed. Higher-numbered sprites appear on top.
PRESENT temporarily composites them and restores the background in reverse order;
moving/hiding them does not leave trails, including overlaps. Text-mode return,
errors and exit release/reset sprites; stored BASIC code remains intact.

## Keyboard, timing and sound

`KEY("LEFT")`, `KEY("RIGHT")`, `KEY("UP")`, `KEY("DOWN")`, `KEY("SPACE")`,
`KEY("A")` and `KEY("B")` return −1 while held and 0 otherwise. Names are case
insensitive. Other names produce ILLEGAL QUANTITY. Public normalized key-down/up
events supply this state; there is no private keyboard access. `KEY()` does not
consume GET's one-shot character queue. Ctrl+C/Q stay reserved. Because KEY does not consume text events, a printable
exit key such as B can appear as an unfinished BASIC command after READY. Exiting
BASIC ends that printed line before the shell prompt; it does not transfer that
character into the shell's edit buffer. Cooked printable
key repeats can still fill the bounded character queue if a long-running program
never consumes GET; the existing visible queue-rejection policy remains in force.

`SLEEP milliseconds` accepts 0–5000. It yields through public sleeps in at most
10 ms slices and services input between them. Zero duration polls input without a
busy loop. TI/TI$ remain the session clock; no separate game clock is introduced.

`SOUND frequency,duration` plays a quiet square-wave tone, 20–20000 Hz and 1–2000 ms,
using public 44100 Hz mono signed 16-bit PCM. This first API is synchronous: BASIC
waits cooperatively while the tone is queued/drained. Ctrl+C/Q remain responsive.
The stream is closed after the tone, interruption or exit. A duration+500 ms
watchdog bounds an unresponsive service ring, followed on successful drain by a
100 ms cooperative settling interval for downstream device queues. Public status
does not report physical playback completion; that final interval is an adaptation,
not a guarantee of exact audible duration. Unavailable/busy audio prints
`?TABOS AUDIO UNAVAILABLE` before BASIC's ILLEGAL QUANTITY error. There is one voice,
no SID emulation and no persistent audio ownership. Pong is silent, so unavailable
audio does not prevent playing it.

## Persistence and compatibility

Native keywords use distinct tokens CC–DD, leaving existing Commodore tokens
unchanged. New keywords are reserved wherever BASIC scans keywords; avoid using
them as variable-name prefixes. Quoted strings, REM and DATA text retain their
contents. Saved graphical programs use the existing validated tokenized format
with the accepted token range extended through DD. These extensions are specific
to TabBASIC; they do not make a saved graphical program compatible with a C64 or
an older TabBASIC build. Unknown tokens still fail LOAD before replacing the program.

Examples can be edited and saved under a personal name, for example
`SAVE "MY PONG"`. Runtime assets in `T:/data/basic/` are replaced by application
installation; personal programs in `T:/basic/` are separate. Bare LOAD checks a
same-named personal file first, then the packaged example. SAVE always targets the
personal directory. See the [game-development tutorial](basic-game-development.md).

## Resources and validation

The heap request remains 256 KiB and the stack request remains 32 KiB. The SDK
canvas allocates 125 KiB. Sprites and their saved underlays occupy 16704 static
bytes, expression continuations 832 bytes, held-key state 128 bytes and PCM staging
512 bytes. No extra virtual C64 RAM is allocated. Compiler stack-usage output
still reports 17680 bytes for the generated core; this is not a measured physical
stack watermark. Public audio additionally owns an OS-side stream buffer.

The checked-in graphics, example, release-session and RV32 tests retain the
automated evidence. Measurements describe the tested workload/backend; no fixed
frame rate or break-latency guarantee is made.

To regenerate the checked-in assets after editing the BASIC sources:

```sh
cmake --build --preset macos-debug
python3 apps/basic/tests/build_examples.py build/macos-debug/tests/tabos_basic_native apps/basic/examples
make -C apps/basic install
```

Run `component.basic_graphics` and `component.basic_examples` with CTest before
installing edited assets. Full Stage F/G language, safety, input and persistence
regressions remain required for interpreter/adapter changes.
