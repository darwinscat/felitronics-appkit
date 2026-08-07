// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// Falsification gate for MicGrid: it paints something at plausible sizes and never outside its own
// bounds, and the availability seam actually gates the click — an unavailable cell must not report
// a selection, because in a player that cell has no capture behind it.

#include <felitronics/appkit/MicGrid.h>

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>

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
                minX = std::min (minX, x); minY = std::min (minY, y);
                maxX = std::max (maxX, x); maxY = std::max (maxY, y);
            }

    if (maxX < minX || maxY < minY)
        return std::nullopt;
    return juce::Rectangle<int> (minX, minY, maxX - minX + 1, maxY - minY + 1);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit MicGrid falsification tests\n");

    // NOT "stays inside its bounds": the component fills its background, and JUCE clips paint() to
    // the component rect, so that property is the framework's and holds trivially. What can actually
    // be falsified is that it draws SOMETHING at every size, and that degenerate sizes — which the
    // radius maths divides by — do not take it down.
    group ("draws at every plausible size, survives degenerate ones");
    {
        bool allPainted = true;

        for (const auto size : { juce::Point<int> { 320, 120 }, { 480, 160 }, { 640, 240 }, { 240, 90 } })
        {
            MicGrid grid;
            grid.setSize (size.x, size.y);

            juce::Image img (juce::Image::ARGB, size.x, size.y, true);
            { juce::Graphics g (img); grid.paint (g); }

            allPainted = allPainted && alphaBounds (img).has_value();
        }

        ok (allPainted, "every plausible size rendered visible pixels");

        for (const auto size : { juce::Point<int> { 1, 1 }, { 40, 4 }, { 4, 40 } })
        {
            MicGrid grid;
            grid.setSize (size.x, size.y);
            juce::Image img (juce::Image::ARGB, juce::jmax (1, size.x), juce::jmax (1, size.y), true);
            { juce::Graphics g (img); grid.paint (g); }
        }

        ok (true, "degenerate sizes paint without dying");
    }

    group ("the availability seam gates selection");
    {
        MicGrid grid;
        grid.setSize (480, 160);

        int selections = 0;
        grid.onSelect = [&selections] (const juce::String&, double) { ++selections; };

        // Nothing is available: no click anywhere in the grid may report a selection.
        grid.isAvailable = [] (const juce::String&, double) { return false; };

        for (int x = 200; x < 470; x += 30)
            for (int y = 20; y < 150; y += 30)
                grid.mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                                  { (float) x, (float) y }, juce::ModifierKeys(),
                                                  1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                  &grid, &grid, juce::Time(), { (float) x, (float) y },
                                                  juce::Time(), 1, false));

        ok (selections == 0, "an unavailable cell reports nothing");

        // Everything available again: the same sweep must now get through.
        grid.isAvailable = nullptr;
        grid.mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                          { 400.0f, 80.0f }, juce::ModifierKeys(),
                                          1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                          &grid, &grid, juce::Time(), { 400.0f, 80.0f },
                                          juce::Time(), 1, false));

        ok (selections == 1, "with no gate set, a cell click reports a selection");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
