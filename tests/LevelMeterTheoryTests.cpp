// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

// Theory-first falsification suite for LevelMeter.h (the dBFS peak meter).
//
// We test the MATH (ballistics + dB mapping), NOT pixels. LevelMeter exposes no getters, so the
// internal `level` / `peakHold` are read back by rendering paint() through a recording
// LowLevelGraphicsContext that captures the EXACT float geometry of the two draws that carry that
// state — the level bar (fillRoundedRectangle, cornerSize 1.5) and the peak-hold tick
// (fillRect, height 2). No pixels, no test hooks, no changes to LevelMeter.h.
//
// Contract derived from the header doc BEFORE reading the body, then run:
//   • setLevel(x): x = max(0, x). INSTANT attack — level := x when x ≥ level; otherwise a smooth
//     one-pole RELEASE  level += (x − level)·kRelease  (kRelease = 0.25 ⇒ level ← 0.75·level + 0.25·x).
//   • peak-hold: peakHold jumps up instantly; if the input stays below it, peakHold holds for exactly
//     kPeakHoldTicks (= 24) ticks, then decays ×kPeakDecay (= 0.92) per tick.
//   • dB MAPPING: a level's vertical position is affine in dB — y = jmap(clamp(db,min,max),min,max,
//     bottom,top). setRange(min,max) only zooms this window; it must NOT touch the ballistics.
//
// The reference model below encodes exactly those documented formulas; agreement between the
// rendered geometry and the reference across crafted tick sequences pins each constant. Registers in
// the FELITRONICS_APPKIT_TESTS_WITH_JUCE tier (juce_audio_basics + juce_gui_basics), like the other
// JUCE-tier gates.

#include <felitronics/appkit/LevelMeter.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
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

// Fixed meter geometry (narrow so no numeric labels are drawn → no font/glyph work in paint()).
static constexpr int   kW = 20;
static constexpr int   kH = 680;
static constexpr float kBottom = (float) kH;   // r.getBottom()
static constexpr float kTop    = 0.0f;         // r.getY()

//==============================================================================
// Recording context: captures the two ballistics-bearing draws, stubs everything else.
class RecordingContext final : public juce::LowLevelGraphicsContext
{
public:
    std::optional<float> barTopY;    // level bar   — fillRoundedRectangle(..., 1.5f)
    std::optional<float> peakTopY;   // peak-hold tick — fillRect(Rectangle<float>) of height ~2

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
    void fillRect (const juce::Rectangle<float>& r) override
    {
        // the peak-hold tick is the only height-2 float rect (scale lines are height 1)
        if (std::abs (r.getHeight() - 2.0f) < 0.5f)
            peakTopY = r.getY();
    }
    void fillRectList (const juce::RectangleList<float>&) override {}
    void fillPath (const juce::Path&, const juce::AffineTransform&) override {}
    void fillRoundedRectangle (const juce::Rectangle<float>& r, float cornerSize) override
    {
        // the level bar uses cornerSize 1.5; the meter background uses 2.0
        if (std::abs (cornerSize - 1.5f) < 0.25f)
            barTopY = r.getY();
    }
    void drawImage (const juce::Image&, const juce::AffineTransform&) override {}
    void drawLine (const juce::Line<float>&) override {}
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

struct Readback
{
    std::optional<float> barY;    // top-y of the level bar (nullopt ⇒ bar not drawn)
    std::optional<float> peakY;   // top-y of the peak-hold tick rect (nullopt ⇒ not drawn)
};

static Readback render (LevelMeter& m)
{
    RecordingContext ctx;
    juce::Graphics g (ctx);
    m.paint (g);
    return { ctx.barTopY, ctx.peakTopY };
}

//==============================================================================
// Documented dB→y mapping (independent replica) and its inverse.
static float dbToY (float db, float minDb, float maxDb)
{
    return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, kBottom, kTop);
}
static float yToDb (float y, float minDb, float maxDb)
{
    return minDb + (y - kBottom) * (maxDb - minDb) / (kTop - kBottom);
}
static float levelToBarY (float level, float minDb, float maxDb)
{
    const float db = level > 0.0f ? juce::Decibels::gainToDecibels (level) : -120.0f;
    return dbToY (db, minDb, maxDb);
}

// Reference ballistics — byte-for-byte the documented recurrence (this is the "theory").
struct Ref
{
    float level = 0.0f, peak = 0.0f;
    int   holdTicks = 0;
    void setLevel (float x)
    {
        x = juce::jmax (0.0f, x);
        if (x >= level) level = x;
        else            level += (x - level) * 0.25f;      // kRelease
        if (x >= peak)  { peak = x; holdTicks = 0; }
        else if (++holdTicks > 24) peak *= 0.92f;          // kPeakHoldTicks / kPeakDecay
    }
};

static float gain (float db) { return juce::Decibels::decibelsToGain (db); }
static bool  approx (float a, float b, float eps) { return std::abs (a - b) <= eps; }

static constexpr float kYEps = 1.0e-3f;   // rendered-vs-predicted top-y, in px

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // GUI subsystem for headless paint()
    std::printf ("felitronics::appkit LevelMeter theory tests\n");

    // =============================================================================================
    group ("instant attack, smooth release (asymmetry)");
    {
        LevelMeter m; m.setSize (kW, kH);
        const float minDb = -60.0f, maxDb = 6.0f;

        // rise: each higher input lands INSTANTLY at its own dB position (no smoothing on the way up)
        m.setLevel (gain (-6.0f));
        auto r1 = render (m);
        ok (r1.barY.has_value() && approx (*r1.barY, dbToY (-6.0f, minDb, maxDb), kYEps),
            "attack to -6 dB places the bar exactly at -6 dB (instant)");

        m.setLevel (gain (-3.0f));   // higher → instant jump, not a 0.75/0.25 blend
        auto r2 = render (m);
        ok (r2.barY.has_value() && approx (*r2.barY, dbToY (-3.0f, minDb, maxDb), kYEps),
            "attack up to -3 dB jumps instantly to -3 dB");

        // fall: one lower input does NOT snap down — it releases by 0.25 toward the input
        const float before = gain (-3.0f);
        const float target = gain (-40.0f);
        const float released = before + (target - before) * 0.25f;   // documented release
        m.setLevel (target);
        auto r3 = render (m);
        ok (r3.barY.has_value()
                && approx (*r3.barY, levelToBarY (released, minDb, maxDb), kYEps)
                && ! approx (*r3.barY, dbToY (-40.0f, minDb, maxDb), 1.0f),
            "one step down RELEASES (0.75·old + 0.25·new), never snaps to the input");
    }

    // =============================================================================================
    group ("release coefficient: level ← 0.75·level per silent tick (kRelease = 0.25)");
    {
        LevelMeter m; m.setSize (kW, kH);
        const float minDb = -60.0f, maxDb = 6.0f;
        Ref ref;

        const float A = gain (-6.0f);
        m.setLevel (A); ref.setLevel (A);

        bool matched = true;
        int  compared = 0;
        for (int n = 1; n <= 30; ++n)
        {
            m.setLevel (0.0f); ref.setLevel (0.0f);        // silence ⇒ level *= 0.75
            const auto rb = render (m);
            const float db = juce::Decibels::gainToDecibels (ref.level);
            if (db > minDb)                                // only assert while the bar is drawn
            {
                ++compared;
                const bool good = rb.barY.has_value()
                               && approx (*rb.barY, levelToBarY (ref.level, minDb, maxDb), kYEps);
                matched = matched && good;
            }
        }
        ok (matched && compared >= 8, "silent-tick decay tracks 0.75^n exactly for every drawn tick");
    }

    // =============================================================================================
    group ("peak-hold: holds EXACTLY 24 ticks, then decays ×0.92 (kPeakHoldTicks / kPeakDecay)");
    {
        LevelMeter m; m.setSize (kW, kH);
        const float minDb = -60.0f, maxDb = 6.0f;
        Ref ref;

        const float P = gain (-6.0f);
        m.setLevel (P); ref.setLevel (P);
        const float heldY = dbToY (juce::Decibels::gainToDecibels (P), minDb, maxDb) - 1.0f;   // tick rect y = py − 1

        // feeds 1..24 hold the peak at P; feed 25 is the first decay
        bool held = true, movedAt25 = false, decayTracks = true;
        for (int feed = 1; feed <= 40; ++feed)
        {
            m.setLevel (0.0f); ref.setLevel (0.0f);
            const auto rb = render (m);
            const float pdb = juce::Decibels::gainToDecibels (ref.peak);

            if (feed <= 24)
            {
                held = held && rb.peakY.has_value() && approx (*rb.peakY, heldY, kYEps);
            }
            else if (feed == 25)
            {
                movedAt25 = rb.peakY.has_value() && ! approx (*rb.peakY, heldY, 0.25f);
            }

            if (pdb > minDb && feed >= 25)   // decayed peak still tracks the reference exactly
            {
                const float expY = dbToY (pdb, minDb, maxDb) - 1.0f;   // tick rect y = py - 1
                decayTracks = decayTracks && rb.peakY.has_value() && approx (*rb.peakY, expY, kYEps);
            }
        }
        ok (held,        "peak-hold tick is stationary for the first 24 silent ticks");
        ok (movedAt25,   "peak begins to decay on the 25th silent tick (hold = exactly 24)");
        ok (decayTracks, "post-hold decay follows ×0.92 per tick exactly");
    }

    // =============================================================================================
    group ("dB mapping is affine in dB (equal dB steps ⇒ equal pixel steps)");
    {
        const float minDb = -60.0f, maxDb = 6.0f;
        auto barYForDb = [&] (float db)
        {
            LevelMeter m; m.setSize (kW, kH);
            m.setLevel (gain (db));           // instant attack from rest ⇒ level = gain(db)
            return render (m).barY;
        };
        const auto yLo = barYForDb (-42.0f);
        const auto yMi = barYForDb (-24.0f);
        const auto yHi = barYForDb (-6.0f);      // three points, equal 18 dB spacing
        ok (yLo && yMi && yHi, "all three probe levels draw a bar");
        if (yLo && yMi && yHi)
        {
            // each lands exactly where the affine map predicts
            const bool onMap = approx (*yLo, dbToY (-42.0f, minDb, maxDb), kYEps)
                            && approx (*yMi, dbToY (-24.0f, minDb, maxDb), kYEps)
                            && approx (*yHi, dbToY (-6.0f,  minDb, maxDb), kYEps);
            // equal dB steps ⇒ equal, monotone pixel steps
            const float d1 = *yLo - *yMi, d2 = *yMi - *yHi;
            ok (onMap, "bar positions match the affine dB→y map at all three probes");
            ok (approx (d1, d2, kYEps) && d1 > 1.0f, "equal 18 dB steps produce equal pixel steps (affine)");
        }
    }

    // =============================================================================================
    group ("range zoom changes only the mapping, never the ballistics");
    {
        // Same input sequence into a wide meter and a zoomed meter; recover the internal level from
        // each meter's own mapping. Ballistics are range-independent ⇒ recovered levels must agree.
        LevelMeter wide; wide.setSize (kW, kH);                          // −60..+6
        LevelMeter zoom; zoom.setSize (kW, kH); zoom.setRange (-24.0f, 6.0f);

        const float attack = gain (-8.0f);
        wide.setLevel (attack); zoom.setLevel (attack);

        bool levelsAgree = true;
        int  compared = 0;
        const float rel = gain (-18.0f);        // release toward −18 dB: level stays inside both windows
        for (int n = 0; n < 20; ++n)
        {
            wide.setLevel (rel); zoom.setLevel (rel);
            const auto rw = render (wide);
            const auto rz = render (zoom);
            if (rw.barY && rz.barY)
            {
                ++compared;
                const float lw = gain (yToDb (*rw.barY, -60.0f, 6.0f));
                const float lz = gain (yToDb (*rz.barY, -24.0f, 6.0f));
                levelsAgree = levelsAgree && approx (lw, lz, 1.0e-4f);
            }
        }
        ok (levelsAgree && compared >= 15, "recovered level is identical across the two zoom ranges");

        // Peak-hold timing must also be range-independent: still holds 24, moves on 25 when zoomed.
        LevelMeter zm; zm.setSize (kW, kH); zm.setRange (-24.0f, 6.0f);
        const float P = gain (-6.0f);
        zm.setLevel (P);
        const float heldY = dbToY (juce::Decibels::gainToDecibels (P), -24.0f, 6.0f) - 1.0f;   // tick rect y = py − 1
        bool zHeld = true, zMoved = false;
        for (int feed = 1; feed <= 26; ++feed)
        {
            zm.setLevel (0.0f);
            const auto rb = render (zm);
            if (feed <= 24)      zHeld  = zHeld && rb.peakY.has_value() && approx (*rb.peakY, heldY, kYEps);
            else if (feed == 25) zMoved = rb.peakY.has_value() && ! approx (*rb.peakY, heldY, 0.25f);
        }
        ok (zHeld && zMoved, "hold duration is 24 ticks regardless of the zoom range");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
