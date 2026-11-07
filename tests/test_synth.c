/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test for Opal Synth: drives the module through Schwung's plugin API v2, as the chain host does.
 *   build/test_synth [out.wav]    every engine's first preset plays a chord; one engine is written to out.wav */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host/plugin_api_v1.h"
#include "dsp/opal_core.h"

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

#include <math.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void note(plugin_api_v2_t *api, void *o, int on, int n)
{
    uint8_t m[3] = {(uint8_t)(on ? 0x90 : 0x80), (uint8_t)n, (uint8_t)(on ? 100 : 0)};
    api->on_midi(o, m, 3, MOVE_MIDI_SOURCE_INTERNAL);
}

/* render secs seconds with a chord held for the first half; returns the peak, adds the RMS into *rms */
static int render(plugin_api_v2_t *api, void *o, double secs, FILE *wav, double *rms)
{
    int16_t buf[2 * MOVE_FRAMES_PER_BLOCK];
    int blocks = (int)(secs * 44100 / MOVE_FRAMES_PER_BLOCK), b, i, peak = 0;
    double sum = 0;
    static const int CH[3] = {60, 64, 67};
    for (i = 0; i < 3; i++)
        note(api, o, 1, CH[i]);
    for (b = 0; b < blocks; b++) {
        if (b == blocks / 2)
            for (i = 0; i < 3; i++)
                note(api, o, 0, CH[i]);
        api->render_block(o, buf, MOVE_FRAMES_PER_BLOCK);
        for (i = 0; i < 2 * MOVE_FRAMES_PER_BLOCK; i++) {
            int a = abs(buf[i]);
            if (a > peak)
                peak = a;
            sum += (double)buf[i] * buf[i];
        }
        if (wav)
            fwrite(buf, sizeof buf, 1, wav);
    }
    *rms = sqrt(sum / (blocks * 2.0 * MOVE_FRAMES_PER_BLOCK));
    return peak;
}

static void wav_header(FILE *f, uint32_t frames)
{
    uint32_t v;
    uint16_t a = 1, ch = 2, ba = 4, bits = 16;
    uint32_t sr = 44100, br = 44100 * 4;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 4; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 4; fwrite(&v, 4, 1, f);
}

int main(int argc, char **argv)
{
    plugin_api_v2_t *api = move_plugin_init_v2(NULL);
    void *o = api->create_instance("/tmp", NULL), *o2;
    char buf[64], name[32];
    int n, e, count;
    double rms;

    CHECK(o, "create_instance");
    CHECK(api->get_error(o, buf, sizeof buf) == 0, "the first instance has no error");

    /* four tracks at once, each its own engine; a fifth instance: an error, silence */
    {
        void *t[OPAL_NCORES];
        int16_t s[2 * MOVE_FRAMES_PER_BLOCK];
        double r[OPAL_NCORES];
        int i, k, nz = 0;
        t[0] = o;
        for (k = 1; k < OPAL_NCORES; k++) {
            t[k] = api->create_instance("/tmp", NULL);
            CHECK(api->get_error(t[k], buf, sizeof buf) == 0, "instance %d has a core", k + 1);
        }
        o2 = api->create_instance("/tmp", NULL);
        CHECK(api->get_error(o2, buf, sizeof buf) > 0, "a fifth instance reports an error");
        note(api, o2, 1, 60);
        api->render_block(o2, s, MOVE_FRAMES_PER_BLOCK);
        for (i = 0; i < 2 * MOVE_FRAMES_PER_BLOCK; i++)
            nz |= s[i];
        CHECK(!nz, "a fifth instance is silent");
        api->destroy_instance(o2);
        /* tracks 1..4 on four engines; only track 2 plays: the others stay silent */
        for (k = 0; k < OPAL_NCORES; k++) {
            snprintf(buf, sizeof buf, "%d", k * 2);
            api->set_param(t[k], "engine", buf);
        }
        note(api, t[1], 1, 64);
        for (k = 0; k < OPAL_NCORES; k++) {
            double sum = 0;
            int b;
            for (b = 0; b < 40; b++) {
                api->render_block(t[k], s, MOVE_FRAMES_PER_BLOCK);
                for (i = 0; i < 2 * MOVE_FRAMES_PER_BLOCK; i++)
                    sum += (double)s[i] * s[i];
            }
            r[k] = sum;
        }
        note(api, t[1], 0, 64);
        printf("four tracks, a note on track 2: energy %.3g %.3g %.3g %.3g\n", r[0], r[1], r[2], r[3]);
        CHECK(r[1] > 1e6 && r[0] == 0 && r[2] == 0 && r[3] == 0, "the tracks are independent");
        api->get_param(t[2], "engine_name", name, sizeof name);
        api->get_param(t[1], "engine_name", buf, sizeof buf);
        CHECK(strcmp(name, buf), "tracks 2 and 3 have their own engines (%s, %s)", buf, name);
        for (k = 1; k < OPAL_NCORES; k++)
            api->destroy_instance(t[k]);
        o2 = api->create_instance("/tmp", NULL);
        CHECK(api->get_error(o2, buf, sizeof buf) == 0, "a freed core is taken again");
        api->destroy_instance(o2);
    }

    count = (int)opal_core0.engines();
    printf("%d engines\n", count);
    for (e = 0; e < count; e++) {
        snprintf(buf, sizeof buf, "%d", e);
        api->set_param(o, "engine", buf);
        api->get_param(o, "engine_name", name, sizeof name);
        api->get_param(o, "preset_name", buf, sizeof buf);
        n = render(api, o, 1.5, NULL, &rms);
        printf("  %-8s %-10s peak %5d  rms %7.1f\n", name, buf, n, rms);
        CHECK(n > 300, "%s: the chord sounds (peak %d)", name, n);
        CHECK(n < 32767, "%s: no hard clipping (peak %d)", name, n);
    }

    /* parameters: engine labels, a clamped EDIT value, presets wrap */
    api->set_param(o, "engine", "0");
    api->get_param(o, "engine_name", name, sizeof name);
    api->get_param(o, "e1_name", buf, sizeof buf);
    printf("engine 0: %s, EDIT 1 = %s\n", name, buf);
    CHECK(buf[0], "EDIT 1 has a label");
    api->set_param(o, "e5", "9999");
    api->get_param(o, "e5", buf, sizeof buf);
    CHECK(atoi(buf) <= 127, "EDIT 5 is clamped (%s)", buf);
    api->get_param(o, "preset_count", buf, sizeof buf);
    n = atoi(buf);
    if (n > 0) {
        snprintf(buf, sizeof buf, "%d", n + 1);
        api->set_param(o, "preset", buf);
        api->get_param(o, "preset", buf, sizeof buf);
        CHECK(atoi(buf) == 1, "presets wrap (%s)", buf);
    }

    /* the pages, the knob list and the state round trip */
    {
        static char js[16384], st[2048], st2[2048];
        int l = api->get_param(o, "ui_hierarchy", js, sizeof js);
        CHECK(l > 0 && js[0] == '{' && js[l - 1] == '}', "ui_hierarchy is a JSON object (%d bytes)", l);
        CHECK(strstr(js, "\"knobs\":[\"e1\""), "knobs 1-8 are the EDIT values");
        l = api->get_param(o, "chain_params", js, sizeof js);
        CHECK(l > 0 && js[0] == '[' && js[l - 1] == ']', "chain_params is a JSON array (%d bytes)", l);
        CHECK(strstr(js, "\"key\":\"reverb\"") && strstr(js, "\"key\":\"rtype\""), "chain_params has the FX");
        api->get_param(o, "ui_hierarchy", js, sizeof js);
        CHECK(strstr(js, "\"fx\":{\"name\":\"FX\"") && strstr(js, "\"fx2\":{"), "the FX and FX+ pages");
        api->set_param(o, "engine", "3");
        api->set_param(o, "e3", "77");
        api->set_param(o, "rel", "99");
        api->set_param(o, "reverb", "80");
        api->set_param(o, "rsize", "100");
        api->set_param(o, "rtype", "1");
        api->get_param(o, "state", st, sizeof st);
        api->set_param(o, "engine", "0");
        api->set_param(o, "synth:state", st);
        api->get_param(o, "state", st2, sizeof st2);
        printf("state: %s\n", st2);
        CHECK(!strcmp(st, st2), "state round-trips");
    }

    /* the FX: a dry sound against the same with delay and reverb sends; the sends ring on after the release */
    {
        double dry, wet;
        api->set_param(o, "engine", "0");
        api->set_param(o, "preset", "0");
        api->set_param(o, "delay", "0");
        api->set_param(o, "reverb", "0");
        api->set_param(o, "chorus", "0");
        render(api, o, 1.0, NULL, &rms);
        {   /* the tail: 1 s after the chord ends */
            int16_t s[2 * MOVE_FRAMES_PER_BLOCK];
            int b, i;
            double sum;
            for (sum = 0, b = 0; b < 344; b++) {
                api->render_block(o, s, MOVE_FRAMES_PER_BLOCK);
                if (b > 150)
                    for (i = 0; i < 2 * MOVE_FRAMES_PER_BLOCK; i++)
                        sum += (double)s[i] * s[i];
            }
            dry = sum;
            api->set_param(o, "delay", "100");
            api->set_param(o, "reverb", "110");
            render(api, o, 1.0, NULL, &rms);
            for (sum = 0, b = 0; b < 344; b++) {
                api->render_block(o, s, MOVE_FRAMES_PER_BLOCK);
                if (b > 150)
                    for (i = 0; i < 2 * MOVE_FRAMES_PER_BLOCK; i++)
                        sum += (double)s[i] * s[i];
            }
            wet = sum;
        }
        printf("FX tail energy: dry %.3g, delay + reverb %.3g\n", dry, wet);
        CHECK(wet > dry * 4 + 1, "the delay and reverb sends ring after the release");
        api->set_param(o, "delay", "0");
        api->set_param(o, "reverb", "0");
    }

    /* a WAV of the analog engine's ACID preset, to listen to */
    {
        const char *out = argc > 1 ? argv[1] : "build/opal_analog.wav";
        FILE *w = fopen(out, "wb");
        int k;
        api->set_param(o, "engine", "0");
        api->get_param(o, "preset_count", buf, sizeof buf);
        for (k = atoi(buf) - 1; k >= 0; k--) {
            snprintf(buf, sizeof buf, "%d", k);
            api->set_param(o, "preset", buf);
            api->get_param(o, "preset_name", name, sizeof name);
            if (!strcmp(name, "ACID"))
                break;
        }
        if (w) {
            wav_header(w, (uint32_t)(2.0 * 44100 / MOVE_FRAMES_PER_BLOCK) * MOVE_FRAMES_PER_BLOCK);
            render(api, o, 2.0, w, &rms);
            fclose(w);
            printf("wrote %s\n", out);
        }
    }

    api->destroy_instance(o);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
