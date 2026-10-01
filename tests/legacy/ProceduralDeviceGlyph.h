// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

// The PROCEDURAL device glyphs exactly as DeviceGlyph.h drew them before the geometry moved to
// assets/glyphs/*.svg (felitronics-appkit 0.10.0, copied verbatim — only the namespace differs).
// Test-only: DeviceGlyphParityTests renders this old code path next to the new SVG-driven one and
// holds the difference to a stated tolerance. Never include it from a product.

#include <felitronics/appkit/DeviceSpec.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace felitronics::appkit::legacy
{

// One symbol inside `r` (a square-ish cell), stroked in `c`. Kept schematic-simple so it reads at ~20 px.
inline void drawDeviceGlyph (juce::Graphics& g, juce::Rectangle<float> r, DeviceType type, juce::Colour c)
{
    if (type == DeviceType::none)
        return;

    const float R  = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const auto  ctr = r.getCentre();
    const float cx = ctr.x, cy = ctr.y;
    const float sw = juce::jmax (1.0f, R * 0.11f);   // stroke width
    g.setColour (c);
    const juce::PathStrokeType stroke (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const float er = R * 0.82f;   // envelope radius

    // Analogue op-amp — the schematic triangle: two inputs into the flat side, one output from the
    // tip, a bubble on the inverting input. Deliberately NOT the chip package: the package is what
    // an op-amp and a DSP have in COMMON, and the point of splitting them is to show what they don't.
    if (type == DeviceType::ic)
    {
        const float w = er * 1.50f, h = er * 1.70f;
        const float lx = cx - w * 0.45f, rx = cx + w * 0.55f;

        // The inverting input is marked by a bubble CENTRED ON the flat side. The flat side is drawn
        // as two segments with a gap the bubble's width, so the bubble reads hollow — interrupting
        // the line, not covering it. Filling it would need the background colour, which a glyph
        // drawn on an arbitrary surface does not know.
        const float iy  = h * 0.22f;
        const float bub = er * 0.13f;

        juce::Path tri;
        tri.startNewSubPath (lx, cy - h * 0.5f);
        tri.lineTo          (rx, cy);
        tri.lineTo          (lx, cy + h * 0.5f);
        tri.lineTo          (lx, cy + iy + bub);   // flat side, up to the bubble
        tri.startNewSubPath (lx, cy + iy - bub);   // and on from the other side of it
        tri.lineTo          (lx, cy - h * 0.5f);
        g.strokePath (tri, stroke);

        // Shorter reach than the enveloped families: with no circle around it, a lead of the same
        // length reads as too long next to the body.
        const float lead = er + R * 0.24f;
        juce::Path leads;
        leads.startNewSubPath (cx - lead, cy - iy); leads.lineTo (lx, cy - iy);
        leads.startNewSubPath (cx - lead, cy + iy); leads.lineTo (lx - bub, cy + iy);
        leads.startNewSubPath (rx, cy);             leads.lineTo (cx + lead, cy);
        g.strokePath (leads, stroke);
        g.drawEllipse (lx - bub, cy + iy - bub, 2.0f * bub, 2.0f * bub, sw * 0.85f);
        return;
    }

    // DSP — a chip package with legs + a pin-1 dot (no valve envelope).
    if (type == DeviceType::dsp)
    {
        const float bw = er * 1.62f, bh = er * 1.35f;
        const float legOut = er * 1.15f;   // where a leg ends, measured from the centre
        auto body = juce::Rectangle<float> (bw, bh).withCentre (ctr);
        g.drawRoundedRectangle (body, sw * 1.3f, sw);
        juce::Path legs;
        for (int k = 0; k < 3; ++k)
        {
            const float y = body.getY() + bh * (0.22f + 0.28f * (float) k);
            legs.startNewSubPath (cx - legOut, y);     legs.lineTo (body.getX(), y);
            legs.startNewSubPath (body.getRight(), y); legs.lineTo (cx + legOut, y);
        }
        g.strokePath (legs, stroke);
        const float dot = sw * 1.8f;   // pin-1 marker
        g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre ({ body.getX() + bw * 0.22f, body.getY() + bh * 0.26f }));
        return;
    }

    // Diode: anode triangle → cathode bar, with leads out each side (no valve envelope).
    if (type == DeviceType::diode)
    {
        const float w = er * 1.18f, h = er * 1.28f;
        const float lx = cx - w * 0.5f, rx = cx + w * 0.5f;
        juce::Path tri;
        tri.startNewSubPath (lx, cy - h * 0.5f);
        tri.lineTo          (rx, cy);
        tri.lineTo          (lx, cy + h * 0.5f);
        tri.closeSubPath();
        g.strokePath (tri, stroke);
        // Same shorter reach as the op-amp — neither sits in an envelope circle.
        const float lead = er + R * 0.24f;
        juce::Path leads;
        leads.startNewSubPath (rx, cy - h * 0.5f);   leads.lineTo (rx, cy + h * 0.5f);   // cathode bar
        leads.startNewSubPath (cx - lead, cy);       leads.lineTo (lx, cy);              // anode lead
        leads.startNewSubPath (rx, cy);              leads.lineTo (cx + lead, cy);       // cathode lead
        g.strokePath (leads, stroke);
        return;
    }

    // Envelope circle (tube / BJT / FET sit in one, schematic style).
    g.drawEllipse (juce::Rectangle<float> (2 * R * 0.82f, 2 * R * 0.82f).withCentre (ctr), sw);

    juce::Path p;
    if (type == DeviceType::tube)
    {
        // Triode: plate (top bar) + grid (dashed) + cathode (shallow V), with leads out top/left/bottom.
        const float w = er * 0.62f;
        // plate
        p.startNewSubPath (cx - w, cy - er * 0.42f);
        p.lineTo         (cx + w, cy - er * 0.42f);
        p.startNewSubPath (cx, cy - er * 0.42f);   // plate lead up
        p.lineTo         (cx, cy - er - R * 0.34f);
        // cathode (V) + lead down
        p.startNewSubPath (cx - w * 0.7f, cy + er * 0.30f);
        p.lineTo         (cx,             cy + er * 0.52f);
        p.lineTo         (cx + w * 0.7f, cy + er * 0.30f);
        p.startNewSubPath (cx, cy + er * 0.52f);
        p.lineTo         (cx, cy + er + R * 0.34f);
        // grid lead out the left
        p.startNewSubPath (cx - er * 0.9f, cy);
        p.lineTo         (cx - er - R * 0.34f, cy);
        g.strokePath (p, stroke);
        // grid: a short dashed bar (drawn as discrete segments — reliable at any size)
        juce::Path grid;
        const int   nd  = 4;
        const float seg = (2 * w) / (float) (nd * 2 - 1);
        for (int k = 0; k < nd; ++k)
        {
            const float x0 = cx - w + (float) k * 2.0f * seg;
            grid.startNewSubPath (x0, cy);
            grid.lineTo (juce::jmin (x0 + seg, cx + w), cy);
        }
        g.strokePath (grid, stroke);
    }
    else if (type == DeviceType::bjt)
    {
        // BJT: vertical base bar; base lead out the left; collector up-right then out the top,
        // emitter down-right then out the bottom. All THREE leads cross the envelope by the same
        // amount — previously only the base did, so the symbol sat lopsided in its circle.
        const float bx = cx - er * 0.18f;              // base bar x
        const float bt = cy - er * 0.42f, bb = cy + er * 0.42f;
        const float lead = er + R * 0.34f;             // how far a lead reaches from the centre
        const float dx = cx + er * 0.5f;               // where the diagonals turn vertical

        const juce::Point<float> c0 (bx, bt + er * 0.16f), c1 (dx, cy - er * 0.62f);
        const juce::Point<float> e0 (bx, bb - er * 0.16f), e1 (dx, cy + er * 0.62f);

        p.startNewSubPath (bx, bt); p.lineTo (bx, bb);                  // base bar
        p.startNewSubPath (cx - lead, cy); p.lineTo (bx, cy);           // base lead (left)
        p.startNewSubPath (c0.x, c0.y); p.lineTo (c1.x, c1.y); p.lineTo (dx, cy - lead);   // collector
        p.startNewSubPath (e0.x, e0.y); p.lineTo (e1.x, e1.y); p.lineTo (dx, cy + lead);   // emitter
        g.strokePath (p, stroke);

        // Emitter arrow, PNP → points INTO the base. It now sits ON the emitter line: the old tip
        // was computed independently of the line and floated beside it.
        const auto  tip = e0 + (e1 - e0) * 0.45f;
        const float dir = std::atan2 (e0.y - e1.y, e0.x - e1.x);   // along the emitter, toward the base
        const float a = juce::MathConstants<float>::pi * 0.26f, len = er * 0.30f;
        juce::Path arr;
        arr.startNewSubPath (tip);
        arr.lineTo (tip.x - len * std::cos (dir - a), tip.y - len * std::sin (dir - a));
        arr.startNewSubPath (tip);
        arr.lineTo (tip.x - len * std::cos (dir + a), tip.y - len * std::sin (dir + a));
        g.strokePath (arr, stroke);
    }
    else // fet
    {
        // JFET: vertical channel bar; gate lead from the left; drain (top-right) + source (bottom-right).
        const float chx = cx + er * 0.10f;
        const float ct = cy - er * 0.46f, cb = cy + er * 0.46f;
        p.startNewSubPath (chx, ct); p.lineTo (chx, cb);                                   // channel
        p.startNewSubPath (cx - er - R * 0.34f, cy); p.lineTo (chx - er * 0.02f, cy);      // gate lead
        p.startNewSubPath (chx, ct + er * 0.16f); p.lineTo (chx + er * 0.72f, ct + er * 0.16f); // drain
        p.lineTo          (chx + er * 0.72f, cy - er - R * 0.10f);
        p.startNewSubPath (chx, cb - er * 0.16f); p.lineTo (chx + er * 0.72f, cb - er * 0.16f); // source
        p.lineTo          (chx + er * 0.72f, cy + er + R * 0.10f);
        g.strokePath (p, stroke);
        // gate arrow (points into the channel)
        const juce::Point<float> tip (chx - er * 0.02f, cy);
        juce::Path arr;
        arr.startNewSubPath (tip.x - er * 0.30f, cy - er * 0.20f);
        arr.lineTo (tip);
        arr.lineTo (tip.x - er * 0.30f, cy + er * 0.20f);
        g.strokePath (arr, stroke);
    }
}

} // namespace felitronics::appkit::legacy
