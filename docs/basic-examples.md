# TabBASIC example catalogue

This is the authoritative catalogue of programs installed under
`T:/data/basic/`. At the shell, use `ls T:/data/basic`. Inside BASIC, load an exact
filename such as:

```basic
LOAD "GRAPHICS.prg"
RUN
```

A personal file under `T:/basic/` with the same bare name takes precedence. Use an
explicit path such as `LOAD "T:/data/basic/GRAPHICS.prg"` when you specifically
want the installed copy. SAVE always writes personal storage.

| Filename | Type | Purpose | Controls | Graphics | Sound | Provenance |
| --- | --- | --- | --- | --- | --- | --- |
| `AMAZING.prg` | Historical text BASIC | Generate a text maze | Enter width and length | Terminal text | None | Jack Hauber; unchanged Unlicense listing from *BASIC Computer Games* |
| `GRAPHICS.prg` | Native demonstration | Drawing commands and movable sprite | Arrows; Space exits | Native canvas and sprite | None | Project-authored |
| `PONG.prg` | Native game | Playable one-player Pong | Up/Down or A; Space restarts; B exits | Native canvas and sprite | None | Project-authored |
| `C64SCREEN.prg` | Compatibility demonstration | Screen and colour RAM | Ctrl+C if needed; TEXT restores terminal | C64-visible screen/colour POKEs | None | Project-authored |
| `C64COLORS.prg` | Compatibility demonstration | Border, background and cell colours | TEXT restores terminal | C64-visible POKEs | None | Project-authored |
| `C64SPRITE.prg` | Compatibility demonstration | Define and show one sprite | TEXT restores terminal | C64-visible sprite bytes/registers | None | Project-authored |
| `C64ANIM.prg` | Compatibility demonstration | Continuous sprite movement | Ctrl+C; then TEXT | C64-visible sprite POKEs | None | Project-authored |
| `C64DODGE.prg` | Compatibility game | Playable falling-object dodge game | Left/Right; B exits | Screen/colour and one sprite | One selected SID voice | Project-authored; not historical C64 software |
| `SIDTONE.prg` | Compatibility demonstration | One sawtooth tone | Runs to completion | None | Selected SID POKEs | Project-authored |
| `SIDSCALE.prg` | Compatibility demonstration | Eight-note scale | Runs to completion | None | Selected SID POKEs | Project-authored |
| `SIDENV.prg` | Compatibility demonstration | Attack and release | Runs to completion | None | Selected SID POKEs | Project-authored |
| `SID3VOICE.prg` | Compatibility demonstration | Three simultaneous voices | Runs to completion | None | Selected SID POKEs | Project-authored |
| `SIDSPRITE.prg` | Compatibility demonstration | Continuous sprite plus tone | Ctrl+C; then TEXT | C64-visible sprite POKEs | Selected SID POKEs | Project-authored |
| `J0SPRITE.prg` | Diagnostic benchmark | Four sprite crossings | Run with `basic --c64-profile` | Compatibility sprite | None | Project-authored Stage J0 diagnostic |
| `J0BORDER.prg` | Diagnostic benchmark | Repeated border/background updates | Run with `basic --c64-profile` | Compatibility colours | None | Project-authored Stage J0 diagnostic |
| `J0CELL.prg` | Diagnostic benchmark | Repeated single-cell updates | Run with `basic --c64-profile` | Compatibility screen cell | None | Project-authored Stage J0 diagnostic |
| `J0FULL.prg` | Diagnostic benchmark | Full screen and colour writes | Run with `basic --c64-profile` | Compatibility screen/colour | None | Project-authored Stage J0 diagnostic |

`AMAZING` is the only redistributed third-party program. Its exact provenance is
recorded in `apps/basic/examples/THIRD_PARTY.md`, and its Unlicense text is
installed under `T:/share/licenses/basic/`. Every other packaged example is
project-authored. Historical listings with unclear redistribution terms and the
audited MIT machine-code shoot-'em-up discussed in the
[program corpus](basic-c64-programs.md) are not packaged.

New programs should normally use the native interface described in the
[game-development tutorial](basic-game-development.md). Compatibility examples
retain POKE-driven behavior to test the bounded historical interface.
