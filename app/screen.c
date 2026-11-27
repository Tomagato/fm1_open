/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Beach 909 panel.
 * Modified from FM1 Open V14 app/screen.c, Copyright (c) 2026 M-VAVE.
 * Changed in November 2026 for the Beach 909 firmware: the multi-page
 * instrument UI was replaced by one drum-machine page with a sunset scene.
 * The band painter, font and input helpers are kept from the original. */
#include "instrument.h"
#include "sampledrum.h"
#include "version.h"
#include "fm1_engine_control.h"
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
/* One page: the kit lives on track 8 (MIDI channel 10). The eight black keys
 * from F#3 pick and play a drum, the sixteen white keys toggle its steps in
 * the bar on screen. All drawing and edits run on the control task. */
#include "beach_scene.inc"
enum { OCT_DOWN, OCT_UP, FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC };
#define BUTTON(b) (1U << (b))
enum { KIT = 7, KIT_SLOT = GROOVE_PROJECT_SLOTS - 1, SCENE_ROWS = BEACH_H * 2, SEA_ROW = BEACH_HORIZON * 2,
       RED = 0xf9a6, INK = 0x2108, INK_BEAT = 0x39cf, INK_LANE = 0x4a35, HEAD = 0x7bf7 };
static const uint8_t white_keys[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t pad_keys[8] = {1, 3, 5, 8, 10, 13, 15, 17};
/* Screen and key order: BD SD CH OH CP RS TM CR, mapped onto the kit's pads. */
static const uint8_t lane_pad[8] = {0, 1, 6, 7, 3, 2, 4, 5}, pad_lane[8] = {0, 1, 5, 4, 6, 7, 2, 3};
static const uint16_t lane_color[8] = {
    BEACH_RGB(255, 100, 110), BEACH_RGB(255, 150, 70),  BEACH_RGB(255, 208, 96),  BEACH_RGB(255, 236, 170),
    BEACH_RGB(240, 110, 180), BEACH_RGB(180, 130, 250), BEACH_RGB(100, 150, 255), BEACH_RGB(70, 214, 200)};
static const char kit_name[] = "BEACH 909";
static const char help[] = "FX:KNOBS SCL:BARS ENV:VAPE LFO:GAME";
enum { PAGE_KIT = 1, PAGE_GAME, PAGE_TERMS, PAGE_SPLASH, GAME_W = 8, GAME_H = 14 };

static unsigned track = KIT, cursor, selected_note, drum, writing;
static unsigned kit_ready, knob_bank, bar, follow, armed, edit_fired, ble_was_linked;
static uint32_t kit_retry, edit_since, scene_at, glo_since, seek_until, seek_at;
static unsigned glo_fired, seeking;
static unsigned view_page = PAGE_SPLASH, home_fired, last_step;
static uint32_t splash_at, home_since, glitch_until, vape_at;
/* Scene effects: a torn, colour-split frame on snare and clap hits, and the
 * vapor cloud blown by ENV. Both are read by the band painter. */
static unsigned glitch, vape_on, vape_age, vape_fade, vape_saved, vape_clean;
/* Tide Blocks: a small falling-block game that runs while the pattern plays. */
static uint8_t game_cells[GAME_H][GAME_W];
static struct { int x, y; unsigned kind, turn; } game_piece;
static unsigned game_lines, game_over;
static uint32_t game_random, game_fall_at;
static unsigned scene_phase, scene_pulse;
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
static uint64_t frame_lanes[8];
static uint8_t frame_pots[GROOVE_TONE_PARAMS], frame_sends[2], frame_effects[4];
static unsigned frame_step_drums, frame_swing, frame_samples, frame_dirty;
struct draw_cache {
    unsigned shown;
    uint32_t scene, sea, sun, effects, status, hint, foot[4], lane[8];
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
    if (vape_age)
        for (int k = 0; k < 4; ++k) {
            int dx = (int)x - (16 + k * 13 + (int)vape_age * 2), dy = (int)y - (47 - k * 6 - (int)vape_age);
            int r = 5 + k * 2 + (int)vape_age / 3, d = dx * dx + dy * dy;
            unsigned grain = beach_hash(x + vape_age, y) & 7U;
            if (d <= r * r && (unsigned)(d * 16 / (r * r)) + 15U - vape_fade < 11U + grain)
                return d * 2 < r * r ? BEACH_RGB(246, 236, 250) : BEACH_RGB(206, 178, 226);
        }
    uint16_t color = beach_scene_pixel(x, y, scene_phase, scene_pulse);
    if (torn == 1) color = (uint16_t)((color & 0x07e0U) | (color >> 11) | (color << 11)); /* swap red and blue */
    else if (torn == 2) color |= 0xf81fU;
    return color;
}
static void scene_draw(void)
{
    uint32_t effects = glitch | (vape_age << 8) | (vape_fade << 16);
    if (!paint_rendering) {
        if (cache->effects != effects) cache->scene = ~0U;
        cache->effects = effects;
        if (cache->scene != 1) {
            cache->scene = 1;
            paint_mark(0, 0, FM1_SCREEN_WIDTH, SCENE_ROWS);
        } else {
            if (cache->sea != scene_phase) {
                paint_mark(0, SEA_ROW, FM1_SCREEN_WIDTH, SCENE_ROWS - SEA_ROW);
                if ((cache->sea ^ scene_phase) & ~3U) paint_mark(0, 0, FM1_SCREEN_WIDTH, 26); /* stars */
            }
            if (cache->sun != scene_pulse) paint_mark(112, 26, 80, SEA_ROW - 26);
        }
        cache->sea = scene_phase;
        cache->sun = scene_pulse;
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
static const char *drum_title(unsigned pad)
{
    static const char *const titles[8][2] = {
        {"BD", "BASS DRUM"}, {"SD", "SNARE"},   {"CH", "CLSD HAT"}, {"OH", "OPEN HAT"},
        {"CP", "HAND CLAP"}, {"RS", "RIM SHOT"}, {"TM", "TOM"},      {"CR", "CRASH"}};
    const char *name = groove_pad_name(KIT, pad);
    for (unsigned i = 0; i < 8; ++i)
        if (name[0] == titles[i][0][0] && name[1] == titles[i][0][1] && (name[2] == ' ' || !name[2]))
            return titles[i][1];
    return name;
}
static void status_draw(void)
{
    if (paint_rendering && (paint_top >= 128 || paint_top + PAINT_ROWS <= 112)) return;
    const struct groove_view *v = &frame_view;
    unsigned bars = v->sequence_length / 16U, blink = (frame_time / 250U) & 1U;
    unsigned recording = writing || (armed && blink);
    const unsigned state[] = {drum, v->tempo, v->playing, recording, bar, bars, v->clock_mode,
                              v->playing ? v->play_step / 16U : 4U, frame_dirty};
    uint32_t sig = signature(5381, state, sizeof state);
    if (sig == cache->status) return;
    cache->status = sig;
    paint_fill(0, 112, FM1_SCREEN_WIDTH, 16, BACK);
    if (v->playing)
        for (unsigned i = 0; i < 4; ++i) paint_fill(4 + 2 * i, 114 + 2 * i, 2, 13 - 4 * i, GREEN);
    else
        paint_fill(4, 115, 8, 10, DIM);
    char name[10] = {0};
    strncpy(name, drum_title(drum), 9);
    text(16, 113, name, 2, lane_color[pad_lane[drum]], BACK);
    for (unsigned b = 0; b < 4; ++b) {
        paint_fill(128 + b * 8, 116, 6, 7, b == bar ? TEXT : b < bars ? INK_LANE : INK);
        if (v->playing && b == v->play_step / 16U) paint_fill(128 + b * 8, 124, 6, 2, GREEN);
    }
    char tempo[12] = "EXT";
    if (!v->clock_mode || v->tempo) decimal(tempo, v->tempo, 3);
    text(164, 113, tempo, 2, v->clock_mode ? AMBER : TEXT, BACK);
    text(202, 120, v->clock_mode ? "MIDI" : "BPM", 1, DIM, BACK);
    if (recording) paint_fill(230, 116, 7, 7, RED);
    else if (frame_dirty) paint_fill(232, 118, 3, 3, AMBER);
}
static void grid_draw(void)
{
    if (paint_rendering && (paint_top >= 193 || paint_top + PAINT_ROWS <= 129)) return;
    const struct groove_view *v = &frame_view;
    unsigned head = v->playing && v->play_step / 16U == bar ? (v->play_step & 15U) : 16U;
    for (unsigned lane = 0; lane < 8; ++lane) {
        unsigned y = 129 + lane * 8, pad = lane_pad[lane], selected = pad == drum;
        if (paint_rendering && (y >= paint_top + PAINT_ROWS || y + 8 <= paint_top)) continue;
        const char *name = groove_pad_name(KIT, pad);
        char label[3] = {name[0], name[0] ? name[1] : 0, 0};
        uint32_t sig = selected | ((uint32_t)(uint8_t)label[0] << 8) | ((uint32_t)(uint8_t)label[1] << 16);
        if (sig != cache->lane[lane]) {
            cache->lane[lane] = sig;
            uint16_t bg = selected ? lane_color[lane] : BACK;
            paint_fill(1, y, 16, 7, bg);
            text(3, y, label, 1, selected ? BACK : lane_color[lane], bg);
        }
        unsigned steps = (unsigned)(frame_lanes[lane] >> (bar * 16U)) & 0xffffU;
        for (unsigned i = 0; i < 16; ++i) {
            unsigned on = (steps >> i) & 1U;
            uint8_t cell = (uint8_t)(on | ((i == head) << 1) | (selected << 2));
            if (cell == cache->cell[lane][i]) continue;
            cache->cell[lane][i] = cell;
            paint_fill(21 + i * 13 + (i / 4) * 3, y, 11, 6,
                       on ? (i == head ? TEXT : lane_color[lane])
                          : i == head ? HEAD : selected ? INK_LANE : i % 4 ? INK : INK_BEAT);
        }
    }
}
#ifdef FM1_HOST_TEST
static char web_knobs[4][2][16];
#endif
static void knob(unsigned col, const char *label, const char *value)
{
#ifdef FM1_HOST_TEST
    memset(web_knobs[col], 0, sizeof web_knobs[col]);
    strncpy(web_knobs[col][0], label, 15);
    strncpy(web_knobs[col][1], value, 15);
#endif
    if (paint_rendering && (paint_top >= 227 || paint_top + PAINT_ROWS <= 195)) return;
    uint16_t color = lane_color[knob_bank ? 4 + col : col];
    uint32_t sig =
        signature(signature(color, label, (unsigned)strlen(label)), value, (unsigned)strlen(value));
    if (cache->foot[col] == sig)
        return;
    cache->foot[col] = sig;
    unsigned x = 6 + 59 * col;
    paint_fill(x, 195, 54, 32, BACK);
    paint_fill(x, 195, 54, 2, color);
    text(x, 200, label, 1, TEXT, BACK);
    text(x, 212, value, strlen(value) <= 4 ? 2 : 1, color, BACK);
}
static void knobs_draw(void)
{
    static const char *const labels[2][4] = {{"LEVEL", "TUNE", "DECAY", "TEMPO"},
                                             {"REVERB", "DELAY", "FEEDBACK", "SWING"}};
    int values[4];
    if (knob_bank) {
        values[0] = frame_sends[0]; values[1] = frame_sends[1];
        values[2] = frame_effects[2]; values[3] = (int)frame_swing;
    } else {
        values[0] = frame_pots[SAMPLE_DRUM_LEVEL];
        values[1] = (int)frame_pots[SAMPLE_DRUM_TUNE] - 24;
        values[2] = frame_pots[SAMPLE_DRUM_LENGTH];
        values[3] = frame_view.tempo;
    }
    for (unsigned col = 0; col < 4; ++col) {
        char value[12];
        decimal(value, values[col], 1);
        if (!knob_bank && col == 1 && values[1] > 0) { value[0] = '+'; decimal(value + 1, values[1], 1); }
        if (!knob_bank && col == 3 && frame_view.clock_mode) strcpy(value, "EXT");
        knob(col, labels[knob_bank][col], value);
    }
}
static void hint_draw(void)
{
    if (paint_rendering && paint_top + PAINT_ROWS <= 231) return;
    const char *line = notice ? notice : frame_samples != 2 ? "NO SAMPLES: WRITE THE KIT FROM THE WEB" : help;
    uint32_t sig = signature(notice != NULL, line, (unsigned)strlen(line));
    if (sig == cache->hint) return;
    cache->hint = sig;
    paint_fill(0, 231, FM1_SCREEN_WIDTH, 7, BACK);
    text(4, 231, line, 1, notice || frame_samples != 2 ? AMBER : DIM, BACK);
}
/* Mizu Records wordmark: the label's name in the panel font, jittered. */
static void splash_draw(void)
{
    if (paint_rendering && paint_top + PAINT_ROWS <= SCENE_ROWS) return;
    static const uint16_t tint[4] = {TEXT, 0x07ff, 0xf81f, TEXT};
    unsigned jitter = (frame_time / 90U) & 7U, shift = jitter == 2 ? 3U : jitter == 5 ? 0U : 1U;
    if (cache->status == jitter) return;
    cache->status = jitter;
    paint_fill(0, 118, FM1_SCREEN_WIDTH, 60, BACK);
    text(69 + shift * 3, 120, "MIZU", 4, tint[jitter & 3U], BACK);
    text(80 - shift * 2, 156, "RECORDS", 2, tint[(jitter + 1U) & 3U], BACK);
    if (cache->hint == 1) return;
    cache->hint = 1;
    text(66, 186, kit_name, 2, lane_color[0], BACK);
    text(48, 212, "FM1 OPEN " FM1_RELEASE_VERSION " - UNOFFICIAL", 1, DIM, BACK);
    text(54, 226, "HOLD HOME FOR THE TERMS", 1, DIM, BACK);
}
static void terms_draw(void)
{
    static const char *const lines[] = {
        "BEACH 909 IS FREE SOFTWARE UNDER GPLV3", "WITH THE FM1 SDK LINKING EXCEPTION.",
        "NO WARRANTY. FLASHING IS AT YOUR OWN",   "RISK AND MAY ERASE DATA OR STOP THE",
        "DEVICE FROM STARTING.",                  "UNOFFICIAL: NOT MADE OR ENDORSED BY",
        "M-VAVE OR ROLAND.",                      "THE MIZU RECORDS NAME AND MARK ARE",
        "NOT LICENSED BY THE GPL.",               "FULL TERMS: TERMS.MD WITH THE SOURCE.",
        "ANY BUTTON: BACK"};
    if (cache->status == 1) return;
    cache->status = 1;
    text(48, 114, "TERMS OF USE", 2, lane_color[2], BACK);
    for (unsigned i = 0; i < sizeof lines / sizeof lines[0]; ++i)
        text(4, 132 + i * 9, lines[i], 1, i == 10 ? DIM : TEXT, BACK);
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
    if (paint_rendering && paint_top + PAINT_ROWS <= SCENE_ROWS) return;
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
        paint_fill(0, 116, 86, 112, BACK);
        paint_fill(154, 116, 86, 112, BACK);
        paint_fill(86, 116, 2, 112, INK_LANE);
        paint_fill(152, 116, 2, 112, INK_LANE);
        text(6, 120, "TIDE", 2, lane_color[7], BACK);
        text(6, 138, "BLOCKS", 2, lane_color[4], BACK);
        text(6, 170, "LINES", 1, DIM, BACK);
        char lines[12];
        decimal(lines, (int)game_lines, 1);
        text(6, 182, lines, 3, TEXT, BACK);
        static const char *const keys[] = {"OCT: MOVE", "SEQ: TURN", "REC: DROP", "PLAY: BEAT", "LFO: EXIT"};
        for (unsigned i = 0; i < 5; ++i) text(162, 120 + i * 11, keys[i], 1, DIM, BACK);
        if (game_over) {
            text(162, 190, "WIPEOUT", 1, AMBER, BACK);
            text(162, 202, "REC: AGAIN", 1, AMBER, BACK);
        }
    }
    for (unsigned row = 0; row < GAME_H; ++row)
        for (unsigned column = 0; column < GAME_W; ++column) {
            if (board[row][column] == cache->game[row][column]) continue;
            cache->game[row][column] = board[row][column];
            paint_fill(88 + column * 8, 116 + row * 8, 8, 8, BACK);
            paint_fill(88 + column * 8, 116 + row * 8, 7, 7, board[row][column] ? lane_color[board[row][column] - 1U] : INK);
        }
}
static void draw_page(void)
{
    if (cache->shown != view_page) {
        memset(cache, 0xff, sizeof *cache);
        cache->shown = view_page;
        paint_fill(0, SCENE_ROWS, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT - SCENE_ROWS, BACK);
    }
    scene_draw();
    if (view_page == PAGE_SPLASH) splash_draw();
    else if (view_page == PAGE_TERMS) terms_draw();
    else if (view_page == PAGE_GAME) game_draw();
    else {
        status_draw();
        grid_draw();
        knobs_draw();
        hint_draw();
    }
}
static void leds(void)
{
    const struct groove_view *v = &frame_view;
    uint32_t bits = 0;
    unsigned steps = (unsigned)(frame_lanes[pad_lane[drum]] >> (bar * 16U)) & 0xffffU;
    unsigned bars = v->sequence_length / 16U, blink = (frame_time / 250U) & 1U;
    for (unsigned i = 0; i < 16; ++i)
        if ((steps >> i) & 1U) bits |= 1U << white_keys[i];
    if (v->playing && v->play_step / 16U == bar) bits ^= 1U << white_keys[v->play_step & 15U];
    /* The selected drum stays lit; every drum winks when the pattern hits it. */
    for (unsigned lane = 0; lane < 8; ++lane)
        if ((lane_pad[lane] == drum) != ((frame_step_drums >> lane_pad[lane]) & 1U)) bits |= 1U << pad_keys[lane];
    uint32_t changed = bits ^ led_cache;
    for (unsigned key = 0; key < 27; ++key)
        if (changed & (1U << key)) fm1_led_set(key + 14, (bits >> key) & 1U);
    led_cache = bits;
    bits = ((bar > 0) << OCT_DOWN) | ((bar + 1U < bars) << OCT_UP) | (knob_bank << FX) | (1U << HOME)
        | (frame_dirty << SAVE) | (vape_on << ENV) | ((view_page == PAGE_GAME) << LFO) | ((v->clock_mode != 0) << ARP) | (follow << SEQ)
        | ((v->playing != 0) << PLAY) | ((writing || (armed && blink)) << REC);
    changed = bits ^ button_led_cache;
    for (unsigned key = 0; key < 14; ++key)
        if (changed & (1U << key)) fm1_led_set(key, (bits >> key) & 1U);
    button_led_cache = (uint16_t)bits;
}
static void draw(void)
{
    frame_time = fm1_millis();
    if (notice && (int32_t)(frame_time - notice_until) >= 0) notice = NULL;
    groove_get_view(KIT, cursor, &frame_view);
    frame_step_drums = 0;
    if (frame_view.playing) {
        struct groove_view at = {0};
        groove_get_view(KIT, frame_view.play_step, &at);
        frame_step_drums = at.step.drums;
    }
    for (unsigned lane = 0; lane < 8; ++lane) frame_lanes[lane] = groove_drum_steps(KIT, lane_pad[lane]);
    memset(frame_pots, 0, sizeof frame_pots);
    groove_tone_copy(KIT, drum, frame_pots);
    groove_sends_get(KIT, frame_sends);
    groove_effects_get(frame_effects);
    frame_swing = groove_sequence_setting(0);
    frame_samples = groove_samples_status();
    frame_dirty = groove_project_dirty() != 0;
    /* The water drifts in time; the sun swells on every quarter note. */
    if (frame_time - scene_at >= (frame_view.playing ? 140U : 320U)) {
        scene_at = frame_time;
        ++scene_phase;
    }
    scene_pulse = frame_view.playing && !(frame_view.play_step & 3U);
    /* Snare and clap tear the picture for a moment; the splash never settles. */
    unsigned step = frame_view.playing ? frame_view.play_step + 1U : 0;
    if (step != last_step) {
        last_step = step;
        if (step && (frame_step_drums & 0x0aU)) { glitch = step; glitch_until = frame_time + 90U; }
    }
    if (view_page == PAGE_SPLASH) { glitch = 1U + (frame_time / 120U) % 250U; glitch_until = frame_time + 200U; }
    if (glitch && (int32_t)(frame_time - glitch_until) >= 0) glitch = 0;
    if ((vape_on || vape_age) && frame_time - vape_at >= 110U) {
        vape_at = frame_time;
        if (vape_on) { vape_fade = 15; if (vape_age < 14) ++vape_age; }
        else {
            ++vape_age;
            vape_fade = vape_fade > 2 ? vape_fade - 2U : 0;
            if (!vape_fade) vape_age = 0;
        }
    }
    paint_priority = 0;
    cache = &saved_cache;
    draw_page();
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
            if (!post((uint8_t)(0x80 + groove_midi_input_channel(KIT)), key_note[key], 0, 3))
                return 0;
            key_note[key] = 0xff;
        }
    return 1;
}
static void toggle_step(unsigned step)
{
    struct groove_view v = {0};
    groove_get_view(KIT, step, &v);
    groove_sequence_edit(KIT, step, groove_pad_note(KIT, drum), (v.step.drums & (1U << drum)) ? 0 : 100);
    cursor = step;
}
static int kit_loaded(void)
{
    char name[13];
    groove_project_name(name);
    return groove_source_type(KIT) == GROOVE_SAMPLE && !memcmp(name, kit_name, sizeof kit_name - 1);
}
/* Keep the Beach 909 project in the workspace: the pattern saved in the last
 * project slot when there is one, the built-in pattern otherwise. Projects in
 * the other slots are never written. Flash may be busy at power-on; retry. */
static void kit_prepare(uint32_t now)
{
    if (kit_loaded()) {
        if (!kit_ready) {
            kit_ready = 1;
            drum = cursor = bar = writing = armed = 0;
            follow = 1;
        }
        /* Settings left by other firmware must not silence the kit. */
        if (groove_track_role(KIT) != GROOVE_TRACK_NORMAL) (void)groove_track_role_set(KIT, GROOVE_TRACK_NORMAL);
        if (groove_midi_setting(KIT, 0) == 2) groove_settings_midi(KIT, 0, -1);
        return;
    }
    kit_ready = 0;
    if ((int32_t)(now - kit_retry) < 0) return;
    char stored[13];
    int r = groove_project_info(KIT_SLOT, stored);
    if (r == FM1_FLASH_BUSY) return;
    r = !r && !memcmp(stored, kit_name, sizeof kit_name - 1) ? groove_project_load_slot(KIT_SLOT) : groove_factory_load(0);
    if (r == FM1_FLASH_BUSY) return;
    if (r || !kit_loaded()) r = groove_factory_load(0);
    if (!r) groove_project_clean(); /* SAVE lights only for the player's own edits. */
    else {
        kit_retry = now + 1000U;
        say("KIT LOAD FAILED - RETRYING", now, 1500);
    }
}
static void clear_lane(void)
{
    uint64_t steps = groove_drum_steps(KIT, drum);
    for (unsigned step = 0; step < GROOVE_STEPS; ++step)
        if ((steps >> step) & 1U) groove_sequence_edit(KIT, step, groove_pad_note(KIT, drum), 0);
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
/* ENV blows vapor: the kit drowns in reverb while the button is held. */
static void vape_control(const struct fm1_controls *controls, uint32_t pressed)
{
    if ((pressed & BUTTON(ENV)) && !vape_on) {
        uint8_t sends[2];
        groove_sends_get(KIT, sends);
        vape_saved = sends[0];
        vape_clean = !groove_project_dirty();
        groove_sends_edit(KIT, 0, 15);
        vape_on = 1;
        vape_age = 1;
    } else if (vape_on && !(controls->buttons & BUTTON(ENV))) {
        groove_sends_edit(KIT, 0, (int)vape_saved - 15);
        if (vape_clean) groove_project_clean();
        vape_on = 0;
    }
}
static void apply_controls(const struct fm1_controls *controls, uint32_t now, const struct fm1_ble_status *ble)
{
    uint32_t pressed = controls->buttons & ~previous_buttons;
    uint32_t key_down = controls->keys & ~previous_keys;
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
    groove_get_view(KIT, cursor, &view);
    unsigned channel = groove_midi_input_channel(KIT);

    /* Pages: splash at power-on, terms on a held HOME, the game on LFO. */
    if (view_page == PAGE_SPLASH) {
        if (!pressed && !key_down && !encoder_active(controls->encoders) && now - splash_at < 2600U) return;
        view_page = PAGE_KIT;
    } else if (view_page == PAGE_TERMS) {
        if (pressed) { view_page = PAGE_KIT; blocked_keys = controls->keys; }
        return;
    }
    if (pressed & BUTTON(HOME)) { home_since = now; home_fired = 0; }
    if ((controls->buttons & BUTTON(HOME)) && !home_fired && now - home_since >= 1000U) {
        home_fired = 1;
        if (!release_keys()) return;
        view_page = PAGE_TERMS;
        return;
    }
    vape_control(controls, pressed);
    if (pressed & BUTTON(LFO)) {
        if (!release_keys()) return;
        view_page = view_page == PAGE_GAME ? PAGE_KIT : PAGE_GAME;
        if (view_page == PAGE_GAME && (game_over || !game_random)) game_reset(now);
        game_fall_at = now;
    }
    if (view_page == PAGE_GAME) {
        if ((pressed & BUTTON(PLAY)) && !post(view.playing ? 0xfc : 0xfa, 0, 0, 1)) return;
        game_control(controls, pressed, now);
        return;
    }

    /* Transport and live recording. */
    writing = groove_edit_recording();
    if (pressed & BUTTON(PLAY)) {
        if (!post(view.playing ? 0xfc : 0xfa, 0, 0, 1)) return;
        if (!view.playing) follow = 1;
        else armed = 0;
    }
    if (pressed & BUTTON(REC)) {
        if (writing || armed) writing = armed = 0;
        else if (view.playing) writing = 1;
        else {
            if (!post(0xfa, 0, 0, 1)) return;
            armed = follow = 1;
        }
    }
    if (armed && view.playing) { armed = 0; writing = 1; }
    else if (writing && !view.playing && !armed) writing = 0;
    groove_edit_record(writing, KIT, cursor);

    /* Bars: SCL sets the pattern length, OCT picks the bar on screen. */
    unsigned bars = view.next_length / 16U;
    if (pressed & BUTTON(SCL)) {
        static const char *const lengths[4] = {"PATTERN: 1 BAR", "PATTERN: 2 BARS", "PATTERN: 3 BARS", "PATTERN: 4 BARS"};
        bars = bars % 4U + 1U;
        groove_sequence_length_set(KIT, bars * 16U);
        say(lengths[bars - 1], now, 1500);
    }
    if (pressed & (BUTTON(OCT_DOWN) | BUTTON(OCT_UP))) {
        bar = (unsigned)clamp((int)bar + (pressed & BUTTON(OCT_UP) ? 1 : -1), 0, (int)bars - 1);
        follow = 0;
    }
    if (pressed & BUTTON(SEQ)) follow = 1;
    if (follow && view.playing) bar = view.play_step / 16U;
    if (bar >= bars && !(follow && view.playing)) bar = bars - 1U;
    if (bar > 3) bar = 3;

    if (pressed & BUTTON(FX)) knob_bank ^= 1U;
    if (pressed & BUTTON(ARP)) {
        unsigned external = groove_clock_mode() != GROOVE_CLOCK_MIDI;
        groove_settings_clock(external ? GROOVE_CLOCK_MIDI : GROOVE_CLOCK_INTERNAL);
        say(external ? "SYNC: MIDI CLOCK" : "SYNC: INTERNAL", now, 1500);
    }
    if ((pressed & BUTTON(SAVE)) && vape_on) say("RELEASE ENV, THEN SAVE", now, 1500);
    else if (pressed & BUTTON(SAVE)) {
        int r = groove_project_save_slot(KIT_SLOT);
        say(r == FM1_FLASH_BUSY ? "BUSY - PRESS SAVE AGAIN" : r ? "SAVE FAILED" : "PATTERN SAVED", now, 1500);
    }
    if (pressed & BUTTON(EDIT)) {
        edit_since = now;
        edit_fired = 0;
        say("HOLD EDIT TO CLEAR THIS DRUM", now, 1200);
    }
    if ((controls->buttons & BUTTON(EDIT)) && !edit_fired && now - edit_since >= 700U) {
        edit_fired = 1;
        clear_lane();
        say("DRUM CLEARED", now, 1500);
    }

    /* Encoders: the outer three pick the drum and tempo, K1-K4 follow the footer. */
    int move = controls->encoders[0] + controls->encoders[1];
    if (move) drum = lane_pad[clamp((int)pad_lane[drum] + move, 0, 7)];
    if (controls->encoders[6]) groove_tempo_edit(controls->encoders[6]);
    for (unsigned col = 0; col < 4; ++col) {
        int16_t turn = controls->encoders[2 + col];
        if (!turn) continue;
        if (knob_bank) {
            if (col < 2) groove_sends_edit(KIT, col, turn);
            else if (col == 2) groove_effects_edit(2, encoder_delta(2 + col, turn));
            else groove_sequence_setting_edit(0, turn);
        } else if (col == 3) {
            groove_tempo_edit(turn);
        } else {
            static const uint8_t parameter[3] = {SAMPLE_DRUM_LEVEL, SAMPLE_DRUM_TUNE, SAMPLE_DRUM_LENGTH};
            groove_tone_edit(KIT, drum, parameter[col], col == 1 ? turn : encoder_delta(2 + col, turn));
        }
        if (!knob_bank && col == 3 && groove_clock_mode() == GROOVE_CLOCK_MIDI) say("MIDI CLOCK - ARP FOR INTERNAL", now, 2000);
    }

    /* Keys: black keys play and select drums, white keys write steps. */
    uint32_t keys = controls->keys & ~blocked_keys;
    for (unsigned lane = 0; lane < 8; ++lane) {
        unsigned key = pad_keys[lane], down = (keys >> key) & 1U, note = groove_pad_note(KIT, lane_pad[lane]);
        if (key_note[key] != 0xff && !down) {
            if (!post((uint8_t)(0x80 + channel), key_note[key], 0, 3)) return;
            key_note[key] = 0xff;
        }
        if (down && key_note[key] == 0xff && note < 128) {
            if (!post((uint8_t)(0x90 + channel), (uint8_t)note, 100, 3)) return;
            key_note[key] = (uint8_t)note;
            drum = lane_pad[lane];
        }
    }
    for (unsigned i = 0; i < 16; ++i)
        if ((key_down & keys) & (1U << white_keys[i])) toggle_step(bar * 16U + i);
    selected_note = groove_pad_note(KIT, drum);
}
void groove_screen_init(void)
{
    track = KIT;
    cursor = drum = writing = kit_ready = knob_bank = bar = armed = edit_fired = ble_was_linked = seeking = 0;
    follow = 1;
    kit_retry = scene_at = splash_at = fm1_millis();
    view_page = PAGE_SPLASH;
    home_fired = last_step = glitch = vape_on = vape_age = vape_fade = 0;
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
    writing = command == 6;
    armed = 0;
    groove_edit_record(writing, KIT, cursor);
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
    const unsigned context[] = {drum, knob_bank, bar};
    return signature(2166136261U, context, sizeof context);
}
unsigned groove_screen_web_mask(void) { return frame_view.clock_mode && !knob_bank ? 7U : 15U; }
const void *groove_screen_web_targets(void)
{
    draw();
    memset(web_targets, 0, sizeof web_targets);
    for (unsigned col = 0; col < 4; ++col) {
        struct web_target *t = &web_targets[col];
        t->x = 6 + 59 * (int)col; t->y = 195; t->w = 54; t->h = 32; t->index = (int)col; t->owner = KIT;
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
unsigned groove_screen_web_first(void) { return bar * 16U; }
int groove_screen_web_step(unsigned step, unsigned toggle)
{
    if (!kit_ready || step >= frame_view.sequence_length || toggle > 1) return -1;
    view_page = PAGE_KIT;
    bar = step / 16U;
    follow = 0;
    if (toggle) toggle_step(step);
    return 0;
}
/* Read-only panel state for the browser simulator. */
void groove_screen_snapshot(uint8_t state[8])
{
    memset(state, 0, 8);
    state[0] = KIT;
    state[2] = (uint8_t)cursor;
    state[3] = (uint8_t)drum;
    state[4] = (uint8_t)writing;
    state[7] = 24;
}
#endif
