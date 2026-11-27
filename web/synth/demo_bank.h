/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Synthesized 909-style drum kit for browser initialization (Beach 909). */
#ifndef FM1_DEMO_BANK_H
#define FM1_DEMO_BANK_H
#include <math.h>

static void demo_put32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8*i));
}
/* One-pole/biquad helpers for the drum voices below. */
struct demo_biquad { double b0, b1, b2, a1, a2, x1, x2, y1, y2; };
static void demo_filter(struct demo_biquad *f, int high, double hz, double q)
{
    double w = 6.283185307179586*hz/32000, c = cos(w), alpha = sin(w)/(2*q), a0 = 1+alpha;
    f->b1 = (high ? -(1+c) : 1-c)/a0; f->b0 = f->b2 = (high ? 1+c : 1-c)/(2*a0);
    f->a1 = -2*c/a0; f->a2 = (1-alpha)/a0;
    f->x1 = f->x2 = f->y1 = f->y2 = 0;
}
static double demo_run(struct demo_biquad *f, double x)
{
    double y = f->b0*x + f->b1*f->x1 + f->b2*f->x2 - f->a1*f->y1 - f->a2*f->y2;
    f->x2 = f->x1; f->x1 = x; f->y2 = f->y1; f->y1 = y;
    return y;
}
/* 909-style kit synthesized from oscillators and noise: no recordings.
 * Pads follow the factory layout: kick, snare, rim, clap, tom, crash,
 * closed hat, open hat (the last two share a choke group). */
enum { DEMO_BD, DEMO_SD, DEMO_RS, DEMO_CP, DEMO_TM, DEMO_CR, DEMO_CH, DEMO_OH };
static const uint8_t *demo_bank(unsigned *bytes)
{
    static uint8_t *bank;
    static unsigned total;
    static const char *const names[SAMPLE_DRUM_PADS] = {
        "BD KICK", "SD SNARE", "RS RIM", "CP CLAP", "TM TOM", "CR CRASH", "CH HAT", "OH HAT"
    };
    static const unsigned duration[SAMPLE_DRUM_PADS] = {14400,8960,2240,9600,11520,40000,2560,14080};
    static const unsigned level[SAMPLE_DRUM_PADS] = {26000,23000,20000,21000,23000,15000,15000,15000};
    /* Six detuned squares give the cymbals their metallic core. */
    static const double metal[6] = {540.0, 800.5, 1047.2, 1371.9, 1685.3, 2202.6};
    if (!bank) {
        uint8_t *candidate = calloc(SAMPLE_DRUM_BANK_BYTES, 1);
        float *work = malloc(40000 * sizeof *work);
        int16_t *pcm = malloc(40000 * sizeof *pcm);
        unsigned offset = SAMPLE_DRUM_BANK_HEADER;
        uint32_t random = 909;
        if (!candidate || !work || !pcm) goto failed;
        for (unsigned pad = 0; pad < SAMPLE_DRUM_PADS; ++pad) {
            const unsigned frames = duration[pad];
            struct demo_biquad high, low;
            double phase = 0, second = 0, square[6] = {0}, peak = 0;
            unsigned cymbal = pad == DEMO_CH || pad == DEMO_OH || pad == DEMO_CR;
            demo_filter(&high, 1, pad == DEMO_SD ? 1400 : pad == DEMO_CP ? 700 : pad == DEMO_CR ? 3600 : 6800, 0.8);
            demo_filter(&low, 0, pad == DEMO_SD ? 8500 : pad == DEMO_CP ? 1500 : 13000, pad == DEMO_CP ? 1.8 : 0.7);
            for (unsigned i = 0; i < frames; ++i) {
                const double t = i / 32000.0;
                random = random*1664525u + 1013904223u;
                double noise = (double)(random >> 8) / 8388608.0 - 1.0, value = 0;
                if (pad == DEMO_BD) {
                    double hz = 47 + 190*exp(-t*42) + 46*exp(-t*9);
                    phase += hz/32000;
                    value = tanh(1.9*sin(6.283185307179586*phase)*exp(-t*7.2));
                    value += 0.38*noise*exp(-t*600) + (i < 24 ? 0.5*(1 - i/24.0) : 0);
                } else if (pad == DEMO_SD) {
                    phase += (186 + 150*exp(-t*70))/32000; second += (332 + 190*exp(-t*70))/32000;
                    double body = 0.62*sin(6.283185307179586*phase) + 0.38*sin(6.283185307179586*second);
                    value = body*exp(-t*24) + 0.82*demo_run(&low, demo_run(&high, noise))*exp(-t*16);
                    value = tanh(1.4*value);
                } else if (pad == DEMO_RS) {
                    phase += 494.0/32000; second += 1738.0/32000;
                    value = sin(6.283185307179586*phase) + 0.7*sin(6.283185307179586*second) + 0.5*noise*exp(-t*900);
                    value = tanh(2.2*value*exp(-t*95));
                } else if (pad == DEMO_CP) {
                    /* Three quick slaps, then the room tail. */
                    double gate = t < 0.011 ? exp(-t*210) : t < 0.022 ? exp(-(t-0.011)*210)
                                : t < 0.033 ? exp(-(t-0.022)*210) : 0.8*exp(-(t-0.033)*21);
                    value = demo_run(&low, demo_run(&high, noise))*gate;
                } else if (pad == DEMO_TM) {
                    phase += (104 + 96*exp(-t*28))/32000;
                    value = tanh(1.5*sin(6.283185307179586*phase)*exp(-t*8.5)) + 0.16*noise*exp(-t*260);
                } else {
                    double ring = 0;
                    for (unsigned j = 0; j < 6; ++j) {
                        square[j] += metal[j]*(pad == DEMO_CR ? 1.31 : 1.9)/32000;
                        square[j] -= floor(square[j]);
                        ring += square[j] < 0.5 ? 1.0 : -1.0;
                    }
                    value = demo_run(&high, ring/6 + (pad == DEMO_CR ? 0.9 : 0.45)*noise);
                    value *= pad == DEMO_CH ? exp(-t*75)
                           : pad == DEMO_OH ? 0.75*exp(-t*7.5) + 0.25*exp(-t*60)
                                            : 0.7*exp(-t*3.4) + 0.3*exp(-t*26);
                }
                work[i] = (float)value;
            }
            for (unsigned i = 0; i < frames; ++i) {
                double fade = fmin(1, (frames-1-i)/(cymbal ? 1600.0 : 400.0));
                work[i] = (float)(work[i]*fade);
                if (fabs(work[i]) > peak) peak = fabs(work[i]);
            }
            if (!(peak > 0)) goto failed;
            for (unsigned i = 0; i < frames; ++i) pcm[i] = (int16_t)(work[i]*level[pad]/peak);
            unsigned size = ((frames+SAMPLE_DRUM_CHUNK_FRAMES-1)/SAMPLE_DRUM_CHUNK_FRAMES)*SAMPLE_DRUM_CHUNK_BYTES;
            if (size > SAMPLE_DRUM_BANK_BYTES-offset) goto failed;
            uint8_t *encoded = fm1_sea_encode(pcm, frames, sample_dqt);
            if (!encoded) goto failed;
            uint8_t *entry = candidate+32+pad*28;
            demo_put32(entry,offset); demo_put32(entry+4,frames); demo_put32(entry+8,size);
            memcpy(entry+12,names[pad],strlen(names[pad]));
            memcpy(candidate+offset,encoded,size); offset += size; free(encoded);
        }
        /* Existing 512-byte, 16-entry disk layout; the last eight are empty.
         * Eight playable pads; retain the compatible on-disk format. */
        for (unsigned pad = SAMPLE_DRUM_PADS; pad < 16; ++pad) demo_put32(candidate+32+pad*28,offset);
        demo_put32(candidate,0x53314d46); demo_put32(candidate+4,1);
        demo_put32(candidate+8,offset); demo_put32(candidate+20,32000);
        uint32_t crc = UINT32_MAX;
        for (unsigned i = 0; i < offset; ++i) {
            crc ^= candidate[i];
            for (unsigned b = 0; b < 8; ++b) crc = (crc>>1) ^ (0xedb88320u & (0u-(crc&1)));
        }
        demo_put32(candidate+12,~crc);
        free(work); free(pcm); total = offset; bank = candidate;
        goto ready;
failed:
        free(work); free(pcm); free(candidate);
        if (bytes) *bytes = 0;
        return NULL;
    }
ready:
    if (bytes) *bytes = total;
    return bank;
}
#endif