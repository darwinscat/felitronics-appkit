// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ChromeMetrics.h"

#include <cmath>

//==============================================================================
// The flat, self-painted toolbar widgets the chrome cells (compare / preset) ride on. Theme-driven:
// each button carries a ChromeTheme BY VALUE (defaulted so a bare button still paints; the owning
// cell overwrites it with the consumer's theme). No product palette is reached for — the seam is the
// theme.
namespace felitronics::appkit::chrome
{

//==============================================================================
// FlatItem — a flat text item (unframed chrome): no background, no outline — just the label, dim at
// rest and brightening on hover, FabFilter-style. A PUBLIC generic primitive (a consumer may reuse
// it directly for any flat text button in a toolbar). Popups launched off these open upward at the
// window's bottom edge automatically.
class FlatItem : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    ChromeTheme theme = ChromeTheme::makeDefaultDark();   // the owning cell overwrites with the consumer theme

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        g.setColour ((highlighted ? theme.text : theme.textDim).withAlpha (isEnabled() ? 1.0f : 0.4f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }
};

//==============================================================================
// detail — INTERNAL to the chrome layer, NOT part of the public API. The undo/redo arrow lives here
// (not as a public primitive) deliberately: felitronics::appkit::IconButton is `final` and already
// ships undo/redo vector icons to the family, so a second public undo/redo button would be a rival
// primitive. This one exists only because the compare cell needs a FLAT, theme-driven arrow that
// matches its bare register letters; it is an implementation detail of CompareCell.
namespace detail
{

//==============================================================================
// HistoryArrowButton — self-painted curved undo/redo arrows. Unicode arrow glyphs (↶/↷) render as
// mismatched emoji or tofu depending on the host's font stack, so the icon is a Path instead.
//
// TODO(chrome→IconButton unification, deferred): fold this into appkit::IconButton (Kind::undo/redo)
// once a FLAT (frame-less) variant + a theme-colour seam land there. The two arrows are NOT yet
// pixel-interchangeable — reconcile these EXACT params before merging:
//   • arc:    here a0=-2.4, a1=+1.0 rad (from 12 o'clock), rad = 0.26·min(w,h);
//             IconButton: a0=60°, a1=345°, r=6.6 in a 24×24 design box.
//   • stroke: here 1.7 px curved/rounded; IconButton 2.0 px curved/rounded.
//   • head:   here hl=0.85·rad, hw=0.55·rad; IconButton hl=6.0, hw=3.6 (design-box units).
//   • colour: here theme.text/textDim (+ disabled 0.35 alpha); IconButton a single public `colour`
//             member with over/down multipliers — a flat variant must take the theme pair.
class HistoryArrowButton final : public juce::TextButton
{
public:
    explicit HistoryArrowButton (bool pointsRight) : redoArrow (pointsRight) {}

    ChromeTheme theme = ChromeTheme::makeDefaultDark();   // the owning cell overwrites with the consumer theme

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        // FLAT — no stock frame; a soft tint on hover, the arrow dimmed when disabled.
        if ((highlighted || down) && isEnabled())
        {
            g.setColour (theme.text.withAlpha (down ? 0.16f : 0.08f));
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f);
        }

        const auto  b   = getLocalBounds().toFloat();
        const auto  c   = b.getCentre();
        const float rad = juce::jmin (b.getWidth(), b.getHeight()) * 0.26f;

        // A 3/4 arc, clockwise from lower-left past 12 o'clock, with a tangent-aligned arrowhead
        // at the end — the classic "redo" shape; undo is its mirror.
        const float a0 = -2.4f, a1 = 1.0f;                 // radians, clockwise from 12 o'clock
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, a1, true);

        const juce::Point<float> end  (c.x + rad * std::sin (a1), c.y - rad * std::cos (a1));
        const juce::Point<float> dir  (std::cos (a1), std::sin (a1));   // clockwise tangent at a1
        const juce::Point<float> perp (-dir.y, dir.x);
        const float hl = rad * 0.85f, hw = rad * 0.55f;
        juce::Path head;
        head.addTriangle (end + dir * hl, end + perp * hw, end - perp * hw);

        if (! redoArrow)
        {
            const auto flip = juce::AffineTransform::scale (-1.0f, 1.0f, c.x, c.y);
            arc.applyTransform (flip);
            head.applyTransform (flip);
        }

        g.setColour ((highlighted ? theme.text : theme.textDim).withAlpha (isEnabled() ? 1.0f : 0.35f));
        g.strokePath (arc, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillPath (head);
    }

private:
    bool redoArrow;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HistoryArrowButton)
};

} // namespace detail

} // namespace felitronics::appkit::chrome
