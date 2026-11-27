/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Beach 909 sunset: 120x56 logical pixels, painted at 2x on the LCD.
 * Integer only and stateless, so the same pixels come out on the device and
 * in the browser preview. `phase` advances the water; `pulse` swells the sun. */
/* The Terms Of Service mark is the act's logo, redrawn here as the sun. */
#include "beach_art.inc"

#define BEACH_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
enum { BEACH_W = 120, BEACH_H = 56, BEACH_HORIZON = 34, BEACH_SUN_X = 76, BEACH_SUN_Y = 20 };

static unsigned beach_hash(unsigned x, unsigned y)
{
    unsigned h = x * 374761393U + y * 668265263U;
    h = (h ^ (h >> 13)) * 1274126177U;
    return h ^ (h >> 16);
}
/* Top of the sand at column x; BEACH_H where there is no sand. */
static unsigned beach_sand_top(unsigned x)
{
    unsigned y = 47U + x * x / 300U;
    return y < BEACH_H ? y : BEACH_H;
}
static uint16_t beach_scene_pixel(unsigned x, unsigned y, unsigned phase, unsigned pulse, uint16_t mark)
{
    static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    static const uint16_t sky[7] = {
        BEACH_RGB(22, 12, 58),  BEACH_RGB(58, 22, 98),   BEACH_RGB(122, 34, 120), BEACH_RGB(196, 52, 116),
        BEACH_RGB(240, 96, 84), BEACH_RGB(252, 150, 60), BEACH_RGB(255, 206, 110)};
    static const uint16_t sea[4] = {
        BEACH_RGB(112, 44, 116), BEACH_RGB(72, 40, 120), BEACH_RGB(40, 36, 108), BEACH_RGB(18, 22, 70)};
    /* Thin evening clouds: row, first column, length. */
    static const uint8_t clouds[][3] = {
        {9, 84, 20}, {10, 78, 34}, {11, 90, 16}, {17, 40, 18}, {18, 34, 30}, {19, 44, 12},
        {23, 96, 20}, {24, 92, 26}, {26, 2, 16}, {27, 0, 22}};

    if ((beach_palms[y][x >> 3] >> (x & 7U)) & 1U) return BEACH_RGB(14, 8, 30);
    unsigned sand = beach_sand_top(x);
    if (y >= sand) {
        if (y == sand) return BEACH_RGB(206, 120, 110);
        return (beach_hash(x, y) & 7U) ? BEACH_RGB(70, 34, 70) : BEACH_RGB(104, 54, 84);
    }
    if (y < BEACH_HORIZON) {
        int dx = (int)x - BEACH_SUN_X, dy = (int)y - BEACH_SUN_Y, r = pulse ? 14 : 13;
        if (dx * dx + dy * dy <= r * r) {
            /* The sun is the Terms Of Service mark: a disc with a square pulse.
             * `mark` is the pulse colour, set by whichever drum is sounding. */
            int ax = dx < 0 ? -dx : dx;
            if ((dy >= -4 && dy <= -2 && ax <= 5) || (dy >= -4 && dy <= 4 && ax >= 3 && ax <= 5) ||
                (dy >= 2 && dy <= 4 && ax >= 3 && ax <= 10))
                return mark;
            return dy < -6 ? BEACH_RGB(255, 244, 170) : dy < 0 ? BEACH_RGB(255, 208, 96)
                 : dy < 6 ? BEACH_RGB(255, 150, 70) : BEACH_RGB(255, 100, 110);
        }
        if (pulse && dx * dx + dy * dy <= 17 * 17 && ((x ^ y) & 1U)) return BEACH_RGB(255, 190, 110);
        for (unsigned i = 0; i < sizeof clouds / sizeof clouds[0]; ++i)
            if (y == clouds[i][0] && x >= clouds[i][1] && x < (unsigned)clouds[i][1] + clouds[i][2])
                return y < 14 ? BEACH_RGB(150, 64, 140) : y < 21 ? BEACH_RGB(238, 110, 130) : BEACH_RGB(255, 176, 100);
        if (y < 13) {
            unsigned h = beach_hash(x, y);
            if (h % 61U == 0 && ((h >> 8) + phase / 4U) % 5U) return BEACH_RGB(255, 230, 220);
        }
        unsigned position = y * 6U * 16U / BEACH_HORIZON, band = position >> 4;
        return sky[band + ((position & 15U) > bayer[y & 3U][x & 3U])];
    }
    unsigned depth = y - BEACH_HORIZON;
    /* Foam washing along the sand. */
    if (sand < BEACH_H && y + 1U == sand && ((x + phase) & 7U) < 5U) return BEACH_RGB(255, 232, 226);
    if (sand < BEACH_H && y + 2U == sand && ((x + phase + 3U) & 7U) < 2U) return BEACH_RGB(236, 170, 190);
    /* Sun path: broken glints that narrow toward the shore. */
    int offset = (int)x - BEACH_SUN_X;
    unsigned reach = depth < 4 ? 13U - depth : 10U - depth / 3U;
    if ((unsigned)(offset < 0 ? -offset : offset) <= reach) {
        unsigned shimmer = beach_hash(x / 2U + (depth & 1U ? phase : 0U - phase) / 2U, depth);
        if (shimmer & 3U)
            return depth < 3 ? BEACH_RGB(255, 222, 130) : depth < 9 ? BEACH_RGB(255, 160, 80)
                 : BEACH_RGB(240, 96, 100);
    }
    /* Rolling swells: dashes drift sideways, faster and longer near the shore. */
    if (depth % 3U == 1U) {
        unsigned drift = x + 200U - phase * (1U + depth / 6U) / 2U + depth * 11U;
        if (drift % 26U < 3U + depth / 3U) return depth < 8 ? BEACH_RGB(200, 110, 160) : BEACH_RGB(92, 108, 200);
    }
    unsigned position = depth * 3U * 16U / (BEACH_H - BEACH_HORIZON), band = position >> 4;
    return sea[band + ((position & 15U) > bayer[y & 3U][x & 3U])];
}
