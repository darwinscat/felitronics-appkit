// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::InputList — IN 1..IN n of an audio interface, each with its level meter, the ones heard lit;
// moved from the cab builders (0.5.0) for every standalone of the family, beside InputPick.h, which feeds it. A click
// is the app's to read (pick one, add a second for a sum, set a pair's side...), and a lit cell may carry the app's
// word ("L", "R"); the list stays open while the player plucks, so a string shows where it is plugged in. Eight inputs
// to a column, the device's name above. The look is the app's: colours, the font and how text is drawn
// (InputListStyle; the defaults are the family's). drawInputMeter is the meter alone, for a box elsewhere.
// Header-only; the consumer supplies JUCE (juce_gui_basics).
//==============================================================================

#include "InputPick.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace felitronics::appkit
{

/** How the list and its meters look; the app hands its own, the defaults are the family's dark panel. */
struct InputListStyle
{
    juce::Colour background { 0xff111119 };   // the list's panel
    juce::Colour heading    { 0xff6f6b86 };   // the device's name
    juce::Colour text       { 0xffa6a2bd };   // an input's number
    juce::Colour lit        { 0xffff8a3d };   // ...heard: its number, its cell's edge
    juce::Colour litFill    { 0xff15151f };   // ...its cell
    juce::Colour meterTrack { 0xff050509 };
    juce::Colour meterFill  { 0xff9778ff };
    juce::Colour meterHot   { 0xffff8a3d };   // from hotDb up, where a hotter string would clip
    float floorDb = -60.0f, hotDb = -3.0f;    // the meter's empty end, and its hot one
    float textHeight = 13.0f;

    std::function<juce::Font (float height)> font = [] (float height) { return juce::Font (juce::FontOptions (height)); };
    std::function<void (juce::Graphics&, const juce::String&, juce::Rectangle<float>, const juce::Font&, juce::Justification)> drawText =
        [] (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, const juce::Font& f, juce::Justification j)
        {
            g.setFont (f);
            g.drawText (t, r, j, false);
        };
    std::function<float (const juce::String&, const juce::Font&)> textWidth = [] (const juce::String& t, const juce::Font& f)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (f, t, 0.0f, 0.0f);
        return glyphs.getBoundingBox (0, -1, true).getWidth();
    };
};

/** An input's meter: a bar from floorDb to full scale, meterFill, meterHot from hotDb. `level` is linear, 1 full scale. */
inline void drawInputMeter (juce::Graphics& g, juce::Rectangle<float> r, float level, const InputListStyle& style = {})
{
    const float radius = r.getHeight() * 0.5f;
    g.setColour (style.meterTrack);
    g.fillRoundedRectangle (r, radius);
    const float db = level > 0.0f ? 20.0f * std::log10 (level) : -200.0f;
    const float share = juce::jlimit (0.0f, 1.0f, (db - style.floorDb) / (0.0f - style.floorDb));
    if (share <= 0.0f)
        return;
    g.setColour (db >= style.hotDb ? style.meterHot : style.meterFill);
    g.fillRoundedRectangle (r.withWidth (std::max (r.getHeight(), r.getWidth() * share)), radius);
}

/** A device's open inputs (AudioIODevice::getActiveInputChannels) as InputPick::prepare and the fallbacks take them,
    input k's bit set, up to InputChoice::maxInputs (BigInteger::toInt64 would drop the last). */
inline std::uint64_t openInputBits (const juce::BigInteger& active)
{
    std::uint64_t bits = 0;
    for (int ch = 0; ch < InputChoice::maxInputs; ++ch)
        if (active[ch])
            bits |= std::uint64_t (1) << ch;
    return bits;
}

class InputList final : public juce::Component,
                        private juce::Timer
{
public:
    static constexpr int rowH = 28, colW = 156, top = 38, pad = 10, perColumn = 8;

    /** What the list shows and does, asked as it paints (30 times a second) and on a click. */
    struct Feed
    {
        std::function<bool (int input)> lit;                                   // heard (IN 1 is 0)
        std::function<juce::String (int input)> tag;                           // a word in its cell, "L" say; may be empty
        std::function<float (int input)> level;                                // its meter, as drawn (held and falling)
        std::function<void (int input, const juce::ModifierKeys&)> picked;     // a click on it
    };

    InputList (juce::String deviceName, int inputs, InputListStyle styleToUse, Feed feedToUse)
        : device (std::move (deviceName)), count (std::clamp (inputs, 1, InputChoice::maxInputs)),   // as many as a choice can name
          style (std::move (styleToUse)), feed (std::move (feedToUse))
    {
        columns = (count + perColumn - 1) / perColumn;
        rows = (count + columns - 1) / columns;
        const int named = juce::roundToInt (style.textWidth (device.toUpperCase(), style.font (style.textHeight))) + (pad + 8) * 2;
        setSize (std::max (pad * 2 + columns * colW, named), top + rows * rowH + pad);   // the device's name whole above
        left = (getWidth() - columns * colW) / 2;
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (style.background);
        const auto font = style.font (style.textHeight);
        g.setColour (style.heading);
        style.drawText (g, device.toUpperCase(), getLocalBounds().removeFromTop (top).reduced (pad + 8, 0).toFloat(), font,
                        juce::Justification::centredLeft);
        for (int i = 0; i < count; ++i)
        {
            const auto cell = cellArea (i).toFloat();
            const bool on = feed.lit != nullptr && feed.lit (i);
            if (on)
            {
                g.setColour (style.litFill);
                g.fillRoundedRectangle (cell.reduced (2.0f), 4.0f);
                g.setColour (style.lit);
                g.drawRoundedRectangle (cell.reduced (2.0f), 4.0f, 1.0f);
            }
            g.setColour (on ? style.lit : style.text);
            style.drawText (g, "IN " + juce::String (i + 1), cell.withWidth (54.0f).withTrimmedLeft (10.0f), font,
                            juce::Justification::centredLeft);
            const auto word = feed.tag != nullptr ? feed.tag (i) : juce::String();
            auto meter = cell.withTrimmedLeft (58.0f).withTrimmedRight (12.0f);
            if (word.isNotEmpty())
            {
                style.drawText (g, word, meter.removeFromRight (18.0f), font, juce::Justification::centredRight);
                meter.removeFromRight (6.0f);
            }
            drawInputMeter (g, meter.withSizeKeepingCentre (meter.getWidth(), 6.0f), feed.level != nullptr ? feed.level (i) : 0.0f, style);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < count; ++i)
            if (cellArea (i).contains (e.getPosition()))
            {
                // The app may close the list from its click: the call made on a copy, nothing touched after if it did.
                juce::Component::SafePointer<InputList> self (this);
                if (const auto picked = feed.picked)
                    picked (i, e.mods);
                if (self != nullptr)
                    repaint();
                return;
            }
    }

private:
    juce::String device;
    int count, columns = 1, rows = 1, left = pad;
    InputListStyle style;
    Feed feed;

    juce::Rectangle<int> cellArea (int i) const   // down a column, then the next
    {
        return { left + (i / rows) * colW, top + (i % rows) * rowH, colW, rowH };
    }

    void timerCallback() override { repaint(); }
};

} // namespace felitronics::appkit
