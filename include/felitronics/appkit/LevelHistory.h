// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::LevelHistory — a scrolling peak-level history strip (dBFS), generalized from
// OrbitCapture's per-mic MicHistory for the family's calibrators. Feed contract mirrors LevelMeter:
// the GUI timer calls push(linearPeak) once per tick on the message thread; every push scrolls one
// column, so the visible window is capacityTicks over the feed rate (default 600 ≈ 30 s at 20 Hz,
// 20 s at 30 Hz). setRange() zooms the dBFS window like LevelMeter.
//
// Two optional overlays tint the trace (default is a plain grey trace):
//   • setRefLines({(dB, colour)…}) — the FIXED calibration grid (the primary calibrator surface):
//     dashed coloured reference lines that never move, with the trace fill/stroke gradient-tinted to
//     match them (dark low → red high). Pair with setNoiseFloor(dB) for a black room-quiet line and
//     setCurrentDb(dB) for a live level readout in the bottom-right corner.
//   • setGreenZone(loDb, hiDb) — an alternate MOVING corridor: dashed green-floor / amber-ceiling
//     lines with the trace tinted grey→green→red around them; a non-finite or inverted pair CLEARS it.
// setClipCeiling(dB) marks a hard "too hot" line independent of the mode: every column that broke it
// gets a full-height red bar (an overload can't hide in the trace); in corridor mode it also opens a
// yellow band up to the line. peakDb() exposes the held peak (sticks ~1.5 s, then decays) for a
// readout or shared verdict. Header-only; the consumer supplies juce_audio_basics + juce_gui_basics.
//==============================================================================

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
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
        else if (++peakAge_ > kHoldTicks)                                            // …then walk down
        {
            peakAge_  = kHoldTicks + 1;                                              // cap: no unbounded growth over long silence
            peakHold_ = juce::jmax (db, peakHold_ - kDecayDbPerTick);
        }

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

    // The instant return level (dBFS) drawn as a big readout in the bottom-right corner (coloured by
    // loudness). Set from the GUI timer; a change under 0.05 dB is ignored to avoid churn repaints.
    void setCurrentDb (float db) { if (std::abs (db - curDb_) > 0.05f) { curDb_ = db; repaint(); } }

    // A fixed reference line (dBFS) — the noise-floor threshold to calibrate the room against with the
    // tone off. Drawn as a black line across the strip; a non-finite value hides it.
    void setNoiseFloor (float db) { noiseFloor_ = db; repaint(); }

    // Fixed dBFS reference lines (the calibration grid) — always drawn, never move: each (dB, colour)
    // is a dashed coloured line across the strip, and the trace is gradient-tinted to match. The entry
    // order does not matter (endpoints are chosen by dB); non-finite dB entries are dropped. Takes
    // paint precedence over the setGreenZone corridor; an empty list clears the grid.
    void setRefLines (std::vector<std::pair<float, juce::Colour>> lines)
    {
        refLines_.clear();
        for (auto& l : lines) if (std::isfinite (l.first)) refLines_.push_back (std::move (l));
        repaint();
    }

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
        // openSubpath=true: this path is fresh, so vertex 0 opens the subpath and the rest extend it.
        // false: the caller pre-seeded a corner (the fill), so EVERY vertex is a lineTo — opening a
        // new subpath here would orphan that corner. Note we must key off i==0, NOT p.isEmpty():
        // Path::isEmpty() skips lone move markers, so after startNewSubPath it stays true until the
        // first lineTo — using it would turn every vertex into a move and collapse the trace to
        // nothing (no line/fill drawn at all).
        auto trace = [&] (juce::Path& p, float clampY, bool openSubpath)   // oldest → newest, left → right
        {
            for (int i = 0; i < n; ++i)
            {
                const float x = b.getX() + (float) i / (float) (n - 1) * b.getWidth();
                const float y = juce::jmin (yOf (hist_[(size_t) ((head_ + i) % n)]), clampY);
                if (openSubpath && i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
        };
        const juce::Colour grey (0xff8a8f98), green (0xff33d13f), red (0xffe0402e);
        const float h = juce::jmax (1.0f, b.getBottom() - b.getY());
        auto fB = [&] (float y) { return juce::jlimit (0.001, 0.999, (double) ((b.getBottom() - y) / h)); };

        if (! refLines_.empty())
        {
            // FIXED calibration fill: one gradient under the trace (dark low → red high, through
            // green/yellow/orange), matching the reference lines. Endpoints are picked by dB — the
            // lowest-dB colour at the bottom, the highest-dB colour at the top — so the entry ORDER is
            // irrelevant (interior stops sit at each line's dB; JUCE sorts them). Never changes with mode.
            const auto ends = std::minmax_element (refLines_.begin(), refLines_.end(),
                                                   [] (const auto& lo, const auto& hi) { return lo.first < hi.first; });
            const juce::Colour bottomCol = ends.first->second;    // lowest dB  → strip bottom
            const juce::Colour topCol    = ends.second->second;   // highest dB → strip top

            juce::Path fill;
            fill.startNewSubPath (b.getX(), b.getBottom());
            trace (fill, b.getBottom(), false);
            fill.lineTo (b.getRight(), b.getBottom());
            fill.closeSubPath();
            juce::ColourGradient fg (bottomCol.withAlpha (0.10f), 0.0f, b.getBottom(),
                                     topCol.withAlpha (0.80f), 0.0f, b.getY(), false);
            for (const auto& [db, col] : refLines_) fg.addColour (fB (yOf (db)), col.withAlpha (0.42f));
            g.setGradientFill (fg);
            g.fillPath (fill);

            juce::Path line;
            trace (line, b.getBottom(), true);
            // Stroke in the SAME hues as the fill (dark low → red high) so the outline reads as the
            // edge of the coloured band, not a bright white-grey line on top of it.
            juce::ColourGradient lg (bottomCol, 0.0f, b.getBottom(), topCol, 0.0f, b.getY(), false);
            for (const auto& [db, col] : refLines_) lg.addColour (fB (yOf (db)), col);
            g.setGradientFill (lg);
            g.strokePath (line, juce::PathStrokeType (1.6f));
        }
        else if (hasZone())
        {
            const juce::Colour amber (0xfff5c57a);
            // zoneLo_..zoneHi_ is the GREEN band; clipCeiling_ (when above zoneHi_) opens a YELLOW band
            // up to it, red beyond. No clip / clip ≤ band-top ⇒ the old green→red-at-band-top strip.
            const bool  hasYellow = std::isfinite (clipCeiling_) && clipCeiling_ > zoneHi_;
            const float yLo   = yOf (zoneLo_);                            // floor → green dashed line
            const float yMid  = yOf (zoneHi_);                            // green→yellow → amber line
            const float yClip = hasYellow ? yOf (clipCeiling_) : yMid;    // yellow→red → red dashed line
            const float yGA   = yOf (zoneLo_ - 11.0f);                    // grey fade-in floor
            const float yG1   = yOf (juce::jmin (zoneLo_ + 3.0f, zoneHi_));   // full green just inside
            const float yYel  = hasYellow ? yOf ((zoneHi_ + clipCeiling_) * 0.5f) : yMid;   // mid-yellow
            const float yR1   = yOf ((hasYellow ? clipCeiling_ : zoneHi_) + 3.0f);          // full red

            // fill only where the trace rises above the fade-in floor
            juce::Path fill;
            fill.startNewSubPath (b.getX(), yGA);
            trace (fill, yGA, false);              // continue the pre-seeded corner
            fill.lineTo (b.getRight(), yGA);
            fill.closeSubPath();
            juce::ColourGradient fg (green.withAlpha (0.05f), 0.0f, b.getBottom(),
                                     red.withAlpha (0.82f), 0.0f, b.getY(), false);
            fg.addColour (fB (yGA),  green.withAlpha (0.06f));
            fg.addColour (fB (yLo),  green.withAlpha (0.30f));
            fg.addColour (fB (yMid), green.withAlpha (0.48f));
            if (hasYellow) {
                fg.addColour (fB (yYel),  amber.withAlpha (0.55f));
                fg.addColour (fB (yClip), amber.withAlpha (0.62f));
            }
            fg.addColour (fB (yR1),  red.withAlpha (0.80f));
            g.setGradientFill (fg);
            g.fillPath (fill);

            juce::Path line;
            trace (line, b.getBottom(), true);
            juce::ColourGradient lg (grey, 0.0f, b.getBottom(), red, 0.0f, b.getY(), false);
            lg.addColour (fB (yGA),  grey);
            lg.addColour (fB (yLo),  grey.interpolatedWith (green, 0.5f));
            lg.addColour (fB (yG1),  green);
            lg.addColour (fB (yMid), green);
            if (hasYellow) {
                lg.addColour (fB (yYel),  amber);
                lg.addColour (fB (yClip), amber);
            }
            lg.addColour (fB (yR1),  red);
            g.setGradientFill (lg);
            g.strokePath (line, juce::PathStrokeType (1.4f));

            const float dashes[] = { 5.0f, 4.0f };
            g.setColour (juce::Colours::limegreen.withAlpha (0.85f));
            g.drawDashedLine (juce::Line<float> (b.getX(), yLo, b.getRight(), yLo), dashes, 2, 1.2f);
            if (hasYellow) {
                g.setColour (amber.withAlpha (0.9f));
                g.drawDashedLine (juce::Line<float> (b.getX(), yMid, b.getRight(), yMid), dashes, 2, 1.2f);
            }
            g.setColour (juce::Colours::red.withAlpha (0.85f));
            g.drawDashedLine (juce::Line<float> (b.getX(), yClip, b.getRight(), yClip), dashes, 2, 1.2f);
        }
        else
        {
            juce::Path line;
            trace (line, b.getBottom(), true);
            g.setColour (grey);
            g.strokePath (line, juce::PathStrokeType (1.4f));
        }

        // Fixed calibration grid: DASHED coloured reference lines at their dBFS, always in the same place.
        const float refDash[] = { 5.0f, 4.0f };
        for (const auto& [db, col] : refLines_)
        {
            const float y = yOf (db);
            g.setColour (col.withAlpha (0.85f));
            g.drawDashedLine (juce::Line<float> (b.getX(), y, b.getRight(), y), refDash, 2, 1.2f);
        }

        // Noise-floor reference: a black line across the strip (with a faint light edge so it reads on
        // the dark background) — calibrate the room's quiet against it with the tone off.
        if (std::isfinite (noiseFloor_))
        {
            const float y = yOf (noiseFloor_);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRect (b.getX(), y - 1.5f, b.getWidth(), 3.0f);
            g.setColour (juce::Colours::black);
            g.fillRect (b.getX(), y - 0.75f, b.getWidth(), 1.5f);
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

        // Current return level — in the bottom-right corner, coloured by loudness (green → yellow →
        // orange → red as it climbs the -9/-6/-3 dBFS danger marks): the calibrator's live number.
        if (curDb_ > kSilenceDb + 0.5f && getWidth() >= 60)
        {
            juce::Colour c (0xff33d13f);                            // green — comfortably low
            if      (curDb_ >= -3.0f) c = juce::Colour (0xffe0402e);   // red
            else if (curDb_ >= -6.0f) c = juce::Colour (0xffff8a3d);   // orange
            else if (curDb_ >= -9.0f) c = juce::Colour (0xffffd54f);   // yellow
            g.setColour (c);
            g.setFont (juce::FontOptions ((float) juce::jlimit (18, 34, getHeight() / 6), juce::Font::bold));
            g.drawText (juce::String (curDb_, 1) + " dB",
                        getLocalBounds().reduced (12, 8), juce::Justification::bottomRight, false);
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
    float curDb_    = kSilenceDb;   // instant level for the big centred overlay
    float noiseFloor_ = std::numeric_limits<float>::quiet_NaN();   // fixed reference line; NaN = hidden
    std::vector<std::pair<float, juce::Colour>> refLines_;         // fixed calibration grid lines

    // Corridor thresholds (dBFS). NaN = no corridor → plain grey trace (default).
    float zoneLo_ = std::numeric_limits<float>::quiet_NaN();
    float zoneHi_ = std::numeric_limits<float>::quiet_NaN();

    // Hard overload ceiling (dBFS). NaN = off. Columns above it get a full-height red bar.
    float clipCeiling_ = std::numeric_limits<float>::quiet_NaN();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelHistory)
};

} // namespace felitronics::appkit
