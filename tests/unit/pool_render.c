#include <pool/render.h>
#include <pool/table.h>
#include <tabos/internal/elf_api.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int presents;
static int present_result;

static int open_graphics(uint32_t* width, uint32_t* height)
{
    *width  = 1280U;
    *height = 720U;
    return 0;
}

static int close_graphics(void)
{
    return 0;
}

static int present(void)
{
    ++presents;
    return present_result;
}

static int blit(const tabos_graphics_blit_options_t* options)
{
    assert(options->bitmap_width == 640U && options->bitmap_height == 360U);
    assert(options->destination.x == 0 && options->destination.y == 0);
    assert(options->destination.width == 1280U && options->destination.height == 720U);
    assert(options->pixels != NULL);
    return 0;
}

static const tabos_elf_api_t api = {
    .abi_version      = TABOS_ELF_API_VERSION,
    .graphics_open    = open_graphics,
    .graphics_close   = close_graphics,
    .graphics_present = present,
    .graphics_blit_ex = blit,
};
const tabos_elf_api_t* tabos_runtime_api = &api;

static tabos_color_t pixel(const tabos_graphics_t* graphics, int x, int y)
{
    assert(x >= 0 && x < POOL_CANVAS_WIDTH && y >= 0 && y < POOL_CANVAS_HEIGHT);
    return graphics->pixels[(size_t) y * graphics->width + (size_t) x];
}

static void capture(const tabos_graphics_t* graphics, const char* suffix)
{
    const char* prefix = getenv("POOL_CAPTURE_PREFIX");
    if (prefix == NULL) {
        return;
    }
    char path[512];
    (void) snprintf(path, sizeof(path), "%s-%s.ppm", prefix, suffix);
    FILE* file = fopen(path, "wb");
    assert(file != NULL);
    fprintf(file, "P6\n%u %u\n255\n", POOL_CANVAS_WIDTH, POOL_CANVAS_HEIGHT);
    for (unsigned int i = 0U; i < POOL_CANVAS_WIDTH * POOL_CANVAS_HEIGHT; ++i) {
        const uint16_t p          = graphics->pixels[i];
        const unsigned char rgb[] = {(unsigned char) (((p >> 11U) & 31U) * 255U / 31U),
                                     (unsigned char) (((p >> 5U) & 63U) * 255U / 63U),
                                     (unsigned char) ((p & 31U) * 255U / 31U)};
        assert(fwrite(rgb, 1U, 3U, file) == 3U);
    }
    assert(fclose(file) == 0);
}

int main(void)
{
    tabos_graphics_t graphics = {.width = POOL_CANVAS_WIDTH, .height = POOL_CANVAS_HEIGHT};
    pool_game_t game;
    pool_game_reset(&game);
    assert(tabos_graphics_open(&graphics) == 0);
    assert(graphics.scale == 2U);
    assert(pool_render(&graphics, &game) == 0 && presents == 1U);
    assert(pixel(&graphics, 100, 101) == TABOS_RGB565(23, 112, 88));
    assert(pixel(&graphics, 100, 44) == TABOS_RGB565(17, 78, 64));
    for (unsigned int i = 0U; i < POOL_POCKET_COUNT; ++i) {
        const pool_pocket_t* pocket = &pool_pockets[i];
        assert(pixel(&graphics, POOL_TABLE_X + pocket->center.x, POOL_TABLE_Y + pocket->center.y) ==
               TABOS_RGB565(3, 7, 10));
        /* The mouth interior is visibly open, not a painted sink behind a wall. */
        const int x = (pocket->mouth_a.x + pocket->mouth_b.x + pocket->center.x) / 3;
        const int y = (pocket->mouth_a.y + pocket->mouth_b.y + pocket->center.y) / 3;
        assert(pixel(&graphics, POOL_TABLE_X + x, POOL_TABLE_Y + y) == TABOS_RGB565(3, 7, 10));
    }
    tabos_color_t numbers[16][35] = {{0U}};
    for (unsigned int i = 1U; i < POOL_BALL_COUNT; ++i) {
        const int x = POOL_TABLE_X + pool_initial_balls[i].x;
        const int y = POOL_TABLE_Y + pool_initial_balls[i].y;
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 7; ++column) {
                numbers[i][row * 7 + column] = pixel(&graphics, x - 3 + column, y - 2 + row);
            }
        }
        for (unsigned int j = 1U; j < i; ++j) {
            assert(memcmp(numbers[i], numbers[j], sizeof(numbers[i])) != 0);
        }
    }
    /* Stripe cap stays white where an equivalent solid retains its color. */
    const pool_point_t solid  = pool_initial_balls[2];
    const pool_point_t stripe = pool_initial_balls[10];
    assert(pixel(&graphics, POOL_TABLE_X + solid.x, POOL_TABLE_Y + solid.y - 5) !=
           pixel(&graphics, POOL_TABLE_X + stripe.x, POOL_TABLE_Y + stripe.y - 5));
    for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
        game.angle = i * 256U;
        assert(pool_render(&graphics, &game) == 0);
        /* Cue and HUD changes never overwrite the identifying digits. */
        for (unsigned int j = 1U; j < POOL_BALL_COUNT; ++j) {
            for (int row = 0; row < 5; ++row) {
                for (int column = 0; column < 7; ++column) {
                    assert(numbers[j][row * 7 + column] == pixel(&graphics,
                                                                 POOL_TABLE_X + pool_initial_balls[j].x - 3 + column,
                                                                 POOL_TABLE_Y + pool_initial_balls[j].y - 2 + row));
                }
            }
        }
    }
    assert(pool_render(&graphics, NULL) == -1 && errno == EINVAL);
    pool_game_reset(&game);
    assert(pool_render(&graphics, &game) == 0);
    const int guide_x = POOL_TABLE_X + pool_initial_balls[0].x + 64;
    const int guide_y = POOL_TABLE_Y + pool_initial_balls[0].y;
    assert(pixel(&graphics, guide_x, guide_y) == TABOS_RGB565(246, 242, 218));
    assert(pixel(&graphics, 420, 17) == TABOS_RGB565(219, 183, 105));
    game.power = 1U;
    assert(pool_render(&graphics, &game) == 0);
    assert(pixel(&graphics, 420, 17) == TABOS_RGB565(7, 14, 18));
    assert(pool_game_shoot(&game));
    assert(pool_render(&graphics, &game) == 0);
    assert(pixel(&graphics, guide_x, guide_y) != TABOS_RGB565(246, 242, 218));
    pool_game_reset(&game);
    game.paused = true;
    assert(pool_render(&graphics, &game) == 0);
    assert(pixel(&graphics, guide_x, guide_y) != TABOS_RGB565(246, 242, 218));
    pool_game_reset(&game);
    assert(pool_render(&graphics, &game) == 0);
    const int object_x               = POOL_TABLE_X + pool_initial_balls[1].x + 4;
    const int object_y               = POOL_TABLE_Y + pool_initial_balls[1].y + 2;
    const tabos_color_t object_pixel = pixel(&graphics, object_x, object_y);
    game.potted                      = 3U;
    game.placement                   = true;
    game.placement_position          = (pool_vec_t) {132 * POOL_ONE, 132 * POOL_ONE};
    assert(pool_render(&graphics, &game) == 0);
    assert(pixel(&graphics, object_x, object_y) != object_pixel);
    assert(pixel(&graphics, POOL_TABLE_X + 132, POOL_TABLE_Y + 132) == TABOS_RGB565(219, 183, 105));
    game.placement_position = game.balls[3].position;
    assert(pool_render(&graphics, &game) == 0);
    assert(pixel(&graphics, POOL_TABLE_X + game.placement_position.x / POOL_ONE,
                 POOL_TABLE_Y + game.placement_position.y / POOL_ONE) == TABOS_RGB565(235, 65, 65));
    pool_game_reset(&game);
    game.title    = true;
    game.computer = true;
    assert(pool_render(&graphics, &game) == 0);
    assert(pixel(&graphics, 122, 100) == TABOS_RGB565(219, 183, 105));
    capture(&graphics, "title");
    game.help = true;
    assert(pool_render(&graphics, &game) == 0);
    assert(pixel(&graphics, 70, 100) == TABOS_RGB565(219, 183, 105));
    capture(&graphics, "help");
    game.help            = false;
    game.title           = false;
    game.potted          = (uint16_t) ((1U << 1U) | (1U << 9U));
    game.rules.groups[0] = POOL_SOLIDS;
    game.rules.groups[1] = POOL_STRIPES;
    assert(pool_render(&graphics, &game) == 0);
    assert(pixel(&graphics, 19, 90) != TABOS_RGB565(13, 23, 29));
    capture(&graphics, "table");
    game.rules.complete = true;
    game.rules.winner   = 0U;
    assert(pool_render(&graphics, &game) == 0);
    assert(pixel(&graphics, 132, 150) == TABOS_RGB565(219, 183, 105));
    capture(&graphics, "result");
    /* Reserved upper-right OS overlay region stays clear. */
    assert(pixel(&graphics, 630, 10) == TABOS_RGB565(13, 23, 29));
    pool_game_reset(&game);
    game.potted                    = UINT16_C(0xfffe);
    game.shot_set                  = true;
    pool_game_t previous           = game;
    previous.balls[0].position     = (pool_vec_t) {100 * POOL_ONE, 100 * POOL_ONE};
    game.balls[0].position         = (pool_vec_t) {120 * POOL_ONE, 100 * POOL_ONE};
    const unsigned int before_draw = presents;
    assert(pool_render_draw(&graphics, &game, &previous, 500U) == 0);
    assert(presents == before_draw);
    assert(pixel(&graphics, POOL_TABLE_X + 110, POOL_TABLE_Y + 100) == TABOS_RGB565(246, 242, 218));
    assert(pixel(&graphics, POOL_TABLE_X + 120, POOL_TABLE_Y + 100) != TABOS_RGB565(246, 242, 218));
    assert(pool_render_draw(&graphics, &game, &previous, 1000U) == -1 && errno == EINVAL);
    present_result = -EIO;
    assert(pool_render(&graphics, &game) == -1 && errno == EIO);
    assert(tabos_graphics_close(&graphics) == 0);
    assert(pool_render(&graphics, &game) == -1);
    puts("Pool rendering: scale, pocket openings, distinct labels and errors passed");
    return 0;
}
