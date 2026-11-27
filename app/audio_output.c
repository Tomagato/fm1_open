/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "audio_output.h"
#include "fm1_engine_control.h"
#include "fm1_audio_io.h"

static unsigned output_gain;
static int32_t usb_pcm[FM1_BLOCK_WORDS] __attribute__((aligned(32)));
static int16_t bluetooth_pcm[FM1_BLOCK_WORDS] __attribute__((aligned(32)));

/* Master effects state. The control task writes three words; the audio
 * interrupt reads them once per block. Everything here stays in SRAM. */
enum { FX_MEMORY = 8192 }; /* mono, about 186 ms at 44.1 kHz */
static volatile unsigned fx_mask, fx_depth, fx_rate;
static int16_t fx_memory[FX_MEMORY];
static struct {
    uint32_t write, loop_start, loop_at, ring, chop;
    int32_t held[2], low[2], high[2];
    unsigned hold, looping;
} fx;
void fm1_master_fx(unsigned mask, unsigned depth, unsigned rate)
{
    fx_depth = depth > 15U ? 15U : depth;
    fx_rate = rate > 15U ? 15U : rate;
    fx_mask = mask;
}
void fm1_output_reset(void) { output_gain = 0; }
static inline __attribute__((always_inline)) int32_t limit(int64_t value)
{
    /* Bias signed 24-bit PCM into [0, 2^24). Unsigned arithmetic also
     * defines the check at the int64 limits, without two wide compares. */
    if (((uint64_t)value + 8388608U) >> 24)
        return value < 0 ? -8388608 : 8388607;
    return (int32_t)value;
}
/* Ten destructive effects on the synthesized block, in a fixed order: memory
 * (stutter, reverse), then distortion, then rate and gating, then filters. */
static inline __attribute__((always_inline)) void master_effects(int32_t *output)
{
    const unsigned mask = fx_mask, depth = fx_depth, rate = fx_rate;
    const int32_t top = 8388607;
    const unsigned length = 512U + rate * 448U;                 /* 12 to 164 ms */
    const unsigned repeat = (mask >> MASTER_STUTTER | mask >> MASTER_REVERSE) & 1U;
    if (repeat && !fx.looping) { fx.loop_start = fx.write - length; fx.loop_at = 0; }
    fx.looping = repeat;
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; i += 2) {
        int32_t left = output[i], right = output[i + 1];
        if (repeat) {
            /* Play the captured slice round and round, backwards for REVERSE. */
            unsigned at = (mask >> MASTER_REVERSE) & 1U ? length - 1U - fx.loop_at : fx.loop_at;
            left = right = (int32_t)fx_memory[(fx.loop_start + at) & (FX_MEMORY - 1U)] * 256;
            if (++fx.loop_at >= length) fx.loop_at = 0;
        } else fx_memory[fx.write++ & (FX_MEMORY - 1U)] = (int16_t)((left + right) >> 9);
        for (unsigned side = 0; side < 2; ++side) {
            int64_t wide = side ? right : left;
            if (mask & (1U << MASTER_DRIVE)) {
                wide <<= 2U + depth / 4U; /* slam it into the rails, then halve it so it is not twice as loud */
                wide = (wide > top ? top : wide < -top ? -top : wide) / 2;
            }
            if (mask & (1U << MASTER_FOLD)) {
                wide *= 3 + (int)depth;
                for (unsigned turns = 0; turns < 24 && (wide > top || wide < -top); ++turns)
                    wide = wide > top ? 2 * (int64_t)top - wide : -2 * (int64_t)top - wide;
            }
            int32_t value = wide > top ? top : wide < -top ? -top : (int32_t)wide;
            if (mask & (1U << MASTER_CRUSH)) {
                unsigned shift = 15U + depth / 2U; /* down to two bits, silence stays silent */
                int32_t size = value < 0 ? -value : value;
                size = (int32_t)(((uint32_t)size >> shift) << shift);
                value = value < 0 ? -size : size;
            }
            if (mask & (1U << MASTER_RING)) { if (fx.ring & 0x80000000U) value = -value; }
            if (mask & (1U << MASTER_DOWNSAMPLE)) {
                if (!fx.hold) fx.held[side] = value;
                value = fx.held[side];
            }
            if ((mask & (1U << MASTER_CHOP)) && (fx.chop & 0x80000000U)) value = 0;
            if (mask & (1U << MASTER_LOWPASS)) {
                fx.low[side] += (value - fx.low[side]) >> (3U + depth / 3U);
                value = fx.low[side];
            }
            if (mask & (1U << MASTER_HIGHPASS)) {
                fx.high[side] += (value - fx.high[side]) >> (1U + (15U - depth) / 4U);
                wide = (int64_t)(value - fx.high[side]) * 2;
                value = wide > top ? top : wide < -top ? -top : (int32_t)wide;
            }
            if (side) right = value; else left = value;
        }
        if (++fx.hold > 2U + rate * 5U) fx.hold = 0;               /* 15 kHz down to 570 Hz */
        fx.ring += (60U + rate * rate * 9U) * 97391U;              /* 60 Hz to 2 kHz, square */
        fx.chop += (3U + rate * 2U) * 97391U;                      /* 3 to 33 Hz */
        output[i] = left;
        output[i + 1] = right;
    }
}
FM1_AUDIO_CODE void fm1_output_mix(int32_t *output)
{
    if (!output) return;
    if (fx_mask) master_effects(output);
    else {
        for (unsigned i = 0; i < FM1_BLOCK_WORDS; i += 2)
            fx_memory[fx.write++ & (FX_MEMORY - 1U)] = (int16_t)((output[i] + output[i + 1]) >> 9);
        fx.looping = 0;
    }
    unsigned target = fm1_engine_output_level();
    if (target > 127U) target = 127U;
    int delta = (int)target - (int)output_gain;
    output_gain = (unsigned)((int)output_gain + (delta > 8 ? 8 : delta < -8 ? -8 : delta));

    uint16_t usb_gain[2] = {0, 0};
    int usb_frames = fm1_usb_audio_read(usb_pcm, FM1_BLOCK_FRAMES, usb_gain);
    unsigned left = ((unsigned)usb_gain[0] * output_gain) >> 7;
    unsigned right = ((unsigned)usb_gain[1] * output_gain) >> 7;
    /* Accumulate USB before clipping, then add Bluetooth.
     * Discard the block if the USB stream resets. */
    for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) {
        int64_t value = ((int64_t)output[i] * output_gain) >> 7;
        if (usb_frames == FM1_BLOCK_FRAMES)
            value += ((int64_t)usb_pcm[i] * ((i & 1U) ? right : left)) >> 7;
        output[i] = usb_frames == -2 ? 0 : limit(value);
    }
    if (fm1_bluetooth_audio_read(bluetooth_pcm, FM1_BLOCK_FRAMES) == FM1_BLOCK_FRAMES) {
        /* output is already clipped to 24 bits. Adding signed 16-bit PCM
         * times at most 254 fits in 25 bits, so this stage needs no int64. */
        for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) {
            int32_t value = output[i] + (int32_t)bluetooth_pcm[i] * (int32_t)(output_gain * 2U);
            output[i] = value > 8388607 ? 8388607 : value < -8388608 ? -8388608 : value;
        }
    }
    (void)fm1_usb_audio_write(output, FM1_BLOCK_FRAMES);
}
