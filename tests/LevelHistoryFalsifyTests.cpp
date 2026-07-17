// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// Falsification suite for LevelHistory.h (the scrolling peak-history strip).
//
// Like the LevelMeter suites we test the MATH, not pixels. The observable surfaces:
//   • peakDb() — the documented hold ballistics read directly: sticks for exactly kHoldTicks (30)
//     quiet ticks, then walks down kDecayDbPerTick (0.8 dB) per tick, floored at the current input.
//   • the TRACE — with no overlay set, paint() strokes exactly ONE path (the trace); a recording
//     LowLevelGraphicsContext captures its geometry, which must sit on the documented affine dB→y
//     map (and move when setRange re-maps it).
//   • the CORRIDOR — a valid setGreenZone adds the gradient fill + dashed threshold fills on top of
//     the trace; an invalid pair must CLEAR it back to the single-path strip.
//   • the REF-LINE GRID — setRefLines draws a dashed segment at each reference dB (its documented
//     y-map); an empty grid clears them back to the bare trace.
//   • the NOISE FLOOR — setNoiseFloor paints a full-width line (a fillRect) at its dBFS; a
//     non-finite value hides it. (setCurrentDb draws a text-only corner readout, width-gated at ≥ 60
//     px — not geometry the path/rect recorder can see, so it is out of scope here.)
//
// Registers in the FELITRONICS_APPKIT_TESTS_WITH_JUCE tier like the other paint-reading gates.

#include <felitronics/appkit/LevelHistory.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace felitronics::appkit;

static int checks = 0, failures = 0;
static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}
static void group (const char* name) { std::printf ("  - %s\n", name); }

// Strip geometry: narrow enough that the peak readout (needs width ≥ 60) never draws — the trace
// and corridor fills are the only paths that reach the context.
static constexpr int kW = 48;
static constexpr int kH = 220;

//==============================================================================
// Recording context: collects the bounds of every fillPath (stroked paths arrive here too).
class RecordingContext final : public juce::LowLevelGraphicsContext
{
public:
    std::vector<juce::Rectangle<float>> paths;   // fillPath + drawLine geometry (trace, dashes)
    std::vector<juce::Rectangle<float>> rects;    // fillRect geometry (noise-floor line, clip bars)

    bool isVectorDevice() const override { return true; }
    void setOrigin (juce::Point<int>) override {}
    void addTransform (const juce::AffineTransform&) override {}
    float getPhysicalPixelScaleFactor() const override { return 1.0f; }
    bool clipToRectangle (const juce::Rectangle<int>&) override { return true; }
    bool clipToRectangleList (const juce::RectangleList<int>&) override { return true; }
    void excludeClipRectangle (const juce::Rectangle<int>&) override {}
    void clipToPath (const juce::Path&, const juce::AffineTransform&) override {}
    void clipToImageAlpha (const juce::Image&, const juce::AffineTransform&) override {}
    bool clipRegionIntersects (const juce::Rectangle<int>&) override { return true; }
    juce::Rectangle<int> getClipBounds() const override { return { 0, 0, 1 << 20, 1 << 20 }; }
    bool isClipEmpty() const override { return false; }
    void saveState() override {}
    void restoreState() override {}
    void beginTransparencyLayer (float) override {}
    void endTransparencyLayer() override {}
    void setFill (const juce::FillType&) override {}
    void setOpacity (float) override {}
    void setInterpolationQuality (juce::Graphics::ResamplingQuality) override {}
    void fillRect (const juce::Rectangle<int>&, bool) override {}
    void fillRect (const juce::Rectangle<float>& r) override { rects.push_back (r); }
    void fillRectList (const juce::RectangleList<float>&) override {}
    void fillPath (const juce::Path& p, const juce::AffineTransform& t) override
    {
        paths.push_back (p.getBoundsTransformed (t));
    }
    void fillRoundedRectangle (const juce::Rectangle<float>&, float) override {}   // the background
    void drawImage (const juce::Image&, const juce::AffineTransform&) override {}
    void drawLine (const juce::Line<float>& l) override
    {
        paths.push_back (juce::Rectangle<float>::leftTopRightBottom (l.getStartX(), l.getStartY(),
                                                                     l.getEndX(), l.getEndY()));
    }
    void setFont (const juce::Font& f) override { font_ = f; }
    const juce::Font& getFont() override { return font_; }
    void drawGlyphs (juce::Span<const uint16_t>, juce::Span<const juce::Point<float>>,
                     const juce::AffineTransform&) override {}
    std::unique_ptr<juce::ImageType> getPreferredImageTypeForTemporaryImages() const override
    {
        return std::make_unique<juce::SoftwareImageType>();
    }
    uint64_t getFrameId() const override { return 0; }

private:
    juce::Font font_ { juce::FontOptions {} };
};

static std::vector<juce::Rectangle<float>> render (LevelHistory& h)
{
    RecordingContext ctx;
    juce::Graphics g (ctx);
    h.paint (g);
    return ctx.paths;
}

// The fillRect surface (noise-floor line, clip bars) — kept separate from the path recorder so the
// trace/corridor path-count assertions stay unaffected.
static std::vector<juce::Rectangle<float>> renderRects (LevelHistory& h)
{
    RecordingContext ctx;
    juce::Graphics g (ctx);
    h.paint (g);
    return ctx.rects;
}

//==============================================================================
// The documented dB→y map: paint() insets the background by 1 px, then maps [minDb..maxDb] onto
// [bottom..top] affinely.
static float dbToY (float db, float minDb, float maxDb)
{
    const float top = 1.0f, bottom = (float) kH - 1.0f;
    return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, bottom, top);
}

static float gain (float db) { return juce::Decibels::decibelsToGain (db); }
static bool  near (float a, float b, float eps) { return std::abs (a - b) <= eps; }

static constexpr float kStrokeEps = 1.0f;   // half the 1.4 px stroke + margin

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit LevelHistory falsify tests\n");

    // =============================================================================================
    group ("peak-hold ballistics through peakDb(): stick 30 ticks, then 0.8 dB per tick");
    {
        LevelHistory h (16);
        ok (h.peakDb() <= -119.0f, "a fresh strip reads silence");

        h.push (gain (-6.0f));
        ok (near (h.peakDb(), -6.0f, 0.01f), "one push lands the held peak exactly at its dB");

        bool held = true;
        for (int t = 1; t <= 30; ++t) { h.push (0.0f); held = held && near (h.peakDb(), -6.0f, 0.01f); }
        ok (held, "the held peak is stationary for exactly 30 quiet ticks");

        h.push (0.0f);
        ok (near (h.peakDb(), -6.8f, 0.01f), "tick 31 is the first decay step (0.8 dB)");
        h.push (0.0f);
        ok (near (h.peakDb(), -7.6f, 0.01f), "decay keeps walking 0.8 dB per tick");

        h.push (gain (-30.0f));
        bool floored = false;
        for (int t = 0; t < 200 && ! floored; ++t) { h.push (gain (-30.0f)); floored = near (h.peakDb(), -30.0f, 0.01f); }
        ok (floored, "decay floors at the current input, never below");

        h.push (gain (-3.0f));
        ok (near (h.peakDb(), -3.0f, 0.01f), "a louder push retakes the hold instantly");

        h.clear();
        ok (h.peakDb() <= -119.0f, "clear() resets the held peak to silence");
    }

    // =============================================================================================
    group ("trace geometry rides the affine dB→y map (no corridor: exactly one path)");
    {
        LevelHistory h (16);
        h.setSize (kW, kH);
        for (int i = 0; i < 16; ++i) h.push (gain (-18.0f));   // a flat trace at −18

        const auto flat = render (h);
        ok (flat.size() == 1, "corridor unset: the trace is the only path painted");
        if (flat.size() == 1)
            ok (near (flat[0].getCentreY(), dbToY (-18.0f, -60.0f, 0.0f), kStrokeEps),
                "flat −18 dB trace strokes exactly at the default map's −18 line");

        h.setRange (-36.0f, -6.0f);                            // zoom: same data, new map
        const auto zoomed = render (h);
        ok (zoomed.size() == 1
                && near (zoomed[0].getCentreY(), dbToY (-18.0f, -36.0f, -6.0f), kStrokeEps),
            "setRange only re-maps: the same trace lands on the zoomed −18 line");
    }

    // =============================================================================================
    group ("the strip scrolls: a spike falls off after capacity pushes");
    {
        LevelHistory h (16);
        h.setSize (kW, kH);
        h.push (gain (-6.0f));                                  // the spike
        for (int i = 0; i < 15; ++i) h.push (0.0f);
        const auto with = render (h);
        ok (with.size() == 1 && near (with[0].getY(), dbToY (-6.0f, -60.0f, 0.0f), kStrokeEps),
            "while in the window, the spike is the trace's top");

        h.push (0.0f);                                          // 16th quiet push evicts the spike
        const auto without = render (h);
        ok (without.size() == 1
                && without[0].getY() > dbToY (-60.0f, -60.0f, 0.0f) - 2.0f * kStrokeEps,
            "one more push scrolls the spike out — the trace flattens to the floor");
    }

    // =============================================================================================
    group ("corridor: a valid zone adds fill + dashed thresholds; an invalid pair clears it");
    {
        LevelHistory h (16);
        h.setSize (kW, kH);
        for (int i = 0; i < 16; ++i) h.push (gain (-7.5f));     // mid-corridor trace

        h.setGreenZone (-9.0f, -6.0f);
        const auto zoned = render (h);
        ok (zoned.size() >= 4, "zone set: gradient fill + trace + dashed thresholds all paint");
        // the dashed thresholds paint as dash segments ON the two corridor lines
        const float yLo = dbToY (-9.0f, -60.0f, 0.0f), yHi = dbToY (-6.0f, -60.0f, 0.0f);
        bool loDash = false, hiDash = false;
        for (const auto& r : zoned)
        {
            if (r.getWidth() < (float) kW / 2.0f)   // a dash, not the trace/fill
            {
                loDash = loDash || near (r.getCentreY(), yLo, 1.5f);
                hiDash = hiDash || near (r.getCentreY(), yHi, 1.5f);
            }
        }
        ok (loDash && hiDash, "dash segments sit on both corridor thresholds");

        h.setGreenZone (-6.0f, -9.0f);                          // inverted ⇒ clears
        const auto cleared = render (h);
        ok (cleared.size() == 1, "an inverted pair clears the corridor back to the bare trace");

        h.setGreenZone (std::numeric_limits<float>::quiet_NaN(), -6.0f);
        ok (render (h).size() == 1, "a NaN threshold also clears the corridor");
    }

    // =============================================================================================
    group ("clip ceiling: over-ceiling columns paint full-height red bars (fillRect, not the trace)");
    {
        LevelHistory h (16);
        h.setSize (kW, kH);
        // a full-height bar: narrow (≪ strip width) and ≈ the whole strip height
        auto bars = [&] (LevelHistory& lh)
        {
            std::vector<juce::Rectangle<float>> out;
            for (const auto& r : renderRects (lh))
                if (r.getWidth() < (float) kW / 2.0f && r.getHeight() > (float) kH * 0.9f) out.push_back (r);
            return out;
        };
        for (int i = 0; i < 16; ++i) h.push (gain (-7.5f));    // all below a -3 ceiling
        h.setClipCeiling (-3.0f);
        ok (render (h).size() == 1, "no column over the ceiling ⇒ still just the trace path");
        ok (bars (h).empty(), "no over-ceiling column ⇒ no red bar painted");

        h.push (gain (-1.0f));                                  // one over-ceiling spike, now the newest column
        ok (render (h).size() == 1, "the red bar is a fillRect, not a path — trace count is unchanged");
        const auto b1 = bars (h);
        ok (b1.size() == 1, "exactly one full-height red bar for the single over-ceiling column");
        if (b1.size() == 1)
            ok (near (b1[0].getCentreX(), (float) kW - 1.0f, 2.0f), "the bar sits on the newest (rightmost) column");
        // peakDb() and the trace math must be untouched by the ceiling
        ok (near (h.peakDb(), -1.0f, 0.01f), "clip ceiling does not disturb the held peak");

        h.setClipCeiling (std::numeric_limits<float>::quiet_NaN());
        ok (render (h).size() == 1, "clearing the ceiling is a no-op on the path set");
        ok (bars (h).empty(), "clearing the ceiling removes the red bar");
    }

    // =============================================================================================
    group ("ref-line grid: a dashed segment paints at each reference dB; an empty grid clears them");
    {
        LevelHistory h (16);
        h.setSize (kW, kH);
        for (int i = 0; i < 16; ++i) h.push (gain (-30.0f));    // a trace to tint under the grid

        const std::vector<std::pair<float, juce::Colour>> grid {
            { -3.0f,  juce::Colours::red },
            { -9.0f,  juce::Colours::yellow },
            { -15.0f, juce::Colours::green },
        };
        h.setRefLines (grid);
        const auto p = render (h);
        for (const auto& [db, col] : grid)
        {
            juce::ignoreUnused (col);
            const float y = dbToY (db, -60.0f, 0.0f);
            bool dashAtY = false;
            for (const auto& r : p)
                if (r.getWidth() < (float) kW / 2.0f && near (r.getCentreY(), y, 1.5f))
                    dashAtY = true;
            ok (dashAtY, "a dashed grid segment sits on the " + std::to_string ((int) db) + " dB reference line");
        }

        // the gradient-tinted fill must reach the strip floor — deleting the fill/stroke block would
        // otherwise regress silently (the dashes alone don't cover it)
        bool fillToFloor = false;
        for (const auto& r : p)
            if (r.getWidth() > (float) kW / 2.0f && near (r.getBottom(), (float) kH - 1.0f, 2.0f))
                fillToFloor = true;
        ok (fillToFloor, "grid mode fills the trace band down to the strip floor");

        // order-independence (crew finding): the SAME grid passed ascending must place the dashes
        // identically — the fix picks gradient endpoints by dB, not by vector position
        LevelHistory h2 (16);
        h2.setSize (kW, kH);
        for (int i = 0; i < 16; ++i) h2.push (gain (-30.0f));
        h2.setRefLines ({ { -15.0f, juce::Colours::green }, { -9.0f, juce::Colours::yellow },
                          { -3.0f, juce::Colours::red } });
        const auto pAsc = render (h2);
        for (const float db : { -3.0f, -9.0f, -15.0f })
        {
            const float y = dbToY (db, -60.0f, 0.0f);
            bool dashAtY = false;
            for (const auto& r : pAsc)
                if (r.getWidth() < (float) kW / 2.0f && near (r.getCentreY(), y, 1.5f)) dashAtY = true;
            ok (dashAtY, "ascending-order grid still lands a dash at each reference dB");
        }

        h.setRefLines ({});
        ok (render (h).size() == 1, "an empty grid clears the reference lines back to the bare trace");
    }

    // =============================================================================================
    group ("noise floor: a full-width line (fillRect) paints at its dBFS; a non-finite value hides it");
    {
        LevelHistory h (16);
        h.setSize (kW, kH);
        for (int i = 0; i < 16; ++i) h.push (gain (-40.0f));    // trace above the −50 floor line
        // no clip ceiling ⇒ the only fillRects are the noise-floor line
        h.setNoiseFloor (-50.0f);
        const float y = dbToY (-50.0f, -60.0f, 0.0f);
        auto floorLineAtY = [&] (LevelHistory& lh)
        {
            for (const auto& r : renderRects (lh))
                if (r.getWidth() > (float) kW / 2.0f && near (r.getCentreY(), y, 2.0f)) return true;
            return false;
        };
        ok (floorLineAtY (h), "a full-width line paints at the −50 dBFS noise floor");

        h.setNoiseFloor (std::numeric_limits<float>::quiet_NaN());
        ok (! floorLineAtY (h), "a non-finite noise floor hides the line");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
