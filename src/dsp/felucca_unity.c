/* SPDX-License-Identifier: GPL-3.0-only */
/* Felucca's sound side (engines, voices, modulation, FX) in one translation unit, the way Felucca's own
 * tests/hostsim.c builds it on a computer. Sources unchanged in vendor/felucca/src. */
#include <stdint.h>
#include <string.h>
#ifndef FELUCCA_SAMPLE
#define FELUCCA_SAMPLE 0
#endif
#ifndef FELUCCA_SLICE
#define FELUCCA_SLICE 0
#endif
#define __attribute__(x)
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "libc.c"
#undef memset
#undef memcpy
#undef memcmp
static struct { volatile uint32_t notes, buttons; } fm1_in;
#include "core.h"
#include "engines.c"
#include "params.c"
#include "mod.c"
#include "voice.c"
#include "slicer.c"
#include "fx.c"
/* Felucca's USB-MIDI rings (usb.c, hardware-only here): Opal feeds notes straight to the parts, so the input
 * ring stays empty; the declarations are what seq.c reads. */
#define MQ 64u
static uint32_t midi_in_q[MQ], midi_out_q[MQ];
static uint32_t midi_in_ms[MQ];
static uint8_t midi_in_source[MQ];
static volatile uint32_t mi_w, mi_r, mo_w, mo_r;
static volatile uint8_t midi_in_overflow;
static void midi_out_event(uint32_t pkt) { (void)pkt; }   /* Opal sends no MIDI of its own */
#include "song_chain.c"
#include "seq.c"
