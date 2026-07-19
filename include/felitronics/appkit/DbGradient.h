// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::dbGradient — a vertical juce::ColourGradient keyed by dBFS. The lowest-dB stop's
// colour sits at yBottom, the highest at yTop, and each stop is placed at (db - minDb)/(maxDb - minDb)
// (clamped to the interior). A LevelMeter bar and a LevelHistory fill/stroke share this so they read
// IDENTICAL colours at identical dB. The three alphas let the history fill fade translucent (bottom/top
// endpoints + interior stops) while the stroke and the meter bar stay opaque (all alphas = 1).
//==============================================================================

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace felitronics::appkit
{

inline juce::ColourGradient dbGradient (const std::vector<std::pair<float, juce::Colour>>& stops,
                                        float minDb, float maxDb, float x, float yTop, float yBottom,
                                        float bottomAlpha = 1.0f, float topAlpha = 1.0f, float stopAlpha = 1.0f)
{
    if (stops.empty())
        return juce::ColourGradient (juce::Colours::transparentBlack, x, yBottom,
                                     juce::Colours::transparentBlack, x, yTop, false);

    const auto ends = std::minmax_element (stops.begin(), stops.end(),
                                           [] (const auto& a, const auto& b) { return a.first < b.first; });
    juce::ColourGradient g (ends.first->second.withAlpha (bottomAlpha), x, yBottom,   // lowest dB → bottom
                            ends.second->second.withAlpha (topAlpha),  x, yTop, false);   // highest → top
    const float span = juce::jmax (0.001f, maxDb - minDb);
    for (const auto& s : stops)
        g.addColour (juce::jlimit (0.001, 0.999, (double) ((s.first - minDb) / span)), s.second.withAlpha (stopAlpha));
    return g;
}

} // namespace felitronics::appkit
