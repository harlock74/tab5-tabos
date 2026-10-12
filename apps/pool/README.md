# Pool

Standalone TabOS house 8-ball for a computer opponent or two local players, with
turns, solids/stripes, fouls, ball in hand and win/loss outcomes. Title/help/result
panels, potted trays, cue/pocket feedback and optional speaker sound are included. Read the [rules](../../docs/pool-rules.md). Use `pool --two-player` for local shared-keyboard play.

Run `pool` from the TabOS shell. Choose the mode with Left/Right and press Enter.
During play, Left/Right or A/D aim; Up/Down or W/S set
power; Aa/Shift gives fine aim; **Space/K shoots**. After a foul, move the cue
preview with arrows/WASD and press **Enter** to confirm a gold valid position.
The computer handles its own shots and placement.
R asks for a fresh rack; Enter confirms, R/Escape cancels. Enter also starts a
new rack after a result. P pauses; H opens help and freezes play; M toggles sound.
Q exits. Escape closes help or confirmation first, otherwise exits. Launch with
`pool --profile` to print aggregate simulation, display-wait, rendering and frame-submission timing
after quitting; it can be combined with `--two-player`.

Build from the repository root with `./apps/build.sh`, or with the RISC-V
toolchain active, `make -C apps/pool`. See [the guide](../../docs/pool.md) for
geometry, validation, controls and the staged development plan.

`table.c` owns shared table geometry and rack positions; `render.c` draws the
logical canvas as a pure job suitable for TabOS's optional bounded compute worker;
`game.c` owns shot state/math; `physics.c` integrates motion and contacts;
`rules.c` resolves match outcomes; `ai.c` selects bounded direct shots; `clock.c` bounds fixed ticks; `input.c` handles public key events; `main.c`
owns scheduling, waits and graphics lifetime. `sound.c` consumes observed events
through optional bounded SDK audio. Precomputed PCM and a prepared stream keep
device startup/shutdown off the shot path; pause/help/mute/exit release audio.
From the repository root, check the original samples with
`python3 apps/pool/tools/generate_sound.py --check`; omit `--check` to regenerate.
All visuals are original procedural drawing except the MIT-licensed Starfall
5×7 HUD font reused through Snake. Its notice is preserved in `LICENSE`.

Installation includes the MIT notice at `T:/share/licenses/pool/LICENSE`.
See the [acceptance report](../../docs/pool-acceptance.md) for build/test evidence,
hardware feedback, known limitations and the proposed PR.
