# TabBASIC safe C64 graphics profile

Stage I adds a bounded compatibility profile for pure BASIC listings that use
selected C64 screen, colour and VIC-II sprite PEEK/POKE addresses. It is not a C64
emulator: there is no 6502, ROM, machine-code execution, raster timing, CIA,
interrupt, bank-switching or native-memory access. Selected Stage J sound uses the
separate bounded synthesizer documented in `basic-c64-sid.md`; it is not part of
the graphics renderer.

## Try the supplied examples

The self-authored tokenized examples are installed under `T:/data/basic/`:

```basic
LOAD "C64SCREEN.prg"
RUN
```

The [example catalogue](basic-examples.md) is the authoritative list of graphics,
game and diagnostic programs.
The Stage K `C64DODGE.prg` example is a playable, self-authored combined
screen/sprite/SID game with explicit `KEY()` controls. With current storage rules,
packaged examples load by bare filename when no personal file with that name exists.
`C64ANIM` runs until Ctrl+C. After a finite example leaves the compatibility
display visible, type `TEXT` and Enter to restore the terminal. Ctrl+Q exits BASIC
and also restores the terminal.

## Virtual memory and safety boundary

`basic_c64.c` owns all compatibility state for one BASIC application session. The
generated interpreter keeps its private 64 KiB array, but PEEK/POKE never read or
write that array. BASIC's existing expression parser first rejects negative or
out-of-range addresses and values. The final byte operation then calls a narrow
`uint16_t`/`uint8_t` virtual-device function.

The fixed profile owns:

| Region | Size | Behavior |
| --- | ---: | --- |
| `$0000-$3FFF` | 16,384 bytes | Separate fixed VIC bank, initialized to zero. |
| `$0400-$07E7` | within bank | 40×25 screen codes; initialized to spaces. |
| `$07F8-$07FF` | within bank | Sprite pointers 0–7. |
| `$D800-$DBE7` | 1,000 bytes | Colour RAM; only the low nibble is stored/read. |
| Selected `$D000-$D02E` | 47-byte address window, sparse | Explicit registers below; every other address is rejected. |

Accepting `$0000-$3FFF` does not expose zero page, BASIC program memory or native
memory. It is an independent byte array. This fixed bank makes sprite pointer `P`
select virtual bytes `P*64` through `P*64+62`; byte 63 is conventional padding.
No VIC bank register is implemented.

Implemented registers are:

- `$D000-$D00F`: eight low X/Y coordinate pairs;
- `$D010`: X-coordinate high bits;
- `$D015`: sprite enable bits;
- `$D017` and `$D01D`: Y/X expansion bits;
- `$D01C`: sprite multicolour bits;
- `$D020` and `$D021`: border and background colour;
- `$D025` and `$D026`: shared sprite multicolours;
- `$D027-$D02E`: individual sprite colours.

PEEK returns the stored virtual byte for implemented locations. Unsupported
addresses print `?UNSUPPORTED C64 ADDRESS $xxxx` and then use BASIC's existing
ILLEGAL QUANTITY error recovery. POKE does the same without a partial write.
`POKE -1,1`, `POKE 70000,1`, and byte values outside 0–255 are rejected by the
interpreter before this boundary. No BASIC number is cast to a pointer.

SYS, USR and WAIT remain disabled. Virtual bytes cannot be executed, dispatched as
generated-core labels, interpreted as function pointers or passed to TabOS as
addresses. SID `$D400-$D418`, CIA `$DC00/$DD00`, raster/control registers,
collisions, character-set switching and unknown hardware are rejected.

## Renderer and lifecycle

The first supported POKE automatically opens a compatibility canvas, so ordinary
listings need no new command. PEEK alone does not enter graphics. The renderer uses
the public TabOS graphics SDK and never touches the physical framebuffer.

The logical canvas is 320×200 RGB565, the same allocation size as native TabBASIC
graphics. A compact 280×175 character field begins at (20,12); each of the 40×25
cells is 7×7 pixels. The remaining area shows the virtual border colour. This is a
readable compatibility layout rather than VIC-II pixel geometry.

Glyphs use the project-authored synthetic 5×7 uppercase/digit/punctuation set
introduced for Stage H. No C64 character-ROM data is embedded. Screen codes 1–26
and 65–90 map to uppercase letters, 48–57 map to digits, space is blank, and other
codes use a question-mark fallback. Bit 7 reverses foreground/background. PETSCII
graphics and shifted character sets are unsupported.

The separate compatibility palette uses these logical RGB values:

| Index | RGB | Index | RGB |
| ---: | --- | ---: | --- |
| 0 | `0,0,0` | 8 | `221,136,85` |
| 1 | `255,255,255` | 9 | `102,68,0` |
| 2 | `136,0,0` | 10 | `255,119,119` |
| 3 | `170,255,238` | 11 | `51,51,51` |
| 4 | `204,68,204` | 12 | `119,119,119` |
| 5 | `0,204,85` | 13 | `170,255,102` |
| 6 | `0,0,170` | 14 | `0,136,255` |
| 7 | `238,238,119` | 15 | `187,187,187` |

Writes mark only the affected display state dirty. Screen and colour writes track
individual cells. Sprite changes retain the union of the old and new bounds, restore
the underlying cells, and redraw intersecting sprites in the established priority.
Border changes repaint the border; background changes repaint the character field.
The first frame remains a full redraw.

Broad updates present after 128 visible writes or about 16 ms. Repeated writes to
one cell use an eight-write batch, while sprite movement uses four writes. This
keeps full-screen initialization coalesced while giving moving sprites enough
positions to avoid large jumps. The final dirty state is forced before BASIC waits
for another command. Rendering polls normalized input in bounded chunks, so Ctrl+C
and Ctrl+Q remain responsive.

`GRAPHICS` closes the compatibility canvas before opening native graphics. A later
supported POKE closes native graphics and opens the compatibility renderer. `TEXT`,
Ctrl+C, BASIC errors and Ctrl+Q close either renderer. Virtual bytes persist across
NEW, CLR, RUN, TEXT and renderer handoffs, as C64 hardware memory would; they reset
to documented defaults when the BASIC process starts again. SAVE/LOAD persists the
BASIC program, not virtual display state.

Host measurements from the Stage I acceptance run are observations, not timing
guarantees:

| Workload | Native Debug | Native Release | RV32 Debug | RV32 Release |
| --- | ---: | ---: | ---: | ---: |
| 1,000 screen writes | 48.7 ms | 11.8 ms | 308 ms | 159 ms |
| 1,000 colour writes | 56.6 ms | 13.2 ms | 479 ms | 221 ms |
| 1,000 background writes | 52.0 ms | 11.4 ms | 432 ms | 199 ms |
| 1,000 sprite-X writes | 53.3 ms | 11.5 ms | 441 ms | 204 ms |
| 1,000 paired screen+colour writes | 102.9 ms | 24.1 ms | 800 ms | 385 ms |
| One forced full redraw | 3.9 ms | 0.4 ms | 25 ms | 16 ms |

The RV32 figures use the real SDK-built application under the host interpreter.
On the physical Tab5, the screen, colour and static-sprite examples rendered
correctly, but the moving-sprite example was visibly stuttering. No device FPS was
measured.

Stage J0 added opt-in instrumentation at commit `ebf1047` and measured the unchanged
Stage I refresh model on Tab5 before optimization. Launch `basic --c64-profile`;
the next `TEXT`, Ctrl+C or Ctrl+Q prints short lines containing counters for
writes, redraws, presents, skipped/coalesced service checks and timing totals. The
packaged `J0SPRITE`, `J0BORDER`, `J0CELL` and `J0FULL` programs are fixed workloads
for before/after comparison. Ordinary `basic` sessions do not collect or print a
report.

The physical Stage I baseline measurements were:

| Workload | Writes | Presents | Elapsed | Render | Present | Observed |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Four sprite crossings | 1,112 | 24 | 1,309 ms | 109 ms | 501 ms | Visibly stuttering, almost unusable |
| Border/background | 512 | 11 | 726 ms | 50 ms | 229 ms | Correct, rapid flashing |
| Repeated single cell | 417 | 9 | 644 ms | 40 ms | 203 ms | Too fast to follow individual values |
| Full screen/colour fill | 2,000 | 47 | 2,761 ms | 504 ms | 1,048 ms | Correct coloured letters |

The baseline sprite showed only about six positions per crossing. Present averaged
21–23 ms, while each frame also rebuilt all 1,000 cells. The measured bottleneck is
therefore the combination of full-screen redraw, full-canvas SDK PRESENT and a
time threshold that fired after roughly 43–47 POKEs. The optimized RV32 host path
reduces incremental redraw work while retaining public-SDK PRESENT:

| Workload | Stage I Debug → J0 | Stage I Release → J0 |
| --- | ---: | ---: |
| 1,000 screen writes | 308 → 177 ms | 159 → 93 ms |
| 1,000 colour writes | 479 → 201 ms | 221 → 114 ms |
| 1,000 background writes | 432 → 402 ms | 203 → 192 ms |
| 1,000 sprite-X writes | 441 → 152 ms | 211 → 87 ms |
| 2,000 paired screen/colour writes | 800 → 350 ms | 385 → 178 ms |
| Incremental redraw | 25 → 2 ms | 16 → 1 ms |

Background changes remain broad by definition, and PRESENT still scales the whole
logical canvas. The optimized physical measurements were:

| Workload | Presents/redraws | Elapsed | Render | Present | Full/partial | Observed |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Four sprite crossings | 278 | 8,299 ms | 96 ms | 6,715 ms | 1/277 | Mostly smooth |
| Border/background | 11 | 761 ms | not recorded | 253 ms | 1/10 | Correct rapid border and centre-colour changes |
| Repeated single cell | 53 | 1,785 ms | not recorded | 1,261 ms | 1/52 | Correct fixed-colour A-Z changes |
| Full screen/colour fill | 50 | 1,872 ms | not recorded | 766 ms | 1/49 | Correct coloured letters |

The moving-sprite workload now exposes 278 positions rather than 24 and changed
from almost unusable stutter to mostly smooth motion. Its longer elapsed time is a
deliberate consequence of presenting the extra positions; 6,715 ms of the 8,299 ms
was spent in the public SDK PRESENT path, while compatibility rendering used only
96 ms. Maximum measured physical PRESENT time was 29 ms. Broad border/background
and full-screen workloads remain close to their prior presentation behavior.

A clean-process `C64ANIM` rerun also remained mostly smooth. The tester observed
the vertical yellow sprite appearing to build from top to bottom for the first
one or two seconds and an occasional brief box near the left fifth of the screen.
Both observations reproduced after a clean BASIC restart. Deterministic backing-
canvas tests show correct old-region restoration, clipping and overlap priority;
there was no freeze, reset or persistent trail. They are retained as physical
full-canvas presentation limitations rather than attributed to C64 register-state
corruption. J0 therefore improves animation substantially but does not claim a
tear-free or fixed-rate display.

The post-optimization physical regression also passed native GRAPHICS sprite
movement, native Pong, TEXT restoration and Ctrl+Q shell restoration. Pong retained
its previously documented slow speed. Its physical B event ended the program, then
the corresponding normalized lowercase `b` text event appeared after `READY.`;
this retained input-queue artifact is unrelated to C64 rendering.

## Sprites

Eight logical sprites use the standard 24×21 bitmap shape, three bytes per row,
most-significant bit first. Zero bits are transparent. Sprite 0 has highest visual
priority among the eight rendered sprites. Coordinates are direct compatibility
canvas coordinates and are clipped safely; they do not reproduce the VIC-II's
border-coordinate offsets.

Monochrome, individual colour, X/Y expansion and transparency are supported.
Multicolour is also supported: bit pairs `00`, `01`, `10`, `11` select transparent,
`$D025`, the individual colour, and `$D026`, with the conventional double-width
pair. Collision registers, background priority, DMA timing and raster behavior are
not implemented.

## Compatibility tiers

- **Tier 1:** existing pure BASIC/text programs, subject to the documented
  ASCII/terminal adaptations.
- **Tier 2A:** tested screen RAM, colour RAM, border/background and basic
  position/enable/colour/pointer sprite listings. Stage I supports this subset.
- **Tier 2B:** multicolour sprites are supported; collisions, character switching,
  banking, raster state and VIC timing remain unsupported.
- **Tier 3A:** selected SID POKE tones are supported by the separate Stage J
  compatibility synthesizer documented in `basic-c64-sid.md`.
- **Tier 3B:** CIA, raster and timing-dependent programs remain unsupported.
- **Tier 4:** SYS/USR or embedded machine-code programs remain unsupported.

These tiers describe features, not a percentage of C64 software compatibility.

## Historical sample

The well-known one-line `10 PRINT CHR$(205.5+RND(1));:GOTO 10` listing was tried
unchanged from [TIME's published account](https://time.com/69316/basic/). It has no
SYS/USR or machine-code dependency. The language loop runs and Ctrl+C breaks it,
but TabBASIC's ASCII/CP437 terminal path does not render C64 PETSCII codes 205/206
as diagonal maze glyphs. Its visual result is therefore **incompatible**, for the
documented PETSCII reason. It is not imported into the repository because the
article does not provide a redistribution license for a corpus asset.
