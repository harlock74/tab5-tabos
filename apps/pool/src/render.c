#include <pool/render.h>
#include <pool/table.h>

#include <errno.h>
#include <string.h>

#define BACKGROUND TABOS_RGB565(13, 23, 29)
#define INK        TABOS_RGB565(7, 14, 18)
#define FELT       TABOS_RGB565(23, 112, 88)
#define CUSHION    TABOS_RGB565(17, 78, 64)
#define EDGE       TABOS_RGB565(43, 144, 111)
#define WHITE      TABOS_RGB565(246, 242, 218)
#define MUTED      TABOS_RGB565(137, 164, 159)
#define GOLD       TABOS_RGB565(219, 183, 105)
#define POCKET     TABOS_RGB565(3, 7, 10)

typedef struct {
        char character;
        uint8_t rows[7];
} glyph_t;
static const glyph_t glyphs[] = {
#include "../assets/font5x7.inc"
};

typedef struct {
        tabos_color_t* pixels;
        int error;
} painter_t;

static int scaled_start(int value)
{
    return value;
}

static int scaled_end(int value)
{
    return value;
}

static int scaled_coordinate(int value)
{
    return value;
}

static void canvas_box(painter_t* painter, int x, int y, int width, int height, tabos_color_t color);

static void box(painter_t* painter, int x, int y, int width, int height, tabos_color_t color)
{
    if (painter->error == 0 && width > 0 && height > 0) {
        const int left   = scaled_start(x);
        const int top    = scaled_start(y);
        const int right  = scaled_end(x + width);
        const int bottom = scaled_end(y + height);
        if (right > left && bottom > top) {
            canvas_box(painter, left, top, right - left, bottom - top, color);
        }
    }
}

typedef struct {
        char* text;
        size_t capacity;
        size_t length;
} string_builder_t;

static string_builder_t string_builder(char* text_value, size_t capacity)
{
    if (capacity > 0U) {
        text_value[0] = '\0';
    }
    return (string_builder_t) {.text = text_value, .capacity = capacity, .length = 0U};
}

static void append_text(string_builder_t* builder, const char* value)
{
    while (*value != '\0' && builder->length + 1U < builder->capacity) {
        builder->text[builder->length++] = *value++;
    }
    if (builder->capacity > 0U) {
        builder->text[builder->length] = '\0';
    }
}

static void append_unsigned(string_builder_t* builder, unsigned int value)
{
    char digits[10];
    unsigned int count = 0U;
    do {
        digits[count++]  = (char) ('0' + value % 10U);
        value           /= 10U;
    } while (value != 0U && count < sizeof(digits));
    while (count > 0U) {
        char digit[2] = {digits[--count], '\0'};
        append_text(builder, digit);
    }
}

static void canvas_box(painter_t* painter, int x, int y, int width, int height, tabos_color_t color)
{
    if (painter->error != 0 || width <= 0 || height <= 0) {
        return;
    }
    const int left   = x < 0 ? 0 : x;
    const int top    = y < 0 ? 0 : y;
    const int right  = x + width > POOL_CANVAS_WIDTH ? POOL_CANVAS_WIDTH : x + width;
    const int bottom = y + height > POOL_CANVAS_HEIGHT ? POOL_CANVAS_HEIGHT : y + height;
    if (right <= left || bottom <= top) {
        return;
    }
    for (int row = top; row < bottom; ++row) {
        tabos_color_t* destination = painter->pixels + (size_t) row * POOL_CANVAS_WIDTH + (size_t) left;
        for (int column = left; column < right; ++column) {
            *destination++ = color;
        }
    }
}

static void line(painter_t* painter, int x0, int y0, int x1, int y1, tabos_color_t color)
{
    x0           = scaled_coordinate(x0);
    y0           = scaled_coordinate(y0);
    x1           = scaled_coordinate(x1);
    y1           = scaled_coordinate(y1);
    const int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = y1 >= y0 ? y0 - y1 : y1 - y0;
    const int sy = y0 < y1 ? 1 : -1;
    int error    = dx + dy;
    for (;;) {
        canvas_box(painter, x0, y0, 1, 1, color);
        if (x0 == x1 && y0 == y1) {
            return;
        }
        const int twice_error = error * 2;
        if (twice_error >= dy) {
            error += dy;
            x0    += sx;
        }
        if (twice_error <= dx) {
            error += dx;
            y0    += sy;
        }
    }
}

static void disc(painter_t* painter, int x, int y, int radius, tabos_color_t color)
{
    for (int dy = -radius; dy <= radius; ++dy) {
        int span = radius;
        while (span * span + dy * dy > radius * radius) {
            --span;
        }
        box(painter, x - span, y + dy, 2 * span + 1, 1, color);
    }
}

/* Convex table-local polygons only. Half-open edge crossings avoid counting a
 * vertex twice. SDK spans still clip the final screen coordinates. */
static void polygon(painter_t* painter, const pool_point_t* points, unsigned int count, tabos_color_t color)
{
    int top    = points[0].y;
    int bottom = top;
    for (unsigned int i = 1U; i < count; ++i) {
        if (points[i].y < top) {
            top = points[i].y;
        }
        if (points[i].y > bottom) {
            bottom = points[i].y;
        }
    }
    for (int y = top; y < bottom; ++y) {
        int left  = POOL_TABLE_WIDTH + 32;
        int right = -32;
        for (unsigned int i = 0U; i < count; ++i) {
            const pool_point_t a = points[i];
            const pool_point_t b = points[(i + 1U) % count];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y)) {
                const int x = a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y);
                if (x < left) {
                    left = x;
                }
                if (x > right) {
                    right = x;
                }
            }
        }
        if (right >= left) {
            box(painter, POOL_TABLE_X + left, POOL_TABLE_Y + y, right - left + 1, 1, color);
        }
    }
}

static void text(painter_t* painter, int x, int y, const char* value, int scale, tabos_color_t color)
{
    while (*value != '\0') {
        const uint8_t* rows = glyphs[0].rows;
        for (size_t i = 0U; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i) {
            if (glyphs[i].character == *value) {
                rows = glyphs[i].rows;
                break;
            }
        }
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[row] & (1U << (4 - column))) != 0U) {
                    box(painter, x + column * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += 6 * scale;
        ++value;
    }
}

static void number(painter_t* painter, int x, int y, unsigned int value)
{
    static const uint8_t digits[10][5] = {
        {7, 5, 5, 5, 7},
        {2, 6, 2, 2, 7},
        {7, 1, 7, 4, 7},
        {7, 1, 7, 1, 7},
        {5, 5, 7, 1, 1},
        {7, 4, 7, 1, 7},
        {7, 4, 7, 5, 7},
        {7, 1, 1, 1, 1},
        {7, 5, 7, 5, 7},
        {7, 5, 7, 1, 7},
    };
    x                        = scaled_coordinate(x);
    y                        = scaled_coordinate(y);
    const unsigned int count = value >= 10U ? 2U : 1U;
    const int start          = x - (count == 2U ? 3 : 1);
    for (unsigned int digit = 0U; digit < count; ++digit) {
        unsigned int index = value % 10U;
        if (count == 2U && digit == 0U) {
            index = value / 10U;
        }
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 3; ++column) {
                if ((digits[index][row] & (1U << (2 - column))) != 0U) {
                    canvas_box(painter, start + (int) digit * 4 + column, y - 2 + row, 1, 1, INK);
                }
            }
        }
    }
}

static tabos_color_t ball_color(unsigned int id)
{
    static const tabos_color_t colors[8] = {
        TABOS_RGB565(227, 180, 39), TABOS_RGB565(48, 105, 204), TABOS_RGB565(215, 54, 47), TABOS_RGB565(142, 72, 166),
        TABOS_RGB565(238, 116, 34), TABOS_RGB565(29, 133, 83),  TABOS_RGB565(138, 43, 51), TABOS_RGB565(20, 25, 31),
    };
    return id == 0U ? WHITE : colors[(id - 1U) % 8U];
}

static void ball(painter_t* painter, int x, int y, unsigned int id)
{
    const int radius          = POOL_BALL_RADIUS;
    const tabos_color_t color = ball_color(id);
    disc(painter, x + 1, y + 2, radius, TABOS_RGB565(15, 66, 53));
    disc(painter, x, y, radius, INK);
    disc(painter, x, y, radius - 1, id > 8U ? WHITE : color);
    if (id > 8U) {
        for (int dy = -3; dy <= 3; ++dy) {
            const int span = dy == 0 ? radius - 1 : radius - 2;
            box(painter, x - span, y + dy, span * 2 + 1, 1, color);
        }
    }
    if (id != 0U) {
        const int half = id >= 10U ? 4 : 2;
        canvas_box(painter, scaled_coordinate(x) - half, scaled_coordinate(y) - 3, half * 2 + 1, 7, WHITE);
        number(painter, x, y, id);
    }
    box(painter, x - 3, y - 4, 2, 1, WHITE);
}

static void diamond(painter_t* painter, int x, int y)
{
    line(painter, x, y - 2, x + 2, y, GOLD);
    line(painter, x + 2, y, x, y + 2, GOLD);
    line(painter, x, y + 2, x - 2, y, GOLD);
    line(painter, x - 2, y, x, y - 2, GOLD);
}

static void table(painter_t* painter)
{
    const int x = POOL_TABLE_X;
    const int y = POOL_TABLE_Y;
    const int w = POOL_TABLE_WIDTH;
    const int h = POOL_TABLE_HEIGHT;
    box(painter, x - 18, y - 16, w + 39, h + 37, INK);
    box(painter, x - 17, y - 17, w + 35, h + 35, TABOS_RGB565(127, 84, 49));
    box(painter, x - 15, y - 15, w + 31, h + 31, TABOS_RGB565(86, 54, 34));
    box(painter, x - 12, y - 12, w + 25, h + 25, TABOS_RGB565(39, 45, 34));
    box(painter, x, y, w + 1, h + 1, FELT);
    /* Quiet cloth detail and the head string; no large second background buffer. */
    for (int row = 8; row < h; row += 8) {
        for (int column = 8; column < w; column += 16) {
            box(painter, x + column, y + row, 1, 1, TABOS_RGB565(25, 115, 90));
        }
    }
    for (int row = 6; row < h; row += 6) {
        box(painter, x + w / 4, y + row, 1, 2, TABOS_RGB565(78, 151, 122));
    }
    disc(painter, x + w / 4, y + h / 2, 2, MUTED);
    disc(painter, x + w * 3 / 4, y + h / 2, 2, MUTED);

    for (unsigned int i = 0U; i < POOL_POCKET_COUNT; ++i) {
        const pool_pocket_t* pocket = &pool_pockets[i];
        disc(painter, x + pocket->center.x, y + pocket->center.y, pocket->radius + 3, TABOS_RGB565(155, 135, 91));
        disc(painter, x + pocket->center.x, y + pocket->center.y, pocket->radius + 1, INK);
        const pool_point_t throat[] = {pocket->mouth_a, pocket->mouth_b, pocket->center};
        polygon(painter, throat, 3U, POCKET);
        disc(painter, x + pocket->center.x, y + pocket->center.y, pocket->radius, POCKET);
    }
    for (unsigned int i = 0U; i < POOL_CUSHION_COUNT; ++i) {
        const pool_cushion_t* cushion = &pool_cushions[i];
        const pool_point_t points[]   = {cushion->a, cushion->b, cushion->c, cushion->d};
        polygon(painter, points, 4U, CUSHION);
        line(painter, x + cushion->a.x, y + cushion->a.y, x + cushion->b.x, y + cushion->b.y, EDGE);
        disc(painter, x + cushion->a.x, y + cushion->a.y, POOL_JAW_RADIUS, EDGE);
        disc(painter, x + cushion->b.x, y + cushion->b.y, POOL_JAW_RADIUS, EDGE);
    }
    for (int i = 1; i < 8; ++i) {
        if (i != 4) {
            diamond(painter, x + i * w / 8, y - 14);
            diamond(painter, x + i * w / 8, y + h + 14);
        }
    }
    for (int i = 1; i < 4; ++i) {
        diamond(painter, x - 14, y + i * h / 4);
        diamond(painter, x + w + 14, y + i * h / 4);
    }
}

static void aiming(painter_t* painter, const pool_game_t* game)
{
    const pool_vec_t direction = pool_direction(game->angle);
    const pool_vec_t start     = game->balls[0].position;
    const int x                = POOL_TABLE_X + start.x / POOL_ONE;
    const int y                = POOL_TABLE_Y + start.y / POOL_ONE;
    if (!pool_game_can_aim(game)) {
        return;
    }
    const pool_vec_t end = pool_game_aim_end(game);
    /* Dashed direction guide; it predicts no object-ball collision yet. */
    for (int i = 4; i < 64; i += 2) {
        const int px = POOL_TABLE_X + (start.x + (end.x - start.x) * i / 64) / POOL_ONE;
        const int py = POOL_TABLE_Y + (start.y + (end.y - start.y) * i / 64) / POOL_ONE;
        box(painter, px, py, 1, 1, WHITE);
    }
    const int gap    = 12 + (int) game->power / 10;
    const int tip_x  = x - direction.x * gap / POOL_ONE;
    const int tip_y  = y - direction.y * gap / POOL_ONE;
    const int tail_x = x - direction.x * (gap + 56) / POOL_ONE;
    const int tail_y = y - direction.y * (gap + 56) / POOL_ONE;
    line(painter, tail_x, tail_y + 1, tip_x, tip_y + 1, INK);
    line(painter, tail_x, tail_y, tip_x, tip_y, GOLD);
    line(painter, tip_x - direction.x * 5 / POOL_ONE, tip_y - direction.y * 5 / POOL_ONE, tip_x, tip_y, WHITE);
}

static void ring(painter_t* painter, int x, int y, int radius, tabos_color_t color)
{
    for (int row = -radius; row <= radius; ++row) {
        for (int col = -radius; col <= radius; ++col) {
            const int square = row * row + col * col;
            if (square <= radius * radius && square > (radius - 1) * (radius - 1)) {
                box(painter, x + col, y + row, 1, 1, color);
            }
        }
    }
}

static void feedback(painter_t* painter, const pool_game_t* game)
{
    for (unsigned int i = 0U; i < POOL_POCKET_COUNT; ++i) {
        if (game->pot_flash[i] != 0U) {
            const pool_pocket_t* pocket = &pool_pockets[i];
            const int radius            = pocket->radius + 2 + (int) (18U - game->pot_flash[i]) / 3;
            ring(painter, POOL_TABLE_X + pocket->center.x, POOL_TABLE_Y + pocket->center.y, radius, GOLD);
        }
    }
    if (game->cue_flash != 0U && !game->placement) {
        const pool_vec_t direction = pool_direction(game->cue_angle);
        const int x                = POOL_TABLE_X + game->cue_origin.x / POOL_ONE;
        const int y                = POOL_TABLE_Y + game->cue_origin.y / POOL_ONE;
        const int gap              = 8 + (int) (8U - game->cue_flash) * 3;
        line(painter, x - direction.x * (gap + 35) / POOL_ONE, y - direction.y * (gap + 35) / POOL_ONE,
             x - direction.x * gap / POOL_ONE, y - direction.y * gap / POOL_ONE, GOLD);
    }
}

static void trays(painter_t* painter, const pool_game_t* game)
{
    for (unsigned int side = 0U; side < 2U; ++side) {
        const int x = side == 0U ? 19 : 621;
        text(painter, x - (side == 0U ? 8 : 11), 70, side == 0U ? "1-7" : "9-15", 1, MUTED);
        for (unsigned int row = 0U; row < 7U; ++row) {
            const unsigned int id = side * 8U + row + 1U;
            const int y           = 94 + (int) row * 27;
            if ((game->potted & (1U << id)) != 0U) {
                ball(painter, x, y, id);
            } else {
                ring(painter, x, y, 6, TABOS_RGB565(43, 65, 66));
            }
        }
    }
}

static void centered(painter_t* painter, int y, const char* value, int scale, tabos_color_t color)
{
    text(painter, (POOL_LAYOUT_WIDTH - (int) strlen(value) * 6 * scale + scale) / 2, y, value, scale, color);
}

static void panel(painter_t* painter, int x, int y, int width, int height)
{
    box(painter, x + 4, y + 5, width, height, INK);
    box(painter, x, y, width, height, GOLD);
    box(painter, x + 1, y + 1, width - 2, height - 2, BACKGROUND);
    line(painter, x + 16, y + 8, x + width - 17, y + 8, CUSHION);
}

static void dialogs(painter_t* painter, const pool_game_t* game)
{
    if (game->help) {
        panel(painter, 70, 54, 500, 268);
        centered(painter, 70, "HOW TO PLAY", 2, GOLD);
        static const char* lines[] = {
            "LEFT RIGHT / A D: AIM    UP DOWN / W S: POWER",       "SHIFT: FINE AIM    SPACE OR K: SHOOT",
            "BALL IN HAND: ARROWS / WASD MOVE, ENTER CONFIRMS",    "GOLD PLACEMENT IS VALID. RED IS BLOCKED.",
            "P: PAUSE    R: NEW RACK    M: MUTE    Q: QUIT",       "GROUPS STAY OPEN AFTER THE BREAK.",
            "YOUR FIRST LEGAL POT THEN ASSIGNS SOLIDS / STRIPES.", "HIT YOUR GROUP FIRST. POT YOUR BALL TO CONTINUE.",
            "AFTER CONTACT, ANY BALL MUST REACH A RAIL OR POT.",   "FOUL OR SCRATCH: OPPONENT GETS BALL IN HAND.",
            "CLEAR YOUR GROUP BEFORE SHOOTING THE 8 TO WIN.",      "EARLY 8 OR FOUL WITH 8: LOSS. BREAK 8: RERACK.",
            "NO CALLED POCKETS. SIDE TRAYS SHOW POTTED BALLS."};
        for (unsigned int i = 0U; i < sizeof(lines) / sizeof(lines[0]); ++i) {
            centered(painter, 100 + (int) i * 14, lines[i], 1, WHITE);
        }
        centered(painter, 298, "H / ENTER / ESC: RETURN", 1, GOLD);
    } else if (game->restart_pending) {
        panel(painter, 150, 116, 340, 128);
        centered(painter, 136, "NEW RACK?", 3, WHITE);
        centered(painter, 172, "THIS RACK WILL BE REPLACED", 1, MUTED);
        centered(painter, 196, "ENTER: YES   R / ESC: CANCEL", 1, GOLD);
        centered(painter, 220, "Q: QUIT", 1, MUTED);
    } else if (game->title) {
        panel(painter, 122, 64, 396, 248);
        centered(painter, 86, "POOL", 5, WHITE);
        centered(painter, 132, "THE TABOS BILLIARD ROOM", 1, GOLD);
        ball(painter, 320, 157, 8U);
        box(painter, 164, 178, 312, 31, CUSHION);
        centered(painter, 189, game->computer ? "<  YOU VS COMPUTER  >" : "<  TWO LOCAL PLAYERS  >", 1, WHITE);
        centered(painter, 219, "LEFT / RIGHT: CHANGE MODE", 1, MUTED);
        centered(painter, 240, "ENTER TO PLAY", 2, GOLD);
        centered(painter, 276, game->muted ? "H: HOW TO PLAY   M: SOUND OFF" : "H: HOW TO PLAY   M: SOUND ON", 1,
                 WHITE);
        centered(painter, 294, "Q / ESC: RETURN TO SHELL", 1, MUTED);
    } else if (game->rules.complete) {
        panel(painter, 132, 106, 376, 154);
        char winner[32];
        string_builder_t winner_text = string_builder(winner, sizeof(winner));
        append_text(&winner_text, "PLAYER ");
        append_unsigned(&winner_text, game->rules.winner + 1U);
        append_text(&winner_text, " WINS");
        const char* title = winner;
        if (game->computer) {
            title = game->rules.winner == 0U ? "YOU WIN!" : "COMPUTER WINS";
        }
        centered(painter, 128, title, 3, WHITE);
        const char* reason = "LEGAL 8-BALL";
        if (game->rules.early_eight) {
            reason = "8 POTTED TOO EARLY";
        } else if (game->rules.foul != POOL_FAIR) {
            reason = pool_rules_foul_name(game->rules.foul);
        }
        centered(painter, 171, reason, 1, GOLD);
        centered(painter, 202, "ENTER: PLAY AGAIN", 2, WHITE);
        centered(painter, 238, "H: RULES   Q / ESC: QUIT", 1, MUTED);
    } else if (game->paused) {
        panel(painter, 180, 124, 280, 108);
        centered(painter, 143, "PAUSED", 3, WHITE);
        centered(painter, 181, "P: RESUME   H: HELP   M: MUTE", 1, GOLD);
        centered(painter, 210, "R: NEW RACK   Q: QUIT", 1, MUTED);
    }
}

static int32_t interpolate(int32_t previous, int32_t current, uint32_t fraction)
{
    return previous + (int32_t) (((int64_t) current - previous) * fraction / 1000);
}

static int render_background(tabos_color_t* pixels)
{
    if (pixels == NULL) {
        return -1;
    }
    painter_t painter = {.pixels = pixels};
    canvas_box(&painter, 0, 0, POOL_CANVAS_WIDTH, POOL_CANVAS_HEIGHT, BACKGROUND);
    table(&painter);
    return painter.error == 0 ? 0 : -1;
}

static int render_pixels(tabos_color_t* pixels, const tabos_color_t* background, const pool_game_t* game,
                         const pool_game_t* previous, uint32_t interpolation)
{
    if (pixels == NULL || game == NULL || interpolation >= 1000U) {
        return -1;
    }
    pool_game_t frame;
    if (previous != NULL && previous->shot_set && game->shot_set) {
        frame = *game;
        for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
            frame.balls[i].position.x =
                interpolate(previous->balls[i].position.x, game->balls[i].position.x, interpolation);
            frame.balls[i].position.y =
                interpolate(previous->balls[i].position.y, game->balls[i].position.y, interpolation);
        }
        game = &frame;
    }
    painter_t painter = {.pixels = pixels};
    if (background != NULL) {
        memcpy(pixels, background, (size_t) POOL_CANVAS_WIDTH * POOL_CANVAS_HEIGHT * sizeof(*pixels));
    } else if (render_background(pixels) != 0) {
        return -1;
    }
    trays(&painter, game);
    feedback(&painter, game);
    aiming(&painter, game);
    for (unsigned int i = 0U; i < POOL_BALL_COUNT; ++i) {
        if ((game->potted & (1U << i)) != 0U) {
            continue;
        }
        ball(&painter, POOL_TABLE_X + game->balls[i].position.x / POOL_ONE,
             POOL_TABLE_Y + game->balls[i].position.y / POOL_ONE, i);
    }

    if (game->placement) {
        const int x               = POOL_TABLE_X + game->placement_position.x / POOL_ONE;
        const int y               = POOL_TABLE_Y + game->placement_position.y / POOL_ONE;
        const tabos_color_t color = pool_game_placement_valid(game) ? GOLD : TABOS_RGB565(235, 65, 65);
        disc(&painter, x, y, POOL_BALL_RADIUS + 2, color);
        disc(&painter, x, y, POOL_BALL_RADIUS, FELT);
        line(&painter, x - 3, y, x + 3, y, color);
        line(&painter, x, y - 3, x, y + 3, color);
    }
    text(&painter, 40, 10, "POOL", 2, WHITE);
    const bool computer = pool_game_computer_turn(game);
    const char* state   = "AIM AND SHOOT";
    if (game->help) {
        state = "HOW TO PLAY";
    } else if (game->title) {
        state = "WELCOME";
    } else if (game->restart_pending) {
        state = "NEW RACK?";
    } else if (game->rules.complete) {
        state = "GAME OVER";
    } else if (game->paused) {
        state = "PAUSED";
    } else if (computer && !game->shot_set) {
        state = "COMPUTER THINKS";
    } else if (game->placement) {
        state = "PLACE CUE";
    } else if ((game->potted & 1U) != 0U) {
        state = "SCRATCH - WAIT";
    } else if (game->shot_set) {
        state = "BALLS MOVING";
    } else if (!pool_game_can_aim(game)) {
        state = "BALLS MOVING";
    }
    text(&painter, 106, 14, state, 1, GOLD);
    char label[40];
    const unsigned int degrees  = game->angle * 3600U / POOL_ANGLE_COUNT;
    string_builder_t label_text = string_builder(label, sizeof(label));
    append_text(&label_text, "AIM ");
    append_unsigned(&label_text, degrees / 10U);
    append_text(&label_text, ".");
    append_unsigned(&label_text, degrees % 10U);
    append_text(&label_text, "   POWER ");
    append_unsigned(&label_text, game->power);
    text(&painter, 254, 14, label, 1, WHITE);
    box(&painter, 400, 12, 102, 11, INK);
    box(&painter, 401, 13, (int) game->power, 9, GOLD);
    const char* controls = "LEFT RIGHT AIM   UP DOWN POWER   SHIFT FINE   SPACE OR K SHOOT";
    if (game->placement) {
        controls = "BALL IN HAND: ARROWS OR WASD MOVE   SHIFT FINE   ENTER CONFIRM";
    }
    if (game->computer) {
        controls = computer ? "COMPUTER P2   P PAUSE   R NEW RACK   Q QUIT" :
                              "YOU P1: LEFT RIGHT AIM  UP DOWN POWER  SHIFT FINE  SPACE K SHOOT";
        if (game->placement && !computer) {
            controls = "YOU P1 BALL IN HAND: ARROWS WASD MOVE  SHIFT FINE  ENTER CONFIRM";
        }
    }
    if (game->title) {
        controls = "LEFT RIGHT: MODE   ENTER: PLAY   H: HELP   M: SOUND";
    }
    if (game->paused) {
        controls = "P: RESUME   H: HELP   M: SOUND   R: NEW RACK   Q: QUIT";
    }
    if (game->rules.complete) {
        controls = "ENTER: PLAY AGAIN   H: RULES   M: SOUND   Q: QUIT";
    }
    if (game->restart_pending) {
        controls = "ENTER: NEW RACK   R OR ESC: CANCEL   Q: QUIT";
    }
    if (game->help) {
        controls = "H OR ENTER OR ESC: RETURN   M: SOUND   Q: QUIT";
    }
    text(&painter, 40, 338, controls, 1, WHITE);
    char progress[100];
    char status_line[100];
    const pool_group_t group       = game->rules.groups[game->rules.player];
    string_builder_t progress_text = string_builder(progress, sizeof(progress));
    if (game->rules.break_shot) {
        append_text(&progress_text, "P");
        append_unsigned(&progress_text, game->rules.player + 1U);
        append_text(&progress_text, " BREAK - TABLE OPEN   P PAUSE   R NEW RACK   Q QUIT");
    } else if (group == POOL_OPEN) {
        append_text(&progress_text, "P");
        append_unsigned(&progress_text, game->rules.player + 1U);
        append_text(&progress_text, " TABLE OPEN   P PAUSE   R NEW RACK   Q QUIT");
    } else {
        const char* target = pool_rules_remaining(group, game->potted) == 0U ? "ON 8" : pool_rules_group_name(group);
        append_text(&progress_text, "P");
        append_unsigned(&progress_text, game->rules.player + 1U);
        append_text(&progress_text, " ");
        append_text(&progress_text, target);
        append_text(&progress_text, "  |  P1 ");
        append_text(&progress_text, pool_rules_group_name(game->rules.groups[0]));
        append_text(&progress_text, " ");
        append_unsigned(&progress_text, pool_rules_remaining(game->rules.groups[0], game->potted));
        append_text(&progress_text, " LEFT  P2 ");
        append_text(&progress_text, pool_rules_group_name(game->rules.groups[1]));
        append_text(&progress_text, " ");
        append_unsigned(&progress_text, pool_rules_remaining(game->rules.groups[1], game->potted));
        append_text(&progress_text, " LEFT  |  R NEW Q QUIT");
    }
    const char* hint = progress;
    if (game->placement) {
        string_builder_t status = string_builder(status_line, sizeof(status_line));
        append_text(&status, "P");
        append_unsigned(&status, game->rules.player + 1U);
        append_text(&status, " BALL IN HAND: ");
        append_text(&status, pool_rules_foul_name(game->rules.foul));
        append_text(&status, " | ");
        append_text(&status, pool_game_placement_valid(game) ? "GOLD VALID - ENTER" : "RED BLOCKED - MOVE");
        hint = status_line;
    } else if (game->rules.reracked) {
        string_builder_t status = string_builder(status_line, sizeof(status_line));
        append_text(&status, "8 ON BREAK - RERACKED   P");
        append_unsigned(&status, game->rules.player + 1U);
        append_text(&status, " BREAKS AGAIN   SPACE OR K SHOOT");
        hint = status_line;
    }
    if (game->paused) {
        hint = "PAUSED - P TO RESUME   R RESET   Q OR ESC QUIT";
    }
    if (game->rules.complete) {
        const char* reason = "LEGAL 8-BALL";
        if (game->rules.early_eight) {
            reason = "8 POTTED TOO EARLY";
        } else if (game->rules.foul != POOL_FAIR) {
            reason = pool_rules_foul_name(game->rules.foul);
        }
        string_builder_t status = string_builder(status_line, sizeof(status_line));
        append_text(&status, "PLAYER ");
        append_unsigned(&status, game->rules.winner + 1U);
        append_text(&status, " WINS - ");
        append_text(&status, reason);
        append_text(&status, "   ENTER NEW RACK   Q QUIT");
        hint = status_line;
    }
    if (game->restart_pending) {
        hint = "START NEW RACK? ENTER YES   R OR ESC CANCEL   Q QUIT";
    }
    if (game->title) {
        hint = "HOUSE 8-BALL   CLEAR YOUR GROUP, THEN POT THE 8";
    }
    if (game->help) {
        hint = "PLAY IS FROZEN WHILE HELP IS OPEN";
    }
    text(&painter, 40, 350, hint, 1, MUTED);
    text(&painter, 7, 291, "H", 1, GOLD);
    text(&painter, 7, 302, "HELP", 1, MUTED);
    text(&painter, 610, 291, "M", 1, GOLD);
    text(&painter, 610, 302, game->muted ? "OFF" : "ON", 1, MUTED);
    dialogs(&painter, game);
    if (painter.error != 0) {
        return -1;
    }
    return 0;
}

void pool_render_prepare(pool_render_job_t* job, tabos_color_t* pixels, const tabos_color_t* background,
                         const pool_game_t* game, const pool_game_t* previous, uint32_t interpolation)
{
    if (job == NULL) {
        return;
    }
    *job = (pool_render_job_t) {
        .pixels = pixels, .background = background, .interpolation = interpolation, .has_previous = previous != NULL};
    if (game != NULL) {
        job->game = *game;
    }
    if (previous != NULL) {
        job->previous = *previous;
    }
}

void pool_render_execute(void* data)
{
    pool_render_job_t* job = data;
    if (job != NULL) {
        job->result = render_pixels(job->pixels, job->background, &job->game, job->has_previous ? &job->previous : NULL,
                                    job->interpolation);
    }
}

void pool_render_background_execute(void* data)
{
    pool_render_background_job_t* job = data;
    if (job != NULL) {
        job->result = render_background(job->pixels);
    }
}

int pool_render_draw(tabos_graphics_t* graphics, const pool_game_t* game, const pool_game_t* previous,
                     uint32_t interpolation)
{
    if (game == NULL || graphics == NULL || graphics->width != POOL_CANVAS_WIDTH ||
        graphics->height != POOL_CANVAS_HEIGHT || interpolation >= 1000U) {
        errno = EINVAL;
        return -1;
    }
    pool_render_job_t job;
    pool_render_prepare(&job, tabos_graphics_pixels(graphics), NULL, game, previous, interpolation);
    pool_render_execute(&job);
    if (job.result != 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int pool_render(tabos_graphics_t* graphics, const pool_game_t* game)
{
    if (pool_render_draw(graphics, game, NULL, 0U) != 0) {
        return -1;
    }
    return tabos_graphics_present(graphics);
}
