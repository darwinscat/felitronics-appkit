// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// The ONE source of the family's glyph geometry. Writes, from the same numbers:
//   assets/glyphs/<key>.svg                       what a non-JUCE consumer (the website) vendors
//   include/felitronics/appkit/DeviceGlyphPaths.h the same path data as C++ string literals, so a
//                                                 header-only JUCE consumer needs no BinaryData step
// Run: node tools/gen-glyphs.mjs (from any directory). The output is deterministic: a re-run on an
// unchanged generator leaves no diff. GlyphAssetsTests fails if the two outputs disagree.
//
// The frame: viewBox 0 0 100 100, centre (50, 50), R = 50, envelope radius ER = 0.82 R. Leads reach
// past the box (to 1.16 R for the enveloped glyphs, 1.06 R for the others), as the procedural JUCE
// glyphs always did; a drawer that clips to the cell must pad it by 8 %.
//
// A glyph is an ordered list of PATHS. The drawer strokes each path separately, in order, in one
// colour, with round caps and joins, at max(1 px, 0.11 R) where R is half the drawn cell's shorter
// side. Nothing is filled, and nothing about colour or stroke width lives in the files.
//
// Why several paths and not one: stroking a path fills its outline with the non-zero rule, and the
// outline of a closed ring winds against the outline of an open lead crossing it — in one path the
// crossing cancels to a gap. So two subpaths share a path only if both are open, or if their strokes
// cannot touch.
//
// The six original glyphs (tube, bjt, fet, ic, dsp, diode) are line-for-line ports of the procedural
// code DeviceGlyph.h used to carry; tests/legacy/ProceduralDeviceGlyph.h keeps that code and
// DeviceGlyphParityTests holds the two together. Three things could not port as they were, because
// they depended on the stroke width: dsp's pin-1 dot was a FILLED disc of 1.8 strokes (now eight
// stroked spokes, the same disc wherever the stroke is 0.11 R), dsp's corner radius was 1.3
// strokes (now fixed at that value for the same stroke), and ic's bubble was stroked at 0.85 of the
// width (now the full width, like everything else).
//
// transformer = draft A of the saturation-glyph round (3 turns a side, leads), tape = draft B (one
// reel, tape leaving as a lead), shape-* = the transfer-shape marks, "framed curve" variant. The
// drafts came from a contact sheet built with this same code.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const SVG_DIR = path.join(ROOT, 'assets', 'glyphs');
const HEADER = path.join(ROOT, 'include', 'felitronics', 'appkit', 'DeviceGlyphPaths.h');

const C = 50, R = 50, ER = R * 0.82;
const SW = 0.11 * R;          // the stroke a cell of 18 px and up draws with, in frame units (5.5)
const L1 = ER + R * 0.34;     // lead reach of the enveloped family (tube / bjt / fet / tape)
const L2 = ER + R * 0.24;     // lead reach of the envelope-less family (ic / diode / transformer)
const DOT = 1.8 * SW / 2 - SW / 2;   // half-length of a dot's spokes: with round caps of SW they reach 0.9 SW

// Three decimals in a 100-unit frame: 0.0005 units, far below a pixel at any size a glyph is drawn.
const n = (v) => { const s = (+v.toFixed(3)).toString(); return s === '-0' ? '0' : s; };

// Curves are cubic Béziers built exactly as JUCE builds its own (Path::addEllipse,
// Path::addRoundedRectangle: control points at 0.55 of the radius), so JUCE flattens them at draw
// time just as it flattened the procedural glyphs. An SVG arc (A) would not do: JUCE's parser turns
// an arc into a fixed 0.05-radian polyline when it reads it, and at 16 px the envelope then came out
// up to 41/255 away from the procedural one.
const K = 0.55;

class P {
  constructor() { this.d = []; }
  M(x, y) { this.d.push(`M${n(x)} ${n(y)}`); return this; }
  L(x, y) { this.d.push(`L${n(x)} ${n(y)}`); return this; }
  C(x1, y1, x2, y2, x, y) { this.d.push(`C${n(x1)} ${n(y1)} ${n(x2)} ${n(y2)} ${n(x)} ${n(y)}`); return this; }
  Z() { this.d.push('Z'); return this; }
  circle(cx, cy, r) {   // as Path::addEllipse: from the top, clockwise
    const k = r * K;
    return this.M(cx, cy - r)
      .C(cx + k, cy - r, cx + r, cy - k, cx + r, cy)
      .C(cx + r, cy + k, cx + k, cy + r, cx, cy + r)
      .C(cx - k, cy + r, cx - r, cy + k, cx - r, cy)
      .C(cx - r, cy - k, cx - k, cy - r, cx, cy - r).Z();
  }
  roundRect(x, y, w, h, cs) {   // as Path::addRoundedRectangle, all four corners
    const c45 = cs * 0.45, x2 = x + w, y2 = y + h;
    return this.M(x, y + cs).C(x, y + c45, x + c45, y, x + cs, y)
      .L(x2 - cs, y).C(x2 - c45, y, x2, y + c45, x2, y + cs)
      .L(x2, y2 - cs).C(x2, y2 - c45, x2 - c45, y2, x2 - cs, y2)
      .L(x + cs, y2).C(x + c45, y2, x, y2 - c45, x, y2 - cs).Z();
  }
  // a half circle down from the current point (xc, yc - r) to (xc, yc + r), bulging right (side = 1) or left (-1)
  halfDown(xc, yc, r, side) {
    const k = r * K, b = side * r, bk = side * k;
    return this.C(xc + bk, yc - r, xc + b, yc - k, xc + b, yc).C(xc + b, yc + k, xc + bk, yc + r, xc, yc + r);
  }
  // A disc of diameter 1.8 SW — the old filled pin-1 dot — from strokes alone: eight segments through
  // the centre, their round caps tracing the rim to within 1.5 % of its radius. A circle stroked wider
  // than itself is the obvious spelling and the wrong one: its inner offset turns inside out, and JUCE
  // fills the centre as a hole (measured: alpha 0 at the middle of a 200 px dot).
  dot(cx, cy) {
    for (let k = 0; k < 8; ++k) {
      const a = (k * Math.PI) / 8, dx = DOT * Math.cos(a), dy = DOT * Math.sin(a);
      this.M(cx - dx, cy - dy).L(cx + dx, cy + dy);
    }
    return this;
  }
  str() { return this.d.join(' '); }
}
const ring = (cx, cy, r) => new P().circle(cx, cy, r);

// ---------------------------------------------------------------- the procedural originals, ported
function tube() {
  const w = ER * 0.62;
  const p = new P()
    .M(C - w, C - ER * 0.42).L(C + w, C - ER * 0.42)                                     // plate
    .M(C, C - ER * 0.42).L(C, C - ER - R * 0.34)                                         // plate lead up
    .M(C - w * 0.7, C + ER * 0.30).L(C, C + ER * 0.52).L(C + w * 0.7, C + ER * 0.30)     // cathode V
    .M(C, C + ER * 0.52).L(C, C + ER + R * 0.34)                                         // cathode lead down
    .M(C - ER * 0.9, C).L(C - ER - R * 0.34, C);                                         // grid lead left
  const grid = new P(), nd = 4, seg = (2 * w) / (nd * 2 - 1);                           // dashed grid
  for (let k = 0; k < nd; ++k) { const x0 = C - w + k * 2 * seg; grid.M(x0, C).L(Math.min(x0 + seg, C + w), C); }
  return [ring(C, C, ER), p, grid];
}
function bjt() {
  const bx = C - ER * 0.18, bt = C - ER * 0.42, bb = C + ER * 0.42, dx = C + ER * 0.5;
  const c0 = [bx, bt + ER * 0.16], c1 = [dx, C - ER * 0.62];
  const e0 = [bx, bb - ER * 0.16], e1 = [dx, C + ER * 0.62];
  const p = new P()
    .M(bx, bt).L(bx, bb)                                    // base bar
    .M(C - L1, C).L(bx, C)                                  // base lead
    .M(...c0).L(...c1).L(dx, C - L1)                        // collector
    .M(...e0).L(...e1).L(dx, C + L1);                       // emitter
  const tip = [e0[0] + (e1[0] - e0[0]) * 0.45, e0[1] + (e1[1] - e0[1]) * 0.45];   // PNP arrow, into the base
  const dir = Math.atan2(e0[1] - e1[1], e0[0] - e1[0]), a = Math.PI * 0.26, len = ER * 0.30;
  const arr = new P()
    .M(...tip).L(tip[0] - len * Math.cos(dir - a), tip[1] - len * Math.sin(dir - a))
    .M(...tip).L(tip[0] - len * Math.cos(dir + a), tip[1] - len * Math.sin(dir + a));
  return [ring(C, C, ER), p, arr];
}
function fet() {
  const chx = C + ER * 0.10, ct = C - ER * 0.46, cb = C + ER * 0.46;
  const p = new P()
    .M(chx, ct).L(chx, cb)                                                                         // channel
    .M(C - ER - R * 0.34, C).L(chx - ER * 0.02, C)                                                 // gate lead
    .M(chx, ct + ER * 0.16).L(chx + ER * 0.72, ct + ER * 0.16).L(chx + ER * 0.72, C - ER - R * 0.10)   // drain
    .M(chx, cb - ER * 0.16).L(chx + ER * 0.72, cb - ER * 0.16).L(chx + ER * 0.72, C + ER + R * 0.10);  // source
  const tx = chx - ER * 0.02;
  const arr = new P().M(tx - ER * 0.30, C - ER * 0.20).L(tx, C).L(tx - ER * 0.30, C + ER * 0.20);   // gate arrow
  return [ring(C, C, ER), p, arr];
}
function ic() {
  const w = ER * 1.5, h = ER * 1.7, lx = C - w * 0.45, rx = C + w * 0.55, iy = h * 0.22, bub = ER * 0.13;
  // the flat side is broken around the inverting-input bubble, so the bubble reads hollow
  const tri = new P().M(lx, C - h / 2).L(rx, C).L(lx, C + h / 2).L(lx, C + iy + bub).M(lx, C + iy - bub).L(lx, C - h / 2);
  const leads = new P().M(C - L2, C - iy).L(lx, C - iy).M(C - L2, C + iy).L(lx - bub, C + iy).M(rx, C).L(C + L2, C);
  return [tri, leads, ring(lx, C + iy, bub)];
}
function dsp() {
  const bw = ER * 1.62, bh = ER * 1.35, legOut = ER * 1.15, bx = C - bw / 2, by = C - bh / 2;
  const legs = new P();
  for (let k = 0; k < 3; ++k) { const y = by + bh * (0.22 + 0.28 * k); legs.M(C - legOut, y).L(bx, y).M(bx + bw, y).L(C + legOut, y); }
  return [new P().roundRect(bx, by, bw, bh, 1.3 * SW), legs, new P().dot(bx + bw * 0.22, by + bh * 0.26)];
}
function diode() {
  const w = ER * 1.18, h = ER * 1.28, lx = C - w / 2, rx = C + w / 2;
  const tri = new P().M(lx, C - h / 2).L(rx, C).L(lx, C + h / 2).Z();
  const leads = new P().M(rx, C - h / 2).L(rx, C + h / 2).M(C - L2, C).L(lx, C).M(rx, C).L(C + L2, C);
  return [tri, leads];
}

// ---------------------------------------------------------------- transformer (draft A)
// The schematic "3||E": two windings whose turns face the core, two core lines between them. No
// envelope — like ic and diode, a transformer is not a discrete device — so the leads reach L2.
function transformer() {
  const turns = 3, rb = 9, coreHalf = 7, gap = 6, coreExt = 4;
  const H = turns * 2 * rb, y0 = C - H / 2, y1 = C + H / 2;
  const xl = C - coreHalf - gap - rb, xr = C + coreHalf + gap + rb;
  const p = new P().M(xl, y0);
  for (let i = 0; i < turns; ++i) p.halfDown(xl, y0 + (2 * i + 1) * rb, rb, 1);    // left winding, turns bulge right
  p.M(xr, y0);
  for (let i = 0; i < turns; ++i) p.halfDown(xr, y0 + (2 * i + 1) * rb, rb, -1);   // right winding, turns bulge left
  p.M(C - coreHalf, y0 - coreExt).L(C - coreHalf, y1 + coreExt).M(C + coreHalf, y0 - coreExt).L(C + coreHalf, y1 + coreExt);
  p.M(xl, y0).L(C - L2, y0).M(xl, y1).L(C - L2, y1).M(xr, y0).L(C + L2, y0).M(xr, y1).L(C + L2, y1);
  return [p];   // all open: one path
}

// ---------------------------------------------------------------- tape (draft B)
// One reel in the envelope's place (rim = ER), three windows, a hub, the tape leaving the bottom of
// the rim tangentially as a lead. The rim and the windows never touch, so they share a path; the
// tape starts on the rim, so it is its own.
function tape() {
  const rim = ring(C, C, ER);
  for (const deg of [-90, 30, 150]) rim.circle(C + 20 * Math.cos(deg * Math.PI / 180), C + 20 * Math.sin(deg * Math.PI / 180), 8.5);
  return [rim, new P().dot(C, C), new P().M(C, C + ER).L(C + L1, C + ER)];
}

// ---------------------------------------------------------------- transfer-shape marks (framed curve)
// The shapes of felitronics-core's WaveShaper, drive exaggerated so the four differ at 16 px.
const shapes = {
  tanh: (x) => Math.tanh(2.6 * x) / Math.tanh(2.6),
  atan: (x) => Math.atan(6 * x) / Math.atan(6),
  cubic: (x) => { const u = 1.6 * x; return Math.abs(u) < 1 ? 1.5 * u - 0.5 * u ** 3 : Math.sign(u); },
  asym: (x) => {
    const k = 2.2, b = 0.35, r = (v) => Math.tanh(k * (v + b)) - Math.tanh(k * b);
    return r(x) / Math.max(Math.abs(r(1)), Math.abs(r(-1)));
  },
};
const framedCurve = (fn) => () => {
  const f0 = 13, f1 = 87, x0 = 24, x1 = 76, amp = 24, steps = 48;
  const curve = new P();
  for (let i = 0; i <= steps; ++i) {
    const t = -1 + (2 * i) / steps, x = C + t * (x1 - x0) / 2, y = C - fn(t) * amp;
    if (i === 0) curve.M(x, y); else curve.L(x, y);
  }
  return [new P().roundRect(f0, f0, f1 - f0, f1 - f0, 1.6 * SW), curve];
};

// ---------------------------------------------------------------- catalogue (order = header order)
const glyphs = [
  ['tube', tube], ['bjt', bjt], ['fet', fet], ['ic', ic], ['dsp', dsp], ['diode', diode],
  ['transformer', transformer], ['tape', tape],
  ['shape-tanh', framedCurve(shapes.tanh)], ['shape-atan', framedCurve(shapes.atan)],
  ['shape-cubic', framedCurve(shapes.cubic)], ['shape-asym', framedCurve(shapes.asym)],
].map(([key, fn]) => ({ key, paths: fn().map((p) => p.str()) }));

const SPDX = 'SPDX-License-Identifier: AGPL-3.0-or-later';
const COPY = "Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.";

fs.mkdirSync(SVG_DIR, { recursive: true });
for (const g of glyphs) {
  const svg = `<!-- ${SPDX} -->\n<!-- ${COPY} -->\n<!-- Generated by tools/gen-glyphs.mjs: stroke each path on its own, round caps and joins, width max(1 px, 0.11 R). -->\n`
    + '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" overflow="visible" fill="none" stroke-linecap="round" stroke-linejoin="round">\n'
    + g.paths.map((d) => `<path d="${d}"/>\n`).join('')
    + '</svg>\n';
  fs.writeFileSync(path.join(SVG_DIR, `${g.key}.svg`), svg);
}

const ident = (key) => key.replace(/-/g, '_');
let h = `// ${SPDX}
// ${COPY}

// GENERATED by tools/gen-glyphs.mjs — do not edit; change the generator and re-run it. The same run
// writes assets/glyphs/<key>.svg, and GlyphAssetsTests fails when this file and those disagree.
//
// The family's glyph geometry as path data (SVG syntax: M, L, C, Z) in a 100 x 100 frame centred on
// (50, 50). Each glyph is an ordered list of paths; stroke each one separately, round caps and joins,
// width max(1 px, 0.11 R) with R half the cell's shorter side. JUCE-free on purpose: DeviceGlyph.h
// parses it with juce::Drawable::parseSVGPath, anything else can read the strings as they are.

#pragma once

#include <span>
#include <string_view>

namespace felitronics::appkit::glyphpaths
{

`;
for (const g of glyphs) {
  h += `inline constexpr std::string_view ${ident(g.key)}[] = {\n${g.paths.map((d) => `    "${d}",\n`).join('')}};\n\n`;
}
h += `struct Glyph
{
    std::string_view                  key;     // the SVG's file name under assets/glyphs/, without .svg
    std::span<const std::string_view> paths;   // stroke each one separately, in order
};

inline constexpr Glyph all[] = {
${glyphs.map((g) => `    { "${g.key}", ${ident(g.key)} },`).join('\n')}
};

// The paths of the glyph named \`key\`, or an empty span when the catalogue has no such glyph.
constexpr std::span<const std::string_view> find (std::string_view key) noexcept
{
    for (const auto& g : all)
        if (g.key == key)
            return g.paths;
    return {};
}

} // namespace felitronics::appkit::glyphpaths
`;
fs.writeFileSync(HEADER, h);
console.log(`ok: ${glyphs.length} glyphs -> ${path.relative(ROOT, SVG_DIR)}/, ${path.relative(ROOT, HEADER)}`);
