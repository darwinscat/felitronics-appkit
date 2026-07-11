// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::brand — the Darwin's Cat visual identity, consolidated from the diverged
// per-product copies (orbitcab ui/BrandMark.h, orbit-capture ui/UiSupport.h): the palette, the
// concentric-ring "orbit" mark, the fixed slot palette, and the large-glyph gear button. Header-only;
// the consumer supplies JUCE (juce_gui_basics). The matching font/logo files live in this repo's
// assets/ dir — consumers embed them via their own juce_add_binary_data (see README).
//==============================================================================

#include <juce_gui_basics/juce_gui_basics.h>

namespace felitronics::appkit::brand
{

inline const juce::Colour violet { 0xff9778ff };   // primary accent
inline const juce::Colour lilac  { 0xffb9a6ff };   // mid tint
inline const juce::Colour orange { 0xffff8a3d };   // hot accent

// Fixed slot palette (Fender-style: the colour IS the slot across a whole scene — row tint,
// grid dot, slider, meter badge, strip).
inline const juce::Colour slotColours[8] = {
    juce::Colour (0xffff8a3d), juce::Colour (0xff4fc3f7), juce::Colour (0xfff06292), juce::Colour (0xff81c784),
    juce::Colour (0xffb9a6ff), juce::Colour (0xffffd54f), juce::Colour (0xffe57373), juce::Colour (0xff4db6ac) };

// The orbit "target" mark (concentric violet/lilac/orange rings). Variant 08 "converging-arrows":
// target rings + four chevrons pointing inward to an orange core — energy gathered to center =
// capture. (SVG viewBox 0..40, centre 20,20.) Moved verbatim from the product copies.
inline void drawOrbit (juce::Graphics& g, float cx, float cy, float d, bool hover = false)
{
    const float s = d / 40.0f;
    auto X = [&] (float p) { return cx + (p - 20.0f) * s; };
    auto Y = [&] (float p) { return cy + (p - 20.0f) * s; };
    auto chev = [&] (float ax, float ay, float bx, float by, float ex, float ey)
    {
        juce::Path p; p.startNewSubPath (X (ax), Y (ay)); p.lineTo (X (bx), Y (by)); p.lineTo (X (ex), Y (ey));
        g.strokePath (p, juce::PathStrokeType (2.0f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    g.setColour (juce::Colour (0xff0b0b11));                         // dark disc body
    g.fillEllipse (cx - 20.0f * s, cy - 20.0f * s, 40.0f * s, 40.0f * s);
    g.setColour (hover ? violet.brighter (0.2f) : violet);
    g.drawEllipse (cx - 18.5f * s, cy - 18.5f * s, 37.0f * s, 37.0f * s, 2.0f * s);   // violet outer ring
    g.setColour (lilac);                                             // lilac middle ring + chevrons
    g.drawEllipse (cx - 12.0f * s, cy - 12.0f * s, 24.0f * s, 24.0f * s, 1.6f * s);
    chev (16, 6, 20, 10, 24, 6); chev (16, 34, 20, 30, 24, 34);
    chev (6, 16, 10, 20, 6, 24); chev (34, 16, 30, 20, 34, 24);
    g.setColour (orange);                                            // orange core
    g.fillEllipse (cx - 3.5f * s, cy - 3.5f * s, 7.0f * s, 7.0f * s);
}

// A gear (⚙) button that draws the glyph LARGE — sized to the button, so it reads clearly in a
// toolbar (a plain TextButton renders the glyph tiny relative to its box).
struct GearButton : juce::Button
{
    GearButton() : juce::Button ("gear") {}
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.6f : over ? 0.95f : 0.7f));
        g.setFont (juce::FontOptions ((float) juce::jmin (getWidth(), getHeight()) * 1.25f));
        g.drawText (juce::String::fromUTF8 ("\xe2\x9a\x99"), getLocalBounds(), juce::Justification::centred);
    }
};

} // namespace felitronics::appkit::brand
