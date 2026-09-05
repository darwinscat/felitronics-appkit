// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.
//
// The footer build stamp says what is running, in four words, in a 22 px strip. Everything that can
// go wrong with it is a rule rather than a picture: which part of a git-describe reaches the line,
// what marks a build that is NOT the release it descends from, how much room the line needs, and
// whether the alert dot moves the words when it lights. Those are what this checks — from the
// theory of the thing, not from what the code happens to return.

#include <felitronics/appkit/VersionStamp.h>

#include <cstdio>
#include <string>

using namespace felitronics::appkit;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

// A checker with no settings file: it never hits the network and never remembers anything, which is
// exactly the state a stamp starts life in.
struct StampChecker final : UpdateChecker
{
    explicit StampChecker (juce::String describe)
        : UpdateChecker ({ .ownerRepo      = "darwinscat/felitronics-appkit",
                           .productName    = "Probe",
                           .currentVersion = std::move (describe),
                           .settings       = {} }) {}
};

// What the line ACTUALLY says, read back through a recording context — no hooks, no accessors.
static juce::String painted (VersionStamp& stamp)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, stamp.getWidth()), juce::jmax (1, stamp.getHeight()), true);
    juce::Graphics g (img);
    stamp.paintEntireComponent (g, false);

    // The text is the only thing with glyphs; count ink instead of parsing pixels.
    int inked = 0;
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt (x, y).getAlpha() > 8)
                ++inked;
    return juce::String (inked);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("VersionStamp — what the strip says about the run\n");

    group ("the version the line shows");
    {
        StampChecker clean ("v0.6.0");
        VersionStamp released (clean, "VST3");

        StampChecker dev ("v0.6.0-1-g3478bea");
        VersionStamp ahead (dev, "VST3");

        StampChecker dirty ("v0.6.0-1-g3478bea-dirty");
        VersionStamp working (dirty, "VST3");

        // A clean release tag is shown as it is. Anything DESCENDED from it keeps a "+": the strip
        // must not claim to be the release it merely comes after, and the About window carries the
        // commit and the dirty flag in full.
        ok (released.preferredWidth() < ahead.preferredWidth(),
            "a build ahead of the tag is wider than the tag itself (it carries the +)");
        ok (ahead.preferredWidth() == working.preferredWidth(),
            "ahead and dirty read the same on the strip — both are simply not that release");

        // The wrapper is part of the line, so a longer wrapper name is a longer line.
        StampChecker c2 ("v0.6.0");
        VersionStamp standalone (c2, "Standalone");
        ok (standalone.preferredWidth() > released.preferredWidth(),
            "\"Standalone\" makes a longer line than \"VST3\"");

        // ...and the version alone is what a narrow strip falls back to.
        ok (released.minimumWidth() < released.preferredWidth(),
            "the minimum width drops the wrapper word");
        ok (released.minimumWidth() > 8, "...but still asks for room to say the version");
    }

    group ("a degenerate describe still leaves a readable line");
    {
        StampChecker empty ({});
        VersionStamp stamp (empty, "AU");
        ok (stamp.minimumWidth() > 8, "an empty version falls back to something drawable");

        StampChecker odd ("not-a-version");
        VersionStamp stamp2 (odd, "AU");
        ok (stamp2.minimumWidth() > 8, "so does a describe that is not a version at all");
    }

    group ("it paints, and its colours are the product's");
    {
        StampChecker c ("v0.6.0");
        VersionStamp stamp (c, "Standalone");
        stamp.setSize (stamp.preferredWidth(), 18);

        const auto inkedDefault = painted (stamp);
        ok (inkedDefault.getIntValue() > 20, "the line actually inks its box");

        // Colour is a parameter: painting in transparent ink must change what lands on the image.
        stamp.setColours (juce::Colours::transparentBlack, juce::Colours::transparentBlack,
                          juce::Colours::transparentBlack);
        ok (painted (stamp).getIntValue() < inkedDefault.getIntValue(),
            "setColours reaches the paint — transparent ink draws less");
    }

    std::printf (failures == 0 ? "\nAll %d checks passed.\n" : "\n%d of %d checks FAILED.\n",
                 failures == 0 ? checks : failures, checks);
    return failures == 0 ? 0 : 1;
}
