#include <basic/c64.h>
#include <basic/graphics.h>
#include <basic/runtime.h>
#include <basic/sid.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <tabos/graphics.h>
#include <tabos/runtime_time.h>

enum {
    C64_BANK_BYTES    = 16384,
    C64_COLOR_BYTES   = 1000,
    C64_SCREEN_START  = 0x0400,
    C64_SCREEN_END    = 0x07e7,
    C64_POINTER_START = 0x07f8,
    C64_POINTER_END   = 0x07ff,
    C64_COLOR_START   = 0xd800,
    C64_COLOR_END     = 0xdbe7,
    C64_WIDTH         = 320,
    C64_HEIGHT        = 200,
    C64_SCREEN_X      = 20,
    C64_SCREEN_Y      = 12,
    C64_CELL_WIDTH    = 7,
    C64_CELL_HEIGHT   = 7,
    C64_SPRITES       = 8,
    C64_REFRESH_MS    = 16,
    C64_WRITE_BATCH   = 128,
    C64_LOCAL_BATCH   = 8,
    C64_SPRITE_BATCH  = 4,
    C64_DIRTY_BYTES   = (C64_COLOR_BYTES + 7) / 8
};

typedef struct {
        uint8_t bank[C64_BANK_BYTES];
        uint8_t color[C64_COLOR_BYTES];
        uint8_t sprite_x[C64_SPRITES];
        uint8_t sprite_y[C64_SPRITES];
        uint8_t sprite_x_msb;
        uint8_t sprite_enable;
        uint8_t sprite_y_expand;
        uint8_t sprite_multicolor;
        uint8_t sprite_x_expand;
        uint8_t border;
        uint8_t background;
        uint8_t multicolor_0;
        uint8_t multicolor_1;
        uint8_t sprite_color[C64_SPRITES];
        uint32_t dirty_writes;
        uint64_t last_present;
        uint8_t dirty_cells[C64_DIRTY_BYTES];
        uint16_t dirty_cell_count;
        int16_t sprite_dirty_x0;
        int16_t sprite_dirty_y0;
        int16_t sprite_dirty_x1;
        int16_t sprite_dirty_y1;
        bool dirty;
        bool full_dirty;
        bool background_dirty;
        bool border_dirty;
        bool sprite_dirty;
        tabos_graphics_t canvas;
} c64_state_t;

typedef struct {
        uint64_t started;
        uint64_t render_ms;
        uint64_t present_ms;
        uint64_t max_render_ms;
        uint64_t max_present_ms;
        uint32_t writes;
        uint32_t redraws;
        uint32_t presents;
        uint32_t skipped;
        uint32_t forced;
        uint32_t presented_writes;
        uint32_t max_writes;
        uint32_t full_redraws;
        uint32_t partial_redraws;
        bool enabled;
        bool active;
} c64_profile_t;

static c64_state_t state;
static c64_profile_t profile;

// Synthetic 5x7 replacement glyphs authored for TabBASIC Stage H. These are
// digits, uppercase Latin, colon, hyphen and a question-mark fallback; no C64
// character-ROM data is included.
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

static const tabos_color_t palette[16] = {
    TABOS_RGB565(0, 0, 0),       TABOS_RGB565(255, 255, 255), TABOS_RGB565(136, 0, 0),     TABOS_RGB565(170, 255, 238),
    TABOS_RGB565(204, 68, 204),  TABOS_RGB565(0, 204, 85),    TABOS_RGB565(0, 0, 170),     TABOS_RGB565(238, 238, 119),
    TABOS_RGB565(221, 136, 85),  TABOS_RGB565(102, 68, 0),    TABOS_RGB565(255, 119, 119), TABOS_RGB565(51, 51, 51),
    TABOS_RGB565(119, 119, 119), TABOS_RGB565(170, 255, 102), TABOS_RGB565(0, 136, 255),   TABOS_RGB565(187, 187, 187)};

static void unsupported(uint16_t address)
{
    char message[48];
    (void) snprintf(message, sizeof(message), "\n?UNSUPPORTED C64 ADDRESS $%04X\n", (unsigned) address);
    basic_runtime_write(message);
}

static void mark_cell(unsigned cell)
{
    const uint8_t mask = (uint8_t) (1U << (cell & 7U));
    if ((state.dirty_cells[cell / 8U] & mask) == 0U) {
        state.dirty_cells[cell / 8U] |= mask;
        ++state.dirty_cell_count;
    }
    state.dirty = true;
}

static bool mark_sprite(unsigned sprite)
{
    if ((state.sprite_enable & (1U << sprite)) == 0U) {
        return false;
    }
    const int32_t x      = state.sprite_x[sprite] + ((state.sprite_x_msb & (1U << sprite)) != 0U ? 256 : 0);
    const int32_t y      = state.sprite_y[sprite];
    const int32_t width  = (state.sprite_x_expand & (1U << sprite)) != 0U ? 48 : 24;
    const int32_t height = (state.sprite_y_expand & (1U << sprite)) != 0U ? 42 : 21;
    int32_t x0           = x < 0 ? 0 : x;
    int32_t y0           = y < 0 ? 0 : y;
    int32_t x1           = x + width > C64_WIDTH ? C64_WIDTH : x + width;
    int32_t y1           = y + height > C64_HEIGHT ? C64_HEIGHT : y + height;
    if (x0 >= x1 || y0 >= y1) {
        return false;
    }
    if (!state.sprite_dirty) {
        state.sprite_dirty_x0 = (int16_t) x0;
        state.sprite_dirty_y0 = (int16_t) y0;
        state.sprite_dirty_x1 = (int16_t) x1;
        state.sprite_dirty_y1 = (int16_t) y1;
    } else {
        if (x0 < state.sprite_dirty_x0) {
            state.sprite_dirty_x0 = (int16_t) x0;
        }
        if (y0 < state.sprite_dirty_y0) {
            state.sprite_dirty_y0 = (int16_t) y0;
        }
        if (x1 > state.sprite_dirty_x1) {
            state.sprite_dirty_x1 = (int16_t) x1;
        }
        if (y1 > state.sprite_dirty_y1) {
            state.sprite_dirty_y1 = (int16_t) y1;
        }
    }
    state.sprite_dirty = true;
    state.dirty        = true;
    return true;
}

static bool mark_all_sprites(void)
{
    bool marked = false;
    for (unsigned sprite = 0U; sprite < C64_SPRITES; ++sprite) {
        marked = mark_sprite(sprite) || marked;
    }
    return marked;
}

static bool mark_bank_effect(uint16_t address)
{
    if (address >= C64_SCREEN_START && address <= C64_SCREEN_END) {
        mark_cell(address - C64_SCREEN_START);
        return true;
    }
    if (address >= C64_POINTER_START && address <= C64_POINTER_END) {
        return mark_sprite(address - C64_POINTER_START);
    }
    bool marked = false;
    for (unsigned sprite = 0U; sprite < C64_SPRITES; ++sprite) {
        const unsigned base = state.bank[C64_POINTER_START + sprite] * 64U;
        if (address >= base && address < base + 63U) {
            marked = mark_sprite(sprite) || marked;
        }
    }
    return marked;
}

static bool mark_register_effect(uint16_t address)
{
    if (address >= 0xd000U && address <= 0xd00fU) {
        return mark_sprite((address - 0xd000U) / 2U);
    }
    if (address == 0xd020U) {
        state.border_dirty = true;
        state.dirty        = true;
        return true;
    }
    if (address == 0xd021U) {
        state.background_dirty = true;
        state.dirty            = true;
        return true;
    }
    if (address >= 0xd027U && address <= 0xd02eU) {
        return mark_sprite(address - 0xd027U);
    }
    switch (address) {
        case 0xd010:
        case 0xd015:
        case 0xd017:
        case 0xd01c:
        case 0xd01d:
        case 0xd025:
        case 0xd026: return mark_all_sprites();
        default: return false;
    }
}

static bool register_read(uint16_t address, uint8_t* value)
{
    if (address >= 0xd000U && address <= 0xd00fU) {
        const unsigned sprite = (address - 0xd000U) / 2U;
        *value                = (address & 1U) == 0U ? state.sprite_x[sprite] : state.sprite_y[sprite];
        return true;
    }
    switch (address) {
        case 0xd010: *value = state.sprite_x_msb; break;
        case 0xd015: *value = state.sprite_enable; break;
        case 0xd017: *value = state.sprite_y_expand; break;
        case 0xd01c: *value = state.sprite_multicolor; break;
        case 0xd01d: *value = state.sprite_x_expand; break;
        case 0xd020: *value = state.border; break;
        case 0xd021: *value = state.background; break;
        case 0xd025: *value = state.multicolor_0; break;
        case 0xd026: *value = state.multicolor_1; break;
        default:
            if (address >= 0xd027U && address <= 0xd02eU) {
                *value = state.sprite_color[address - 0xd027U];
            } else {
                return false;
            }
            break;
    }
    return true;
}

static bool register_write(uint16_t address, uint8_t value)
{
    if (address >= 0xd000U && address <= 0xd00fU) {
        const unsigned sprite = (address - 0xd000U) / 2U;
        if ((address & 1U) == 0U) {
            state.sprite_x[sprite] = value;
        } else {
            state.sprite_y[sprite] = value;
        }
        return true;
    }
    switch (address) {
        case 0xd010: state.sprite_x_msb = value; break;
        case 0xd015: state.sprite_enable = value; break;
        case 0xd017: state.sprite_y_expand = value; break;
        case 0xd01c: state.sprite_multicolor = value; break;
        case 0xd01d: state.sprite_x_expand = value; break;
        case 0xd020: state.border = value & 15U; break;
        case 0xd021: state.background = value & 15U; break;
        case 0xd025: state.multicolor_0 = value & 15U; break;
        case 0xd026: state.multicolor_1 = value & 15U; break;
        default:
            if (address >= 0xd027U && address <= 0xd02eU) {
                state.sprite_color[address - 0xd027U] = value & 15U;
            } else {
                return false;
            }
            break;
    }
    return true;
}

bool basic_c64_active(void)
{
    return state.canvas.open;
}

void basic_c64_set_profile(bool enabled)
{
    memset(&profile, 0, sizeof(profile));
    profile.enabled = enabled;
}

static void report_profile(void)
{
    if (!profile.enabled || !profile.active) {
        return;
    }
    const uint64_t elapsed = tabos_monotonic_ms() - profile.started;
    const uint32_t average = profile.redraws == 0U ? 0U : profile.presented_writes / profile.redraws;
    char message[384];
    (void) snprintf(message, sizeof(message),
                    "C64 PROFILE writes=%u redraws=%u presents=%u\n"
                    "C64 PROFILE skipped=%u forced=%u avg_writes=%u max_writes=%u\n"
                    "C64 PROFILE elapsed_ms=%llu render_ms=%llu present_ms=%llu\n"
                    "C64 PROFILE max_render_ms=%llu max_present_ms=%llu full=%u partial=%u\n",
                    profile.writes, profile.redraws, profile.presents, profile.skipped, profile.forced, average,
                    profile.max_writes, (unsigned long long) elapsed, (unsigned long long) profile.render_ms,
                    (unsigned long long) profile.present_ms, (unsigned long long) profile.max_render_ms,
                    (unsigned long long) profile.max_present_ms, profile.full_redraws, profile.partial_redraws);
    basic_runtime_write(message);
    const bool enabled = profile.enabled;
    memset(&profile, 0, sizeof(profile));
    profile.enabled = enabled;
}

static void clear_dirty(void)
{
    memset(state.dirty_cells, 0, sizeof(state.dirty_cells));
    state.dirty_cell_count = 0U;
    state.dirty_writes     = 0U;
    state.dirty            = false;
    state.full_dirty       = false;
    state.background_dirty = false;
    state.border_dirty     = false;
    state.sprite_dirty     = false;
}

void basic_c64_close(void)
{
    basic_sid_close();
    if (state.canvas.open) {
        (void) tabos_graphics_close(&state.canvas);
    }
    memset(&state.canvas, 0, sizeof(state.canvas));
    clear_dirty();
    report_profile();
}

void basic_c64_reset(void)
{
    basic_c64_close();
    basic_sid_reset();
    memset(&state, 0, sizeof(state));
    memset(state.bank + C64_SCREEN_START, 32, C64_COLOR_BYTES);
    memset(state.color, 14, sizeof(state.color));
    memset(state.sprite_color, 1, sizeof(state.sprite_color));
    state.border     = 14U;
    state.background = 6U;
}

static bool open_canvas(void)
{
    if (state.canvas.open) {
        return true;
    }
    basic_graphics_close();
    state.canvas = (tabos_graphics_t) {.width = C64_WIDTH, .height = C64_HEIGHT};
    if (tabos_graphics_open(&state.canvas) != 0) {
        memset(&state.canvas, 0, sizeof(state.canvas));
        basic_runtime_write("\n?TABOS C64 GRAPHICS UNAVAILABLE\n");
        return false;
    }
    state.full_dirty = true;
    state.dirty      = true;
    return true;
}

static unsigned glyph_for(uint8_t screen_code)
{
    const unsigned code = screen_code & 0x3fU;
    if (code >= 1U && code <= 26U) {
        return 9U + code;
    }
    if (code >= 48U && code <= 57U) {
        return code - 48U;
    }
    if (code == 27U) {
        return 36U;
    }
    if (code == 29U) {
        return 37U;
    }
    return 38U;
}

static void draw_character(unsigned cell)
{
    const uint8_t screen_code = state.bank[C64_SCREEN_START + cell];
    const bool reverse        = (screen_code & 0x80U) != 0U;
    const unsigned foreground = state.color[cell] & 15U;
    const unsigned background = state.background & 15U;
    const int32_t x           = C64_SCREEN_X + (int32_t) ((cell % 40U) * C64_CELL_WIDTH);
    const int32_t y           = C64_SCREEN_Y + (int32_t) ((cell / 40U) * C64_CELL_HEIGHT);
    const tabos_color_t paper = palette[reverse ? foreground : background];
    const tabos_color_t ink   = palette[reverse ? background : foreground];
    (void) tabos_graphics_fill_rect(&state.canvas, x, y, C64_CELL_WIDTH, C64_CELL_HEIGHT, paper);
    if ((screen_code & 0x7fU) == 32U) {
        return;
    }
    const unsigned glyph = glyph_for(screen_code);
    for (unsigned row = 0U; row < 7U; ++row) {
        for (unsigned column = 0U; column < 5U; ++column) {
            if ((glyphs[glyph][row] & (16U >> column)) != 0U) {
                (void) tabos_graphics_pixel(&state.canvas, x + 1 + (int32_t) column, y + (int32_t) row, ink);
            }
        }
    }
}

static bool draw_characters(void)
{
    for (unsigned cell = 0U; cell < C64_COLOR_BYTES; ++cell) {
        if (cell % 200U == 0U && !basic_runtime_sleep(0U)) {
            return false;
        }
        draw_character(cell);
    }
    return true;
}

static bool draw_dirty_characters(void)
{
    for (unsigned cell = 0U; cell < C64_COLOR_BYTES; ++cell) {
        if ((state.dirty_cells[cell / 8U] & (1U << (cell & 7U))) != 0U) {
            draw_character(cell);
        }
    }
    return basic_runtime_sleep(0U);
}

static bool draw_characters_in_sprite_region(void)
{
    if (!state.sprite_dirty) {
        return true;
    }
    const int32_t first_column =
        state.sprite_dirty_x0 <= C64_SCREEN_X ? 0 : (state.sprite_dirty_x0 - C64_SCREEN_X) / C64_CELL_WIDTH;
    const int32_t last_column =
        state.sprite_dirty_x1 <= C64_SCREEN_X ? -1 : (state.sprite_dirty_x1 - 1 - C64_SCREEN_X) / C64_CELL_WIDTH;
    const int32_t first_row =
        state.sprite_dirty_y0 <= C64_SCREEN_Y ? 0 : (state.sprite_dirty_y0 - C64_SCREEN_Y) / C64_CELL_HEIGHT;
    const int32_t last_row =
        state.sprite_dirty_y1 <= C64_SCREEN_Y ? -1 : (state.sprite_dirty_y1 - 1 - C64_SCREEN_Y) / C64_CELL_HEIGHT;
    for (int32_t row = first_row; row <= last_row && row < 25; ++row) {
        if (row < 0) {
            continue;
        }
        for (int32_t column = first_column; column <= last_column && column < 40; ++column) {
            if (column >= 0) {
                draw_character((unsigned) row * 40U + (unsigned) column);
            }
        }
    }
    return basic_runtime_sleep(0U);
}

static void sprite_pixel(int32_t x, int32_t y, unsigned scale_x, unsigned scale_y, tabos_color_t color)
{
    (void) tabos_graphics_fill_rect(&state.canvas, x, y, scale_x, scale_y, color);
}

static bool draw_sprites(void)
{
    for (unsigned reverse = C64_SPRITES; reverse > 0U; --reverse) {
        const unsigned sprite = reverse - 1U;
        if ((state.sprite_enable & (1U << sprite)) == 0U) {
            continue;
        }
        if (!basic_runtime_sleep(0U)) {
            return false;
        }
        const unsigned pointer = state.bank[C64_POINTER_START + sprite];
        const unsigned base    = pointer * 64U;
        const unsigned scale_x = (state.sprite_x_expand & (1U << sprite)) != 0U ? 2U : 1U;
        const unsigned scale_y = (state.sprite_y_expand & (1U << sprite)) != 0U ? 2U : 1U;
        const int32_t origin_x = state.sprite_x[sprite] + ((state.sprite_x_msb & (1U << sprite)) != 0U ? 256 : 0);
        const int32_t origin_y = state.sprite_y[sprite];
        const bool multicolor  = (state.sprite_multicolor & (1U << sprite)) != 0U;
        for (unsigned row = 0U; row < 21U; ++row) {
            const uint32_t bits = (uint32_t) state.bank[base + row * 3U] << 16U |
                                  (uint32_t) state.bank[base + row * 3U + 1U] << 8U | state.bank[base + row * 3U + 2U];
            if (multicolor) {
                for (unsigned pair = 0U; pair < 12U; ++pair) {
                    const unsigned value = (bits >> (22U - pair * 2U)) & 3U;
                    if (value != 0U) {
                        unsigned color = state.sprite_color[sprite] & 15U;
                        if (value == 1U) {
                            color = state.multicolor_0 & 15U;
                        } else if (value == 3U) {
                            color = state.multicolor_1 & 15U;
                        }
                        sprite_pixel(origin_x + (int32_t) (pair * 2U * scale_x), origin_y + (int32_t) (row * scale_y),
                                     2U * scale_x, scale_y, palette[color]);
                    }
                }
            } else {
                for (unsigned column = 0U; column < 24U; ++column) {
                    if ((bits & (1U << (23U - column))) != 0U) {
                        sprite_pixel(origin_x + (int32_t) (column * scale_x), origin_y + (int32_t) (row * scale_y),
                                     scale_x, scale_y, palette[state.sprite_color[sprite] & 15U]);
                    }
                }
            }
        }
    }
    return true;
}

static void draw_border(void)
{
    const tabos_color_t color = palette[state.border & 15U];
    (void) tabos_graphics_fill_rect(&state.canvas, 0, 0, C64_WIDTH, C64_SCREEN_Y, color);
    (void) tabos_graphics_fill_rect(&state.canvas, 0, C64_SCREEN_Y + 25 * C64_CELL_HEIGHT, C64_WIDTH,
                                    C64_HEIGHT - C64_SCREEN_Y - 25 * C64_CELL_HEIGHT, color);
    (void) tabos_graphics_fill_rect(&state.canvas, 0, C64_SCREEN_Y, C64_SCREEN_X, 25 * C64_CELL_HEIGHT, color);
    (void) tabos_graphics_fill_rect(&state.canvas, C64_SCREEN_X + 40 * C64_CELL_WIDTH, C64_SCREEN_Y,
                                    C64_WIDTH - C64_SCREEN_X - 40 * C64_CELL_WIDTH, 25 * C64_CELL_HEIGHT, color);
}

static bool render(void)
{
    if (!open_canvas()) {
        return false;
    }
    const uint64_t render_started = tabos_monotonic_ms();
    if (state.full_dirty) {
        if (tabos_graphics_clear(&state.canvas, palette[state.border & 15U]) != 0 || !draw_characters()) {
            return false;
        }
    } else {
        if (state.sprite_dirty) {
            (void) tabos_graphics_fill_rect(&state.canvas, state.sprite_dirty_x0, state.sprite_dirty_y0,
                                            state.sprite_dirty_x1 - state.sprite_dirty_x0,
                                            state.sprite_dirty_y1 - state.sprite_dirty_y0, palette[state.border & 15U]);
            if (!draw_characters_in_sprite_region()) {
                return false;
            }
        }
        if (state.border_dirty) {
            draw_border();
        }
        if (state.background_dirty) {
            if (!draw_characters()) {
                return false;
            }
        } else if (!draw_dirty_characters()) {
            return false;
        }
    }
    if (!draw_sprites()) {
        return false;
    }
    const uint64_t present_started = tabos_monotonic_ms();
    const uint64_t render_elapsed  = present_started - render_started;
    const bool presented           = tabos_graphics_present(&state.canvas) == 0;
    const uint64_t present_elapsed = tabos_monotonic_ms() - present_started;
    if (profile.enabled) {
        if (state.full_dirty) {
            ++profile.full_redraws;
        } else {
            ++profile.partial_redraws;
        }
        profile.render_ms  += render_elapsed;
        profile.present_ms += present_elapsed;
        if (render_elapsed > profile.max_render_ms) {
            profile.max_render_ms = render_elapsed;
        }
        if (present_elapsed > profile.max_present_ms) {
            profile.max_present_ms = present_elapsed;
        }
        if (presented) {
            ++profile.presents;
        }
    }
    return presented;
}

void basic_c64_service(bool force)
{
    basic_sid_service();
    if (!state.dirty) {
        return;
    }
    const uint64_t now     = tabos_monotonic_ms();
    const bool local_cells = state.dirty_cell_count == 1U;
    const bool batch_ready = state.dirty_writes >= C64_WRITE_BATCH ||
                             (state.sprite_dirty && state.dirty_writes >= C64_SPRITE_BATCH) ||
                             (local_cells && state.dirty_writes >= C64_LOCAL_BATCH);
    if (!force && !batch_ready && now - state.last_present < C64_REFRESH_MS) {
        if (profile.enabled) {
            ++profile.skipped;
        }
        return;
    }
    if (profile.enabled) {
        ++profile.redraws;
        profile.presented_writes += state.dirty_writes;
        if (state.dirty_writes > profile.max_writes) {
            profile.max_writes = state.dirty_writes;
        }
        if (force) {
            ++profile.forced;
        }
    }
    if (render()) {
        state.last_present = tabos_monotonic_ms();
        clear_dirty();
    }
}

bool basic_c64_read(uint16_t address, uint8_t* value)
{
    if (value == NULL) {
        return false;
    }
    if (address < C64_BANK_BYTES) {
        *value = state.bank[address];
        return true;
    }
    if (address >= C64_COLOR_START && address <= C64_COLOR_END) {
        *value = state.color[address - C64_COLOR_START] & 15U;
        return true;
    }
    if (basic_sid_read(address, value)) {
        return true;
    }
    if (register_read(address, value)) {
        return true;
    }
    unsupported(address);
    return false;
}

bool basic_c64_write(uint16_t address, uint8_t value)
{
    if (address >= 0xd400U && address <= 0xd418U) {
        return basic_sid_write(address, value);
    }
    uint8_t ignored;
    if (address >= C64_BANK_BYTES && (address < C64_COLOR_START || address > C64_COLOR_END) &&
        !register_read(address, &ignored)) {
        unsupported(address);
        return false;
    }
    const bool opened = !state.canvas.open;
    if (!open_canvas()) {
        return false;
    }
    if (profile.enabled) {
        if (!profile.active) {
            profile.active  = true;
            profile.started = tabos_monotonic_ms();
        }
        ++profile.writes;
    }
    bool visible_write = opened;
    if (address < C64_BANK_BYTES) {
        visible_write       = mark_bank_effect(address) || visible_write;
        state.bank[address] = value;
        visible_write       = mark_bank_effect(address) || visible_write;
    } else if (address >= C64_COLOR_START && address <= C64_COLOR_END) {
        state.color[address - C64_COLOR_START] = value & 15U;
        mark_cell(address - C64_COLOR_START);
        visible_write = true;
    } else {
        visible_write = mark_register_effect(address) || visible_write;
        (void) register_write(address, value);
        visible_write = mark_register_effect(address) || visible_write;
    }
    if (visible_write && state.dirty_writes != UINT32_MAX) {
        ++state.dirty_writes;
    }
    basic_c64_service(false);
    return true;
}
