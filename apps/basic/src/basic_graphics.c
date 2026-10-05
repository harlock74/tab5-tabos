#include <basic/c64.h>
#include <basic/extensions.h>
#include <basic/graphics.h>
#include <basic/runtime.h>
#include <stdint.h>
#include <string.h>
#include <tabos/graphics.h>

enum {
    WIDTH       = 320,
    HEIGHT      = 200,
    SPRITES     = 16,
    SIDE        = 16,
    TRANSPARENT = 0x0821
};
static tabos_graphics_t canvas;
static unsigned ink                    = 1U;
static const tabos_color_t palette[16] = {
    TABOS_RGB565(0, 0, 0),       TABOS_RGB565(255, 255, 255), TABOS_RGB565(220, 60, 60),   TABOS_RGB565(60, 220, 220),
    TABOS_RGB565(180, 70, 210),  TABOS_RGB565(70, 190, 80),   TABOS_RGB565(60, 90, 220),   TABOS_RGB565(240, 220, 70),
    TABOS_RGB565(240, 140, 50),  TABOS_RGB565(140, 85, 45),   TABOS_RGB565(250, 150, 150), TABOS_RGB565(65, 65, 65),
    TABOS_RGB565(120, 120, 120), TABOS_RGB565(160, 240, 160), TABOS_RGB565(150, 180, 255), TABOS_RGB565(195, 195, 195)};
typedef struct {
        tabos_color_t image[SIDE * SIDE];
        tabos_color_t underneath[SIDE * SIDE];
        int32_t x, y;
        unsigned width, height;
        bool visible;
} sprite_t;
static sprite_t sprites[SPRITES];

bool basic_graphics_active(void)
{
    return canvas.open;
}

void basic_graphics_close(void)
{
    if (canvas.open) {
        (void) tabos_graphics_close(&canvas);
    }
    memset(&canvas, 0, sizeof(canvas));
    memset(sprites, 0, sizeof(sprites));
}

static int present(void)
{
    // The public SDK exposes the application's logical canvas, never a physical
    // framebuffer. Save underlays in sprite order and restore in reverse order
    // so overlapping/translucent-key sprites never leave trails in the backbuffer.
    tabos_color_t* pixels = tabos_graphics_pixels(&canvas);
    if (pixels == NULL) {
        return -1;
    }
    for (unsigned id = 0U; id < SPRITES; ++id) {
        sprite_t* sprite = &sprites[id];
        if (!sprite->visible) {
            continue;
        }
        for (unsigned y = 0U; y < sprite->height; ++y) {
            for (unsigned x = 0U; x < sprite->width; ++x) {
                const int32_t dx      = sprite->x + (int32_t) x;
                const int32_t dy      = sprite->y + (int32_t) y;
                const unsigned offset = y * SIDE + x;
                if (dx >= 0 && dx < WIDTH && dy >= 0 && dy < HEIGHT) {
                    sprite->underneath[offset] = pixels[(unsigned) dy * WIDTH + (unsigned) dx];
                    if (sprite->image[offset] != TRANSPARENT) {
                        (void) tabos_graphics_pixel(&canvas, dx, dy, sprite->image[offset]);
                    }
                }
            }
        }
    }
    const int result = tabos_graphics_present(&canvas);
    for (unsigned id = SPRITES; id > 0U; --id) {
        sprite_t* sprite = &sprites[id - 1U];
        if (!sprite->visible) {
            continue;
        }
        for (unsigned y = 0U; y < sprite->height; ++y) {
            for (unsigned x = 0U; x < sprite->width; ++x) {
                const int32_t dx = sprite->x + (int32_t) x;
                const int32_t dy = sprite->y + (int32_t) y;
                if (dx >= 0 && dx < WIDTH && dy >= 0 && dy < HEIGHT) {
                    (void) tabos_graphics_pixel(&canvas, dx, dy, sprite->underneath[y * SIDE + x]);
                }
            }
        }
    }
    return result;
}

static int circle(int32_t cx, int32_t cy, int32_t radius, bool filled)
{
    // Bounded midpoint rasterizer; radius is at most 512, including off-screen work.
    int32_t x = radius, y = 0, decision = 1 - radius;
    while (x >= y) {
        if (!basic_runtime_sleep(0U)) {
            return 0;
        }
        if (filled) {
            (void) tabos_graphics_line(&canvas, cx - x, cy + y, cx + x, cy + y, palette[ink]);
            (void) tabos_graphics_line(&canvas, cx - x, cy - y, cx + x, cy - y, palette[ink]);
            (void) tabos_graphics_line(&canvas, cx - y, cy + x, cx + y, cy + x, palette[ink]);
            (void) tabos_graphics_line(&canvas, cx - y, cy - x, cx + y, cy - x, palette[ink]);
        } else {
            const int32_t points[8][2] = {
                { x,  y},
                { y,  x},
                {-y,  x},
                {-x,  y},
                {-x, -y},
                {-y, -x},
                { y, -x},
                { x, -y}
            };
            for (unsigned index = 0U; index < 8U; ++index) {
                (void) tabos_graphics_pixel(&canvas, cx + points[index][0], cy + points[index][1], palette[ink]);
            }
        }
        ++y;
        if (decision < 0) {
            decision += 2 * y + 1;
        } else {
            --x;
            decision += 2 * (y - x) + 1;
        }
    }
    return 0;
}

// Application-owned 5x7 glyph rows: digits, uppercase Latin, then punctuation.
static const uint8_t glyphs[][7] = {
    {14, 17, 19, 21, 25, 17, 14},
    { 4, 12,  4,  4,  4,  4, 14},
    {14, 17,  1,  2,  4,  8, 31},
    {30,  1,  1, 14,  1,  1, 30},
    { 2,  6, 10, 18, 31,  2,  2},
    {31, 16, 16, 30,  1,  1, 30},
    {14, 16, 16, 30, 17, 17, 14},
    {31,  1,  2,  4,  8,  8,  8},
    {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15,  1,  1, 14},
    {14, 17, 17, 31, 17, 17, 17},
    {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30},
    {31, 16, 16, 30, 16, 16, 31},
    {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15},
    {17, 17, 17, 31, 17, 17, 17},
    {14,  4,  4,  4,  4,  4, 14},
    { 7,  2,  2,  2, 18, 18, 12},
    {17, 18, 20, 24, 20, 18, 17},
    {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17},
    {17, 25, 21, 19, 17, 17, 17},
    {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16},
    {14, 17, 17, 17, 21, 18, 13},
    {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14,  1,  1, 30},
    {31,  4,  4,  4,  4,  4,  4},
    {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10,  4},
    {17, 17, 17, 21, 21, 21, 10},
    {17, 17, 10,  4, 10, 17, 17},
    {17, 17, 10,  4,  4,  4,  4},
    {31,  1,  2,  4,  8, 16, 31},
    { 0,  4,  4,  0,  4,  4,  0},
    { 0,  0,  0, 31,  0,  0,  0},
    {14, 17,  1,  2,  4,  0,  4}
};

static void text_at(int32_t x, int32_t y, const char* text, unsigned length)
{
    for (unsigned ch = 0U; ch < length; ++ch) {
        unsigned byte = (unsigned char) text[ch];
        if (byte >= 'a' && byte <= 'z') {
            byte -= 'a' - 'A';
        }
        if (byte != ' ') {
            unsigned glyph = 38U;
            if (byte >= '0' && byte <= '9') {
                glyph = byte - '0';
            } else if (byte >= 'A' && byte <= 'Z') {
                glyph = 10U + byte - 'A';
            } else if (byte == ':') {
                glyph = 36U;
            } else if (byte == '-') {
                glyph = 37U;
            }
            for (unsigned row = 0U; row < 7U; ++row) {
                for (unsigned col = 0U; col < 5U; ++col) {
                    if ((glyphs[glyph][row] & (16U >> col)) != 0U) {
                        (void) tabos_graphics_pixel(&canvas, x + (int32_t) (ch * 6U + col), y + (int32_t) row,
                                                    palette[ink]);
                    }
                }
            }
        }
    }
}

extern unsigned basic_sound_play(int32_t frequency, int32_t milliseconds);

unsigned basic_graphics_command(unsigned token, const int32_t* a, unsigned count, const char* text, unsigned length)
{
    static const uint8_t minimum[] = {0, 0, 0, 1, 2, 4, 4, 3, 0, 3, 3, 3, 1, 1, 1, 2, 3};
    static const uint8_t maximum[] = {0, 0, 1, 1, 2, 4, 5, 4, 0, 3, 3, 3, 1, 1, 1, 2, 3};
    if (token < BASIC_GRAPHICS || token >= BASIC_KEY || count < minimum[token - BASIC_GRAPHICS] ||
        count > maximum[token - BASIC_GRAPHICS]) {
        return 11U;
    }
    if (token == BASIC_GRAPHICS) {
        basic_c64_close();
        if (!canvas.open) {
            canvas = (tabos_graphics_t) {.width = WIDTH, .height = HEIGHT};
            if (tabos_graphics_open(&canvas) != 0) {
                basic_graphics_close();
                return 14U;
            }
            ink = 1U;
            memset(sprites, 0, sizeof(sprites));
            (void) tabos_graphics_clear(&canvas, palette[0]);
        }
        return present() == 0 ? 0U : 14U;
    }
    if (token == BASIC_TEXT) {
        basic_c64_close();
        basic_graphics_close();
        return 0U;
    }
    if (token == BASIC_SLEEP) {
        if (a[0] < 0 || a[0] > 5000) {
            return 14U;
        }
        (void) basic_runtime_sleep((uint32_t) a[0]);
        return 0U;
    }
    if (token == BASIC_SOUND) {
        return basic_sound_play(a[0], a[1]);
    }
    if (token == BASIC_COLOR) {
        if (a[0] < 0 || a[0] > 15) {
            return 14U;
        }
        ink = (unsigned) a[0];
        return 0U;
    }
    if (!canvas.open) {
        return 14U;
    }
    if (token == BASIC_PSET || token == BASIC_LINE || token == BASIC_RECT || token == BASIC_CIRCLE ||
        token == BASIC_GTEXT) {
        if (a[0] < -1024 || a[0] > 1024 || a[1] < -1024 || a[1] > 1024 ||
            (token == BASIC_LINE && (a[2] < -1024 || a[2] > 1024 || a[3] < -1024 || a[3] > 1024))) {
            return 14U;
        }
    }
    int result = 0;
    switch (token) {
        case BASIC_CLS:
            if (count != 0U && (a[0] < 0 || a[0] > 15)) {
                return 14U;
            }
            result = tabos_graphics_clear(&canvas, palette[count == 0U ? 0U : (unsigned) a[0]]);
            break;
        case BASIC_PSET: result = tabos_graphics_pixel(&canvas, a[0], a[1], palette[ink]); break;
        case BASIC_LINE: result = tabos_graphics_line(&canvas, a[0], a[1], a[2], a[3], palette[ink]); break;
        case BASIC_RECT:
            if (a[2] < 0 || a[3] < 0 || a[2] > 1024 || a[3] > 1024 || (count == 5U && a[4] != 0 && a[4] != 1)) {
                return 14U;
            }
            if (count == 5U && a[4] != 0) {
                result = tabos_graphics_fill_rect(&canvas, a[0], a[1], (unsigned) a[2], (unsigned) a[3], palette[ink]);
            } else {
                result = tabos_graphics_rect(&canvas, a[0], a[1], (unsigned) a[2], (unsigned) a[3], palette[ink]);
            }
            break;
        case BASIC_CIRCLE:
            if (a[2] < 0 || a[2] > 512 || (count == 4U && a[3] != 0 && a[3] != 1)) {
                return 14U;
            }
            result = circle(a[0], a[1], a[2], count == 4U && a[3] != 0);
            break;
        case BASIC_PRESENT: result = present(); break;
        case BASIC_GTEXT: text_at(a[0], a[1], text, length); break;
        case BASIC_SPRITE:
        case BASIC_SPRITEROW:
        case BASIC_SPRITEPOS:
        case BASIC_SPRITESHOW:
        case BASIC_SPRITEHIDE: {
            if (a[0] < 0 || a[0] >= SPRITES) {
                return 14U;
            }
            sprite_t* sprite = &sprites[a[0]];
            if (token == BASIC_SPRITE) {
                if (a[1] < 1 || a[1] > SIDE || a[2] < 1 || a[2] > SIDE) {
                    return 14U;
                }
                memset(sprite, 0, sizeof(*sprite));
                sprite->width  = (unsigned) a[1];
                sprite->height = (unsigned) a[2];
                for (unsigned i = 0U; i < SIDE * SIDE; ++i) {
                    sprite->image[i] = TRANSPARENT;
                }
            } else if (sprite->width == 0U) {
                return 14U;
            } else if (token == BASIC_SPRITEPOS) {
                sprite->x = a[1];
                sprite->y = a[2];
            } else if (token == BASIC_SPRITEROW) {
                if (a[1] < 0 || (unsigned) a[1] >= sprite->height || length != sprite->width) {
                    return 14U;
                }
                tabos_color_t row[SIDE];
                for (unsigned i = 0U; i < length; ++i) {
                    const unsigned char c = (unsigned char) text[i];
                    if (c == '.') {
                        row[i] = TRANSPARENT;
                    } else if (c >= '0' && c <= '9') {
                        row[i] = palette[c - '0'];
                    } else if (c >= 'A' && c <= 'F') {
                        row[i] = palette[c - 'A' + 10U];
                    } else if (c >= 'a' && c <= 'f') {
                        row[i] = palette[c - 'a' + 10U];
                    } else {
                        return 14U;
                    }
                }
                memcpy(sprite->image + (unsigned) a[1] * SIDE, row, length * sizeof(row[0]));
            } else {
                sprite->visible = token == BASIC_SPRITESHOW;
            }
            break;
        }
        default: return 11U;
    }
    return result == 0 ? 0U : 14U;
}
