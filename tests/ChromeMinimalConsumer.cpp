// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// MINIMAL-consumer gate for the chrome layer. A separate TU that deliberately:
//   • includes ONE chrome header FIRST (self-containment: each header pulls its own deps),
//   • links only felitronics::appkit + juce_gui_basics (the smallest module set the chrome needs),
//   • supplies a NON-tabby mark + a designated-init ChromeTheme,
//   • composes a shell with the blister + underline ONLY — NO compare cell, NO preset cell —
// proving the chrome composition is à la carte (a capture-style consumer that wants only a brand
// blister need not pull the compare/preset machinery).
#include <felitronics/appkit/chrome/BrandBlister.h>   // ONE chrome header FIRST — self-containment gate

#include <felitronics/appkit/chrome/ChromeBar.h>
#include <felitronics/appkit/chrome/ChromeUnderline.h>

#include <cstdio>

namespace chrome = felitronics::appkit::chrome;

// A minimal, NON-tabby brand mark — just a placeholder dot. Proves the frame drives ANY mark that
// reports a content width, not a product-specific one.
struct DotMark : chrome::BlisterMark
{
    int preferredContentWidth (int blisterHeight) const override { return blisterHeight; }   // square-ish
    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::white);
        g.fillEllipse (getLocalBounds().toFloat().withSizeKeepingCentre (10.0f, 10.0f));
    }
};

// A near-empty shell: brand blister + dipping underline, laid out by ChromeBar — and nothing else.
struct MiniShell : juce::Component
{
    chrome::ChromeMetrics metrics {};
    chrome::ChromeTheme   theme { .fill       = juce::Colour (0xff101418),
                                  .underline  = juce::Colour (0x14ffffff),
                                  .accent     = juce::Colour (0xff8877ff),
                                  .attention  = juce::Colour (0xffffaa33),
                                  .text       = juce::Colours::white,
                                  .textDim    = juce::Colour (0x80ffffff),
                                  .activeText = juce::Colours::white };
    DotMark                mark;
    chrome::BrandBlister   blister   { metrics, theme };
    chrome::ChromeUnderline underline { blister, metrics, theme };
    chrome::ChromeBar      bar;

    MiniShell()
    {
        addAndMakeVisible (blister);
        blister.setMark (&mark);
        addAndMakeVisible (underline);   // top-most overlay

        chrome::Cell c;
        c.component  = &blister;
        c.region     = chrome::Cell::Region::RigidCenter;
        c.fixedWidth = blister.preferredWidth ((int) metrics.blisterHeight);
        c.height     = (int) metrics.blisterHeight;
        bar.add (c);
    }

    void resized() override
    {
        underline.setBounds (getLocalBounds());
        bar.layout (getLocalBounds().withHeight ((int) metrics.blisterHeight), (int) metrics.barHeight, 0);
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    MiniShell shell;
    shell.setSize (400, 60);

    // Headless smoke: the blister fill + the underline stroke must actually put paint down (no window).
    juce::Image img (juce::Image::ARGB, 400, 60, true);
    {
        juce::Graphics g (img);
        shell.blister.paint (g);
        shell.underline.paint (g);
    }
    int painted = 0;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt (x, y).getAlpha() > 0) ++painted;

    const bool good = painted > 0 && shell.blister.preferredWidth ((int) shell.metrics.blisterHeight) > 0;
    std::printf (good ? "MINIMAL CONSUMER OK\n" : "MINIMAL CONSUMER FAIL\n");
    return good ? 0 : 1;
}
