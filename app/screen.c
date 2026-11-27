/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Bitch909 panel.
 * Modified from FM1 Open V14 app/screen.c, Copyright (c) 2026 M-VAVE.
 * Changed in November 2026 for the Bitch909 firmware: the multi-page
 * instrument UI was replaced by one drum-machine page with a space scene.
 * The band painter, font and input helpers are kept from the original. */
#include "instrument.h"
#include "sampledrum.h"
#include "fm_patch.h"
#include "version.h"
#include "fm1_engine_control.h"
#include "audio_output.h"
#include <string.h>
enum {
    BACK = 0x0000,
    PANEL = 0x18e3,
    TEXT = 0xef5d,
    ACCENT = 0x07ff,
    DIM = 0x738e,
    GREEN = 0x07e0,
    AMBER = 0xfd20
};
/* Five columns per glyph, bit 0 at the top. Digits followed by uppercase. */
static const uint8_t font[36][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0, 0x42, 0x7f, 0x40, 0},       {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e}, {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0, 0x41, 0x7f, 0x41, 0},       {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}};
/* Compose the final pixels before touching the LCD. Sixteen rows cost 7,680
 * bytes; unchanged bands and unchanged horizontal margins are not transferred.
 * A page change never exposes the temporary background clear on the panel. */
enum { PAINT_ROWS = 16, PAINT_BANDS = FM1_SCREEN_HEIGHT / PAINT_ROWS };
_Static_assert(FM1_SCREEN_WIDTH * PAINT_ROWS <= FM1_SCREEN_DMA_PIXELS, "LCD band capacity");
static uint16_t paint_pixels[FM1_SCREEN_WIDTH * PAINT_ROWS];
static uint8_t paint_left[PAINT_BANDS], paint_right[PAINT_BANDS];
static unsigned paint_rendering, paint_top, paint_next, paint_x, paint_end;
static uint32_t paint_priority;
static void paint_mark(unsigned x, unsigned y, unsigned w, unsigned h)
{
    for (unsigned b = y / PAINT_ROWS; b <= (y + h - 1) / PAINT_ROWS; ++b) {
        paint_priority |= 1U << b;
        if (x < paint_left[b]) paint_left[b] = (uint8_t)x;
        if (x + w > paint_right[b]) paint_right[b] = (uint8_t)(x + w);
    }
}
/* Many LCD pages share this bounded fill; keep a single native copy. */
static __attribute__((noinline)) void paint_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{
    if (!w || !h || x >= FM1_SCREEN_WIDTH || y >= FM1_SCREEN_HEIGHT) return;
    if (w > FM1_SCREEN_WIDTH - x) w = FM1_SCREEN_WIDTH - x;
    if (h > FM1_SCREEN_HEIGHT - y) h = FM1_SCREEN_HEIGHT - y;
    if (!paint_rendering) { paint_mark(x, y, w, h); return; }
    if (x >= paint_end || x + w <= paint_x) return;
    unsigned right = x + w < paint_end ? x + w : paint_end;
    if (x < paint_x) x = paint_x;
    unsigned first = y < paint_top ? paint_top : y;
    unsigned end = y + h < paint_top + PAINT_ROWS ? y + h : paint_top + PAINT_ROWS;
    for (unsigned row = first; row < end; ++row)
        for (unsigned col = x; col < right; ++col)
            paint_pixels[(row - paint_top) * (paint_end - paint_x) + col - paint_x] = color;
}
static const uint8_t punctuation[][6] = {
    {45,8,8,8,8,8}, {47,0x60,0x18,6,1,0}, {43,8,8,0x3e,8,8},
    {35,0x14,0x7f,0x14,0x7f,0x14}, {37,0x63,0x13,8,0x64,0x63},
    {62,0x41,0x22,0x14,8,0}, {63,2,1,0x51,9,6},
    {33,0,0,0x5f,0,0}, {34,0,7,0,7,0}, {36,0x24,0x2a,0x7f,0x2a,0x12},
    {38,0x36,0x49,0x55,0x22,0x50}, {39,0,5,3,0,0}, {40,0,0x1c,0x22,0x41,0},
    {41,0,0x41,0x22,0x1c,0}, {42,0x14,8,0x3e,8,0x14}, {44,0,0x50,0x30,0,0},
    {46,0,0x60,0x60,0,0}, {58,0,0x36,0x36,0,0}, {59,0,0x56,0x36,0,0},
    {60,8,0x14,0x22,0x41,0}, {61,0x14,0x14,0x14,0x14,0x14},
    {64,0x32,0x49,0x79,0x41,0x3e}, {91,0,0x7f,0x41,0x41,0},
    {92,1,6,0x18,0x60,0}, {93,0,0x41,0x41,0x7f,0}, {94,4,2,1,2,4},
    {95,0x40,0x40,0x40,0x40,0x40}, {96,0,1,2,4,0}, {123,0,8,0x36,0x41,0},
    {124,0,0,0x7f,0,0}, {125,0,0x41,0x36,8,0}, {126,8,4,8,0x10,8}
};
static void text(unsigned x, unsigned y, const char *s, unsigned scale, uint16_t color,
                 uint16_t background)
{
    if (!s || !scale || scale > 4 || x >= FM1_SCREEN_WIDTH ||
        y >= FM1_SCREEN_HEIGHT || 7U * scale > FM1_SCREEN_HEIGHT - y) return;
    unsigned w = 6U * scale, h = 7U * scale;
    if (paint_rendering && (y >= paint_top + PAINT_ROWS || y + h <= paint_top)) return;
    if (!paint_rendering) {
        unsigned n = (unsigned)strlen(s), available = (FM1_SCREEN_WIDTH - x) / w;
        if (n > available) n = available;
        if (n) paint_mark(x, y, n * w, h);
        return;
    }
    for (; *s && x + w <= FM1_SCREEN_WIDTH; ++s, x += w) {
        if (x >= paint_end) break;
        if (x + w <= paint_x) continue;
        char letter = *s >= 'a' && *s <= 'z' ? (char)(*s - 'a' + 'A') : *s;
        static const uint8_t blank[5] = {0};
        const uint8_t *shape = blank;
        if (*s >= '0' && *s <= '9')
            shape = font[*s - '0'];
        else if (letter >= 'A' && letter <= 'Z')
            shape = font[10 + letter - 'A'];
        else if (*s != ' ')
            for (unsigned i = 0; i < sizeof punctuation / sizeof punctuation[0]; ++i)
                if ((uint8_t)*s == punctuation[i][0]) {
                    shape = punctuation[i] + 1;
                    break;
                }
        unsigned first_row = y < paint_top ? paint_top : y;
        unsigned end_row = y + h < paint_top + PAINT_ROWS ? y + h : paint_top + PAINT_ROWS;
        unsigned first_col = x < paint_x ? paint_x : x;
        unsigned end_col = x + w < paint_end ? x + w : paint_end;
        unsigned stride = paint_end - paint_x;
        for (unsigned row = first_row; row < end_row; ++row) {
            unsigned mask = 1U << ((row - y) / scale);
            uint16_t *pixel = paint_pixels + (row - paint_top) * stride + first_col - paint_x;
            for (unsigned col = first_col; col < end_col; ++col) {
                unsigned column = (col - x) / scale;
                *pixel++ = column < 5U && (shape[column] & mask) ? color : background;
            }
        }
    }
}
/* One page: the keys play a DX7 on track 1, or (HOME) the drum kit on track 8.
 * REC loops what is played; the loop has no grid, only orbits round the star. All drawing and edits run on the control task. */
#include "beach_scene.inc"
enum { OCT_DOWN, OCT_UP, FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC };
#define BUTTON(b) (1U << (b))
enum { SYNTH = 0, KIT = 7, KIT_SLOT = GROOVE_PROJECT_SLOTS - 1, SCENE_ROWS = BEACH_H * 2,
       RED = 0xf9a6, INK = 0x2108, INK_BEAT = 0x39cf, INK_LANE = 0x4a35, HEAD = 0x7bf7 };
static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t pad_keys[8] = {1, 3, 5, 8, 10, 13, 15, 17};
/* Screen and key order: BD SD CH OH CP RS TM CR, mapped onto the kit's pads. */
static const uint8_t lane_pad[8] = {0, 1, 6, 7, 3, 2, 4, 5};
/* Terms Of Service colours: the logo's red, white and blue, and tints of them. */
static const uint16_t lane_color[8] = {
    BEACH_RGB(232, 36, 48),   BEACH_RGB(244, 244, 244), BEACH_RGB(40, 130, 240),  BEACH_RGB(255, 124, 124),
    BEACH_RGB(124, 184, 255), BEACH_RGB(180, 180, 194), BEACH_RGB(255, 88, 40),   BEACH_RGB(204, 230, 255)};
static const char kit_name[] = "BITCH909";
static const char old_kit_name[] = "BEACH 909"; /* what earlier builds called it; their saved loops still load */
enum { PAGE_KIT = 1, PAGE_GAME, PAGE_SPLASH, PAGE_COINS, PAGE_KNIVES, GAME_W = 8, GAME_H = 14 };

static unsigned drum;
/* The loop: REC records what is played into a four-bar loop on the track the
 * keys are on. There is no grid; the loop is drawn as orbits around the star. */
static unsigned writing, armed, rec_fired, orbit_shown, rec_wanted;
static uint32_t playing_at, kit_revision;
static uint8_t orbit_head[2] = {255, 255};
static uint32_t rec_since;
static struct groove_view frame_loop[2]; /* kit, synth */
static const int8_t orbit_sine[64] = {0,12,25,37,49,60,71,81,90,98,106,112,117,122,125,126,127,126,125,122,117,112,106,98,90,81,71,60,49,37,25,12,0,-12,-25,-37,-49,-60,-71,-81,-90,-98,-106,-112,-117,-122,-125,-126,-127,-126,-125,-122,-117,-112,-106,-98,-90,-81,-71,-60,-49,-37,-25,-12};
static unsigned kit_ready, knob_bank, ble_was_linked;
static uint32_t kit_retry, scene_at, glo_since, seek_until, seek_at, pulse_until;
static unsigned glo_fired, seeking, arp_fired, frame_repeat;
static uint32_t arp_since;
/* The preset list: the voice in use and its three neighbours either side. */
static char frame_names[7][11];
static unsigned names_program = ~0U;
/* Coin pusher (OP1+OP2+OP3). Positions are in eighths of a logical pixel on a
 * 120x120 field. A coin that goes over the front edge plays a note chosen by
 * where it fell and moves the smoke, which is the effects macro. */
enum { COINS = 30, COIN_R = 5, COIN_LEFT = 12, COIN_RIGHT = 108, COIN_BACK = 22, COIN_EDGE = 86,
       COIN_CHORD = (1U << 1) | (1U << 3) | (1U << 5), KNIFE_CHORD = (1U << 8) | (1U << 10),
       BLOCK_CHORD = (1U << 13) | (1U << 15) | (1U << 17) /* OP6 + PIT + GLO */ };
static struct coin { int16_t x, y; uint8_t state, age; } coins[COINS]; /* state: 0 none, 1 on the table, 2 falling */
static struct { uint8_t note, channel, ticks; } coin_notes[6];
static unsigned coin_phase, coin_next, coin_frame, coin_target;
/* Both games play in one scale, so whatever chance does is in tune. The maqam
 * scales use the engine's per-note tuning for their half-flat degrees. */
static const struct scale { const char *name; uint8_t count, step[7]; int8_t cents[7]; } scales[] = {
    {"SCALE: MINOR PENTATONIC", 5, {0, 3, 5, 7, 10}, {0}},  {"SCALE: MAJOR PENTATONIC", 5, {0, 2, 4, 7, 9}, {0}},
    {"SCALE: HIJAZ", 7, {0, 1, 4, 5, 7, 8, 10}, {0}},       {"SCALE: HIJAZ KAR", 7, {0, 1, 4, 5, 7, 8, 11}, {0}},
    {"SCALE: KURD", 7, {0, 1, 3, 5, 7, 8, 10}, {0}},        {"SCALE: NAHAWAND", 7, {0, 2, 3, 5, 7, 8, 11}, {0}},
    {"SCALE: BAYATI", 7, {0, 2, 3, 5, 7, 8, 10}, {0, -50}}, {"SCALE: RAST", 7, {0, 2, 4, 5, 7, 9, 11}, {0, 0, -50, 0, 0, 0, -50}},
    {"SCALE: SABA", 7, {0, 2, 3, 4, 7, 8, 10}, {0, -50}}};
enum { SCALES = sizeof scales / sizeof scales[0], SCALE_ROOT = 45 };
static unsigned game_scale, game_smoke = 60, coin_cap = 22, game_speed = 3;
static struct { uint8_t note, velocity, wait; } burst[12];
/* Knife throwing (OP4+OP5): a target that will not hold still. */
enum { BLADES = 4, STUCK = 6 };
static struct { int16_t x, y; uint8_t live; } blades[BLADES];
static struct { int8_t x, y; } stuck[STUCK];
static unsigned stuck_count, knife_a, knife_b, knife_flash, knife_size = 14;
static const uint8_t *target_picture;
/* Master FX window (hold FX): twelve effects on the whole mix. White keys hold
 * one while pressed, black keys latch it. The first ten run in the audio
 * output stage; FREEZE and WARP lean on the delay. */
enum { FX_TILES = 12, FX_FREEZE = 10, FX_WARP = 11 };
static unsigned fx_held, fx_latched, fx_depth = 10, fx_rate = 6, fx_sent = ~0U, fx_down, fx_fired, fx_engine;
static uint32_t fx_since, warp_at;
static uint8_t fx_delay[2]; /* the player's delay time and feedback while FREEZE or WARP has them */
static unsigned intro, fx_open;
static uint32_t signs_shown, tiles_shown; /* smoke over the welcome page and its fade into the first screen */
static uint32_t coin_at, coin_random;
static unsigned view_page = PAGE_SPLASH;
static uint32_t splash_at, glitch_until;
/* Scene effects: a torn, colour-split frame on snare and clap hits, and the
 * vapor cloud blown by ENV. Both are read by the band painter. */
static unsigned glitch;
/* The vape: a pen on the moon. `smoke` is how much it gives off, 0 to 100; at
 * 100 the cloud covers the whole screen. Not saved with the pattern. */
static unsigned smoke, smoke_drift = 2, smoke_shown, smoke_seen;
/* Smoke is also the effects macro: it pushes both reverb and delay sends and
 * the delay feedback from where the player set them up to full. */
enum { FX_KIT_REVERB, FX_KIT_DELAY, FX_SYNTH_REVERB, FX_SYNTH_DELAY, FX_FEEDBACK, FX_COUNT, FX_FEEDBACK_TOP = 85 };
static uint8_t fx_base[FX_COUNT];
static unsigned fx_applied;
/* HOME hands the keys to the synth on track 1. */
static unsigned synth_mode = 1, synth_retry;
static int octave;
static uint8_t key_channel[27];
static struct groove_view frame_synth;
static uint8_t frame_synth_sends[2];
static char frame_voice[11];
/* The radar: ten controls on ten spokes, each 0 to 100. No numbers anywhere. */
enum { RADAR_LEVEL, RADAR_TUNE, RADAR_DECAY, RADAR_TEMPO, RADAR_RETURN, RADAR_REVERB, RADAR_DELAY,
       RADAR_FEEDBACK, RADAR_SMOKE, RADAR_SYNTH, RADAR_AXES, RADAR_X = 196, RADAR_Y = 161, RADAR_R = 30 };
static uint8_t frame_radar[RADAR_AXES]; /* kept as the controls' 0-100 readings; nothing draws them now */
/* Tide Blocks: a small falling-block game that runs while the pattern plays. */
static uint8_t game_cells[GAME_H][GAME_W];
static struct { int x, y; unsigned kind, turn; } game_piece;
static unsigned game_lines, game_over;
static uint32_t game_random, game_fall_at;
static unsigned scene_phase, scene_pulse;
static uint16_t scene_mark; /* pulse colour of the Terms Of Service sun */
static const uint8_t *scene_picture;
static const char *notice;
static uint32_t notice_until;
static char master_notice[24];
static uint8_t key_note[27], retry_reset;
static uint32_t previous_buttons, previous_keys, blocked_keys, frame_ms, frame_time;
static uint32_t encoder_time[7];
static int8_t encoder_direction[7];
static uint8_t encoder_speed[7];
/* One frame of instrument state; both paint passes read the same snapshot. */
static struct groove_view frame_view;
static uint8_t frame_pots[GROOVE_TONE_PARAMS], frame_sends[2], frame_effects[4];
static unsigned frame_samples, frame_dirty;
struct draw_cache {
    unsigned shown;
    uint32_t scene, sea, sun, effects, status, hint, radar, foot[4], lane[8];
    uint8_t cell[8][16], game[GAME_H][GAME_W];
};
static struct draw_cache saved_cache, band_cache, *cache = &saved_cache;
static uint32_t led_cache;
static uint16_t button_led_cache;

static __attribute__((noinline)) int clamp(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}
static unsigned encoder_active(const int16_t values[7])
{
    for (unsigned i = 0; i < 7; ++i)
        if (values[i])
            return 1;
    return 0;
}
static void read_encoders(const struct fm1_controls *controls, uint32_t now, unsigned boundary)
{
    /* Apply the first detent immediately. A continuing fast turn accelerates
     * continuous parameters. Reversal or a pause returns to fine adjustment. */
    if (boundary) memset(encoder_direction, 0, sizeof encoder_direction);
    for (unsigned i = 0; i < 7; ++i) {
        int direction = (controls->encoders[i] > 0) - (controls->encoders[i] < 0);
        if (!direction) continue;
        encoder_speed[i] = direction == encoder_direction[i] && now - encoder_time[i] <= 40U
            ? (uint8_t)clamp(encoder_speed[i] + 1, 1, 4) : 1;
        encoder_direction[i] = (int8_t)direction;
        encoder_time[i] = now;
    }
}
#ifdef FM1_HOST_TEST
static unsigned web_fine;
void groove_screen_web_fine(unsigned fine) { web_fine = fine; }
#endif
static int encoder_delta(unsigned encoder, int16_t value)
{
#ifdef FM1_HOST_TEST
    if (web_fine) return value;
#endif
    int amount = value < 0 ? -(int)value : value;
    int gain = amount < 2 ? 1 : amount < 4 ? 2 : amount == 4 ? 3 : 4;
    if (gain < encoder_speed[encoder]) gain = encoder_speed[encoder];
    return value * gain;
}
static int post(uint8_t command, uint8_t a, uint8_t b, unsigned bytes)
{
    const uint8_t message[3] = {command, a, b};
    if (!groove_midi_local(message, bytes))
        return 1;
    retry_reset = 1;
    return 0;
}
static __attribute__((noinline)) void decimal(char out[12], int value, unsigned digits)
{
    unsigned n = value < 0 ? (unsigned)-value : (unsigned)value, length;
    length = 1;
    for (unsigned v = n; v >= 10; v /= 10)
        ++length;
    if (length < digits)
        length = digits;
    unsigned start = value < 0;
    if (start)
        out[0] = '-';
    out[start + length] = 0;
    while (length) {
        out[start + --length] = (char)('0' + n % 10);
        n /= 10;
    }
}
static uint32_t signature(uint32_t seed, const void *bytes, unsigned n)
{
    const uint8_t *p = bytes;
    for (unsigned i = 0; i < n; ++i)
        seed = seed * 33U + p[i];
    return seed;
}
static void say(const char *message, uint32_t now, unsigned ms)
{
    notice = message;
    notice_until = now + ms;
}

/* ---- Drawing ---------------------------------------------------------- */
static uint16_t scene_pixel(unsigned x, unsigned y)
{
    unsigned torn = 0;
    if (glitch) {
        unsigned h = beach_hash(y / 2U, glitch);
        if (h % 5U == 0) {
            x = (x + 4U + (h >> 8) % 20U) % BEACH_W;
            torn = 1U + ((h >> 4) & 1U);
        }
    }
    uint16_t color = beach_scene_pixel(x, y, scene_phase, scene_pulse, scene_mark, smoke != 0, scene_picture);
    if (torn == 1) color = (uint16_t)((color & 0x07e0U) | (color >> 11) | (color << 11)); /* swap red and blue */
    else if (torn == 2) color |= 0xf81fU;
    return color;
}
/* Smoke over the finished picture, panel included. It billows out of the
 * mouthpiece, rising faster than it sinks, in 120x120 logical pixels. */
static unsigned smoke_at(unsigned x, unsigned y)
{
    static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    int dx = (int)x - BEACH_VAPE_TIP_X, dy = (int)y - BEACH_VAPE_TIP_Y, reach = (int)(smoke_seen * smoke_seen) / 34;
    int distance = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy / 2 : dy * 2);
    if (distance >= reach + 12) return 0;
    unsigned roll = scene_phase * smoke_drift;
    unsigned puff = beach_hash((x + roll / 2U) / 6U, (y + roll) / 5U);
    int density = (reach - distance) / 4 + (int)(puff & 7U) - 3;
    if (density <= (int)bayer[y & 3U][x & 3U]) return 0;
    /* Three tones keep the billows visible even when the cloud is solid. */
    return density < 6 ? 1U : 2U + ((puff >> 3) % 3U == 0);
}
static void smoke_draw(void)
{
    unsigned stride = paint_end - paint_x;
    for (unsigned row = paint_top; row < paint_top + PAINT_ROWS; ++row)
        for (unsigned col = paint_x; col < paint_end; ++col) {
            unsigned cloud = smoke_at(col >> 1, row >> 1);
            if (cloud) paint_pixels[(row - paint_top) * stride + col - paint_x] =
                cloud == 1 ? BEACH_RGB(180, 164, 204) : cloud == 2 ? BEACH_RGB(236, 230, 244) : BEACH_RGB(208, 196, 226);
        }
}
static void scene_draw(void)
{
    uint32_t effects = glitch | ((uint32_t)(scene_picture != NULL) << 16);
    if (!paint_rendering) {
        if (cache->effects != effects) cache->scene = ~0U;
        cache->effects = effects;
        if (cache->scene != 1) {
            cache->scene = 1;
            paint_mark(0, 0, FM1_SCREEN_WIDTH, SCENE_ROWS);
        } else {
            if (cache->sea != scene_phase) {
                /* Only the shooting star moves by itself: repaint its strip while it flies. */
                if (scene_phase % 60U < 10U) paint_mark(90, 0, 150, 50);
            }
            if (cache->sun != (scene_pulse | ((uint32_t)scene_mark << 1))) paint_mark(80, 52, 98, 98);
        }
        cache->sea = scene_phase;
        cache->sun = scene_pulse | ((uint32_t)scene_mark << 1);
        return;
    }
    if (paint_top >= SCENE_ROWS) return;
    /* Each logical pixel covers 2x2; bands start on even rows. */
    unsigned stride = paint_end - paint_x;
    for (unsigned row = paint_top; row < paint_top + PAINT_ROWS && row < SCENE_ROWS; row += 2) {
        uint16_t *out = paint_pixels + (row - paint_top) * stride, color = 0;
        unsigned last = ~0U;
        for (unsigned col = paint_x; col < paint_end; ++col) {
            if ((col >> 1) != last) {
                last = col >> 1;
                color = scene_pixel(last, row >> 1);
            }
            out[col - paint_x] = color;
        }
        memcpy(out + stride, out, stride * sizeof *out);
    }
}
static unsigned coin_pusher_y(void)
{
    unsigned travel = coin_phase < 30 ? coin_phase : 60U - coin_phase;
    return 34U + travel * 22U / 30U;
}
/* One logical pixel of the coin pusher: coins over the machine. */
static uint16_t coin_pixel(int x, int y, const uint8_t *near, unsigned count)
{
    for (unsigned k = 0; k < count; ++k) {
        const struct coin *c = &coins[near[k]];
        int dx = x - (c->x >> 3), dy = y - (c->y >> 3);
        int r = c->state == 2 ? COIN_R - c->age / 2 : c->age < 2 ? COIN_R + 1 : COIN_R, d = dx * dx + dy * dy;
        if (r < 2) r = 2;
        if (d > r * r) continue;
        if (d > (r - 1) * (r - 1)) return BEACH_RGB(150, 92, 16);
        if (dx + dy <= -r + 1) return BEACH_RGB(255, 246, 190);
        if (d <= 2) return BEACH_RGB(214, 150, 36);
        return dx - dy > r - 2 ? BEACH_RGB(226, 166, 40) : BEACH_RGB(250, 200, 64);
    }
    if (y < COIN_BACK) {
        /* Marquee: chasing bulbs either side of the Terms Of Service mark. */
        int dx = x - 60, dy = y - 11, ax = dx < 0 ? -dx : dx;
        if (dx * dx + dy * dy <= 81) {
            if ((dy >= -3 && dy <= -2 && ax <= 3) || (dy >= -3 && dy <= 2 && ax >= 2 && ax <= 3) ||
                (dy >= 1 && dy <= 2 && ax >= 2 && ax <= 6))
                return BEACH_RGB(14, 8, 30);
            return BEACH_RGB(232, 36, 48);
        }
        if ((y == 4 || y == 17) && x % 8 == 4) return ((unsigned)x / 8U + coin_frame / 2U) % 3U ? BEACH_RGB(120, 60, 20) : BEACH_RGB(255, 236, 150);
        return y == COIN_BACK - 1 ? BEACH_RGB(190, 198, 216) : BEACH_RGB(90, 14, 34);
    }
    if (x < COIN_LEFT - COIN_R || x > COIN_RIGHT + COIN_R) return x % 3 ? BEACH_RGB(46, 22, 78) : BEACH_RGB(70, 36, 110);
    if (y > COIN_EDGE) {
        /* The tray: fifteen slots, one per note. */
        unsigned along = (unsigned)(x - (COIN_LEFT - COIN_R)) * 15U, zone = along / 107U;
        if (y == COIN_EDGE + 1) return BEACH_RGB(206, 212, 228);
        if (along % 107U < 15U) return BEACH_RGB(8, 4, 18);
        return y > 114 ? lane_color[zone % 8U] : BEACH_RGB(22, 12, 44);
    }
    int plate = (int)coin_pusher_y();
    if (y < plate) return y == plate - 1 ? BEACH_RGB(240, 244, 252) : (y & 3) ? BEACH_RGB(160, 170, 196) : BEACH_RGB(112, 122, 152);
    if (y == plate) return BEACH_RGB(28, 12, 56);
    return ((x ^ y) & 1) ? BEACH_RGB(60, 30, 104) : BEACH_RGB(50, 24, 90);
}
static void coins_draw(void)
{
    unsigned stride = paint_end - paint_x;
    for (unsigned row = paint_top; row < paint_top + PAINT_ROWS; row += 2) {
        uint8_t near[COINS];
        unsigned count = 0, last = ~0U;
        int y = (int)(row >> 1);
        for (unsigned i = 0; i < COINS; ++i) {
            int away = (coins[i].y >> 3) - y;
            if (coins[i].state && away >= -COIN_R - 1 && away <= COIN_R + 1) near[count++] = (uint8_t)i;
        }
        uint16_t *out = paint_pixels + (row - paint_top) * stride, color = 0;
        for (unsigned col = paint_x; col < paint_end; ++col) {
            if ((col >> 1) != last) {
                last = col >> 1;
                color = coin_pixel((int)last, y, near, count);
            }
            out[col - paint_x] = color;
        }
        memcpy(out + stride, out, stride * sizeof *out);
    }
}
#ifdef FM1_HOST_TEST
static char web_knobs[4][2][16]; /* the preview page asks for knob labels; there are none to show */
#endif
/* Direct pixel work over the finished scene; band painter only. */
static void plot(int x, int y, uint16_t color)
{
    if (x < (int)paint_x || x >= (int)paint_end || y < (int)paint_top || y >= (int)(paint_top + PAINT_ROWS)) return;
    paint_pixels[((unsigned)y - paint_top) * (paint_end - paint_x) + (unsigned)x - paint_x] = color;
}
static void disc(int cx, int cy, int r, uint16_t color, unsigned filled)
{
    if (cy + r < (int)paint_top || cy - r >= (int)(paint_top + PAINT_ROWS)) return;
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            int d = dx * dx + dy * dy;
            if (d <= r * r && (filled || d > (r - 2) * (r - 2))) plot(cx + dx, cy + dy, color);
        }
}
/* No words or figures anywhere. What the player needs is drawn into the
 * scene: eight moons on the ground while the keys are drums, lit as they are
 * hit, and a row of sixteen lights along the bottom edge for the preset
 * (which light, and its colour for which sixteen). */
static void signs_draw(void)
{
    if (!synth_mode)
        for (unsigned lane = 0; lane < 8; ++lane) {
            unsigned hit = key_note[pad_keys[lane]] != 0xff || key_note[white_keys[lane]] != 0xff;
            disc(22 + (int)lane * 28, 204, hit ? 11 : 8, lane_color[lane], hit || lane_pad[lane] == drum);
        }
    for (unsigned i = 0; i < 16; ++i) {
        unsigned lit = i == (frame_synth.program & 15U);
        uint16_t color = lit ? lane_color[(frame_synth.program >> 4) & 7U] : BEACH_RGB(40, 34, 70);
        for (unsigned k = 0; k < (lit ? 48U : 12U); ++k)
            plot(9 + (int)i * 14 + (int)(k % (lit ? 8U : 4U)) + (lit ? 0 : 2), 231 + (int)(k / (lit ? 8U : 4U)) + (lit ? 0 : 2), color);
    }
}
/* Welcome: the Terms Of Service mark (a disc with a square pulse, redrawn from
 * the act's own logo) jittering over the scene. No lettering. */
static void splash_draw(void)
{
    enum { DISC = 40, DISC_Y = 172 };
    unsigned jitter = (frame_time / 90U) & 7U, shift = jitter == 2 ? 3U : jitter == 5 ? 0U : 1U;
    if (!paint_rendering) {
        if (cache->status != jitter) paint_mark(60, DISC_Y - DISC - 2, 124, 2 * DISC + 5);
        cache->status = jitter;
        return;
    }
    if (paint_top + PAINT_ROWS <= DISC_Y - DISC || paint_top > DISC_Y + DISC) return;
    int cx = 117 + (int)shift * 3;
    uint16_t color = jitter == 2 ? 0x07ff : jitter == 5 ? TEXT : BEACH_RGB(209, 21, 33);
    disc(cx, DISC_Y, DISC, color, 1);
    for (int dy = -12; dy <= 12; ++dy)
        for (int dx = -30; dx <= 30; ++dx) {
            int ax = dx < 0 ? -dx : dx;
            if ((dy <= -6 && ax <= 16) || (ax >= 10 && ax <= 16) || (dy >= 6 && ax >= 10)) plot(cx + dx, DISC_Y + dy, BACK);
        }
}
static const struct { uint8_t size, count; int8_t cell[4][2]; } game_shapes[6] = {
    {2, 2, {{0, 0}, {1, 0}}},                 {3, 3, {{0, 1}, {1, 1}, {2, 1}}},
    {2, 3, {{0, 0}, {0, 1}, {1, 1}}},         {2, 4, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}},
    {3, 4, {{1, 0}, {0, 1}, {1, 1}, {2, 1}}}, {3, 4, {{0, 0}, {0, 1}, {1, 1}, {2, 1}}}};
/* Board position of one block of the falling piece. */
static void game_block(unsigned index, int x, int y, unsigned turn, int *column, int *row)
{
    const unsigned kind = game_piece.kind;
    int cx = game_shapes[kind].cell[index][0], cy = game_shapes[kind].cell[index][1];
    for (unsigned i = 0; i < turn; ++i) {
        int next = game_shapes[kind].size - 1 - cy;
        cy = cx;
        cx = next;
    }
    *column = x + cx;
    *row = y + cy;
}
static void game_draw(void)
{
    if (paint_rendering && paint_top + PAINT_ROWS <= 116) return;
    uint8_t board[GAME_H][GAME_W];
    memcpy(board, game_cells, sizeof board);
    if (!game_over)
        for (unsigned i = 0; i < game_shapes[game_piece.kind].count; ++i) {
            int column, row;
            game_block(i, game_piece.x, game_piece.y, game_piece.turn, &column, &row);
            if (row >= 0 && row < GAME_H && column >= 0 && column < GAME_W) board[row][column] = (uint8_t)(game_piece.kind + 1U);
        }
    uint32_t sig = game_lines | (game_over << 16);
    if (sig != cache->status) {
        cache->status = sig;
        paint_fill(86, 116, 2, 112, game_over ? AMBER : INK_LANE);
        paint_fill(152, 116, 2, 112, game_over ? AMBER : INK_LANE);
        /* Cleared lines stack up as lights beside the well. */
        paint_fill(160, 116, 30, 112, BACK);
        for (unsigned line = 0; line < game_lines && line < 42; ++line)
            paint_fill(162 + (line / 14U) * 9U, 220 - (line % 14U) * 8U, 6, 6, lane_color[line % 8U]);
    }
    for (unsigned row = 0; row < GAME_H; ++row)
        for (unsigned column = 0; column < GAME_W; ++column) {
            if (board[row][column] == cache->game[row][column]) continue;
            cache->game[row][column] = board[row][column];
            paint_fill(88 + column * 8, 116 + row * 8, 8, 8, BACK);
            paint_fill(88 + column * 8, 116 + row * 8, 7, 7, board[row][column] ? lane_color[board[row][column] - 1U] : INK);
        }
}
/* Master FX overlay: twelve tiles over whatever is on screen, each a small
 * picture of what it does to a wave. Red while held, blue when latched. */
static void fx_overlay(void)
{
    static const uint8_t shape[FX_TILES][12] = {
        {2, 2, 6, 6, 10, 10, 14, 14, 10, 10, 6, 6},      {4, 4, 4, 12, 12, 12, 6, 6, 6, 14, 14, 14},
        {2, 8, 14, 14, 14, 14, 14, 14, 8, 2, 2, 2},       {2, 8, 14, 8, 2, 8, 14, 8, 2, 8, 14, 8},
        {14, 2, 14, 2, 14, 2, 14, 2, 14, 2, 14, 2},       {12, 12, 12, 0, 0, 0, 12, 12, 12, 0, 0, 0},
        {14, 6, 2, 14, 6, 2, 14, 6, 2, 14, 6, 2},         {14, 13, 12, 11, 10, 9, 8, 6, 5, 4, 3, 2},
        {14, 14, 14, 14, 13, 12, 10, 8, 5, 3, 2, 1},      {1, 2, 3, 5, 8, 10, 12, 13, 14, 14, 14, 14},
        {14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14}, {8, 12, 14, 12, 8, 4, 2, 4, 8, 12, 14, 12}};
    unsigned active = fx_held | fx_latched;
    for (unsigned i = 0; i < FX_TILES; ++i) {
        unsigned x = 6 + (i % 4U) * 58, y = 62 + (i / 4U) * 40, on = (active >> i) & 1U;
        if (y >= paint_top + PAINT_ROWS || y + 36 <= paint_top) continue;
        uint16_t bg = (fx_latched >> i) & 1U ? lane_color[2] : on ? lane_color[0] : BACK;
        paint_fill(x, y, 54, 36, on ? TEXT : INK_LANE);
        paint_fill(x + 2, y + 2, 50, 32, bg);
        for (unsigned k = 0; k < 12; ++k)
            if (shape[i][k]) paint_fill(x + 4 + k * 4, y + 31 - shape[i][k] * 2, 3, shape[i][k] * 2, on ? BACK : TEXT);
    }
    /* Depth and rate: two rows of fifteen lights under the tiles. */
    for (unsigned k = 0; k < 15; ++k) {
        paint_fill(12 + k * 15, 184, 9, 4, k < fx_depth ? lane_color[0] : INK);
        paint_fill(12 + k * 15, 192, 9, 4, k < fx_rate ? lane_color[2] : INK);
    }
}
static void knife_target(int *x, int *y)
{
    *x = 60 + 40 * orbit_sine[(knife_a >> 3) & 63U] / 127;
    *y = 34 + 15 * orbit_sine[(knife_b >> 3) & 63U] / 127;
}
static uint16_t knife_pixel(int x, int y)
{
    for (unsigned i = 0; i < BLADES; ++i) {
        int dx = x - blades[i].x, dy = y - blades[i].y;
        if (!blades[i].live || dy < 0 || dy > 15 || dx < -1 || dx > 1) continue;
        if (dy < 10) { if (dx == 0 || (dy > 2 && dx == -1)) return dx ? BEACH_RGB(150, 160, 186) : BEACH_RGB(236, 240, 250); }
        else if (dy == 10) return BEACH_RGB(255, 206, 110);
        else return dx ? BEACH_RGB(120, 16, 30) : BEACH_RGB(232, 36, 48);
    }
    int tx, ty, r = (int)knife_size;
    knife_target(&tx, &ty);
    int dx = x - tx, dy = y - ty, d = dx * dx + dy * dy;
    for (unsigned i = 0; i < stuck_count; ++i) { /* handles of the knives already in it */
        int hx = dx - stuck[i].x, hy = dy - stuck[i].y;
        if (hx >= -1 && hx <= 0 && hy >= 0 && hy <= 6) return hy == 0 ? BEACH_RGB(255, 206, 110) : BEACH_RGB(232, 36, 48);
    }
    if (d <= r * r && target_picture) {
        /* The player's picture is the board, scaled to its size. */
        int px = GROOVE_TARGET_SIZE / 2 + dx * (GROOVE_TARGET_SIZE / 2) / (r + 1), py = GROOVE_TARGET_SIZE / 2 + dy * (GROOVE_TARGET_SIZE / 2) / (r + 1);
        const uint8_t *pixel = target_picture + ((unsigned)py * GROOVE_TARGET_SIZE + (unsigned)px) * 2U;
        return (uint16_t)(pixel[0] | (unsigned)pixel[1] << 8);
    }
    if (d <= r * r) {
        static const uint16_t rings[5] = {BEACH_RGB(232, 36, 48), BEACH_RGB(244, 244, 244), BEACH_RGB(40, 130, 240),
                                          BEACH_RGB(244, 244, 244), BEACH_RGB(232, 36, 48)};
        /* The board spins: alternate quarters are shaded. */
        int c = orbit_sine[((knife_a >> 2) + 16U) & 63U], n = orbit_sine[(knife_a >> 2) & 63U];
        uint16_t color = rings[d * 5 / (r * r + 1)];
        if (((dx * c + dy * n) > 0) != ((dy * c - dx * n) > 0) && d * 5 / (r * r + 1)) color = (uint16_t)((color >> 1) & 0x7befU);
        return color;
    }
    if (d <= (r + 2) * (r + 2)) return BEACH_RGB(20, 12, 36);
    if (knife_flash && d <= (r + 9) * (r + 9) && ((x ^ y) & 1)) return BEACH_RGB(255, 236, 150); /* a hit rings the board */
    if (y >= 110) return (x % 4 == 2 && y < 114) ? BEACH_RGB(206, 212, 228) : y == 110 ? BEACH_RGB(190, 198, 216) : BEACH_RGB(22, 12, 44);
    if (y < 6) return (x % 8 == 4 && y == 3) ? (((unsigned)x / 8U + coin_frame / 2U) % 3U ? BEACH_RGB(120, 60, 20) : BEACH_RGB(255, 236, 150)) : BEACH_RGB(90, 14, 34);
    return x % 14 == 0 ? BEACH_RGB(24, 12, 44) : ((x / 14 + y / 20) & 1) ? BEACH_RGB(58, 30, 84) : BEACH_RGB(50, 24, 74);
}
/* Full-screen painter for the two games. */
static void arcade_draw(void)
{
    if (!paint_rendering) {
        if (cache->status != coin_frame) paint_mark(0, 0, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT);
        cache->status = coin_frame;
        return;
    }
    if (view_page == PAGE_COINS) coins_draw();
    else {
        unsigned stride = paint_end - paint_x;
        for (unsigned row = paint_top; row < paint_top + PAINT_ROWS; row += 2) {
            uint16_t *out = paint_pixels + (row - paint_top) * stride, color = 0;
            unsigned last = ~0U;
            for (unsigned col = paint_x; col < paint_end; ++col) {
                if ((col >> 1) != last) { last = col >> 1; color = knife_pixel((int)last, (int)(row >> 1)); }
                out[col - paint_x] = color;
            }
            memcpy(out + stride, out, stride * sizeof *out);
        }
    }
}
/* Screen position of one step of one orbit (0 drums, 1 synth). */
static void orbit_point(unsigned ring, unsigned step, int *x, int *y)
{
    unsigned length = frame_loop[ring].sequence_length ? frame_loop[ring].sequence_length : 64U;
    unsigned angle = (step % length) * 64U / length;
    *x = (BEACH_SUN_X + (ring ? 48 : 34) * orbit_sine[angle] / 127) * 2;
    *y = (BEACH_SUN_Y - (ring ? 30 : 21) * orbit_sine[(angle + 16U) & 63U] / 127) * 2;
}
/* The loop as two orbits around the star: drums on the inner one, the synth on
 * the outer. Every recorded step is a moon; a comet runs round as it plays.
 * Band painter only, over the finished scene. */
static void orbit_draw(void)
{
    if (paint_top >= SCENE_ROWS) return;
    for (unsigned ring = 0; ring < 2; ++ring) {
        const struct groove_view *v = &frame_loop[ring];
        unsigned length = v->sequence_length ? v->sequence_length : 64U;
        for (unsigned step = 0; step < length; ++step) {
            unsigned on = (unsigned)(v->active_steps >> step) & 1U;
            unsigned behind = v->playing ? (v->play_step + length - step) % length : length;
            unsigned head = !behind, tail = behind > 0 && behind < 5, size = head ? 4 : on ? 3 : tail ? 2 : 1;
            if (!on && !head && !tail && (step & 1U)) continue;
            int x, y;
            orbit_point(ring, step, &x, &y);
            uint16_t color = head ? (on ? TEXT : lane_color[7]) : on ? lane_color[ring ? 2 : 0]
                           : tail ? (behind < 3 ? lane_color[4] : BEACH_RGB(70, 110, 190))
                           : writing ? BEACH_RGB(190, 36, 60) : BEACH_RGB(110, 100, 180);
            for (unsigned i = 0; i < size * size * 4U; ++i)
                plot(x - (int)size + 1 + (int)(i % (size * 2U)), y - (int)size + 1 + (int)(i / (size * 2U)), color);
            if (head && on) /* a played moon flares */
                for (int k = -9; k <= 9; ++k) { plot(x + k, y, TEXT); plot(x, y + k, TEXT); plot(x + k * 2 / 3, y + k * 2 / 3, TEXT); plot(x + k * 2 / 3, y - k * 2 / 3, TEXT); }
        }
    }
}
static void draw_page(void)
{
    if (cache->shown != view_page) {
        memset(cache, 0xff, sizeof *cache);
        cache->shown = view_page;
    }
    if (view_page == PAGE_COINS || view_page == PAGE_KNIVES) { arcade_draw(); return; }
    scene_draw();
    if (view_page == PAGE_SPLASH) splash_draw();
    else if (view_page == PAGE_GAME) game_draw();
}
static void leds(void)
{
    uint32_t bits = 0;
    for (unsigned key = 0; key < 27; ++key)
        if (key_note[key] != 0xff) bits |= 1U << key;
    /* In drum mode the sixteen pad keys glow and wink out while they are held. */
    if (!synth_mode && view_page == PAGE_KIT)
        for (unsigned lane = 0; lane < 8; ++lane) bits ^= (1U << pad_keys[lane]) | (1U << white_keys[lane]);
    if (view_page == PAGE_COINS) bits = (frame_time / 120U) & 1U ? COIN_CHORD : 0;
    if (view_page == PAGE_KNIVES) bits = (frame_time / 120U) & 1U ? KNIFE_CHORD : 0;
    uint32_t changed = bits ^ led_cache;
    for (unsigned key = 0; key < 27; ++key)
        if (changed & (1U << key)) fm1_led_set(key + 14, (bits >> key) & 1U);
    led_cache = bits;
    bits = ((octave < 0) << OCT_DOWN) | ((octave > 0) << OCT_UP) | ((knob_bank == 1 || (fx_latched && ((frame_time / 200U) & 1U))) << FX) | (synth_mode << HOME)
        | (frame_dirty << SAVE) | ((knob_bank == 2 || smoke) << ENV) | ((view_page == PAGE_GAME) << LFO) | (frame_repeat << ARP)
        | ((frame_view.playing != 0) << PLAY) | ((writing || (armed && ((frame_time / 250U) & 1U))) << REC);
    changed = bits ^ button_led_cache;
    for (unsigned key = 0; key < 14; ++key)
        if (changed & (1U << key)) fm1_led_set(key, (bits >> key) & 1U);
    button_led_cache = (uint16_t)bits;
}
static void draw(void)
{
    frame_time = fm1_millis();
    if (notice && (int32_t)(frame_time - notice_until) >= 0) notice = NULL;
    groove_get_view(KIT, 0, &frame_view);
    memset(frame_pots, 0, sizeof frame_pots);
    groove_tone_copy(KIT, drum, frame_pots);
    groove_sends_get(KIT, frame_sends);
    groove_effects_get(frame_effects);
    frame_samples = groove_samples_status();
    groove_picture_poll();
    scene_picture = groove_picture(0);
    target_picture = groove_picture(1);
    frame_repeat = groove_arp_enabled(KIT) != 0;
    groove_get_view(SYNTH, 0, &frame_synth);
    groove_sends_get(SYNTH, frame_synth_sends);
    uint8_t patch[156];
    if (frame_synth.program != names_program) {
        names_program = frame_synth.program;
        for (unsigned row = 0; row < 7; ++row) {
            groove_fm_program(patch, (names_program + FM1_FM_PROGRAMS + row - 3U) % FM1_FM_PROGRAMS);
            memcpy(frame_names[row], patch + 145, 10);
            frame_names[row][10] = 0;
        }
    }
    memcpy(frame_voice, "SYNTH", 6);
    if (!groove_fm_copy(SYNTH, patch, NULL)) { memcpy(frame_voice, patch + 145, 10); frame_voice[10] = 0; }
    const uint8_t *sends = synth_mode ? frame_synth_sends : frame_sends;
    frame_radar[RADAR_LEVEL] = (uint8_t)(frame_pots[SAMPLE_DRUM_LEVEL] * 100U / 127U);
    frame_radar[RADAR_TUNE] = (uint8_t)(frame_pots[SAMPLE_DRUM_TUNE] * 100U / 48U);
    frame_radar[RADAR_DECAY] = (uint8_t)(frame_pots[SAMPLE_DRUM_LENGTH] * 100U / 127U);
    frame_radar[RADAR_TEMPO] = (uint8_t)(frame_view.tempo > 30 ? (frame_view.tempo - 30U) * 100U / 270U : 0U);
    frame_radar[RADAR_RETURN] = (uint8_t)(frame_effects[3] > 100 ? 100 : frame_effects[3]);
    frame_radar[RADAR_REVERB] = (uint8_t)(sends[0] * 100U / 15U);
    frame_radar[RADAR_DELAY] = (uint8_t)(sends[1] * 100U / 15U);
    frame_radar[RADAR_FEEDBACK] = frame_effects[2] > 100 ? 100 : frame_effects[2];
    frame_radar[RADAR_SMOKE] = (uint8_t)smoke;
    frame_radar[RADAR_SYNTH] = (uint8_t)(frame_synth.volume * 100U / 127U);
    frame_dirty = groove_project_dirty() != 0;
    frame_loop[0] = frame_view;
    frame_loop[1] = frame_synth;
    /* The stars drift slowly; the red star swells with every note played. */
    if (frame_time - scene_at >= 220U) {
        scene_at = frame_time;
        ++scene_phase;
    }
    scene_pulse = (int32_t)(pulse_until - frame_time) > 0;
    /* A held drum lights the mark in its own colour; the synth lights it blue. */
    scene_mark = BEACH_RGB(14, 8, 30);
    for (unsigned key = 0; key < 27; ++key)
        if (key_note[key] != 0xff) scene_mark = lane_color[2];
    if (!synth_mode)
        for (unsigned lane = 0; lane < 8; ++lane)
            if (key_note[pad_keys[lane]] != 0xff || key_note[white_keys[lane]] != 0xff) scene_mark = lane_color[lane];
    /* Snare and clap tear the picture for a moment; the splash never settles. */
    if (view_page == PAGE_SPLASH) { glitch = 1U + (frame_time / 120U) % 250U; glitch_until = frame_time + 200U; }
    if (glitch && (int32_t)(frame_time - glitch_until) >= 0) glitch = 0;
    /* In the games the smoke still drives the effects in full, but only a
     * thin haze of it is drawn so the game stays visible. */
    smoke_seen = (view_page == PAGE_COINS || view_page == PAGE_KNIVES) && smoke > 38 ? 38 : smoke;
    /* Welcome: four seconds clear, two and a half filling with smoke, then
     * three more as it thins over the first screen. */
    if (intro) {
        uint32_t age = frame_time - splash_at;
        unsigned cloud = age < 4000U ? 0U : age < 6500U ? (age - 4000U) * 100U / 2500U
                       : age < 9500U ? 100U - (age - 6500U) * 100U / 3000U : 0U;
        if (age >= 6500U && view_page == PAGE_SPLASH) view_page = PAGE_KIT;
        if (age >= 9500U) intro = 0;
        cloud = cloud / 4U * 4U;
        if (cloud > smoke_seen) smoke_seen = cloud;
    }
    paint_priority = 0;
    cache = &saved_cache;
    draw_page();
    /* The cloud moves with the stars: repaint what it covers on every drift,
     * and once more when it clears. */
    unsigned drifting = smoke_seen | (scene_phase << 8);
    if ((smoke_seen || smoke_shown) && drifting != smoke_shown) paint_mark(0, 0, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT);
    smoke_shown = smoke_seen ? drifting : 0;
    /* The orbits move with the loop. Repaint only where the comet was and is:
     * redrawing the whole sky every step made the keys late on the device. */
    const uint32_t loop[] = {frame_view.playing ? frame_view.play_step + 1U : 0U, writing,
                             (uint32_t)frame_view.active_steps, (uint32_t)(frame_view.active_steps >> 32),
                             (uint32_t)frame_synth.active_steps, (uint32_t)(frame_synth.active_steps >> 32)};
    unsigned orbit = signature(7, loop + 1, sizeof loop - sizeof loop[0]);
    if (view_page == PAGE_KIT) {
        if (orbit != orbit_shown) paint_mark(20, 28, 220, 146); /* a moon came or went */
        for (unsigned ring = 0; ring < 2; ++ring) {
            unsigned head = frame_loop[ring].playing ? frame_loop[ring].play_step : 255U;
            if (head == orbit_head[ring]) continue;
            for (unsigned k = 0; k < 6; ++k)
                for (unsigned which = 0; which < 2; ++which) {
                    unsigned at = which ? head : orbit_head[ring];
                    int x, y;
                    if (at == 255U) continue;
                    orbit_point(ring, at + 64U - k, &x, &y);
                    paint_mark((unsigned)clamp(x - 11, 0, 216), (unsigned)clamp(y - 11, 0, 216), 23, 23);
                }
            orbit_head[ring] = (uint8_t)head;
        }
    }
    orbit_shown = orbit;
    /* Pads, preset lights and the FX tiles are drawn over the scene: repaint
     * their strips when what they show changes. */
    uint32_t pads = synth_mode ? 0U : 1U | (drum << 1);
    if (!synth_mode)
        for (unsigned lane = 0; lane < 8; ++lane)
            if (key_note[pad_keys[lane]] != 0xff || key_note[white_keys[lane]] != 0xff) pads |= 16U << lane;
    uint32_t signs = pads | ((uint32_t)frame_synth.program << 24);
    if (signs != signs_shown && view_page == PAGE_KIT) {
        if ((signs ^ signs_shown) & 0xffffffU) paint_mark(0, 190, FM1_SCREEN_WIDTH, 28);
        paint_mark(0, 230, FM1_SCREEN_WIDTH, 8);
    }
    signs_shown = signs;
    uint32_t tiles = fx_open | (fx_held << 1) | (fx_latched << 13) | (fx_depth << 25) | (fx_rate << 28);
    if (tiles != tiles_shown) paint_mark(0, (tiles ^ tiles_shown) & 1U ? 0 : 60, FM1_SCREEN_WIDTH, (tiles ^ tiles_shown) & 1U ? FM1_SCREEN_HEIGHT : 138);
    tiles_shown = tiles;
    if (frame_view.playing && !frame_view.play_step && loop[0] && !glitch) { glitch = 1U + (frame_time & 255U); glitch_until = frame_time + 70U; }
    leds();
}
static void paint_flush(void)
{
    uint32_t started = fm1_millis();
    paint_rendering = 1;
    cache = &band_cache;
    for (unsigned visited = 0; visited < PAINT_BANDS; ++visited) {
        int pending = fm1_screen_pending();
        if (pending) {
            if (pending < 0) saved_cache.shown = ~0U;
            break;
        }
        unsigned band = paint_next;
        /* Fresh knob/key feedback goes ahead of the rest of a page repaint.
         * Rotate within that set so continuous motion cannot starve its text. */
        if (paint_priority)
            for (unsigned i = 0; i < PAINT_BANDS; ++i) {
                unsigned candidate = (paint_next + i) % PAINT_BANDS;
                if (paint_priority & (1U << candidate)) { band = candidate; break; }
            }
        paint_next = (band + 1U) % PAINT_BANDS;
        unsigned left = paint_left[band], width = paint_right[band] - left;
        if (left >= paint_right[band]) continue;
        paint_top = band * PAINT_ROWS;
        paint_x = left;
        paint_end = left + width;
        memset(cache, 0xff, sizeof *cache);
        draw_page();
        if (view_page == PAGE_KIT) { orbit_draw(); signs_draw(); } /* never over the games */
        if (smoke_seen) smoke_draw();
        if (fx_open) fx_overlay(); /* on top of everything, in every mode */
        /* Rasterize directly into the tightly packed DMA rectangle. */
        if (fm1_screen_submit(left, paint_top, width, PAINT_ROWS,
                            paint_pixels, width * PAINT_ROWS)) {
            saved_cache.shown = ~0U; /* Retry the whole page after a transport error. */
            break;
        }
        paint_left[band] = FM1_SCREEN_WIDTH;
        paint_right[band] = 0;
        paint_priority &= ~(1U << band);
        /* The DMA borrows paint_pixels. The next iteration/poll must observe
         * completion before composing another band into that same buffer. */
        if (fm1_millis() - started >= 4U) break;
    }
    paint_rendering = 0;
    cache = &saved_cache;
}

/* ---- Control ---------------------------------------------------------- */
static int release_keys(void)
{
    for (unsigned key = 0; key < 27; ++key)
        if (key_note[key] != 0xff) {
            if (!post((uint8_t)(0x80 + key_channel[key]), key_note[key], 0, 3))
                return 0;
            key_note[key] = 0xff;
        }
    return 1;
}
static void clear_lane(unsigned pad)
{
    uint64_t steps = groove_drum_steps(KIT, pad);
    for (unsigned step = 0; step < GROOVE_STEPS; ++step)
        if ((steps >> step) & 1U) groove_sequence_edit(KIT, step, groove_pad_note(KIT, pad), 0);
}
static int kit_loaded(void)
{
    char name[13];
    groove_project_name(name);
    return groove_source_type(KIT) == GROOVE_SAMPLE &&
           (!memcmp(name, kit_name, sizeof kit_name - 1) || !memcmp(name, old_kit_name, sizeof old_kit_name - 1));
}
/* Keep the Bitch909 project in the workspace: the pattern saved in the last
 * project slot when there is one, the built-in pattern otherwise. Projects in
 * the other slots are never written. Flash may be busy at power-on; retry. */
static void kit_prepare(uint32_t now)
{
    if (kit_loaded()) {
        if (!kit_ready) {
            kit_ready = 1;
            drum = 0;
            writing = armed = rec_wanted = 0;
            groove_edit_record(0, KIT, 0);
            kit_revision = ~groove_project_revision();
        }
        /* Every time a project lands in the workspace, set it up again. At
         * power-on the saved pattern is loaded after the first setup, and one
         * saved by an older build has a one-bar drum loop and quiet effects.
         * Four-bar loops on both tracks, and everything soaked: both sends
         * full on the drums and the synth, long reverb, delay close to
         * self-oscillation, effects return full. The knobs can pull it back. */
        if (kit_revision != groove_project_revision() || groove_sequence_length(KIT) != GROOVE_STEPS ||
            groove_sequence_length(SYNTH) != GROOVE_STEPS) {
            unsigned loaded = kit_revision != groove_project_revision();
            kit_revision = groove_project_revision();
            groove_sequence_length_set(KIT, GROOVE_STEPS);
            groove_sequence_length_set(SYNTH, GROOVE_STEPS);
            if (loaded) {
                uint8_t effects[4];
                groove_effects_get(effects);
                for (unsigned i = 0; i < 4; ++i) groove_sends_edit(i < 2 ? KIT : SYNTH, i & 1U, 15);
                groove_effects_edit(0, 127);
                groove_effects_edit(2, FX_FEEDBACK_TOP - (int)effects[2]);
                groove_effects_edit(3, 127);
                fx_applied = 0; /* the smoke macro starts again from these */
            }
        }
        /* Settings left by other firmware must not silence the kit. */
        if (groove_track_role(KIT) != GROOVE_TRACK_NORMAL) (void)groove_track_role_set(KIT, GROOVE_TRACK_NORMAL);
        if (groove_midi_setting(KIT, 0) == 2) groove_settings_midi(KIT, 0, -1);
        /* Patterns saved by earlier builds have no synth yet: give track 1 a DX7. */
        if ((int32_t)(now - synth_retry) >= 0) {
            struct groove_view track1 = {0};
            synth_retry = now + 1000U;
            if (groove_track_role(SYNTH) != GROOVE_TRACK_NORMAL) (void)groove_track_role_set(SYNTH, GROOVE_TRACK_NORMAL);
            if (groove_source_type(SYNTH) != GROOVE_DX7) (void)groove_source_set(SYNTH, GROOVE_DX7);
            groove_get_view(SYNTH, 0, &track1);
            if (track1.muted) groove_mix_toggle(SYNTH, 0);
        }
        return;
    }
    kit_ready = 0;
    if ((int32_t)(now - kit_retry) < 0) return;
    char stored[13];
    int r = groove_project_info(KIT_SLOT, stored);
    if (r == FM1_FLASH_BUSY) return;
    r = !r && (!memcmp(stored, kit_name, sizeof kit_name - 1) || !memcmp(stored, old_kit_name, sizeof old_kit_name - 1))
        ? groove_project_load_slot(KIT_SLOT) : groove_factory_load(0);
    if (r == FM1_FLASH_BUSY) return;
    if (r || !kit_loaded()) r = groove_factory_load(0);
    if (!r) groove_project_clean(); /* SAVE lights only for the player's own edits. */
    else {
        kit_retry = now + 1000U;
        say("KIT LOAD FAILED - RETRYING", now, 1500);
    }
}
/* GLO is the one Bluetooth MIDI control. A tap shows the link. A hold closes
 * an open link; without one it swaps who connects: a phone or computer finds
 * FM1 (the default), or FM1 searches for a minute and joins the first
 * Bluetooth MIDI controller it finds. */
static void bluetooth_control(const struct fm1_controls *controls, uint32_t pressed,
                              const struct fm1_ble_status *ble, uint32_t now)
{
    static const char *const states[] = {
        "BLUETOOTH MIDI: OFF", "BLUETOOTH MIDI: IDLE", "BLUETOOTH MIDI: WAITING FOR HOST",
        "BLUETOOTH MIDI: SEARCHING", "BLUETOOTH MIDI: CONNECTING", "BLUETOOTH MIDI: CONNECTING",
        "BLUETOOTH MIDI: CONNECTED", "BLUETOOTH MIDI: CONNECTED", "BLUETOOTH MIDI: CLOSING"};
    unsigned central = groove_settings_role_value() == FM1_BLE_CENTRAL;
    if (pressed & BUTTON(GLO)) {
        glo_since = now;
        glo_fired = 0;
        struct fm1_bluetooth_status radio = {0};
        fm1_bluetooth_get_status(&radio);
        say(!radio.ready ? "BLUETOOTH: NOT READY" : states[ble->state <= FM1_BLE_DISCONNECTING ? ble->state : 0], now, 2000);
    }
    if ((controls->buttons & BUTTON(GLO)) && !glo_fired && now - glo_since >= 1000U) {
        glo_fired = 1;
        if (ble->state >= FM1_BLE_CONNECTING && ble->state <= FM1_BLE_READY) {
            (void)fm1_ble_disconnect();
            say("BLUETOOTH MIDI: DISCONNECT", now, 2000);
        } else {
            central = !central;
            (void)groove_settings_radio(groove_settings_radio_flags() | GROOVE_RADIO_ON,
                                        central ? FM1_BLE_CENTRAL : FM1_BLE_PERIPHERAL);
            seek_until = now + 60000U;
            seek_at = now;
            seeking = central;
            say(central ? "BLUETOOTH: FM1 SEARCHES FOR MIDI" : "BLUETOOTH: FM1 WAITS FOR A HOST", now, 2500);
        }
    }
    if (seeking && (!central || (int32_t)(seek_until - now) <= 0 || ble->state >= FM1_BLE_CONNECTED)) seeking = 0;
    if (seeking && ble->state == FM1_BLE_IDLE && !ble->pending && now - seek_at >= 3000U) {
        seek_at = now;
        for (unsigned i = 0; i < ble->count && i < FM1_BLE_MAX_DEVICES; ++i) {
            struct fm1_ble_device device;
            memset(&device, 0, sizeof device);
            (void)fm1_ble_get_device(i, &device);
            if (device.midi) {
                (void)fm1_ble_connect(i);
                return;
            }
        }
        (void)fm1_ble_scan();
    }
}
static int game_fits(int x, int y, unsigned turn)
{
    for (unsigned i = 0; i < game_shapes[game_piece.kind].count; ++i) {
        int column, row;
        game_block(i, x, y, turn, &column, &row);
        if (column < 0 || column >= GAME_W || row >= GAME_H || (row >= 0 && game_cells[row][column])) return 0;
    }
    return 1;
}
static void game_spawn(void)
{
    game_random = game_random * 1664525U + 1013904223U;
    game_piece.kind = (game_random >> 16) % 6U;
    game_piece.turn = 0;
    game_piece.x = GAME_W / 2 - 1;
    game_piece.y = 0;
    if (!game_fits(game_piece.x, 0, 0)) game_over = 1;
}
static void game_reset(uint32_t now)
{
    memset(game_cells, 0, sizeof game_cells);
    game_lines = game_over = 0;
    game_random ^= now * 2654435761U;
    game_fall_at = now;
    game_spawn();
}
static void game_land(void)
{
    for (unsigned i = 0; i < game_shapes[game_piece.kind].count; ++i) {
        int column, row;
        game_block(i, game_piece.x, game_piece.y, game_piece.turn, &column, &row);
        if (row >= 0) game_cells[row][column] = (uint8_t)(game_piece.kind + 1U);
    }
    for (int row = GAME_H - 1; row >= 0; --row) {
        unsigned full = 1;
        for (unsigned column = 0; column < GAME_W; ++column) full &= game_cells[row][column] != 0;
        if (!full) continue;
        memmove(game_cells[1], game_cells[0], (size_t)row * GAME_W);
        memset(game_cells[0], 0, GAME_W);
        ++game_lines;
        ++row;
    }
    game_spawn();
}
static void game_control(const struct fm1_controls *controls, uint32_t pressed, uint32_t now)
{
    if (game_over) {
        if (pressed & BUTTON(REC)) game_reset(now);
        return;
    }
    int move = controls->encoders[0] + controls->encoders[2] + ((pressed >> OCT_UP) & 1) - ((pressed >> OCT_DOWN) & 1);
    move = clamp(move, -1, 1);
    if (move && game_fits(game_piece.x + move, game_piece.y, game_piece.turn)) game_piece.x += move;
    if (((pressed & BUTTON(SEQ)) || controls->encoders[6] || controls->encoders[3]) &&
        game_fits(game_piece.x, game_piece.y, (game_piece.turn + 1U) & 3U))
        game_piece.turn = (game_piece.turn + 1U) & 3U;
    if (pressed & BUTTON(REC)) {
        while (game_fits(game_piece.x, game_piece.y + 1, game_piece.turn)) ++game_piece.y;
        game_land();
        game_fall_at = now;
        return;
    }
    if (now - game_fall_at < (game_lines < 30 ? 450U - game_lines * 10U : 150U)) return;
    game_fall_at = now;
    if (game_fits(game_piece.x, game_piece.y + 1, game_piece.turn)) ++game_piece.y;
    else game_land();
}
static unsigned fx_value(unsigned index)
{
    uint8_t values[4];
    if (index == FX_FEEDBACK) { groove_effects_get(values); return values[2]; }
    groove_sends_get(index < FX_SYNTH_REVERB ? KIT : SYNTH, values);
    return values[index & 1U];
}
/* Move the effects to where `level` puts them between the player's own
 * settings and full. Level 0 hands the player's settings back untouched. */
static void fx_apply(unsigned level)
{
    for (unsigned i = 0; i < FX_COUNT; ++i) {
        if (!fx_applied) fx_base[i] = (uint8_t)fx_value(i);
        int top = i == FX_FEEDBACK ? FX_FEEDBACK_TOP : 15, base = fx_base[i];
        int want = base >= top ? base : base + (top - base) * (int)level / 100, delta = want - (int)fx_value(i);
        if (!delta) continue;
        if (i == FX_FEEDBACK) groove_effects_edit(2, delta);
        else groove_sends_edit(i < FX_SYNTH_REVERB ? KIT : SYNTH, i & 1U, delta);
    }
    fx_applied = level;
}
/* An effects knob moves the player's own setting, under the macro if it is up. */
static void fx_knob(unsigned index, int delta)
{
    if (!fx_applied) {
        if (index == FX_FEEDBACK) groove_effects_edit(2, delta);
        else groove_sends_edit(index < FX_SYNTH_REVERB ? KIT : SYNTH, index & 1U, delta);
        return;
    }
    fx_base[index] = (uint8_t)clamp((int)fx_base[index] + delta, 0, index == FX_FEEDBACK ? 100 : 15);
    fx_apply(fx_applied);
}
static unsigned coin_roll(unsigned range)
{
    coin_random = coin_random * 1664525U + 1013904223U;
    return (coin_random >> 12) % range;
}
static void coin_drop(unsigned x)
{
    /* A full table sheds the coin nearest the edge to make room. */
    unsigned live = 0, front = 0;
    for (unsigned i = 0; i < COINS; ++i)
        if (coins[i].state == 1) { ++live; if (coins[i].y > coins[front].y || coins[front].state != 1) front = i; }
    if (live >= coin_cap) coins[front].y = (int16_t)(COIN_EDGE * 8 + 1);
    for (unsigned i = 0; i < COINS; ++i)
        if (!coins[i].state) {
            coins[i].x = (int16_t)(clamp((int)x, COIN_LEFT, COIN_RIGHT) * 8);
            coins[i].y = (int16_t)((coin_pusher_y() + COIN_R + 1U) * 8U);
            coins[i].state = 1;
            coins[i].age = 0;
            return;
        }
}
static void coin_notes_off(unsigned all)
{
    for (unsigned i = 0; i < 6; ++i)
        if (coin_notes[i].ticks && (all || !--coin_notes[i].ticks)) {
            (void)post((uint8_t)(0x80 + coin_notes[i].channel), coin_notes[i].note, 0, 3);
            coin_notes[i].ticks = 0;
        }
    if (all) memset(burst, 0, sizeof burst);
}
/* The note `index` steps up the chosen scale from A2. */
static unsigned scale_note(unsigned index)
{
    const struct scale *sc = &scales[game_scale];
    return SCALE_ROOT + 12U * (index / sc->count) + sc->step[index % sc->count];
}
/* Tune the synth to the scale (half-flats for the maqamat), or back to equal. */
static void scale_tune(unsigned on)
{
    struct groove_tuning tuning;
    memset(&tuning, 0, sizeof tuning);
    const struct scale *sc = &scales[game_scale];
    if (on)
        for (unsigned i = 0; i < sc->count; ++i) tuning.main[(SCALE_ROOT + sc->step[i]) % 12U] = sc->cents[i];
    (void)groove_tuning_set(SYNTH, &tuning);
}
static void game_note(unsigned note, unsigned velocity, unsigned ticks)
{
    unsigned slot = 0, channel = groove_midi_input_channel(SYNTH);
    for (unsigned i = 1; i < 6; ++i)
        if (coin_notes[i].ticks < coin_notes[slot].ticks) slot = i;
    if (coin_notes[slot].ticks) (void)post((uint8_t)(0x80 + coin_notes[slot].channel), coin_notes[slot].note, 0, 3);
    coin_notes[slot].ticks = 0;
    if (note > 127 || !post((uint8_t)(0x90 + channel), (uint8_t)note, (uint8_t)velocity, 3)) return;
    coin_notes[slot].note = (uint8_t)note;
    coin_notes[slot].channel = (uint8_t)channel;
    coin_notes[slot].ticks = (uint8_t)ticks;
    pulse_until = frame_time + 120U;
}
static void game_drum(unsigned pad, unsigned velocity)
{
    unsigned note = groove_pad_note(KIT, pad), channel = groove_midi_input_channel(KIT);
    if (note > 127 || !post((uint8_t)(0x90 + channel), (uint8_t)note, (uint8_t)velocity, 3)) return;
    (void)post((uint8_t)(0x80 + channel), (uint8_t)note, 0, 3);
}
/* Queue a note for `wait` ticks from now. */
static void game_later(unsigned note, unsigned velocity, unsigned wait)
{
    for (unsigned i = 0; i < 12; ++i)
        if (!burst[i].wait) {
            burst[i].note = (uint8_t)note; burst[i].velocity = (uint8_t)velocity; burst[i].wait = (uint8_t)(wait + 1U);
            return;
        }
}
/* Shared by both games each tick: queued notes, note-offs, smoke drift. */
static void game_tick(void)
{
    ++coin_frame;
    coin_notes_off(0);
    for (unsigned i = 0; i < 12; ++i)
        if (burst[i].wait && !--burst[i].wait) game_note(burst[i].note, burst[i].velocity, 3);
    if (smoke != coin_target) smoke = (unsigned)((int)smoke + clamp((int)coin_target - (int)smoke, -4, 4));
    if (knife_flash) --knife_flash;
}
/* A coin went over the edge: fifteen slots across the front, low on the left,
 * each the next step of the scale. Every fall also sends the smoke, which is
 * the effects macro, somewhere new below the SMOKE knob. */
static void coin_fall(int x)
{
    unsigned zone = (unsigned)clamp((x - (COIN_LEFT - COIN_R)) * 15 / 107, 0, 14);
    game_note(scale_note(zone), 64U + coin_roll(56), 3U + coin_roll(8));
    coin_target = game_smoke ? coin_roll(game_smoke + 1U) : 0;
}
static void coins_step(void)
{
    game_tick();
    coin_phase = (coin_phase + 1U) % 60U;
    if (!coin_next--) {
        coin_drop(COIN_LEFT + coin_roll(COIN_RIGHT - COIN_LEFT + 1));
        coin_next = 2U + coin_roll(7);
    }
    int plate = (int)coin_pusher_y() * 8, reach = COIN_R * 8;
    for (unsigned i = 0; i < COINS; ++i) {
        struct coin *c = &coins[i];
        if (c->state == 2) {
            c->y = (int16_t)(c->y + 28);
            if (++c->age > 6) c->state = 0;
        } else if (c->state) {
            if (c->age < 8) ++c->age;
            if (c->y - reach < plate) c->y = (int16_t)(plate + reach);
        }
    }
    /* Coins shove the ones in front of them: the coin nearer the edge gives way. */
    for (unsigned pass = 0; pass < 2; ++pass)
        for (unsigned i = 0; i < COINS; ++i)
            for (unsigned j = i + 1; j < COINS; ++j) {
                if (coins[i].state != 1 || coins[j].state != 1) continue;
                int dx = coins[j].x - coins[i].x, dy = coins[j].y - coins[i].y, d2 = dx * dx + dy * dy;
                if (d2 >= 4 * reach * reach) continue;
                struct coin *front = dy >= 0 ? &coins[j] : &coins[i];
                if (dy < 0) { dx = -dx; dy = -dy; }
                int d = 1;
                while (d * d < d2) ++d;
                int overlap = 2 * reach - d + 1;
                front->x = (int16_t)(front->x + (d2 ? dx * overlap / d : 0));
                front->y = (int16_t)(front->y + (d2 ? dy * overlap / d : overlap));
            }
    for (unsigned i = 0; i < COINS; ++i) {
        struct coin *c = &coins[i];
        if (c->state != 1) continue;
        c->x = (int16_t)clamp(c->x, COIN_LEFT * 8, COIN_RIGHT * 8);
        if (c->y > COIN_EDGE * 8) {
            c->state = 2;
            c->age = 0;
            coin_fall(c->x >> 3);
        }
    }
}
static void coins_enter(uint32_t now)
{
    memset(coins, 0, sizeof coins);
    memset(coin_notes, 0, sizeof coin_notes);
    coin_random ^= now * 2654435761U + 909U;
    coin_phase = coin_frame = coin_next = 0;
    coin_target = smoke;
    coin_at = now;
    for (unsigned i = 0; i < 16 && i + 4U < coin_cap; ++i) {
        coin_drop(COIN_LEFT + coin_roll(COIN_RIGHT - COIN_LEFT + 1));
        coins[i].y = (int16_t)((62U + coin_roll(22)) * 8U);
    }
    scale_tune(1);
    view_page = PAGE_COINS;
}
/* A knife reached the board. The closer to the middle, the wilder the answer,
 * always in the scale. */
static void knife_hit(int dx, int dy, int r)
{
    unsigned ring = (unsigned)((dx * dx + dy * dy) * 4 / (r * r + 1));
    stuck[stuck_count % STUCK].x = (int8_t)dx;
    stuck[stuck_count % STUCK].y = (int8_t)dy;
    if (stuck_count < STUCK) ++stuck_count;
    knife_flash = 2;
    if (!ring) { /* bullseye: a run of ten, crash and kick, everything tears */
        for (unsigned i = 0; i < 10; ++i) game_later(scale_note(coin_roll(21)), 80U + coin_roll(40), i);
        game_drum(5, 120); game_drum(0, 120);
        glitch = 1U + coin_roll(250); glitch_until = frame_time + 400U;
        coin_target = 100;
        stuck_count = 0;
    } else if (ring == 1) { /* a rising figure from a random step */
        unsigned from = coin_roll(10);
        for (unsigned i = 0; i < 5; ++i) game_later(scale_note(from + i * (1U + coin_roll(2))), 96, i * 2U);
        game_drum(3, 110);
        coin_target = game_smoke;
    } else { /* one note, plus its fifth-ish neighbour now and then */
        unsigned at = coin_roll(15);
        game_note(scale_note(at), 100, 5);
        if (coin_roll(2)) game_later(scale_note(at + 2U), 84, 2);
        game_drum(ring == 2 ? 1 : 6, 100);
        coin_target = coin_roll(game_smoke + 1U);
    }
}
static void knives_step(void)
{
    game_tick();
    knife_a += (90U + game_speed * 70U) / 34U; /* 3.4 times slower than it first was */
    knife_b += (50U + game_speed * 50U) / 34U;
    int tx, ty, r = (int)knife_size;
    knife_target(&tx, &ty);
    for (unsigned i = 0; i < BLADES; ++i) {
        if (!blades[i].live) continue;
        blades[i].y = (int16_t)(blades[i].y - 11);
        int dx = blades[i].x - tx, dy = blades[i].y - ty;
        if (dx * dx + dy * dy <= r * r) { blades[i].live = 0; knife_hit(dx, dy, r); }
        else if (blades[i].y < -16) { blades[i].live = 0; game_drum(2, 70); } /* a miss clatters off the rim */
    }
    if (coin_target > game_smoke && smoke >= coin_target) coin_target = game_smoke / 2U;
}
static void knives_enter(uint32_t now)
{
    memset(blades, 0, sizeof blades);
    memset(coin_notes, 0, sizeof coin_notes);
    memset(burst, 0, sizeof burst);
    coin_random ^= now * 2654435761U + 5U;
    stuck_count = knife_flash = coin_frame = 0;
    coin_target = smoke;
    coin_at = now;
    scale_tune(1);
    view_page = PAGE_KNIVES;
}
/* K1-K4 inside either game: smoke ceiling, scale, then two of the game's own. */
static void game_knobs(const struct fm1_controls *controls, uint32_t now)
{
    static char line[24];
    int turn;
    if ((turn = controls->encoders[2]) != 0) {
        game_smoke = (unsigned)clamp((int)game_smoke + turn * 4, 0, 100);
        coin_target = game_smoke;
        say(game_smoke ? "SMOKE" : "SMOKE OFF", now, 900);
    }
    if ((turn = controls->encoders[3]) != 0) {
        game_scale = (unsigned)clamp((int)game_scale + (turn > 0) - (turn < 0), 0, SCALES - 1);
        scale_tune(1);
        say(scales[game_scale].name, now, 1500);
    }
    unsigned coins_page = view_page == PAGE_COINS;
    if ((turn = controls->encoders[4]) != 0) {
        if (coins_page) coin_cap = (unsigned)clamp((int)coin_cap + turn, 4, COINS - 2);
        else knife_size = (unsigned)clamp((int)knife_size + turn, 7, 20);
        strcpy(line, coins_page ? "COINS: " : "TARGET: ");
        decimal(line + strlen(line), (int)(coins_page ? coin_cap : knife_size), 1);
        say(line, now, 1200);
    }
    if ((turn = controls->encoders[5]) != 0) {
        game_speed = (unsigned)clamp((int)game_speed + (turn > 0) - (turn < 0), 1, 6);
        strcpy(line, "SPEED: ");
        decimal(line + strlen(line), (int)game_speed, 1);
        say(line, now, 1200);
    }
}
/* A game chord counts only when its keys go down together, within a tenth of
 * a second, with no other key held: playing through those notes must not
 * open a game in the middle of a take. */
static uint32_t key_pressed[27];
static unsigned chord_struck(uint32_t chord, uint32_t keys, uint32_t key_down, uint32_t now)
{
    if (keys != chord || !(key_down & chord)) return 0;
    for (unsigned key = 0; key < 27; ++key)
        if ((chord & (1U << key)) && now - key_pressed[key] > 100U) return 0;
    return 1;
}
static void apply_controls(const struct fm1_controls *controls, uint32_t now, const struct fm1_ble_status *ble)
{
    uint32_t pressed = controls->buttons & ~previous_buttons, previous_arp = previous_buttons;
    uint32_t key_down = controls->keys & ~previous_keys;
    for (unsigned key = 0; key < 27; ++key)
        if (key_down & (1U << key)) key_pressed[key] = now;
    unsigned chord = chord_struck(COIN_CHORD, controls->keys, key_down, now);
    unsigned block_chord = chord_struck(BLOCK_CHORD, controls->keys, key_down, now);
    unsigned knife_chord = chord_struck(KNIFE_CHORD, controls->keys, key_down, now);
    previous_buttons = controls->buttons;
    previous_keys = controls->keys;
    blocked_keys &= controls->keys;
    if (pressed || encoder_active(controls->encoders)) notice = NULL;
    if (retry_reset) {
        blocked_keys = controls->keys;
        if (!post(0xff, 0, 0, 1)) return;
        retry_reset = 0;
        memset(key_note, 0xff, sizeof key_note);
    }
    bluetooth_control(controls, pressed, ble, now);
    kit_prepare(now);
    if (!kit_ready) return;
    read_encoders(controls, now, pressed != 0);
    struct groove_view view = {0};
    groove_get_view(KIT, 0, &view);

    if (view_page == PAGE_SPLASH) {
        /* The welcome runs its course unless something is touched. */
        if (!pressed && !key_down && !encoder_active(controls->encoders)) return;
        view_page = PAGE_KIT;
        intro = 0;
        blocked_keys = controls->keys;
    }
    /* Presets are always one move away: SEQ is next, SCL is previous, and the
     * three outer encoders scroll the list. */
    int preset = controls->encoders[0] + controls->encoders[1] + controls->encoders[6];
    if (view_page != PAGE_GAME && !fx_open) preset += (int)((pressed >> SEQ) & 1) - (int)((pressed >> SCL) & 1);
    if (preset) groove_control_edit(SYNTH, preset, 0, 0);
    if (smoke != fx_applied) fx_apply(smoke);
    /* Master effects follow the window's keys wherever the player is. */
    unsigned master = fx_held | fx_latched, word = (master & ((1U << MASTER_EFFECTS) - 1U)) | (fx_depth << 16) | (fx_rate << 24);
    if (word != fx_sent) { fx_sent = word; fm1_master_fx(master & ((1U << MASTER_EFFECTS) - 1U), fx_depth, fx_rate); }
    unsigned engine = (master >> FX_FREEZE) & 3U;
    if (engine && !fx_engine) { uint8_t e[4]; groove_effects_get(e); fx_delay[0] = e[1]; fx_delay[1] = e[2]; }
    if (engine != fx_engine || (engine & 2U)) {
        uint8_t e[4];
        groove_effects_get(e);
        /* FREEZE: the delay feeds itself completely. WARP: its time lurches about. */
        int feedback = (engine & 1U) ? 100 : fx_delay[1];
        if (e[2] != feedback) groove_effects_edit(2, feedback - (int)e[2]);
        if ((engine & 2U) && now - warp_at >= 70U) {
            warp_at = now;
            groove_effects_edit(1, (int)((now / 70U) % 9U) - 4);
        } else if (!(engine & 2U) && fx_engine && e[1] != fx_delay[0]) groove_effects_edit(1, (int)fx_delay[0] - (int)e[1]);
        fx_engine = engine;
    }

    /* The loop. PLAY starts and stops it. REC records into it straight away,
     * starting it if it was stopped; REC held for three seconds empties it. */
    unsigned loop_track = synth_mode ? SYNTH : KIT;
    if (pressed & BUTTON(PLAY)) {
        if (!post(view.playing ? 0xfc : 0xfa, 0, 0, 1)) return;
        if (view.playing) rec_wanted = armed = 0;
    }
    if ((pressed & BUTTON(REC)) && view_page != PAGE_GAME) {
        rec_since = now;
        rec_fired = 0;
        if (rec_wanted || armed) rec_wanted = armed = 0;
        else if (view.playing) rec_wanted = 1;
        else if (post(0xfa, 0, 0, 1)) armed = 1;
    }
    if ((controls->buttons & BUTTON(REC)) && !rec_fired && view_page != PAGE_GAME && now - rec_since >= 3000U) {
        rec_fired = 1;
        rec_wanted = armed = 0;
        groove_edit_record(0, loop_track, 0);
        (void)groove_edit_clear(KIT, 0, 2);
        (void)groove_edit_clear(SYNTH, 0, 2);
    }
    if (armed && view.playing) { armed = 0; rec_wanted = 1; }
    /* Recording is the player's choice, not the engine's. It ends when REC is
     * pressed again or the loop has really stopped (not on a momentary blip),
     * and if the engine drops it by itself it is switched straight back on. */
    if (view.playing || armed) playing_at = now;
    if (rec_wanted && now - playing_at > 700U) rec_wanted = 0;
    writing = rec_wanted;
    groove_edit_record(rec_wanted && view.playing, loop_track, 0);
    groove_edit_record(writing, loop_track, 0);

    /* FX: a tap swaps the knobs to the effect sends; a hold lays the master FX
     * tiles over whatever is on screen, in any mode. While they are up the
     * keys choose effects (white holds, black latches) and the mode underneath
     * keeps running. Hold FX again, or tap HOME, and the tiles go; latched
     * effects stay. */
    unsigned fx_tap = 0;
    if (pressed & BUTTON(FX)) { fx_since = now; fx_fired = 0; fx_down = 1; }
    if (fx_down && !fx_fired && (controls->buttons & BUTTON(FX)) && now - fx_since >= 600U) {
        fx_fired = 1;
        if (!release_keys()) return;
        blocked_keys = controls->keys;
        fx_open = !fx_open;
        fx_held = 0;
    }
    if (fx_down && !(controls->buttons & BUTTON(FX))) {
        fx_down = 0;
        fx_tap = !fx_fired && !fx_open && view_page == PAGE_KIT;
    }
    uint32_t play_keys = key_down & ~blocked_keys; /* key presses the mode underneath may use */
    if (fx_open) {
        static const uint8_t black[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
        uint32_t fx_keys = controls->keys & ~blocked_keys;
        fx_held = 0;
        for (unsigned i = 0; i < FX_TILES; ++i) {
            if ((fx_keys >> white_keys[i]) & 1U) fx_held |= 1U << i;
            if (i < 11 && (play_keys >> black[i]) & 1U) fx_latched ^= 1U << i;
        }
        fx_depth = (unsigned)clamp((int)fx_depth + controls->encoders[2], 0, 15);
        fx_rate = (unsigned)clamp((int)fx_rate + controls->encoders[3], 0, 15);
        play_keys = 0;
        if (pressed & BUTTON(HOME)) { fx_open = 0; fx_held = 0; blocked_keys = controls->keys; return; }
    }
    /* OP1+OP2+OP3 together open and close the coin pusher; OP4+OP5 the knives. */
    unsigned in_game = view_page == PAGE_COINS || view_page == PAGE_KNIVES;
    if (!fx_open && (chord || knife_chord || (in_game && (pressed & BUTTON(HOME))))) {
        unsigned wanted = (pressed & BUTTON(HOME)) ? PAGE_KIT : chord ? PAGE_COINS : PAGE_KNIVES;
        if (!release_keys()) return;
        blocked_keys = controls->keys;
        if (in_game) { coin_notes_off(1); scale_tune(0); smoke = 0; }
        if (wanted == view_page || wanted == PAGE_KIT) view_page = PAGE_KIT;
        else if (wanted == PAGE_COINS) coins_enter(now);
        else knives_enter(now);
        return;
    }
    if (in_game) {
        if (!fx_open) game_knobs(controls, now);
        unsigned tick = view_page == PAGE_COINS ? 150U - game_speed * 18U : 60U;
        for (unsigned key = 0; key < 27; ++key) {
            if (!(play_keys & (1U << key))) continue;
            if (view_page == PAGE_COINS) coin_drop(COIN_LEFT + key * (COIN_RIGHT - COIN_LEFT) / 26U);
            else
                for (unsigned i = 0; i < BLADES; ++i) /* a knife flies straight up from above the key */
                    if (!blades[i].live) { blades[i].x = (int16_t)(8 + key * 4); blades[i].y = 108; blades[i].live = 1; break; }
        }
        while (now - coin_at >= tick) {
            coin_at += tick;
            if (now - coin_at > 400U) coin_at = now;
            if (view_page == PAGE_COINS) coins_step(); else knives_step();
        }
        return;
    }
    /* OP6+PIT+GLO together open and close Tide Blocks. */
    if (block_chord && !fx_open) {
        if (!release_keys()) return;
        blocked_keys = controls->keys;
        view_page = view_page == PAGE_GAME ? PAGE_KIT : PAGE_GAME;
        if (view_page == PAGE_GAME && (game_over || !game_random)) game_reset(now);
        game_fall_at = now;
    }
    if (view_page == PAGE_GAME) {
        if (!fx_open) game_control(controls, pressed, now);
        return;
    }
    if (fx_tap) knob_bank = knob_bank == 1 ? 0 : 1;
    if (pressed & BUTTON(HOME)) {
        if (!release_keys()) return;
        synth_mode = !synth_mode;
        blocked_keys = controls->keys;
        say(synth_mode ? "SYNTH: ALL KEYS PLAY, OCT SHIFTS" : "DRUMS: FIRST EIGHT BLACK OR WHITE", now, 2000);
    }
    if (pressed & (BUTTON(OCT_DOWN) | BUTTON(OCT_UP))) {
        if (!release_keys()) return;
        octave = clamp(octave + (pressed & BUTTON(OCT_UP) ? 1 : -1), -2, 2);
    }
    if (pressed & BUTTON(ENV)) knob_bank = knob_bank == 2 ? 0 : 2;
    /* ARP is note repeat for the drums: each tap steps through four rates,
     * then off. Holding ARP swaps the clock source. */
    if (pressed & BUTTON(ARP)) { arp_since = now; arp_fired = 0; }
    if ((controls->buttons & BUTTON(ARP)) && !arp_fired && now - arp_since >= 1000U) {
        arp_fired = 1;
        unsigned external = groove_clock_mode() != GROOVE_CLOCK_MIDI;
        groove_settings_clock(external ? GROOVE_CLOCK_MIDI : GROOVE_CLOCK_INTERNAL);
        say(external ? "SYNC: MIDI CLOCK" : "SYNC: INTERNAL", now, 1500);
    }
    if ((previous_arp & ~controls->buttons & BUTTON(ARP)) && !arp_fired) {
        if (!release_keys()) return;
        if (!groove_arp_enabled(KIT)) groove_arp_set(KIT, 1);
        else if (groove_arp_setting(KIT, 0) < 3) groove_arp_edit(KIT, 0, 1);
        else { groove_arp_set(KIT, 0); groove_arp_edit(KIT, 0, -3); }
        say(groove_arp_enabled(KIT) ? "DRUM REPEAT: HOLD A PAD" : "DRUM REPEAT OFF", now, 1500);
    }
    if (pressed & BUTTON(SAVE)) {
        /* Store the player's own effect settings, not the macro's push. */
        unsigned macro = fx_applied;
        if (macro) fx_apply(0);
        int r = groove_project_save_slot(KIT_SLOT);
        if (macro) fx_apply(macro);
        say(r == FM1_FLASH_BUSY ? "BUSY - PRESS SAVE AGAIN" : r ? "SAVE FAILED" : "SOUND SAVED", now, 1500);
    }

    if (fx_open) return; /* the tiles have the knobs and the keys */
    /* K1-K4: sound or effect sends, or the vape after ENV. */
    for (unsigned col = 0; col < 4; ++col) {
        int16_t turn = controls->encoders[2 + col];
        if (!turn) continue;
        if (knob_bank == 2) {
            if (col == 0) smoke = (unsigned)clamp((int)smoke + turn * 2, 0, 100);
            else if (col == 1) smoke_drift = (unsigned)clamp((int)smoke_drift + turn, 1, 6);
            else smoke = col == 2 ? 100 : 0;
        } else if (knob_bank == 1) {
            if (col < 2) fx_knob((synth_mode ? FX_SYNTH_REVERB : FX_KIT_REVERB) + col, turn);
            else if (col == 2) fx_knob(FX_FEEDBACK, encoder_delta(2 + col, turn));
            else groove_effects_edit(3, encoder_delta(2 + col, turn));
        } else if (col == 3) {
            groove_tempo_edit(turn);
            if (groove_clock_mode() == GROOVE_CLOCK_MIDI) say("MIDI CLOCK - HOLD ARP FOR INTERNAL", now, 2000);
        } else if (synth_mode) {
            if (col == 0) groove_control_edit(SYNTH, 0, encoder_delta(2 + col, turn), 0);
            else fx_knob(FX_SYNTH_REVERB + col - 1U, turn);
        } else {
            static const uint8_t parameter[3] = {SAMPLE_DRUM_LEVEL, SAMPLE_DRUM_TUNE, SAMPLE_DRUM_LENGTH};
            groove_tone_edit(KIT, drum, parameter[col], col == 1 ? turn : encoder_delta(2 + col, turn));
        }
    }

    /* Keys. Synth: all 27 play, F3 to G5 before the octave shift. Drums: the
     * first eight black keys and the first eight white keys are the pads. */
    uint32_t keys = controls->keys & ~blocked_keys;
    for (unsigned key = 0; key < 27; ++key) {
        unsigned down = (keys >> key) & 1U, track = synth_mode ? SYNTH : KIT, note = 128, lane = 8;
        if (synth_mode) note = groove_key_note(SYNTH, key, octave);
        else
            for (unsigned i = 0; i < 8; ++i)
                if (key == pad_keys[i] || key == white_keys[i]) { lane = i; note = groove_pad_note(KIT, lane_pad[i]); }
        if (key_note[key] != 0xff && !down) {
            if (!post((uint8_t)(0x80 + key_channel[key]), key_note[key], 0, 3)) return;
            key_note[key] = 0xff;
        }
        if (down && key_note[key] == 0xff && note < 128) {
            unsigned channel = groove_midi_input_channel(track);
            if (!post((uint8_t)(0x90 + channel), (uint8_t)note, 100, 3)) return;
            key_note[key] = (uint8_t)note;
            key_channel[key] = (uint8_t)channel;
            pulse_until = now + 90U;
            if (lane < 8) {
                drum = lane_pad[lane];
                if (lane == 1 || lane == 4) { glitch = 1U + (now & 255U); glitch_until = now + 90U; }
            }
        }
    }
}
void groove_screen_init(void)
{
    drum = kit_ready = knob_bank = ble_was_linked = seeking = writing = armed = orbit_shown = rec_wanted = 0;
    kit_retry = scene_at = splash_at = synth_retry = fm1_millis();
    view_page = PAGE_SPLASH;
    glitch = smoke = smoke_shown = fx_applied = fx_held = fx_latched = fx_engine = fx_down = 0;
    fx_sent = ~0U;
    intro = 1;
    synth_mode = 1;
    octave = 0;
    names_program = ~0U;
    smoke_drift = 2;
    scene_phase = scene_pulse = 0;
    previous_buttons = previous_keys = blocked_keys = retry_reset = 0;
    led_cache = button_led_cache = 0;
    memset(key_note, 0xff, sizeof key_note);
    memset(paint_left, FM1_SCREEN_WIDTH, sizeof paint_left);
    memset(paint_right, 0, sizeof paint_right);
    paint_next = paint_rendering = paint_priority = 0;
    memset(encoder_direction, 0, sizeof encoder_direction);
    memset(encoder_speed, 0, sizeof encoder_speed);
    saved_cache.shown = ~0U;
    fm1_controls_init();
    for (unsigned key = 0; key < 41; ++key) fm1_led_set(key, 0);
    fm1_screen_init();
    draw();
    paint_flush();
    frame_ms = fm1_millis();
}
void groove_screen_tap(uint32_t now)
{
    int bpm = groove_tempo_tap(now);
    say(bpm < 0 ? "TAP: EXTERNAL MIDI CLOCK" : bpm ? "TAP TEMPO APPLIED" : "TAP AGAIN", now, 1500);
}
void groove_screen_control(void)
{
    struct fm1_controls controls;
    fm1_controls_read(&controls);
    uint32_t now = fm1_millis();
    unsigned master_changed = groove_master_update(controls.knob);
    if (master_changed && (!notice || notice == master_notice || (int32_t)(now - notice_until) >= 0)) {
        unsigned level = fm1_engine_output_level();
        if (level) {
            memcpy(master_notice, "MASTER ", 7);
            decimal(master_notice + 7, (int)((level * 100U + 63U) / 127U), 1);
            memcpy(master_notice + strlen(master_notice), " / 100", sizeof " / 100");
        }
        else memcpy(master_notice, "MASTER MUTED", sizeof "MASTER MUTED");
        say(master_notice, now, 1200);
    }
    /* A dropped BLE link can leave notes hanging: reset the local MIDI input. */
    struct fm1_ble_status ble = {0};
    fm1_ble_get_status(&ble);
    unsigned linked = ble.state == FM1_BLE_CONNECTED || ble.state == FM1_BLE_READY ||
                      ble.state == FM1_BLE_DISCOVERING;
    if (ble_was_linked && !linked) retry_reset = 1;
    ble_was_linked = linked;
    unsigned input_changed = master_changed || controls.buttons != previous_buttons ||
                             controls.keys != previous_keys || encoder_active(controls.encoders);
    apply_controls(&controls, now, &ble);
    /* Input changes do not wait for the animation timer. LCD work is bounded
     * separately and continues on later polls, even between animation frames. */
    if (now - frame_ms >= 33U || input_changed || saved_cache.shown == ~0U) {
        frame_ms = now;
        draw();
    }
    paint_flush();
}
void groove_screen_mmc(unsigned command)
{
    unsigned transport = command == 1 || command == 9 ? 0xfc :
                         command == 2 || command == 3 || command == 6 ? 0xfb : 0;
    if (!transport && command != 7) return;
    writing = rec_wanted = command == 6;
    armed = 0;
    playing_at = fm1_millis();
    groove_edit_record(writing, synth_mode ? SYNTH : KIT, 0);
    if (transport) (void)post((uint8_t)transport, 0, 0, 1);
}
#ifdef FM1_HOST_TEST
/* Browser preview hooks. The workbench page is shared with FM1 Open, so its
 * page shortcuts are accepted but there is only this one page to show. */
struct web_target { int32_t x, y, w, h, action, index, owner, raw; char label[16], value[16]; };
static struct web_target web_targets[4];
int groove_screen_preset(unsigned slot)
{
    if (slot) return GROOVE_PROJECT_INVALID;
    return groove_factory_load(0);
}
unsigned groove_screen_preset_position(void) { return (1U << 8) | (kit_ready << 9); }
int groove_screen_open(unsigned target) { return target ? -1 : 0; }
unsigned groove_screen_web_context(void)
{
    const unsigned context[] = {drum, knob_bank, synth_mode};
    return signature(2166136261U, context, sizeof context);
}
unsigned groove_screen_web_mask(void) { return view_page != PAGE_KIT ? 0U : frame_view.clock_mode && !knob_bank ? 7U : 15U; }
const void *groove_screen_web_targets(void)
{
    draw();
    memset(web_targets, 0, sizeof web_targets);
    for (unsigned col = 0; col < 4; ++col) {
        struct web_target *t = &web_targets[col];
        t->x = 0; t->y = 194 + 9 * (int)col; t->w = 152; t->h = 8; t->index = (int)col; t->owner = KIT;
        memcpy(t->label, web_knobs[col][0], 15);
        memcpy(t->value, web_knobs[col][1], 15);
    }
    return web_targets;
}
unsigned groove_screen_web_target_count(void) { return 4; }
int groove_screen_web_adjust(unsigned context, unsigned action, unsigned index, unsigned owner, int delta)
{
    if (context != groove_screen_web_context()) return 0;
    if (!delta || delta < -127 || delta > 127 || action || owner != KIT || index >= 4 ||
        !(groove_screen_web_mask() & (1U << index))) return -1;
    return (int)index + 3; /* Encoder index + 1. */
}
const char *groove_screen_web_knobs(void) { return &web_knobs[0][0][0]; }
unsigned groove_screen_web_first(void) { return 0; }
int groove_screen_web_step(unsigned step, unsigned toggle)
{
    (void)step; (void)toggle;
    return -1; /* no sequencer */
}
/* Read-only panel state for the browser simulator. */
void groove_screen_snapshot(uint8_t state[8])
{
    memset(state, 0, 8);
    state[0] = synth_mode ? SYNTH : KIT;
    state[3] = (uint8_t)drum;
    state[7] = (uint8_t)(octave + 24);
}
#endif
