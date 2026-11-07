/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Opal Synth's drawn pages, for Schwung's canvas pages (as_page).
 *
 *   opal_view   the engine's page and its preset browser: one moving picture per engine
 *   opal_fx     the FX page: the delay's echoes and the reverb's cloud
 *
 * Each picture comes from what the engine does to the sound, moved by the eight knobs and by the output level.
 * They are drawn for this module; nothing is traced or copied from any other instrument.
 *
 * The host hands drawPage(ctx, o) a context clipped to the page body (0,0 = its top-left, o.width x o.height,
 * about 128x44) and o.values: the level's knobs (e1..e8, or the FX keys) plus "viz", read from the DSP:
 *   "ANALOG|37|0:3,0:127,..."   engine | output level 0..100 | the eight EDIT ranges min:max
 * Every pixel is a fillRect through the host's clipped context, so the pictures draw spans, not dots, where
 * they can.
 */
(function () {
    "use strict";

    /* ------------------------------------------------------------ helpers */

    var vizCache = { raw: "", name: "", act: 0, ranges: [] };

    function parseViz(raw) {
        if (typeof raw !== "string" || !raw)
            return null;
        if (raw === vizCache.raw)
            return vizCache;
        var parts = raw.split("|");
        if (parts.length < 3)
            return null;
        var ranges = parts[2].split(",").map(function (r) {
            var mm = r.split(":");
            return [Number(mm[0]) || 0, Number(mm[1]) || 0];
        });
        vizCache = { raw: raw, name: parts[0], act: Math.max(0, Math.min(1, (Number(parts[1]) || 0) / 100)),
                     ranges: ranges };
        return vizCache;
    }

    /* knob k (0..7) of the engine as 0..1; d when it has not been read */
    function knob(values, viz, k, d) {
        var v = values ? values["e" + (k + 1)] : null;
        var r = viz && viz.ranges[k];
        if (v === null || v === undefined || v === "" || !r || r[1] <= r[0])
            return d;
        var n = Number(v);
        if (!isFinite(n))
            return d;
        return Math.max(0, Math.min(1, (n - r[0]) / (r[1] - r[0])));
    }

    /* an FX key as 0..1 over 0..127 */
    function fxv(values, key, d) {
        var v = values ? values[key] : null;
        var n = Number(v);
        if (v === null || v === undefined || v === "" || !isFinite(n))
            return d;
        return Math.max(0, Math.min(1, n / 127));
    }

    function px(ctx, x, y) { ctx.fillRect(Math.round(x), Math.round(y), 1, 1, 1); }

    /* a vertical span joining two heights in one column: a continuous curve for one call per column */
    function span(ctx, x, y0, y1) {
        var a = Math.round(Math.min(y0, y1)), b = Math.round(Math.max(y0, y1));
        ctx.fillRect(Math.round(x), a, 1, b - a + 1, 1);
    }

    function disc(ctx, x, y, r) {
        if (r <= 0) { px(ctx, x, y); return; }
        ctx.fillCircle(Math.round(x), Math.round(y), Math.round(r), 1);
    }

    /* a small repeatable random generator, so a picture does not flicker frame to frame */
    function rng(seed) {
        var s = seed >>> 0 || 1;
        return function () {
            s ^= s << 13; s >>>= 0; s ^= s >>> 17; s ^= s << 5; s >>>= 0;
            return s / 4294967296;
        };
    }

    /* a curve f(x) -> -1..1 across the width, centred on cy with half-height amp */
    function curve(ctx, w, cy, amp, f, step) {
        var prev = null, x;
        for (x = 0; x < w; x++) {
            var y = cy - f(x) * amp;
            if (step && x % step)
                continue;
            if (step)
                px(ctx, x, y);
            else
                span(ctx, x, prev === null ? y : prev, y);
            prev = y;
        }
    }

    function dotted(ctx, w, y, gap) {
        for (var x = 0; x < w; x += gap)
            px(ctx, x, y);
    }

    /* ------------------------------------------------------- the engines */
    /* each: draw(ctx, w, h, k(i, def), act, t) with t in seconds */

    var ENG = {};

    /* ANALOG: stacked waves. WAVE sets the shape, DTN spreads the copies, CUT rounds the corners, RES rings them */
    ENG.ANALOG = function (ctx, w, h, k, act, t) {
        var shape = k(0, 0), det = k(1, 0.1), cut = k(4, 0.7), res = k(5, 0.2);
        var cy = h / 2 + 3, amp = h * (0.16 + 0.14 * act), period = 40;
        var raw = function (ph) {
            var f = ((ph % 1) + 1) % 1;
            var saw = 1 - 2 * f, sq = f < 0.5 ? 1 : -1, tri = 1 - 4 * Math.abs(f - 0.5);
            return shape < 0.34 ? saw : shape < 0.67 ? sq : tri;
        };
        var filtered = function (offset) {
            var fc = 0.06 + cut * 0.9, q = 1 - res * 0.9, low = 0, band = 0, out = [], i;
            for (i = -period * 2; i < w; i++) {
                var hi = raw(i / period + offset) - low - q * band;
                band += fc * hi;
                low += fc * band;
                if (i >= 0)
                    out.push(Math.max(-1.5, Math.min(1.5, low)));
            }
            return out;
        };
        for (var c = 2; c >= 0; c--) {
            var drift = c === 0 ? t * 0.15 : t * (0.15 + det * 0.4 * c * (c === 1 ? 1 : -0.8));
            var ys = filtered(drift);
            curve(ctx, w, cy, amp, function (x) { return ys[x]; }, c === 0 ? 0 : c + 1);
        }
        dotted(ctx, w, Math.round(cy), 4);
    };

    /* TRIO: three voices at their intervals, one line each, drifting by DTN; PW thins the pulse */
    ENG.TRIO = function (ctx, w, h, k, act, t) {
        var i2 = k(1, 0.5), i3 = k(2, 0.4), det = k(3, 0.2), cut = k(5, 0.5), pw = k(7, 0.5);
        var rows = [h * 0.34, h * 0.6, h * 0.86], amp = h * (0.06 + 0.04 * act);
        var ratios = [1, Math.pow(2, (i2 - 0.5) * 2), Math.pow(2, (i3 - 0.5) * 2)];
        for (var v = 0; v < 3; v++) {
            var per = 26 / ratios[v], off = t * (0.3 + det * v * 0.5);
            curve(ctx, w, rows[v], amp, function (x) {
                var f = ((x / per + off) % 1 + 1) % 1;
                var sq = f < 0.15 + pw * 0.7 ? 1 : -1;
                var saw = 1 - 2 * f;
                return cut * sq + (1 - cut) * saw * 0.8;
            }, 0);
        }
    };

    /* FM6: orbits. ALG sets the moons, FB ripples the planet, MLVL widens the orbits, MRAT speeds them;
     * on the right, the sidebands FM puts in the spectrum (Bessel functions of the index) */
    function bessel(m, x) {
        var s = 0, f = 1, k, j;
        for (k = 0; k < 14; k++) {
            if (k > 0)
                f *= k;
            var fm = 1;
            for (j = 1; j <= k + m; j++)
                fm *= j;
            s += (k % 2 ? -1 : 1) * Math.pow(x / 2, 2 * k + m) / (f * fm);
        }
        return s;
    }
    ENG.FM6 = function (ctx, w, h, k, act, t) {
        var alg = k(0, 0), fb = k(1, 0), lvl = k(2, 0.3), rat = k(3, 0.2);
        var moons = 1 + Math.round(alg * 4), cy = h / 2, cx = Math.round(w * 0.28);
        var ratio = 0.5 + rat * 6, i, a;
        var rad = 4 + act * 2;
        for (i = 0; i < 48; i++) {
            a = i / 48 * Math.PI * 2;
            var r = rad + fb * 2 * Math.sin(5 * a + t * (2 + fb * 9)) + fb * Math.sin(11 * a - t * 5);
            px(ctx, cx + Math.cos(a) * r, cy + Math.sin(a) * r);
        }
        disc(ctx, cx, cy, 1);
        for (var m = 0; m < moons; m++) {
            var orbit = Math.min(cy - 1, 8 + lvl * (cy - 9) * 0.8 + m * 2.5) / 0.6;
            orbit = Math.min(orbit, cx - 1);
            var tilt = Math.min(0.6, (cy - 1) / orbit), ring = Math.ceil(orbit * 4);
            for (i = 0; i < ring; i += 3) {
                a = i / ring * Math.PI * 2;
                px(ctx, cx + Math.cos(a) * orbit, cy + Math.sin(a) * orbit * tilt);
            }
            a = t * 0.6 * ratio * (1 + m * 0.35) + m * 2.1;
            disc(ctx, cx + Math.cos(a) * orbit, cy + Math.sin(a) * orbit * tilt, m === 0 ? 2 : 1);
        }
        var I = lvl * 5, base = Math.round(w * 0.78), gap = Math.max(2, Math.min(5, 1 + Math.round(ratio)));
        var floor = h - 2;
        for (var x = Math.round(w * 0.58); x < w; x += 2)
            px(ctx, x, floor + 1);
        for (m = -6; m <= 6; m++) {
            var bx = base + m * gap;
            if (bx < w * 0.58 || bx > w - 1)
                continue;
            var amp = Math.abs(bessel(Math.abs(m), I)) * (0.9 + 0.1 * Math.sin(t * 3 + m)) * (0.5 + 0.5 * act);
            var bh = Math.round(amp * (h - 4));
            if (bh > 0)
                ctx.fillRect(bx, floor - bh + 1, 1, bh, 1);
        }
    };

    /* PHASE: a cosine whose phase is bent. DCW bends it toward a saw or a pulse, ENV breathes the bend
     * with the level, SUB adds the octave below as dots */
    ENG.PHASE = function (ctx, w, h, k, act, t) {
        var wave = k(0, 0), dcw = k(2, 0.3), env = k(3, 0.7), sub = k(6, 0);
        var d = Math.min(0.97, dcw * (1 - env * 0.6 + env * act * 0.6) + 0.05 * Math.sin(t * 1.3));
        var cy = h / 2 + 3, amp = h * 0.32, per = 42, off = t * 0.2;
        var bend = function (f) {
            var p = 0.5 - d * 0.48;                         /* the knee: where half the cycle has passed */
            if (wave < 0.5)
                return f < p ? f / p * 0.5 : 0.5 + (f - p) / (1 - p) * 0.5;
            return f < p ? f / p * 0.5 : f < 1 - p ? 0.5 : 0.5 + (f - 1 + p) / p * 0.5;
        };
        curve(ctx, w, cy, amp, function (x) {
            var f = ((x / per + off) % 1 + 1) % 1;
            return Math.cos(bend(f) * Math.PI * 2);
        }, 0);
        if (sub > 0.05)
            curve(ctx, w, cy, amp * sub, function (x) {
                return Math.cos((x / (per * 2) + off / 2) * Math.PI * 2);
            }, 3);
    };

    /* LOFI: fat pixel steps. DUTY sets the pulse, CRSH coarsens the steps, VIB wobbles the pitch, ARP hops it */
    ENG.LOFI = function (ctx, w, h, k, act, t) {
        var duty = k(2, 0.25), crsh = k(3, 0), vib = k(5, 0.15), arp = k(6, 0);
        var cell = 2 + Math.round(crsh * 4);
        var hop = arp > 0.05 ? [1, 1.26, 1.5, 2][Math.floor(t * 8) % 4] : 1;
        var per = 32 / hop * (1 + 0.06 * vib * Math.sin(t * 9)), off = t * 0.25;
        var cy = Math.round(h / 2) + 3, amp = Math.max(2, Math.round(h * (0.14 + 0.14 * act) / cell)) * cell;
        var gap = cell > 2 ? 1 : 0;
        for (var x = 0; x < w; x += cell) {
            var f = ((x / per + off) % 1 + 1) % 1;
            if (f < 0.1 + duty * 0.8)
                ctx.fillRect(x, cy - amp, cell - gap, amp, 1);
            else
                ctx.fillRect(x, cy + 1, cell - gap, amp - cell, 1);
        }
        dotted(ctx, w, cy, 4);
    };

    /* VOICE: a mouth. VOWL sets its shape (A wide open, I wide flat, U small round), TALK moves it, BUZZ adds
     * the throat's pulse ticks, BRTH the breath dots */
    ENG.VOICE = function (ctx, w, h, k, act, t) {
        var vowl = k(0, 0), talk = k(2, 0), buzz = k(4, 0.3), brth = k(5, 0.2);
        var v = (vowl + talk * 0.5 * (1 + Math.sin(t * 3))) % 1;
        var cx = Math.round(w * 0.34), cy = h / 2;
        var open = 0.25 + 0.75 * act;
        var rx = (v < 0.5 ? 18 + (0.5 - v) * 14 : 18 - (v - 0.5) * 22);
        var ry = (cy - 3) * (v < 0.33 ? 0.9 : v < 0.66 ? 0.35 : 0.75) * open + 1;
        for (var i = 0; i < 64; i++) {
            var a = i / 64 * Math.PI * 2;
            px(ctx, cx + Math.cos(a) * rx, cy + Math.sin(a) * ry);
        }
        ctx.fillRect(cx - Math.round(rx * 0.6), Math.round(cy), Math.round(rx * 1.2), 1, 1);
        /* the buzz: a pulse train, denser with BUZZ */
        var gap = Math.max(3, Math.round(12 - buzz * 9));
        var sh = Math.round(t * 20) % gap;
        for (var x = Math.round(w * 0.66); x < w; x++) {
            var tick = (x + sh) % gap === 0;
            ctx.fillRect(x, Math.round(cy) - (tick ? Math.round(3 + 8 * act) : 0), 1, tick ? Math.round(3 + 8 * act) : 1, 1);
        }
        var r = rng(Math.floor(t * 12) + 7);
        for (i = 0; i < Math.round(brth * 24 * (0.3 + act)); i++)
            px(ctx, w * 0.62 + r() * w * 0.38, r() * h);
    };

    /* WHEEL: a tone wheel. REG sets the teeth, ROTR spins the cabinet (fast, slow, stop), DRV fattens the rim,
     * PERC flashes on each attack */
    ENG.WHEEL = function (ctx, w, h, k, act, t) {
        var reg = k(0, 0.5), perc = k(4, 0), drv = k(6, 0.2), rotr = k(7, 0.5);
        var cx = Math.round(w / 2), cy = h / 2, R = Math.min(cy - 2, 18);
        var teeth = 6 + Math.round(reg * 18), spin = t * (0.4 + rotr * 4);
        for (var i = 0; i < teeth * 6; i++) {
            var a = i / (teeth * 6) * Math.PI * 2 + spin;
            var bump = Math.cos(a * teeth - spin * teeth) > 0.3 ? 2 : 0;
            px(ctx, cx + Math.cos(a) * (R + bump), cy + Math.sin(a) * (R + bump) * 0.92);
        }
        disc(ctx, cx, cy, 2 + Math.round(drv * 3));
        /* the speaker horns either side, swinging with the rotor */
        var sw = Math.sin(spin * 2);
        for (var s = -1; s <= 1; s += 2) {
            var hx = cx + s * (R + 12 + Math.round(sw * 4 * s));
            var hh = Math.round(4 + 8 * act * (0.6 + 0.4 * sw * s));
            ctx.fillRect(hx - 1, Math.round(cy - hh / 2), 3, hh, 1);
        }
        if (perc > 0.05 && act > 0.6)
            ctx.drawCircle(cx, Math.round(cy), Math.round(R + 5), 1);
    };

    /* GRAIN: a cloud. POS moves the read line over the source, SIZE grows the grains, DENS spawns more,
     * MOTION scatters them */
    var grains = [], grainLast = 0, grainAcc = 0;
    var profile = (function () {
        var r = rng(11), a = [], v = 0.3, x;
        for (x = 0; x < 160; x++) {
            v = Math.max(0.1, Math.min(1, v + (r() - 0.5) * 0.35));
            a.push(v * (0.4 + 0.6 * Math.abs(Math.sin(x / 13))));
        }
        return a;
    })();
    var grainRand = rng(1234);
    ENG.GRAIN = function (ctx, w, h, k, act, t) {
        var motion = k(2, 0.5), size = k(5, 0.7), dens = k(6, 0.7), pos = k(7, 0.4);
        var base = h - 1, x, y;
        for (x = 0; x < w; x++) {                  /* the source: its outline over a dotted floor */
            px(ctx, x, base - Math.round(profile[x % profile.length] * 6));
            if (!(x & 1))
                px(ctx, x, base);
        }
        var rx = Math.round(3 + pos * (w - 6));
        for (y = 2; y < h - 2; y += 2)
            px(ctx, rx, y);
        var dt = Math.min(0.05, Math.max(0, t - (grainLast || t)));
        grainLast = t;
        grainAcc += dt * (2 + dens * 50) * (0.25 + act);
        while (grainAcc >= 1 && grains.length < 60) {
            grainAcc -= 1;
            grains.push({ x: rx + (grainRand() - 0.5) * motion * 60, y: h - 8 + (grainRand() - 0.5) * 4,
                          vx: (grainRand() - 0.5) * motion * 12, vy: -(5 + grainRand() * 9),
                          age: 0, life: 0.35 + size * 1.4, r: Math.round(size * 2.2) });
        }
        if (grainAcc > 1)
            grainAcc = 1;
        grains = grains.filter(function (g) { return (g.age += dt) < g.life; });
        grains.forEach(function (g) {
            g.x += g.vx * dt;
            g.y += g.vy * dt;
            var r = Math.max(0, Math.round(g.r * Math.sin(Math.PI * g.age / g.life)));
            if (g.y > 0 && g.y < h - 1)
                r > 1 ? ctx.drawCircle(Math.round(g.x), Math.round(g.y), r, 1) : disc(ctx, g.x, g.y, r);
        });
    };

    /* PHYS: a string between two pegs. POS is where it is struck, BRIT adds the upper modes, DAMP how long it
     * rings; the level sets how far it swings */
    ENG.PHYS = function (ctx, w, h, k, act, t) {
        var brit = k(2, 0.8), damp = k(3, 0.8), pos = k(4, 0.1);
        var x0 = 6, x1 = w - 7, L = x1 - x0, cy = h / 2;
        var amp = (cy - 4) * (0.12 + 0.88 * act);
        ctx.fillRect(x0 - 2, Math.round(cy) - 4, 2, 9, 1);
        ctx.fillRect(x1 + 1, Math.round(cy) - 4, 2, 9, 1);
        var modes = 1 + Math.round(brit * 4);
        curve(ctx, w, cy, 1, function (x) {
            if (x < x0 || x > x1)
                return 0;
            var u = (x - x0) / L, s = 0, norm = 0;
            for (var n = 1; n <= modes; n++) {
                var g = Math.sin(n * Math.PI * (0.05 + pos * 0.9)) / n * Math.pow(0.75 + damp * 0.2, n);
                s += g * Math.sin(n * Math.PI * u) * Math.cos(n * t * 9);
                norm += Math.abs(g);
            }
            return norm > 0 ? s / norm * amp : 0;
        }, 0);
        var sx = Math.round(x0 + (0.05 + pos * 0.9) * L);
        ctx.fillRect(sx, 0, 1, 3, 1);
        ctx.fillRect(sx, h - 3, 1, 3, 1);
    };

    /* NOISE: a field of dots. DENS thins it into crackle, FREQ and RES pull it into a band, DRFT moves the band */
    ENG.NOISE = function (ctx, w, h, k, act, t) {
        var colr = k(1, 0.5), freq = k(2, 0.55), res = k(3, 0.8), dens = k(5, 0), drft = k(6, 0.5);
        var r = rng(Math.floor(t * 20) * 977 + 3);
        var centre = h * (1 - freq) + Math.sin(t * (0.3 + drft * 2)) * drft * h * 0.2;
        var width = h * (1.1 - res * 0.85);
        var n = Math.round((60 + 220 * act) * (1 - dens * 0.85));
        for (var i = 0; i < n; i++) {
            var x = r() * w;
            var g = (r() + r() + r() - 1.5) / 1.5;          /* roughly a bell, wider with COLR */
            var y = centre + g * width * (0.5 + colr);
            if (y >= 0 && y < h)
                px(ctx, x, y);
        }
    };

    /* DRUM: a drum head seen from above. Each hit (the level jumping) sends a ring out; DECY how far it goes,
     * TUNE how tight the rings are, SNAP the rattle round the rim */
    var drumRings = [], drumPrev = 0;
    ENG.DRUM = function (ctx, w, h, k, act, t) {
        var tune = k(1, 0.5), decy = k(3, 0.5), snap = k(4, 0.5);
        var cx = Math.round(w / 2), cy = Math.round(h / 2), R = Math.min(cy - 1, 20);
        if (act > drumPrev + 0.25 || (act > 0.5 && drumPrev < 0.2))
            drumRings.push({ t0: t, a: act });
        drumPrev = act;
        ctx.drawCircle(cx, cy, R, 1);
        ctx.drawCircle(cx, cy, 1, 1);
        var life = 0.25 + decy * 1.2, speed = 25 + tune * 50;
        drumRings = drumRings.filter(function (g) { return t - g.t0 < life; });
        drumRings.forEach(function (g) {
            var age = t - g.t0, r = Math.round(age * speed);
            for (var e = 0; e < 3; e++) {
                var rr = r - e * Math.round(3 + (1 - tune) * 4);
                if (rr > 1 && rr < R)
                    ctx.drawCircle(cx, cy, rr, 1);
            }
        });
        if (snap > 0.1 && act > 0.2) {
            var rr2 = rng(Math.floor(t * 30));
            for (var i = 0; i < Math.round(snap * 30 * act); i++) {
                var a = rr2() * Math.PI * 2;
                px(ctx, cx + Math.cos(a) * (R + 2 + rr2() * 3), cy + Math.sin(a) * (R + 2 + rr2() * 3));
            }
        }
    };

    function nowSec(o) {
        return (o && typeof o.nowMs === "number" ? o.nowMs : Date.now()) / 1000;
    }

    /* while browsing presets the name sits at the top left, over the picture */
    function presetLabel(ctx, o, w) {
        var p = o && o.preset;
        if (!p || !p.name)
            return;
        var s = String(p.name);
        var tw = ctx.textWidth ? ctx.textWidth(s) : s.length * 6;
        ctx.fillRect(0, 0, Math.min(w, tw + 3), 9, 0);
        ctx.print(1, 1, s, 1);
    }

    globalThis.opal_view = {
        drawPage: function (ctx, o) {
            var w = (o && o.width) || ctx.width || 128, h = (o && o.height) || ctx.height || 40;
            var values = (o && o.values) || {};
            var viz = parseViz(values.viz);
            var name = viz ? viz.name : "";
            var draw = ENG[name];
            if (!draw) {
                ctx.print(2, Math.max(0, Math.round(h / 2) - 4), name || "OPAL", 1);
                return;
            }
            draw(ctx, w, h, function (i, d) { return knob(values, viz, i, d); }, viz.act, nowSec(o));
            presetLabel(ctx, o, w);
        },
    };

    /* FX: each note leaves a row of echoes that drift off and fade. D.Time spaces them, D.Fdbk sets how many
     * survive, Delay how bright they are; the reverb is the haze of dots behind, as wide as R.Size and as dense
     * as Reverb; Drive squares the hit and Chorus wobbles it */
    var fxHits = [], fxPrev = 0;
    globalThis.opal_fx = {
        drawPage: function (ctx, o) {
            var w = (o && o.width) || ctx.width || 128, h = (o && o.height) || ctx.height || 40;
            var v = (o && o.values) || {};
            var viz = parseViz(v.viz), act = viz ? viz.act : 0, t = nowSec(o);
            var dly = fxv(v, "delay", 0.2), rev = fxv(v, "reverb", 0.2), dt = fxv(v, "dtime", 0.4);
            var fb = fxv(v, "dfdbk", 0.5), rs = fxv(v, "rsize", 0.5), dist = fxv(v, "dist", 0), cho = fxv(v, "chorus", 0);
            var cy = Math.round(h / 2) - 2, x, i;
            if (act > fxPrev + 0.25 || (act > 0.5 && fxPrev < 0.2) || !fxHits.length)
                fxHits.push(t);
            fxPrev = act;
            fxHits = fxHits.filter(function (t0) { return t - t0 < 4; }).slice(-3);
            var hit = fxHits[fxHits.length - 1], age = t - hit;
            /* reverb haze */
            var r = rng(Math.floor(t * 8) * 31 + 5), n = Math.round(rev * 140 * (0.3 + 0.7 * Math.max(act, 0.3)));
            for (i = 0; i < n; i++) {
                var gx = 6 + r() * (w * (0.25 + rs * 0.75)), gy = cy + (r() - 0.5) * h * (0.3 + rs * 0.6);
                if (gy >= 0 && gy < h - 4)
                    px(ctx, gx, gy);
            }
            /* the hit */
            var s0 = 10, wob = Math.round(cho * 2 * Math.sin(t * 6));
            if (dist > 0.3)
                ctx.fillRect(2, cy - s0 / 2 + wob, s0, s0, 1);
            else
                ctx.fillCircle(2 + s0 / 2, cy + wob, s0 / 2, 1);
            /* echoes */
            var spacing = 6 + dt * 18, level = 1;
            for (i = 1; i < 18; i++) {
                level *= 0.2 + fb * 0.75;
                if (level < 0.05)
                    break;
                x = 2 + s0 / 2 + i * spacing;
                if (x > w - 3)
                    break;
                var appear = i * (0.05 + dt * 0.25);
                if (age < appear)
                    break;
                var sz = Math.max(1, Math.round(s0 / 2 * (0.4 + 0.6 * level) * (0.4 + 0.6 * dly)));
                if (level * (0.3 + dly) > 0.35)
                    ctx.fillCircle(Math.round(x), cy, sz, 1);
                else
                    ctx.drawCircle(Math.round(x), cy, sz, 1);
                ctx.fillRect(Math.round(x), h - 4, 1, 2, 1);
            }
            dotted(ctx, w, h - 2, 3);
        },
    };
})();
