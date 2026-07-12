// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#include <felitronics/appkit/IconButton.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

using felitronics::appkit::IconButton;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

static juce::Image renderIcon (IconButton::Kind kind, int w, int h)
{
    IconButton b (kind);
    b.setSize (w, h);
    b.colour = juce::Colours::white;

    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Graphics g (img);
    b.paintButton (g, false, false);
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

static juce::Rectangle<int> expectedEnvelope (int w, int h)
{
    const float minDim = (float) std::min (w, h);
    const float pad = minDim * 0.20f;
    const float availW = (float) w - 2.0f * pad;
    const float availH = (float) h - 2.0f * pad;
    const float scale = std::min (availW / 24.0f, availH / 24.0f);
    const float destW = 24.0f * scale;
    const float destH = 24.0f * scale;
    const float slack = std::ceil (scale * 3.0f + 2.0f);
    const int x = (int) std::floor (((float) w - destW) * 0.5f - slack);
    const int y = (int) std::floor (((float) h - destH) * 0.5f - slack);
    const int ew = (int) std::ceil (destW + 2.0f * slack);
    const int eh = (int) std::ceil (destH + 2.0f * slack);
    return juce::Rectangle<int> (x, y, ew, eh).getIntersection ({ 0, 0, w, h });
}

static bool containsRect (juce::Rectangle<int> outer, juce::Rectangle<int> inner)
{
    return inner.getX() >= outer.getX()
        && inner.getY() >= outer.getY()
        && inner.getRight() <= outer.getRight()
        && inner.getBottom() <= outer.getBottom();
}

static int alphaAt (const juce::Image& img, int x, int y)
{
    return (int) img.getPixelAt (x, y).getAlpha();
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit IconButton falsification tests\n");

    group ("all icon kinds stay inside their scaled design envelope at extreme sizes");
    {
        const std::vector<IconButton::Kind> kinds = {
            IconButton::Kind::exportFile, IconButton::Kind::importFile,
            IconButton::Kind::undo,       IconButton::Kind::redo,
            IconButton::Kind::settings,   IconButton::Kind::trash,
            IconButton::Kind::save,       IconButton::Kind::saveAs
        };
        const std::vector<juce::Point<int>> sizes = { { 12, 12 }, { 24, 24 }, { 41, 17 }, { 17, 41 }, { 128, 128 } };

        bool allInside = true;
        bool allNonBlank = true;
        for (const auto kind : kinds)
            for (const auto size : sizes)
            {
                const auto img = renderIcon (kind, size.x, size.y);
                const auto bounds = alphaBounds (img);
                allNonBlank = allNonBlank && bounds.has_value();
                if (bounds)
                    allInside = allInside && containsRect (expectedEnvelope (size.x, size.y), *bounds);
            }
        ok (allNonBlank, "every kind/size render produced visible icon pixels");
        ok (allInside, "every kind/size render stayed inside the scaled design envelope");
    }

    group ("settings gear keeps its even-odd center hole");
    {
        const auto img = renderIcon (IconButton::Kind::settings, 96, 96);
        bool centreClear = true;
        for (int y = 46; y <= 50; ++y)
            for (int x = 46; x <= 50; ++x)
                centreClear = centreClear && alphaAt (img, x, y) == 0;
        ok (centreClear, "5x5 center of the gear is transparent");
        ok (alphaAt (img, 70, 48) > 180, "right tooth is opaque");
        ok (alphaAt (img, 26, 48) > 180, "left tooth is opaque");
        ok (alphaAt (img, 48, 70) > 180, "bottom tooth is opaque");
        ok (alphaAt (img, 48, 26) > 180, "top tooth is opaque");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
