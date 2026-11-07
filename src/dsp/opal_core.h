/* SPDX-License-Identifier: GPL-3.0-only */
/* An Opal core: one independent copy of Felucca's engine (opal_core.c, compiled once per core). */
#ifndef OPAL_CORE_H
#define OPAL_CORE_H
#include <stdint.h>

#define OPAL_NCORES 4   /* one per Move track */

typedef struct {
    void *(*claim)(void);                 /* the core's state, reset; NULL when another instance has it */
    void (*release)(void *core);
    void (*on_midi)(void *core, const uint8_t *msg, int len, int source);
    void (*set_param)(void *core, const char *key, const char *val);
    int (*get_param)(void *core, const char *key, char *buf, int len);
    void (*render)(void *core, int16_t *out, int frames);
    uint32_t (*engines)(void);            /* how many engines a core offers */
} opal_core_t;

extern const opal_core_t opal_core0, opal_core1, opal_core2, opal_core3;
#endif
