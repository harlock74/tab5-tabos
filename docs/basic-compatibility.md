# TabOS BASIC compatibility

This is tested language coverage, **not a claim of general C64 compatibility**.
Stage F characterized the Stage E implementation (`f528d51`; repository baseline
`1c8cb67`). Stage H adds the focused clock/cursor repairs and native graphics
extensions described below, while retaining the language and safety regressions.

PASS means the specified tests match the intended BASIC V2/cbmbasic behavior within
stated adaptations. PARTIAL means a known difference or adapted subset remains.
UNSUPPORTED means deliberately disabled. NOT TESTED means no evidence was obtained.
A PASS does not establish every possible input or combination.

Stage G adds [C64-specific policy](basic-c64-policy.md). Stage H0 repairs the missing
TI$ assignment helper and implements logical POS/comma/TAB tracking. The generated
core and storage remained unchanged at H0. H1 adds distinct native tokens and a
bounded continuation bridge. Stage I redirects selected PEEK/POKE operations into
a separate virtual C64 graphics state while SYS/USR/WAIT remain disabled. See the
[Stage I graphics profile](basic-c64-graphics.md) for the exact address map.
Stage J optimizes that renderer and adds the bounded Tier 3A register-driven sound
profile documented in [selected SID compatibility](basic-c64-sid.md).
Stage K's [program corpus](basic-c64-programs.md) applies these feature results to
historical listings and self-authored compatibility demonstrations without making
a percentage-of-games claim.

## Evidence and reproduction

Case IDs below refer to [compatibility.py](../apps/basic/tests/compatibility.py).
The suite runs 129 cases: 123 cases are also exported as 421 command/line-entry
checks for the actual SDK-built RV32 application; six additional interactive
conversations run against the native production core/adapter. Both paths compare
complete result text after removing the command echo and outer whitespace. Internal
spaces and line breaks remain significant; interactive conversations compare the
complete byte transcript, including prompts and numeric spaces. Nontrivial math
checks use an absolute tolerance of 0.000001 expressed as BASIC comparisons.

`native.py` and `rv32.c` retain Stage C/E editing, INPUT/GET, interrupt, lifecycle and
persistence regressions. `storage.c` retains filesystem fault injection. Native
execution uses the production interpreter/adapter with a test SDK boundary; RV32
execution uses the actual TabOS loader, SDK, process, input, console and filesystem.
Temporary storage isolates all writes. New language results are host evidence;
the compact physical sample also passed. It covered DATA/FOR/READ/GOSUB,
LEFT$/SQR, an intentional error, SAVE/LOAD, Ctrl+C and Ctrl+Q/shell recovery.

```sh
cmake --build --preset macos-debug
ctest --preset macos-debug --output-on-failure
python3 apps/basic/tests/compatibility.py build/macos-debug/tests/tabos_basic_native \
  --transcript /tmp/basic-f-debug.json
python3 apps/basic/tests/compatibility.py --rv32-corpus /tmp/basic-i-rv32.tsv
build/macos-debug/tests/tabos_basic_rv32 \
  build/apps/shell/shell build/apps/basic/basic /tmp/basic-i-rv32.tsv
python3 apps/basic/tests/verify_upstream.py
```

Build the shell and BASIC artifacts with the ordinary SDK application workflow if
absent. Repeat with `macos-release`. CTest does not require SDK artifacts; the
optional RV32 corpus is generated from the same checked-in expectations, not from
observed interpreter output. A mismatch fails the run. The original three-argument
RV32 invocation still runs its existing acceptance checks.

## Compatibility matrix

| Feature | Status | Evidence | Notes |
| --- | --- | --- | --- |
| Direct mode | PASS | `arithmetic.*`, `variables.*` | Immediate commands, lowercase commands, READY recovery. |
| Program lines | PASS | `program.management`, `list.lexical` | Ordering, replacement, deletion; 80-byte editor bound remains. |
| RUN | PASS | `program.management`, `run.clears` | Whole program/line target; clears variables. |
| LIST | PASS | `program.management`, `list.lexical`, `storage.roundtrip` | All, single line, range, token rendering, quoted/REM/DATA case, loaded program. |
| NEW | PASS | `program.management`; every case setup | Removes program; subsequent cases start with cleared variables. |
| CLR | PASS | `clr` | Resets numbers, strings, arrays; preserves program. |
| PRINT | PARTIAL | `print.*`, `data.sample-numeric`, native INPUT transcripts | Semicolons/numeric spaces/SPC work; H0 tracks comma/TAB positions using TabOS terminal width. |
| LET | PASS | `variables.tokenization` | Explicit LET and implicit assignment. |
| Arithmetic | PASS | `arithmetic.*` | Precedence, signs, fractions, large values, overflow/underflow, division errors. |
| Numeric variables | PASS | `variables.*` | Float/integer types, defaults, first-two-character aliasing, reserved-word tokenization. |
| String variables | PASS | `strings.*` | ASCII mixed case, comparison, concatenation, empty/reassignment, 255-byte bound, storage churn. |
| Arrays | PASS | `arrays.*` | Numeric/string, multidimensional, implicit DIM, errors and bounded capacity. |
| IF/THEN | PASS | `if.*` | Statement/line targets, false branch skips remaining colon statements, strings. |
| GOTO | PASS | `goto.*` | Forward branch, nonexistent/invalid target diagnostics. |
| GOSUB/RETURN | PASS | `gosub.*`, `goto.missing` | Nested calls, return, missing target/orphan RETURN. |
| FOR/NEXT | PASS | `for.*` | Default, negative/fractional/zero STEP, nesting and errors. |
| DATA/READ/RESTORE | PASS | `data.*` | Numbers, quoted/unquoted mixed-case strings, commas, repeated READ, RESTORE, errors. |
| INPUT | PARTIAL | `input.*`; existing native/RV32 tests | BASIC conversion/prompts and retry work; TabOS ASCII line editor and Ctrl+C replace C64 input/editor. |
| GET | PARTIAL | `get.*`; existing native/RV32 tests | Empty no-key result, case/digits/punctuation/order, Ctrl+C/Q; normalized TabOS input, not C64 PETSCII keyboard. |
| DEF FN | PASS | `def-fn.*` | Multiple calls, expression use, parameter scope restored, invalid direct/undefined use. |
| ON GOTO | PASS | `on.*` | Selection, zero/out-of-list fallthrough, fractional selector, invalid range. |
| ON GOSUB | PASS | `on.gosub`, `on.fallthrough` | Selected call returns; out-of-list falls through. |
| STOP | PASS | `stop.cont` | BREAK IN line, program/variables retained, CONT resumes. |
| END | PASS | `end.cont` | Silent READY; CONT resumes at next statement. Does not exit application. |
| CONT | PARTIAL | `stop.cont`, `end.cont`, `cont.*`; upstream probes | STOP/END resume, editing invalidates; CONT after NEW on empty program silently returns, unlike manual's general error description. |
| String functions | PARTIAL | `functions.*` | LEN/LEFT$/RIGHT$/MID$/ASC/CHR$/STR$/VAL ASCII/edge cases pass; full PETSCII rendering absent. |
| Math functions | PASS | `math.*` except RND | ABS/INT/SGN/SQR/SIN/COS/TAN/ATN/LOG/EXP, nontrivial tolerance and domain errors. |
| RND | PARTIAL | `math.rnd`; KERNAL IOBASE audit | Negative seed reproducibility and positive range checked; zero argument uses deterministic pseudo-timer bytes, not CIA timers. |
| Logical/comparison operators | PASS | `comparison.*`, `arithmetic.logic`, `arithmetic.relations` | Six comparisons, AND/OR/NOT and mixed expressions; true -1, false 0. |
| REM | PASS | `list.lexical`; native/RV32 lexical tests | Case and text retained; not executed. |
| LOAD | PARTIAL | `storage.roundtrip`; Stage E native/RV32/storage regressions | Restricted tokenized $0801 format and TabOS paths/devices, failed LOAD preserves program. |
| SAVE | PARTIAL | Same as LOAD | Restricted filename/format, recoverable overwrite; no IEC/tape behavior. |
| VERIFY | UNSUPPORTED | `unsupported.verify`; native/RV32 denied commands | Rejected before LOAD file access. |
| PEEK | PARTIAL | `c64.peek`, `c64_graphics.py`, actual RV32 pixels, physical Stage I | Only the fixed virtual VIC bank, colour RAM and listed VIC registers; unsupported addresses error. |
| POKE | PARTIAL | Same plus physical screen/sprite/transition/break tests | Writes only separate virtual state; never interpreter/native RAM. Optimized sprite motion is mostly smooth; full-canvas presentation artifacts remain. |
| SYS | UNSUPPORTED | `unsupported.sys`; native/RV32 direct/conditional/stored SYS | Core gate before translated/native dispatch. |
| USR | UNSUPPORTED | `unsupported.usr`; native/RV32 denied commands | Gate before indirect RAM-vector dispatch. |
| WAIT | UNSUPPORTED | `unsupported.wait`; native/RV32 denied commands | Core gate before polling addresses. |
| OPEN/CLOSE/CMD | UNSUPPORTED | `unsupported.open/close/cmd`; native/RV32 denied commands | No general files/channels/process execution. |
| Full PETSCII/control glyphs | UNSUPPORTED | Runtime design; high-code storage only tested | ASCII input and TabOS CP437 terminal, no expanded emulation. |
| External Commodore file interoperability | NOT TESTED | None | Local tokenized roundtrip is not external-tool interoperability. |
| TI | PARTIAL | Stage G `policy.py`, RV32 `--policy` | Session-local monotonic 60 Hz clock, daily wrap; numeric assignment errors. |
| TI$ | PARTIAL | H0 `policy.py`, RV32 `--policy` | Reads and assignment work; strict valid six-digit HHMMSS fields are a deliberate adaptation. |
| POS | PARTIAL | H0 `policy.py`, RV32 `--policy` | Logical cursor follows output and public TTY wrapping; byte-sized result saturates above 255. |
| ST | PARTIAL | Stage G `policy.py`, RV32 `--policy` | Always zero with no channels; not working device status. |
| PRINT#/INPUT#/GET# | UNSUPPORTED | Stage G `policy.py` | General channel selection remains disabled; direct GET# first errors ILLEGAL DIRECT. |
| FRE and other unlisted facilities | NOT TESTED | No executed evidence claimed | No compatibility claim from token presence. |

## Observed arithmetic and variable behavior

| Expression | Result |
| --- | --- |
| `1+2*3` | `7` |
| `(1+2)*3` | `9` |
| `10/4` | `2.5` |
| `2^3` | `8` |
| `-2^2` | `-4` |
| `(-2)^2` | `4` |
| `.25+.5` | `.75` |
| `1E30*10` | `1E+31` |
| `1E-39` | `0` |
| `1E38*10` | `?OVERFLOW  ERROR` |
| `1/0` | `?DIVISION BY ZERO  ERROR` |

`ABC=1:ABD=2` makes both names read as 2. Only the first two characters are
significant; `%` and `$` distinguish integer/string types from float variables.
`SCORE=1` gives syntax error because its name contains the keyword OR. Variables
start at zero or empty string. `A%=5.9` stores 5, but `A%=-5.9` stores -6, not -5.
The latter was reproduced against the pristine pinned upstream executable; no
modern truncation rule was substituted. Tested integer endpoints are -32768 and
32767; assigning 32768 errors. RUN clears numeric/string/array values; CLR does the
same without discarding program lines.

Strings can be built to 255 bytes; concatenating a 256th byte raises STRING TOO
LONG. This differs from the **80-byte input line** bound. The modest storage test
reassigns 41 string-array elements 100 times, allocating more cumulative string
text than the 38,911-byte arena while retaining correct lengths and values. It is
not exhaustive GC verification or a heap high-water measurement.

`DIM A(10)` includes indices 0 through 10; an undeclared array implicitly includes
0 through 10. String/multidimensional arrays pass. In the otherwise empty arena,
`DIM A(7779)` and access to element 7779 pass; `DIM A(7780)` reports OUT OF MEMORY
and recovers. This is an observed capacity for that isolated numeric-array setup,
not a guarantee with programs/other values occupying memory. Negative index -1
produces ILLEGAL QUANTITY; a positive index above the declared bound gives BAD
SUBSCRIPT. Repeated DIM produces REDIM'D ARRAY.

## Control flow, data and functions

False IF skips the rest of its colon-separated line. True IF executes the THEN
statement or branches to the numeric target. Nested GOSUB/RETURN and FOR/NEXT pass.
A FOR whose initial value is already past its bound still executes its body once.
STEP 0 leaves the counter unchanged: the test exits after three iterations with
an explicit END rather than running forever. Mismatched NEXT and orphan NEXT
report NEXT WITHOUT FOR. Fractional ON selector 1.9 selects entry 1; zero or an
index beyond the list falls through; -1 and 256 give ILLEGAL QUANTITY.

READ handles numeric and quoted/unquoted text, including a quoted comma. Mixed-case
DATA text survives normalization and RESTORE resets the data pointer. DATA after
another colon statement works. Reading text DATA into a number reports SYNTAX at
the DATA line; reading beyond available items reports OUT OF DATA at the READ line.
DEF FN is tested in program mode: its parameter shadows the same-named variable
for the call, then restores the original value. Direct DEF FN errors.

String function checks include zero length, an overlong requested substring,
starting past the end, empty strings, invalid numeric text (`VAL("XYZ")` is zero),
and numeric prefixes (`VAL("12.5XYZ")` is 12.5). `ASC(CHR$(255))` is 255, proving
byte storage, **not** PETSCII display fidelity. CHR$(256), ASC("") and MID$ start 0
report ILLEGAL QUANTITY. Nontrivial trig/log/exp values pass 0.000001 tolerance;
INT(-1.2) is -2. Domain errors remain historical BASIC errors.

RND with a negative argument repeats the seeded sequence; positive arguments
advance the generator, with the tested result in [0,1). RND(0) requests the
adapter's IOBASE pseudo-timer data. The adapter initializes an LCG state to 1 per
application session and generates private RAM bytes; it does not read CIA hardware
or obtain real entropy. Tests do not establish statistical quality, security, or
compatibility with a physical C64's timing-dependent sequence.

## INPUT, GET and terminal differences

INPUT accepts numbers, strings and multiple variables, an explicit prompt,
quoted commas, and mixed case. Invalid numeric input gives `?REDO FROM START` and
repeats the prompt. Too few values produce `?? `; extras are discarded with
`?EXTRA IGNORED`. Empty input preserves an existing value (or its initial zero/empty
value after RUN). Stage C/E Ctrl+C during INPUT still reports BREAK and preserves
the stored program.

GET without a key supplies an empty string or numeric zero. The string test polls
without Enter, then consumes `a9!?` in order without duplication. Existing tests
retain Ctrl+C break and Ctrl+Q exit while GET waits. Input is normalized TabOS
ASCII, not the C64 keyboard matrix or PETSCII. Control keys remain reserved.

PRINT numeric values include trailing spaces and a leading space for nonnegative
values: `PRINT 1;2;3` emits ` 1  2  3 ` before newline. Semicolons concatenate;
a trailing semicolon suppresses the statement's newline. SPC(3) emits three spaces.
Stage H0 tracks the current logical column: `PRINT "A","B"` emits A, nine
spaces, B; `PRINT "A";TAB(5);"B"` emits A, four spaces, B. Backward TAB adds no
spaces. Wrapping follows the public TabOS TTY width, not a 40-column VIC-II screen.
Prompts, errors and editor echo share the tracker. POS has a byte-sized KERNAL
result, saturating columns above 255. Terminal resizing reinterprets the current
logical line offset at the new width; arbitrary external cursor movement and
full PETSCII controls are outside this adaptation.


STOP prints `BREAK IN <line>` and allows CONT; END returns silently to READY and
also allows CONT. Editing the stored program after STOP makes CONT report CAN'T
CONTINUE. CONT after NEW with an empty program silently returns to READY, reproduced
in pristine cbmbasic too; it was characterized rather than patched. Ctrl+C invokes
BASIC's break path and retains the program; CONT after every possible interruption
site is not established by this suite. Ctrl+Q exits the application, discards its
in-memory program and restores the existing shell. It is not BASIC END or STOP.

## Error matrix

Exact observed text retains the core's two spaces before `ERROR`; program errors
append ` IN <line>` where shown. Every tested error returns to READY.

| Trigger | Observed diagnostic | Evidence |
| --- | --- | --- |
| `LET 1=2` | `?SYNTAX  ERROR` | `errors.types` |
| `PRINT 1+"A"` | `?TYPE MISMATCH  ERROR` | `errors.types` |
| READ past DATA | `?OUT OF DATA  ERROR IN 20` | `data.exhausted` |
| `PRINT SQR(-1)` | `?ILLEGAL QUANTITY  ERROR` | `math.sqr-domain` |
| `PRINT 1/0` | `?DIVISION BY ZERO  ERROR` | `arithmetic.divide-zero` |
| `GOTO 999` | `?UNDEF'D STATEMENT  ERROR` | `goto.missing` |
| `RETURN` | `?RETURN WITHOUT GOSUB  ERROR` | `gosub.orphan` |
| `NEXT` | `?NEXT WITHOUT FOR  ERROR` | `for.orphan` |
| Index 11 in implicit A | `?BAD SUBSCRIPT  ERROR` | `arrays.implicit` |
| DIM same array twice | `?REDIM'D ARRAY  ERROR` | `arrays.errors` |
| Excess numeric array | `?OUT OF MEMORY  ERROR` | `arrays.maximum-empty-arena` |
| Oversized string | `?STRING TOO LONG  ERROR` | `strings.length` |
| Excess magnitude | `?OVERFLOW  ERROR` | `arithmetic.overflow` |
| Direct DEF FN | `?ILLEGAL DIRECT  ERROR` | `def-fn.direct` |
| Undefined FN | `?UNDEF'D FUNCTION  ERROR` | `def-fn.undefined` |
| CONT after line editing | `?CAN'T CONTINUE  ERROR` | `cont.edited` |

Safety-disabled SYS/USR/WAIT/channel commands first print `?UNSUPPORTED IN TABOS STAGE B`, then the
core's generic ILLEGAL QUANTITY error. Storage failures similarly prepend a useful
adapter diagnostic (for example FILE NOT FOUND or INVALID TOKENIZED PROGRAM).
Stage F retains this wording. The original core gates and dispatcher restrictions
remain unchanged; native/RV32 direct, expression, conditional and stored command
regressions exercise rejection and subsequent interpreter use. Tests are finite
coverage, not a formal proof for every possible input. PEEK/POKE reach only the
application-owned Stage I byte API; unsupported addresses and invalid numeric
ranges error. WAIT never reaches address polling; SYS/USR never reach machine or
indirect dispatch. VERIFY and general channels never enter the storage adapter.
Only bounded LOAD/SAVE access TabOS files.

## Storage and open limitations

SAVE writes bare names under `T:/basic/`. LOAD checks that personal path first and,
only on `ENOENT`, checks the exact name under `T:/data/basic/`. A malformed or
unreadable personal file never silently falls back. Explicit TabOS drive paths,
63-byte names, devices 1/8 as aliases, secondary address 0 and validated tokenized
$0801 programs with no relocation or trailing data remain supported. Roundtrip,
personal precedence, packaged fallback, byte recovery, overwrite, process relaunch,
malformed/missing LOAD preservation and fault-injected storage regressions pass.
See [usage and storage restrictions](basic.md).
This does not prove interoperability with external Commodore tools or IEC devices.

The following remain open: installed Tab5 firmware identity/configuration, device
stack/heap high-water measurements, serial watchdog logs, exact physical 80/81-byte
input-boundary checks, post-copy device executable hashing, and external Commodore
file interoperability. Stage F host tests resolve none of these. Graphics, sound,
full PETSCII and general device/channel emulation remain outside this stage.

## Historical references

Expected semantics were compared with Commodore's *C64 Programmer's Reference
Guide*, using its [variable rules](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_1/page_007.html),
[array rules](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_1/page_008.html),
[PRINT description](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_2/page_070.html),
[CONT description](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_2/page_041.html),
[RUN description](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_2/page_081.html),
and [error descriptions](https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Appendices/page_400.html).
These are transcriptions of primary documentation, not evidence that this port
passes. Executed tests establish the port's observations. Negative integer
assignment and empty-program CONT were also reproduced with the pristine pinned
upstream cbmbasic executable; the desktop runtime was not imported into TabOS.


## Native TabBASIC extensions

These are native additions, not compatibility claims for C64 hardware commands.
See [syntax and limits](basic-graphics.md). Physical evidence is described
separately from the automated matrix.

| Feature | Classification | Evidence / adaptation |
| --- | --- | --- |
| GRAPHICS/TEXT | PARTIAL (native adaptation) | SDK canvas/ownership, errors and repeated text/shell restoration. |
| CLS/COLOR/PSET/LINE/RECT/CIRCLE | PARTIAL (native adaptation) | Deterministic pixel assertions, clipping and bounds; explicit PRESENT. |
| SPRITE/SPRITEROW/SPRITEPOS/SPRITESHOW/SPRITEHIDE | PARTIAL (native adaptation) | 16 software sprites, bounded row encoding, transparency and background restoration. |
| GTEXT | PARTIAL | Small uppercase/digit font; not PETSCII text graphics. |
| KEY | PARTIAL | Public held key events, −1/0; GET queue remains separate. |
| SLEEP | PARTIAL (native adaptation) | Cooperative 0–5000 ms delay, break/exit recovery. |
| SOUND | PARTIAL (native adaptation) | Bounded synchronous mono PCM tone, independent of virtual SID state. |
| SID `$D400-$D418` | PARTIAL (Tier 3A) | Three voices, triangle/saw/pulse/noise, gate, approximate ADSR and volume through public PCM; no filter or chip fidelity. |
| SID program LIST/SAVE/LOAD | PASS (TabBASIC format) | Register POKEs roundtrip as ordinary BASIC tokens; virtual device state itself is session-local. |
| Native token LIST/SAVE/LOAD/RUN | PASS (TabBASIC format) | CC–DD tokens roundtrip; examples generated by production interpreter. Not C64 interchange. |
| BASIC Pong | PASS (native adaptation) | Source/token roundtrip, deterministic gameplay, and Stage H/I physical launch/input/break/text/exit checks. Physical motion remains slow. |
