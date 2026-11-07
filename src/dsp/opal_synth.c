/* SPDX-License-Identifier: GPL-3.0-only */
/* Opal Synth: Felucca's engines as a Schwung sound generator (plugin API v2).
 *
 * Each instance claims one of four cores (opal_core.h), each a whole copy of Felucca's engine, so up to four
 * tracks play Opal at once, each with its own engine, sound and FX. A fifth instance reports an error and stays
 * silent. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host/plugin_api_v1.h"
#include "opal_core.h"

typedef struct {
    const opal_core_t *core;   /* NULL: every core is taken */
    void *state;
} opal_inst_t;

static const opal_core_t *const CORES[OPAL_NCORES] = {&opal_core0, &opal_core1, &opal_core2, &opal_core3};

static void *opal_create(const char *module_dir, const char *json_defaults)
{
    opal_inst_t *in = calloc(1, sizeof *in);
    int i;
    (void)module_dir;
    (void)json_defaults;
    if (!in)
        return NULL;
    for (i = 0; i < OPAL_NCORES && !in->core; i++) {
        void *st = CORES[i]->claim();
        if (st) {
            in->core = CORES[i];
            in->state = st;
        }
    }
    return in;
}

static void opal_destroy(void *inst)
{
    opal_inst_t *in = inst;
    if (!in)
        return;
    if (in->core)
        in->core->release(in->state);
    free(in);
}

static void opal_on_midi(void *inst, const uint8_t *msg, int len, int source)
{
    opal_inst_t *in = inst;
    if (in && in->core)
        in->core->on_midi(in->state, msg, len, source);
}

static void opal_set_param(void *inst, const char *key, const char *val)
{
    opal_inst_t *in = inst;
    if (in && in->core)
        in->core->set_param(in->state, key, val);
}

static int opal_get_param(void *inst, const char *key, char *buf, int len)
{
    opal_inst_t *in = inst;
    if (!in || !in->core)
        return -1;
    return in->core->get_param(in->state, key, buf, len);
}

static int opal_get_error(void *inst, char *buf, int len)
{
    opal_inst_t *in = inst;
    if (in && !in->core)
        return snprintf(buf, (size_t)len, "Opal Synth plays on up to %d tracks at a time", OPAL_NCORES);
    return 0;
}

static void opal_render(void *inst, int16_t *out, int frames)
{
    opal_inst_t *in = inst;
    if (in && in->core)
        in->core->render(in->state, out, frames);
    else
        memset(out, 0, (size_t)frames * 2u * sizeof *out);
}

static plugin_api_v2_t g_api = {
    .api_version = MOVE_PLUGIN_API_VERSION_2,
    .create_instance = opal_create,
    .destroy_instance = opal_destroy,
    .on_midi = opal_on_midi,
    .set_param = opal_set_param,
    .get_param = opal_get_param,
    .get_error = opal_get_error,
    .render_block = opal_render,
};

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host)
{
    (void)host;
    return &g_api;
}
