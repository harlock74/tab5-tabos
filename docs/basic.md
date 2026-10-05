# BASIC interactive host environment

BASIC is an ordinary independently loaded TabOS application. It uses the pinned
`mist64/cbmbasic` translated native-C core and a public-SDK adapter.

**TabBASIC is not a C64 emulator.** Selected historical BASIC-visible operations
are translated into native TabOS services. There is no 6502 or C64 ROM execution,
arbitrary SYS/USR, machine-code execution from DATA, CIA/raster emulation,
cycle-accurate VIC-II, complete SID fidelity or complete C64 memory map. The
[compatibility matrix](basic-compatibility.md) gives feature-by-feature evidence
and known deviations.

## Build and launch

With the project's SDK toolchain activated:

```sh
eval "$(./tools/tabos activate-idf)"
make -C apps/basic
./tools/tabos macos debug run
```

Enter `basic` in the TabOS shell. Ordinary `./apps/build.sh` builds and installs it
too. It installs as `T:/bin/basic`; license materials go under
`T:/share/licenses/basic/`. The executable requests a 32 KiB stack and 256 KiB heap.

## Try it

Commands accept upper- or lowercase BASIC keywords and variable names:

```basic
PRINT 2+2
10 PRINT "HELLO TABOS"
20 END
LIST
RUN
NEW
10 FOR I=1 TO 5
20 PRINT I
30 NEXT I
40 END
RUN
```

Immediate arithmetic prints numeric 4; RUN prints the greeting or 1 through 5,
respectively. LIST displays the stored program. The banner's 38,911 bytes free
refers to BASIC's language arena, not available TabOS heap.

For packaged programs, controls and provenance, use the
[authoritative example catalogue](basic-examples.md). A quick sample is:

```basic
LOAD "AMAZING.prg"
RUN
```

- Enter submits a line; Backspace removes the last typed character.
- Ctrl+C cancels the current draft or breaks a running program, including pending
  INPUT, and returns to `READY.` with the program retained. INPUT reports
  `BREAK IN <line>`. Text queued before Ctrl+C is canceled; later text is retained.
- Ctrl+Q exits at READY, during command editing, INPUT, GET or a running program.
  Runtime cleanup restores the saved input policy and resumes the existing shell
  with status zero. Unsaved programs are lost. BASIC END/STOP return to READY,
  not to the shell.
- Input is limited to 80 printable ASCII characters per submission. Overflow
  visibly rejects that entire submission rather than executing a truncated line.
  Finish it with Enter, then type a replacement, or cancel with Ctrl+C.
- Backspace uses normalized physical key-down events, including TabOS repeats;
  printable characters and Enter use only text events. Empty Enter submits once.

The minimal editor is not a C64 screen editor: there is no cursor-based line recall
or cursor/history navigation. Command normalization preserves quoted strings, REM
text and DATA fields (including quoted colons); it resumes after a DATA statement's
unquoted colon. INPUT values and GET characters preserve their original case.
For example, `print "Hello TabOS"` prints exactly `Hello TabOS`, and
`10 print "hello"` stores a PRINT statement with lowercase string content.

The logical limit is **80 ASCII bytes**, excluding Enter, including line numbers,
spaces and quotes. It applies to commands and INPUT responses. The pinned core's
reader accepts up to 88 bytes, but the adapter deliberately retains the traditional
80-character bound. Backspace at an empty draft does nothing. Unsupported non-ASCII
or control text rejects the whole submission visibly. Tabs are not expanded.
Visual wrapping and scrolling belong to the existing TabOS terminal; they do not
submit commands. BASIC supplies the submitted newline, so the adapter does not
add a duplicate newline. Empty INPUT preserves the existing variable value (empty string or zero
for a freshly initialized variable). Invalid numeric input displays `?REDO FROM START` and allows retry or break.

## LOAD and SAVE

```basic
SAVE "HELLO"
NEW
LOAD "HELLO"
LIST
RUN
```

SAVE with a bare name maps to `T:/basic/HELLO` and creates `T:/basic` if needed.
LOAD with a bare name first checks the same personal path, then
`T:/data/basic/<name>` for a packaged example if the personal file does not exist.
The filename is exact: no extension is appended. This makes
`LOAD "PONG.prg"` concise while a personal file of that name predictably overrides
the installed copy. An invalid or unreadable personal file reports its error rather
than silently falling through. SAVE never writes into the packaged directory.
Neither command changes the working directory. Explicit absolute drive paths
are supported, for example `SAVE "T:/basic/Hello.BAS"`; their parent directory must
already exist. Quoted filename case is preserved; actual case sensitivity follows
the filesystem (host paths and device FAT can differ).

Names are 1–63 ASCII bytes, including the entire drive path when supplied. Components
may contain letters, digits, spaces, `_`, `-` and `.`, but cannot start with a dot or
space or end in a dot/space. Empty components, traversal, relative subpaths, native
host paths, wildcards and embedded NUL are rejected. Use bare names or `T:/...`-style
absolute paths. `$` directory loads are not supported. Optional device 1 (the core's
default) or 8 is accepted as an alias for this same TabOS storage mapping; only
secondary address 0 is supported. There is no serial-disk or tape emulation.

Files are **tokenized binary programs**, even when named `.BAS`: a little-endian
`$0801` load address followed by linked BASIC lines and the final zero pointer.
They are not plain text. External Commodore tool interoperability has not been
verified; no general C64 PRG compatibility is claimed. LOAD accepts only this fixed
address and a bounded, validated program structure; machine-code tails, relocation,
invalid links/tokens and out-of-arena data are rejected.

SAVE overwrites existing files. It writes and closes an exclusively created sibling
`<name>.tmp0` (or `.tmp1` through `.tmp15` if occupied) before renaming. On Tab5 FAT,
which cannot rename over an existing destination, it first moves the old file to
`<name>.bak`, promotes the completed temporary file, then removes the backup.
An existing backup blocks that fallback. Promotion failure attempts to restore the
old file; failures leave recovery files and produce an explicit error. A backup
cleanup failure reports that the new SAVE completed but cleanup failed. Use shell
file tools to inspect recovery files; do not delete them blindly.

This is not power-loss atomicity or a concurrent-writer transaction. No durability
barrier beyond successful write/close is claimed. Storage operations are synchronous;
control shortcuts are processed after an operation returns and cleans up.

LOAD reads and closes a staging buffer, validates the complete file, and only then
replaces the live program. Missing, invalid, oversized, truncated or unreadable files
produce a specific message followed by BASIC's existing error/READY recovery; the
stored program remains available to LIST/RUN. Variable state and continuation are
not promised across error recovery. A valid LOAD uses the core's normal program
relink/reset behavior.

Host and physical Tab5 persistence tests passed with limitations. The automated
storage suite covers simulated failures; firmware identity and physical resource
high-water measurements remain unverified.
See [game development](basic-game-development.md) for copying an installed example
to a new personal name and [the Stage K corpus](basic-c64-programs.md) for evaluated
historical and C64-style programs.

## Current limits

SYS, USR and WAIT are rejected at execution. Selected PEEK/POKE addresses now use
the separate, bounded [Stage I virtual C64 graphics profile](basic-c64-graphics.md).
Unsupported addresses print `?UNSUPPORTED C64 ADDRESS $xxxx`, followed by the
core's `?ILLEGAL QUANTITY ERROR`, and return to READY. No BASIC numeric address is
mapped into interpreter, TabOS or native memory.
VERIFY, OPEN, CLOSE, CMD and general channel selection remain disabled. LOAD/SAVE
use a bounded application-local adapter; no upstream desktop file or plugin
implementation is linked.

ASCII text, CR/newline and cursor-right-as-space output are supported. Other
PETSCII controls are ignored. Native graphics, palette colors, software sprites and
PCM tones are documented in [TabBASIC graphics](basic-graphics.md); they do not
implement PETSCII controls. Cursor query uses the H0 logical column tracker. High-bit CP437/PETSCII graphic
bytes are not interpreted as glyphs: line entry rejects them if they reach the
application, while output drops them. ASCII-range PETSCII codes display as ASCII.
The macOS SDL input layer filters non-ASCII text before delivery, so such text may
be ignored there without an application error. No full PETSCII mapping is provided.
INPUT, GET, interruption, line limits and editing have native and actual-RV32 host
checks; this is not full language or PETSCII compatibility certification.
The adapter holds 256 pending characters. Overflow is reported and discards queued
input through the next Enter, never executing a truncated suffix. INPUT waits for a
replacement response. GET reports overflow and returns an empty result at the error
marker. This cannot recover events already lost upstream of the public input API.

TI/TI$ use a session-local monotonic offset; they do not set the TabOS clock.
RND timer bytes use a repeatable session seed, without a C64 hardware timer.
These adaptations retain the Stage F language and Stage G/H0 policy regressions.

Host tests and a user-operated physical Tab5 session validate the documented
functional checks; the installed device firmware version was not identified. The fixed language RAM and rejected hardware
commands do not establish a general security sandbox for arbitrary malformed BASIC.
A repeated mathematical-expression workload is interruptible in host tests; this
does not establish exhaustive execution-path coverage. Resource exhaustion and
hardware watchdog/heap/stack behavior remain later validation work.

See [upstream provenance and patches](../apps/basic/UPSTREAM.md), the
[compatibility matrix](basic-compatibility.md), and the checked-in tests for the
maintained evidence. The source retains ROM-derived static-translation data; this
integration is not legal clearance of underlying Commodore/Microsoft-derived
material.

## Language compatibility notes

Variable names use only their first two significant characters: ABC and ABD alias.
Keyword fragments inside names are tokenized, so SCORE is not a usable ordinary
variable name. Integer assignment follows historical rounding behavior; for example,
A%=-5.9 stores -6. Strings can be constructed up to 255 bytes, independently of the
80-byte line editor limit. RUN clears variables, strings and arrays; CLR clears them
while preserving program lines.

STOP reports BREAK and END returns quietly to READY. Both retain the program and
allow CONT in the tested cases; editing a program after STOP invalidates continuation.
Ctrl+C breaks through BASIC; Ctrl+Q exits to the shell and loses unsaved in-memory
state. Full C64 keyboard/PETSCII and display behavior are not provided.

PRINT semicolon and numeric spacing work. Comma zones, TAB() and POS() share a
logical output-column tracker using the current TabOS terminal width rather than a
40-column C64 screen. SPC(n) emits a fixed number of spaces. RND(0) uses session-
local pseudo-timer data rather than physical CIA timers. See the compatibility
matrix for exact observations, errors and unsupported facilities.

## C64-specific policy and clock limitation

The [Stage G command policy](basic-c64-policy.md) retains all current machine and
channel safety gates. [Native graphics](basic-graphics.md) remains independent of
the separate [Stage I virtual C64 graphics profile](basic-c64-graphics.md). The
profile implements selected display and sprite bytes, not general C64 emulation.

TI counts session-local 60 Hz ticks and wraps after 24 hours; TI$ reads the same
clock as HHMMSS. Both use monotonic elapsed time, not the TabOS system clock.
Stage H0 repairs assignment: `TI$="123456"` sets this session to 12:34:56.
The value must contain exactly six decimal digits with hours 00–23 and minutes/
seconds 00–59. Invalid values report a BASIC error and preserve the clock. This
strict HHMMSS validation is an intentional adaptation; historical BASIC accepts
some out-of-range fields. The TabOS RTC is never changed.

POS(), comma zones and TAB() now share a logical output-column tracker with the
editor and adapter diagnostics. Public TTY width determines wrapping; there is no
emulated 40-column screen. Columns above 255 saturate at the KERNAL byte boundary.
See the [compatibility matrix](basic-compatibility.md) for current behavior and
test coverage.


## Native games

Stage H adds [graphics, sprites, keyboard state, timing and sound](basic-graphics.md).
After installing current applications, try `LOAD "PONG.prg"` and RUN.
Pong's movement, collisions and scoring are BASIC code. Up/Down moves the paddle,
Space restarts and B returns to text. Ctrl+C breaks; Ctrl+Q exits to the shell.
See the [graphics guide](basic-graphics.md) for validation status and limits.

## Safe C64 graphics subset

Stage I supports selected pure BASIC screen/colour/sprite PEEK/POKE listings through
separate virtual state. Try `LOAD "C64SCREEN.prg"`, then RUN. The
supplied `C64COLORS`, `C64SPRITE` and `C64ANIM` examples exercise the other supported
paths. Use Ctrl+C for the continuous animation; type TEXT after a finite example to
restore the terminal. The [C64 graphics profile](basic-c64-graphics.md) lists every
implemented address, palette/font behavior, lifecycle and compatibility tier.

Stage J dirty-region rendering changed the physical moving-sprite benchmark from
24 presents and almost unusable stutter to 278 presents and mostly smooth motion.
The public SDK PRESENT path remains the dominant measured cost, and brief physical
presentation artifacts remain documented limitations.

## Selected C64 SID subset

Stage J accepts bounded virtual `$D400-$D418` PEEK/POKE state and cooperatively
synthesizes three voices through the public TabOS PCM API. Try
`LOAD "SIDTONE.prg"`; see the [example catalogue](basic-examples.md) for every
sound and combined demonstration. See the
[selected SID profile](basic-c64-sid.md) for registers, the fixed clock formula,
waveforms, approximate ADSR, cleanup behavior and fidelity limits. Native `SOUND`
remains independent. This is not full SID or C64 emulation.
