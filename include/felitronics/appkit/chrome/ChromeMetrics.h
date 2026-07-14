// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_graphics/juce_graphics.h>

//==============================================================================
// felitronics::appkit::chrome — the FabFilter-style top-toolbar "chrome": a brand blister that
// bulges out of a flat toolbar band, a continuous dipping underline, and the compare / preset cells
// that ride the band. Product-agnostic bricks: the headers take data/callbacks in and hand geometry
// out; the CONSUMER supplies the marks, the models and the colours (a ChromeTheme). Header-only; the
// consumer supplies JUCE (juce_gui_basics; this file needs only juce_graphics).
namespace felitronics::appkit::chrome
{

//==============================================================================
// ChromeMetrics — the ONE source of the bar geometry. `barHeight` is the flat toolbar band; it
// drives BOTH the blister's bulge fill AND the shell's underline stroke, so the single "band bottom"
// is defined once here rather than duplicated across the frame, the overlay and the layout.
// `blisterHeight` is the taller brand badge that overhangs the bar by (blisterHeight - barHeight).
struct ChromeMetrics
{
    float barHeight     = 30.0f;   // flat toolbar band — feeds the blister fill AND the underline stroke
    float blisterHeight = 46.0f;   // the brand badge overhangs the bar by (blisterHeight - barHeight)
};

//==============================================================================
// ChromeTheme — the chrome's paint colours, supplied by the CONSUMER (it seeds them from its own
// palette). A flat POD aggregate held BY VALUE everywhere (2 floats' worth of metrics + these
// colours cost nothing to copy and remove every dangling-reference hazard for a consumer that builds
// its theme from temporaries). Designed complete UP FRONT: this aggregate is part of the tagged
// library API, and appending a field to a positional aggregate after the fact is a silent
// designated-init/positional-init hazard for consumers — so fill it via C++20 designated init and
// grow it only with a deliberate API bump.
//
// Every field carries a neutral-dark DEFAULT member initializer (== makeDefaultDark). This keeps the
// struct an aggregate AND keeps designated init working, but means a bare `ChromeTheme{}` — or a
// partial designated init that omits a field — paints a usable dark chrome instead of transparent
// black (which would look like a broken build). A pixel-compared product still fills ALL seven via
// designated init (and MUST — see the guardrail), so the defaults only serve casual/minimal consumers.
struct ChromeTheme
{
    juce::Colour fill       = juce::Colour (0xff0e1014);   // blister bulge fill (a consumer seeds from its canvas/bg)
    juce::Colour underline  = juce::Colour (0x12ffffff);   // toolbar-bottom hairline (a faint white, ~0.07 alpha)

    // GUARDRAIL — a pixel-compared product MUST seed accent/attention from ITS OWN palette, never from
    // felitronics::appkit::brand (nor rely on these defaults): the family brand values differ from a
    // product's tuned palette (e.g. a violet 0xff9170ff vs brand 0xff9778ff; an orange 0xffff8822 vs
    // brand 0xffff8a3d), so leaning on these shifts every active-register frame / edited dot by a few LSBs.
    juce::Colour accent     = juce::Colour (0xff9778ff);   // active-register frame — SEED FROM THE PRODUCT PALETTE
    juce::Colour attention  = juce::Colour (0xffff8a3d);   // edited dot + drop ring — SEED FROM THE PRODUCT PALETTE

    juce::Colour text       = juce::Colour (0xffd8d8d8);   // bright label (hover / active)
    juce::Colour textDim    = juce::Colour (0x99ffffff);   // dim label (at rest)
    juce::Colour activeText  = juce::Colours::white;       // the on-state label inside the active register's frame

    // A named factory equal to `ChromeTheme{}` — reads clearly at a call site ("give me the default
    // dark chrome") and documents intent. A product with its own palette (or a light theme) supplies
    // its own ChromeTheme via designated init and does NOT reach for this — see the guardrail above.
    static ChromeTheme makeDefaultDark();
};

inline ChromeTheme ChromeTheme::makeDefaultDark() { return ChromeTheme {}; }

} // namespace felitronics::appkit::chrome
