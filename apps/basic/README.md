# TabBASIC

TabBASIC is an ordinary TabOS RV32 application built around the pinned translated
native-C Commodore BASIC V2 core. It provides an interactive BASIC environment,
TabOS-native graphics, sprites, held-key input and sound, plus a bounded
compatibility layer for selected BASIC-visible C64 screen, sprite and SID POKEs.

**TabBASIC is not a C64 emulator.** It does not boot C64 ROMs or execute 6502
machine code. SYS, USR and WAIT remain disabled, and CIA, raster, joystick-matrix,
cycle-accurate VIC-II and complete SID behavior are not implemented.

## Build and install

Activate the repository SDK and use the ordinary application Makefile:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/basic
```

This installs `T:/bin/basic`, packaged programs under `T:/data/basic/`, and the
required notices under `T:/share/licenses/basic/` in the normal staging root. The
ordinary `./apps/build.sh` application build also includes BASIC. The application
requests a 32 KiB stack and 256 KiB heap.

To use the host TabOS environment:

```sh
./tools/tabos macos debug run
```

At the TabOS shell, enter `basic`.

## Quick start

Keywords may be typed in upper or lower case:

```basic
PRINT 2+2
10 PRINT "HELLO TABOS"
20 END
RUN
```

Save and restore the stored program:

```basic
SAVE "HELLO"
NEW
LOAD "HELLO"
RUN
```

Try the packaged historical maze, native Pong and compatibility game:

```basic
LOAD "AMAZING.prg"
RUN
```

```basic
LOAD "PONG.prg"
RUN
```

```basic
LOAD "C64DODGE.prg"
RUN
```

Pong uses Up/Down (or A for up), Space to restart and B to return to text.
C64DODGE uses Left/Right and B. Ctrl+C cancels an edit or breaks a running
program while retaining it. Ctrl+Q exits BASIC and restores the shell.

The [authoritative example catalogue](../../docs/basic-examples.md) lists every
installed example, its controls, graphics/audio use and provenance.

## Editing and storage

The line editor accepts up to 80 printable ASCII bytes. Enter submits once and
Backspace deletes the preceding character. Lowercase normalization affects BASIC
syntax while preserving strings, REM text and DATA values. INPUT and GET preserve
entered case.

SAVE with a bare name writes only `T:/basic/<name>`. LOAD with a bare name checks:

1. `T:/basic/<name>`;
2. only if that file is absent, `T:/data/basic/<name>`.

Names are exact; no `.prg` extension is added. A personal program intentionally
overrides a packaged example. If that personal file is unreadable or malformed,
LOAD reports the error instead of silently using the packaged copy. Explicit
TabOS drive paths are also supported within the documented filename rules.

## Native extensions

New games should normally use TabBASIC's native commands:

- GRAPHICS, TEXT, CLS and PRESENT;
- COLOR, PSET, LINE, RECT, CIRCLE and GTEXT;
- SPRITE, SPRITEROW, SPRITEPOS, SPRITESHOW and SPRITEHIDE;
- KEY() for held keys and SLEEP for cooperative pacing;
- SOUND for a bounded synchronous PCM tone.

See [native graphics and sound](../../docs/basic-graphics.md) and the
[game-development tutorial](../../docs/basic-game-development.md).

## Bounded C64 compatibility

Selected screen RAM, colour RAM, sprite bytes/registers and `$D400-$D418` SID
registers live in separate, bounded application state and are translated to public
TabOS graphics and PCM services. They never alias interpreter or native memory.
This path supports useful pure-BASIC listings within the documented profile; it
does not provide arbitrary C64 memory or hardware behavior.

See [safe C64 graphics](../../docs/basic-c64-graphics.md),
[selected SID compatibility](../../docs/basic-c64-sid.md), the
[program evaluation](../../docs/basic-c64-programs.md), and the
[feature matrix](../../docs/basic-compatibility.md).

## Major limitations

- The TabOS terminal and keyboard replace the C64 screen editor, PETSCII keyboard
  and 40-column display. PETSCII graphics are incomplete.
- Native Pong is playable but physically slow.
- The optimized compatibility sprite path is mostly smooth on tested hardware,
  while full-canvas PRESENT can still show brief construction artifacts.
- Physical SID output is quiet and buzzy; occasional multivoice interruption is
  possible. Analogue SID fidelity is not claimed.
- LOAD/SAVE use TabBASIC's validated tokenized format and TabOS paths, not IEC or
  tape-device emulation.
- SYS, USR, WAIT, CIA/raster behavior and machine-code execution are unsupported.

The [main user guide](../../docs/basic.md) documents syntax, storage and lifecycle
details. [Upstream provenance](UPSTREAM.md) records the pinned core, hashes, local
patch and licensing qualification. The checked-in test suite and compatibility
documentation provide the maintained validation evidence.

## Release verification

```sh
cmake --build --preset macos-debug
ctest --preset macos-debug --output-on-failure
python3 apps/basic/tests/compatibility.py --rv32-corpus /tmp/basic-rv32.tsv
build/macos-debug/tests/tabos_basic_rv32 \
  build/apps/shell/shell build/apps/basic/basic /tmp/basic-rv32.tsv --policy
python3 apps/basic/tests/verify_upstream.py
```

Repeat the build, CTest and RV32 steps with `macos-release`. The native Debug
targets use the repository ASan/UBSan configuration. The RV32 harness exercises
the real loader, SDK, process ownership, input, filesystem, graphics, audio and
terminal services.
