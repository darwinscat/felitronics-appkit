// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// The device glyphs moved from procedural JUCE code to shared path data (DeviceGlyphPaths.h, the
// same geometry as assets/glyphs/*.svg). This suite renders the OLD code path —
// tests/legacy/ProceduralDeviceGlyph.h, the 0.10.0 drawing copied verbatim — next to the new one and
// holds the difference to a stated tolerance, at the sizes the family draws them: 16, 26 and 44 px.
//
// Each case renders twice: through JUCE's own software renderer, where the old and the new code
// stroke the same Bézier geometry and any difference is the geometry's, and through the platform's
// native renderer (CoreGraphics on the CI's macOS row), which is what a user sees. The old code drew
// its circles with Graphics::drawEllipse, which a native context renders as its own true ellipse;
// the new code strokes a path everywhere, so there the old and new differ by the renderer's idea of
// a circle as well.
//
// THEORY. The six original glyphs are linear in R except where they used the stroke width as a
// length, so the new drawing must match the old to rasteriser noise everywhere else:
//   P1  tube, bjt, fet, diode — a pure port. Software renderer: every pixel's coverage within 8/255
//       of the old (even here drawEllipse fills two concentric ellipses rather than stroking one). Native renderer: within 24/255, total ink within 2 %, IoU >= 0.97 — the envelope
//       circle, native ellipse against stroked path, is the whole difference.
//   P2  ic, dsp — three deliberate changes, each because the frame cannot hold a stroke-dependent
//       length (see tools/gen-glyphs.mjs): ic's bubble is stroked at the full width (was 0.85), dsp's
//       pin-1 dot is eight stroked spokes (was a filled 1.8-stroke disc) and its corner radius is fixed
//       (was 1.3 strokes). The ink moves only there, so on either renderer the total ink differs by
//       at most 4 % and old and new overlap (intersection over union of coverage) by at least 0.94.
// Coverage = alpha of a white glyph drawn on a transparent image; the numbers are printed per case
// so a change that passes is still visible in the log.

#include <felitronics/appkit/DeviceGlyph.h>

#include "legacy/ProceduralDeviceGlyph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace felitronics::appkit;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

struct Diff
{
    int    maxDelta = 0;      // largest per-pixel coverage difference, 0..255
    int    over32   = 0;      // pixels differing by more than 32/255
    double inkOld   = 0.0, inkNew = 0.0;
    double iou      = 1.0;    // sum(min) / sum(max) of coverage
};

template <typename Draw>
static juce::Image render (bool software, int size, Draw&& draw)
{
    const int side = size * 2;   // leads reach past the cell — pad so nothing is clipped
    juce::Image img = software ? juce::Image (juce::Image::ARGB, side, side, true, juce::SoftwareImageType())
                               : juce::Image (juce::Image::ARGB, side, side, true);
    juce::Graphics g (img);
    draw (g, juce::Rectangle<float> ((float) size * 0.5f, (float) size * 0.5f, (float) size, (float) size));
    return img;
}

static Diff compare (const juce::Image& a, const juce::Image& b)
{
    Diff d;
    double mins = 0.0, maxs = 0.0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
        {
            const int va = a.getPixelAt (x, y).getAlpha(), vb = b.getPixelAt (x, y).getAlpha();
            const int delta = std::abs (va - vb);
            d.maxDelta = std::max (d.maxDelta, delta);
            d.over32 += delta > 32 ? 1 : 0;
            d.inkOld += va;
            d.inkNew += vb;
            mins += std::min (va, vb);
            maxs += std::max (va, vb);
        }
    d.iou = maxs > 0.0 ? mins / maxs : 1.0;
    return d;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit device glyph parity (procedural 0.10.0 vs shared geometry)\n");
    const juce::Colour white (0xffffffff);
    for (const bool software : { true, false })
    {
    std::printf ("  %s renderer\n  %-6s %4s  %6s %7s %8s %8s %7s\n", software ? "software" : "native",
                 "glyph", "px", "maxD", ">32/255", "inkOld", "inkNew", "IoU");
    for (const auto type : { DeviceType::tube, DeviceType::bjt, DeviceType::fet,
                             DeviceType::diode, DeviceType::ic, DeviceType::dsp })
    {
        const bool reshaped = type == DeviceType::ic || type == DeviceType::dsp;
        const std::string name (deviceGlyphKey (type));
        for (const int px : { 16, 26, 44 })
        {
            const auto before = render (software, px, [&] (juce::Graphics& g, juce::Rectangle<float> r) { legacy::drawDeviceGlyph (g, r, type, white); });
            const auto after  = render (software, px, [&] (juce::Graphics& g, juce::Rectangle<float> r) { drawDeviceGlyph (g, r, type, white); });
            const auto d = compare (before, after);
            const double inkChange = std::abs (d.inkNew - d.inkOld) / d.inkOld;
            std::printf ("  %-6s %4d  %6d %7d %8.0f %8.0f %7.4f\n", name.c_str(), px, d.maxDelta, d.over32,
                         d.inkOld / 255.0, d.inkNew / 255.0, d.iou);

            const auto at = name + " @" + std::to_string (px) + " px, " + (software ? "software" : "native");
            ok (d.inkOld > 0.0 && d.inkNew > 0.0, at + ": both drawings put ink down");
            if (! reshaped && software)
                ok (d.maxDelta <= 8, at + ": every pixel within 8/255 of the procedural glyph (P1)");
            else if (! reshaped)
            {
                ok (d.maxDelta <= 24, at + ": every pixel within 24/255 of the procedural glyph (P1)");
                ok (inkChange <= 0.02, at + ": total ink within 2 % (P1)");
                ok (d.iou >= 0.97, at + ": IoU >= 0.97 (P1)");
            }
            else
            {
                ok (inkChange <= 0.04, at + ": total ink within 4 % (P2)");
                ok (d.iou >= 0.94, at + ": old and new ink overlap, IoU >= 0.94 (P2)");
            }
        }
    }
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
