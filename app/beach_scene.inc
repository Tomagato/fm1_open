/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Beach 909 backdrop: deep space, 120x56 logical pixels painted at 2x on the
 * LCD. Integer only and stateless, so the same pixels come out on the device
 * and in the browser preview. `phase` drifts the stars; `pulse` swells the
 * star that carries the Terms Of Service mark, the act's logo. */

#define BEACH_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
enum { BEACH_W = 120, BEACH_H = 56, BEACH_HORIZON = 34, BEACH_SUN_X = 78, BEACH_SUN_Y = 20,
       BEACH_PLANET_X = 24, BEACH_PLANET_Y = 36 };

static unsigned beach_hash(unsigned x, unsigned y)
{
    unsigned h = x * 374761393U + y * 668265263U;
    h = (h ^ (h >> 13)) * 1274126177U;
    return h ^ (h >> 16);
}
static uint16_t beach_scene_pixel(unsigned x, unsigned y, unsigned phase, unsigned pulse, uint16_t mark)
{
    static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    static const uint16_t space[5] = {
        BEACH_RGB(4, 2, 14), BEACH_RGB(10, 5, 32), BEACH_RGB(22, 9, 54), BEACH_RGB(44, 14, 80), BEACH_RGB(76, 22, 104)};
    unsigned dither = bayer[y & 3U][x & 3U];

    /* Moon surface along the bottom edge, with a lit rim and craters. */
    int centre = (int)x - 60;
    unsigned ground = 50U + (unsigned)(centre * centre) / 800U;
    if (y >= ground) {
        if (y == ground) return BEACH_RGB(206, 190, 230);
        unsigned crater = beach_hash(x / 3U, y / 2U) % 11U;
        return crater == 0 ? BEACH_RGB(34, 26, 60) : crater < 4 ? BEACH_RGB(92, 80, 128) : BEACH_RGB(70, 58, 104);
    }
    /* Ringed planet. The ring passes behind the upper half and in front of the lower. */
    int px = (int)x - BEACH_PLANET_X, py = (int)y - BEACH_PLANET_Y;
    unsigned in_planet = px * px + py * py <= 11 * 11;
    unsigned in_ring = px * px * 16 + py * py * 361 <= 361 * 16 && px * px * 9 + py * py * 196 > 196 * 9;
    if (in_ring && (!in_planet || py > 0)) return (px + 20) % 5 ? BEACH_RGB(226, 196, 150) : BEACH_RGB(150, 120, 110);
    if (in_planet) {
        if (px + py > 8) return BEACH_RGB(40, 18, 60); /* night side */
        unsigned band = (unsigned)(py + 11 + (px > 2 ? 1 : 0)) % 6U;
        return band < 2 ? BEACH_RGB(250, 150, 90) : band < 4 ? BEACH_RGB(214, 90, 110) : BEACH_RGB(150, 70, 150);
    }
    /* The star with the Terms Of Service mark: a disc with a square pulse.
     * `mark` is the pulse colour, set by whichever drum is sounding. */
    int dx = (int)x - BEACH_SUN_X, dy = (int)y - BEACH_SUN_Y, r = pulse ? 14 : 13;
    if (dx * dx + dy * dy <= r * r) {
        int ax = dx < 0 ? -dx : dx;
        if ((dy >= -4 && dy <= -2 && ax <= 5) || (dy >= -4 && dy <= 4 && ax >= 3 && ax <= 5) ||
            (dy >= 2 && dy <= 4 && ax >= 3 && ax <= 10))
            return mark;
        return dx + dy < -9 ? BEACH_RGB(255, 120, 110) : dx + dy < 8 ? BEACH_RGB(209, 21, 33) : BEACH_RGB(140, 12, 44);
    }
    if (dx * dx + dy * dy <= (pulse ? 19 * 19 : 16 * 16) && ((x ^ y) & 1U)) return BEACH_RGB(150, 20, 60);
    /* Shooting star. */
    unsigned flight = phase % 60U;
    if (flight < 9) {
        int hx = 112 - (int)flight * 7, tail = (int)x - hx;
        if (tail >= 0 && tail < 8 && (int)y == 5 + (int)flight * 2 - tail / 3)
            return tail < 2 ? BEACH_RGB(255, 255, 255) : BEACH_RGB(150, 190, 255);
    }
    /* Three star layers drift at different speeds. */
    for (unsigned layer = 0; layer < 3; ++layer) {
        unsigned h = beach_hash((x + phase * (layer + 1U) / 4U) % 240U + layer * 977U, y);
        if (h % (layer ? 113U : 71U) == 0 && ((h >> 9) + phase / 3U) % 6U)
            return layer == 2 ? BEACH_RGB(255, 255, 255) : layer ? BEACH_RGB(170, 200, 255) : BEACH_RGB(110, 100, 170);
    }
    /* Two nebulae: dithered so the dark shows through their edges. */
    int ex = (int)x - 34, ey = (int)y - 14, fx = (int)x - 104, fy = (int)y - 38;
    int near = (ex * ex * 100 + ey * ey * 729) * 16 / (729 * 100), far = (fx * fx * 81 + fy * fy * 484) * 16 / (484 * 81);
    unsigned grain = beach_hash(x / 2U, y / 2U) & 3U;
    if (near + (int)dither / 2 + (int)grain < 14) return near < 5 ? BEACH_RGB(190, 60, 170) : BEACH_RGB(96, 32, 132);
    if (far + (int)dither / 2 + (int)grain < 14) return far < 5 ? BEACH_RGB(60, 170, 190) : BEACH_RGB(30, 80, 130);
    unsigned position = y * 4U * 16U / BEACH_H, level = position >> 4;
    return space[level + ((position & 15U) > dither)];
}
