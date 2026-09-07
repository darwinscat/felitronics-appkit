// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-appkit — see LICENSE.

#pragma once

//==============================================================================
// felitronics::appkit::IrWaveView — an impulse response, drawn, with the two things a player does to
// one drawn ON it: TRIM (drag anywhere to truncate — the cut region dims and a marker tracks it) and
// the HPF/LPF response curve with a draggable corner point per enabled filter. Moved here from
// OrbitCab's WaveformDisplay so the family's cabinet blocks draw an IR one way; OrbitCab consumes it
// from here next. The view decodes the IR itself on the message thread and shares nothing with any
// audio-side load. Changes are reported through callbacks; the owner writes the parameters and
// pushes them back with setFilters / setTrimFraction, so the picture always says what the sound is.
//
// The geometry and envelope math (freq <-> x, EQ magnitude/curve, peak buckets, dB height) is in
// irwave:: below — plain numbers, no Graphics — so a headless test can pin it.
//
// Modules: juce_audio_utils (formats + gui).
//==============================================================================

#include <felitronics/appkit/Brand.h>

#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <functional>
#include <vector>

namespace felitronics::appkit
{

namespace irwave
{
    // Log-frequency axis: frequency <-> horizontal pixel across [x0, x0 + w] for [fMin, fMax].
    inline float xForFreq (float f, float x0, float w, float fMin, float fMax)
    {
        return x0 + w * std::log (f / fMin) / std::log (fMax / fMin);
    }

    inline float freqForX (float x, float x0, float w, float fMin, float fMax)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - x0) / juce::jmax (1.0f, w));
        return fMin * std::pow (fMax / fMin, t);
    }

    // Butterworth HPF/LPF magnitude product at the given orders (slope dB/oct / 6): ~1 in the
    // passband, ~0.707 at a corner, -> 0 deep in a stopband. The closed form is exact for a
    // Butterworth cascade of any order, so the curve draws the slope the sound actually has.
    inline float eqMagnitude (float f, bool hpfOn, float hpfHz, int hpfSlopeDb,
                              bool lpfOn, float lpfHz, int lpfSlopeDb)
    {
        float m = 1.0f;
        if (hpfOn) { const float r = hpfHz / f;
                     m *= 1.0f / std::sqrt (1.0f + std::pow (r, (float) hpfSlopeDb / 3.0f)); }
        if (lpfOn) { const float r = f / lpfHz;
                     m *= 1.0f / std::sqrt (1.0f + std::pow (r, (float) lpfSlopeDb / 3.0f)); }
        return m;
    }

    // Magnitude (0..1) -> y within [y0, y0 + h]: the passband sits at 0.62 h, so the curve hints at
    // the EQ below the waveform's midline; mag = 0 runs off the bottom.
    inline float eqCurveY (float mag, float y0, float h)
    {
        const float top = y0 + h * 0.62f;
        const float bot = y0 + h * 1.30f;
        return bot - (bot - top) * mag;
    }

    // A mono max-abs span downsampled into `buckets` peak magnitudes, normalised to 0..1 — the
    // drawn envelope of whatever window the caller hands in.
    inline std::vector<float> computePeaks (const float* d, int numSamples, int buckets)
    {
        std::vector<float> peaks ((size_t) juce::jmax (0, buckets), 0.0f);
        if (buckets <= 0 || numSamples <= 0 || d == nullptr)
            return peaks;

        float globalMax = 0.0f;
        for (int b = 0; b < buckets; ++b)
        {
            const int start = (int) ((juce::int64) b       * numSamples / buckets);
            int       end   = (int) ((juce::int64) (b + 1) * numSamples / buckets);
            if (end <= start) end = juce::jmin (numSamples, start + 1);
            float mx = 0.0f;
            for (int k = start; k < end; ++k)
                mx = juce::jmax (mx, d[k]);
            peaks[(size_t) b] = mx;
            globalMax = juce::jmax (globalMax, mx);
        }
        if (globalMax > 0.0f)
            for (auto& v : peaks)
                v /= globalMax;
        return peaks;
    }

    // An IR downsampled into `buckets` peak magnitudes, normalised to 0..1 — the drawn envelope. A
    // short IR stretches to the width; a long one is covered whole.
    inline std::vector<float> computePeaks (const juce::AudioBuffer<float>& buf, int numSamples, int buckets)
    {
        std::vector<float> peaks ((size_t) juce::jmax (0, buckets), 0.0f);
        if (buckets <= 0 || numSamples <= 0 || buf.getNumChannels() <= 0)
            return peaks;

        float globalMax = 0.0f;
        for (int b = 0; b < buckets; ++b)
        {
            const int start = (int) ((juce::int64) b       * numSamples / buckets);
            int       end   = (int) ((juce::int64) (b + 1) * numSamples / buckets);
            if (end <= start) end = juce::jmin (numSamples, start + 1);
            float mx = 0.0f;
            for (int c = 0; c < buf.getNumChannels(); ++c)
            {
                const float* d = buf.getReadPointer (c);
                for (int k = start; k < end; ++k)
                    mx = juce::jmax (mx, std::abs (d[k]));
            }
            peaks[(size_t) b] = mx;
            globalMax = juce::jmax (globalMax, mx);
        }
        if (globalMax > 0.0f)
            for (auto& v : peaks)
                v /= globalMax;
        return peaks;
    }

    // Normalised peak (0..1) -> height factor on a dB scale with a floor: a cab IR's onset crushes
    // its tail on a linear scale; in dB the tail comes into view.
    inline float dbHeightFactor (float mag01, float floorDb)
    {
        const float db = juce::Decibels::gainToDecibels (mag01, floorDb);
        return juce::jlimit (0.0f, 1.0f, (db - floorDb) / (0.0f - floorDb));
    }
} // namespace irwave

class IrWaveView final : public juce::Component,
                         private juce::Timer
{
public:
    IrWaveView() { formatManager.registerBasicFormats(); }

    std::function<void (float)>        onTrimChanged;    // fraction to keep, (0,1]
    std::function<void (bool, float)>  onHpfChanged;     // (on, Hz)
    std::function<void (bool, float)>  onLpfChanged;     // (on, Hz)
    std::function<void (int)>          onHpfSlopeStep;   // slope-ladder notches, +1 = steeper
    std::function<void (int)>          onLpfSlopeStep;   //   (vertical drag on the cut line, down = steeper)
    std::function<void (bool)>         onTrimToggled;    // the right-click menu's TRIM entry
    std::function<void()>              onMenuRequested;  // set by a consumer that owns the trim story:
                                                         //   right-click calls this instead of the built-in menu

    // The servo thirds: while the hand drags the trim, the window follows so the handle keeps to
    // the middle band — past the left third it zooms in (fine milliseconds near zero), past the
    // right two-thirds it zooms out (always more room to pull). Stays where the drag left it.
    bool trimServoZoom = true;

    /** How briskly the picture travels to a new window: the fraction of the remaining ratio it
        covers each tick, at 60 a second. Lower is slower and softer; 1.0 is the old jump.
        0.06 puts the bulk of the travel around a third of a second — a zoom you can watch happen
        rather than one you have to re-read after it lands. */
    double glideStep = 0.06;

    // Each cut wears its own colour on its line, and the cut fill is the consumer's to tint —
    // defaults follow the accent so a one-colour picture stays a one-colour picture.
    juce::Colour hpfTint = brand::orange;
    juce::Colour lpfTint = brand::violet;
    juce::Colour cutFill = brand::violet;

    // The impulse envelope's own colour. Transparent = follow the accent, as it always did — a
    // consumer may give the wave a voice apart from the curve's.
    juce::Colour waveTint { 0x00000000 };

    void setFromMemory (const void* data, size_t size)
    {
        load (formatManager.createReaderFor (std::make_unique<juce::MemoryInputStream> (data, size, false)));
    }

    void setFromFile (const juce::File& file) { load (formatManager.createReaderFor (file)); }

    void clearIR()
    {
        peaks.clear();
        waveAbs.clear();
        metrics.clear();
        trimFraction = 1.0f;
        irMs = 0.0;
        viewWindowMs = viewTargetMs = 0.0;
        stopTimer();
        repaint();
    }

    void setTrimInteractive (bool shouldBeInteractive) { trimInteractive = shouldBeInteractive; updateCursor(); repaint(); }
    void setTrimFraction (float fraction01)            { trimFraction = juce::jlimit (minTrimFraction(), 1.0f, fraction01); repaint(); }
    float getTrimFraction() const                       { return trimFraction; }
    double lengthMs() const                             { return irMs; }
    void setEqVisible (bool shouldShow)                { eqVisible = shouldShow; updateCursor(); repaint(); }

    // The TRIM handle (and trimming) only show and work while the owner's TRIM is on.
    void setTrimEnabled (bool shouldBeEnabled)
    {
        // TRIM switched on under a view window falls under the window's rule at once: a handle
        // standing beyond the window jumps to its edge, and the jump is a real parameter write —
        // it stays wherever the window put it even after the window is gone.
        if (shouldBeEnabled && ! trimEnabled)
            clampTrimToWindow();

        trimEnabled = shouldBeEnabled;
        repaint();
    }

    /** The view window in ms — 0 shows the whole IR. Also the trim's master while one is chosen:
        an enabled trim beyond the window is pulled to its edge (a real write, through
        onTrimChanged). The right-click menu drives this; a consumer may too. */
    void setViewWindow (double ms)
    {
        // The RULE lands at once — a handle outside the new window is pulled in immediately, and
        // that is a parameter write, which must happen once and not once per frame of the travel.
        // Only the PICTURE takes its time getting there.
        glideTo (ms);

        if (trimEnabled)
            clampTrimToWindow();

        repaint();
    }

    /** THE FRAMING LAW: where the picture stands for the trim it is showing.

        The window becomes twice the trim's milliseconds, so the trim's edge — a handle in manual,
        a chosen window's cut — sits in the MIDDLE of the frame, with as much of the shot before it
        as after. Floored at 20 ms, because below that there is nothing to read; and a window that
        outgrows the shot is no window, so it collapses to FULL.

        Twice is not arbitrary: the servo inside a drag keeps the trim between a third and two
        thirds of the frame, and the middle of that band is exactly one half. This is that same
        band's centre, applied in one go.

        The point of having it at all is that the picture then follows the TRIM and not the story
        told about it. A trim of 50 ms looks the same whether the hand put it there, a menu named
        it, or a preset brought it back — the picture only moves when the trim moves. */
    void frameTrim()
    {
        if (irMs <= 0.0)
            return;

        // The band's centre — see kServoEdge. Twice the milliseconds, written as the middle of the
        // same band the servo holds, so the two can never drift apart.
        const double win = juce::jlimit (20.0, irMs, (double) trimFraction * irMs / 0.5);

        glideTo (win >= irMs ? 0.0 : win);
    }

    // A live spectrum behind the impulse, drawn by the OWNER — handed the impulse's rectangle, so
    // whatever analyser look the owner's console already has is the look here too, one renderer
    // for both. Optional; nothing is drawn without it.
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> paintSpectrumUnder;

    // The tint of the impulse, the curve, the spectrum and the trim handle.
    void setAccent (juce::Colour c)                    { accent = c; repaint(); }

    // Amplitude scale: log (dB, with a floor) lifts a cab IR's decay tail into view; linear does not.
    void setAmplitudeScale (bool log, float floorDb)   { ampLog = log; dbFloor = floorDb; repaint(); }

    // Pushed from the owner (parameters, host automation) so the curve stays in sync.
    void setFilters (bool hOn, float hHz, float hMin, float hMax,
                     bool lOn, float lHz, float lMin, float lMax)
    {
        hpfOn = hOn; hpfHz = hHz; hpfMin = hMin; hpfMax = hMax;
        lpfOn = lOn; lpfHz = lHz; lpfMin = lMin; lpfMax = lMax;
        repaint();
    }

    // The cuts' steepness, in dB/oct — drawn exactly, and stepped by the vertical drag ladder.
    void setSlopes (int hpfDb, int lpfDb)
    {
        hpfSlopeDb = hpfDb; lpfSlopeDb = lpfDb;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff191920), 0.0f, 0.0f,
                                                 juce::Colour (0xff101013), 0.0f, (float) getHeight(), false));
        g.fillRect (getLocalBounds());
        const auto r = getLocalBounds().toFloat();

        if (peaks.empty())
        {
            g.setColour (juce::Colour (0xff3a3a40));
            g.setFont (juce::FontOptions (14.0f));
            g.drawText ("No IR", getLocalBounds(), juce::Justification::centred, false);
            return;
        }

        const float mid   = r.getCentreY();
        const float amp   = r.getHeight() * 0.46f;
        const int   n     = (int) peaks.size();
        const float trimX = trimXFor (r);

        g.setColour (juce::Colour (0x14ffffff));
        g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
        drawDbGrid (g, r, mid, amp);
        drawTimeGrid (g, r);

        if (paintSpectrumUnder != nullptr && isEnabled())
            paintSpectrumUnder (g, r);

        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) n;
            const float h = (ampLog ? irwave::dbHeightFactor (peaks[(size_t) i], dbFloor)
                                    : peaks[(size_t) i]) * amp;
            g.setColour (x <= trimX ? (waveTint.getAlpha() > 0 ? waveTint : accent).withAlpha (0.85f)
                                    : juce::Colour (0x33ffffff));
            g.drawLine (x, mid - h, x, mid + h, 1.0f);
        }

        // WHERE THE CUT IS is a fact of the picture, whoever named it. It used to be drawn only
        // while the trim was a handle, so a chosen 50 ms had nothing to mark it but the wave
        // changing colour, and against a grey ruler line at the same place that reads as no answer
        // at all. The shade and the edge belong to the trim; only the GRIP belongs to the hand —
        // a tab on an edge no hand can move is a promise the picture cannot keep.
        if (trimEnabled)
        {
            if (trimFraction < 0.999f)
            {
                g.setColour (juce::Colour (0x73000000));
                g.fillRect (juce::Rectangle<float> (trimX, r.getY(), r.getRight() - trimX, r.getHeight()));
            }

            // Kept just inside the right edge at full length, so TRIM is discoverable.
            const float hx = juce::jlimit (r.getX() + 4.0f, r.getRight() - 4.0f, trimX);
            g.setColour (accent.withAlpha (0.8f));
            g.drawLine (hx, r.getY(), hx, r.getBottom(), 1.5f);

            if (trimInteractive)
            {
                const juce::Rectangle<float> tab (hx - 6.0f, mid - 20.0f, 12.0f, 40.0f);
                g.setColour (accent);
                g.fillRoundedRectangle (tab, 3.0f);
                g.setColour (juce::Colour (0xcc141417));
                for (int i = -1; i <= 1; ++i)
                    g.drawLine (hx + (float) i * 2.5f, mid - 8.0f, hx + (float) i * 2.5f, mid + 8.0f, 1.0f);
            }
        }

        if (eqVisible && (hpfOn || lpfOn))
            drawEq (g, r);

        drawReadout (g, r);

        g.setColour (juce::Colour (0xffb0b0b0));
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (metrics, getLocalBounds().reduced (8).removeFromBottom (18),
                    juce::Justification::bottomLeft, false);

        if (! isEnabled())
        {
            g.setColour (juce::Colour (0xa8141417));
            g.fillRect (getLocalBounds());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (onMenuRequested != nullptr) onMenuRequested();
            else                            showViewMenu();
            return;
        }
        dragMode = pickMode (e.position); stepAnchor = e.position.y; applyDrag (e.position); repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override { applyDrag (e.position); repaint(); }
    void mouseUp   (const juce::MouseEvent& e) override
    {
        // The hand LETS GO: the trim has come to rest, so the picture settles into the frame that
        // trim asks for. During the drag the servo only nudges the window when the handle would
        // otherwise leave the frame — it has to, or the handle would stop following the cursor —
        // and where that leaves the window depends on which way the hand was moving. Settling here
        // is what makes a hand-placed 50 ms and a 50 ms picked from a menu the same picture.
        const bool wasTrimming = dragMode == Drag::trim;

        dragMode = Drag::none;
        hoverEl  = pickMode (e.position);

        if (wasTrimming && trimServoZoom)
            frameTrim();

        repaint();
    }
    void mouseMove (const juce::MouseEvent& e) override { if (const auto h = pickMode (e.position); h != hoverEl) { hoverEl = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&)   override { if (hoverEl != Drag::none) { hoverEl = Drag::none; repaint(); } }

private:
    enum class Drag { none, trim, hpf, lpf };

    // The trim's floor is MILLISECONDS, not a share: 2% of a long shot forbade the extreme cuts
    // (a 1.3 s cab IR could not go under ~26 ms), while 2 ms is where the speaker itself still
    // lives — the servo lets the hand actually get there.
    static constexpr double kMinTrimMs = 2.0;
    static constexpr float  kMinTrim   = 0.001f;   // last-resort clamp when the length is unknown

    float minTrimFraction() const
    {
        return irMs > 0.0 ? juce::jmax ((float) (kMinTrimMs / irMs), 1.0e-4f) : kMinTrim;
    }
    static constexpr float kFMin    = 20.0f;
    static constexpr float kFMax    = 20000.0f;
    static constexpr float kGrabPx  = 14.0f;
    // The magnet's reach, in pixels. Wide on purpose: the marks ARE the places worth stopping, so
    // the hand should fall into them rather than be asked to aim. Command held turns it off for
    // the times a number between the numbers is what you want.
    static constexpr float kSnapPx  = 19.0f;
    static constexpr int   kBuckets = 512;
    static constexpr int   kGlideHz = 60;       // the zoom's travel, frames a second

    /** The ms marks: the ruler's lines, the magnet's stops, and the window choices in the
        picture's own menu — ONE list, because a hand that cannot land on a number the owner's
        menu offers reads as a hand that is being fought. 25 rather than 20 for the shortest, to
        match the window the cabinet names. */
    static constexpr double kTimeMarksMs[] = { 25.0, 50.0, 100.0, 200.0, 500.0 };

    /** How far into the frame a mark must stand to be worth drawing — a tenth of it. */
    static constexpr double kMarkRoom = 10.0;

    /** How near the frame's edge the handle may drift before the window goes after it: it lives
        between this and one minus this, and the picture holds still for the whole of that. A
        narrower band means the zoom starts earlier and the ground moves under the hand sooner
        than the hand asked; a fifth leaves most of the frame as room to work in, and the servo
        only speaks up once the handle is genuinely running out of picture. The band's CENTRE is
        where `frameTrim` puts it, which is why that one is twice the milliseconds. */
    static constexpr double kServoEdge = 0.2;

    /** Whether that mark is on the picture at all: it lives in the outer nine tenths of the frame
        and nowhere else. Past the right edge there is nothing to point at; nearer the left edge
        than a tenth it is a smudge rather than a reading — at the whole 1276 ms shot 25, 50 and
        100 stand at two, four and eight percent, on top of one another, and three unreadable
        numbers are worse than one readable one. Each comes back as the window closes on it.

        ONE rule for all five, not a table of exceptions: a mark is shown while it stands at least
        a tenth of the way across. The MAGNET asks the same question — a stop the eye cannot see is
        a hand being fought, so the lines you can land on are exactly the lines you are shown. */
    bool markShown (size_t i) const
    {
        const double win = effectiveMs();

        return kTimeMarksMs[i] < win && win < kTimeMarksMs[i] * kMarkRoom;
    }

    float xForFreq (float f, juce::Rectangle<float> r) const { return irwave::xForFreq (f, r.getX(), r.getWidth(), kFMin, kFMax); }
    float freqForX (float x, juce::Rectangle<float> r) const { return irwave::freqForX (x, r.getX(), r.getWidth(), kFMin, kFMax); }
    float curveY   (float mag, juce::Rectangle<float> r) const { return irwave::eqCurveY (mag, r.getY(), r.getHeight()); }
    float magAt    (float f) const { return irwave::eqMagnitude (f, hpfOn, hpfHz, hpfSlopeDb,
                                                                 lpfOn, lpfHz, lpfSlopeDb); }

    void drawDbGrid (juce::Graphics& g, juce::Rectangle<float> r, float mid, float amp)
    {
        auto rule = [&] (int db, float dy)
        {
            g.setColour (juce::Colour (0x12ffffff));
            g.drawHorizontalLine ((int) (mid - dy), r.getX(), r.getRight());
            g.drawHorizontalLine ((int) (mid + dy), r.getX(), r.getRight());
            g.setColour (juce::Colour (0x55b0b0b0));
            g.setFont (juce::FontOptions (9.0f));
            g.drawText (juce::String (db), juce::Rectangle<float> (r.getRight() - 26.0f, mid - dy - 6.0f, 24.0f, 12.0f),
                        juce::Justification::centredRight, false);
        };
        if (ampLog)
            for (const int db : { -12, -24, -36 })
                rule (db, amp * irwave::dbHeightFactor (juce::Decibels::decibelsToGain ((float) db), dbFloor));
        else
            for (const int db : { -6, -12, -18 })
                rule (db, amp * juce::Decibels::decibelsToGain ((float) db));
    }

    void drawTimeGrid (juce::Graphics& g, juce::Rectangle<float> r)
    {
        if (irMs <= 0.0)
            return;
        // Every mark reads the same, because every mark does the same thing: it is a place the
        // trim lands. There used to be two ranks — 50 and 100 bright and bold, the rest a whisper
        // — which said the whisperers were somehow lesser stops. They are not; they are the ones
        // that had no room, and the answer to no room is to stand out of the way, not to fade.
        for (size_t i = 0; i < std::size (kTimeMarksMs); ++i)
        {
            if (! markShown (i))
                continue;

            const double ms = kTimeMarksMs[i];
            const float  x  = r.getX() + r.getWidth() * (float) (ms / effectiveMs());

            g.setColour (juce::Colour (0x30ffffffu));
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            g.setColour (juce::Colour (0xaab2b2bau));
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawText (juce::String ((int) ms), juce::Rectangle<float> (x + 2.0f, r.getY() + 1.0f, 32.0f, 11.0f),
                        juce::Justification::topLeft, false);
        }
    }

    void drawEq (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Path p;
        const int w = (int) r.getWidth();
        for (int x = 0; x <= w; ++x)
        {
            const float f = freqForX (r.getX() + (float) x, r);
            const float y = curveY (magAt (f), r);
            if (x == 0) p.startNewSubPath (r.getX() + (float) x, y);
            else        p.lineTo          (r.getX() + (float) x, y);
        }
        // What the cuts take away, filled the consoles' way: the tint densest at the passband
        // line, fading to nothing at the bottom — the DEVIATION from flat, not the passband.
        const float y0 = curveY (1.0f, r);
        juce::Path fill = p;
        fill.lineTo (r.getRight(), y0);
        fill.lineTo (r.getX(),     y0);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (cutFill.withAlpha (0.0f),  0.0f, r.getBottom(),
                                                 cutFill.withAlpha (0.40f), 0.0f, y0, false));
        g.fillPath (fill);

        // The consoles' composite stroke: the accent with a faint glow — stacked strokes, no blur.
        g.setColour (accent.withAlpha (0.12f));
        g.strokePath (p, juce::PathStrokeType (5.0f));
        g.setColour (accent.withAlpha (0.22f));
        g.strokePath (p, juce::PathStrokeType (2.5f));
        g.setColour (accent);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

        if (hpfOn) drawCutLine (g, r, hpfHz, hpfTint, dragMode == Drag::hpf || hoverEl == Drag::hpf);
        if (lpfOn) drawCutLine (g, r, lpfHz, lpfTint, dragMode == Drag::lpf || hoverEl == Drag::lpf);
    }

    // A cut is a PLACE, not an amount — its handle is the whole vertical line, the consoles'
    // dashed grammar, grabbable anywhere along its height.
    void drawCutLine (juce::Graphics& g, juce::Rectangle<float> r, float f,
                      juce::Colour tint, bool lit)
    {
        const float x = xForFreq (f, r);
        const float dashes[] = { 5.0f, 4.0f };
        g.setColour (tint.withAlpha (lit ? 1.0f : 0.65f));
        g.drawDashedLine ({ x, r.getY() + 2.0f, x, r.getBottom() - 2.0f },
                          dashes, 2, lit ? 2.2f : 1.4f);
    }

    // The value in a pill beside the hovered or dragged handle: Hz, kHz, ms.
    /** Where the trim's edge falls on screen. The fraction is of the WHOLE shot and the picture
        shows a window of it, so the two have to be divided — anything that points at the trim
        without doing that points at it only when the picture is showing everything. */
    float trimXFor (juce::Rectangle<float> r) const
    {
        return r.getX() + r.getWidth()
                 * (float) juce::jmin (1.0, (double) trimFraction * irMs
                                              / juce::jmax (1.0, effectiveMs()));
    }

    void drawReadout (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const Drag el = (dragMode != Drag::none) ? dragMode : hoverEl;
        juce::String text;
        float x = 0.0f, y = 0.0f;

        if (el == Drag::hpf && hpfOn && eqVisible)
        {
            text = juce::String (juce::roundToInt (hpfHz)) + " Hz";
            x = xForFreq (hpfHz, r);
            y = curveY (magAt (hpfHz), r);
        }
        else if (el == Drag::lpf && lpfOn && eqVisible)
        {
            text = juce::String (lpfHz / 1000.0f, 1) + " kHz";
            x = xForFreq (lpfHz, r);
            y = curveY (magAt (lpfHz), r);
        }
        else if (el == Drag::trim && trimEnabled)
        {
            text = juce::String (juce::roundToInt (trimFraction * irMs)) + " ms";
            x = juce::jlimit (r.getX() + 4.0f, r.getRight() - 4.0f, trimXFor (r));
            y = r.getCentreY() - 8.0f;
        }
        else
            return;

        const float w = (float) text.length() * 7.0f + 12.0f;
        juce::Rectangle<float> box (0.0f, 0.0f, w, 16.0f);
        box.setCentre (juce::jlimit (r.getX() + w * 0.5f, r.getRight() - w * 0.5f, x),
                       juce::jlimit (r.getY() + 9.0f, r.getBottom() - 9.0f, y - 16.0f));
        g.setColour (juce::Colour (0xdd0e0e12));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (accent.withAlpha (0.95f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (text, box, juce::Justification::centred, false);
    }

    Drag pickMode (juce::Point<float> pos)
    {
        if (peaks.empty())
            return Drag::none;

        const auto r = getLocalBounds().toFloat();
        if (eqVisible)
        {
            if (hpfOn && std::abs (pos.x - xForFreq (hpfHz, r)) < kGrabPx) return Drag::hpf;
            if (lpfOn && std::abs (pos.x - xForFreq (lpfHz, r)) < kGrabPx) return Drag::lpf;
        }
        if (trimInteractive && trimEnabled)
            return Drag::trim;
        return Drag::none;
    }

    void applyDrag (juce::Point<float> pos)
    {
        const auto r = getLocalBounds().toFloat();
        switch (dragMode)
        {
            case Drag::trim: setTrimFromMouse (pos.x); break;
            case Drag::hpf:
                hpfHz = juce::jlimit (hpfMin, hpfMax, freqForX (pos.x, r));
                workLadder (pos.y, onHpfSlopeStep);
                repaint();
                if (onHpfChanged) onHpfChanged (true, hpfHz);
                break;
            case Drag::lpf:
                lpfHz = juce::jlimit (lpfMin, lpfMax, freqForX (pos.x, r));
                workLadder (pos.y, onLpfSlopeStep);
                repaint();
                if (onLpfChanged) onLpfChanged (true, lpfHz);
                break;
            case Drag::none: break;
        }
    }

    // The vertical axis of a cut drag works the slope ladder, the consoles' gesture: every stepPx
    // of travel is one notch, down = steeper — the cut digs in as the hand digs down.
    void workLadder (float y, const std::function<void (int)>& step)
    {
        constexpr float stepPx = 28.0f;
        const int steps = (int) ((y - stepAnchor) / stepPx);

        if (steps != 0 && step != nullptr)
        {
            stepAnchor += (float) steps * stepPx;
            step (steps);
        }
    }

    void setTrimFromMouse (float x)
    {
        if (! trimInteractive || peaks.empty())
            return;
        const float w = (float) juce::jmax (1, getWidth());
        // The mouse walks the WINDOW; the fraction stays of the whole IR.
        const double winOfFull = irMs > 0.0 ? effectiveMs() / irMs : 1.0;
        float f = juce::jlimit (minTrimFraction(), 1.0f, (float) ((double) (x / w) * winOfFull));

        // The trim magnetises to the ms marks; hold the command key for a fine one.
        if (irMs > 0.0 && ! juce::ModifierKeys::getCurrentModifiers().isCommandDown())
        {
            float best = kSnapPx;
            for (size_t i = 0; i < std::size (kTimeMarksMs); ++i)
            {
                if (! markShown (i))     // the same lines the eye is given, and no others
                    continue;

                const double ms = kTimeMarksMs[i];
                const float  mx = w * (float) (ms / effectiveMs());

                if (std::abs (x - mx) < best)
                {
                    best = std::abs (x - mx);
                    f    = juce::jlimit (minTrimFraction(), 1.0f, (float) (ms / irMs));
                }
            }
        }

        if (std::abs (f - trimFraction) < 1.0e-4f)
            return;
        trimFraction = f;

        // The servo thirds (see the field above): continuous, floored at 20 ms, capped at the
        // whole shot; a window that leaves the cap collapses back to FULL.
        if (trimServoZoom && irMs > 0.0)
        {
            const double ms  = (double) trimFraction * irMs;
            const double win = targetMs();          // where it is HEADED, not where it is now:
            double newWin = win;                    // judging the travel re-aims it every frame

            // Out of the band: bring the window to where the handle stands ON the edge it left,
            // and no further — the least correction that puts it back in the picture.
            if      (ms < win * kServoEdge)         newWin = ms / kServoEdge;
            else if (ms > win * (1.0 - kServoEdge)) newWin = ms / (1.0 - kServoEdge);

            newWin = juce::jlimit (20.0, irMs, newWin);

            if (! juce::approximatelyEqual (newWin, win))
                glideTo (newWin >= irMs ? 0.0 : newWin);
        }

        repaint();
        if (onTrimChanged) onTrimChanged (f);
    }

    void updateCursor()
    {
        setMouseCursor ((trimInteractive || eqVisible) ? juce::MouseCursor::PointingHandCursor
                                                       : juce::MouseCursor::NormalCursor);
    }

    void load (juce::AudioFormatReader* rawReader)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (rawReader);
        if (reader == nullptr || reader->lengthInSamples <= 0)
        {
            clearIR();
            return;
        }

        const double sr = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
        const int n  = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (20.0 * sr));
        const int ch = juce::jmax (1, (int) reader->numChannels);

        juce::AudioBuffer<float> buf (ch, n);
        reader->read (&buf, 0, n, 0, true, true);

        // The wave is kept as a mono max-abs span, so a view window can re-bucket at full
        // resolution instead of stretching 512 whole-length buckets over fifty milliseconds.
        waveAbs.assign ((size_t) n, 0.0f);
        for (int c = 0; c < ch; ++c)
        {
            const float* d = buf.getReadPointer (c);
            for (int k = 0; k < n; ++k)
                waveAbs[(size_t) k] = juce::jmax (waveAbs[(size_t) k], std::abs (d[k]));
        }

        trimFraction = 1.0f;
        irMs = n / sr * 1000.0;

        // The window survives an IR swap — comparing cabinets at 50 ms is exactly when the zoom
        // earns its keep. A shot shorter than the window simply fills it (effectiveMs clamps).
        rebucket();
        metrics = juce::String (juce::roundToInt (irMs)) + "ms  |  "
                + juce::String (sr / 1000.0, 1) + "kHz  |  "
                + juce::String (reader->bitsPerSample) + "-bit  |  "
                + (ch == 1 ? "mono" : ch == 2 ? "stereo" : juce::String (ch) + "ch");
        repaint();
    }

    juce::AudioFormatManager formatManager;
    std::vector<float>       peaks;
    std::vector<float>       waveAbs;        // mono max-abs of the whole shot — the window re-buckets from it
    juce::Colour             accent { 0xff7c4dff };
    juce::String             metrics;
    float                    trimFraction    = 1.0f;
    bool                     ampLog          = true;
    float                    dbFloor         = -48.0f;
    bool                     trimInteractive = false;
    bool                     trimEnabled     = false;
    double                   irMs            = 0.0;
    double                   viewWindowMs    = 0.0;   // 0 = the whole IR — where the picture IS
    double                   viewTargetMs    = 0.0;   // ...and where it is going. Same encoding.
    Drag                     dragMode        = Drag::none;
    Drag                     hoverEl         = Drag::none;

    // What the picture spans right now, in ms.
    double effectiveMs() const { return viewWindowMs > 0.0 ? juce::jmin (viewWindowMs, irMs) : irMs; }

    // ...and what it will span when the glide is done. Everything that must not be decided twice
    // reads THIS one: a rule applied against a window still on its way would be applied again on
    // every frame of the way, and `clampTrimToWindow` writes a parameter each time it fires.
    double targetMs() const { return viewTargetMs > 0.0 ? juce::jmin (viewTargetMs, irMs) : irMs; }

    /** Point the picture at a window and let it travel. The zoom is geometric — a step is a
        RATIO, not a number of milliseconds — because that is how a zoom is read: halving 1000 ms
        and halving 40 ms should feel like the same gesture. */
    void glideTo (double ms)
    {
        viewTargetMs = juce::jmax (0.0, ms);

        if (juce::approximatelyEqual (targetMs(), effectiveMs()))
        {
            viewWindowMs = viewTargetMs;   // already there; keep the encoding honest (0 == FULL)
            return;
        }

        if (! isTimerRunning())
            startTimerHz (kGlideHz);
    }

    void timerCallback() override
    {
        const double from = effectiveMs(), to = targetMs();

        if (from <= 0.0 || to <= 0.0 || irMs <= 0.0)
        {
            viewWindowMs = viewTargetMs;
            stopTimer();
            repaint();
            return;
        }

        // One tick moves a fixed FRACTION of the remaining ratio, so the travel is exponential and
        // ends where it was aimed rather than creeping at it forever: close enough is arrived.
        const double next = from * std::pow (to / from, glideStep);

        if (std::abs (std::log (to / next)) < 0.01)   // within 1% — the eye is done before this
        {
            viewWindowMs = viewTargetMs;
            stopTimer();
        }
        else
        {
            viewWindowMs = next >= irMs ? 0.0 : next;
        }

        rebucket();
        repaint();
    }

    void rebucket()
    {
        if (waveAbs.empty() || irMs <= 0.0)
            return;

        const int n = juce::jmax (1, (int) ((double) waveAbs.size() * effectiveMs() / irMs));
        peaks = irwave::computePeaks (waveAbs.data(), juce::jmin (n, (int) waveAbs.size()), kBuckets);
    }

    // The window's rule over the trim: a handle beyond the window's edge is pulled onto it, and
    // the pull is a real parameter write.
    void clampTrimToWindow()
    {
        if (viewTargetMs <= 0.0 || irMs <= 0.0)
            return;

        const float limit = (float) (targetMs() / irMs);

        if (trimFraction > limit + 1.0e-4f)
        {
            trimFraction = juce::jlimit (minTrimFraction(), 1.0f, limit);
            if (onTrimChanged) onTrimChanged (trimFraction);
        }
    }

    // Right-click: the fixed windows (whichever the IR is long enough for), FULL, and the TRIM
    // switch again — the menu is the picture's own gear.
    void showViewMenu()
    {
        juce::PopupMenu m;

        // Named so the two halves read as what they are: the fixed windows are TRIM, the handle
        // on the picture is the MANUAL one.
        m.addSectionHeader ("TRIM");

        for (size_t i = 0; i < std::size (kTimeMarksMs); ++i)
        {
            const double ms = kTimeMarksMs[i];

            // 20 ms stays a grid mark but not an offer — that deep the trim starts thinning the
            // cab itself, not the room.
            if (ms < 50.0)
                continue;

            // The two waypoints wear their character: 50 is the tight dry window, 200 is already
            // mostly room. The rest sit between and say nothing.
            auto label = juce::String ((int) ms) + " ms";
            if ((int) ms == 50)  label += " - DRY";
            if ((int) ms == 200) label += " - WET";

            m.addItem ((int) i + 1, label, ms < irMs,
                       juce::approximatelyEqual (viewTargetMs, ms));
        }

        m.addItem (100, "FULL", true, viewTargetMs <= 0.0);
        m.addSeparator();
        m.addItem (200, "MANUAL TRIM", trimInteractive, trimEnabled);

        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                         [safe = juce::Component::SafePointer<IrWaveView> (this)] (int r)
                         {
                             if (safe == nullptr || r == 0)
                                 return;

                             if (r == 200)
                             {
                                 if (safe->onTrimToggled) safe->onTrimToggled (! safe->trimEnabled);
                                 return;
                             }

                             safe->setViewWindow (r == 100 ? 0.0
                                                           : kTimeMarksMs[(size_t) (r - 1)]);
                         });
    }

    bool  eqVisible = false;
    bool  hpfOn = false, lpfOn = false;
    float hpfHz = 80.0f,   hpfMin = 30.0f,   hpfMax = 180.0f;
    float lpfHz = 7000.0f, lpfMin = 4000.0f, lpfMax = 12000.0f;
    int   hpfSlopeDb = 12, lpfSlopeDb = 12;
    float stepAnchor = 0.0f;   // the slope ladder's last notch, in view y

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IrWaveView)
};

} // namespace felitronics::appkit
