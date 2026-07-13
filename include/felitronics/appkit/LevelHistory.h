// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::LevelHistory — a scrolling peak-level history strip (dBFS), generalized from
// OrbitCapture's per-mic MicHistory (which hard-coded the −21/−9 mic thresholds) for the family's
// calibrators. Feed contract mirrors LevelMeter: the GUI timer calls push(linearPeak) once per tick
// on the message thread; every push scrolls one column, so the visible window is capacityTicks over
// the feed rate (default 600 ≈ 30 s at 20 Hz, 20 s at 30 Hz). setGreenZone(loDb, hiDb) draws the
// calibration corridor as two dashed lines (green floor / red ceiling) and tints the trace/fill by
// where it sits — grey in the noise, green inside, red above; a non-finite or inverted pair CLEARS
// the corridor and the strip falls back to a plain grey trace. peakDb() exposes the held peak
// (sticks ~1.5 s, then decays) for readouts and a shared calibration verdict; setRange() zooms the
// dBFS window like LevelMeter. Header-only; the consumer supplies juce_audio_basics + juce_gui_basics.
//==============================================================================

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <limits>
#include <vector>

namespace felitronics::appkit
{

class LevelHistory final : public juce::Component
{
public:
    explicit LevelHistory (int capacityTicks = 600)
        : hist_ ((size_t) juce::jmax (2, capacityTicks), kSilenceDb) {}

    // Zoom the dBFS window (floor .. ceiling). Default −60..0 — the MicHistory window.
    void setRange (float floorDb, float ceilDb)
    {
        if (! std::isfinite (floorDb) || ! std::isfinite (ceilDb) || floorDb >= ceilDb)
            return;

        minDb_ = floorDb;
        maxDb_ = ceilDb;
        repaint();
    }

    // The calibration corridor: dashed green floor + dashed red ceiling, trace/fill tinted around
    // them. An invalid pair (non-finite or loDb >= hiDb) clears the corridor — plain grey trace.
    void setGreenZone (float loDb, float hiDb)
    {
        if (std::isfinite (loDb) && std::isfinite (hiDb) && loDb < hiDb) { zoneLo_ = loDb; zoneHi_ = hiDb; }
        else { zoneLo_ = zoneHi_ = std::numeric_limits<float>::quiet_NaN(); }
        repaint();
    }

    // A hard "too hot" ceiling, independent of the displayed corridor: any history column whose peak
    // exceeded ceilDb is painted a FULL-HEIGHT red bar (an overload can't hide down in the trace). A
    // non-finite value clears it. Distinct from the green zone's red ceiling line, which only tints.
    void setClipCeiling (float ceilDb)
    {
        clipCeiling_ = ceilDb;
        repaint();
    }

    // One GUI tick: append the latest linear peak (0..~1+) and advance the strip one column.
    void push (float linearPeak)
    {
        const float db = juce::Decibels::gainToDecibels (juce::jmax (0.0f, linearPeak), kSilenceDb);
        hist_[(size_t) head_] = db;
        head_ = (head_ + 1) % (int) hist_.size();

        if (db >= peakHold_) { peakHold_ = db; peakAge_ = 0; }                       // stick…
        else if (++peakAge_ > kHoldTicks) peakHold_ = juce::jmax (db, peakHold_ - kDecayDbPerTick);   // …then walk down

        repaint();
    }

    void clear()
    {
        std::fill (hist_.begin(), hist_.end(), kSilenceDb);
        head_ = 0;
        peakHold_ = kSilenceDb;
        peakAge_ = 0;
        repaint();
    }

    // The held peak (dBFS) — the stable read for a readout or a shared calibration verdict.
    float peakDb() const { return peakHold_; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff141417));
        g.fillRoundedRectangle (b, 2.0f);
        b = b.reduced (1.0f);

        const int n = (int) hist_.size();
        auto yOf = [&] (float db)
        {
            return juce::jmap (juce::jlimit (minDb_, maxDb_, db), minDb_, maxDb_, b.getBottom(), b.getY());
        };
        auto trace = [&] (juce::Path& p, float clampY)   // oldest → newest, left → right
        {
            for (int i = 0; i < n; ++i)
            {
                const float x = b.getX() + (float) i / (float) (n - 1) * b.getWidth();
                const float y = juce::jmin (yOf (hist_[(size_t) ((head_ + i) % n)]), clampY);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
        };
        const juce::Colour grey (0xff8a8f98), green (0xff33d13f), red (0xffe0402e);

        if (hasZone())
        {
            const float yGreen = yOf (zoneLo_), yRed = yOf (zoneHi_);
            // colour anchors ride the corridor: fade in from lo−11, full green by lo+3,
            // hold to hi−3, full red by hi+3 (MicHistory's −32/−18/−12/−6 around −21/−9)
            const float yGA = yOf (zoneLo_ - 11.0f), yG1 = yOf (zoneLo_ + 3.0f);
            const float yR0 = yOf (zoneHi_ - 3.0f),  yR1 = yOf (zoneHi_ + 3.0f);
            const float h = juce::jmax (1.0f, b.getBottom() - b.getY());
            auto fB = [&] (float y) { return juce::jlimit (0.001, 0.999, (double) ((b.getBottom() - y) / h)); };

            // fill only where the trace rises above the fade-in floor
            juce::Path fill;
            fill.startNewSubPath (b.getX(), yGA);
            trace (fill, yGA);
            fill.lineTo (b.getRight(), yGA);
            fill.closeSubPath();
            juce::ColourGradient fg (green.withAlpha (0.05f), 0.0f, b.getBottom(),
                                     red.withAlpha (0.82f), 0.0f, b.getY(), false);
            fg.addColour (fB (yGA), green.withAlpha (0.06f));
            fg.addColour (fB (yGreen), green.withAlpha (0.28f));
            fg.addColour (fB (yG1), green.withAlpha (0.50f));
            fg.addColour (fB (yR0), green.withAlpha (0.50f));
            fg.addColour (fB (yR1), red.withAlpha (0.80f));
            g.setGradientFill (fg);
            g.fillPath (fill);

            juce::Path line;
            trace (line, b.getBottom());
            juce::ColourGradient lg (grey, 0.0f, b.getBottom(), red, 0.0f, b.getY(), false);
            lg.addColour (fB (yGA), grey);
            lg.addColour (fB (yGreen), grey.interpolatedWith (green, 0.5f));
            lg.addColour (fB (yG1), green);
            lg.addColour (fB (yR0), green);
            lg.addColour (fB (yR1), red);
            g.setGradientFill (lg);
            g.strokePath (line, juce::PathStrokeType (1.4f));

            const float dashes[] = { 5.0f, 4.0f };
            g.setColour (juce::Colours::limegreen.withAlpha (0.85f));
            g.drawDashedLine (juce::Line<float> (b.getX(), yGreen, b.getRight(), yGreen), dashes, 2, 1.2f);
            g.setColour (juce::Colours::red.withAlpha (0.85f));
            g.drawDashedLine (juce::Line<float> (b.getX(), yRed, b.getRight(), yRed), dashes, 2, 1.2f);
        }
        else
        {
            juce::Path line;
            trace (line, b.getBottom());
            g.setColour (grey);
            g.strokePath (line, juce::PathStrokeType (1.4f));
        }

        // Hard-ceiling overloads: full-height red bars for every column that broke the ceiling — an
        // overload spike is visible even when it lasted a single tick. Painted OVER the trace.
        if (std::isfinite (clipCeiling_))
        {
            g.setColour (juce::Colour (0xffe0402e).withAlpha (0.6f));
            for (int i = 0; i < n; ++i)
                if (hist_[(size_t) ((head_ + i) % n)] > clipCeiling_)
                {
                    const float x = b.getX() + (float) i / (float) (n - 1) * b.getWidth();
                    g.fillRect (x - 0.5f, b.getY(), 1.5f, b.getHeight());
                }
        }

        if (peakHold_ > kSilenceDb + 0.5f && getWidth() >= 60)   // held-peak readout, top-right
        {
            g.setColour (hasZone() && peakHold_ > zoneHi_ ? red : juce::Colours::white);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (juce::String (peakHold_, 1) + " dB", getLocalBounds().reduced (4, 2),
                        juce::Justification::topRight, false);
        }
    }

private:
    bool hasZone() const
    {
        return std::isfinite (zoneLo_) && std::isfinite (zoneHi_) && zoneLo_ < zoneHi_;
    }

    static constexpr float kSilenceDb      = -120.0f;
    static constexpr int   kHoldTicks      = 30;      // ~1.5 s at a 20 Hz feed
    static constexpr float kDecayDbPerTick = 0.8f;

    // dBFS window (per-instance, like LevelMeter's setRange)
    float minDb_ = -60.0f;
    float maxDb_ =   0.0f;

    std::vector<float> hist_;
    int   head_     = 0;
    int   peakAge_  = 0;
    float peakHold_ = kSilenceDb;

    // Corridor thresholds (dBFS). NaN = no corridor → plain grey trace (default).
    float zoneLo_ = std::numeric_limits<float>::quiet_NaN();
    float zoneHi_ = std::numeric_limits<float>::quiet_NaN();

    // Hard overload ceiling (dBFS). NaN = off. Columns above it get a full-height red bar.
    float clipCeiling_ = std::numeric_limits<float>::quiet_NaN();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelHistory)
};

} // namespace felitronics::appkit
