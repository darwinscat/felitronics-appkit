// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#include <felitronics/appkit/LevelMeter.h>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace felitronics::appkit;

static int checks = 0, failures = 0;

static void ok (bool cond, const std::string& what)
{
    ++checks;
    if (! cond) { ++failures; std::printf ("    FAIL: %s\n", what.c_str()); }
}

static void group (const char* name) { std::printf ("  - %s\n", name); }

class RecordingContext final : public juce::LowLevelGraphicsContext
{
public:
    std::optional<float> barTopY;

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
    void fillRect (const juce::Rectangle<float>&) override {}
    void fillRectList (const juce::RectangleList<float>&) override {}
    void fillPath (const juce::Path&, const juce::AffineTransform&) override {}
    void fillRoundedRectangle (const juce::Rectangle<float>& r, float cornerSize) override
    {
        if (std::abs (cornerSize - 1.5f) < 0.25f)
            barTopY = r.getY();
    }
    void drawImage (const juce::Image&, const juce::AffineTransform&) override {}
    void drawLine (const juce::Line<float>&) override {}
    void setFont (const juce::Font& f) override { font = f; }
    const juce::Font& getFont() override { return font; }
    void drawGlyphs (juce::Span<const uint16_t>, juce::Span<const juce::Point<float>>,
                     const juce::AffineTransform&) override {}
    std::unique_ptr<juce::ImageType> getPreferredImageTypeForTemporaryImages() const override
    {
        return std::make_unique<juce::SoftwareImageType>();
    }
    uint64_t getFrameId() const override { return 0; }

private:
    juce::Font font { juce::FontOptions {} };
};

static std::optional<float> renderedBarY (LevelMeter& m)
{
    RecordingContext ctx;
    juce::Graphics g (ctx);
    m.paint (g);
    return ctx.barTopY;
}

static bool approx (float a, float b, float eps)
{
    return std::abs (a - b) <= eps;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::printf ("felitronics::appkit LevelMeter falsification tests\n");

    group ("invalid ranges are ignored instead of poisoning the dB mapping");
    {
        LevelMeter m;
        m.setSize (20, 240);
        m.setLevel (juce::Decibels::decibelsToGain (-6.0f));
        const auto baseline = renderedBarY (m);
        ok (baseline.has_value() && std::isfinite (*baseline), "baseline -6 dB bar is finite");

        const float nan = std::numeric_limits<float>::quiet_NaN();
        for (const auto badRange : { std::pair<float, float> { -12.0f, -12.0f },
                                     std::pair<float, float> {   6.0f, -60.0f },
                                     std::pair<float, float> { nan, 6.0f },
                                     std::pair<float, float> { -60.0f, nan } })
        {
            m.setRange (badRange.first, badRange.second);
            m.setLevel (juce::Decibels::decibelsToGain (-6.0f));
            const auto after = renderedBarY (m);
            ok (after.has_value() && std::isfinite (*after), "bar remains finite after invalid range");
            if (baseline && after)
                ok (approx (*after, *baseline, 1.0e-3f), "invalid range leaves the previous valid mapping intact");
        }

        m.setRange (-24.0f, 0.0f);
        m.setLevel (juce::Decibels::decibelsToGain (-6.0f));
        const auto zoomed = renderedBarY (m);
        ok (zoomed.has_value() && std::isfinite (*zoomed), "a later valid range is still accepted");
        if (baseline && zoomed)
            ok (! approx (*zoomed, *baseline, 0.5f), "valid zoom range changes the bar mapping");
    }

    group ("green zone verdict — below / inside / above, boundaries inclusive, invalid clears");
    {
        auto verdictAt = [] (float loDb, float hiDb, float peakDb)
        {
            LevelMeter m; m.setSize (20, 240);
            m.setGreenZone (loDb, hiDb);
            m.setLevel (juce::Decibels::decibelsToGain (peakDb));       // peak-hold instant-attacks to this
            return m.zone();
        };
        ok (verdictAt (-9.0f, -6.0f, -12.0f) == LevelMeter::Zone::below,  "peak under the floor reads below (won't train)");
        ok (verdictAt (-9.0f, -6.0f,  -7.5f) == LevelMeter::Zone::inside, "peak between thresholds reads inside");
        ok (verdictAt (-9.0f, -6.0f,  -3.0f) == LevelMeter::Zone::above,  "peak over the ceiling reads above (too hot / clip)");
        ok (verdictAt (-9.0f, -6.0f,  -9.0f) == LevelMeter::Zone::inside, "exactly the floor is inside (inclusive)");
        ok (verdictAt (-9.0f, -6.0f,  -6.0f) == LevelMeter::Zone::inside, "exactly the ceiling is inside (inclusive)");

        LevelMeter m; m.setSize (20, 240);
        ok (m.zone() == LevelMeter::Zone::none, "no zone set → none (default, existing consumers unaffected)");
        m.setGreenZone (-9.0f, -6.0f);
        m.setLevel (juce::Decibels::decibelsToGain (-7.0f));
        ok (m.zone() == LevelMeter::Zone::inside, "a set zone reports inside");
        const auto withZone = renderedBarY (m);
        ok (withZone.has_value() && std::isfinite (*withZone), "paint with a zone set still renders a finite bar");

        m.setGreenZone (-6.0f, -9.0f);                                  // lo >= hi
        ok (m.zone() == LevelMeter::Zone::none, "an inverted range clears the zone back to none");
        const float nan = std::numeric_limits<float>::quiet_NaN();
        m.setGreenZone (-9.0f, -6.0f);
        m.setGreenZone (nan, -6.0f);                                    // non-finite
        ok (m.zone() == LevelMeter::Zone::none, "a non-finite threshold clears the zone");
    }

    group ("clip ceiling opens a yellow (warn) band between the green top and the clip line");
    {
        auto zoneAt = [] (float loDb, float hiDb, float clipDb, float peakDb)
        {
            LevelMeter m; m.setSize (20, 240);
            m.setGreenZone (loDb, hiDb);
            m.setClipCeiling (clipDb);
            m.setLevel (juce::Decibels::decibelsToGain (peakDb));
            return m.zone();
        };
        // green band -12..-6, clip line -3 → below / inside / warn / above
        ok (zoneAt (-12.f, -6.f, -3.f, -20.f) == LevelMeter::Zone::below,  "under the floor → below (dark)");
        ok (zoneAt (-12.f, -6.f, -3.f,  -9.f) == LevelMeter::Zone::inside, "in the band → inside (green)");
        ok (zoneAt (-12.f, -6.f, -3.f,  -6.f) == LevelMeter::Zone::inside, "the band top is inside (inclusive)");
        ok (zoneAt (-12.f, -6.f, -3.f,  -4.f) == LevelMeter::Zone::warn,   "band top..clip → warn (yellow)");
        ok (zoneAt (-12.f, -6.f, -3.f,  -3.f) == LevelMeter::Zone::warn,   "the clip line is warn (inclusive)");
        ok (zoneAt (-12.f, -6.f, -3.f,  -1.f) == LevelMeter::Zone::above,  "over the clip line → above (red)");

        // clearing the ceiling reverts to two zones: everything above the band is `above`
        LevelMeter m; m.setSize (20, 240);
        m.setGreenZone (-12.f, -6.f);
        m.setClipCeiling (-3.f);
        m.setClipCeiling (std::numeric_limits<float>::quiet_NaN());
        m.setLevel (juce::Decibels::decibelsToGain (-4.f));
        ok (m.zone() == LevelMeter::Zone::above, "no clip set → above the band is `above` (old behaviour)");
    }

    std::printf ("%d checks, %d failures\n%s\n", checks, failures, failures == 0 ? "ALL TESTS PASSED" : "FAILED");
    return failures == 0 ? 0 : 1;
}
