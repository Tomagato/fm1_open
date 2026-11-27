/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Bitch909 backdrop: deep space, 120x120 logical pixels painted at 2x on the
 * LCD. Integer only and stateless, so the same pixels come out on the device
 * and in the browser preview. `phase` drifts the stars; `pulse` swells the
 * star that carries the Terms Of Service mark, the act's logo. `picture`, when
 * set, is 120x56 little-endian RGB565 drawn behind the top half of the scene. */

#define BEACH_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
enum { BEACH_W = 120, BEACH_H = 120, BEACH_PICTURE_H = 56, BEACH_SUN_X = 64, BEACH_SUN_Y = 50, BEACH_SUN_R = 16,
       BEACH_VAPE_X = 6, BEACH_VAPE_Y = 98, BEACH_VAPE_TIP_X = 44, BEACH_VAPE_TIP_Y = 89 };

static unsigned beach_hash(unsigned x, unsigned y)
{
    unsigned h = x * 374761393U + y * 668265263U;
    h = (h ^ (h >> 13)) * 1274126177U;
    return h ^ (h >> 16);
}
static uint16_t beach_scene_pixel(unsigned x, unsigned y, unsigned phase, unsigned pulse, uint16_t mark, unsigned glow,
                                  const uint8_t *picture)
{
    static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    static const uint16_t space[5] = {
        BEACH_RGB(4, 2, 14), BEACH_RGB(10, 5, 32), BEACH_RGB(22, 9, 54), BEACH_RGB(44, 14, 80), BEACH_RGB(76, 22, 104)};
    unsigned dither = bayer[y & 3U][x & 3U];

    /* Moon surface along the bottom edge, with a lit rim and craters. */
    int centre = (int)x - 60;
    unsigned ground = 108U + (unsigned)(centre * centre) / 800U;
    if (y >= ground) {
        if (y == ground) return BEACH_RGB(206, 190, 230);
        unsigned crater = beach_hash(x / 3U, y / 2U) % 11U;
        return crater == 0 ? BEACH_RGB(34, 26, 60) : crater < 4 ? BEACH_RGB(92, 80, 128) : BEACH_RGB(70, 58, 104);
    }
    /* An e-cigarette floats where a planet would be, tilted up to the right:
     * light, battery with its button, tank and coil, mouthpiece. `glow` lights
     * the tip and the coil while it is giving off smoke. */
    if (x >= BEACH_VAPE_X && x <= BEACH_VAPE_X + 36) {
        int along = (int)x - BEACH_VAPE_X, across = (int)y - (BEACH_VAPE_Y - along / 4);
        if (along >= 33 ? across >= -2 && across <= 1 : across >= -3 && across <= 2) {
            if (along < 2) return glow ? BEACH_RGB(90, 255, 240) : BEACH_RGB(40, 90, 100);
            if (along >= 33) return across == -2 ? BEACH_RGB(70, 62, 96) : BEACH_RGB(30, 24, 48);
            if (along == 19 || along == 20 || along == 31 || along == 32) return BEACH_RGB(40, 34, 60);
            if (along > 20) {
                if (across == -3) return BEACH_RGB(214, 240, 246);
                if (across == 0 && along >= 24 && along <= 28) return glow ? BEACH_RGB(255, 150, 60) : BEACH_RGB(120, 96, 100);
                return across > 0 ? BEACH_RGB(240, 170, 70) : BEACH_RGB(110, 170, 190);
            }
            if (along >= 8 && along <= 9 && across >= -1 && across <= 0) return BEACH_RGB(232, 36, 48);
            return across == -3 ? BEACH_RGB(244, 246, 252) : across < 0 ? BEACH_RGB(190, 198, 216)
                 : across < 2 ? BEACH_RGB(136, 144, 170) : BEACH_RGB(84, 90, 120);
        }
    }
    /* The star with the Terms Of Service mark: a disc with a square pulse.
     * `mark` is the pulse colour, set by whichever drum is sounding. */
    int dx = (int)x - BEACH_SUN_X, dy = (int)y - BEACH_SUN_Y, r = BEACH_SUN_R + (pulse ? 1 : 0);
    if (dx * dx + dy * dy <= r * r) {
        int ax = dx < 0 ? -dx : dx;
        if ((dy >= -5 && dy <= -3 && ax <= 6) || (dy >= -5 && dy <= 5 && ax >= 4 && ax <= 6) ||
            (dy >= 3 && dy <= 5 && ax >= 4 && ax <= 12))
            return mark;
        return dx + dy < -11 ? BEACH_RGB(255, 120, 110) : dx + dy < 8 ? BEACH_RGB(209, 21, 33) : BEACH_RGB(140, 12, 44);
    }
    if (dx * dx + dy * dy <= (pulse ? 23 * 23 : 20 * 20) && ((x ^ y) & 1U)) return BEACH_RGB(150, 20, 60);
    /* Shooting star. */
    unsigned flight = phase % 60U;
    if (flight < 9) {
        int hx = 112 - (int)flight * 7, tail = (int)x - hx;
        if (tail >= 0 && tail < 8 && (int)y == 5 + (int)flight * 2 - tail / 3)
            return tail < 2 ? BEACH_RGB(255, 255, 255) : BEACH_RGB(150, 190, 255);
    }
    /* Three layers of stars. They twinkle where the picture is being redrawn anyway. */
    for (unsigned layer = picture && y < BEACH_PICTURE_H ? 1 : 0; layer < 3; ++layer) {
        unsigned h = beach_hash(x + layer * 977U, y); /* fixed: a drifting sky repaints the whole scene */
        if (h % (layer ? 113U : 71U) == 0 && ((h >> 9) + phase / 3U) % 6U)
            return layer == 2 ? BEACH_RGB(255, 255, 255) : layer ? BEACH_RGB(170, 200, 255) : BEACH_RGB(110, 100, 170);
    }
    /* A picture sent by the player is the back layer: the star, the e-cigarette,
     * the shooting star and the brighter stars above all play over it. */
    if (picture && y < BEACH_PICTURE_H) {
        const uint8_t *pixel = picture + (y * BEACH_W + x) * 2U;
        return (uint16_t)(pixel[0] | (unsigned)pixel[1] << 8);
    }
    /* Two nebulae: dithered so the dark shows through their edges. */
    int ex = (int)x - 28, ey = (int)y - 18, fx = (int)x - 98, fy = (int)y - 82;
    int near = (ex * ex * 100 + ey * ey * 729) * 16 / (729 * 100), far = (fx * fx * 81 + fy * fy * 484) * 16 / (484 * 81);
    unsigned grain = beach_hash(x / 2U, y / 2U) & 3U;
    if (near + (int)dither / 2 + (int)grain < 14) return near < 5 ? BEACH_RGB(190, 60, 170) : BEACH_RGB(96, 32, 132);
    if (far + (int)dither / 2 + (int)grain < 14) return far < 5 ? BEACH_RGB(60, 170, 190) : BEACH_RGB(30, 80, 130);
    unsigned position = y * 4U * 16U / BEACH_H, level = position >> 4;
    return space[level + ((position & 15U) > dither)];
}
