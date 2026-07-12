// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#include <felitronics/appkit/DeviceGlyph.h>

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

using namespace felitronics::appkit;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

static std::optional<juce::Rectangle<int>> alphaBounds (const juce::Image& img)
{
    int minX = img.getWidth(), minY = img.getHeight(), maxX = -1, maxY = -1;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt (x, y).getAlpha() != 0)
            {
                minX = std::min (minX, x);
                minY = std::min (minY, y);
                maxX = std::max (maxX, x);
                maxY = std::max (maxY, y);
            }

    if (maxX < minX || maxY < minY)
        return std::nullopt;
    return juce::Rectangle<int> (minX, minY, maxX - minX + 1, maxY - minY + 1);
}

static bool containsRect (juce::Rectangle<int> outer, juce::Rectangle<int> inner)
{
    return inner.getX() >= outer.getX()
        && inner.getY() >= outer.getY()
        && inner.getRight() <= outer.getRight()
        && inner.getBottom() <= outer.getBottom();
}

static juce::Image renderStaticSpec (const DeviceSpec& spec, juce::Rectangle<float> area)
{
    juce::Image img (juce::Image::ARGB, 320, 80, true);
    juce::Graphics g (img);
    drawDeviceSpecStatic (g, area, spec);
    return img;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit DeviceGlyph falsification tests\n");

    group ("hand-built spec counts saturate without trusting parser invariants");
    {
        ok (deviceSpecCount ({ { DeviceType::tube, 100 }, { DeviceType::pnp, 100 } }) == kMaxDeviceGlyphs,
            "oversize positive hand-built counts saturate at kMaxDeviceGlyphs");
        ok (deviceSpecCount ({ { DeviceType::tube, -5 }, { DeviceType::pnp, 1 } }) == 1,
            "negative hand-built counts do not subtract from later valid entries");
    }

    group ("static glyph rows stay inside the caller-provided area");
    {
        const juce::Rectangle<float> area { 20.0f, 10.0f, 40.0f, 40.0f };
        const juce::Rectangle<int> allowed = area.getSmallestIntegerContainer().expanded (1);
        bool allNonBlank = true;
        bool allInside = true;
        for (const auto type : std::vector<DeviceType> { DeviceType::tube, DeviceType::pnp, DeviceType::fet,
                                                         DeviceType::dsp, DeviceType::diode })
        {
            const auto img = renderStaticSpec ({ { type, 1 } }, area);
            const auto bounds = alphaBounds (img);
            allNonBlank = allNonBlank && bounds.has_value();
            if (bounds)
                allInside = allInside && containsRect (allowed, *bounds);
        }
        ok (allNonBlank, "each device family rendered visible static pixels");
        ok (allInside, "each single static glyph stayed inside its row area");
    }

    group ("oversize hand-built specs do not draw past the clamped row width");
    {
        const juce::Rectangle<float> area { 20.0f, 10.0f, 120.0f, 30.0f };
        const auto img = renderStaticSpec ({ { DeviceType::tube, 100 } }, area);
        const auto bounds = alphaBounds (img);
        ok (bounds.has_value(), "oversize static spec still renders visible pixels");
        if (bounds)
            ok (containsRect (area.getSmallestIntegerContainer().expanded (2), *bounds),
                "oversize static spec is clipped by glyph count, not raw count");

        DeviceStrip strip;
        strip.set ({ { DeviceType::tube, 100 } });
        strip.setSize (120, 30);
        juce::Image stripImg (juce::Image::ARGB, 320, 80, true);
        juce::Graphics g (stripImg);
        g.addTransform (juce::AffineTransform::translation (20.0f, 10.0f));
        strip.paint (g);
        const auto stripBounds = alphaBounds (stripImg);
        ok (stripBounds.has_value(), "oversize DeviceStrip spec still renders visible pixels");
        if (stripBounds)
            ok (containsRect (juce::Rectangle<int> (20, 10, 120, 30).expanded (3), *stripBounds),
                "oversize DeviceStrip spec is bounded by the clamped glyph count");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
