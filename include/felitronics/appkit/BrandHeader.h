// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::BrandHeader — the family's clickable header strip, generalized from the
// orbit-capture/orbitcab copies for the third product that needed it: [cat] [orbit mark]
// <Product>  by Darwin's Cat — the branded run links to the product home URL; the right edge is a
// version readout whose CLICK the product wires to its update/about window, with an orange badge
// while a newer release is stored (see UpdateChecker). The consumer passes its embedded assets
// (catlogo.svg + Michroma from this repo's assets/, via its own juce_add_binary_data) — the header
// stays header-only. Michroma wordmark; byline is an inline violet continuation on the baseline;
// soft halo + hand cursor over the link run only.
//==============================================================================

#include <felitronics/appkit/Brand.h>

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace felitronics::appkit
{

struct BrandHeader : juce::Component
{
    BrandHeader (const void* logoSvg, size_t logoSvgSize,
                 const void* wordmarkTtf, size_t wordmarkTtfSize,
                 juce::String productName, juce::String homeUrl)
        : logo (juce::Drawable::createFromImageData (logoSvg, logoSvgSize)),
          wordmarkFace (juce::Typeface::createSystemTypefaceFor (wordmarkTtf, wordmarkTtfSize)),
          product (std::move (productName)), home (std::move (homeUrl)) {}

    juce::String version;                      // right-aligned readout; CLICK opens the update window
    bool updateDot = false;                    // orange badge next to the version when a newer release is stored
    int clickRight = 1 << 30;                  // set from the owner's resized(): end of the byline text
    std::function<void()> onVersionClick;      // wired by the orchestrator (the update/about window)

    bool linkArea (juce::Point<float> p) const { return p.x < (float) clickRight; }

    void updateHover (juce::Point<float> p)
    {
        const bool h = linkArea (p), vh = versionArea.contains (p);
        setMouseCursor (h || vh ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        if (h != hover || vh != versionHover) { hover = h; versionHover = vh; repaint(); }
    }
    void mouseEnter (const juce::MouseEvent& e) override { updateHover (e.position); }
    void mouseMove  (const juce::MouseEvent& e) override { updateHover (e.position); }
    void mouseExit  (const juce::MouseEvent&)   override { hover = false; versionHover = false; repaint(); }
    void mouseUp    (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        if (linkArea (e.position))
            juce::URL (home).launchInDefaultBrowser();
        else if (versionArea.contains (e.position) && onVersionClick)
            onVersionClick();
    }

    static float textWidth (const juce::Font& f, const juce::String& s)
    {
        juce::GlyphArrangement ga; ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    // Where the branded run ends — the link hit-area boundary. Mirrors paint()'s x math.
    float contentRight() const
    {
        const float h = (float) getHeight(), d = h * 0.86f;
        const auto wf = juce::Font (juce::FontOptions().withHeight (h * 0.44f).withTypeface (wordmarkFace));
        const auto bf = juce::Font (juce::FontOptions().withHeight (h * 0.26f).withTypeface (wordmarkFace));
        return h + 6.0f + d + 14.0f + textWidth (wf, product) + 14.0f + textWidth (bf, byline) + 8.0f;
    }

    void paint (juce::Graphics& g) override
    {
        const float h = (float) getHeight();
        const float cy = getLocalBounds().toFloat().getCentreY();
        if (hover)
        {
            g.setColour (brand::violet.withAlpha (0.14f));            // halo over the LINK run only
            g.fillRoundedRectangle (getLocalBounds().toFloat().withRight ((float) clickRight), 8.0f);
        }
        if (logo != nullptr)                                          // cat logo (square) far left
            logo->drawWithin (g, juce::Rectangle<float> (4.0f, 2.0f, h - 4.0f, h - 4.0f),
                              juce::RectanglePlacement::centred, 1.0f);
        const float d = h * 0.86f;                                    // orbit mark (bigger)
        brand::drawOrbit (g, h + 6.0f + d * 0.5f, cy, d, hover);
        float x = h + 6.0f + d + 14.0f;
        const auto wf = juce::Font (juce::FontOptions().withHeight (h * 0.44f).withTypeface (wordmarkFace));
        const float baseline = cy + (wf.getAscent() - wf.getDescent()) * 0.5f;
        g.setFont (wf);
        g.setColour (hover ? juce::Colours::white : juce::Colour (0xffeef0f6));
        g.drawSingleLineText (product, juce::roundToInt (x), juce::roundToInt (baseline));
        x += textWidth (wf, product) + 14.0f;
        const auto bf = juce::Font (juce::FontOptions().withHeight (h * 0.26f).withTypeface (wordmarkFace));
        g.setFont (bf);
        g.setColour (hover ? brand::violet.brighter (0.3f) : brand::violet);
        g.drawSingleLineText (byline, juce::roundToInt (x), juce::roundToInt (baseline));
        if (version.isNotEmpty())                                     // version readout, far right, same line
        {
            const auto vf = juce::Font (juce::FontOptions().withHeight (h * 0.24f));
            const float tw = textWidth (vf, version);
            const float dotD = h * 0.15f, gap = updateDot ? dotD + 7.0f : 0.0f, right = (float) getWidth() - 12.0f;
            const float left = right - tw - gap;
            versionArea = { left - 8.0f, 0.0f, tw + gap + 20.0f, h };  // generous hit target
            if (updateDot)                                            // the "newer release exists" badge
            {
                g.setColour (brand::orange);
                g.fillEllipse (left, cy - dotD * 0.5f, dotD, dotD);
            }
            g.setFont (vf);
            g.setColour (juce::Colours::white.withAlpha (versionHover ? 0.8f : 0.35f));
            g.drawText (version, juce::Rectangle<float> (left + gap, 0.0f, tw + 2.0f, h),
                        juce::Justification::centredLeft, false);
        }
        g.setColour (juce::Colour (0x22ffffff));
        g.fillRect (0, getHeight() - 1, getWidth(), 1);               // separator
    }

private:
    std::unique_ptr<juce::Drawable> logo;
    juce::Typeface::Ptr wordmarkFace;
    juce::String product, home;
    const juce::String byline { "by Darwin's Cat" };
    bool hover = false, versionHover = false;
    juce::Rectangle<float> versionArea;
};

} // namespace felitronics::appkit
