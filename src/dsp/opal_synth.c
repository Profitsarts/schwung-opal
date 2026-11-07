/* SPDX-License-Identifier: GPL-3.0-only */
/* Opal Synth: Felucca's engines as a Schwung sound generator (plugin API v2).
 *
 * Felucca's sound side keeps its state in globals (four parts, trk[0..3], and one mix). Opal plays part 1 only,
 * so one instance owns the engine at a time: a second instance reports an error and stays silent.
 *
 * Felucca renders blocks of CTL (32) frames at 44.1 kHz; Schwung asks for 128, so each block is four of Felucca's.
 * Parameters arrive on Schwung's control path and are plain stores into the part, as Felucca's own UI writes them
 * while its audio interrupt runs. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "host/plugin_api_v1.h"
#include "felucca_unity.c"

#define OPAL_PART 0u

typedef struct {
    int owner;      /* this instance plays the engine */
    int engine;     /* ENGINES[] index */
    int preset;     /* within the engine */
} opal_t;

static const host_api_v1_t *g_host;
static opal_t *g_owner;

/* ---------------------------------------------------------------- engines */

/* the engines Opal offers, in Felucca's display order (ENGINE_ORDER, retired ones left out) */
static uint32_t opal_engine_count(void) { return NENG_SHOWN; }
static uint32_t opal_engine_at(uint32_t n) { return eng_vis(n); }
static int opal_engine_rank(uint32_t e)
{
    uint32_t n;
    for (n = 0; n < NENG_SHOWN; n++)
        if (eng_vis(n) == e)
            return (int)n;
    return -1;
}

/* preset pi of engine e onto part t: the sound, its envelope and its sends; as Felucca's ui.c apply_preset_to */
static void opal_load(track_t *t, uint32_t e, uint32_t pi)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    const engine_t *en = ENGINES[e];
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    t->eng_req = (uint8_t)e;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = en->edit[i].def;
    t->preset = 0;
    if (en->npresets) {
        const preset_t *pr;
        pi %= en->npresets;
        pr = &en->presets[pi];
        t->preset = (uint8_t)pi;
        for (i = 0; i < 8u; i++)
            t->p[P_E0 + i] = pr->e[i];
        t->p[P_ATK] = pr->env[0];
        t->p[P_DEC] = pr->env[1];
        t->p[P_SUS] = pr->env[2];
        t->p[P_REL] = pr->env[3];
        t->p[P_ED_FLT] = pr->fenv;
        t->p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
        for (i = 0; i < 4u; i++)
            t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
    }
    fm6_track_loaded(t);
}

static void opal_engine_init(void)
{
    uint32_t i, k;
    fm6_tables_init();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].time = ST_REST;
        trk[k].p[P_LEVEL] = k == OPAL_PART ? trk[k].p[P_LEVEL] : 0;
    }
    song.master_q12 = 4096;
}

/* ------------------------------------------------------------- parameters */

/* the values beyond the engine's eight: the envelope and level (part), the FX sends (part) and the FX buses
 * (Felucca's globals, shared by the whole mix); key, Schwung name, global?, Felucca id */
typedef struct { const char *key, *name; uint8_t global; uint8_t id; } extra_t;
static const extra_t EXTRA[] = {
    {"atk", "Attack", 0, P_ATK},   {"dec", "Decay", 0, P_DEC},     {"sus", "Sustain", 0, P_SUS},
    {"rel", "Release", 0, P_REL},  {"level", "Level", 0, P_LEVEL},
    {"dist", "Drive", 0, P_DIST},  {"chorus", "Chorus", 0, P_CHOR}, {"delay", "Delay", 0, P_DLY},
    {"reverb", "Reverb", 0, P_REV},
    {"dtime", "D.Time", 1, G_DTIME}, {"dfdbk", "D.Fdbk", 1, G_DFDBK}, {"dcolor", "D.Color", 1, G_DCOLOR},
    {"dmix", "D.Mix", 1, G_DMIX},    {"rsize", "R.Size", 1, G_RSIZE}, {"rdamp", "R.Damp", 1, G_RDAMP},
    {"rtype", "R.Type", 1, G_RTYPE}, {"crate", "C.Rate", 1, G_CRATE}, {"cdepth", "C.Depth", 1, G_CDEPTH},
};
#define NEXTRA ((int)NELEM(EXTRA))

static int opal_extra(const char *key)
{
    int i;
    for (i = 0; i < NEXTRA; i++)
        if (!strcmp(key, EXTRA[i].key))
            return i;
    return -1;
}
static const param_desc_t *extra_desc(int i) { return EXTRA[i].global ? &GP[EXTRA[i].id] : &TP[EXTRA[i].id]; }
static int16_t *extra_val(int i) { return EXTRA[i].global ? &song.g[EXTRA[i].id] : &trk[OPAL_PART].p[EXTRA[i].id]; }

/* "e1".."e8": the engine's eight EDIT values */
static int opal_edit_id(const char *key)
{
    if (key[0] == 'e' && key[1] >= '1' && key[1] <= '8' && !key[2])
        return P_E0 + (key[1] - '1');
    return -1;
}

static void opal_restore(opal_t *o, const char *js);

static void opal_set_param(void *inst, const char *key, const char *val)
{
    opal_t *o = inst;
    track_t *t = &trk[OPAL_PART];
    int id;
    if (!o || !o->owner || !key || !val)
        return;
    {   /* "state" or "<slot>:state": the whole sound, as get_param("state") gave it */
        const char *c = strrchr(key, ':');
        if (!strcmp(c ? c + 1 : key, "state")) {
            opal_restore(o, val);
            return;
        }
    }
    if (!strcmp(key, "engine")) {
        int n = atoi(val), c = (int)opal_engine_count();
        n = ((n % c) + c) % c;
        o->engine = (int)opal_engine_at((uint32_t)n);
        o->preset = 0;
        trk_all_off(t);
        opal_load(t, (uint32_t)o->engine, 0);
        return;
    }
    if (!strcmp(key, "preset")) {
        uint32_t np = ENGINES[o->engine]->npresets;
        int n = atoi(val);
        if (!np)
            return;
        o->preset = (int)(((n % (int)np) + (int)np) % (int)np);
        opal_load(t, (uint32_t)o->engine, (uint32_t)o->preset);
        return;
    }
    id = opal_edit_id(key);
    if (id >= 0) {
        const param_desc_t *ed = &ENGINES[o->engine]->edit[id - P_E0];
        int v = atoi(val);
        t->p[id] = (int16_t)(v < ed->min ? ed->min : v > ed->max ? ed->max : v);
        return;
    }
    id = opal_extra(key);
    if (id >= 0) {
        const param_desc_t *d = extra_desc(id);
        int v = atoi(val);
        *extra_val(id) = (int16_t)(v < d->min ? d->min : v > d->max ? d->max : v);
    }
}

/* ------------------------------------------------------- Schwung's JSON */

typedef struct { char *b; int len, at; } jbuf_t;

static void jput(jbuf_t *j, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (j->at >= j->len)
        return;
    va_start(ap, fmt);
    n = vsnprintf(j->b + j->at, (size_t)(j->len - j->at), fmt, ap);
    va_end(ap);
    j->at = n < 0 ? j->len : (j->at + n > j->len ? j->len : j->at + n);
}

/* one parameter's descriptor: an EDIT value with the engine's own label and range, or an envelope value */
static void jparam(jbuf_t *j, const char *key, const param_desc_t *d)
{
    int i;
    jput(j, "{\"key\":\"%s\",\"name\":\"%s\",", key, d->label ? d->label : key);
    if (d->fmt == F_ENUM && d->names) {
        jput(j, "\"type\":\"enum\",\"options\":[");
        for (i = d->min; i <= d->max; i++)
            jput(j, "%s\"%s\"", i > d->min ? "," : "", d->names[i - d->min] ? d->names[i - d->min] : "");
        jput(j, "]}");
    } else
        jput(j, "\"type\":\"int\",\"min\":%d,\"max\":%d,\"step\":1}", d->min, d->max);
}

static void jengine(jbuf_t *j)
{
    uint32_t n;
    jput(j, "{\"key\":\"engine\",\"name\":\"Engine\",\"type\":\"enum\",\"options\":[");
    for (n = 0; n < opal_engine_count(); n++)
        jput(j, "%s\"%s\"", n ? "," : "", ENGINES[opal_engine_at(n)]->name);
    jput(j, "]}");
}

static void jextra(jbuf_t *j, int k)
{
    param_desc_t d = *extra_desc(k);
    d.label = EXTRA[k].name;
    if (d.fmt != F_ENUM)
        d.fmt = F_INT;
    jparam(j, EXTRA[k].key, &d);
}

/* a level: its params (the EXTRA keys named), knobs = the same keys in order */
static void jlevel(jbuf_t *j, const char *id, const char *name, const char *const *keys, int n, int engine_first,
                   const char *next, const char *next_label)
{
    int i;
    jput(j, "\"%s\":{\"name\":\"%s\",\"params\":[", id, name);
    if (engine_first) {
        jengine(j);
        jput(j, ",");
    }
    for (i = 0; i < n; i++) {
        jextra(j, opal_extra(keys[i]));
        jput(j, ",");
    }
    if (next)
        jput(j, "{\"level\":\"%s\",\"label\":\"%s\"}", next, next_label);
    else
        j->at -= j->at > 0 && j->b[j->at - 1] == ',';
    jput(j, "],\"knobs\":[");
    if (engine_first)
        jput(j, "\"engine\",");
    for (i = 0; i < n; i++)
        jput(j, "%s\"%s\"", i ? "," : "", keys[i]);
    jput(j, "]}");
}

static const char *const ENV_PAGE[] = {"atk", "dec", "sus", "rel", "level"};
static const char *const FX_PAGE[] = {"dist", "chorus", "delay", "reverb", "dtime", "dfdbk", "rsize", "rdamp"};
static const char *const FX2_PAGE[] = {"dcolor", "dmix", "rtype", "crate", "cdepth"};

/* the pages: the engine (preset browser + its eight EDIT values on knobs 1-8), Envelope (engine, ADSR, level),
 * FX (sends, delay time / feedback, reverb size / damping), FX+ (delay color / mix, reverb type, chorus) */
static int opal_hierarchy(const opal_t *o, char *buf, int len)
{
    const engine_t *en = ENGINES[o->engine];
    jbuf_t j = {buf, len, 0};
    char key[4] = "e1";
    int i;
    jput(&j, "{\"levels\":{\"root\":{\"name\":\"%s\",", en->name);
    if (en->npresets)
        jput(&j, "\"list_param\":\"preset\",\"count_param\":\"preset_count\",\"name_param\":\"preset_name\",");
    jput(&j, "\"params\":[");
    for (i = 0; i < 8; i++) {
        key[1] = (char)('1' + i);
        jparam(&j, key, &en->edit[i]);
        jput(&j, ",");
    }
    jput(&j, "{\"level\":\"env\",\"label\":\"Envelope\"}],\"knobs\":[\"e1\",\"e2\",\"e3\",\"e4\",\"e5\",\"e6\",\"e7\",\"e8\"]},");
    jlevel(&j, "env", "Envelope", ENV_PAGE, (int)NELEM(ENV_PAGE), 1, "fx", "FX");
    jput(&j, ",");
    jlevel(&j, "fx", "FX", FX_PAGE, (int)NELEM(FX_PAGE), 0, "fx2", "FX+");
    jput(&j, ",");
    jlevel(&j, "fx2", "FX+", FX2_PAGE, (int)NELEM(FX2_PAGE), 0, NULL, NULL);
    jput(&j, "}}");
    return j.at < len ? j.at : -1;
}

static int opal_chain_params(const opal_t *o, char *buf, int len)
{
    const engine_t *en = ENGINES[o->engine];
    jbuf_t j = {buf, len, 0};
    char key[4] = "e1";
    int i;
    jput(&j, "[");
    jengine(&j);
    for (i = 0; i < 8; i++) {
        key[1] = (char)('1' + i);
        jput(&j, ",");
        jparam(&j, key, &en->edit[i]);
    }
    for (i = 0; i < NEXTRA; i++) {
        jput(&j, ",");
        jextra(&j, i);
    }
    jput(&j, "]");
    return j.at < len ? j.at : -1;
}

/* the whole sound as a flat JSON object of strings: engine, preset, e1..e8, the envelope and the FX */
static int opal_state(const opal_t *o, char *buf, int len)
{
    const track_t *t = &trk[OPAL_PART];
    jbuf_t j = {buf, len, 0};
    int i;
    jput(&j, "{\"engine\":\"%s\",\"preset\":\"%d\"", ENGINES[o->engine]->name, o->preset);
    for (i = 0; i < 8; i++)
        jput(&j, ",\"e%d\":\"%d\"", i + 1, t->p[P_E0 + i]);
    for (i = 0; i < NEXTRA; i++)
        jput(&j, ",\"%s\":\"%d\"", EXTRA[i].key, *extra_val(i));
    jput(&j, "}");
    return j.at < len ? j.at : -1;
}

/* a string value of the flat object: "key":"value" */
static int jfind(const char *js, const char *key, char *out, int len)
{
    char pat[24];
    const char *p, *q;
    snprintf(pat, sizeof pat, "\"%s\":\"", key);
    p = strstr(js, pat);
    if (!p)
        return 0;
    p += strlen(pat);
    q = strchr(p, '"');
    if (!q || q - p >= len)
        return 0;
    memcpy(out, p, (size_t)(q - p));
    out[q - p] = 0;
    return 1;
}

static void opal_restore(opal_t *o, const char *js)
{
    track_t *t = &trk[OPAL_PART];
    char v[32], key[4] = "e1";
    uint32_t n;
    int i;
    if (jfind(js, "engine", v, sizeof v))
        for (n = 0; n < opal_engine_count(); n++)
            if (!strcmp(ENGINES[opal_engine_at(n)]->name, v)) {
                o->engine = (int)opal_engine_at(n);
                break;
            }
    o->preset = jfind(js, "preset", v, sizeof v) ? atoi(v) : 0;
    trk_all_off(t);
    opal_load(t, (uint32_t)o->engine, (uint32_t)o->preset);
    if (ENGINES[o->engine]->npresets)
        o->preset %= (int)ENGINES[o->engine]->npresets;
    for (i = 0; i < 8; i++) {
        key[1] = (char)('1' + i);
        if (jfind(js, key, v, sizeof v))
            opal_set_param(o, key, v);
    }
    for (i = 0; i < NEXTRA; i++)
        if (jfind(js, EXTRA[i].key, v, sizeof v))
            opal_set_param(o, EXTRA[i].key, v);
}

static int opal_get_param(void *inst, const char *key, char *buf, int len)
{
    opal_t *o = inst;
    const engine_t *en;
    int id;
    if (!o || !key || !buf || len <= 0)
        return -1;
    en = ENGINES[o->engine];
    if (!strcmp(key, "ui_hierarchy"))
        return opal_hierarchy(o, buf, len);
    if (!strcmp(key, "chain_params"))
        return opal_chain_params(o, buf, len);
    if (!strcmp(key, "state"))
        return o->owner ? opal_state(o, buf, len) : -1;
    if (!strcmp(key, "engine"))
        return snprintf(buf, (size_t)len, "%d", opal_engine_rank((uint32_t)o->engine));
    if (!strcmp(key, "engine_name"))
        return snprintf(buf, (size_t)len, "%s", en->name);
    if (!strcmp(key, "preset"))
        return snprintf(buf, (size_t)len, "%d", o->preset);
    if (!strcmp(key, "preset_name"))
        return snprintf(buf, (size_t)len, "%s", en->npresets ? en->presets[o->preset].name : "INIT");
    if (!strcmp(key, "preset_count"))
        return snprintf(buf, (size_t)len, "%u", (unsigned)en->npresets);
    /* "e1_name" .. "e8_name": the engine's own label for each EDIT value */
    if (key[0] == 'e' && key[1] >= '1' && key[1] <= '8' && !strcmp(key + 2, "_name"))
        return snprintf(buf, (size_t)len, "%s", en->edit[key[1] - '1'].label);
    id = opal_edit_id(key);
    if (id >= 0)
        return snprintf(buf, (size_t)len, "%d", trk[OPAL_PART].p[id]);
    id = opal_extra(key);
    if (id >= 0)
        return snprintf(buf, (size_t)len, "%d", *extra_val(id));
    return -1;
}

static int opal_get_error(void *inst, char *buf, int len)
{
    opal_t *o = inst;
    if (o && !o->owner)
        return snprintf(buf, (size_t)len, "Opal Synth plays on one track at a time");
    return 0;
}

/* ------------------------------------------------------------------- MIDI */

static void opal_on_midi(void *inst, const uint8_t *msg, int len, int source)
{
    opal_t *o = inst;
    track_t *t = &trk[OPAL_PART];
    uint8_t st;
    (void)source;
    if (!o || !o->owner || len < 1)
        return;
    st = msg[0] & 0xF0;
    if (st == 0x90 && len >= 3 && msg[2])
        trk_note_on(t, msg[1], msg[2]);
    else if ((st == 0x80 || st == 0x90) && len >= 3)
        trk_note_off(t, msg[1]);
    else if (st == 0xE0 && len >= 3) {          /* pitch bend: +-2 semitones, Felucca's q8 semitones */
        int32_t b = (int32_t)(msg[1] | msg[2] << 7) - 8192;
        midi_bend_target[OPAL_PART] = b * (2 * 256) / (b < 0 ? 8192 : 8191);
    } else if (st == 0xB0 && len >= 3 && (msg[1] == 120 || msg[1] == 123))
        trk_all_off(t);
}

/* ----------------------------------------------------------------- render */

static void opal_render(void *inst, int16_t *out, int frames)
{
    opal_t *o = inst;
    int32_t blk[2 * CTL];
    int f, i;
    if (!o || !o->owner) {
        memset(out, 0, (size_t)frames * 2u * sizeof *out);
        return;
    }
    for (f = 0; f < frames; f += CTL) {
        int n = frames - f < CTL ? frames - f : CTL;
        mix_block(blk, (uint32_t)n);
        for (i = 0; i < 2 * n; i++) {
            int32_t s = blk[i];
            out[2 * f + i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
        }
    }
}

/* -------------------------------------------------------------- lifecycle */

static void *opal_create(const char *module_dir, const char *json_defaults)
{
    opal_t *o = calloc(1, sizeof *o);
    (void)module_dir;
    (void)json_defaults;
    if (!o)
        return NULL;
    o->engine = (int)opal_engine_at(0);
    if (!g_owner) {
        g_owner = o;
        o->owner = 1;
        opal_engine_init();
        opal_load(&trk[OPAL_PART], (uint32_t)o->engine, 0);
        trk[OPAL_PART].engine = trk[OPAL_PART].eng_req;
    }
    return o;
}

static void opal_destroy(void *inst)
{
    opal_t *o = inst;
    if (!o)
        return;
    if (g_owner == o) {
        trk_all_off(&trk[OPAL_PART]);
        g_owner = NULL;
    }
    free(o);
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
    g_host = host;
    return &g_api;
}
