// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#include <felitronics/appkit/Brand.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace brand = felitronics::appkit::brand;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

using DrawMark = void (*) (juce::Graphics&, float, float, float, bool);

static juce::Image renderMark (DrawMark draw, int size, float diameter)
{
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    draw (g, (float) size * 0.5f, (float) size * 0.5f, diameter, false);
    return img;
}

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

static bool nearColour (juce::Colour actual, juce::Colour expected, int tolerance)
{
    return std::abs ((int) actual.getRed()   - (int) expected.getRed())   <= tolerance
        && std::abs ((int) actual.getGreen() - (int) expected.getGreen()) <= tolerance
        && std::abs ((int) actual.getBlue()  - (int) expected.getBlue())  <= tolerance
        && std::abs ((int) actual.getAlpha() - (int) expected.getAlpha()) <= tolerance;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit Brand falsification tests\n");

    group ("orbit marks stay inside their declared component footprint");
    {
        bool allNonBlank = true;
        bool allInside = true;
        for (const float diameter : { 8.0f, 17.0f, 40.0f, 96.0f })
        {
            const int size = 128;
            const int slack = (int) std::ceil (diameter / 40.0f + 2.0f);
            const int left = (int) std::floor ((float) size * 0.5f - diameter * 0.5f) - slack;
            const int top  = (int) std::floor ((float) size * 0.5f - diameter * 0.5f) - slack;
            const int side = (int) std::ceil (diameter + 2.0f * (float) slack);
            const juce::Rectangle<int> allowed = juce::Rectangle<int> (left, top, side, side).getIntersection ({ 0, 0, size, size });

            for (const auto draw : std::vector<DrawMark> { brand::drawOrbit, brand::drawOrbitRings })
            {
                const auto img = renderMark (draw, size, diameter);
                const auto bounds = alphaBounds (img);
                allNonBlank = allNonBlank && bounds.has_value();
                if (bounds)
                    allInside = allInside && containsRect (allowed, *bounds);
            }
        }
        ok (allNonBlank, "every mark/diameter render produced pixels");
        ok (allInside, "every mark/diameter render stayed inside the allowed footprint");
    }

    group ("visual fingerprint pixels remain pinned");
    {
        const auto chevron = renderMark (brand::drawOrbit, 128, 96.0f);
        ok (nearColour (chevron.getPixelAt (64, 64), brand::orange, 0), "chevron orbit center is brand orange");
        ok (chevron.getPixelAt (0, 0).getAlpha() == 0, "chevron orbit corner stays transparent");

        const auto rings = renderMark (brand::drawOrbitRings, 128, 96.0f);
        ok (nearColour (rings.getPixelAt (64, 64), juce::Colour (0xff0b0b11), 0), "ring orbit center is the dark body");
        ok (nearColour (rings.getPixelAt (78, 64), brand::orange, 4), "ring orbit inner ring is orange");
        ok (rings.getPixelAt (0, 0).getAlpha() == 0, "ring orbit corner stays transparent");
    }

    group ("wordmark fallback font metrics are sane");
    {
        const auto f = brand::wordmarkFont (nullptr, 20.0f);
        ok (brand::textWidth (f, "") == 0.0f, "empty wordmark has zero width");
        ok (brand::textWidth (f, "Felitronics") > brand::textWidth (f, "F"), "longer wordmark measures wider");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
