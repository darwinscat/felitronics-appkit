// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <felitronics/appkit/CallOut.h>   // launchCallOut — editor-parented CallOutBox

#include <memory>
#include <utility>
#include <vector>

//==============================================================================
// felitronics::appkit::PerfBadge — a small clickable readout next to the version:
// "<latency> smp · <CPU>%". The editor pushes a Stats snapshot each timer tick (it owns the
// processor handle); the badge just displays. Click → a CallOutBox (same pattern as VersionBadge)
// with the per-stage DSP-load breakdown + Total, kept live by its own timer while open. Extracted
// verbatim from OrbitCab's ui/PerfBadge.h; the stage ROWS (the only product part) are data-driven
// via Config.
//
// The CPU figure is a wall-clock estimate (% of the block's real-time budget), smoothed in the
// engine — noisy and machine-dependent by nature, so it's shown as an approximate gauge.
//==============================================================================
namespace felitronics::appkit
{

class PerfBadge final : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    //==========================================================================
    // The product's per-stage rows, in display order — the only product-specific part. Stats::stages
    // is pushed PARALLEL to `rows` (same order, same count); the popup height follows the row count.
    struct StageRow
    {
        juce::String label;    // e.g. "Preamp" — fits the 60 px label column
        juce::Colour colour;   // the stage's bar fill (products use their accent/neutral palette)
    };

    struct Config
    {
        std::vector<StageRow> rows;   // REQUIRED (an empty vector legally shows just Latency + Total)

        // Generic wording by default; override if the product needs different copy.
        juce::String title   = "DSP load";
        juce::String tooltip = "DSP latency & load — click for the per-stage breakdown";

        // Visual default — OrbitCab's current pixels (popup title / row-label text).
        juce::Colour text { 0xffd8d8d8 };
    };

    struct Stats
    {
        int latencySamples = 0;
        float latencyMs = 0.0f, total = 0.0f;
        std::vector<float> stages;    // % per Config row, same order; missing entries draw as 0
    };

    explicit PerfBadge (Config cfg) : config (std::move (cfg))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (config.tooltip);
    }

    void setStats (const Stats& s) { stats = s; repaint(); }
    const Stats& getStats() const noexcept { return stats; }

    void paint (juce::Graphics& g) override
    {
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        const juce::uint32 col = stats.total < 50.0f ? 0xff70707a
                               : stats.total < 80.0f ? 0xffe0b020 : 0xffe06060;
        g.setColour (juce::Colour (col));
        g.drawText (juce::String (stats.latencySamples) + " smp  " + juce::String (stats.total, 1) + "%",
                    getLocalBounds().toFloat(), juce::Justification::centredRight, false);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()))
            showPopup();
    }

private:
    Config config;
    Stats  stats;

    struct Panel final : public juce::Component, private juce::Timer
    {
        explicit Panel (PerfBadge& b) : owner (&b), config (b.config)
        {
            // 20 px per stage row over the fixed chrome (title/latency/Total/margins) — OrbitCab's
            // five rows land on its original hardcoded 236×198.
            setSize (236, 98 + 20 * (int) config.rows.size());
            startTimerHz (20);
        }
        void timerCallback() override { repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().reduced (14, 12);
            const Stats s = owner != nullptr ? owner->getStats() : Stats {};

            g.setColour (config.text);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText (config.title, r.removeFromTop (18), juce::Justification::centredLeft, false);

            g.setFont (juce::FontOptions (11.0f));
            g.setColour (juce::Colour (0xff9a9aa4));
            g.drawText ("Latency " + juce::String (s.latencySamples) + " smp  \xc2\xb7  " + juce::String (s.latencyMs, 2) + " ms"
                        + (s.latencySamples > 0 ? "  (PDC)" : "  (zero)"),
                        r.removeFromTop (16), juce::Justification::centredLeft, false);
            r.removeFromTop (6);

            for (size_t i = 0; i < config.rows.size(); ++i)
                drawBar (g, r.removeFromTop (20), config.rows[i].label,
                         i < s.stages.size() ? s.stages[i] : 0.0f, config.rows[i].colour, false);
            r.removeFromTop (4);
            drawBar (g, r.removeFromTop (22), "Total", s.total, juce::Colour (0xffd0d0d8), true);
        }

        void drawBar (juce::Graphics& g, juce::Rectangle<int> rr, const juce::String& name,
                      float pct, juce::Colour col, bool bold) const
        {
            g.setFont (juce::FontOptions (11.0f, bold ? juce::Font::bold : juce::Font::plain));
            g.setColour (config.text);
            g.drawText (name, rr.removeFromLeft (60), juce::Justification::centredLeft, false);
            g.drawText (juce::String (pct, 1) + "%", rr.removeFromRight (44),
                        juce::Justification::centredRight, false);
            auto track = rr.reduced (4, 0).withSizeKeepingCentre (juce::jmax (1, rr.getWidth() - 8), 6).toFloat();
            g.setColour (juce::Colour (0x33ffffff));
            g.fillRoundedRectangle (track, 3.0f);
            g.setColour (col);
            g.fillRoundedRectangle (track.withWidth (juce::jlimit (0.0f, 1.0f, pct / 100.0f) * track.getWidth()), 3.0f);
        }

        juce::Component::SafePointer<PerfBadge> owner;
        const Config config;   // OWN copy — safe if the badge dies while the popup is open
    };

    void showPopup()
    {
        launchCallOut (*this, std::make_unique<Panel> (*this));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerfBadge)
};

} // namespace felitronics::appkit
