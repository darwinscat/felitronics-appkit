// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <felitronics/appkit/BrandAssets.h>   // the mark and the wordmark face, carried by the library itself

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

// The brand palette. The VALUES here are the brand home — product LookAndFeels may intentionally
// diverge for their own scenes; such a divergence is a per-product decision, not drift to sync back.
inline const juce::Colour violet { 0xff9778ff };   // primary accent
inline const juce::Colour lilac  { 0xffb9a6ff };   // mid tint
inline const juce::Colour orange { 0xffff8a3d };   // hot accent

// Fixed slot palette (Fender-style: the colour IS the slot across a whole scene — row tint,
// grid dot, slider, meter badge, strip).
inline const juce::Colour slotColours[8] = {
    juce::Colour (0xffff8a3d), juce::Colour (0xff4fc3f7), juce::Colour (0xfff06292), juce::Colour (0xff81c784),
    juce::Colour (0xffb9a6ff), juce::Colour (0xffffd54f), juce::Colour (0xffe57373), juce::Colour (0xff4db6ac) };

// The orbit "target" mark, CHEVRON variant — ORBIT-CAPTURE's mark. Variant 08 "converging-arrows":
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

// The orbit "target" mark, RING variant — ORBITCAB's mark (the app-icon "variant I" family: three
// concentric rings on a dark disc, orange innermost — "a cabinet on orbit"). Centred at (cx, cy),
// diameter d; stroke widths scale from the d=40 SVG design. Moved verbatim from orbitcab
// ui/BrandMark.h drawOrbit.
inline void drawOrbitRings (juce::Graphics& g, float cx, float cy, float d, bool hover = false)
{
    const float r = d * 0.5f;
    const float s = d / 40.0f;
    g.setColour (juce::Colour (0xff0b0b11));                         // dark planet body
    g.fillEllipse (cx - r, cy - r, d, d);
    g.setColour (hover ? violet.brighter (0.2f) : violet);
    g.drawEllipse (cx - r, cy - r, d, d, 2.0f * s);                  // violet outer ring
    const float r2 = r * (13.0f / 20.0f);
    g.setColour (lilac);                                             // lilac middle ring
    g.drawEllipse (cx - r2, cy - r2, r2 * 2.0f, r2 * 2.0f, 2.0f * s);
    const float r3 = r * (6.0f / 20.0f);
    g.setColour (orange);                                            // orange inner ring
    g.drawEllipse (cx - r3, cy - r3, r3 * 2.0f, r3 * 2.0f, 2.5f * s);
}

// The suite tip-jar — "Feed the Cat". ONE canonical URL for the whole family: the site's
// steerable hop, which 302s to wherever SiteConfig.tipJarUrl points today. Deliberately NOT the
// payment page itself — a URL baked into a shipped binary lives for years, and the hop lets the
// destination change server-side without re-teaching a single old build. Consumers surface it
// via VersionBadge's feed row (on by default) or their own affordance; a product opts out by
// clearing VersionBadge::Config::feedUrl.
inline const char* const feedTheCatUrl = "https://darwinscat.com/feed-the-cat";

// The click-through for a product's "Feed the Cat" affordance: the base URL plus the signature of
// the thing that is asking — `from` (the product name folded to bare [a-z0-9]), `platform` (the OS
// this binary was compiled for) and, when the caller knows it, `format` (VST3 / AU / CLAP /
// Standalone). Three columns in the server's access log: which app fed the cat, from which kind of
// machine, and whether the player was in a host or standing alone.
//
// None of it reaches the payment page — the hop logs the query and redirects clean — so this is
// access-log statistics, not tracking, and it carries nothing about the person.
//
// `baseUrl` stays the second parameter it has always been, so a product that passes its own hop
// (with its own campaign tail on it) keeps working and gains the new pair for free.
inline juce::URL feedTheCatLink (const juce::String& productName,
                                 const juce::String& baseUrl = feedTheCatUrl,
                                 const juce::String& pluginFormat = {})
{
   #if JUCE_MAC
    const char* const platform = "macos";
   #elif JUCE_WINDOWS
    const char* const platform = "windows";
   #elif JUCE_LINUX
    const char* const platform = "linux";
   #else
    const char* const platform = "other";
   #endif

    const auto fold = [] (const juce::String& s)
    {
        return s.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789");
    };

    const juce::String slug = fold (productName);
    juce::URL url (baseUrl);
    if (slug.isNotEmpty())
        url = url.withParameter ("from", slug);
    url = url.withParameter ("platform", platform);

    const juce::String fmt = fold (pluginFormat);
    return fmt.isEmpty() ? url : url.withParameter ("format", fmt);
}

// The paw print for the "Feed the Cat" affordance — where a cat ate, it leaves a print. A pad and
// four fanned toes, drawn filled in `colour` (the hot accent, canonically); centred at (cx, cy),
// diameter d, chunky enough to stay legible at popover-row size (~14 px).
inline void drawPaw (juce::Graphics& g, float cx, float cy, float d, juce::Colour colour)
{
    const float s = d / 40.0f;
    g.setColour (colour);
    auto blob = [&] (float w, float h, float bx, float by, float angle)
    {
        juce::Path p;
        p.addEllipse (-w * 0.5f, -h * 0.5f, w, h);
        p.applyTransform (juce::AffineTransform::rotation (angle)
                              .scaled (s)
                              .translated (cx + (bx - 20.0f) * s, cy + (by - 20.0f) * s));
        g.fillPath (p);
    };
    blob (19.0f, 14.0f, 20.0f, 26.0f, 0.0f);       // the pad
    blob (6.6f, 9.4f, 9.0f, 15.0f, -0.55f);        // outer toes, tilted outward
    blob (6.6f, 9.4f, 31.0f, 15.0f, 0.55f);
    blob (6.6f, 9.4f, 15.6f, 9.6f, -0.18f);        // inner toes
    blob (6.6f, 9.4f, 24.4f, 9.6f, 0.18f);
}

// The wordmark font — embedded Michroma (OFL, this repo's assets/) via the supplied typeface;
// bold system fallback if null.
inline juce::Font wordmarkFont (juce::Typeface::Ptr typeface, float height)
{
    if (typeface != nullptr)
        return juce::Font (juce::FontOptions().withHeight (height).withTypeface (typeface));
    return juce::Font (juce::FontOptions (height, juce::Font::bold));
}

// Rendered width of `s` in font `f` (for laying the wordmark + trailing text out by hand).
inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
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
