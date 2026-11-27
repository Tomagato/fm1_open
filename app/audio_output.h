/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_AUDIO_OUTPUT_H
#define FM1_AUDIO_OUTPUT_H
#include "fm1.h"
/* Reset before start or while the audio ISR is excluded. */
void fm1_output_reset(void);
/* Finish one synthesized block: master gain, USB, Bluetooth, USB recording.
 * Audio ISR only; output holds FM1_BLOCK_WORDS Q23 samples. */
FM1_AUDIO_CODE void fm1_output_mix(int32_t *output);
/* Bitch909 master effects, run on the finished mix inside fm1_output_mix.
 * Control task: a bit mask of effects, and depth and rate from 0 to 15. */
enum { MASTER_CRUSH, MASTER_DOWNSAMPLE, MASTER_DRIVE, MASTER_FOLD, MASTER_RING, MASTER_CHOP,
       MASTER_STUTTER, MASTER_REVERSE, MASTER_LOWPASS, MASTER_HIGHPASS, MASTER_EFFECTS };
void fm1_master_fx(unsigned mask, unsigned depth, unsigned rate);
#endif
