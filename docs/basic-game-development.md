# Build a game with TabBASIC

New TabBASIC games should normally use the native graphics, input and sound
commands. The C64-style POKE path exists for evaluating or adapting compatible old
BASIC listings. Native commands are clearer, faster to develop with and independent
of magic hardware addresses.

## 1. Create and save a program

Start `basic` at the TabOS shell, enter numbered lines, then LIST and SAVE:

```basic
10 PRINT "MY GAME"
20 END
LIST
SAVE "MYGAME"
```

Personal programs are stored under `T:/basic/`. Packaged examples live under
`T:/data/basic/`. A bare LOAD such as `LOAD "PONG.prg"` checks the personal
directory first and then packaged examples. SAVE always writes the personal file,
so an installed example is never overwritten.

Use `ls T:/data/basic` at the shell to list packaged examples. The
[example catalogue](basic-examples.md) records their controls and provenance.
Inside BASIC:

```basic
LOAD "PONG.prg"
SAVE "MY PONG"
```

The second command creates an editable personal copy. A personal file with the same
bare name intentionally takes precedence over the packaged one; use the explicit
`T:/data/basic/PONG.prg` path when you need the installed original.

## 2. Enter graphics and draw

The native canvas is 320×200. Drawing changes the backbuffer; PRESENT displays it.

```basic
10 GRAPHICS:CLS 0:COLOR 7
20 RECT 20,20,80,40,1
30 COLOR 1:CIRCLE 160,100,25
40 PRESENT:SLEEP 1000:TEXT
```

PSET, LINE, RECT, CIRCLE and GTEXT clip safely. Palette indices are 0–15. TEXT
returns to the terminal; Ctrl+C also closes graphics and preserves the program.

## 3. Define and move a sprite

Native software sprites are 1–16 pixels wide and high. A dot is transparent and
hexadecimal digits select palette colours.

```basic
10 GRAPHICS:SPRITE 0,3,3
20 SPRITEROW 0,0,".7.":SPRITEROW 0,1,"777"
30 SPRITEROW 0,2,"7.7":SPRITESHOW 0
40 X=20:SPRITEPOS 0,X,100:PRESENT
```

Move it by changing X, calling SPRITEPOS and presenting the next frame. The runtime
restores old sprite regions without leaving trails.

## 4. Read held keys

`KEY()` returns −1 while a supported physical key is held and zero otherwise.

```basic
50 IF KEY("LEFT") THEN X=X-2
60 IF KEY("RIGHT") THEN X=X+2
70 IF KEY("B") THEN TEXT:END
80 SPRITEPOS 0,X,100:PRESENT
90 SLEEP 16:GOTO 50
```

Available names are LEFT, RIGHT, UP, DOWN, SPACE, A and B. Ctrl+C and Ctrl+Q stay
reserved for break and application exit. A printable held key can also create a
normalized text event, so arrow keys are preferable for continuous movement.

## 5. Pace the loop

`SLEEP milliseconds` yields to TabOS and services input; it does not busy-wait.
Use a small value such as 16 as a pacing request, then test on the Tab5. PRESENT
cost and BASIC execution mean this is not a fixed-frame-rate guarantee. Keep game
rules independent of an assumed C64 raster or CPU cycle rate.

## 6. Add native sound

`SOUND frequency,duration` plays one synchronous tone while still servicing Ctrl+C
and Ctrl+Q:

```basic
100 SOUND 660,80
```

Frequency is 20–20000 Hz and duration is 1–2000 ms. Frequent synchronous tones
pause the game loop, so use them as short cues. Native SOUND is the normal choice
for a new game; selected SID POKEs are for compatible old listings.

## 7. Build a small loop safely

Keep state in BASIC variables, clamp coordinates, draw one frame, PRESENT once,
then yield. Handle the exit key before expensive drawing. Use Ctrl+C during
development; LIST and SAVE still work after a break.

`PONG.prg` is a complete native example with paddles, collision, scoring and
restart. It is playable but physically slow; Stage K does not silently rewrite its
accepted behavior. `GRAPHICS.prg` is a smaller command tour.

## 8. Port a compatible C64 BASIC listing

The compatibility path supports selected screen RAM, colour RAM, basic VIC sprite
registers and `$D400-$D418` sound registers. Keep those POKEs when the purpose is
to evaluate an old listing. Do not translate the sample to native GRAPHICS and then
call it C64 compatibility.

Replace a simple joystick or keyboard-matrix dependency at source level:

```basic
REM OLD: J=PEEK(56320)
IF KEY("LEFT") THEN X=X-2
IF KEY("RIGHT") THEN X=X+2
```

Record every changed line. Programs requiring SYS/USR machine code, CIA or raster
timing remain unsupported. `C64DODGE.prg` is a self-authored example of the bounded
screen/sprite/SID path with explicit KEY controls; it is not a historical game.

## 9. Choose the appropriate API

| Goal | Preferred interface |
| --- | --- |
| New TabBASIC game | GRAPHICS, drawing commands, native sprites, KEY(), SLEEP, SOUND |
| Test an old pure BASIC text program | Standard BASIC language and terminal |
| Port an old compatible POKE listing | Bounded virtual C64 screen/sprite/SID profile |
| Program needs machine code, raster IRQ or CIA | Outside TabBASIC compatibility scope |

See [native graphics](basic-graphics.md), [C64 graphics](basic-c64-graphics.md),
[selected SID compatibility](basic-c64-sid.md) and the
[program corpus](basic-c64-programs.md) for exact limits and tested examples.
