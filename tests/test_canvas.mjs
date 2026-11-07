// SPDX-License-Identifier: GPL-3.0-only
// Host test for src/canvas.js: draws every engine's page and the FX page into a 1-bit frame the size of
// Schwung's page body, as the host would, for a few seconds at rest and while notes play.
//   node tests/test_canvas.mjs [build/viz.txt] [build/opal_screens.png]
// Checks: nothing throws (the host retires a page for the session after one throw), every picture draws
// something, stays inside its frame, and moves. Writes a contact sheet of the 128x64 screens to look at.
import fs from "node:fs";
import vm from "node:vm";
import zlib from "node:zlib";

const vizPath = process.argv[2] || "build/viz.txt";
const pngPath = process.argv[3] || "build/opal_screens.png";
let fails = 0;
const check = (c, msg) => { if (!c) { fails++; console.log("FAIL: " + msg); } };

/* a clipped 1-bit frame with the host's drawing calls (frame_ctx.mjs in Schwung) */
function frame(w, h) {
    const px = new Uint8Array(w * h);
    let overflow = 0;
    const self = {
        width: w, height: h, px,
        overflow: () => overflow,
        fillRect(x, y, rw, rh, c) {
            x = Math.round(x); y = Math.round(y); rw = Math.round(rw); rh = Math.round(rh);
            if (![x, y, rw, rh].every(Number.isFinite)) { overflow++; return; }
            let x0 = Math.max(0, x), y0 = Math.max(0, y), x1 = Math.min(w, x + rw), y1 = Math.min(h, y + rh);
            if (x0 !== x || y0 !== y || x1 !== x + rw || y1 !== y + rh) overflow++;
            for (let j = y0; j < y1; j++) for (let i = x0; i < x1; i++) px[j * w + i] = c ? 1 : 0;
        },
        textWidth: (s) => String(s).length * 6,
        print(x, y, s, c) { self.fillRect(x, y + 2, String(s).length * 6 - 1, 3, c); },
        setPixel(x, y, c) { self.fillRect(x, y, 1, 1, c); },
        line(x0, y0, x1, y1, c) {
            const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0), 1);
            for (let i = 0; i <= n; i++) self.fillRect(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, 1, 1, c);
        },
        fillCircle(cx, cy, r, c) {
            for (let dy = -r; dy <= r; dy++) {
                const dx = Math.floor(Math.sqrt(r * r - dy * dy));
                self.fillRect(cx - dx, cy + dy, 2 * dx + 1, 1, c);
            }
        },
        drawCircle(cx, cy, r, c) {
            for (let a = 0; a < 360; a += 4)
                self.fillRect(cx + Math.cos(a * Math.PI / 180) * r, cy + Math.sin(a * Math.PI / 180) * r, 1, 1, c);
        },
        drawArc(cx, cy, r) { self.drawCircle(cx, cy, r, 1); },
    };
    return self;
}

const sandbox = { Math, Number, String, Date, isFinite, console };
sandbox.globalThis = sandbox;
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(new URL("../src/canvas.js", import.meta.url), "utf8"), sandbox);
check(sandbox.opal_view && typeof sandbox.opal_view.drawPage === "function", "canvas.js exports opal_view.drawPage");
check(sandbox.opal_fx && typeof sandbox.opal_fx.drawPage === "function", "canvas.js exports opal_fx.drawPage");

const lines = fs.readFileSync(vizPath, "utf8").trim().split("\n");
check(lines.length === 11, `11 engines in ${vizPath} (${lines.length})`);

/* the screens for the sheet: the page body sits between the host's header (11 px) and footer (7 px) */
const SW = 128, SH = 64, BODY_Y = 11, BODY_H = 44;
const shots = [];

function shoot(title, f) {
    const s = new Uint8Array(SW * SH);
    for (let x = 0; x < SW; x += 2) s[9 * SW + x] = 1;              /* stand-in header rule */
    for (let y = 0; y < f.height; y++)
        for (let x = 0; x < f.width; x++) s[(BODY_Y + y) * SW + x] = f.px[y * f.width + x];
    shots.push({ title, s });
}

function play(drawer, values, label, preset) {
    let lastPx = null, moved = false, lit = 0;
    const t0 = 1_000_000;
    for (const h of [BODY_H, 36]) {
        for (let step = 0; step < 120; step++) {          /* 4 s at 30 frames/s; a note from 0.5 s to 2 s */
            const ms = t0 + step * 33;
            const act = step > 15 && step < 60 ? 80 : Math.max(0, 80 - (step - 60) * 4) * (step >= 60 ? 1 : 0);
            const viz = values.viz.replace(/\|\d+\|/, `|${act}|`);
            const f = frame(SW, h);
            try {
                drawer.drawPage(f, { values: { ...values, viz }, nowMs: ms, width: SW, height: h, preset,
                                     state: {}, key: "view", base: {}, keys: [], touched: -1 });
            } catch (e) {
                check(false, `${label}: drawPage threw at frame ${step}: ${e.stack}`);
                return;
            }
            const n = f.px.reduce((a, b) => a + b, 0);
            lit = Math.max(lit, n);
            if (lastPx && Buffer.compare(Buffer.from(lastPx), Buffer.from(f.px))) moved = true;
            lastPx = f.px;
            if (h === BODY_H && (step === 8 || step === 40))
                shoot(`${label} ${step === 8 ? "rest" : "playing"}`, f);
        }
    }
    check(lit > 20, `${label}: draws something (${lit} px)`);
    check(moved, `${label}: moves`);
}

for (const line of lines) {
    const [viz, ...e] = line.split(" ");
    const name = viz.split("|")[0];
    const values = { viz };
    e.forEach((v, i) => { values["e" + (i + 1)] = v; });
    play(sandbox.opal_view, values, name, { name: "PRESET", index: 0, count: 8, entered: false });
}
play(sandbox.opal_fx, { viz: lines[0].split(" ")[0], delay: "70", reverb: "60", dtime: "50", dfdbk: "80",
                         rsize: "90", dist: "10", chorus: "40" }, "FX");
/* a page that has not read viz yet must not throw */
{
    const f = frame(SW, BODY_H);
    try { sandbox.opal_view.drawPage(f, { values: {}, nowMs: 0 }); } catch (e) { check(false, "draws with no values: " + e); }
}

/* contact sheet: 3 screens a row, each 128x64 scaled x2 with a gap */
function png(w, h, gray) {
    const raw = Buffer.alloc((w + 1) * h);
    for (let y = 0; y < h; y++) { raw[y * (w + 1)] = 0; gray.copy(raw, y * (w + 1) + 1, y * w, y * w + w); }
    const crcTable = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
    const crc = (b) => { let c = 0xffffffff; for (const x of b) c = crcTable[(c ^ x) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
    const chunk = (type, data) => {
        const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
        const td = Buffer.concat([Buffer.from(type), data]);
        const c = Buffer.alloc(4); c.writeUInt32BE(crc(td));
        return Buffer.concat([len, td, c]);
    };
    const ihdr = Buffer.alloc(13);
    ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 0;
    return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr),
                          chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]);
}
{
    const S = 2, G = 8, cols = 4, rows = Math.ceil(shots.length / cols);
    const W = cols * (SW * S + G) + G, H = rows * (SH * S + G) + G;
    const img = Buffer.alloc(W * H, 40);
    shots.forEach((sh, i) => {
        const ox = G + (i % cols) * (SW * S + G), oy = G + Math.floor(i / cols) * (SH * S + G);
        for (let y = 0; y < SH * S; y++)
            for (let x = 0; x < SW * S; x++)
                img[(oy + y) * W + ox + x] = sh.s[Math.floor(y / S) * SW + Math.floor(x / S)] ? 235 : 0;
    });
    fs.writeFileSync(pngPath, png(W, H, img));
    console.log(`wrote ${pngPath} (${shots.length} screens: ${shots.map((s) => s.title).join(", ")})`);
}

console.log(fails ? `${fails} FAILED` : "canvas: all passed");
process.exit(fails ? 1 : 0);
