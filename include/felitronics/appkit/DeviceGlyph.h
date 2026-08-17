// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <felitronics/appkit/DeviceSpec.h>   // DeviceType / DeviceSpec / parse (pure juce_core — the data model, unit-tested)
#include <felitronics/appkit/Flicker.h>      // the shared one-pole heater shimmer (also used by OrbitCab's TubeDisplay)

#include <cmath>

//==============================================================================
// felitronics::appkit — schematic glyphs for a preamp's active device: a triode (tube), a bipolar
// transistor (bjt), a FET, an analogue op-amp (ic), a digital chip (dsp), or a diode. Driven by the
// "device" spec (see DeviceSpec.h) so the UI shows WHAT a capture is — e.g. 4 tubes for a V4, one
// for a Volt, a BJT for the transistor ISA, or a tube+BJT for a hybrid. Moved from OrbitCab
// ui/DeviceGlyph.h when the device visuals were promoted here.
//==============================================================================
namespace felitronics::appkit
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

// Per-family colours: the glyph stroke and the (behind-glyph) glow. Warm=tube, blue=BJT, green=FET.
inline juce::Colour deviceStroke (DeviceType t)
{
    switch (t) { case DeviceType::tube: return juce::Colour (0xfff2c793);
                 case DeviceType::bjt:  return juce::Colour (0xffc3dbf6);
                 case DeviceType::fet:  return juce::Colour (0xffc3edcf);
                 case DeviceType::dsp:  return juce::Colour (0xfff2b8b4);
                 case DeviceType::ic:   return juce::Colour (0xfff2b8e0);
                 case DeviceType::diode: return juce::Colour (0xffeef0f4);
                 case DeviceType::none: default: break; }   // explicit: keeps -Wswitch-enum consumers clean
    return juce::Colour (0xffb8b0a0);
}
inline juce::Colour deviceGlow (DeviceType t)
{
    switch (t) { case DeviceType::tube: return juce::Colour (0xffff9a3c);   // warm amber
                 case DeviceType::bjt:  return juce::Colour (0xff4e9ae8);   // blue (BJT)
                 case DeviceType::fet:  return juce::Colour (0xff58c877);   // green (FET)
                 case DeviceType::dsp:  return juce::Colour (0xffe23b3b);   // red (digital chip)
                 case DeviceType::ic:   return juce::Colour (0xffe264b4);   // magenta (analogue op-amp)
                 case DeviceType::diode: return juce::Colour (0xffffffff);  // white (diode)
                 case DeviceType::none: default: break; }   // explicit: keeps -Wswitch-enum consumers clean
    return juce::Colours::transparentBlack;
}

// Draw a spec as a STATIC row (no glow) — used in combo popup items. Each glyph is stroked in its
// own family colour, so a hybrid reads as a warm tube next to a blue transistor.
inline void drawDeviceSpecStatic (juce::Graphics& g, juce::Rectangle<float> area, const DeviceSpec& spec)
{
    const int total = deviceSpecCount (spec);
    if (total <= 0)
        return;
    const float cell = juce::jmin (area.getHeight(), area.getWidth() / (float) total);
    auto row = area.withSizeKeepingCentre (cell * (float) total, cell);
    int drawn = 0;
    for (const auto& [type, cnt] : spec)
    {
        if (glyphsForType (type, cnt) <= 0 || drawn >= total)
            continue;
        ++drawn;
        drawDeviceGlyph (g, row.removeFromLeft (cell).reduced (cell * 0.12f), type, deviceStroke (type));
    }
}

// The device glyph strip (OrbitCab shows it below the preamp combo): N schematic glyphs, each over
// a soft flickering glow tinted by device family — warm amber (tube), blue (BJT), green (FET). The
// flicker is the shared appkit::Flicker shimmer (the same kernel OrbitCab's poweramp heater tubes
// use), advanced by the owning editor's 30 Hz timer.
class DeviceStrip : public juce::Component
{
public:
    DeviceStrip() { setInterceptsMouseClicks (false, false); }

    void set (DeviceSpec s)
    {
        if (s == spec) return;
        spec = std::move (s); repaint();
    }

    // Advance the heater/glow flicker one frame + repaint (editor 30 Hz timer, only while visible).
    void tick()
    {
        if (spec.empty())
            return;
        glow.tick();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const int total = deviceSpecCount (spec);
        if (total <= 0)
            return;
        auto area = getLocalBounds().toFloat();
        const float cell = juce::jmin (area.getHeight(), area.getWidth() / (float) total);
        auto row = area.withSizeKeepingCentre (cell * (float) total, cell);
        int gi = 0;
        for (const auto& [type, cnt] : spec)
        {
            if (glyphsForType (type, cnt) <= 0 || gi >= total)
                continue;

            auto c = row.removeFromLeft (cell);
            const auto  ctr  = c.getCentre();
            // Bounded by the strip's own height as well as the cell: now that a part is drawn once,
            // the cell is as large as the row is tall, and a glow sized off it alone spilled past the
            // component — clipped to a flat-topped semicircle by whoever painted it.
            const float rad  = juce::jmin (cell * 0.66f, area.getHeight() * 0.5f);
            const float lvl  = glow.level (gi);   // clamped indexing, matches the spec's glyph cap
            const auto  gc   = deviceGlow (type); // per-glyph colour → hybrid glows amber + blue
            juce::ColourGradient grad (gc.withAlpha (0.60f * lvl), ctr.x, ctr.y,
                                       gc.withAlpha (0.0f),        ctr.x + rad, ctr.y, true);
            g.setGradientFill (grad);
            g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (ctr));
            drawDeviceGlyph (g, c.reduced (cell * 0.14f), type, deviceStroke (type));
            ++gi;
        }
    }

private:
    DeviceSpec               spec;
    Flicker<kMaxDeviceGlyphs> glow;
};

} // namespace felitronics::appkit
