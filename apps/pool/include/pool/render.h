#ifndef POOL_RENDER_H
#define POOL_RENDER_H

#include <tabos/graphics.h>
#include <pool/game.h>

enum {
    POOL_CANVAS_WIDTH  = 639,
    POOL_CANVAS_HEIGHT = 360,
    POOL_TABLE_X       = 56,
    POOL_TABLE_Y       = 48
};

int pool_render(tabos_graphics_t* graphics, const pool_game_t* game);
int pool_render_draw(tabos_graphics_t* graphics, const pool_game_t* game, const pool_game_t* previous,
                     uint32_t interpolation);

#endif
