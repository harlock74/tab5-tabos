#include <pool/render.h>
#include <pool/input.h>
#include <pool/clock.h>
#include <pool/ai.h>
#include <pool/sound.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <tabos/runtime_time.h>
#include <tabos/compute.h>
#include <tabos/filesystem.h>

#include <stdio.h>
#include <sys/ioctl.h>
#include <tabos/input.h>
#include <tabos/tty.h>
#include <tabos/wait.h>
#include <unistd.h>

static bool active(const pool_game_t* game, const pool_input_t* controls)
{
    return !game->title && !game->help && !game->paused && !game->restart_pending && !game->rules.complete &&
           (game->shot_set || pool_input_active(controls, game) || pool_game_computer_turn(game) ||
            pool_game_animating(game));
}

static bool sound_enabled(const pool_game_t* game)
{
    return !game->muted && !game->title && !game->help && !game->paused && !game->restart_pending;
}

enum { POOL_FRAME_COUNT = 2 };

int main(int argc, char** argv)
{
    bool two_player = false;
    bool profiling  = false;
    for (int argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--two-player") == 0 && !two_player) {
            two_player = true;
        } else if (strcmp(argv[argument], "--profile") == 0 && !profiling) {
            profiling = true;
        } else {
            fprintf(stderr, "usage: pool [--two-player] [--profile]\n");
            return 1;
        }
    }
    uint64_t active_ms = 0U, frames = 0U, ticks_total = 0U;
    uint64_t simulation_ms = 0U, wait_ms = 0U, render_ms = 0U, submit_ms = 0U;
    uint64_t max_simulation_ms = 0U, max_wait_ms = 0U, max_render_ms = 0U, max_submit_ms = 0U;
    uint32_t tty = 0U;
    if (ioctl(STDIN_FILENO, TABOS_TTY_GET_MODE, &tty) != 0 ||
        ioctl(STDIN_FILENO, TABOS_TTY_SET_MODE, tty | (uint32_t) TABOS_TTY_MODE_RAW_INPUT) != 0) {
        fprintf(stderr, "pool: raw keyboard unavailable\n");
        return 1;
    }
    tabos_graphics_t graphics = {.width = POOL_CANVAS_WIDTH, .height = POOL_CANVAS_HEIGHT};
    if (tabos_graphics_open(&graphics) != 0) {
        fprintf(stderr, "pool: graphics unavailable\n");
        return 1;
    }
    tabos_color_t* frame_buffers[POOL_FRAME_COUNT] = {graphics.pixels};
    const size_t frame_pixels                      = (size_t) graphics.width * graphics.height;
    for (unsigned int frame = 1U; frame < POOL_FRAME_COUNT; ++frame) {
        frame_buffers[frame] = calloc(frame_pixels, sizeof(*frame_buffers[frame]));
        if (frame_buffers[frame] == NULL) {
            for (unsigned int allocated = 1U; allocated < frame; ++allocated) {
                free(frame_buffers[allocated]);
            }
            (void) tabos_graphics_close(&graphics);
            fprintf(stderr, "pool: frame buffers unavailable\n");
            return 1;
        }
    }
    pool_render_job_t* render_jobs = calloc(POOL_FRAME_COUNT, sizeof(*render_jobs));
    if (render_jobs == NULL) {
        for (unsigned int frame = 1U; frame < POOL_FRAME_COUNT; ++frame) {
            free(frame_buffers[frame]);
        }
        (void) tabos_graphics_close(&graphics);
        fprintf(stderr, "pool: render jobs unavailable\n");
        return 1;
    }
    tabos_wait_item_t input = {.source = tabos_input_wait_source(), .events = TABOS_WAIT_READABLE};
    int status              = input.source == TABOS_WAIT_SOURCE_INVALID ? 1 : 0;
    pool_game_t game;
    pool_game_reset(&game);
    game.computer      = !two_player;
    game.title         = true;
    pool_sound_t sound = {.stream = TABOS_AUDIO_STREAM_INVALID};
    pool_ai_t ai;
    pool_ai_reset(&ai, UINT32_C(0x504f4f4c));
    pool_input_t controls = {0};
    bool dirty            = true;
    unsigned int frame_index    = 0U;
    unsigned int frames_pending = 0U;
    uint64_t previous     = tabos_monotonic_ms();
    pool_clock_t clock    = {0};
    while (!controls.quit && status == 0) {
        pool_sound_effect_t effect = POOL_SOUND_NONE;
        const bool was_active      = active(&game, &controls);
        bool transition            = false;
        tabos_input_event_t event;
        while (tabos_input_poll(&event)) {
            const pool_game_t before = game;
            const bool had_shot      = game.shot_set;
            const bool had_placement = game.placement;
            const bool had_restart   = game.restart_pending;
            const bool had_result    = game.rules.complete;
            const bool changed       = pool_input_event(&controls, &game, &event);
            if (changed &&
                (event.key == TABOS_KEY_P || event.key == TABOS_KEY_R || had_shot != game.shot_set ||
                 had_placement != game.placement || had_restart != game.restart_pending ||
                 had_result != game.rules.complete || before.title != game.title || before.help != game.help)) {
                transition = true;
            }
            if (changed && event.key == TABOS_KEY_ENTER && (had_restart || had_result || before.title)) {
                pool_ai_reset(&ai, UINT32_C(0x504f4f4c));
            }
            const pool_sound_effect_t next = pool_sound_event(&before, &game);
            if (next > effect) {
                effect = next;
            }
            if (changed && (event.key == TABOS_KEY_M || event.key == TABOS_KEY_R || event.key == TABOS_KEY_P ||
                            before.help != game.help || before.title != game.title || had_restart || had_result)) {
                pool_sound_close(&sound);
                if (sound_enabled(&game)) {
                    pool_sound_prepare(&sound);
                }
                effect = POOL_SOUND_NONE;
            }
            dirty = changed || dirty;
            if (controls.quit) {
                break;
            }
        }
        if (controls.quit) {
            break;
        }
        const uint64_t now     = tabos_monotonic_ms();
        const uint64_t elapsed = now - previous;
        previous               = now;
        const bool is_active   = active(&game, &controls);
        if (profiling && is_active && was_active && !transition) {
            active_ms += elapsed;
        }
        pool_game_t previous_motion;
        bool interpolate_motion  = false;
        unsigned int frame_ticks = 0U;
        if (is_active && was_active && !transition) {
            frame_ticks               = pool_clock_advance(&clock, elapsed);
            uint64_t simulation_start = 0U;
            if (profiling) {
                simulation_start = tabos_monotonic_ms();
            }
            for (unsigned int i = 0U; i < frame_ticks; ++i) {
                const pool_game_t before = game;
                dirty                    = pool_game_effects_step(&game) || dirty;
                dirty                    = pool_input_step(&controls, &game) || dirty;
                if (game.shot_set) {
                    pool_ai_reset(&ai, ai.seed);
                    const bool changed = pool_game_step(&game);
                    if (changed) {
                        previous_motion    = before;
                        interpolate_motion = true;
                    }
                    dirty = changed || dirty;
                } else if (pool_game_computer_turn(&game)) {
                    controls.blocked = controls.held;
                    dirty            = pool_ai_step(&ai, &game) || dirty;
                }
                const pool_sound_effect_t next = pool_sound_event(&before, &game);
                if (next > effect) {
                    effect = next;
                }
            }
            if (profiling) {
                const uint64_t duration  = tabos_monotonic_ms() - simulation_start;
                ticks_total             += frame_ticks;
                simulation_ms           += duration;
                if (duration > max_simulation_ms) {
                    max_simulation_ms = duration;
                }
            }
        } else {
            /* Never apply idle/pre-transition time to new controls or motion. */
            clock.accumulator = 0U;
        }
        if (sound_enabled(&game)) {
            pool_sound_play(&sound, effect);
        }
        if (dirty) {
            const pool_game_t* motion = interpolate_motion ? &previous_motion : NULL;
            uint64_t wait_start       = 0U;
            if (profiling) {
                wait_start = tabos_monotonic_ms();
            }
            if (frames_pending == POOL_FRAME_COUNT && tabos_graphics_wait(&graphics) != 0) {
                status = 1;
                break;
            }
            if (frames_pending == POOL_FRAME_COUNT) {
                frames_pending = 0U;
            }
            const uint64_t wait_duration = profiling ? tabos_monotonic_ms() - wait_start : 0U;
            uint64_t render_start        = 0U;
            if (profiling) {
                render_start = tabos_monotonic_ms();
            }
            pool_render_job_t* render_job = &render_jobs[frame_index];
            pool_render_prepare(render_job, graphics.pixels, &game, motion, clock.accumulator);
            int render_status = tabos_compute_submit(pool_render_execute, render_job, sizeof(*render_job));
            if (render_status == -TABOS_ENOTSUP) {
                pool_render_execute(render_job);
                render_status = 0;
            } else if (render_status == 0) {
                render_status = tabos_compute_wait();
            }
            if (render_status != 0 || render_job->result != 0) {
                status = 1;
                break;
            }
            const uint64_t render_duration = profiling ? tabos_monotonic_ms() - render_start : 0U;
            uint64_t submit_start          = 0U;
            if (profiling) {
                submit_start = tabos_monotonic_ms();
            }
            if (tabos_graphics_submit_borrowed(&graphics) != 0) {
                status = 1;
                break;
            }
            ++frames_pending;
            frame_index     = (frame_index + 1U) % POOL_FRAME_COUNT;
            graphics.pixels = frame_buffers[frame_index];
            const uint64_t submit_duration = profiling ? tabos_monotonic_ms() - submit_start : 0U;
            if (profiling && is_active) {
                ++frames;
                wait_ms    += wait_duration;
                render_ms  += render_duration;
                submit_ms  += submit_duration;
                if (wait_duration > max_wait_ms) {
                    max_wait_ms = wait_duration;
                }
                if (render_duration > max_render_ms) {
                    max_render_ms = render_duration;
                }
                if (submit_duration > max_submit_ms) {
                    max_submit_ms = submit_duration;
                }
            }
        }
        dirty            = false;
        uint32_t timeout = TABOS_WAIT_TIMEOUT_INFINITE;
        if (active(&game, &controls)) {
            timeout = pool_clock_wait(&clock, tabos_monotonic_ms() - previous);
        }
        if (tabos_wait(&input, 1U, timeout) < 0) {
            status = 1;
        }
    }
    pool_sound_close(&sound);
    if (sound.error != 0) {
        fprintf(stderr, "pool: audio unavailable: %s\n", strerror(sound.error));
    }
    if (frames_pending != 0U && tabos_graphics_wait(&graphics) != 0) {
        status = 1;
    }
    graphics.pixels = frame_buffers[0];
    if (tabos_graphics_close(&graphics) != 0) {
        status = 1;
    }
    for (unsigned int frame = 1U; frame < POOL_FRAME_COUNT; ++frame) {
        free(frame_buffers[frame]);
    }
    free(render_jobs);
    if (status != 0) {
        fprintf(stderr, "pool: graphics or input service failed\n");
    }
    if (profiling) {
        printf("Pool profile (active play and animation):\n"
               "  elapsed: %" PRIu64 " ms; frames: %" PRIu64 "; ticks: %" PRIu64 "\n"
               "  simulation: total %" PRIu64 " ms; worst %" PRIu64 " ms\n"
               "  wait: total %" PRIu64 " ms; worst %" PRIu64 " ms\n"
               "  render: total %" PRIu64 " ms; worst %" PRIu64 " ms\n"
               "  submit: total %" PRIu64 " ms; worst %" PRIu64 " ms\n"
               "  catch-up discarded: %" PRIu64 " ms\n",
               active_ms, frames, ticks_total, simulation_ms, max_simulation_ms, wait_ms, max_wait_ms, render_ms,
               max_render_ms, submit_ms, max_submit_ms, clock.discarded_ms);
    }
    return status;
}
