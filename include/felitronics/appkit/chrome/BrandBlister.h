// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ChromeMetrics.h"

#include <cmath>

namespace felitronics::appkit::chrome
{

//==============================================================================
// BlisterMark — the CONSUMER's brand mark, drawn inside the blister frame. The consumer hands the
// frame a concrete mark (its logo + wordmark) and the mark reports the blister's preferred CONTENT
// width for a given height. Kept abstract so BrandBlister stays product-free while the concrete mark
// stays in the product.
struct BlisterMark : juce::Component
{
    // Preferred CONTENT width (skirts + pads EXCLUDED) at a given blister height. The consumer owns
    // this because only it knows its content (marks + wordmark + font metrics); the FRAME adds its
    // own skirt/pad geometry around this (see BrandBlister::preferredWidth), so a mark never has to
    // know the frame's skirt constants to agree on a total width.
    //
    // ROUNDING CONTRACT — return the CEILING of a fractional content width. The frame adds an
    // INTEGRAL skirt/pad and does NOT round again (see preferredWidth), so if you truncate/round-DOWN
    // a fractional content width here the blister comes out up to 1px narrow — a pixel-parity trap for
    // a product migrating its mark onto this frame. Return `(int) std::ceil (yourFloatWidth)`.
    virtual int preferredContentWidth (int blisterHeight) const = 0;
};

//==============================================================================
// BrandBlister — the FabFilter-style brand blister as a FRAME (not a generic cell). It owns the
// downward BULGE of the toolbar, the soft skirts at each end, the plateau depth and
// `contentLeftOffset`; a single ChromeMetrics.barHeight drives BOTH this fill AND the shell's
// underline stroke. The consumer hands it a MARK component; the frame FILLS the bulge (theme.fill)
// and frames the mark on top. The continuous hairline is drawn ONCE by the shell overlay
// (ChromeUnderline) — never here — so there is no line junction to mismatch.
//
//    ╱‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾╲
//   (   <the consumer mark>  )
//    ╲__________________╱
//
// The frame is mouse-transparent to ITSELF (setInterceptsMouseClicks(false, true)); the mark child
// covers the whole badge and owns the hover + click, so the entire blister is the clickable link.
//
// Metrics + theme are held BY VALUE (flat PODs; copying them costs nothing and removes every
// dangling-reference hazard for a consumer that builds them from temporaries). The non-owned mark
// pointer is made foolproof the same way ChromeUnderline hardens its blister (C7): the frame
// self-subscribes as a ComponentListener on the mark and nulls the pointer if the mark is destroyed
// first, so a mis-ordered teardown paints nothing rather than dangling.
class BrandBlister final : public juce::Component,
                           private juce::ComponentListener
{
public:
    BrandBlister (const ChromeMetrics& m, const ChromeTheme& t) : metrics (m), theme (t)
    {
        setInterceptsMouseClicks (false, true);   // child-only hit-testing — the mark handles the click
    }

    ~BrandBlister() override
    {
        if (mark != nullptr)
            mark->removeComponentListener (this);
    }

    // The consumer's mark. The frame does not own it (the consumer configures its assets); the frame
    // sizes it to the whole badge so the entire blister stays clickable. Foolproof against the mark
    // being destroyed out from under the frame (see componentBeingDeleted).
    void setMark (BlisterMark* m)
    {
        if (mark == m) return;
        if (mark != nullptr) { mark->removeComponentListener (this); removeChildComponent (mark); }
        mark = m;
        if (mark != nullptr) { addAndMakeVisible (*mark); mark->addComponentListener (this); }
        resized();
    }

    // The full preferred blister width. The FRAME owns the skirt + pad geometry; the mark reports
    // only its CONTENT width, and the frame wraps it in the two soft transitions + inner pads. An
    // absent mark still returns a VALID minimum (room for the two transitions) so appendBottomLine
    // never degenerates into a zero-width dip.
    int preferredWidth (int height) const
    {
        if (mark == nullptr)
            return (int) std::ceil (2.0f * (kEndFlat + kTransW));
        // `skirtsAndPads` is an exact integer with the current constants, so this ceil never rounds
        // the total — the sub-pixel rounding is the mark's job (see preferredContentWidth's contract).
        const float skirtsAndPads = 2.0f * (kEndFlat + kTransW) + kPadL + kPadR;
        return (int) std::ceil (skirtsAndPads + (float) mark->preferredContentWidth (height));
    }

    //==============================================================================
    // Skirt geometry — the FRAME owns it. Public so the consumer mark can size its content to the
    // frame (it offsets its content by contentLeftOffset()). Geometry constants are VERBATIM.
    static constexpr float kEndFlat = 12.0f;   // flat run along the toolbar line at each end (tangent)
    static constexpr float kTransW  = 30.0f;   // width of each soft S-transition
    static constexpr float kPadL = 5.0f, kPadR = 5.0f;   // inner pads between the skirt and the content

    // The content's left edge sits this far from the blister's own left edge. The shell clamps the
    // group so the content never crosses the window edge: the blister then slides its left skirt
    // OFF-SCREEN (clipped by the window → a straight rectangular cut) with NO content jump.
    static constexpr float contentLeftOffset() { return kEndFlat + kTransW + kPadL; }

    // Append the bulge bottom profile (left flat run + dip + right flat run) to `p`, in ABSOLUTE
    // coords, continuing from the current point at (x0, y0). Ends at (x0 + w, y0). Shared by this
    // frame's own fill AND the full-width shell underline overlay, so the line is ONE path.
    // GEOMETRY VERBATIM — the loop count, the -1.0f plateau, and the order of ops are pixel-critical.
    static void appendBottomLine (juce::Path& p, float x0, float w, float y0, float yMax)
    {
        const float dipL = x0 + kEndFlat;
        const float dipR = x0 + w - kEndFlat;
        p.lineTo (dipL, y0);                                // left flat run
        for (int i = 1; i <= kN; ++i)                       // left S-transition
            p.lineTo (dipL + (float) i / (float) kN * kTransW, y0 + (yMax - y0) * smoothstep ((float) i / (float) kN));
        p.lineTo (dipR - kTransW, yMax);                    // flat plateau under the content
        for (int i = 1; i <= kN; ++i)                       // right S-transition
            p.lineTo (dipR - kTransW + (float) i / (float) kN * kTransW, y0 + (yMax - y0) * smoothstep (1.0f - (float) i / (float) kN));
        p.lineTo (x0 + w, y0);                              // right flat run
    }

    void paint (juce::Graphics& g) override
    {
        const auto  b = getLocalBounds().toFloat();
        const float W = b.getWidth(), H = b.getHeight();

        // The blister is a downward BULGE of the toolbar itself (SAME colour, not an overlaid panel):
        // the toolbar's bottom line DIPS to a flat plateau under the mark and rises back, blending in
        // by TANGENT (zero-slope smoothstep) with no corner. Here we only FILL the bulge (theme.fill);
        // the LINE is drawn ONCE, full width, by the shell's ChromeUnderline overlay (using the SAME
        // appendBottomLine), so there is no line-junction to mismatch.
        const float y0   = metrics.barHeight;   // ONE source (feeds fill AND the underline stroke)
        const float yMax = H - 1.0f;

        juce::Path fill;
        fill.startNewSubPath (0.0f, y0);
        appendBottomLine (fill, 0.0f, W, y0, yMax);   // dip profile, ends at (W, y0)
        fill.lineTo (W, 0.0f);
        fill.lineTo (0.0f, 0.0f);
        fill.closeSubPath();
        g.setColour (theme.fill);
        g.fillPath (fill);
    }

    void resized() override { if (mark != nullptr) mark->setBounds (getLocalBounds()); }

private:
    void componentBeingDeleted (juce::Component& c) override { if (&c == mark) mark = nullptr; }

    static constexpr int kN = 30;   // dip curve resolution

    // Smoothstep 0→1 with zero slope at both ends — the sigmoid that blends the plateau into the
    // toolbar line without a corner (a soft S-curve transition).
    static float smoothstep (float t) { t = juce::jlimit (0.0f, 1.0f, t); return t * t * (3.0f - 2.0f * t); }

    ChromeMetrics metrics;
    ChromeTheme   theme;
    BlisterMark*  mark = nullptr;   // not owned — the consumer configures + outlives it

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrandBlister)
};

} // namespace felitronics::appkit::chrome
