#ifndef POOL_RENDER_H
#define POOL_RENDER_H

#include <tabos/graphics.h>
#include <pool/game.h>

enum {
    POOL_LAYOUT_WIDTH  = 640,
    POOL_LAYOUT_HEIGHT = 360,
    POOL_CANVAS_WIDTH  = POOL_LAYOUT_WIDTH,
    POOL_CANVAS_HEIGHT = POOL_LAYOUT_HEIGHT,
    POOL_TABLE_X       = 56,
    POOL_TABLE_Y       = 48
};

typedef struct {
        tabos_color_t* pixels;
        pool_game_t game;
        pool_game_t previous;
        uint32_t interpolation;
        bool has_previous;
        int result;
} pool_render_job_t;

void pool_render_prepare(pool_render_job_t* job, tabos_color_t* pixels, const pool_game_t* game,
                         const pool_game_t* previous, uint32_t interpolation);
void pool_render_execute(void* data);
int pool_render(tabos_graphics_t* graphics, const pool_game_t* game);
int pool_render_draw(tabos_graphics_t* graphics, const pool_game_t* game, const pool_game_t* previous,
                     uint32_t interpolation);

#endif
