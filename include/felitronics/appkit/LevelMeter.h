// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::LevelMeter — a thin vertical peak meter (dBFS), moved verbatim from
// OrbitCab's ui/LevelMeter.h. Feed contract (RT-safe by construction): the PROCESSOR publishes a
// per-block linear peak through a std::atomic<float>; the EDITOR's GUI timer reads it and calls
// setLevel() on the message thread — this component itself never touches the audio thread.
// setLevel() applies the ballistics (instant attack, smooth release) + a short peak-hold; the
// constants are per-tick, tuned for a ~30 Hz timer. setRange() zooms the dBFS window (default
// −60..+6 wide bus meter). Colours follow the family brand: green / amber / clip-red (#ff6b6b).
// setGreenZone() overlays an adjustable target corridor and zone() reports where the held peak sits
// — for a "turn the input gain until green" calibrator (OrbitCapture NAM per-take level set);
// setClipCeiling() adds a yellow warn-band + red clip line above it. setRefLines() instead draws a
// FIXED dashed dBFS grid (the non-moving calibrator surface). The top cap doubles as a clickable clip
// lamp — setClipLatched() reds it, onClipClick fires on acknowledge.
// Header-only; the consumer supplies JUCE (juce_audio_basics + juce_gui_basics).
//==============================================================================

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace felitronics::appkit
{

class LevelMeter final : public juce::Component
{
public:
    LevelMeter() = default;

    // Zoom the dBFS window (floor .. ceiling). Default −60..+6 is a wide bus meter;
    // a hot internal tap (e.g. a preamp-OUT) reads better on a tighter range like −24..+6.
    void setRange (float floorDb, float ceilDb)
    {
        if (! std::isfinite (floorDb) || ! std::isfinite (ceilDb) || floorDb >= ceilDb)
            return;

        minDb_ = floorDb;
        maxDb_ = ceilDb;
        repaint();
    }

    // Called from the editor timer with the latest linear peak (0..~1+).
    void setLevel (float linearPeak)
    {
        linearPeak = juce::jmax (0.0f, linearPeak);

        if (linearPeak >= level) level = linearPeak;            // instant attack
        else                     level += (linearPeak - level) * kRelease;

        if (linearPeak >= peakHold) { peakHold = linearPeak; peakHoldTicks = 0; }
        else if (++peakHoldTicks > kPeakHoldTicks) peakHold *= kPeakDecay;

        repaint();
    }

    // Optional calibration corridor: the GREEN target band [loDb, hiDb]. Draws the band + edge lines
    // and colours the held-peak tick by where it sits (dim below, green inside). A non-finite range or
    // loDb >= hiDb CLEARS it — the meter reverts to the plain dBFS colour scale, so existing consumers
    // that never call this are unaffected. Pair with setClipCeiling() to add a YELLOW band + red line.
    void setGreenZone (float loDb, float hiDb)
    {
        if (std::isfinite (loDb) && std::isfinite (hiDb) && loDb < hiDb) { zoneLo_ = loDb; zoneHi_ = hiDb; }
        else { zoneLo_ = zoneHi_ = std::numeric_limits<float>::quiet_NaN(); }
        repaint();
    }

    // The "too hot" line (dBFS): green-band top → this reads YELLOW (hot but safe), and above it reads
    // clip-RED. A non-finite value CLEARS it — then everything above the green band is red (the old
    // two-zone behaviour), so consumers that only set a green zone are unaffected.
    void setClipCeiling (float ceilDb)
    {
        clipDb_ = ceilDb;
        repaint();
    }

    // The clip lamp lives in the meter's top cap: setClipLatched(true) reds it; a click anywhere on
    // the meter acknowledges it (fires onClipClick — the owner clears the latch). Purely a display +
    // hit target; the latch state itself is owned by the consumer.
    void setClipLatched (bool latched)
    {
        if (latched != clipLatched_) { clipLatched_ = latched; repaint(); }
    }
    std::function<void()> onClipClick;
    void mouseDown (const juce::MouseEvent&) override { if (onClipClick) onClipClick(); }

    // Fixed dBFS reference ticks (the calibration grid) — always drawn, never move.
    void setRefLines (std::vector<std::pair<float, juce::Colour>> lines) { refLines_ = std::move (lines); repaint(); }

    enum class Zone { none, below, inside, warn, above };

    // Where the held peak sits: below the floor / green band / yellow (over the band, under the clip
    // line) / red (over the clip line). `none` when no zone is set. Uses the peak-hold (the stable
    // read the user calibrates against), not the instant level.
    Zone zone() const
    {
        if (! hasZone()) return Zone::none;
        const float pdb = peakHold > 0.0f ? juce::Decibels::gainToDecibels (peakHold) : -120.0f;
        if (pdb < zoneLo_) return Zone::below;
        if (pdb <= zoneHi_) return Zone::inside;
        if (std::isfinite (clipDb_) && pdb > clipDb_) return Zone::above;   // over the clip line → red
        return std::isfinite (clipDb_) ? Zone::warn : Zone::above;          // no clip set ⇒ old red-above
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff141417));
        g.fillRoundedRectangle (r, 2.0f);

        auto dbToY = [&] (float db)
        {
            return juce::jmap (juce::jlimit (minDb_, maxDb_, db), minDb_, maxDb_, r.getBottom(), r.getY());
        };

        const float db = level > 0.0f ? juce::Decibels::gainToDecibels (level) : -120.0f;
        if (db > minDb_)
        {
            const float y = dbToY (db);
            juce::ColourGradient grad (juce::Colour (0xff7be29a), r.getCentreX(), r.getBottom(),
                                       juce::Colour (0xffff6b6b), r.getCentreX(), r.getY(), false);
            grad.addColour (0.74, juce::Colour (0xfff5c57a));   // amber band near the top
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r.withTop (y).reduced (1.0f, 0.0f), 1.5f);
        }

        // calibration corridor: translucent green band [lo, mid], a yellow band (mid, clip] when a
        // clip line is set, edge lines at the floor (green) and mid (amber), and a red clip line
        if (hasZone())
        {
            const float yLo = dbToY (zoneLo_);      // lower dB → nearer the bottom (larger y)
            const float yHi = dbToY (zoneHi_);      // green top / yellow start → smaller y
            g.setColour (juce::Colour (0x2f7be29a));
            g.fillRect (juce::Rectangle<float> (r.getX(), yHi, r.getWidth(), yLo - yHi));
            if (std::isfinite (clipDb_) && clipDb_ > zoneHi_)
            {
                const float yClip = dbToY (clipDb_);
                g.setColour (juce::Colour (0x2ff5c57a));                    // yellow band (mid, clip]
                g.fillRect (juce::Rectangle<float> (r.getX(), yClip, r.getWidth(), yHi - yClip));
                g.setColour (juce::Colour (0x99ff6b6b));                    // red clip line
                g.fillRect (r.getX(), yClip - 0.5f, r.getWidth(), 1.0f);
            }
            g.setColour (juce::Colour (0x887be29a));
            g.fillRect (r.getX(), yLo - 0.5f, r.getWidth(), 1.0f);          // green floor line
            g.setColour (juce::Colour (0x88f5c57a));
            g.fillRect (r.getX(), yHi - 0.5f, r.getWidth(), 1.0f);          // amber green→yellow line
        }

        // peak-hold tick — coloured by the zone verdict when a corridor is set, else the plain scale
        const float pdb = peakHold > 0.0f ? juce::Decibels::gainToDecibels (peakHold) : -120.0f;
        if (pdb > minDb_)
        {
            const float py = dbToY (pdb);
            g.setColour (hasZone() ? colourForZone (zone()) : colourForDb (pdb));
            g.fillRect (r.getX() + 1.0f, py - 1.0f, r.getWidth() - 2.0f, 2.0f);
        }

        // dBFS scale: ticks at 0/−6/−12/−24/−48 + labels when the meter is wide enough
        g.setFont (juce::FontOptions (8.0f));
        for (const int mark : { 0, -6, -12, -24, -48 })
        {
            if ((float) mark < minDb_ || (float) mark > maxDb_) continue;   // outside the zoom window
            const float y = dbToY ((float) mark);
            g.setColour (mark == 0 ? juce::Colour (0x44ffffff) : juce::Colour (0x1effffff));
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
            if (getWidth() >= 24)
            {
                g.setColour (juce::Colour (0x66b0b0b0));
                g.drawText (juce::String (mark), juce::Rectangle<float> (r.getX(), y - 5.0f, r.getWidth(), 10.0f),
                            juce::Justification::centred, false);
            }
        }

        // Fixed calibration reference ticks at their dBFS (always in the same place), DASHED.
        const float refDash[] = { 4.0f, 3.0f };
        for (const auto& [rdb, col] : refLines_)
        {
            const float y = dbToY (rdb);
            g.setColour (col.withAlpha (0.9f));
            g.drawDashedLine (juce::Line<float> (r.getX(), y, r.getRight(), y), refDash, 2, 1.2f);
        }

        // Clip cap: the top strip doubles as the (clickable) clip lamp — red when latched, else dim.
        g.setColour (clipLatched_ ? juce::Colour (0xffff5544) : juce::Colour (0xff3a3a42));
        g.fillRect (r.withHeight (5.0f).reduced (1.0f, 0.0f));
    }

private:
    static juce::Colour colourForDb (float db)
    {
        if (db >= -3.0f)  return juce::Colour (0xffff6b6b);   // clip-red
        if (db >= -12.0f) return juce::Colour (0xfff5c57a);   // amber
        return juce::Colour (0xff7be29a);                     // green
    }

    static juce::Colour colourForZone (Zone z)
    {
        if (z == Zone::inside) return juce::Colour (0xff7be29a);   // green — on target
        if (z == Zone::warn)   return juce::Colour (0xfff5c57a);   // amber — hot but safe
        if (z == Zone::above)  return juce::Colour (0xffff6b6b);   // clip-red — too hot
        return juce::Colour (0xff8a8f98);                          // dim grey — too low (or none)
    }

    bool hasZone() const
    {
        return std::isfinite (zoneLo_) && std::isfinite (zoneHi_) && zoneLo_ < zoneHi_;
    }

    static constexpr float kRelease       = 0.25f;   // per timer tick
    static constexpr float kPeakDecay     = 0.92f;
    static constexpr int   kPeakHoldTicks = 24;      // ~0.8 s at 30 Hz

    // dBFS window (per-instance so a hot tap can zoom in via setRange while the app's
    // shared IN/OUT meters keep the wide default).
    float minDb_ = -60.0f;
    float maxDb_ =   6.0f;

    float level         = 0.0f;
    float peakHold      = 0.0f;
    int   peakHoldTicks = 0;

    // Calibration corridor thresholds (dBFS). NaN = no zone → plain colour scale (default).
    // zoneLo_..zoneHi_ is the green band; clipDb_ (NaN = unset) is the yellow→red line above it.
    float zoneLo_ = std::numeric_limits<float>::quiet_NaN();
    float zoneHi_ = std::numeric_limits<float>::quiet_NaN();
    float clipDb_ = std::numeric_limits<float>::quiet_NaN();
    bool  clipLatched_ = false;                     // top-cap clip lamp (owner sets/clears it)
    std::vector<std::pair<float, juce::Colour>> refLines_;   // fixed calibration reference ticks

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

} // namespace felitronics::appkit
